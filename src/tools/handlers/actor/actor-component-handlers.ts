import { ResponseFactory } from '../../../utils/responses/response-factory.js';
import { normalizeArgs, extractString, extractOptionalString, extractOptionalNumber } from '../foundation/arguments/argument-helper.js';
import { ActorActionHandler, executeActorRequest } from './actor-handler-utils.js';

export const componentActorHandlers: Record<string, ActorActionHandler> = {
    set_component_property: async (args, tools) => {
        const params = normalizeArgs(args, [
            { key: 'actorName', aliases: ['name', 'actor_name'], required: true },
            { key: 'componentName', aliases: ['component_name'], required: true }
        ]);
        const actorName = extractString(params, 'actorName');
        const componentName = extractString(params, 'componentName');
        let properties: Record<string, unknown>;

        if (args.properties && typeof args.properties === 'object') {
            properties = args.properties as Record<string, unknown>;
        } else if (args.propertyName && args.value !== undefined) {
            properties = { [String(args.propertyName)]: args.value };
        } else {
            return ResponseFactory.error(new Error('Either "properties" object or "propertyName" and "value" must be provided'));
        }

        return await executeActorRequest(tools, {
            action: 'set_component_properties',
            actorName,
            componentName,
            properties
        });
    },
    set_material: async (args, tools) => {
        // actorNames gives many actors the same material in one call; the plugin reports each.
        const actorNames = Array.isArray(args.actorNames)
            ? args.actorNames.filter((name): name is string => typeof name === 'string')
            : [];
        const params = normalizeArgs(args, [
            { key: 'actorName', aliases: ['name', 'actor_name'], required: actorNames.length === 0 },
            { key: 'materialPath', aliases: ['assetPath', 'material', 'path'], required: true },
            { key: 'componentName', aliases: ['component_name'] },
            { key: 'materialSlot', aliases: ['materialIndex', 'slotIndex', 'slot'], default: 0 }
        ]);
        const materialSlot = extractOptionalNumber(params, 'materialSlot') ?? 0;

        return await executeActorRequest(tools, {
            action: 'set_material',
            actorName: extractOptionalString(params, 'actorName'),
            actorNames: actorNames.length > 0 ? actorNames : undefined,
            materialPath: extractString(params, 'materialPath'),
            componentName: extractOptionalString(params, 'componentName'),
            materialSlot: Math.trunc(materialSlot),
            allComponents: typeof args.allComponents === 'boolean' ? args.allComponents : undefined
        });
    },
    remove_component: async (args, tools) => await handleComponentAction('remove_component', args, tools),
    get_component_property: async (args, tools) => {
        const params = normalizeArgs(args, [
            { key: 'actorName', aliases: ['name', 'actor_name'], required: true },
            { key: 'componentName', aliases: ['component_name'], required: true },
            { key: 'propertyName', aliases: ['property_name'], required: true }
        ]);
        return await executeActorRequest(tools, {
            action: 'get_component_property',
            actorName: extractString(params, 'actorName'),
            componentName: extractString(params, 'componentName'),
            propertyName: extractString(params, 'propertyName')
        });
    },
    set_collision: async (args, tools) => {
        const params = normalizeArgs(args, [
            { key: 'actorName', aliases: ['name', 'actor_name'], required: true },
            { key: 'collisionEnabled', aliases: ['collision_enabled'], default: true }
        ]);
        return await executeActorRequest(tools, {
            action: 'set_collision',
            actorName: extractString(params, 'actorName'),
            collisionEnabled: params.collisionEnabled ?? true
        });
    },
    call_function: async (args, tools) => {
        const params = normalizeArgs(args, [
            { key: 'actorName', aliases: ['name', 'actor_name'], required: true },
            { key: 'functionName', aliases: ['function_name'], required: true },
            { key: 'arguments', aliases: ['args'] }
        ]);
        return await executeActorRequest(tools, {
            action: 'call_function',
            actorName: extractString(params, 'actorName'),
            functionName: extractString(params, 'functionName'),
            arguments: params.arguments
        });
    },
    find_by_class: async (args, tools) => {
        const params = normalizeArgs(args, [
            { key: 'className', aliases: ['class_name', 'class', 'classPath'], required: true }
        ]);
        return await executeActorRequest(tools, {
            action: 'find_by_class',
            className: extractString(params, 'className')
        });
    },
    get_bounding_box: async (args, tools) => {
        const params = normalizeArgs(args, [
            { key: 'actorName', aliases: ['name', 'actor_name'], required: true }
        ]);
        return await executeActorRequest(tools, {
            action: 'get_bounding_box',
            actorName: extractString(params, 'actorName')
        });
    }
};

async function handleComponentAction(action: string, args: Parameters<ActorActionHandler>[0], tools: Parameters<ActorActionHandler>[1]) {
    const params = normalizeArgs(args, [
        { key: 'actorName', aliases: ['name', 'actor_name'], required: true },
        { key: 'componentName', aliases: ['component_name'], required: true }
    ]);
    return await executeActorRequest(tools, {
        action,
        actorName: extractString(params, 'actorName'),
        componentName: extractString(params, 'componentName')
    });
}
