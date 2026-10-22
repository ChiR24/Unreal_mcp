// Fold specs for manage_pcg. Data only; see ../shared/fold.ts.
import { byTarget } from '../shared/fold-spec.js';
import type { FoldSpec } from '../shared/fold-types.js';

export const MANAGE_PCG_FOLDS: readonly FoldSpec[] = [
  {
    primary: 'control_pcg_execution', selector: 'control',
    summary: 'Explicit native PCG lifecycle control: clean one component managed resources or request cancellation of an owned generation operation.',
    topics: ['clean pcg generated resources', 'cancel pcg generation', 'pcg execution control'],
    members: { cleanup: 'cleanup_pcg_component', cancel: 'cancel_pcg_operation' },
  },
  {
    primary: 'edit_pcg_parameters', selector: 'parameterEdit',
    summary: 'Add graph parameter schema/defaults, change defaults or instance overrides, rename stable parameters, or reset an override.',
    topics: ['edit pcg parameters', 'pcg component overrides', 'rename graph parameter', 'reset pcg override'],
    members: { add: 'add_pcg_parameter', set: 'set_pcg_parameter', rename: 'rename_pcg_parameter', reset_override: 'reset_pcg_parameter_override' },
  },
  {
    primary: 'mutate_pcg_graph', selector: 'mutation',
    summary: 'Explicit destructive PCG graph edits: disconnect pins, remove a node, or apply an atomic batch with destructive steps.',
    topics: ['disconnect pcg pins', 'remove pcg node', 'destructive pcg batch'],
    members: { disconnect: 'disconnect_pcg_pins', remove_node: 'remove_pcg_node', batch: 'batch_edit_pcg_graph_destructive' },
  },
  {
    primary: 'inspect_pcg', selector: 'view',
    summary: 'Discover PCG classes, graphs and schemas, inspect root outputs or bounded immutable typed data views, and validate native pins without generation or save.',
    topics: ['inspect pcg graph', 'pcg node types', 'pcg settings schema', 'read pcg pins', 'validate pcg connections'],
    members: { node_types: 'list_pcg_node_types', settings_schema: 'get_pcg_settings_schema', node_schema: 'get_pcg_node_schema', graph: 'get_pcg_graph', editor_graph: 'get_pcg_editor_graph', validation: 'validate_pcg_graph', parameters: 'get_pcg_parameters', operation_status: 'get_pcg_operation_status', component_results: 'get_pcg_component_results', data: 'list_pcg_data', data_schema: 'get_pcg_data_schema', create_data_view: 'create_pcg_data_view', data_rows: 'read_pcg_data', release_data_view: 'release_pcg_data_view' },
  },
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
    members: { create: 'create_pcg_graph', create_subgraph: 'create_pcg_subgraph', sync_editor: 'sync_pcg_editor_graph', move_node: 'move_pcg_node', connect_pins: 'connect_pcg_pins', set_node_settings: 'set_pcg_node_settings', set_partition_grid_size: 'set_pcg_partition_grid_size' },
  },
];
