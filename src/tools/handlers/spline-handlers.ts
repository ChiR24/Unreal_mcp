/**
 * Spline Handlers (Phase 26)
 *
 * Complete spline-based content creation system including:
 * - Spline Creation: create_spline_actor, add_spline_point, remove_spline_point, set_spline_point_position
 * - Spline Configuration: set_spline_point_tangents, set_spline_point_rotation, set_spline_point_scale, set_spline_type
 * - Spline Mesh: create_spline_mesh_component, create_spline_mesh_actor, set_spline_mesh_asset, configure_spline_mesh_axis, set_spline_mesh_material
 * - Spline Mesh Array: scatter_meshes_along_spline, configure_mesh_spacing, configure_mesh_randomization
 * - Quick Templates: create_road_spline, create_river_spline, create_fence_spline, create_wall_spline, create_cable_spline, create_pipe_spline
 * - Utility: get_splines_info
 *
 * @module spline-handlers
 */

import { ITools } from '../../types/tool-interfaces.js';
import type { HandlerArgs } from '../../types/handler-types.js';
import { createSubActionDispatcher } from './common-handlers.js';

/**
 * Handles all spline actions for the manage_splines tool.
 */
export async function handleSplineTools(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<Record<string, unknown>> {
  const { sendRequest } = createSubActionDispatcher(tools, args, {
    toolName: 'manage_splines',
    domainName: 'spline',
    pathFields: ['actorPath', 'blueprintPath', 'meshPath', 'materialPath', 'splinePath']
  });

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
    // Spline Mesh (5 actions)
    // ========================================================================
    case 'create_spline_mesh_component':
      return sendRequest('create_spline_mesh_component');

    case 'create_spline_mesh_actor':
      return sendRequest('create_spline_mesh_actor');

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
      return {
        success: false,
        error: 'UNKNOWN_ACTION',
        message: `Unknown spline action: ${action}`
      };
  }
}
