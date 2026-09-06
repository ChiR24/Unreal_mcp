/**
 * Navigation Handlers
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

import { ITools } from '../../../types/tools/tool-interfaces.js';
import type { HandlerArgs } from '../../../types/handlers/handler-types.js';
import { createSubActionDispatcher, createUnknownActionResponse } from '../foundation/dispatch/common-handlers.js';

const NAVIGATION_ACTIONS = new Set([
  'configure_nav_mesh_settings',
  'set_nav_agent_properties',
  'rebuild_navigation',
  'create_nav_modifier_component',
  'set_nav_area_class',
  'configure_nav_area_cost',
  'create_nav_link_proxy',
  'configure_nav_link',
  'set_nav_link_type',
  'create_smart_link',
  'configure_smart_link_behavior',
  'get_navigation_info',
]);

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

  if (NAVIGATION_ACTIONS.has(action)) {
    return sendRequest(action);
  }
  return createUnknownActionResponse(`Unknown navigation action: ${action}`);
}
