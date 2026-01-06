/**
 * Water Handlers (Phase 28)
 *
 * Complete water system for UE Water Plugin integration including:
 * - Water Body Creation: create_water_body_ocean, create_water_body_lake, create_water_body_river
 * - Water Configuration: configure_water_body, configure_water_waves
 * - Water Info: get_water_body_info, list_water_bodies
 *
 * Requires: Water Plugin (Experimental) enabled in Unreal Engine
 *
 * @module water-handlers
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
    'actorPath', 'waterMaterial', 'underwaterPostProcessMaterial',
    'waterInfoMaterial', 'waterMeshPath', 'materialPath',
    'lakeTransitionMaterialPath', 'oceanTransitionMaterialPath', 'waterZonePath'
  ];

  for (const field of pathFields) {
    const value = result[field];
    if (typeof value === 'string' && value.length > 0) {
      let normalized = value.replace(/\\/g, '/');
      // Replace /Content/ with /Game/ for common user mistake
      if (normalized.startsWith('/Content/')) {
        normalized = '/Game/' + normalized.slice('/Content/'.length);
      }
      // Allow /Script/ paths for built-in UE classes
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
 * Handles all water actions for the manage_water tool.
 */
export async function handleWaterTools(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<Record<string, unknown>> {
  // Normalize path fields before sending to C++
  const argsRecord = normalizePathFields(args as Record<string, unknown>);
  const timeoutMs = getTimeoutMs();

  // All actions are dispatched to C++ via automation bridge
  // C++ handler reads "action" field from payload
  const sendRequest = async (actionName: string): Promise<Record<string, unknown>> => {
    const payload = { ...argsRecord, action: actionName };
    const result = await executeAutomationRequest(
      tools,
      'manage_water',
      payload as HandlerArgs,
      `Automation bridge not available for water action: ${actionName}`,
      { timeoutMs }
    );
    return cleanObject(result) as Record<string, unknown>;
  };

  switch (action) {
    // ========================================================================
    // Water Body Creation (3 actions)
    // ========================================================================
    case 'create_water_body_ocean':
      return sendRequest('create_water_body_ocean');

    case 'create_water_body_lake':
      return sendRequest('create_water_body_lake');

    case 'create_water_body_river':
      return sendRequest('create_water_body_river');

    // ========================================================================
    // Water Configuration (2 actions)
    // ========================================================================
    case 'configure_water_body':
      return sendRequest('configure_water_body');

    case 'configure_water_waves':
      return sendRequest('configure_water_waves');

    // ========================================================================
    // Water Info (2 actions)
    // ========================================================================
    case 'get_water_body_info':
      return sendRequest('get_water_body_info');

    case 'list_water_bodies':
      return sendRequest('list_water_bodies');

    // ========================================================================
    // River Configuration (1 action)
    // ========================================================================
    case 'set_river_depth':
      return sendRequest('set_river_depth');

    // ========================================================================
    // Ocean Configuration (1 action)
    // ========================================================================
    case 'set_ocean_extent':
      return sendRequest('set_ocean_extent');

    // ========================================================================
    // Water Static Mesh (1 action)
    // ========================================================================
    case 'set_water_static_mesh':
      return sendRequest('set_water_static_mesh');

    // ========================================================================
    // River Transitions (1 action)
    // ========================================================================
    case 'set_river_transitions':
      return sendRequest('set_river_transitions');

    // ========================================================================
    // Water Zone Override (1 action)
    // ========================================================================
    case 'set_water_zone':
      return sendRequest('set_water_zone');

    // ========================================================================
    // Water Surface Info Queries (2 actions)
    // ========================================================================
    case 'get_water_surface_info':
      return sendRequest('get_water_surface_info');

    case 'get_wave_info':
      return sendRequest('get_wave_info');

    default:
      return cleanObject({
        success: false,
        error: 'UNKNOWN_ACTION',
        message: `Unknown water action: ${action}`
      });
  }
}
