import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import type { UiArgs } from '../../types/handler-types.js';
import { executeAutomationRequest, validateArgsSecurity, validateExpectedParams } from './common-handlers.js';

const UI_ACTION_ALLOWED_PARAMS: Record<string, string[]> = {
  list_ui_targets: [],
  list_visible_windows: [],
  resolve_ui_target: ['identifier', 'tabId', 'windowTitle'],
  open_ui_target: ['identifier', 'tabId'],
  register_editor_command: ['name', 'label', 'tooltip', 'iconName', 'kind', 'command', 'assetPath', 'tabId'],
  add_menu_entry: ['menuName', 'sectionName', 'entryName', 'commandName', 'identifier', 'label', 'tooltip', 'iconName', 'command', 'assetPath', 'tabId']
};

function validateUiActionArgs(action: string, args: Record<string, unknown>): void {
  validateArgsSecurity({ action, ...args });

  const allowedParams = UI_ACTION_ALLOWED_PARAMS[action];
  if (allowedParams !== undefined) {
    validateExpectedParams(args, allowedParams, `manage_ui:${action}`);
  }

  if (action === 'open_ui_target' && typeof args.identifier !== 'string' && typeof args.tabId !== 'string') {
    throw new Error('open_ui_target requires identifier or tabId');
  }

  if (
    action === 'resolve_ui_target' &&
    typeof args.identifier !== 'string' &&
    typeof args.tabId !== 'string' &&
    typeof args.windowTitle !== 'string'
  ) {
    throw new Error('resolve_ui_target requires identifier or tabId or windowTitle');
  }
}

export async function handleUiTools(action: string, args: UiArgs, tools: ITools) {
  const argsRecord = args as Record<string, unknown>;
  validateUiActionArgs(action, argsRecord);

  const response = await executeAutomationRequest(
    tools,
    'manage_ui',
    {
      ...args,
      subAction: action
    },
    'Automation bridge not available for manage_ui operations'
  );

  return cleanObject(response as Record<string, unknown>);
}