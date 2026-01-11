/**
 * Phase 33: Testing & Quality Handlers
 * Handles automation tests, functional tests, profiling, and validation.
 */

import { cleanObject } from '../../utils/safe-json.js';
import type { HandlerResult } from '../../types/handler-types.js';
import { ITools } from '../../types/tool-interfaces.js';
import { executeAutomationRequest } from './common-handlers.js';

/**
 * Main handler for testing & quality tools
 */
export async function handleTestingTools(
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
    // AUTOMATION TESTS
    // =========================================
    case 'list_tests':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_testing',
        payload,
        'Automation bridge not available for list_tests'
      )) as HandlerResult;

    case 'run_tests':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_testing',
        payload,
        'Automation bridge not available for run_tests'
      )) as HandlerResult;

    case 'run_test':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_testing',
        payload,
        'Automation bridge not available for run_test'
      )) as HandlerResult;

    case 'get_test_results':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_testing',
        payload,
        'Automation bridge not available for get_test_results'
      )) as HandlerResult;

    case 'get_test_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_testing',
        payload,
        'Automation bridge not available for get_test_info'
      )) as HandlerResult;

    // =========================================
    // FUNCTIONAL TESTS
    // =========================================
    case 'list_functional_tests':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_testing',
        payload,
        'Automation bridge not available for list_functional_tests'
      )) as HandlerResult;

    case 'run_functional_test':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_testing',
        payload,
        'Automation bridge not available for run_functional_test'
      )) as HandlerResult;

    case 'get_functional_test_results':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_testing',
        payload,
        'Automation bridge not available for get_functional_test_results'
      )) as HandlerResult;

    // =========================================
    // PROFILING - TRACE
    // =========================================
    case 'start_trace':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_testing',
        payload,
        'Automation bridge not available for start_trace'
      )) as HandlerResult;

    case 'stop_trace':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_testing',
        payload,
        'Automation bridge not available for stop_trace'
      )) as HandlerResult;

    case 'get_trace_status':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_testing',
        payload,
        'Automation bridge not available for get_trace_status'
      )) as HandlerResult;

    // =========================================
    // PROFILING - VISUAL LOGGER
    // =========================================
    case 'enable_visual_logger':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_testing',
        payload,
        'Automation bridge not available for enable_visual_logger'
      )) as HandlerResult;

    case 'disable_visual_logger':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_testing',
        payload,
        'Automation bridge not available for disable_visual_logger'
      )) as HandlerResult;

    case 'get_visual_logger_status':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_testing',
        payload,
        'Automation bridge not available for get_visual_logger_status'
      )) as HandlerResult;

    // =========================================
    // PROFILING - STATS
    // =========================================
    case 'start_stats_capture':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_testing',
        payload,
        'Automation bridge not available for start_stats_capture'
      )) as HandlerResult;

    case 'stop_stats_capture':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_testing',
        payload,
        'Automation bridge not available for stop_stats_capture'
      )) as HandlerResult;

    case 'get_memory_report':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_testing',
        payload,
        'Automation bridge not available for get_memory_report'
      )) as HandlerResult;

    case 'get_performance_stats':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_testing',
        payload,
        'Automation bridge not available for get_performance_stats'
      )) as HandlerResult;

    // =========================================
    // VALIDATION
    // =========================================
    case 'validate_asset':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_testing',
        payload,
        'Automation bridge not available for validate_asset'
      )) as HandlerResult;

    case 'validate_assets_in_path':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_testing',
        payload,
        'Automation bridge not available for validate_assets_in_path'
      )) as HandlerResult;

    case 'validate_blueprint':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_testing',
        payload,
        'Automation bridge not available for validate_blueprint'
      )) as HandlerResult;

    case 'check_map_errors':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_testing',
        payload,
        'Automation bridge not available for check_map_errors'
      )) as HandlerResult;

    case 'fix_redirectors':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_testing',
        payload,
        'Automation bridge not available for fix_redirectors'
      )) as HandlerResult;

    case 'get_redirectors':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_testing',
        payload,
        'Automation bridge not available for get_redirectors'
      )) as HandlerResult;

    default:
      return { success: false, error: `Unknown manage_testing action: ${action}` };
  }
}
