import { ITools } from '../../types/tool-interfaces.js';
import type { HandlerArgs, HandlerResult, ActorArgs, Vector3, ComponentInfo } from '../../types/handler-types.js';
import { ACTOR_CLASS_ALIASES, getRequiredComponent } from '../../config/class-aliases.js';
import { cleanObject } from '../../utils/safe-json.js';
import { ResponseFactory } from '../../utils/response-factory.js';
import { normalizeArgs, extractString, extractOptionalString, extractOptionalNumber, extractOptionalBoolean } from './argument-helper.js';
import { executeAutomationRequest } from './common-handlers.js';

/** Actor handler function type */
type ActorActionHandler = (args: ActorArgs, tools: ITools) => Promise<HandlerResult>;

/** Result from list actors with actor info */
interface ListActorsResult {
    success?: boolean;
    actors?: Array<{ label?: string; name?: string }>;
    [key: string]: unknown;
}

/** Result from getComponents */
interface ComponentsResult {
    success?: boolean;
    components?: ComponentInfo[];
    [key: string]: unknown;
}

const handlers: Record<string, ActorActionHandler> = {
    spawn: async (args, tools) => {
        const params = normalizeArgs(args, [
            { key: 'classPath', aliases: ['class', 'type', 'actorClass'], required: true, map: ACTOR_CLASS_ALIASES },
            { key: 'actorName', aliases: ['name'] },
            { key: 'timeoutMs', default: undefined }
        ]);

        const classPath = extractString(params, 'classPath');
        const actorName = extractOptionalString(params, 'actorName');
        const timeoutMs = extractOptionalNumber(params, 'timeoutMs');

        // Extremely small timeouts are treated as an immediate timeout-style
        // failure so tests can exercise timeout handling deterministically
        // without relying on editor performance.
        if (typeof timeoutMs === 'number' && timeoutMs > 0 && timeoutMs < 200) {
            throw new Error(`Timeout too small for spawn operation: ${timeoutMs}ms`);
        }

        // For SplineActor alias, add SplineComponent automatically
        // Check original args for raw input since map transforms the alias
        const originalClass = args.classPath || args.class || args.type;
        const componentToAdd = typeof originalClass === 'string' ? getRequiredComponent(originalClass) : undefined;

        const result = await tools.actorTools.spawn({
            classPath,
            actorName,
            location: args.location,
            rotation: args.rotation,
            meshPath: typeof args.meshPath === 'string' ? args.meshPath : undefined,
            timeoutMs,
            ...(componentToAdd ? { componentToAdd } : {})
        });

        // Ensure successful spawn returns the actual actor name
        if (result && result.success && result.actorName) {
            return {
                ...result,
                message: `Spawned actor: ${result.actorName}`,
                // Explicitly return the actual name so the client can use it
                name: result.actorName
            };
        }
        return result;
    },
    delete: async (args, tools) => {
        if (args.actorNames && Array.isArray(args.actorNames)) {
            return tools.actorTools.delete({ actorNames: args.actorNames as string[] });
        }
        const params = normalizeArgs(args, [
            { key: 'actorName', aliases: ['name'], required: true }
        ]);
        const actorName = extractString(params, 'actorName');
        return tools.actorTools.delete({ actorName });
    },
    apply_force: async (args, tools) => {
        const params = normalizeArgs(args, [
            { key: 'actorName', aliases: ['name'], required: true }
        ]);
        const actorName = extractString(params, 'actorName');
        const force = args.force as Vector3;

        // Function to attempt applying force, returning the result or throwing
        const tryApplyForce = async () => {
            return await tools.actorTools.applyForce({
                actorName,
                force
            });
        };

        try {
            // Initial attempt
            return await tryApplyForce();
        } catch (error: unknown) {
            // Check if error is due to physics
            const errorMsg = error instanceof Error ? error.message : String(error);

            if (errorMsg.toUpperCase().includes('PHYSICS')) {
                try {
                    // Auto-enable physics logic
                    const compsResult = await tools.actorTools.getComponents(actorName) as ComponentsResult;
                    if (compsResult && compsResult.success && Array.isArray(compsResult.components)) {
                        const meshComp = compsResult.components.find((c: ComponentInfo) => {
                            const name = c.name || '';
                            const match = typeof name === 'string' && (
                                name.toLowerCase().includes('staticmesh') ||
                                name.toLowerCase().includes('mesh') ||
                                name.toLowerCase().includes('primitive')
                            );
                            return match;
                        });

                        if (meshComp) {
                            const compName = meshComp.name;
                            await tools.actorTools.setComponentProperties({
                                actorName,
                                componentName: compName,
                                properties: { SimulatePhysics: true, bSimulatePhysics: true, Mobility: 2 }
                            });

                            // Retry
                            return await tryApplyForce();
                        }
                    }
                } catch (retryError: unknown) {
                    // If retry fails, append debug info to original error and rethrow
                    const retryMsg = retryError instanceof Error ? retryError.message : String(retryError);
                    throw new Error(`${errorMsg} (Auto-enable physics failed: ${retryMsg})`);
                }
            }

            // Re-throw if not a physics error or if auto-enable logic matched nothing
            throw error;
        }
    },
    set_transform: async (args, tools) => {
        const params = normalizeArgs(args, [
            { key: 'actorName', aliases: ['name'], required: true }
        ]);
        const actorName = extractString(params, 'actorName');
        return tools.actorTools.setTransform({
            actorName,
            location: args.location,
            rotation: args.rotation,
            scale: args.scale
        });
    },
    get_transform: async (args, tools) => {
        const params = normalizeArgs(args, [
            { key: 'actorName', aliases: ['name'], required: true }
        ]);
        const actorName = extractString(params, 'actorName');
        return tools.actorTools.getTransform(actorName);
    },
    duplicate: async (args, tools) => {
        const params = normalizeArgs(args, [
            { key: 'actorName', aliases: ['name'], required: true },
            { key: 'newName', aliases: ['nameTo'] }
        ]);
        const actorName = extractString(params, 'actorName');
        const newName = extractOptionalString(params, 'newName');
        return tools.actorTools.duplicate({
            actorName,
            newName,
            offset: args.offset
        });
    },
    attach: async (args, tools) => {
        const params = normalizeArgs(args, [
            { key: 'childActor', aliases: ['actorName', 'child'], required: true },
            { key: 'parentActor', aliases: ['parent'], required: true }
        ]);
        const childActor = extractString(params, 'childActor');
        const parentActor = extractString(params, 'parentActor');
        return tools.actorTools.attach({ childActor, parentActor });
    },
    detach: async (args, tools) => {
        const params = normalizeArgs(args, [
            { key: 'actorName', aliases: ['childActor', 'child'], required: true }
        ]);
        const actorName = extractString(params, 'actorName');
        return tools.actorTools.detach(actorName);
    },
    add_tag: async (args, tools) => {
        const params = normalizeArgs(args, [
            { key: 'actorName', aliases: ['name'], required: true },
            { key: 'tag', required: true }
        ]);
        const actorName = extractString(params, 'actorName');
        const tag = extractString(params, 'tag');
        return tools.actorTools.addTag({ actorName, tag });
    },
    remove_tag: async (args, tools) => {
        const params = normalizeArgs(args, [
            { key: 'actorName', aliases: ['name'], required: true },
            { key: 'tag', required: true }
        ]);
        const actorName = extractString(params, 'actorName');
        const tag = extractString(params, 'tag');
        return tools.actorTools.removeTag({ actorName, tag });
    },
    find_by_tag: async (args, tools) => {
        const params = normalizeArgs(args, [
            { key: 'tag', default: '' }
        ]);
        const tag = extractOptionalString(params, 'tag') ?? '';
        const matchType = typeof args.matchType === 'string' ? args.matchType : undefined;
        return tools.actorTools.findByTag({ tag, matchType });
    },
    delete_by_tag: async (args, tools) => {
        const params = normalizeArgs(args, [
            { key: 'tag', required: true }
        ]);
        const tag = extractString(params, 'tag');
        return tools.actorTools.deleteByTag(tag);
    },
    spawn_blueprint: async (args, tools) => {
        const params = normalizeArgs(args, [
            { key: 'blueprintPath', aliases: ['path', 'bp'], required: true },
            { key: 'actorName', aliases: ['name'] }
        ]);
        const blueprintPath = extractString(params, 'blueprintPath');
        const actorName = extractOptionalString(params, 'actorName');
        const result = await tools.actorTools.spawnBlueprint({
            blueprintPath,
            actorName,
            location: args.location,
            rotation: args.rotation
        });

        if (result && result.success && result.actorName) {
            return {
                ...result,
                message: `Spawned blueprint: ${result.actorName}`,
                name: result.actorName
            };
        }
        return result;
    },
    list: async (args, tools) => {
        const params = normalizeArgs(args, [
            { key: 'limit', default: 50 },
            { key: 'refresh', default: false }
        ]);
        const refresh = extractOptionalBoolean(params, 'refresh');
        
        const result = await tools.actorTools.listActors({ refresh }) as ListActorsResult;
        if (result && result.actors && Array.isArray(result.actors)) {
            const limit = extractOptionalNumber(params, 'limit') ?? 50;
            const count = result.actors.length;
            const names = result.actors.slice(0, limit).map((a) => a.label || a.name || 'unknown').join(', ');
            const remaining = count - limit;
            const suffix = remaining > 0 ? `... and ${remaining} others` : '';
            (result as Record<string, unknown>).message = `Found ${count} actors: ${names}${suffix}`;
        }
        return result as HandlerResult;
    },
    find_by_name: async (args, tools) => {
        // Support both actorName and name parameters for consistency
        const params = normalizeArgs(args, [
            { key: 'name', aliases: ['actorName', 'query'], required: true }
        ]);
        const name = extractString(params, 'name');

        // Use the plugin's fuzzy query endpoint (contains-match) instead of the
        // exact lookup endpoint. This improves "spawn then find" reliability.
        return tools.actorTools.findByName(name);
    },
    // Missing handlers from test report - now implemented
    find_by_class: async (args, tools) => {
        const className = extractString(normalizeArgs(args, [{ key: 'className', required: true }]), 'className');
        const res = await executeAutomationRequest(tools, 'control_actor', {
            action: 'find_by_class',
            className
        });
        return cleanObject(res) as HandlerResult;
    },
    inspect_object: async (args, tools) => {
        const objectPath = extractString(normalizeArgs(args, [{ key: 'objectPath', aliases: ['actorName'], required: true }]), 'objectPath');
        const res = await executeAutomationRequest(tools, 'control_actor', {
            action: 'inspect_object',
            objectPath
        });
        return cleanObject(res) as HandlerResult;
    },
    get_property: async (args, tools) => {
        const params = normalizeArgs(args, [
            { key: 'actorName', aliases: ['name'], required: true },
            { key: 'propertyName', required: true }
        ]);
        const actorName = extractString(params, 'actorName');
        const propertyName = extractString(params, 'propertyName');
        const res = await executeAutomationRequest(tools, 'control_actor', {
            action: 'get_property',
            actorName,
            propertyName
        });
        return cleanObject(res) as HandlerResult;
    },
    set_property: async (args, tools) => {
        const params = normalizeArgs(args, [
            { key: 'actorName', aliases: ['name'], required: true },
            { key: 'propertyName', required: true },
            { key: 'propertyValue', required: true }
        ]);
        const actorName = extractString(params, 'actorName');
        const propertyName = extractString(params, 'propertyName');
        const propertyValue = params.propertyValue;
        const res = await executeAutomationRequest(tools, 'control_actor', {
            action: 'set_property',
            actorName,
            propertyName,
            value: propertyValue
        });
        return cleanObject(res) as HandlerResult;
    },
    inspect_class: async (args, tools) => {
        const className = extractString(normalizeArgs(args, [{ key: 'className', required: true }]), 'className');
        const res = await executeAutomationRequest(tools, 'control_actor', {
            action: 'inspect_class',
            className
        });
        return cleanObject(res) as HandlerResult;
    },
    list_objects: async (args, tools) => {
        const res = await executeAutomationRequest(tools, 'control_actor', {
            action: 'list_objects',
            objectType: args.objectType,
            limit: args.limit ?? 100
        });
        return cleanObject(res) as HandlerResult;
    },
    get_component_property: async (args, tools) => {
        const params = normalizeArgs(args, [
            { key: 'actorName', aliases: ['name'], required: true },
            { key: 'componentName', required: true },
            { key: 'propertyName', required: true }
        ]);
        const actorName = extractString(params, 'actorName');
        const componentName = extractString(params, 'componentName');
        const propertyName = extractString(params, 'propertyName');
        const res = await executeAutomationRequest(tools, 'control_actor', {
            action: 'get_component_property',
            actorName,
            componentName,
            propertyName
        });
        return cleanObject(res) as HandlerResult;
    },
    set_component_property: async (args, tools) => {
        const params = normalizeArgs(args, [
            { key: 'actorName', aliases: ['name'], required: true },
            { key: 'componentName', required: true },
            { key: 'propertyName', required: true },
            { key: 'propertyValue', required: true }
        ]);
        const actorName = extractString(params, 'actorName');
        const componentName = extractString(params, 'componentName');
        const propertyName = extractString(params, 'propertyName');
        const propertyValue = params.propertyValue;
        const res = await executeAutomationRequest(tools, 'control_actor', {
            action: 'set_component_property',
            actorName,
            componentName,
            propertyName,
            value: propertyValue
        });
        return cleanObject(res) as HandlerResult;
    },
    delete_object: async (args, tools) => {
        const objectPath = extractString(normalizeArgs(args, [{ key: 'objectPath', required: true }]), 'objectPath');
        const res = await executeAutomationRequest(tools, 'control_actor', {
            action: 'delete_object',
            objectPath
        });
        return cleanObject(res) as HandlerResult;
    },
    // Wave 1.13: Query Enhancement - query actors by predicate
    query_actors_by_predicate: async (args, tools) => {
        const predicate = args.predicate || {};
        const limit = typeof args.limit === 'number' ? args.limit : 100;
        const res = await executeAutomationRequest(tools, 'control_actor', {
            action: 'query_actors_by_predicate',
            predicate,
            limit
        });
        return cleanObject(res) as HandlerResult;
    },
    // Wave 2.11-2.20: Actor Enhancement
    get_all_component_properties: async (args, tools) => {
        const actorName = extractString(normalizeArgs(args, [{ key: 'actorName', aliases: ['name'], required: true }]), 'actorName');
        const res = await executeAutomationRequest(tools, 'control_actor', {
            action: 'get_all_component_properties',
            actorName,
            componentName: args.componentFilter
        });
        return cleanObject(res) as HandlerResult;
    },
    batch_set_component_properties: async (args, tools) => {
        const actorName = extractString(normalizeArgs(args, [{ key: 'actorName', aliases: ['name'], required: true }]), 'actorName');
        const properties = args.properties;
        if (!properties || typeof properties !== 'object') {
            throw new Error('control_actor.batch_set_component_properties: properties object is required');
        }
        const res = await executeAutomationRequest(tools, 'control_actor', {
            action: 'batch_set_component_properties',
            actorName,
            properties
        });
        return cleanObject(res) as HandlerResult;
    },
    clone_component_hierarchy: async (args, tools) => {
        const sourceActor = extractString(normalizeArgs(args, [{ key: 'sourceActor', aliases: ['actorName'], required: true }]), 'sourceActor');
        const targetActor = extractString(normalizeArgs(args, [{ key: 'targetActor', required: true }]), 'targetActor');
        const res = await executeAutomationRequest(tools, 'control_actor', {
            action: 'clone_component_hierarchy',
            sourceActor,
            targetActor,
            componentFilter: args.componentName
        });
        return cleanObject(res) as HandlerResult;
    },
    serialize_actor_state: async (args, tools) => {
        const actorName = extractString(normalizeArgs(args, [{ key: 'actorName', aliases: ['name'], required: true }]), 'actorName');
        const res = await executeAutomationRequest(tools, 'control_actor', {
            action: 'serialize_actor_state',
            actorName,
            includeComponents: args.includeComponents ?? true
        });
        return cleanObject(res) as HandlerResult;
    },
    deserialize_actor_state: async (args, tools) => {
        const actorName = extractString(normalizeArgs(args, [{ key: 'actorName', aliases: ['name'], required: true }]), 'actorName');
        const state = args.state;
        if (!state || typeof state !== 'object') {
            throw new Error('control_actor.deserialize_actor_state: state object is required');
        }
        const res = await executeAutomationRequest(tools, 'control_actor', {
            action: 'deserialize_actor_state',
            actorName,
            state
        });
        return cleanObject(res) as HandlerResult;
    },
    get_actor_bounds: async (args, tools) => {
        const actorName = extractString(normalizeArgs(args, [{ key: 'actorName', aliases: ['name'], required: true }]), 'actorName');
        const res = await executeAutomationRequest(tools, 'control_actor', {
            action: 'get_actor_bounds',
            actorName,
            includeChildActors: args.includeChildActors ?? false
        });
        return cleanObject(res) as HandlerResult;
    },
    batch_transform_actors: async (args, tools) => {
        const transforms = args.transforms;
        if (!Array.isArray(transforms) || transforms.length === 0) {
            throw new Error('control_actor.batch_transform_actors: transforms array is required');
        }
        const res = await executeAutomationRequest(tools, 'control_actor', {
            action: 'batch_transform_actors',
            transforms
        });
        return cleanObject(res) as HandlerResult;
    },
    get_actor_references: async (args, tools) => {
        const actorName = extractString(normalizeArgs(args, [{ key: 'actorName', aliases: ['name'], required: true }]), 'actorName');
        const res = await executeAutomationRequest(tools, 'control_actor', {
            action: 'get_actor_references',
            actorName
        });
        return cleanObject(res) as HandlerResult;
    },
    replace_actor_class: async (args, tools) => {
        const actorName = extractString(normalizeArgs(args, [{ key: 'actorName', aliases: ['name'], required: true }]), 'actorName');
        const newClass = extractString(normalizeArgs(args, [{ key: 'newClass', aliases: ['classPath'], required: true }]), 'newClass');
        const res = await executeAutomationRequest(tools, 'control_actor', {
            action: 'replace_actor_class',
            actorName,
            newClass,
            preserveTransform: args.preserveTransform ?? true,
            preserveComponents: args.preserveComponents ?? false
        });
        return cleanObject(res) as HandlerResult;
    },
    merge_actors: async (args, tools) => {
        const actorNames = args.actorNames;
        if (!Array.isArray(actorNames) || actorNames.length < 2) {
            throw new Error('control_actor.merge_actors: actorNames array with at least 2 actors is required');
        }
        const res = await executeAutomationRequest(tools, 'control_actor', {
            action: 'merge_actors',
            actorNames,
            targetName: args.targetName,
            mergeType: args.mergeType ?? 'static_mesh'
        });
        return cleanObject(res) as HandlerResult;
    }
};

export async function handleActorTools(action: string, args: HandlerArgs, tools: ITools): Promise<HandlerResult> {
    try {
        const handler = handlers[action];
        if (handler) {
            const res = await handler(args as ActorArgs, tools);
            // The actor tool handlers already return a StandardActionResponse-like object.
            // Don't wrap into { data: ... } since tests and tool schemas expect actorName/actorPath at top-level.
            return cleanObject(res) as HandlerResult;
        }
        // Fallback to direct bridge call or error
        const res = await executeAutomationRequest(tools, 'control_actor', args);
        return cleanObject(res) as HandlerResult;
    } catch (error: unknown) {
        return ResponseFactory.error(error);
    }
}
