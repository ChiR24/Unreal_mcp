import type { EditorArgs } from '../../../types/handlers/handler-types.js';
import type { ITools } from '../../../types/tools/tool-interfaces.js';
import { cleanObject } from '../../../utils/serialization/safe-json.js';
import { sanitizeCommandArgument } from '../../../utils/validation/validation.js';
import { executeAutomationRequest } from '../foundation/dispatch/common-handlers.js';
import { EDITOR_ACTION_UNHANDLED, editorActionHandled, type EditorActionResult } from './editor-action-result.js';

async function startRecording(args: EditorArgs, tools: ITools): Promise<unknown> {
  const name = typeof args.name === 'string' ? args.name : undefined;
  const filename = args.filename || name || 'TestRecording';
  const frameRate = typeof args.frameRate === 'number' ? args.frameRate : undefined;
  const durationSeconds = typeof args.durationSeconds === 'number' ? args.durationSeconds : undefined;
  const metadata = args.metadata;
  const safeFilename = sanitizeCommandArgument(filename);

  try {
    const res = await executeAutomationRequest(tools, 'control_editor', {
      action: 'start_recording',
      filename,
      frameRate,
      durationSeconds,
      metadata
    });
    return cleanObject(res);
  } catch {
    if (!safeFilename) {
      return { success: false, error: 'Filename is required after sanitization', action: 'start_recording' };
    }
    await executeAutomationRequest(tools, 'console_command', { command: `DemoRec ${safeFilename}` });
    return {
      success: true,
      message: `Started recording to ${safeFilename}`,
      action: 'start_recording',
      filename: safeFilename,
      frameRate,
      durationSeconds
    };
  }
}

export async function handleEditorSessionAction(
  action: string,
  args: EditorArgs,
  tools: ITools
): Promise<EditorActionResult> {
  switch (action) {
    case 'play': {
      const res = await executeAutomationRequest(tools, 'control_editor', { action: 'play' }, undefined, { timeoutMs: args.timeoutMs }) as Record<string, unknown>;
      return editorActionHandled(cleanObject(res));
    }
    case 'stop':
    case 'stop_pie': {
      const res = await executeAutomationRequest(tools, 'control_editor', { action: 'stop' }) as Record<string, unknown>;
      return editorActionHandled(cleanObject(res));
    }
    case 'eject':
      return editorActionHandled(await executeAutomationRequest(tools, 'control_editor', { action: 'eject' }));
    case 'possess':
      return editorActionHandled(await executeAutomationRequest(tools, 'control_editor', args));
    case 'restart_editor':
      // Passed straight through: the plugin owns the unsaved-package gate and
      // re-enforces it, and it is the only side that can see which packages
      // are dirty at the moment the restart actually fires.
      return editorActionHandled(await executeAutomationRequest(tools, 'control_editor', {
        action: 'restart_editor',
        validateOnly: args.validateOnly,
        discardUnsaved: args.discardUnsaved,
        delaySeconds: args.delaySeconds,
      }));
    case 'pause': {
      const res = await executeAutomationRequest(tools, 'control_editor', { action: 'pause' }) as Record<string, unknown>;
      return editorActionHandled(cleanObject(res));
    }
    case 'resume': {
      const res = await executeAutomationRequest(tools, 'control_editor', { action: 'resume' }) as Record<string, unknown>;
      return editorActionHandled(cleanObject(res));
    }
    case 'start_recording':
      return editorActionHandled(await startRecording(args, tools));
    case 'stop_recording': {
      const res = await executeAutomationRequest(tools, 'control_editor', { action: 'stop_recording' }) as Record<string, unknown>;
      return editorActionHandled(cleanObject(res));
    }
    case 'step_frame': {
      // The plugin steps `steps` frames itself and reports them. Looping one call
      // per frame here also dropped every reply, so with no PIE running the
      // caller was told the frames had been stepped.
      const steps = typeof args.steps === 'number' && args.steps > 0 ? args.steps : 1;
      const res = await executeAutomationRequest(tools, 'control_editor', { action: 'step_frame', steps }) as Record<string, unknown>;
      return editorActionHandled(cleanObject(res));
    }
    case 'set_game_speed': {
      const safeSpeed = sanitizeCommandArgument(String(args.speed));
      if (!safeSpeed) return editorActionHandled({ success: false, error: 'Speed is required after sanitization', action: 'set_game_speed' });
      const res = await executeAutomationRequest(tools, 'control_editor', { action: 'set_game_speed', speed: Number(safeSpeed) }) as Record<string, unknown>;
      return editorActionHandled(cleanObject(res));
    }
    case 'set_fixed_delta_time': {
      const deltaTime = typeof args.deltaTime === 'number' ? args.deltaTime : 0.01667;
      const res = await executeAutomationRequest(tools, 'control_editor', { action: 'set_fixed_delta_time', deltaTime }) as Record<string, unknown>;
      return editorActionHandled(cleanObject(res));
    }
    default:
      return EDITOR_ACTION_UNHANDLED;
  }
}
