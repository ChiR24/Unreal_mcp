import type { HandlerArgs } from '../../../types/handlers/handler-types.js';
import type { ITools } from '../../../types/tools/tool-interfaces.js';
import { createUnknownActionResponse } from '../foundation/dispatch/handler-error-context.js';
import { getWidgetActionRequiredFields } from './widget-authoring-action-requirements.js';
import {
  createWidgetAuthoringContext,
  sendWidgetAuthoringRequest,
  validateWidgetRequiredFields
} from './widget-authoring-context.js';

export async function handleWidgetAuthoringTools(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<Record<string, unknown>> {
  const requiredFields = getWidgetActionRequiredFields(action);
  if (!requiredFields) {
    return createUnknownActionResponse(`Unknown widget authoring action: ${action}`);
  }

  const context = createWidgetAuthoringContext(args, tools);
  validateWidgetRequiredFields(context.argsRecord, requiredFields);
  return await sendWidgetAuthoringRequest(context, action);
}
