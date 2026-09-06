/**
 * Spline Handlers
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

import { ITools } from '../../../types/tools/tool-interfaces.js';
import type { HandlerArgs } from '../../../types/handlers/handler-types.js';
import { createSubActionDispatcher, createUnknownActionResponse } from '../foundation/dispatch/common-handlers.js';

const SPLINE_ACTIONS = new Set([
  'create_spline_actor',
  'add_spline_point',
  'remove_spline_point',
  'set_spline_point_position',
  'set_spline_point_tangents',
  'set_spline_point_rotation',
  'set_spline_point_scale',
  'set_spline_type',
  'create_spline_mesh_component',
  'create_spline_mesh_actor',
  'set_spline_mesh_asset',
  'configure_spline_mesh_axis',
  'set_spline_mesh_material',
  'scatter_meshes_along_spline',
  'configure_mesh_spacing',
  'configure_mesh_randomization',
  'create_road_spline',
  'create_river_spline',
  'create_fence_spline',
  'create_wall_spline',
  'create_cable_spline',
  'create_pipe_spline',
  'get_splines_info',
]);

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

  if (SPLINE_ACTIONS.has(action)) {
    return sendRequest(action);
  }
  return createUnknownActionResponse(`Unknown spline action: ${action}`);
}
