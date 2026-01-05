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
    default:
      return await executeAutomationRequest(tools, 'control_editor', args);
  }
}
