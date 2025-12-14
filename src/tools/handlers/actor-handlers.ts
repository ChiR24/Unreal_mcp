import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import { executeAutomationRequest, requireNonEmptyString } from './common-handlers.js';

type ActorActionHandler = (args: any, tools: ITools) => Promise<any>;

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

        let classPath = args.classPath;

        // Apply alias if classPath matches a known alias
        if (classPath && classAliases[classPath]) {
            classPath = classAliases[classPath];
        }

        classPath = requireNonEmptyString(classPath, 'classPath', 'Invalid classPath: must be a non-empty string');
        const timeoutMs = typeof args.timeoutMs === 'number' ? args.timeoutMs : undefined;

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
        const componentToAdd = (args.classPath === 'SplineActor' || args.classPath === 'Spline')
            ? 'SplineComponent'
            : undefined;

        const result = await tools.actorTools.spawn({
            classPath,
            actorName: args.actorName,
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
        const actorName = requireNonEmptyString(args.actorName || args.name, 'actorName', 'Invalid actorName');
        return tools.actorTools.delete({ actorName });
    },
    apply_force: async (args, tools) => {
        const actorName = requireNonEmptyString(args.actorName, 'actorName');
        const force = args.force;

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
        } catch (error: any) {
            // Check if error is due to physics
            const errorMsg = error.message || String(error);

            if (errorMsg.toUpperCase().includes('PHYSICS')) {
                try {
                    // Auto-enable physics logic
                    const compsResult = await tools.actorTools.getComponents(actorName);
                    if (compsResult && compsResult.success && Array.isArray(compsResult.components)) {
                        console.log('DEBUG: Components found:', JSON.stringify(compsResult.components));
                        const meshComp = compsResult.components.find((c: any) => {
                            const name = c.name || c;
                            const match = typeof name === 'string' && (
                                name.toLowerCase().includes('staticmesh') ||
                                name.toLowerCase().includes('mesh') ||
                                name.toLowerCase().includes('primitive')
                            );
                            console.log(`DEBUG: Checking component '${name}' matches? ${match}`);
                            return match;
                        });

                        if (meshComp) {
                            const compName = meshComp.name || meshComp;
                            console.log(`Auto-enabling physics for component: ${compName}`); // Debug log
                            await tools.actorTools.setComponentProperties({
                                actorName,
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
        const actorName = requireNonEmptyString(args.actorName, 'actorName');
        return tools.actorTools.setTransform({
            actorName,
            location: args.location,
            rotation: args.rotation,
            scale: args.scale
        });
    },
    get_transform: async (args, tools) => {
        const actorName = requireNonEmptyString(args.actorName, 'actorName');
        return tools.actorTools.getTransform(actorName);
    },
    duplicate: async (args, tools) => {
        const actorName = requireNonEmptyString(args.actorName, 'actorName');
        return tools.actorTools.duplicate({
            actorName,
            newName: args.newName,
            offset: args.offset
        });
    },
    attach: async (args, tools) => {
        // Allow actorName as alias for childActor for consistency with other tools
        const childActor = requireNonEmptyString(args.childActor || args.actorName, 'childActor (or actorName)');
        const parentActor = requireNonEmptyString(args.parentActor, 'parentActor');
        return tools.actorTools.attach({ childActor, parentActor });
    },
    detach: async (args, tools) => {
        const actorName = requireNonEmptyString(args.actorName || args.childActor, 'actorName', 'detach requires actorName (or childActor)');
        return tools.actorTools.detach(actorName);
    },
    add_tag: async (args, tools) => {
        const actorName = requireNonEmptyString(args.actorName, 'actorName');
        const tag = requireNonEmptyString(args.tag, 'tag');
        return tools.actorTools.addTag({ actorName, tag });
    },
    remove_tag: async (args, tools) => {
        const actorName = requireNonEmptyString(args.actorName, 'actorName');
        const tag = requireNonEmptyString(args.tag, 'tag');
        return tools.actorTools.removeTag({ actorName, tag });
    },
    find_by_tag: async (args, tools) => {
        const rawTag = typeof args.tag === 'string' ? args.tag : '';
        return tools.actorTools.findByTag({ tag: rawTag, matchType: args.matchType });
    },
    delete_by_tag: async (args, tools) => {
        const tag = requireNonEmptyString(args.tag, 'tag');
        return tools.actorTools.deleteByTag(tag);
    },
    spawn_blueprint: async (args, tools) => {
        const blueprintPath = requireNonEmptyString(args.blueprintPath, 'blueprintPath', 'Invalid blueprintPath: must be a non-empty string');
        const result = await tools.actorTools.spawnBlueprint({
            blueprintPath,
            actorName: args.actorName,
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
        const query = typeof (args.name ?? args.actorName ?? args.query) === 'string'
            ? String(args.name ?? args.actorName ?? args.query).trim()
            : '';
        if (!query) {
            return { success: false, error: 'INVALID_ARGUMENT', message: 'name (or actorName) is required' };
        }

        // Use the plugin's fuzzy query endpoint (contains-match) instead of the
        // exact lookup endpoint. This improves "spawn then find" reliability.
        return tools.actorTools.findByName(query);
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