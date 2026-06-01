import { ITools } from '../../types/tool-interfaces.js';
import type { HandlerArgs } from '../../types/handler-types.js';
import { createSubActionDispatcher } from './common-handlers.js';

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

  switch (action) {
    case 'create_pcg_graph':
      return sendRequest('create_pcg_graph');

    case 'create_pcg_subgraph':
      return sendRequest('create_pcg_subgraph');

    case 'add_pcg_node':
      return sendRequest('add_pcg_node');

    case 'connect_pcg_pins':
      return sendRequest('connect_pcg_pins');

    case 'set_pcg_node_settings':
      return sendRequest('set_pcg_node_settings');

    case 'add_landscape_data_node':
      return sendRequest('add_landscape_data_node');

    case 'add_spline_data_node':
      return sendRequest('add_spline_data_node');

    case 'add_volume_data_node':
      return sendRequest('add_volume_data_node');

    case 'add_actor_data_node':
      return sendRequest('add_actor_data_node');

    case 'add_texture_data_node':
      return sendRequest('add_texture_data_node');

    case 'add_surface_sampler':
      return sendRequest('add_surface_sampler');

    case 'add_mesh_sampler':
      return sendRequest('add_mesh_sampler');

    case 'add_spline_sampler':
      return sendRequest('add_spline_sampler');

    case 'add_volume_sampler':
      return sendRequest('add_volume_sampler');

    case 'add_bounds_modifier':
      return sendRequest('add_bounds_modifier');

    case 'add_density_filter':
      return sendRequest('add_density_filter');

    case 'add_height_filter':
      return sendRequest('add_height_filter');

    case 'add_slope_filter':
      return sendRequest('add_slope_filter');

    case 'add_distance_filter':
      return sendRequest('add_distance_filter');

    case 'add_bounds_filter':
      return sendRequest('add_bounds_filter');

    case 'add_self_pruning':
      return sendRequest('add_self_pruning');

    case 'add_transform_points':
      return sendRequest('add_transform_points');

    case 'add_project_to_surface':
      return sendRequest('add_project_to_surface');

    case 'add_copy_points':
      return sendRequest('add_copy_points');

    case 'add_merge_points':
      return sendRequest('add_merge_points');

    case 'add_static_mesh_spawner':
      return sendRequest('add_static_mesh_spawner');

    case 'add_actor_spawner':
      return sendRequest('add_actor_spawner');

    case 'add_spline_spawner':
      return sendRequest('add_spline_spawner');

    case 'execute_pcg_graph':
      return sendRequest('execute_pcg_graph');

    case 'set_pcg_partition_grid_size':
      return sendRequest('set_pcg_partition_grid_size');

    default:
      return {
        success: false,
        error: 'UNKNOWN_ACTION',
        message: `Unknown PCG action: ${action}`
      };
  }
}
