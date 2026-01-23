import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import type { EditorArgs } from '../../types/handler-types.js';
import { executeAutomationRequest, requireNonEmptyString } from './common-handlers.js';

export async function handleEditorTools(action: string, args: EditorArgs, tools: ITools) {
  switch (action) {
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
        return { success: false, message: 'Editor not playing', error: 'NOT_PLAYING' };
      }
      return await executeAutomationRequest(tools, 'control_editor', { action: 'eject' });
    }
    case 'possess': {
      const inPie = await tools.editorTools.isInPIE();
      if (!inPie) {
        return { success: false, message: 'Editor not playing', error: 'NOT_PLAYING' };
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
      // Route to C++ bridge for proper demo recording
      const filename = args.filename || 'TestRecording';
      const res = await executeAutomationRequest(tools, 'control_editor', { 
        action: 'start_recording', 
        filename 
      });
      return cleanObject(res);
    }
    case 'stop_recording': {
      // Route to C++ bridge for proper demo stop
      const res = await executeAutomationRequest(tools, 'control_editor', { 
        action: 'stop_recording' 
      });
      return cleanObject(res);
    }
    case 'step_frame': {
      // Route to C++ bridge for proper frame stepping
      const res = await executeAutomationRequest(tools, 'control_editor', { 
        action: 'step_frame',
        steps: args.steps ?? 1 
      });
      return cleanObject(res);
    }
    case 'create_bookmark': {
      // Route to C++ bridge for proper bookmark creation
      const res = await executeAutomationRequest(tools, 'control_editor', { 
        action: 'create_bookmark',
        bookmarkName: args.bookmarkName ?? '0'
      });
      return cleanObject(res);
    }
    case 'jump_to_bookmark': {
      // Route to C++ bridge for proper bookmark jump
      const res = await executeAutomationRequest(tools, 'control_editor', { 
        action: 'jump_to_bookmark',
        bookmarkName: args.bookmarkName ?? '0'
      });
      return cleanObject(res);
    }
    case 'set_preferences': {
      // Route to C++ bridge for actual editor preference setting
      const res = await executeAutomationRequest(tools, 'control_editor', {
        action: 'set_preferences',
        category: args.category ?? '',
        preferences: args.preferences ?? {}
      });
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
      // Route to C++ bridge for proper FOV control
      const res = await executeAutomationRequest(tools, 'control_editor', {
        action: 'set_camera_fov',
        fov: args.fov
      });
      return cleanObject(res);
    }
    case 'set_game_speed': {
      // Route to C++ bridge for proper game speed control
      const res = await executeAutomationRequest(tools, 'control_editor', {
        action: 'set_game_speed',
        speed: args.speed
      });
      return cleanObject(res);
    }
    case 'set_view_mode': {
      const viewMode = requireNonEmptyString(args.viewMode, 'viewMode');
      // Route to C++ bridge for proper viewport mode setting
      const res = await executeAutomationRequest(tools, 'control_editor', {
        action: 'set_view_mode',
        viewMode
      });
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
      // Route to C++ bridge for proper viewport resolution control
      const res = await executeAutomationRequest(tools, 'control_editor', {
        action: 'set_viewport_resolution',
        width,
        height
      });
      return cleanObject(res);
    }
    case 'set_viewport_realtime': {
      const enabled = args.enabled !== undefined ? args.enabled : (args.realtime !== false);
      // Route to C++ bridge for proper viewport realtime toggle
      const res = await executeAutomationRequest(tools, 'control_editor', {
        action: 'set_viewport_realtime',
        enabled
      });
      return cleanObject(res);
    }
    case 'capture_viewport': {
      // G1: Advanced viewport screenshot capture
      // Supports: base64 return, file output, format options, HDR capture
      const res = await executeAutomationRequest(tools, 'control_editor', {
        action: 'capture_viewport',
        outputPath: args.outputPath,
        filename: args.filename,
        format: args.format ?? 'png',
        width: args.width,
        height: args.height,
        returnBase64: args.returnBase64 ?? false,
        captureHDR: args.captureHDR ?? false,
        showUI: args.showUI ?? false
      });
      return cleanObject(res);
    }
    // Wave 1.5-1.8: Batch Operation Actions
    case 'batch_execute': {
      // Sequential execution of multiple operations with stopOnError support
      const operations = args.operations;
      if (!Array.isArray(operations) || operations.length === 0) {
        throw new Error('control_editor.batch_execute: operations array is required and must not be empty');
      }
      // CRITICAL: Block recursive batch operations
      const forbiddenActions = new Set(['batch_execute', 'parallel_execute', 'queue_operations', 'flush_operation_queue']);
      for (const op of operations) {
        if (typeof op.action === 'string' && forbiddenActions.has(op.action)) {
          throw new Error(`control_editor.batch_execute: Recursive batch operation '${op.action}' is not allowed`);
        }
      }
      const res = await executeAutomationRequest(tools, 'control_editor', {
        action: 'batch_execute',
        operations,
        stopOnError: args.stopOnError ?? false
      });
      return cleanObject(res);
    }
    case 'parallel_execute': {
      // Parallel execution of operations with concurrency limit
      const operations = args.operations;
      if (!Array.isArray(operations) || operations.length === 0) {
        throw new Error('control_editor.parallel_execute: operations array is required and must not be empty');
      }
      // CRITICAL: Block recursive batch operations
      const forbiddenActions = new Set(['batch_execute', 'parallel_execute', 'queue_operations', 'flush_operation_queue']);
      for (const op of operations) {
        if (typeof op.action === 'string' && forbiddenActions.has(op.action)) {
          throw new Error(`control_editor.parallel_execute: Recursive batch operation '${op.action}' is not allowed`);
        }
      }
      // Enforce max concurrency of 10
      const maxConcurrency = Math.min(Math.max(1, Number(args.maxConcurrency) || 10), 10);
      const res = await executeAutomationRequest(tools, 'control_editor', {
        action: 'parallel_execute',
        operations,
        maxConcurrency,
        stopOnError: args.stopOnError ?? false
      });
      return cleanObject(res);
    }
    case 'queue_operations': {
      // Add operations to queue for later execution
      const operations = args.operations;
      if (!Array.isArray(operations) || operations.length === 0) {
        throw new Error('control_editor.queue_operations: operations array is required and must not be empty');
      }
      // CRITICAL: Block recursive batch operations
      const forbiddenActions = new Set(['batch_execute', 'parallel_execute', 'queue_operations', 'flush_operation_queue']);
      for (const op of operations) {
        if (typeof op.action === 'string' && forbiddenActions.has(op.action)) {
          throw new Error(`control_editor.queue_operations: Recursive batch operation '${op.action}' is not allowed`);
        }
      }
      const res = await executeAutomationRequest(tools, 'control_editor', {
        action: 'queue_operations',
        operations
      });
      return cleanObject(res);
    }
    case 'flush_operation_queue': {
      // Execute all queued operations
      const res = await executeAutomationRequest(tools, 'control_editor', {
        action: 'flush_operation_queue'
      });
      return cleanObject(res);
    }

    // Wave 1.1-1.4: Error Recovery Actions
    case 'get_last_error_details': {
      const includeStackTrace = args.includeStackTrace ?? false;
      const res = await executeAutomationRequest(tools, 'control_editor', {
        action: 'get_last_error_details',
        includeStackTrace
      });
      return cleanObject(res);
    }
    case 'suggest_fix_for_error': {
      const errorCode = requireNonEmptyString(args.errorCode, 'errorCode');
      const res = await executeAutomationRequest(tools, 'control_editor', {
        action: 'suggest_fix_for_error',
        errorCode
      });
      return cleanObject(res);
    }
    case 'validate_operation_preconditions': {
      const targetAction = requireNonEmptyString(args.targetAction || args.action, 'targetAction');
      const parameters = args.parameters || {};
      const res = await executeAutomationRequest(tools, 'control_editor', {
        action: 'validate_operation_preconditions',
        targetAction,
        parameters
      });
      return cleanObject(res);
    }
    case 'get_operation_history': {
      const limit = args.limit ?? 20;
      const filter = args.filter;
      const res = await executeAutomationRequest(tools, 'control_editor', {
        action: 'get_operation_history',
        limit,
        filter
      });
      return cleanObject(res);
    }

    // Wave 1.9-1.12: Introspection Actions
    case 'get_available_actions': {
      // TS-only: Read from tool definitions, no bridge call needed
      const tool = args.tool;
      const category = args.category;
      const includeExperimental = args.includeExperimental ?? false;
      
      // Import tool definitions and filter
      const { consolidatedToolDefinitions } = await import('../consolidated-tool-definitions.js');
      const actions: Array<{ tool: string; action: string; description: string; experimental?: boolean }> = [];
      
      for (const toolDef of consolidatedToolDefinitions) {
        if (tool && toolDef.name !== tool) continue;
        if (category && toolDef.category !== category) continue;
        if (toolDef.experimental && !includeExperimental) continue;
        
        const inputSchema = toolDef.inputSchema as { properties?: { action?: { enum?: string[] } } };
        const actionEnum = inputSchema?.properties?.action?.enum || [];
        
        for (const actionName of actionEnum) {
          actions.push({
            tool: toolDef.name,
            action: actionName,
            description: toolDef.description,
            experimental: toolDef.experimental
          });
        }
      }
      
      return { success: true, actions };
    }
    case 'explain_action_parameters': {
      // TS-only: Parse JSON schema for action
      const tool = requireNonEmptyString(args.tool, 'tool');
      const targetAction = requireNonEmptyString(args.targetAction || args.actionName, 'targetAction');
      
      const { consolidatedToolDefinitions } = await import('../consolidated-tool-definitions.js');
      const toolDef = consolidatedToolDefinitions.find(t => t.name === tool);
      
      if (!toolDef) {
        return { success: false, error: 'TOOL_NOT_FOUND', message: `Tool '${tool}' not found` };
      }
      
      const inputSchema = toolDef.inputSchema as { properties?: Record<string, unknown> };
      const properties = inputSchema?.properties || {};
      
      const parameters: Array<{ name: string; type: string; required: boolean; description?: string }> = [];
      
      for (const [name, prop] of Object.entries(properties)) {
        if (name === 'action') continue;
        const propObj = prop as { type?: string; description?: string };
        parameters.push({
          name,
          type: propObj.type || 'unknown',
          required: false,
          description: propObj.description
        });
      }
      
      return { success: true, tool, action: targetAction, parameters };
    }
    case 'get_class_hierarchy': {
      const className = requireNonEmptyString(args.className, 'className');
      const res = await executeAutomationRequest(tools, 'control_editor', {
        action: 'get_class_hierarchy',
        className
      });
      return cleanObject(res);
    }
    case 'validate_action_input': {
      // TS-only: Validate input against JSON schema
      const tool = requireNonEmptyString(args.tool, 'tool');
      const targetAction = requireNonEmptyString(args.targetAction || args.actionName, 'targetAction');
      const parameters = args.parameters || {};
      
      const { consolidatedToolDefinitions } = await import('../consolidated-tool-definitions.js');
      const toolDef = consolidatedToolDefinitions.find(t => t.name === tool);
      
      if (!toolDef) {
        return { success: false, valid: false, errors: [{ path: 'tool', message: `Tool '${tool}' not found` }] };
      }
      
      // Basic validation - check required properties
      const inputSchema = toolDef.inputSchema as { required?: string[]; properties?: Record<string, unknown> };
      const errors: Array<{ path: string; message: string }> = [];
      
      const required = inputSchema.required || [];
      for (const reqProp of required) {
        if (reqProp === 'action') continue;
        const params = parameters as Record<string, unknown>;
        if (!(reqProp in params)) {
          errors.push({ path: reqProp, message: `Missing required property: ${reqProp}` });
        }
      }
      
      return { success: true, valid: errors.length === 0, errors, tool, action: targetAction };
    }

    // Wave 1.15-1.16: Query Enhancement Actions
    case 'get_action_statistics': {
      const since = args.since;
      const res = await executeAutomationRequest(tools, 'control_editor', {
        action: 'get_action_statistics',
        since
      });
      return cleanObject(res);
    }
    case 'get_bridge_health': {
      const res = await executeAutomationRequest(tools, 'control_editor', {
        action: 'get_bridge_health'
      });
      return cleanObject(res);
    }

    // Wave 2.21-2.30: Editor Enhancement Actions
    case 'configure_megalights': {
      // UE 5.7+ feature - C++ will return feature unavailable on older versions
      const res = await executeAutomationRequest(tools, 'control_editor', {
        action: 'configure_megalights',
        enabled: args.enabled ?? true,
        maxLights: args.maxLights,
        shadowQuality: args.shadowQuality
      });
      return cleanObject(res);
    }
    case 'get_light_budget_stats': {
      // UE 5.7+ feature
      const res = await executeAutomationRequest(tools, 'control_editor', {
        action: 'get_light_budget_stats'
      });
      return cleanObject(res);
    }
    case 'convert_to_substrate': {
      // UE 5.7+ feature
      const materialPath = requireNonEmptyString(args.materialPath || args.assetPath, 'materialPath');
      const res = await executeAutomationRequest(tools, 'control_editor', {
        action: 'convert_to_substrate',
        materialPath
      });
      return cleanObject(res);
    }
    case 'batch_substrate_migration': {
      // UE 5.7+ feature
      const materialPaths = args.materialPaths || args.assetPaths;
      if (!Array.isArray(materialPaths) || materialPaths.length === 0) {
        throw new Error('control_editor.batch_substrate_migration: materialPaths array is required');
      }
      const res = await executeAutomationRequest(tools, 'control_editor', {
        action: 'batch_substrate_migration',
        materialPaths,
        stopOnError: args.stopOnError ?? false
      });
      return cleanObject(res);
    }
    case 'record_input_session': {
      const sessionName = args.sessionName || 'InputSession';
      const res = await executeAutomationRequest(tools, 'control_editor', {
        action: 'record_input_session',
        sessionName,
        duration: args.duration
      });
      return cleanObject(res);
    }
    case 'playback_input_session': {
      const sessionName = requireNonEmptyString(args.sessionName, 'sessionName');
      const res = await executeAutomationRequest(tools, 'control_editor', {
        action: 'playback_input_session',
        sessionName,
        speed: args.speed ?? 1.0
      });
      return cleanObject(res);
    }
    case 'capture_viewport_sequence': {
      const outputPath = requireNonEmptyString(args.outputPath, 'outputPath');
      const res = await executeAutomationRequest(tools, 'control_editor', {
        action: 'capture_viewport_sequence',
        outputPath,
        frameCount: args.frameCount ?? 30,
        frameRate: args.frameRate ?? 30,
        format: args.format ?? 'png'
      });
      return cleanObject(res);
    }
    case 'set_editor_mode': {
      const mode = requireNonEmptyString(args.mode, 'mode');
      const res = await executeAutomationRequest(tools, 'control_editor', {
        action: 'set_editor_mode',
        mode
      });
      return cleanObject(res);
    }
    case 'get_selection_info': {
      const res = await executeAutomationRequest(tools, 'control_editor', {
        action: 'get_selection_info',
        includeComponents: args.includeComponents ?? false
      });
      return cleanObject(res);
    }
    case 'toggle_realtime_rendering': {
      const enabled = args.enabled;
      const res = await executeAutomationRequest(tools, 'control_editor', {
        action: 'toggle_realtime_rendering',
        enabled
      });
      return cleanObject(res);
    }

    default:
      return await executeAutomationRequest(tools, 'control_editor', args);
  }
}
