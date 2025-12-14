import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import { executeAutomationRequest, requireNonEmptyString } from './common-handlers.js';

export async function handleEditorTools(action: string, args: any, tools: ITools) {
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
        return { success: false, error: 'NOT_IN_PIE', message: 'Cannot eject while not in PIE' };
      }
      return await executeAutomationRequest(tools, 'control_editor', { action: 'eject' });
    }
    case 'possess': {
      const inPie = await tools.editorTools.isInPIE();
      if (!inPie) {
        return { success: false, error: 'NOT_IN_PIE', message: 'Cannot possess actor while not in PIE' };
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
      const res = await tools.editorTools.executeConsoleCommand(args.command);
      return cleanObject(res);
    }
    case 'set_camera': {
      const res = await tools.editorTools.setViewportCamera(args.location, args.rotation);
      return cleanObject(res);
    }
    case 'start_recording': {
      // Use console command as fallback if bridge doesn't support it
      const filename = args.filename || 'TestRecording';
      await tools.editorTools.executeConsoleCommand(`DemoRec ${filename}`);
      return { success: true, message: `Started recording to ${filename}`, action: 'start_recording' };
    }
    case 'stop_recording': {
      await tools.editorTools.executeConsoleCommand('DemoStop');
      return { success: true, message: 'Stopped recording', action: 'stop_recording' };
    }
    case 'step_frame': {
      // Use console command for single frame advance
      await tools.editorTools.executeConsoleCommand('r.SingleFrameAdvance 1');
      return { success: true, message: 'Stepped frame', action: 'step_frame' };
    }
    case 'create_bookmark': {
      const idx = parseInt(args.bookmarkName) || 0;
      await tools.editorTools.executeConsoleCommand(`r.SetBookmark ${idx}`);
      return { success: true, message: `Created bookmark ${idx}`, action: 'create_bookmark' };
    }
    case 'jump_to_bookmark': {
      const idx = parseInt(args.bookmarkName) || 0;
      await tools.editorTools.executeConsoleCommand(`r.JumpToBookmark ${idx}`);
      return { success: true, message: `Jumped to bookmark ${idx}`, action: 'jump_to_bookmark' };
    }
    case 'set_preferences': {
      const res = await tools.editorTools.setEditorPreferences(args.category, args.preferences);
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
      await tools.editorTools.executeConsoleCommand(`r.SetRes ${args.width}x${args.height}`);
      return { success: true, message: `Set viewport resolution to ${args.width}x${args.height}`, action: 'set_viewport_resolution' };
    }
    case 'set_viewport_realtime': {
      const enabled = args.enabled !== undefined ? args.enabled : (args.realtime !== false);
      // Use console command since interface doesn't have setViewportRealtime
      await tools.editorTools.executeConsoleCommand(`r.ViewportRealtime ${enabled ? 1 : 0}`);
      return { success: true, message: `Set viewport realtime to ${enabled}`, action: 'set_viewport_realtime' };
    }
    default:
      return await executeAutomationRequest(tools, 'control_editor', args);
  }
}
