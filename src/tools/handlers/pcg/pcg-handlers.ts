import { ITools } from '../../../types/tools/tool-interfaces.js';
import type { HandlerArgs } from '../../../types/handlers/handler-types.js';
import { createSubActionDispatcher, createUnknownActionResponse } from '../foundation/dispatch/common-handlers.js';

const PCG_ACTIONS = new Set([
  'create_pcg_graph',
  'create_pcg_subgraph',
  'add_pcg_node',
  'connect_pcg_pins',
  'set_pcg_node_settings',
  'add_landscape_data_node',
  'add_spline_data_node',
  'add_volume_data_node',
  'add_actor_data_node',
  'add_texture_data_node',
  'add_surface_sampler',
  'add_mesh_sampler',
  'add_spline_sampler',
  'add_volume_sampler',
  'add_bounds_modifier',
  'add_density_filter',
  'add_height_filter',
  'add_slope_filter',
  'add_distance_filter',
  'add_bounds_filter',
  'add_self_pruning',
  'add_transform_points',
  'add_project_to_surface',
  'add_copy_points',
  'add_merge_points',
  'add_static_mesh_spawner',
  'add_actor_spawner',
  'add_spline_spawner',
  'execute_pcg_graph',
  'set_pcg_partition_grid_size',
]);

export async function handlePCGTools(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<Record<string, unknown>> {
  const { sendRequest } = createSubActionDispatcher(tools, args, {
    toolName: 'manage_pcg',
    domainName: 'PCG',
    pathFields: ['graphPath', 'parentGraphPath', 'subgraphPath', 'assetPath', 'path', 'meshPath', 'texturePath']
  });

  if (PCG_ACTIONS.has(action)) {
    return sendRequest(action);
  }
  return createUnknownActionResponse(`Unknown PCG action: ${action}`);
}
