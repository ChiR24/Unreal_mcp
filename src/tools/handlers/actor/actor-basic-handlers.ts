import { ACTOR_CLASS_ALIASES, getRequiredComponent } from '../../../config/class-aliases.js';
import type { ActorArgs } from '../../../types/handlers/handler-types.js';
import { normalizeArgs, extractString, extractOptionalString, extractOptionalNumber } from '../foundation/arguments/argument-helper.js';
import {
    ActorActionHandler,
    executeActorRequest,
    extractActorListPayload,
    normalizeActorListLimit
} from './actor-handler-utils.js';

export const basicActorHandlers: Record<string, ActorActionHandler> = {
    spawn: async (args, tools) => {
        const params = normalizeArgs(args, [
            { key: 'classPath', aliases: ['class', 'type', 'actorClass', 'actor_class', 'className', 'class_name'], required: true, map: ACTOR_CLASS_ALIASES },
            { key: 'actorName', aliases: ['name', 'actor_name'] },
            { key: 'timeoutMs', default: undefined }
        ]);
        const classPath = extractString(params, 'classPath');
        const actorName = extractOptionalString(params, 'actorName');
        const timeoutMs = extractOptionalNumber(params, 'timeoutMs');

        if (typeof timeoutMs === 'number' && timeoutMs > 0 && timeoutMs < 200) {
            throw new Error(`Timeout too small for spawn operation: ${timeoutMs}ms`);
        }

        const originalClass = args.classPath || args.class || args.type;
        const componentToAdd = typeof originalClass === 'string' ? getRequiredComponent(originalClass) : undefined;
        const payload: Record<string, unknown> = {
            action: 'spawn',
            classPath,
            actorName,
            location: args.location,
            rotation: args.rotation,
            scale: args.scale,
            meshPath: typeof args.meshPath === 'string' ? args.meshPath : undefined
        };
        if (componentToAdd) {
            payload.componentToAdd = componentToAdd;
        }

        const result = await executeActorRequest(tools, payload);
        if (result && result.success && result.actorName) {
            return {
                ...result,
                message: `Spawned actor: ${result.actorName}`,
                name: result.actorName
            };
        }
        return result;
    },
    delete: async (args, tools) => {
        if (args.actorNames && Array.isArray(args.actorNames)) {
            return await executeActorRequest(tools, {
                action: 'delete',
                actorNames: args.actorNames as string[]
            });
        }
        const params = normalizeArgs(args, [
            { key: 'actorName', aliases: ['name'], required: true }
        ]);
        const actorName = extractString(params, 'actorName');
        return await executeActorRequest(tools, { action: 'delete', actorName });
    },
    set_transform: async (args, tools) => {
        if (Array.isArray(args.actors)) {
            return await executeActorRequest(tools, { action: 'set_transform', actors: args.actors });
        }
        const actorName = getActorName(args);
        return await executeActorRequest(tools, {
            action: 'set_transform',
            actorName,
            location: args.location,
            rotation: args.rotation,
            scale: args.scale
        });
    },
    get_transform: async (args, tools) => {
        return await executeActorRequest(tools, {
            action: 'get_transform',
            actorName: getActorName(args)
        });
    },
    duplicate: async (args, tools) => {
        const params = normalizeArgs(args, [
            { key: 'actorName', aliases: ['name'], required: true },
            { key: 'newName', aliases: ['nameTo'] }
        ]);
        const actorName = extractString(params, 'actorName');
        const newName = extractOptionalString(params, 'newName');
        return await executeActorRequest(tools, {
            action: 'duplicate',
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
        return await executeActorRequest(tools, {
            action: 'attach',
            childActor: extractString(params, 'childActor'),
            parentActor: extractString(params, 'parentActor')
        });
    },
    detach: async (args, tools) => {
        return await executeActorRequest(tools, {
            action: 'detach',
            actorName: getActorName(args, ['childActor', 'child'])
        });
    },
    add_tag: async (args, tools) => await handleTagAction('add_tag', args, tools),
    remove_tag: async (args, tools) => await handleTagAction('remove_tag', args, tools),
    find_by_tag: async (args, tools) => {
        const params = normalizeArgs(args, [{ key: 'tag', default: '' }]);
        return await executeActorRequest(tools, {
            action: 'find_by_tag',
            tag: extractOptionalString(params, 'tag') ?? '',
            matchType: typeof args.matchType === 'string' ? args.matchType : undefined
        });
    },
    delete_by_tag: async (args, tools) => {
        if (Array.isArray(args.tags)) {
            return await executeActorRequest(tools, { action: 'delete_by_tag', tags: args.tags, tag: args.tag });
        }
        const params = normalizeArgs(args, [{ key: 'tag', required: true }]);
        return await executeActorRequest(tools, {
            action: 'delete_by_tag',
            tag: extractString(params, 'tag')
        });
    },
    spawn_blueprint: async (args, tools) => {
        const params = normalizeArgs(args, [
            { key: 'blueprintPath', aliases: ['path', 'bp'], required: true },
            { key: 'actorName', aliases: ['name'] }
        ]);
        const result = await executeActorRequest(tools, {
            action: 'spawn_blueprint',
            blueprintPath: extractString(params, 'blueprintPath'),
            actorName: extractOptionalString(params, 'actorName'),
            location: args.location,
            rotation: args.rotation,
            scale: args.scale
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
        const offset = typeof args.offset === 'number' && args.offset > 0 ? Math.trunc(args.offset) : 0;
        const result = await executeActorRequest(tools, {
            action: 'list',
            limit: normalizeActorListLimit(args.limit),
            offset: offset > 0 ? offset : undefined,
            filter: typeof args.filter === 'string' ? args.filter : undefined,
            propertyNames: Array.isArray(args.propertyNames) ? args.propertyNames : undefined
        });
        const listPayload = extractActorListPayload(result);
        if (listPayload) {
            Object.assign(result, listPayload);
            const returnedCount = listPayload.actors?.length ?? 0;
            const totalCount = typeof listPayload.totalCount === 'number' ? listPayload.totalCount : returnedCount;
            const names = (listPayload.actors ?? []).map((a) => a.label || a.name || 'unknown').join(', ');
            const remaining = totalCount - offset - returnedCount;
            const suffix = remaining > 0 ? `... and ${remaining} more (offset ${offset + returnedCount})` : '';
            result.message = `Found ${totalCount} actors: ${names}${suffix}`;
        }
        return result;
    },
    find_by_name: async (args, tools) => {
        const params = normalizeArgs(args, [
            { key: 'name', aliases: ['actorName', 'query'], required: true }
        ]);
        return await executeActorRequest(tools, {
            action: 'find_by_name',
            name: extractString(params, 'name')
        });
    }
};

function getActorName(args: ActorArgs, aliases: string[] = ['name']): string {
    const params = normalizeArgs(args, [
        { key: 'actorName', aliases, required: true }
    ]);
    return extractString(params, 'actorName');
}

async function handleTagAction(action: string, args: ActorArgs, tools: Parameters<ActorActionHandler>[1]) {
    const actorNames = (args as Record<string, unknown>).actorNames;
    if (action === 'add_tag' && Array.isArray(actorNames) && actorNames.length > 0) {
        const tag = typeof args.tag === 'string' ? args.tag : '';
        return await executeActorRequest(tools, { action, actorNames, tag });
    }
    const params = normalizeArgs(args, [
        { key: 'actorName', aliases: ['name'], required: true },
        { key: 'tag', required: true }
    ]);
    return await executeActorRequest(tools, {
        action,
        actorName: extractString(params, 'actorName'),
        tag: extractString(params, 'tag')
    });
}
