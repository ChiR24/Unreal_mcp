import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import type { EditorArgs } from '../../types/handler-types.js';
import { executeAutomationRequest, requireNonEmptyString, validateExpectedParams, validateRequiredParams, validateArgsSecurity } from './common-handlers.js';

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
  'open_asset': ['assetPath'],
  'close_asset': ['assetPath'],
  'open_level': ['levelPath'],
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
  'screenshot': ['filename'],
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
  'open_level': ['levelPath', 'path'],
  'focus_actor': ['actorName', 'name'],
  'focus': ['actorName', 'name'],  // Normalized alias for focus_actor
  'set_camera': ['location', 'rotation', 'actorName'],
  'set_viewport_resolution': ['width', 'height'],
  'set_view_mode': ['viewMode'],
  'set_editor_mode': ['mode'],
  'set_camera_fov': ['fov'],
  'set_game_speed': ['speed'],
  'set_fixed_delta_time': ['deltaTime'],
  'screenshot': ['filename', 'resolution'],
  'set_preferences': ['category', 'preferences', 'section', 'key', 'value'],
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
  'simulate_input': ['key', 'action', 'axis', 'value'],
};

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

export async function handleEditorTools(action: string, args: EditorArgs, tools: ITools) {
  // Normalize action name for test compatibility
  const normalizedAction = normalizeEditorAction(action);
  
  // Validate arguments for this action
  const argsRecord = args as Record<string, unknown>;
  validateEditorActionArgs(normalizedAction, argsRecord);
  
  switch (normalizedAction) {
    case 'play': {
      const res = await tools.editorTools.playInEditor(args.timeoutMs);
      return cleanObject(res);
    }
    case 'stop':
    case 'stop_pie': {
      const res = await tools.editorTools.stopPlayInEditor();
      return cleanObject(res);
    }
    case 'eject': {
      const inPie = await tools.editorTools.isInPIE();
      if (!inPie) {
        throw new Error('Cannot eject while not in PIE');
      }
      return await executeAutomationRequest(tools, 'control_editor', { action: 'eject' });
    }
    case 'possess': {
      const inPie = await tools.editorTools.isInPIE();
      if (!inPie) {
        throw new Error('Cannot possess actor while not in PIE');
      }
      return await executeAutomationRequest(tools, 'control_editor', args);
    }
    case 'pause': {
      const res = await tools.editorTools.pausePlayInEditor();
      return cleanObject(res);
    }
    case 'resume': {
      const res = await tools.editorTools.resumePlayInEditor();
      return cleanObject(res);
    }
    case 'screenshot': {
      const res = await tools.editorTools.takeScreenshot(args.filename, args.resolution);
      return cleanObject(res);
    }
    case 'console_command': {
      const res = await tools.editorTools.executeConsoleCommand(args.command ?? '');
      return cleanObject(res);
    }
    case 'set_camera': {
      const res = await tools.editorTools.setViewportCamera(args.location, args.rotation);
      return cleanObject(res);
    }
    case 'start_recording': {
      // Use console command as fallback if bridge doesn't support it
      const filename = args.filename || 'TestRecording';
      const frameRate = typeof args.frameRate === 'number' ? args.frameRate : undefined;
      const durationSeconds = typeof args.durationSeconds === 'number' ? args.durationSeconds : undefined;
      const metadata = args.metadata;
      
      // Try automation bridge first with all params
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
        // Fallback to console command
        await tools.editorTools.executeConsoleCommand(`DemoRec ${filename}`);
        return { 
          success: true, 
          message: `Started recording to ${filename}`, 
          action: 'start_recording',
          filename,
          frameRate,
          durationSeconds
        };
      }
    }
    case 'stop_recording': {
      await tools.editorTools.executeConsoleCommand('DemoStop');
      return { success: true, message: 'Stopped recording', action: 'stop_recording' };
    }
    case 'step_frame': {
      // Support stepping multiple frames
      const steps = typeof args.steps === 'number' && args.steps > 0 ? args.steps : 1;
      for (let i = 0; i < steps; i++) {
        await tools.editorTools.executeConsoleCommand('r.SingleFrameAdvance 1');
      }
      return { success: true, message: `Stepped ${steps} frame(s)`, action: 'step_frame', steps };
    }
    case 'create_bookmark': {
      const idx = parseInt(args.bookmarkName ?? '0') || 0;
      await tools.editorTools.executeConsoleCommand(`r.SetBookmark ${idx}`);
      return { success: true, message: `Created bookmark ${idx}`, action: 'create_bookmark' };
    }
    case 'jump_to_bookmark': {
      const idx = parseInt(args.bookmarkName ?? '0') || 0;
      await tools.editorTools.executeConsoleCommand(`r.JumpToBookmark ${idx}`);
      return { success: true, message: `Jumped to bookmark ${idx}`, action: 'jump_to_bookmark' };
    }
    case 'set_preferences': {
      const res = await tools.editorTools.setEditorPreferences(args.category ?? '', args.preferences ?? {});
      return cleanObject(res);
    }
    case 'open_asset': {
      const assetPath = requireNonEmptyString(args.assetPath || args.path, 'assetPath');
      const res = await executeAutomationRequest(tools, 'control_editor', { action: 'open_asset', assetPath });
      return cleanObject(res);
    }
    case 'execute_command': {
      const command = requireNonEmptyString(args.command, 'command');
      const res = await tools.editorTools.executeConsoleCommand(command);
      return { ...cleanObject(res), action: 'execute_command' };
    }
    case 'set_camera_fov': {
      await tools.editorTools.executeConsoleCommand(`fov ${args.fov}`);
      return { success: true, message: `Set FOV to ${args.fov}`, action: 'set_camera_fov' };
    }
    case 'set_game_speed': {
      await tools.editorTools.executeConsoleCommand(`slomo ${args.speed}`);
      return { success: true, message: `Set game speed to ${args.speed}`, action: 'set_game_speed' };
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
      await tools.editorTools.executeConsoleCommand(`viewmode ${viewMode}`);
      return { success: true, message: `Set view mode to ${viewMode}`, action: 'set_view_mode' };
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
      await tools.editorTools.executeConsoleCommand(`r.SetRes ${width}x${height}`);
      return { success: true, message: `Set viewport resolution to ${width}x${height}`, action: 'set_viewport_resolution' };
    }
    case 'set_viewport_realtime': {
      const enabled = args.enabled !== undefined ? args.enabled : (args.realtime !== false);
      // Use console command since interface doesn't have setViewportRealtime
      await tools.editorTools.executeConsoleCommand(`r.ViewportRealtime ${enabled ? 1 : 0}`);
      return { success: true, message: `Set viewport realtime to ${enabled}`, action: 'set_viewport_realtime' };
    }
    // Additional handlers for test compatibility
    case 'close_asset': {
      const assetPath = requireNonEmptyString(args.assetPath || args.path, 'assetPath');
      const res = await executeAutomationRequest(tools, 'control_editor', { action: 'close_asset', assetPath });
      return cleanObject(res);
    }
    case 'save_all': {
      const res = await executeAutomationRequest(tools, 'control_editor', { action: 'save_all' });
      return cleanObject(res);
    }
    case 'undo': {
      await tools.editorTools.executeConsoleCommand('Undo');
      return { success: true, message: 'Undo executed', action: 'undo' };
    }
    case 'redo': {
      await tools.editorTools.executeConsoleCommand('Redo');
      return { success: true, message: 'Redo executed', action: 'redo' };
    }
    case 'set_editor_mode': {
      const mode = requireNonEmptyString(args.mode, 'mode');
      const res = await executeAutomationRequest(tools, 'control_editor', { action: 'set_editor_mode', mode });
      return cleanObject(res);
    }
    case 'show_stats': {
      await tools.editorTools.executeConsoleCommand('Stat FPS');
      await tools.editorTools.executeConsoleCommand('Stat Unit');
      return { success: true, message: 'Stats displayed', action: 'show_stats' };
    }
    case 'hide_stats': {
      await tools.editorTools.executeConsoleCommand('Stat None');
      return { success: true, message: 'Stats hidden', action: 'hide_stats' };
    }
    case 'set_game_view': {
      const enabled = args.enabled !== false;
      await tools.editorTools.executeConsoleCommand(`ToggleGameView ${enabled ? 1 : 0}`);
      return { success: true, message: `Game view ${enabled ? 'enabled' : 'disabled'}`, action: 'set_game_view' };
    }
    case 'set_immersive_mode': {
      const enabled = args.enabled !== false;
      await tools.editorTools.executeConsoleCommand(`ToggleImmersion ${enabled ? 1 : 0}`);
      return { success: true, message: `Immersive mode ${enabled ? 'enabled' : 'disabled'}`, action: 'set_immersive_mode' };
    }
    case 'set_fixed_delta_time': {
      const deltaTime = typeof args.deltaTime === 'number' ? args.deltaTime : 0.01667;
      await tools.editorTools.executeConsoleCommand(`r.FixedDeltaTime ${deltaTime}`);
      return { success: true, message: `Fixed delta time set to ${deltaTime}`, action: 'set_fixed_delta_time' };
    }
    case 'open_level': {
      const levelPath = requireNonEmptyString(args.levelPath || args.path, 'levelPath');
      const res = await executeAutomationRequest(tools, 'control_editor', { action: 'open_level', levelPath });
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
