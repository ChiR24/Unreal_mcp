// Fold specs for manage_pcg. Data only; see ../shared/fold.ts.
import { byTarget } from '../shared/fold-spec.js';
import type { FoldSpec } from '../shared/fold-types.js';

export const MANAGE_PCG_FOLDS: readonly FoldSpec[] = [
  {
    primary: 'add_pcg_node', selector: 'nodeKind',
    summary: 'Add a node to a PCG graph: any settings class by name, or a typed node (samplers, spawners, filters, data nodes, point operations).',
    topics: ['pcg node', 'surface sampler', 'static mesh spawner', 'density filter', 'landscape data', 'spline sampler', 'transform points'],
    members: {
      node: 'add_pcg_node',
      ...byTarget('add_', ['add_surface_sampler', 'add_spline_sampler', 'add_mesh_sampler', 'add_volume_sampler', 'add_static_mesh_spawner', 'add_actor_spawner', 'add_spline_spawner',
        'add_density_filter', 'add_distance_filter', 'add_height_filter', 'add_slope_filter', 'add_bounds_filter', 'add_bounds_modifier',
        'add_landscape_data_node', 'add_spline_data_node', 'add_actor_data_node', 'add_texture_data_node', 'add_volume_data_node',
        'add_transform_points', 'add_copy_points', 'add_merge_points', 'add_project_to_surface', 'add_self_pruning']),
    },
  },
  {
    primary: 'edit_pcg_graph', selector: 'edit',
    summary: 'Create a PCG graph or subgraph, connect pins, set node settings, or set the partition grid size.',
    topics: ['pcg graph', 'pcg subgraph', 'connect pcg pins', 'pcg node settings', 'partition grid size'],
    members: { create: 'create_pcg_graph', create_subgraph: 'create_pcg_subgraph', connect_pins: 'connect_pcg_pins', set_node_settings: 'set_pcg_node_settings', set_partition_grid_size: 'set_pcg_partition_grid_size' },
  },
];
