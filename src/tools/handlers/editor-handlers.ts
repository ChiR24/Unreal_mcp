import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import type { EditorArgs } from '../../types/handler-types.js';
import { executeAutomationRequest, normalizePathFields, requireNonEmptyString, validateExpectedParams, validateRequiredParams, validateArgsSecurity } from './common-handlers.js';
import { sanitizeCommandArgument } from '../../utils/validation.js';

/**
 * Action aliases for test compatibility
 * Maps test action names to handler action names
 */
const EDITOR_ACTION_ALIASES: Record<string, string> = {
  'focus_actor': 'focus',
  'set_camera_position': 'set_camera',
  'set_viewport_camera': 'set_camera',
  'take_screenshot': 'screenshot',
  'close_asset': 'close_asset',
  'save_all': 'save_all',
  'undo': 'undo',
  'redo': 'redo',
  'set_editor_mode': 'set_editor_mode',
  'show_stats': 'show_stats',
  'hide_stats': 'hide_stats',
  'set_game_view': 'set_game_view',
  'set_immersive_mode': 'set_immersive_mode',
  'single_frame_step': 'step_frame',
  'set_fixed_delta_time': 'set_fixed_delta_time',
  'open_level': 'open_level',
};

/**
 * Idempotent actions that accept success even with invalid/missing params.
 * These are global commands that have sensible defaults or no-ops.
 * NOTE: Include both original and normalized action names for proper validation.
 */
const IDEMPOTENT_ACTIONS = new Set([
  'stop', 'stop_pie', 'pause', 'resume',
  'set_game_speed', 'set_fixed_delta_time',
  'set_immersive_mode', 'set_game_view',
  'show_stats', 'hide_stats',
  'undo', 'redo',
  'step_frame', 'single_frame_step'
]);

/**
 * Actions that require specific parameters.
 * Maps action name to array of required parameter names.
 * NOTE: Includes both original and normalized action names for proper validation.
 */
const ACTION_REQUIRED_PARAMS: Record<string, string[]> = {
  'focus_actor': ['actorName'],
  'focus': ['actorName'],  // Normalized alias for focus_actor
  'possess': ['actorName'],
  'set_camera': ['location', 'rotation'],
  'set_viewport_resolution': ['width', 'height'],
  'set_view_mode': ['viewMode'],
  'set_editor_mode': ['mode'],
  'set_camera_fov': ['fov'],
  'set_game_speed': ['speed'],
  'set_fixed_delta_time': ['deltaTime'],
  'set_preferences': ['category', 'preferences'],
  'execute_command': ['command'],
  'console_command': ['command'],
};

/**
 * Actions that have specific allowed parameters.
 * Maps action name to array of allowed parameter names (excluding action/subAction/timeoutMs).
 * NOTE: Includes both original and normalized action names for proper validation.
 */
const ACTION_ALLOWED_PARAMS: Record<string, string[]> = {
  'play': [],
  'stop': [],
  'stop_pie': [],
  'pause': [],
  'resume': [],
  'eject': [],
  'possess': ['actorName'],
  'open_asset': ['assetPath', 'path'],
  'close_asset': ['assetPath', 'path'],
  'open_level': ['levelPath', 'path', 'assetPath'],
  'focus_actor': ['actorName', 'name'],
  'focus': ['actorName', 'name'],  // Normalized alias for focus_actor
  'set_camera': ['location', 'rotation', 'actorName'],
  'set_viewport_resolution': ['width', 'height'],
  'set_view_mode': ['viewMode'],
  'set_editor_mode': ['mode'],
  'set_camera_fov': ['fov'],
  'set_game_speed': ['speed'],
  'set_fixed_delta_time': ['deltaTime'],
  'screenshot': ['filename', 'path', 'resolution', 'mode', 'returnBase64', 'includeMetadata', 'metadata'],
  'set_preferences': ['category', 'preferences'],
  'execute_command': ['command'],
  'console_command': ['command'],
  'undo': [],
  'redo': [],
  'save_all': [],
  'show_stats': ['stat'],
  'hide_stats': ['stat'],
  'set_game_view': ['enabled'],
  'set_immersive_mode': ['enabled'],
  'step_frame': ['steps'],
  'single_frame_step': ['steps'],
  'create_bookmark': ['id', 'description', 'bookmarkName'],
  'jump_to_bookmark': ['id', 'bookmarkName'],
  'start_recording': ['filename', 'name', 'frameRate', 'durationSeconds', 'metadata'],
  'stop_recording': [],
  'set_viewport_realtime': ['enabled', 'realtime'],
  'simulate_input': ['key', 'type', 'inputType', 'inputAction', 'x', 'y', 'button'],
};

const INPUT_TYPE_ALIASES: Record<string, string> = {
  press: 'key_down',
  pressed: 'key_down',
  down: 'key_down',
  release: 'key_up',
  released: 'key_up',
  up: 'key_up',
  click: 'mouse_click',
  move: 'mouse_move',
};

const SUPPORTED_INPUT_TYPES = new Set(['key_down', 'key_up', 'mouse_click', 'mouse_move']);
const EDITOR_ASSET_PATH_ACTIONS = new Set(['open_asset', 'close_asset', 'open_level']);
const EDITOR_PATH_FIELDS = ['assetPath', 'levelPath', 'path'] as const;
const SUPPORTED_SCREENSHOT_MODES = new Set(['editor_viewport', 'game_viewport', 'full_editor_window']);

/**
 * Normalize editor action names for test compatibility
 */
function normalizeEditorAction(action: string): string {
  return EDITOR_ACTION_ALIASES[action] ?? action;
}

/**
 * Validates arguments for editor actions.
 * For non-idempotent actions, validates that only expected parameters are present.
 * Always validates security patterns (path traversal, etc).
 */
function validateEditorActionArgs(
  action: string,
  args: Record<string, unknown>
): void {
  // Always validate security patterns first
  validateArgsSecurity({ action, ...args } as Record<string, unknown>);

  // Validate required parameters FIRST (applies to ALL actions including idempotent)
  // This ensures required param validation is not skipped for idempotent actions
  const requiredParams = ACTION_REQUIRED_PARAMS[action];
  if (requiredParams !== undefined) {
    validateRequiredParams(args, requiredParams, `control_editor:${action}`);
  }

  // Idempotent actions skip allowed params validation (they accept extras gracefully)
  // But they still require their required params to be present (validated above)
  if (IDEMPOTENT_ACTIONS.has(action)) {
    return;
  }

  // Validate that only expected parameters are present for non-idempotent actions
  const allowedParams = ACTION_ALLOWED_PARAMS[action];
  if (allowedParams !== undefined) {
    validateExpectedParams(args, allowedParams, `control_editor:${action}`);
  }
}

function getInputType(args: EditorArgs): string {
  const inputTypeValue = args.type ?? args.inputType ?? args.inputAction;
  if (typeof inputTypeValue !== 'string' || inputTypeValue.trim() === '') {
    throw new Error('Missing required parameters for control_editor:simulate_input: [type|inputType|inputAction]');
  }

  const normalized = inputTypeValue.trim().toLowerCase();
  if ((normalized === 'key' || normalized === 'keyboard') && typeof args.inputAction === 'string') {
    const normalizedInputAction = args.inputAction.trim().toLowerCase();
    const mappedActionType = INPUT_TYPE_ALIASES[normalizedInputAction] ?? normalizedInputAction;
    if (mappedActionType === 'key_down' || mappedActionType === 'key_up') {
      return mappedActionType;
    }
  }

  const mappedType = INPUT_TYPE_ALIASES[normalized] ?? normalized;
  if (!SUPPORTED_INPUT_TYPES.has(mappedType)) {
    throw new Error(`Unknown input type: ${inputTypeValue}. Supported: key_down, key_up, mouse_click, mouse_move`);
  }

  return mappedType;
}

function getScreenshotMode(args: EditorArgs): { mode?: string; error?: string } {
  if (typeof args.mode !== 'string' || args.mode.trim() === '') {
    return {};
  }

  const mode = args.mode.trim().toLowerCase();
  if (!SUPPORTED_SCREENSHOT_MODES.has(mode)) {
    return { error: `Unknown screenshot mode: ${args.mode}. Supported: editor_viewport, game_viewport, full_editor_window` };
  }

  return { mode };
}

export async function handleEditorTools(action: string, args: EditorArgs, tools: ITools) {
  // Normalize action name for test compatibility
  const normalizedAction = normalizeEditorAction(action);

  // Validate arguments for this action
  const argsRecord = EDITOR_ASSET_PATH_ACTIONS.has(normalizedAction)
    ? normalizePathFields(args as Record<string, unknown>, EDITOR_PATH_FIELDS)
    : args as Record<string, unknown>;
  validateEditorActionArgs(normalizedAction, argsRecord);
  const editorArgs = argsRecord as EditorArgs;

  switch (normalizedAction) {
    case 'play': {
      const res = await executeAutomationRequest(tools, 'control_editor', { action: 'play' }, undefined, { timeoutMs: args.timeoutMs }) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'stop':
    case 'stop_pie': {
      const res = await executeAutomationRequest(tools, 'control_editor', { action: 'stop' }) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'eject': {
      // CRITICAL FIX: Removed redundant isInPIE() check that caused race condition.
      // PIE state is now validated atomically in the C++ handler (HandleControlEditorEject)
      // which checks GEditor->PlayWorld directly before executing the eject command.
      // This prevents the race where PIE stops between TS check and C++ execution.
      return await executeAutomationRequest(tools, 'control_editor', { action: 'eject' });
    }
    case 'possess': {
      // CRITICAL FIX: Removed redundant isInPIE() check that caused race condition.
      // PIE state is now validated atomically in the C++ handler (HandleControlEditorPossess)
      // which checks GEditor->PlayWorld directly before executing the possess command.
      // This prevents the race where PIE stops between TS check and C++ execution.
      return await executeAutomationRequest(tools, 'control_editor', args);
    }
    case 'pause': {
      const res = await executeAutomationRequest(tools, 'control_editor', { action: 'pause' }) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'resume': {
      const res = await executeAutomationRequest(tools, 'control_editor', { action: 'resume' }) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'screenshot': {
      const filename = args.filename ?? args.path;
      const modeResult = getScreenshotMode(args);
      if (modeResult.error) {
        return {
          success: false,
          type: 'INVALID_ARGUMENT',
          error: 'INVALID_ARGUMENT',
          message: modeResult.error,
          action: 'screenshot'
        };
      }

      const mode = modeResult.mode;
      const payload: Record<string, unknown> = { action: 'screenshot', filename, resolution: args.resolution };
      if (mode !== undefined) {
        payload.mode = mode;
      }
      if (typeof args.returnBase64 === 'boolean') {
        payload.returnBase64 = args.returnBase64;
      } else if (mode === 'full_editor_window' || mode === 'game_viewport') {
        payload.returnBase64 = true;
      }
      if (args.includeMetadata === true) {
        payload.includeMetadata = true;
      }
      if (args.includeMetadata === true && args.metadata !== undefined) {
        payload.metadata = args.metadata;
      }

      const targetAction = mode === 'game_viewport' ? 'system_control' : 'control_editor';
      const res = await executeAutomationRequest(tools, targetAction, payload) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'console_command': {
      const res = await executeAutomationRequest(tools, 'console_command', { command: args.command ?? '' }) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'set_camera': {
      const res = await executeAutomationRequest(tools, 'control_editor', { action: 'set_camera', location: args.location, rotation: args.rotation }) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'start_recording': {
      // Use console command as fallback if bridge doesn't support it
      const name = typeof args.name === 'string' ? args.name : undefined;
      const filename = args.filename || name || 'TestRecording';
      const frameRate = typeof args.frameRate === 'number' ? args.frameRate : undefined;
      const durationSeconds = typeof args.durationSeconds === 'number' ? args.durationSeconds : undefined;
      const metadata = args.metadata;

      const safeFilename = sanitizeCommandArgument(filename);

      // Try automation bridge first with all params
      try {
        const res = await executeAutomationRequest(tools, 'control_editor', {
          action: 'start_recording',
          filename, // JSON path - use raw value (JSON.stringify handles escaping)
          frameRate,
          durationSeconds,
          metadata
        });
        return cleanObject(res);
      } catch {
        if (!safeFilename) {
          return { success: false, error: 'Filename is required after sanitization', action: 'start_recording' };
        }
        // Fallback to console command
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
    case 'stop_recording': {
      const res = await executeAutomationRequest(tools, 'control_editor', { action: 'stop_recording' }) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'step_frame': {
      // Support stepping multiple frames
      const steps = typeof args.steps === 'number' && args.steps > 0 ? args.steps : 1;
      for (let i = 0; i < steps; i++) {
        await executeAutomationRequest(tools, 'control_editor', { action: 'step_frame' });
      }
      return { success: true, message: `Stepped ${steps} frame(s)`, action: 'step_frame', steps };
    }
    case 'create_bookmark': {
      const idx = typeof args.id === 'number' ? Math.trunc(args.id) : (parseInt(args.bookmarkName ?? '0') || 0);
      const res = await executeAutomationRequest(tools, 'control_editor', { action: 'create_bookmark', index: idx }) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'jump_to_bookmark': {
      const idx = typeof args.id === 'number' ? Math.trunc(args.id) : (parseInt(args.bookmarkName ?? '0') || 0);
      const res = await executeAutomationRequest(tools, 'control_editor', { action: 'jump_to_bookmark', index: idx }) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'set_preferences': {
      const res = await executeAutomationRequest(tools, 'control_editor', { action: 'set_preferences', category: args.category ?? '', preferences: args.preferences ?? {} }) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'open_asset': {
      const assetPath = requireNonEmptyString(editorArgs.assetPath || editorArgs.path, 'assetPath');
      const res = await executeAutomationRequest(tools, 'control_editor', { action: 'open_asset', assetPath });
      return cleanObject(res);
    }
    case 'execute_command': {
      const command = requireNonEmptyString(args.command, 'command');
      const res = await executeAutomationRequest(tools, 'console_command', { command }) as Record<string, unknown>;
      return { ...cleanObject(res), action: 'execute_command' };
    }
    case 'set_camera_fov': {
      const safeFov = sanitizeCommandArgument(String(args.fov));
      if (!safeFov) return { success: false, error: 'FOV is required after sanitization', action: 'set_camera_fov' };
      const res = await executeAutomationRequest(tools, 'control_editor', { action: 'set_camera_fov', fov: Number(safeFov) }) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'set_game_speed': {
      const safeSpeed = sanitizeCommandArgument(String(args.speed));
      if (!safeSpeed) return { success: false, error: 'Speed is required after sanitization', action: 'set_game_speed' };
      const res = await executeAutomationRequest(tools, 'control_editor', { action: 'set_game_speed', speed: Number(safeSpeed) }) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'set_view_mode': {
      const viewMode = requireNonEmptyString(args.viewMode, 'viewMode');
      const validModes = [
        'Lit', 'Unlit', 'Wireframe', 'DetailLighting', 'LightingOnly', 'Reflections',
        'OptimizationViewmodes', 'ShaderComplexity', 'LightmapDensity', 'StationaryLightOverlap', 'LightComplexity',
        'PathTracing', 'Visualizer', 'LODColoration', 'CollisionPawn', 'CollisionVisibility'
      ];
      if (!validModes.includes(viewMode)) {
        throw new Error(`unknown_viewmode: ${viewMode}. Must be one of: ${validModes.join(', ')}`);
      }
      const res = await executeAutomationRequest(tools, 'control_editor', { action: 'set_view_mode', viewMode }) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'set_viewport_resolution': {
      const width = Number(args.width);
      const height = Number(args.height);
      if (!Number.isFinite(width) || !Number.isFinite(height) || width <= 0 || height <= 0) {
        return {
          success: false,
          error: 'VALIDATION_ERROR',
          message: 'Width and height must be positive numbers',
          action: 'set_viewport_resolution'
        };
      }
      const res = await executeAutomationRequest(tools, 'console_command', { command: `r.SetRes ${width}x${height}` }) as Record<string, unknown>;
      return cleanObject({ ...res, action: 'set_viewport_resolution', width, height });
    }
    case 'set_viewport_realtime': {
      const enabled = args.enabled !== undefined ? args.enabled : (args.realtime !== false);
      const res = await executeAutomationRequest(tools, 'control_editor', { action: 'set_viewport_realtime', enabled, realtime: enabled }) as Record<string, unknown>;
      return cleanObject(res);
    }
    // Additional handlers for test compatibility
    case 'close_asset': {
      const assetPath = requireNonEmptyString(editorArgs.assetPath || editorArgs.path, 'assetPath');
      const res = await executeAutomationRequest(tools, 'control_editor', { action: 'close_asset', assetPath });
      return cleanObject(res);
    }
    case 'save_all': {
      const res = await executeAutomationRequest(tools, 'control_editor', { action: 'save_all' });
      return cleanObject(res);
    }
    case 'undo': {
      const res = await executeAutomationRequest(tools, 'control_editor', { action: 'undo' }) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'redo': {
      const res = await executeAutomationRequest(tools, 'control_editor', { action: 'redo' }) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'set_editor_mode': {
      const mode = requireNonEmptyString(args.mode, 'mode');
      const res = await executeAutomationRequest(tools, 'control_editor', { action: 'set_editor_mode', mode });
      return cleanObject(res);
    }
    case 'show_stats': {
      const res = await executeAutomationRequest(tools, 'control_editor', { action: 'show_stats', stat: args.stat }) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'hide_stats': {
      const res = await executeAutomationRequest(tools, 'control_editor', { action: 'hide_stats', stat: args.stat }) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'set_game_view': {
      const enabled = args.enabled !== false;
      const res = await executeAutomationRequest(tools, 'control_editor', { action: 'set_game_view', enabled }) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'set_immersive_mode': {
      const enabled = args.enabled !== false;
      const res = await executeAutomationRequest(tools, 'control_editor', { action: 'set_immersive_mode', enabled }) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'set_fixed_delta_time': {
      const deltaTime = typeof args.deltaTime === 'number' ? args.deltaTime : 0.01667;
      const res = await executeAutomationRequest(tools, 'control_editor', { action: 'set_fixed_delta_time', deltaTime }) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'open_level': {
      // Accept 'assetPath' as alias since users commonly think of level paths as asset paths
      const levelPath = requireNonEmptyString(editorArgs.levelPath || editorArgs.path || editorArgs.assetPath, 'levelPath');
      const res = await executeAutomationRequest(tools, 'control_editor', { action: 'open_level', levelPath });
      return cleanObject(res);
    }
    case 'simulate_input': {
      const mappedType = getInputType(args);

      const res = await executeAutomationRequest(tools, 'control_editor', {
        action: 'simulate_input',
        type: mappedType,
        key: args.key,
        x: args.x,
        y: args.y,
        button: args.button
      });
      return cleanObject(res);
    }
    case 'focus':
    case 'focus_actor': {
      const actorName = requireNonEmptyString(args.actorName || args.name, 'actorName');
      const res = await executeAutomationRequest(tools, 'control_editor', { action: 'focus_actor', actorName });
      return cleanObject(res);
    }
    default:
      return await executeAutomationRequest(tools, 'control_editor', args);
  }
}
