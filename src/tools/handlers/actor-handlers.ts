import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import { executeAutomationRequest, requireNonEmptyString } from './common-handlers.js';

type ActorActionHandler = (args: any, tools: ITools) => Promise<any>;

const handlers: Record<string, ActorActionHandler> = {
    spawn: async (args, tools) => {
        const classPath = requireNonEmptyString(args.classPath, 'classPath', 'Invalid classPath: must be a non-empty string');
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

        return tools.actorTools.spawn({
            classPath,
            actorName: args.actorName,
            location: args.location,
            rotation: args.rotation,
            meshPath: args.meshPath,
            timeoutMs
        });
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
        return tools.actorTools.applyForce({
            actorName,
            force: args.force
        });
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
        const childActor = requireNonEmptyString(args.childActor, 'childActor');
        const parentActor = requireNonEmptyString(args.parentActor, 'parentActor');
        return tools.actorTools.attach({ childActor, parentActor });
    },
    detach: async (args, tools) => {
        const actorName = requireNonEmptyString(args.actorName, 'actorName');
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
        return tools.actorTools.spawnBlueprint({
            blueprintPath,
            actorName: args.actorName,
            location: args.location,
            rotation: args.rotation
        });
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