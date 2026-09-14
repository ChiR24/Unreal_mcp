// Fold specs for manage_level_structure. Data only; see ../shared/fold.ts.
import type { FoldSpec } from '../shared/fold-types.js';

export const MANAGE_LEVEL_STRUCTURE_FOLDS: readonly FoldSpec[] = [
  {
    primary: 'create_level_structure', selector: 'kind',
    summary: 'Create level structure: a level, sublevel, level instance, packed level actor, data layer, or minimap volume.',
    topics: ['create level', 'create sublevel', 'level instance', 'packed level actor', 'data layer', 'minimap volume'],
    members: { level: 'create_level', sublevel: 'create_sublevel', level_instance: 'create_level_instance', packed_level_actor: 'create_packed_level_actor', data_layer: 'create_data_layer', minimap_volume: 'create_minimap_volume' },
  },
  {
    primary: 'configure_level_streaming', selector: 'setting',
    summary: 'Configure streaming and partition: level streaming, streaming distance, level bounds, World Partition, grid size, HLOD layer, data-layer assignment.',
    topics: ['level streaming', 'streaming distance', 'level bounds', 'world partition', 'grid size', 'hlod', 'data layer assignment'],
    members: {
      streaming: 'configure_level_streaming', streaming_distance: 'set_streaming_distance', bounds: 'configure_level_bounds', world_partition: 'enable_world_partition',
      grid_size: 'configure_grid_size', hlod_layer: 'configure_hlod_layer', data_layer_assignment: 'assign_actor_to_data_layer',
    },
  },
  {
    primary: 'edit_level_blueprint', selector: 'edit',
    summary: 'Edit the level Blueprint: open it, add or remove nodes, connect nodes.',
    topics: ['level blueprint', 'level blueprint node'],
    members: { open: 'open_level_blueprint', add_node: 'add_level_blueprint_node', connect_nodes: 'connect_level_blueprint_nodes', remove_node: 'remove_level_blueprint_node' },
  },
  {
    primary: 'set_volume_properties', selector: 'volumeProperty',
    summary: 'Set a volume\'s properties, its extent, or its bounds.',
    topics: ['volume extent', 'volume bounds', 'volume properties'],
    members: { properties: 'set_volume_properties', extent: 'set_volume_extent', bounds: 'set_volume_bounds' },
  },
];
