/**
 * Volume Handlers (Phase 24)
 *
 * Complete volume and trigger system management including:
 * - Trigger Volumes: trigger_volume, trigger_box, trigger_sphere, trigger_capsule
 * - Gameplay Volumes: blocking, kill_z, pain_causing, physics, audio, reverb
 * - Rendering Volumes: cull_distance, precomputed_visibility, lightmass_importance
 * - Navigation Volumes: nav_mesh_bounds, nav_modifier, camera_blocking
 * - Volume Configuration: extent, properties
 *
 * @module volume-handlers
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
    'volumePath', 'reverbEffect', 'damageType', 'areaClass'
  ];

  for (const field of pathFields) {
    const value = result[field];
    if (typeof value === 'string' && value.length > 0) {
      let normalized = value.replace(/\\/g, '/');
      // Replace /Content/ with /Game/ for common user mistake
      if (normalized.startsWith('/Content/')) {
        normalized = '/Game/' + normalized.slice('/Content/'.length);
      }
      // Ensure path starts with /Game/ if it doesn't start with a valid root
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
 * Handles all volume actions for the manage_volumes tool.
 */
export async function handleVolumeTools(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<HandlerResult> {
  // Normalize path fields before sending to C++
  const argsRecord = normalizePathFields(args as Record<string, unknown>);
  const timeoutMs = getTimeoutMs();

  // All actions are dispatched to C++ via automation bridge
  const sendRequest = async (actionName: string): Promise<HandlerResult> => {
    const payload = { ...argsRecord, action: actionName };
    const result = await executeAutomationRequest(
      tools,
      'manage_volumes',
      payload as HandlerArgs,
      `Automation bridge not available for volume action: ${actionName}`,
      { timeoutMs }
    );
    return cleanObject(result) as HandlerResult;
  };


  switch (action) {
    // ========================================================================
    // Trigger Volumes (4 actions)
    // ========================================================================
    case 'create_trigger_volume':
      return sendRequest('create_trigger_volume');

    case 'create_trigger_box':
      return sendRequest('create_trigger_box');

    case 'create_trigger_sphere':
      return sendRequest('create_trigger_sphere');

    case 'create_trigger_capsule':
      return sendRequest('create_trigger_capsule');

    // ========================================================================
    // Gameplay Volumes (11 actions)
    // ========================================================================
    case 'create_blocking_volume':
      return sendRequest('create_blocking_volume');

    case 'create_kill_z_volume':
      return sendRequest('create_kill_z_volume');

    case 'create_pain_causing_volume':
      return sendRequest('create_pain_causing_volume');

    case 'create_physics_volume':
      return sendRequest('create_physics_volume');

    case 'create_audio_volume':
      return sendRequest('create_audio_volume');

    case 'create_reverb_volume':
      return sendRequest('create_reverb_volume');

    case 'create_cull_distance_volume':
      return sendRequest('create_cull_distance_volume');

    case 'create_precomputed_visibility_volume':
      return sendRequest('create_precomputed_visibility_volume');

    case 'create_lightmass_importance_volume':
      return sendRequest('create_lightmass_importance_volume');

    case 'create_nav_mesh_bounds_volume':
      return sendRequest('create_nav_mesh_bounds_volume');

    case 'create_nav_modifier_volume':
      return sendRequest('create_nav_modifier_volume');

    case 'create_camera_blocking_volume':
      return sendRequest('create_camera_blocking_volume');

    // ========================================================================
    // Volume Configuration (2 actions)
    // ========================================================================
    case 'set_volume_extent':
      return sendRequest('set_volume_extent');

    case 'set_volume_properties':
      return sendRequest('set_volume_properties');

    // ========================================================================
    // Utility (1 action)
    // ========================================================================
    case 'get_volumes_info':
      return sendRequest('get_volumes_info');

    // ========================================================================
    // Splines (merged from manage_splines)
    // ========================================================================
    case 'create_spline_actor':
    case 'add_spline_point':
    case 'remove_spline_point':
    case 'set_spline_point_position':
    case 'set_spline_point_tangents':
    case 'set_spline_point_rotation':
    case 'set_spline_point_scale':
    case 'set_spline_type':
    case 'create_spline_mesh_component':
    case 'set_spline_mesh_asset':
    case 'configure_spline_mesh_axis':
    case 'set_spline_mesh_material':
    case 'scatter_meshes_along_spline':
    case 'configure_mesh_spacing':
    case 'configure_mesh_randomization':
    case 'create_road_spline':
    case 'create_river_spline':
    case 'create_fence_spline':
    case 'create_wall_spline':
    case 'create_cable_spline':
    case 'create_pipe_spline':
    case 'get_splines_info':
      return sendRequest(action);

    default:

      return cleanObject({
        success: false,
        error: 'UNKNOWN_ACTION',
        message: `Unknown volume action: ${action}`
      });
  }
}
