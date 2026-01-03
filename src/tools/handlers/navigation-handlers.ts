/**
 * Navigation Handlers (Phase 25)
 *
 * Complete navigation mesh and pathfinding system management including:
 * - NavMesh: configure_nav_mesh_settings, set_nav_agent_properties, rebuild_navigation
 * - Nav Modifiers: create_nav_modifier_component, set_nav_area_class, configure_nav_area_cost
 * - Nav Links: create_nav_link_proxy, configure_nav_link, set_nav_link_type,
 *              create_smart_link, configure_smart_link_behavior
 * - Utility: get_navigation_info
 *
 * @module navigation-handlers
 */

import { ITools } from '../../types/tool-interfaces.js';
import { cleanObject } from '../../utils/safe-json.js';
import type { HandlerArgs } from '../../types/handler-types.js';
import { executeAutomationRequest } from './common-handlers.js';

function getTimeoutMs(): number {
  const envDefault = Number(process.env.MCP_AUTOMATION_REQUEST_TIMEOUT_MS ?? '120000');
  return Number.isFinite(envDefault) && envDefault > 0 ? envDefault : 120000;
}

/**
 * Normalize path fields to ensure they start with /Game/ and use forward slashes.
 * Returns a copy of the args with normalized paths.
 */
function normalizePathFields(args: Record<string, unknown>): Record<string, unknown> {
  const result = { ...args };
  const pathFields = [
    'navMeshPath', 'actorPath', 'blueprintPath', 'areaClass', 'areaClassToReplace',
    'enabledAreaClass', 'disabledAreaClass', 'obstacleAreaClass'
  ];

  for (const field of pathFields) {
    const value = result[field];
    if (typeof value === 'string' && value.length > 0) {
      let normalized = value.replace(/\\/g, '/');
      // Replace /Content/ with /Game/ for common user mistake
      if (normalized.startsWith('/Content/')) {
        normalized = '/Game/' + normalized.slice('/Content/'.length);
      }
      // Allow /Script/ paths for built-in UE classes (e.g., /Script/NavigationSystem.NavArea_Obstacle)
      // Allow plugin paths like /MyPlugin/Assets to pass through unchanged
      if (!normalized.startsWith('/')) {
        normalized = '/Game/' + normalized;
      }
      result[field] = normalized;
    }
  }

  return result;
}

/**
 * Handles all navigation actions for the manage_navigation tool.
 */
export async function handleNavigationTools(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<Record<string, unknown>> {
  // Normalize path fields before sending to C++
  const argsRecord = normalizePathFields(args as Record<string, unknown>);
  const timeoutMs = getTimeoutMs();

  // All actions are dispatched to C++ via automation bridge
  const sendRequest = async (subAction: string): Promise<Record<string, unknown>> => {
    const payload = { ...argsRecord, subAction };
    const result = await executeAutomationRequest(
      tools,
      'manage_navigation',
      payload as HandlerArgs,
      `Automation bridge not available for navigation action: ${subAction}`,
      { timeoutMs }
    );
    return cleanObject(result) as Record<string, unknown>;
  };

  switch (action) {
    // ========================================================================
    // NavMesh Configuration (3 actions)
    // ========================================================================
    case 'configure_nav_mesh_settings':
      return sendRequest('configure_nav_mesh_settings');

    case 'set_nav_agent_properties':
      return sendRequest('set_nav_agent_properties');

    case 'rebuild_navigation':
      return sendRequest('rebuild_navigation');

    // ========================================================================
    // Nav Modifiers (3 actions)
    // ========================================================================
    case 'create_nav_modifier_component':
      return sendRequest('create_nav_modifier_component');

    case 'set_nav_area_class':
      return sendRequest('set_nav_area_class');

    case 'configure_nav_area_cost':
      return sendRequest('configure_nav_area_cost');

    // ========================================================================
    // Nav Links (5 actions)
    // ========================================================================
    case 'create_nav_link_proxy':
      return sendRequest('create_nav_link_proxy');

    case 'configure_nav_link':
      return sendRequest('configure_nav_link');

    case 'set_nav_link_type':
      return sendRequest('set_nav_link_type');

    case 'create_smart_link':
      return sendRequest('create_smart_link');

    case 'configure_smart_link_behavior':
      return sendRequest('configure_smart_link_behavior');

    // ========================================================================
    // Utility (1 action)
    // ========================================================================
    case 'get_navigation_info':
      return sendRequest('get_navigation_info');

    default:
      return cleanObject({
        success: false,
        error: 'UNKNOWN_ACTION',
        message: `Unknown navigation action: ${action}`
      });
  }
}
