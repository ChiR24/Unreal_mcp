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
  'fit_blueprint_graph': ['assetPath'],
  'set_blueprint_graph_view': ['assetPath'],
  'jump_to_blueprint_node': ['assetPath'],
  'capture_blueprint_graph_review': ['assetPath', 'filename'],
  'set_widget_blueprint_mode': ['assetPath', 'mode'],
  'fit_widget_designer': ['assetPath'],
  'set_widget_designer_view': ['assetPath'],
  'select_widget_in_designer': ['assetPath'],
  'select_widgets_in_designer_rect': ['assetPath'],
  'focus_editor_surface': ['surface'],
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
  'open_level': ['levelPath', 'path', 'assetPath'],
  'fit_blueprint_graph': ['assetPath', 'graphName', 'scope', 'tabId', 'windowTitle'],
  'set_blueprint_graph_view': ['assetPath', 'graphName', 'viewLocation', 'delta', 'zoomAmount', 'preserveZoom', 'tabId', 'windowTitle'],
  'jump_to_blueprint_node': ['assetPath', 'graphName', 'nodeGuid', 'nodeName', 'nodeTitle', 'tabId', 'windowTitle'],
  'capture_blueprint_graph_review': ['assetPath', 'graphName', 'nodeGuid', 'nodeName', 'nodeTitle', 'scope', 'filename', 'tabId', 'windowTitle', 'includeMenus'],
  'set_widget_blueprint_mode': ['assetPath', 'mode', 'tabId', 'windowTitle'],
  'fit_widget_designer': ['assetPath', 'tabId', 'windowTitle'],
  'set_widget_designer_view': ['assetPath', 'viewLocation', 'delta', 'preserveZoom', 'tabId', 'windowTitle'],
  'select_widget_in_designer': ['assetPath', 'mode', 'widgetName', 'widgetPath', 'widgetObjectPath', 'templateObjectPath', 'appendOrToggle', 'tabId', 'windowTitle'],
  'select_widgets_in_designer_rect': ['assetPath', 'rect', 'appendOrToggle', 'tabId', 'windowTitle'],
  'focus_editor_surface': ['surface', 'assetPath', 'tabId', 'windowTitle'],
  'focus_actor': ['actorName', 'name'],
  'focus': ['actorName', 'name'],  // Normalized alias for focus_actor
  'set_camera': ['location', 'rotation', 'actorName'],
  'set_viewport_resolution': ['width', 'height'],
  'set_view_mode': ['viewMode'],
  'set_editor_mode': ['mode'],
  'set_camera_fov': ['fov'],
  'set_game_speed': ['speed'],
  'set_fixed_delta_time': ['deltaTime'],
  'screenshot': ['filename', 'resolution', 'name', 'windowTitle', 'tabId', 'mode', 'includeMenus'],
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
  'simulate_input': [
    'key', 'action', 'inputAction', 'inputType', 'type', 'axis', 'value',
    'assetPath',
    'tabId', 'windowTitle', 'captureScreenshots',
    'text', 'submit', 'button',
    'x', 'y', 'clientX', 'clientY',
    'wheelSteps', 'preciseDelta',
    'holdDurationMs', 'durationMs', 'holdBeforeMoveMs', 'holdAfterMoveMs', 'steps',
    'start', 'end'
  ],
};

/**
 * Normalize editor action names for test compatibility
 */
function normalizeEditorAction(action: string): string {
  return EDITOR_ACTION_ALIASES[action] ?? action;
}

function hasNonEmptyString(value: unknown): value is string {
  return typeof value === 'string' && value.trim() !== '';
}

function hasDesignerRect(value: unknown): value is Record<string, number> {
  if (!value || typeof value !== 'object') {
    return false;
  }

  const rect = value as Record<string, unknown>;
  return typeof rect.left === 'number' && Number.isFinite(rect.left) &&
    typeof rect.top === 'number' && Number.isFinite(rect.top) &&
    typeof rect.right === 'number' && Number.isFinite(rect.right) &&
    typeof rect.bottom === 'number' && Number.isFinite(rect.bottom) &&
    rect.right > rect.left && rect.bottom > rect.top;
}

function buildControlEditorPayload(
  action: string,
  args: EditorArgs,
  allowedFields: string[]
): Record<string, unknown> {
  const payload: Record<string, unknown> = { action };

  for (const field of allowedFields) {
    const value = args[field];
    if (value !== undefined) {
      payload[field] = value;
    }
  }

  return cleanObject(payload);
}

function validateSemanticNavigationArgs(
  action: string,
  args: Record<string, unknown>
): void {
  switch (action) {
    case 'set_blueprint_graph_view':
      if (args.viewLocation === undefined && args.delta === undefined) {
        throw new Error('control_editor:set_blueprint_graph_view requires viewLocation or delta');
      }
      break;
    case 'jump_to_blueprint_node':
      if (!hasNonEmptyString(args.nodeGuid) && !hasNonEmptyString(args.nodeName) && !hasNonEmptyString(args.nodeTitle)) {
        throw new Error('control_editor:jump_to_blueprint_node requires nodeGuid, nodeName, or nodeTitle');
      }
      break;
    case 'set_widget_designer_view':
      if (args.viewLocation === undefined && args.delta === undefined) {
        throw new Error('control_editor:set_widget_designer_view requires viewLocation or delta');
      }
      break;
    case 'select_widget_in_designer':
      if (!hasNonEmptyString(args.widgetName) &&
          !hasNonEmptyString(args.widgetPath) &&
          !hasNonEmptyString(args.widgetObjectPath) &&
          !hasNonEmptyString(args.templateObjectPath)) {
        throw new Error('control_editor:select_widget_in_designer requires widgetName, widgetPath, widgetObjectPath, or templateObjectPath');
      }
      break;
    case 'select_widgets_in_designer_rect':
      if (!hasDesignerRect(args.rect)) {
        throw new Error('control_editor:select_widgets_in_designer_rect requires rect with left, top, right, and bottom');
      }
      break;
    default:
      break;
  }
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
  validateSemanticNavigationArgs(normalizedAction, argsRecord);
  
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
      const res = await executeAutomationRequest(tools, 'console_command', { command: 'pause' }) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'resume': {
      const res = await executeAutomationRequest(tools, 'console_command', { command: 'pause' }) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'screenshot': {
      const targetWindowName = typeof args.windowTitle === 'string' && args.windowTitle.trim() !== ''
        ? args.windowTitle
        : args.name;
      const res = await executeAutomationRequest(
        tools,
        'control_editor',
        cleanObject({
          action: 'screenshot',
          filename: args.filename,
          resolution: args.resolution,
          mode: args.mode,
          includeMenus: args.includeMenus,
          name: targetWindowName,
          tabId: args.tabId
        })
      ) as Record<string, unknown>;
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
        await executeAutomationRequest(tools, 'console_command', { command: `DemoRec ${filename}` });
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
      await executeAutomationRequest(tools, 'console_command', { command: 'DemoStop' });
      return { success: true, message: 'Stopped recording', action: 'stop_recording' };
    }
    case 'step_frame': {
      // Support stepping multiple frames
      const steps = typeof args.steps === 'number' && args.steps > 0 ? args.steps : 1;
      for (let i = 0; i < steps; i++) {
        await executeAutomationRequest(tools, 'console_command', { command: 'r.SingleFrameAdvance 1' });
      }
      return { success: true, message: `Stepped ${steps} frame(s)`, action: 'step_frame', steps };
    }
    case 'create_bookmark': {
      const idx = parseInt(args.bookmarkName ?? '0') || 0;
      await executeAutomationRequest(tools, 'console_command', { command: `r.SetBookmark ${idx}` });
      return { success: true, message: `Created bookmark ${idx}`, action: 'create_bookmark' };
    }
    case 'jump_to_bookmark': {
      const idx = parseInt(args.bookmarkName ?? '0') || 0;
      await executeAutomationRequest(tools, 'console_command', { command: `r.JumpToBookmark ${idx}` });
      return { success: true, message: `Jumped to bookmark ${idx}`, action: 'jump_to_bookmark' };
    }
    case 'set_preferences': {
      const res = await executeAutomationRequest(tools, 'control_editor', { action: 'set_preferences', category: args.category ?? '', preferences: args.preferences ?? {} }) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'open_asset': {
      const assetPath = requireNonEmptyString(args.assetPath || args.path, 'assetPath');
      const res = await executeAutomationRequest(tools, 'control_editor', { action: 'open_asset', assetPath });
      return cleanObject(res);
    }
    case 'fit_blueprint_graph': {
      const res = await executeAutomationRequest(
        tools,
        'control_editor',
        buildControlEditorPayload('fit_blueprint_graph', args, ACTION_ALLOWED_PARAMS.fit_blueprint_graph)
      ) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'set_blueprint_graph_view': {
      const res = await executeAutomationRequest(
        tools,
        'control_editor',
        buildControlEditorPayload('set_blueprint_graph_view', args, ACTION_ALLOWED_PARAMS.set_blueprint_graph_view)
      ) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'jump_to_blueprint_node': {
      const res = await executeAutomationRequest(
        tools,
        'control_editor',
        buildControlEditorPayload('jump_to_blueprint_node', args, ACTION_ALLOWED_PARAMS.jump_to_blueprint_node)
      ) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'capture_blueprint_graph_review': {
      const res = await executeAutomationRequest(
        tools,
        'control_editor',
        buildControlEditorPayload('capture_blueprint_graph_review', args, ACTION_ALLOWED_PARAMS.capture_blueprint_graph_review)
      ) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'set_widget_blueprint_mode': {
      const res = await executeAutomationRequest(
        tools,
        'control_editor',
        buildControlEditorPayload('set_widget_blueprint_mode', args, ACTION_ALLOWED_PARAMS.set_widget_blueprint_mode)
      ) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'fit_widget_designer': {
      const res = await executeAutomationRequest(
        tools,
        'control_editor',
        buildControlEditorPayload('fit_widget_designer', args, ACTION_ALLOWED_PARAMS.fit_widget_designer)
      ) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'set_widget_designer_view': {
      const res = await executeAutomationRequest(
        tools,
        'control_editor',
        buildControlEditorPayload('set_widget_designer_view', args, ACTION_ALLOWED_PARAMS.set_widget_designer_view)
      ) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'select_widget_in_designer': {
      const res = await executeAutomationRequest(
        tools,
        'control_editor',
        buildControlEditorPayload('select_widget_in_designer', args, ACTION_ALLOWED_PARAMS.select_widget_in_designer)
      ) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'select_widgets_in_designer_rect': {
      const res = await executeAutomationRequest(
        tools,
        'control_editor',
        buildControlEditorPayload('select_widgets_in_designer_rect', args, ACTION_ALLOWED_PARAMS.select_widgets_in_designer_rect)
      ) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'focus_editor_surface': {
      const res = await executeAutomationRequest(
        tools,
        'control_editor',
        buildControlEditorPayload('focus_editor_surface', args, ACTION_ALLOWED_PARAMS.focus_editor_surface)
      ) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'execute_command': {
      const command = requireNonEmptyString(args.command, 'command');
      const res = await executeAutomationRequest(tools, 'console_command', { command }) as Record<string, unknown>;
      return { ...cleanObject(res), action: 'execute_command' };
    }
    case 'set_camera_fov': {
      await executeAutomationRequest(tools, 'console_command', { command: `fov ${args.fov}` });
      return { success: true, message: `Set FOV to ${args.fov}`, action: 'set_camera_fov' };
    }
    case 'set_game_speed': {
      await executeAutomationRequest(tools, 'console_command', { command: `slomo ${args.speed}` });
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
      await executeAutomationRequest(tools, 'console_command', { command: `viewmode ${viewMode}` });
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
      await executeAutomationRequest(tools, 'console_command', { command: `r.SetRes ${width}x${height}` });
      return { success: true, message: `Set viewport resolution to ${width}x${height}`, action: 'set_viewport_resolution' };
    }
    case 'set_viewport_realtime': {
      const enabled = args.enabled !== undefined ? args.enabled : (args.realtime !== false);
      // Use console command since interface doesn't have setViewportRealtime
      await executeAutomationRequest(tools, 'console_command', { command: `r.ViewportRealtime ${enabled ? 1 : 0}` });
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
      await executeAutomationRequest(tools, 'console_command', { command: 'Undo' });
      return { success: true, message: 'Undo executed', action: 'undo' };
    }
    case 'redo': {
      await executeAutomationRequest(tools, 'console_command', { command: 'Redo' });
      return { success: true, message: 'Redo executed', action: 'redo' };
    }
    case 'set_editor_mode': {
      const mode = requireNonEmptyString(args.mode, 'mode');
      const res = await executeAutomationRequest(tools, 'control_editor', { action: 'set_editor_mode', mode });
      return cleanObject(res);
    }
    case 'show_stats': {
      await executeAutomationRequest(tools, 'console_command', { command: 'Stat FPS' });
      await executeAutomationRequest(tools, 'console_command', { command: 'Stat Unit' });
      return { success: true, message: 'Stats displayed', action: 'show_stats' };
    }
    case 'hide_stats': {
      await executeAutomationRequest(tools, 'console_command', { command: 'Stat None' });
      return { success: true, message: 'Stats hidden', action: 'hide_stats' };
    }
    case 'set_game_view': {
      const enabled = args.enabled !== false;
      await executeAutomationRequest(tools, 'console_command', { command: `ToggleGameView ${enabled ? 1 : 0}` });
      return { success: true, message: `Game view ${enabled ? 'enabled' : 'disabled'}`, action: 'set_game_view' };
    }
    case 'set_immersive_mode': {
      const enabled = args.enabled !== false;
      await executeAutomationRequest(tools, 'console_command', { command: `ToggleImmersion ${enabled ? 1 : 0}` });
      return { success: true, message: `Immersive mode ${enabled ? 'enabled' : 'disabled'}`, action: 'set_immersive_mode' };
    }
    case 'set_fixed_delta_time': {
      const deltaTime = typeof args.deltaTime === 'number' ? args.deltaTime : 0.01667;
      await executeAutomationRequest(tools, 'console_command', { command: `r.FixedDeltaTime ${deltaTime}` });
      return { success: true, message: `Fixed delta time set to ${deltaTime}`, action: 'set_fixed_delta_time' };
    }
    case 'open_level': {
      // Accept 'assetPath' as alias since users commonly think of level paths as asset paths
      const levelPath = requireNonEmptyString(args.levelPath || args.path || args.assetPath, 'levelPath');
      const res = await executeAutomationRequest(tools, 'control_editor', { action: 'open_level', levelPath });
      return cleanObject(res);
    }
    case 'simulate_input': {
      const inputActionValue = args.inputAction ?? args.inputType ?? args.type ?? '';
      const inputType = typeof inputActionValue === 'string' ? inputActionValue.toLowerCase() : '';
      let mappedType = inputType;

      if (inputType === 'pressed' || inputType === 'down') {
        mappedType = 'key_down';
      } else if (inputType === 'released' || inputType === 'up') {
        mappedType = 'key_up';
      } else if (inputType === 'click') {
        mappedType = 'mouse_click';
      } else if (inputType === 'move') {
        mappedType = 'mouse_move';
      } else if (inputType === 'drag') {
        mappedType = 'mouse_drag';
      } else if (inputType === 'wheel' || inputType === 'scroll') {
        mappedType = 'mouse_wheel';
      } else if (inputType === 'reset' || inputType === 'clear') {
        mappedType = 'reset_input';
      }

      const res = await executeAutomationRequest(tools, 'control_editor', { 
        action: 'simulate_input',
        type: mappedType,
        assetPath: args.assetPath,
        key: args.key,
        axis: args.axis,
        value: args.value,
        tabId: args.tabId,
        windowTitle: args.windowTitle,
        captureScreenshots: args.captureScreenshots,
        text: args.text,
        submit: args.submit,
        button: args.button,
        x: args.x,
        y: args.y,
        clientX: args.clientX,
        clientY: args.clientY,
        wheelSteps: args.wheelSteps,
        preciseDelta: args.preciseDelta,
        holdDurationMs: args.holdDurationMs,
        durationMs: args.durationMs,
        holdBeforeMoveMs: args.holdBeforeMoveMs,
        holdAfterMoveMs: args.holdAfterMoveMs,
        steps: args.steps,
        start: args.start,
        end: args.end
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
