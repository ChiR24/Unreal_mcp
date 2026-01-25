/**
 * Spline Handlers (Phase 26)
 *
 * Complete spline-based content creation system including:
 * - Spline Creation: create_spline_actor, add_spline_point, remove_spline_point, set_spline_point_position
 * - Spline Configuration: set_spline_point_tangents, set_spline_point_rotation, set_spline_point_scale, set_spline_type
 * - Spline Mesh: create_spline_mesh_component, set_spline_mesh_asset, configure_spline_mesh_axis, set_spline_mesh_material
 * - Spline Mesh Array: scatter_meshes_along_spline, configure_mesh_spacing, configure_mesh_randomization
 * - Quick Templates: create_road_spline, create_river_spline, create_fence_spline, create_wall_spline, create_cable_spline, create_pipe_spline
 * - Utility: get_splines_info
 *
 * @module spline-handlers
 */

import { ITools } from '../../types/tool-interfaces.js';
import { cleanObject } from '../../utils/safe-json.js';
import type { HandlerArgs, HandlerResult } from '../../types/handler-types.js';
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
    'actorPath', 'blueprintPath', 'meshPath', 'materialPath', 'splinePath'
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
 * Handles all spline actions for the manage_splines tool.
 */
export async function handleSplineTools(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<HandlerResult> {
  // Normalize path fields before sending to C++
  const argsRecord = normalizePathFields(args as Record<string, unknown>);
  const timeoutMs = getTimeoutMs();

  // All actions are dispatched to C++ via automation bridge
  const sendRequest = async (subAction: string): Promise<HandlerResult> => {
    const payload = { ...argsRecord, subAction };
    const result = await executeAutomationRequest(
      tools,
      'manage_splines',
      payload as HandlerArgs,
      `Automation bridge not available for spline action: ${subAction}`,
      { timeoutMs }
    );
    return cleanObject(result) as HandlerResult;
  };

  switch (action) {
    // ========================================================================
    // Spline Creation (8 actions)
    // ========================================================================
    case 'create_spline_actor':
      return sendRequest('create_spline_actor');

    case 'add_spline_point':
      return sendRequest('add_spline_point');

    case 'remove_spline_point':
      return sendRequest('remove_spline_point');

    case 'set_spline_point_position':
      return sendRequest('set_spline_point_position');

    case 'set_spline_point_tangents':
      return sendRequest('set_spline_point_tangents');

    case 'set_spline_point_rotation':
      return sendRequest('set_spline_point_rotation');

    case 'set_spline_point_scale':
      return sendRequest('set_spline_point_scale');

    case 'set_spline_type':
      return sendRequest('set_spline_type');

    // ========================================================================
    // Spline Mesh (4 actions)
    // ========================================================================
    case 'create_spline_mesh_component':
      if (argsRecord.blueprintPath === undefined && argsRecord.path !== undefined) {
        argsRecord.blueprintPath = argsRecord.path;
      }
      return sendRequest('create_spline_mesh_component');

    case 'set_spline_mesh_asset':
      return sendRequest('set_spline_mesh_asset');

    case 'configure_spline_mesh_axis':
      return sendRequest('configure_spline_mesh_axis');

    case 'set_spline_mesh_material':
      return sendRequest('set_spline_mesh_material');

    // ========================================================================
    // Spline Mesh Array (3 actions)
    // ========================================================================
    case 'scatter_meshes_along_spline':
      return sendRequest('scatter_meshes_along_spline');

    case 'configure_mesh_spacing':
      return sendRequest('configure_mesh_spacing');

    case 'configure_mesh_randomization':
      return sendRequest('configure_mesh_randomization');

    // ========================================================================
    // Quick Templates (6 actions)
    // ========================================================================
    case 'create_road_spline':
      return sendRequest('create_road_spline');

    case 'create_river_spline':
      return sendRequest('create_river_spline');

    case 'create_fence_spline':
      return sendRequest('create_fence_spline');

    case 'create_wall_spline':
      return sendRequest('create_wall_spline');

    case 'create_cable_spline':
      return sendRequest('create_cable_spline');

    case 'create_pipe_spline':
      return sendRequest('create_pipe_spline');

    // ========================================================================
    // Utility (1 action)
    // ========================================================================
    case 'get_splines_info':
      return sendRequest('get_splines_info');

    default:
      return cleanObject({
        success: false,
        error: 'UNKNOWN_ACTION',
        message: `Unknown spline action: ${action}`
      });
  }
}
