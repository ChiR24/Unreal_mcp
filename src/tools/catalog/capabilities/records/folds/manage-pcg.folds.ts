// Fold specs for manage_pcg. Data only; see ../shared/fold.ts.
import { byTarget } from '../shared/fold-spec.js';
import type { FoldSpec } from '../shared/fold-types.js';

export const MANAGE_PCG_FOLDS: readonly FoldSpec[] = [
  {
    primary: 'add_pcg_node', selector: 'nodeKind',
    summary: 'Add a node to a PCG graph (graphPath), chosen by nodeKind. node takes any PCG settings class by name (settingsClass or nodeType, such as PCGStaticMeshSpawnerSettings or StaticMeshSpawner; CLASS_NOT_FOUND otherwise). The other kinds add a ready node: surface_sampler, spline_sampler, mesh_sampler (meshPath, checked to load), volume_sampler, static_mesh_spawner (meshPath), actor_spawner (actorClass), spline_spawner, density_filter, distance_filter, height_filter (an attribute range filter), slope_filter (Normal To Density), bounds_filter (culls points outside actor bounds), bounds_modifier, the landscape, spline, actor, texture (texturePath) and volume data nodes, transform_points, copy_points, merge_points, project_to_surface and self_pruning. settings sets any property of the new node by name (INVALID_SETTINGS for one it lacks); nodeName titles it and x, y place it. The reply gives nodeId (for connect_pcg_pins), title, nodeType, settingsApplied and saved; saved unless save is false.',
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
    summary: 'Build a PCG graph, chosen by edit. create makes a graph asset and create_subgraph one to run inside another (with parentGraphPath it adds the Subgraph node there too); a graph already at the path is refused unless overwrite is true, which hands back that graph unchanged (created false). connect_pins wires an output pin of one node to an input pin of another (labels default to Out and In; PIN_NOT_FOUND lists the labels). set_node_settings changes an existing node (found by nodeId or nodeName) and refuses an edit that changes nothing. set_partition_grid_size sets the world or one component\'s generation grid and saves the level unless save is false. Graph edits save the graph unless save is false (saved).',
    topics: ['pcg graph', 'pcg subgraph', 'connect pcg pins', 'pcg node settings', 'partition grid size'],
    members: { create: 'create_pcg_graph', create_subgraph: 'create_pcg_subgraph', connect_pins: 'connect_pcg_pins', set_node_settings: 'set_pcg_node_settings', set_partition_grid_size: 'set_pcg_partition_grid_size' },
  },
];
