import type { ITools } from '../../../../types/tools/tool-interfaces.js';
import type { HandlerArgs } from '../../../../types/handlers/handler-types.js';
import { cleanObject } from '../../../../utils/serialization/safe-json.js';
import { executeAutomationRequest, getTimeoutMs } from '../../foundation/dispatch/common-handlers.js';
import { createUnknownActionResponse } from '../../foundation/dispatch/handler-error-context.js';
import { prepareAudioAuthoringPayload } from './audio-authoring-payload.js';
import { validateAudioAuthoringAction } from './audio-authoring-action-validation.js';

function unknownAudioAuthoringAction(action: string): Record<string, unknown> {
  return createUnknownActionResponse(`Unknown audio authoring action: ${action}`);
}

export async function handleAudioAuthoringTools(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<Record<string, unknown>> {
  const argsRecord: Record<string, unknown> = args;
  if (!validateAudioAuthoringAction(action, argsRecord)) {
    return unknownAudioAuthoringAction(action);
  }

  const timeoutMs = getTimeoutMs();
  const payload = prepareAudioAuthoringPayload(argsRecord, action);
  const result = await executeAutomationRequest(
    tools,
    'manage_audio_authoring',
    payload,
    `Automation bridge not available for audio authoring action: ${action}`,
    { timeoutMs }
  );
  return cleanObject(result) as Record<string, unknown>;
}
