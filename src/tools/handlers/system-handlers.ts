import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import { executeAutomationRequest } from './common-handlers.js';

export async function handleSystemTools(action: string, args: any, tools: ITools) {
  const sysAction = String(action || '').toLowerCase();
  switch (sysAction) {
    case 'show_fps':
      await tools.systemTools.executeConsoleCommand(args.enabled !== false ? 'stat fps' : 'stat fps 0');
      return { success: true, message: `FPS display ${args.enabled !== false ? 'enabled' : 'disabled'}`, action: 'show_fps' };
    case 'profile':
      await tools.systemTools.executeConsoleCommand(args.enabled !== false ? 'stat unit' : 'stat unit 0');
      return { success: true, message: `Profiling ${args.enabled !== false ? 'enabled' : 'disabled'}`, action: 'profile' };
    case 'set_quality':
      const quality = args.quality || 'medium';
      const qVal = quality === 'high' || quality === 'epic' ? 3 : (quality === 'low' ? 0 : 1);
      await tools.systemTools.executeConsoleCommand(`sg.ViewDistanceQuality ${qVal}`);
      return { success: true, message: `Quality set to ${quality}`, action: 'set_quality' };
    case 'execute_command':
      return cleanObject(await tools.systemTools.executeConsoleCommand(args.command));
    case 'show_widget':
      return cleanObject(await tools.uiTools.showWidget(args.widgetPath));
    case 'set_cvar':
      await tools.systemTools.executeConsoleCommand(`${args.cvar} ${args.value}`);
      return { success: true, message: `CVar ${args.cvar} set to ${args.value}`, action: 'set_cvar' };
    case 'get_project_settings':
      return cleanObject(await tools.systemTools.getProjectSettings(args.section));
    case 'validate_assets':
      // Interface only supports single asset validation
      if (args.paths && args.paths.length > 0) {
        const results = [];
        for (const path of args.paths) {
          results.push(await tools.assetTools.validate({ assetPath: path }));
        }
        return { success: true, results, action: 'validate_assets' };
      }
      return { success: false, message: 'No paths provided for validation', action: 'validate_assets' };
    case 'play_sound':
      return cleanObject(await tools.audioTools.playSound(args.soundPath, args.volume, args.pitch));
    case 'screenshot':
      return cleanObject(await tools.editorTools.takeScreenshot(args.filename));
    case 'set_resolution': {
      const width = Number(args.width);
      const height = Number(args.height);
      if (!Number.isFinite(width) || !Number.isFinite(height) || width <= 0 || height <= 0) {
        throw new Error('Invalid resolution: width and height must be positive numbers');
      }
      const windowed = args.windowed !== false; // default to windowed=true
      const suffix = windowed ? 'w' : 'f';
      await tools.systemTools.executeConsoleCommand(`r.SetRes ${width}x${height}${suffix}`);
      return {
        success: true,
        message: `Resolution set to ${width}x${height} (${windowed ? 'windowed' : 'fullscreen'})`,
        action: 'set_resolution'
      };
    }
    case 'set_fullscreen': {
      const width = Number(args.width);
      const height = Number(args.height);
      if (!Number.isFinite(width) || !Number.isFinite(height) || width <= 0 || height <= 0) {
        throw new Error('Invalid resolution: width and height must be positive numbers');
      }
      const windowed = args.windowed === true; // default to fullscreen when omitted
      const suffix = windowed ? 'w' : 'f';
      await tools.systemTools.executeConsoleCommand(`r.SetRes ${width}x${height}${suffix}`);
      return {
        success: true,
        message: `Fullscreen mode set to ${width}x${height} (${windowed ? 'windowed' : 'fullscreen'})`,
        action: 'set_fullscreen'
      };
    }
    default:
      const res = await executeAutomationRequest(tools, 'system_control', args, 'Automation bridge not available for system control operations');
      return cleanObject(res);
  }
}

export async function handleConsoleCommand(args: any, tools: ITools) {
  const res = await executeAutomationRequest(tools, 'console_command', args, 'Automation bridge not available for console command operations');
  return cleanObject(res);
}
