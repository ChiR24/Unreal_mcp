import type { ITools } from '../../../types/tools/tool-interfaces.js';
import type { ActorArgs, ComponentInfo } from '../../../types/handlers/handler-types.js';
import { executeAutomationRequest } from '../foundation/dispatch/common-handlers.js';
import { TOOL_ACTIONS } from '../../../utils/commands/action-constants.js';
import { isRecord } from '../../../utils/validation/type-guards.js';

export type ActorActionHandler = (args: ActorArgs, tools: ITools) => Promise<Record<string, unknown>>;

export interface ListActorsResult {
    success?: boolean;
    actors?: Array<{ label?: string; name?: string }>;
    [key: string]: unknown;
}

export interface ComponentsResult {
    success?: boolean;
    components?: ComponentInfo[];
    [key: string]: unknown;
}

const DEFAULT_ACTOR_LIST_LIMIT = 50;

const ACTOR_ACTION_ALIASES: Record<string, string> = {
    'spawn_actor': 'spawn',
    'destroy_actor': 'delete',
    'teleport_actor': 'set_transform',
    'set_actor_location': 'set_transform',
    'set_actor_rotation': 'set_transform',
    'set_actor_scale': 'set_transform',
    'set_actor_transform': 'set_transform',
    'get_actor_transform': 'get_transform',
    'set_actor_visible': 'set_visibility',
    'attach_actor': 'attach',
    'detach_actor': 'detach',
    'get_actor_bounds': 'get_bounding_box',
    'get_actor_components': 'get_components',
    'add_component': 'add_component',
    'remove_component': 'remove_component',
    'set_component_properties': 'set_component_property',
    'set_component_property': 'set_component_property',
    'set_actor_material': 'set_material',
    'apply_material': 'set_material',
    'get_component_property': 'get_component_property',
    'call_actor_function': 'call_function',
    'find_actors_by_class': 'find_by_class',
    'find_actors_by_name': 'find_by_name',
    'find_actors_by_tag': 'find_by_tag',
    'set_actor_collision': 'set_collision',
};

export { isRecord };

export function normalizeActorAction(action: string): string {
    return ACTOR_ACTION_ALIASES[action] ?? action;
}

export function extractActorListPayload(response: Record<string, unknown>): ListActorsResult | undefined {
    if (response.success === false) {
        return undefined;
    }

    if (Array.isArray(response.actors)) {
        return response as ListActorsResult;
    }

    const result = response.result;
    if (isRecord(result)) {
        if (result.success === false) {
            return undefined;
        }

        if (Array.isArray(result.actors)) {
            return result as ListActorsResult;
        }

        const data = result.data;
        if (isRecord(data) && Array.isArray(data.actors)) {
            return data as ListActorsResult;
        }
    }

    return undefined;
}

export function normalizeActorListLimit(value: unknown): number {
    if (typeof value !== 'number' || !Number.isFinite(value) || value < 0) {
        return DEFAULT_ACTOR_LIST_LIMIT;
    }

    return Math.floor(value);
}

export async function executeActorRequest(tools: ITools, payload: Record<string, unknown>): Promise<Record<string, unknown>> {
    return await executeAutomationRequest(tools, TOOL_ACTIONS.CONTROL_ACTOR, payload) as Record<string, unknown>;
}
