/**
 * Animation Notify Handlers
 *
 * Manage animation notifies on UAnimSequence / UAnimMontage:
 * - add_notify: Add a UAnimNotify at a specific time
 * - add_notify_state: Add a UAnimNotifyState with begin/end times
 * - remove_notify: Remove a notify by index or name
 * - list_notifies: List all notifies on an animation asset
 * - set_notify_properties: Set properties on an existing notify
 * - list_notify_classes: List available notify classes
 *
 * @module anim-notify-handlers
 */

import { ITools } from '../../../types/tools/tool-interfaces.js';
import { cleanObject } from '../../../utils/serialization/safe-json.js';
import type { HandlerArgs } from '../../../types/handlers/handler-types.js';
import { executeAutomationRequest } from '../foundation/dispatch/common-handlers.js';

export async function handleAnimNotifyTools(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<Record<string, unknown>> {
  const argsRecord = args as Record<string, unknown>;
  const payload = { ...argsRecord, subAction: action };

  const result = await executeAutomationRequest(
    tools,
    'manage_anim_notify',
    payload as HandlerArgs,
    `Automation bridge not available for anim notify action: ${action}`
  );
  return cleanObject(result) as Record<string, unknown>;
}
