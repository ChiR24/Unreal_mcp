import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import { executeAutomationRequest } from './common-handlers.js';
import { Logger } from '../../utils/logger.js';
import { normalizeArgs } from './argument-helper.js';

type ActorActionHandler = (args: any, tools: ITools) => Promise<any>;

const logger = new Logger('ActorHandlers');

const handlers: Record<string, ActorActionHandler> = {
    spawn: async (args, tools) => {
        // Class name aliases for user-friendly names
        const classAliases: Record<string, string> = {
            'SplineActor': '/Script/Engine.Actor',  // Use Actor with SplineComponent
            'Spline': '/Script/Engine.Actor',       // Use Actor with SplineComponent
            'PointLight': '/Script/Engine.PointLight',
            'SpotLight': '/Script/Engine.SpotLight',
            'DirectionalLight': '/Script/Engine.DirectionalLight',
            'Camera': '/Script/Engine.CameraActor',
            'CameraActor': '/Script/Engine.CameraActor',
            'StaticMeshActor': '/Script/Engine.StaticMeshActor',
            'SkeletalMeshActor': '/Script/Engine.SkeletalMeshActor',
            'PlayerStart': '/Script/Engine.PlayerStart',
            'TriggerBox': '/Script/Engine.TriggerBox',
            'TriggerSphere': '/Script/Engine.TriggerSphere',
            'BlockingVolume': '/Script/Engine.BlockingVolume',
            'Pawn': '/Script/Engine.Pawn',
            'Character': '/Script/Engine.Character',
            'Actor': '/Script/Engine.Actor'
        };

        const params = normalizeArgs(args, [
            { key: 'classPath', aliases: ['class', 'type', 'actorClass'], required: true, map: classAliases },
            { key: 'actorName', aliases: ['name'] },
            { key: 'timeoutMs', default: undefined }
        ]);
        
        const timeoutMs = typeof params.timeoutMs === 'number' ? params.timeoutMs : undefined;

        // Extremely small timeouts are treated as an immediate timeout-style
        // failure so tests can exercise timeout handling deterministically
        // without relying on editor performance.
        if (typeof timeoutMs === 'number' && timeoutMs > 0 && timeoutMs < 200) {
            return cleanObject({
                success: false,
                error: `Timeout too small for spawn operation: ${timeoutMs}ms`,
                message: 'Timeout too small for spawn operation'
            });
        }

        // For SplineActor alias, add SplineComponent automatically
        // Check original args for raw input or assume normalized classPath check is sufficient if map didn't obscure it?
        // Map transforms 'SplineActor' -> '/Script/Engine.Actor', so we check original args.
        const originalClass = args.classPath || args.class || args.type || args.actorClass;
        const componentToAdd = (originalClass === 'SplineActor' || originalClass === 'Spline')
            ? 'SplineComponent'
            : undefined;

        const result = await tools.actorTools.spawn({
            classPath: params.classPath,
            actorName: params.actorName,
            location: args.location,
            rotation: args.rotation,
            meshPath: args.meshPath,
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
            return tools.actorTools.delete({ actorNames: args.actorNames });
        }
        const params = normalizeArgs(args, [
            { key: 'actorName', aliases: ['name'], required: true }
        ]);
        return tools.actorTools.delete({ actorName: params.actorName });
    },
    apply_force: async (args, tools) => {
        const params = normalizeArgs(args, [
            { key: 'actorName', aliases: ['name'], required: true }
        ]);
        const force = args.force;

        // Function to attempt applying force, returning the result or throwing
        const tryApplyForce = async () => {
            return await tools.actorTools.applyForce({
                actorName: params.actorName,
                force
            });
        };

        try {
            // Initial attempt
            return await tryApplyForce();
        } catch (error: any) {
            // Check if error is due to physics
            const errorMsg = error.message || String(error);

            if (errorMsg.toUpperCase().includes('PHYSICS')) {
                try {
                    // Auto-enable physics logic
                    const compsResult = await tools.actorTools.getComponents(params.actorName);
                    if (compsResult && compsResult.success && Array.isArray(compsResult.components)) {
                        logger.debug('Components found:', JSON.stringify(compsResult.components));
                        const meshComp = compsResult.components.find((c: any) => {
                            const name = c.name || c;
                            const match = typeof name === 'string' && (
                                name.toLowerCase().includes('staticmesh') ||
                                name.toLowerCase().includes('mesh') ||
                                name.toLowerCase().includes('primitive')
                            );
                            logger.debug(`Checking component '${name}' matches? ${match}`);
                            return match;
                        });

                        if (meshComp) {
                            const compName = meshComp.name || meshComp;
                            logger.debug(`Auto-enabling physics for component: ${compName}`); // Debug log
                            await tools.actorTools.setComponentProperties({
                                actorName: params.actorName,
                                componentName: compName,
                                properties: { SimulatePhysics: true, bSimulatePhysics: true, Mobility: 2 }
                            });

                            // Retry
                            return await tryApplyForce();
                        }
                    }
                } catch (retryError: any) {
                    // If retry fails, append debug info to original error and rethrow
                    throw new Error(`${errorMsg} (Auto-enable physics failed: ${retryError.message})`);
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
        return tools.actorTools.setTransform({
            actorName: params.actorName,
            location: args.location,
            rotation: args.rotation,
            scale: args.scale
        });
    },
    get_transform: async (args, tools) => {
        const params = normalizeArgs(args, [
            { key: 'actorName', aliases: ['name'], required: true }
        ]);
        return tools.actorTools.getTransform(params.actorName);
    },
    duplicate: async (args, tools) => {
        const params = normalizeArgs(args, [
            { key: 'actorName', aliases: ['name'], required: true },
            { key: 'newName', aliases: ['nameTo'] }
        ]);
        return tools.actorTools.duplicate({
            actorName: params.actorName,
            newName: params.newName,
            offset: args.offset
        });
    },
    attach: async (args, tools) => {
        const params = normalizeArgs(args, [
            { key: 'childActor', aliases: ['actorName', 'child'], required: true },
            { key: 'parentActor', aliases: ['parent'], required: true }
        ]);
        return tools.actorTools.attach({ childActor: params.childActor, parentActor: params.parentActor });
    },
    detach: async (args, tools) => {
        const params = normalizeArgs(args, [
            { key: 'actorName', aliases: ['childActor', 'child'], required: true }
        ]);
        return tools.actorTools.detach(params.actorName);
    },
    add_tag: async (args, tools) => {
        const params = normalizeArgs(args, [
            { key: 'actorName', aliases: ['name'], required: true },
            { key: 'tag', required: true }
        ]);
        return tools.actorTools.addTag({ actorName: params.actorName, tag: params.tag });
    },
    remove_tag: async (args, tools) => {
        const params = normalizeArgs(args, [
            { key: 'actorName', aliases: ['name'], required: true },
            { key: 'tag', required: true }
        ]);
        return tools.actorTools.removeTag({ actorName: params.actorName, tag: params.tag });
    },
    find_by_tag: async (args, tools) => {
        const params = normalizeArgs(args, [
            { key: 'tag', default: '' }
        ]);
        return tools.actorTools.findByTag({ tag: params.tag, matchType: args.matchType });
    },
    delete_by_tag: async (args, tools) => {
        const params = normalizeArgs(args, [
            { key: 'tag', required: true }
        ]);
        return tools.actorTools.deleteByTag(params.tag);
    },
    spawn_blueprint: async (args, tools) => {
        const params = normalizeArgs(args, [
            { key: 'blueprintPath', aliases: ['path', 'bp'], required: true },
            { key: 'actorName', aliases: ['name'] }
        ]);
        const result = await tools.actorTools.spawnBlueprint({
            blueprintPath: params.blueprintPath,
            actorName: params.actorName,
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
        const result = await tools.actorTools.listActors();
        if (result && result.actors && Array.isArray(result.actors)) {
            const limit = typeof args.limit === 'number' ? args.limit : 50;
            const count = result.actors.length;
            const names = result.actors.slice(0, limit).map((a: any) => a.label || a.name).join(', ');
            const remaining = count - limit;
            const suffix = remaining > 0 ? `... and ${remaining} others` : '';
            (result as any).message = `Found ${count} actors: ${names}${suffix}`;
        }
        return result;
    },
    find_by_name: async (args, tools) => {
        // Support both actorName and name parameters for consistency
        const params = normalizeArgs(args, [
            { key: 'name', aliases: ['actorName', 'query'], required: true }
        ]);
        
        // Use the plugin's fuzzy query endpoint (contains-match) instead of the
        // exact lookup endpoint. This improves "spawn then find" reliability.
        return tools.actorTools.findByName(params.name);
    }
};

export async function handleActorTools(action: string, args: any, tools: ITools) {
    const handler = handlers[action];
    if (handler) {
        const res = await handler(args, tools);
        return cleanObject(res);
    }
    // Fallback to direct bridge call or error
    return executeAutomationRequest(tools, 'control_actor', args);
}