/**
 * UI Handlers
 * Runtime UI management: spawn widgets, hierarchy, viewport control.
 * 7 actions for widget management and input mode control.
 */

import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import type { HandlerResult } from '../../types/handler-types.js';
import { executeAutomationRequest } from './common-handlers.js';

/**
 * Main handler for manage_ui tool
 */
export async function handleUiTools(
  action: string,
  args: Record<string, unknown>,
  tools: ITools
): Promise<HandlerResult> {
  // Build the payload for automation request
  const payload: Record<string, unknown> = {
    action_type: action,
    ...args
  };

  // Remove undefined values
  Object.keys(payload).forEach(key => {
    if (payload[key] === undefined) {
      delete payload[key];
    }
  });

  switch (action) {
    // =========================================
    // WIDGET MANAGEMENT (4 actions)
    // =========================================
    case 'create_widget':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_ui',
        payload,
        'Automation bridge not available for create_widget'
      )) as HandlerResult;

    case 'remove_widget_from_viewport':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_ui',
        payload,
        'Automation bridge not available for remove_widget_from_viewport'
      )) as HandlerResult;

    case 'get_all_widgets':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_ui',
        payload,
        'Automation bridge not available for get_all_widgets'
      )) as HandlerResult;

    case 'get_widget_hierarchy':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_ui',
        payload,
        'Automation bridge not available for get_widget_hierarchy'
      )) as HandlerResult;

    case 'set_widget_visibility':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_ui',
        payload,
        'Automation bridge not available for set_widget_visibility'
      )) as HandlerResult;

    // =========================================
    // INPUT MODE & CURSOR (2 actions)
    // =========================================
    case 'set_input_mode':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_ui',
        payload,
        'Automation bridge not available for set_input_mode'
      )) as HandlerResult;

    case 'show_mouse_cursor':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_ui',
        payload,
        'Automation bridge not available for show_mouse_cursor'
      )) as HandlerResult;

    default:
      return {
        success: false,
        error: `Unknown UI action: ${action}`
      };
  }
}
