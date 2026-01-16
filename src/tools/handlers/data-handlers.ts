/**
 * Phase 31: Data & Persistence Handlers
 * Handles data assets, data tables, save games, gameplay tags, and config files.
 */

import { cleanObject } from '../../utils/safe-json.js';
import type { HandlerResult } from '../../types/handler-types.js';
import { ITools } from '../../types/tool-interfaces.js';
import { executeAutomationRequest } from './common-handlers.js';

/**
 * Main handler for data & persistence tools
 */
export async function handleDataTools(
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
    // DATA ASSETS
    // =========================================
    case 'create_data_asset':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_data',
        payload,
        'Automation bridge not available for create_data_asset'
      )) as HandlerResult;

    case 'create_primary_data_asset':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_data',
        payload,
        'Automation bridge not available for create_primary_data_asset'
      )) as HandlerResult;

    case 'get_data_asset_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_data',
        payload,
        'Automation bridge not available for get_data_asset_info'
      )) as HandlerResult;

    case 'set_data_asset_property':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_data',
        payload,
        'Automation bridge not available for set_data_asset_property'
      )) as HandlerResult;

    // =========================================
    // DATA TABLES
    // =========================================
    case 'create_data_table':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_data',
        payload,
        'Automation bridge not available for create_data_table'
      )) as HandlerResult;

    case 'add_data_table_row':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_data',
        payload,
        'Automation bridge not available for add_data_table_row'
      )) as HandlerResult;

    case 'remove_data_table_row':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_data',
        payload,
        'Automation bridge not available for remove_data_table_row'
      )) as HandlerResult;

    case 'get_data_table_row':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_data',
        payload,
        'Automation bridge not available for get_data_table_row'
      )) as HandlerResult;

    case 'get_data_table_rows':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_data',
        payload,
        'Automation bridge not available for get_data_table_rows'
      )) as HandlerResult;

    case 'import_data_table_csv':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_data',
        payload,
        'Automation bridge not available for import_data_table_csv'
      )) as HandlerResult;

    case 'export_data_table_csv':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_data',
        payload,
        'Automation bridge not available for export_data_table_csv'
      )) as HandlerResult;

    case 'empty_data_table':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_data',
        payload,
        'Automation bridge not available for empty_data_table'
      )) as HandlerResult;

    // =========================================
    // CURVE TABLES
    // =========================================
    case 'create_curve_table':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_data',
        payload,
        'Automation bridge not available for create_curve_table'
      )) as HandlerResult;

    case 'add_curve_row':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_data',
        payload,
        'Automation bridge not available for add_curve_row'
      )) as HandlerResult;

    case 'get_curve_value':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_data',
        payload,
        'Automation bridge not available for get_curve_value'
      )) as HandlerResult;

    case 'import_curve_table_csv':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_data',
        payload,
        'Automation bridge not available for import_curve_table_csv'
      )) as HandlerResult;

    case 'export_curve_table_csv':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_data',
        payload,
        'Automation bridge not available for export_curve_table_csv'
      )) as HandlerResult;

    // =========================================
    // SAVE GAME
    // =========================================
    case 'create_save_game_blueprint':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_data',
        payload,
        'Automation bridge not available for create_save_game_blueprint'
      )) as HandlerResult;

    case 'save_game_to_slot':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_data',
        payload,
        'Automation bridge not available for save_game_to_slot'
      )) as HandlerResult;

    case 'load_game_from_slot':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_data',
        payload,
        'Automation bridge not available for load_game_from_slot'
      )) as HandlerResult;

    case 'delete_save_slot':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_data',
        payload,
        'Automation bridge not available for delete_save_slot'
      )) as HandlerResult;

    case 'does_save_exist':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_data',
        payload,
        'Automation bridge not available for does_save_exist'
      )) as HandlerResult;

    case 'get_save_slot_names':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_data',
        payload,
        'Automation bridge not available for get_save_slot_names'
      )) as HandlerResult;

    // =========================================
    // GAMEPLAY TAGS
    // =========================================
    case 'create_gameplay_tag':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_data',
        payload,
        'Automation bridge not available for create_gameplay_tag'
      )) as HandlerResult;

    case 'add_native_gameplay_tag':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_data',
        payload,
        'Automation bridge not available for add_native_gameplay_tag'
      )) as HandlerResult;

    case 'request_gameplay_tag':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_data',
        payload,
        'Automation bridge not available for request_gameplay_tag'
      )) as HandlerResult;

    case 'check_tag_match':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_data',
        payload,
        'Automation bridge not available for check_tag_match'
      )) as HandlerResult;

    case 'create_tag_container':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_data',
        payload,
        'Automation bridge not available for create_tag_container'
      )) as HandlerResult;

    case 'add_tag_to_container':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_data',
        payload,
        'Automation bridge not available for add_tag_to_container'
      )) as HandlerResult;

    case 'remove_tag_from_container':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_data',
        payload,
        'Automation bridge not available for remove_tag_from_container'
      )) as HandlerResult;

    case 'has_tag':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_data',
        payload,
        'Automation bridge not available for has_tag'
      )) as HandlerResult;

    case 'get_all_gameplay_tags':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_data',
        payload,
        'Automation bridge not available for get_all_gameplay_tags'
      )) as HandlerResult;

    // =========================================
    // CONFIG
    // =========================================
    case 'read_config_value':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_data',
        payload,
        'Automation bridge not available for read_config_value'
      )) as HandlerResult;

    case 'write_config_value':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_data',
        payload,
        'Automation bridge not available for write_config_value'
      )) as HandlerResult;

    case 'get_config_section':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_data',
        payload,
        'Automation bridge not available for get_config_section'
      )) as HandlerResult;

    case 'flush_config':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_data',
        payload,
        'Automation bridge not available for flush_config'
      )) as HandlerResult;

    case 'reload_config':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_data',
        payload,
        'Automation bridge not available for reload_config'
      )) as HandlerResult;

    // =========================================
    // ANALYTICS
    // =========================================
    case 'log_analytics_event':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_data',
        payload,
        'Automation bridge not available for log_analytics_event'
      )) as HandlerResult;

    case 'get_session_analytics':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_data',
        payload,
        'Automation bridge not available for get_session_analytics'
      )) as HandlerResult;

    case 'export_analytics_data':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_data',
        payload,
        'Automation bridge not available for export_analytics_data'
      )) as HandlerResult;

    case 'configure_telemetry':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_data',
        payload,
        'Automation bridge not available for configure_telemetry'
      )) as HandlerResult;

    case 'get_crash_reports':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_data',
        payload,
        'Automation bridge not available for get_crash_reports'
      )) as HandlerResult;

    default:
      return {
        success: false,
        message: `Unknown manage_data action: ${action}`,
        error: 'UNKNOWN_ACTION'
      };
  }
}
