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
import type { HandlerArgs } from '../../types/handler-types.js';
import { createSubActionDispatcher } from './common-handlers.js';

/**
 * Handles all navigation actions for the manage_navigation tool.
 */
export async function handleNavigationTools(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<Record<string, unknown>> {
  const { sendRequest } = createSubActionDispatcher(tools, args, {
    toolName: 'manage_navigation',
    domainName: 'navigation',
    pathFields: [
      'navMeshPath', 'actorPath', 'blueprintPath', 'areaClass', 'areaClassToReplace',
      'enabledAreaClass', 'disabledAreaClass', 'obstacleAreaClass'
    ]
  });

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
      return {
        success: false,
        error: 'UNKNOWN_ACTION',
        message: `Unknown navigation action: ${action}`
      };
  }
}
