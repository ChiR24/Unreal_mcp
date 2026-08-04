import type { ITools } from '../../../types/tools/tool-interfaces.js';
import type { HandlerArgs, ActorArgs } from '../../../types/handlers/handler-types.js';
import { cleanObject } from '../../../utils/serialization/safe-json.js';
import { executeAutomationRequest, withHandlerContext } from '../foundation/dispatch/common-handlers.js';
import { TOOL_ACTIONS } from '../../../utils/commands/action-constants.js';
import { basicActorHandlers } from './actor-basic-handlers.js';
import { componentActorHandlers } from './actor-component-handlers.js';
import { handleApplyForce } from './actor-physics-handler.js';
import { ActorActionHandler, normalizeActorAction } from './actor-handler-utils.js';

const handlers: Record<string, ActorActionHandler> = {
    ...basicActorHandlers,
    apply_force: handleApplyForce,
    ...componentActorHandlers
};

export async function handleActorTools(action: string, args: HandlerArgs, tools: ITools): Promise<Record<string, unknown>> {
    return withHandlerContext(async () => {
        const normalizedAction = normalizeActorAction(action);
        const handler = handlers[normalizedAction];
        if (handler) {
            const res = await handler(args as ActorArgs, tools);
            return cleanObject(res) as Record<string, unknown>;
        }
        const res = await executeAutomationRequest(tools, TOOL_ACTIONS.CONTROL_ACTOR, { ...args, action: normalizedAction });
        return cleanObject(res) as Record<string, unknown>;
    });
}
