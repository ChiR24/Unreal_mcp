import { cleanObject } from '../../../utils/serialization/safe-json.js';
import type { ITools } from '../../../types/tools/tool-interfaces.js';
import type { HandlerArgs, SystemArgs } from '../../../types/handlers/handler-types.js';
import { executeAutomationRequest, validateArgsSecurity } from '../foundation/dispatch/common-handlers.js';
import {
  handleExecuteCommand,
  handleProfile,
  handleSetCvar,
  handleSetQuality,
  handleShowFps,
  handleShowStats
} from './system-display-handlers.js';
import {
  handleAddWidgetChild,
  handleCreateWidget,
  handleShowWidget
} from './system-widget-handlers.js';
import {
  handleExportAsset,
  handleGetProjectSettings,
  handleValidateAssets
} from './system-asset-handlers.js';
import { handlePlaySound } from './system-audio-handlers.js';
import {
  handleScreenshot,
  handleSetFullscreen,
  handleSetResolution
} from './system-viewport-handlers.js';
export { handleConsoleCommand } from './system-console-handlers.js';

type SystemActionHandler = (args: SystemArgs, tools: ITools) => Promise<Record<string, unknown>>;

const SYSTEM_ACTION_HANDLERS: Record<string, SystemActionHandler> = {
  show_fps: handleShowFps,
  profile: handleProfile,
  show_stats: handleShowStats,
  set_quality: handleSetQuality,
  execute_command: handleExecuteCommand,
  create_widget: handleCreateWidget,
  show_widget: handleShowWidget,
  add_widget_child: handleAddWidgetChild,
  set_cvar: handleSetCvar,
  get_project_settings: handleGetProjectSettings,
  validate_assets: handleValidateAssets,
  play_sound: handlePlaySound,
  screenshot: handleScreenshot,
  set_resolution: handleSetResolution,
  set_fullscreen: handleSetFullscreen,
  export_asset: handleExportAsset
};

export async function handleSystemTools(action: string, args: HandlerArgs, tools: ITools): Promise<Record<string, unknown>> {
  validateArgsSecurity(args);

  const systemArgs = args as SystemArgs;
  const systemAction = String(action || '').toLowerCase();
  const handler = SYSTEM_ACTION_HANDLERS[systemAction];

  if (handler) {
    return await handler(systemArgs, tools);
  }

  const response = await executeAutomationRequest(
    tools,
    'system_control',
    args,
    'Automation bridge not available for system control operations'
  );
  return cleanObject(response) as Record<string, unknown>;
}
