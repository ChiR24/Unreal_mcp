// Fold specs for manage_level_structure. Data only; see ../shared/fold.ts.
import type { FoldSpec } from '../shared/fold-types.js';

export const MANAGE_LEVEL_STRUCTURE_FOLDS: readonly FoldSpec[] = [
  {
    primary: 'create_level_structure', selector: 'kind',
    summary: 'Create level structure: a level, sublevel, level instance, packed level blueprint, data layer, or minimap volume.',
    topics: ['create level', 'create sublevel', 'level instance', 'packed level actor', 'data layer', 'minimap volume'],
    members: { level: 'create_level', sublevel: 'create_sublevel', level_instance: 'create_level_instance', packed_level_actor: 'create_packed_level_actor', data_layer: 'create_data_layer', minimap_volume: 'create_minimap_volume' },
  },
  {
    primary: 'configure_level_streaming', selector: 'setting',
    summary: 'Configure streaming and partition: level streaming, streaming distance, World Partition, grid size, HLOD layer, data-layer assignment.',
    topics: ['level streaming', 'streaming distance', 'world partition', 'grid size', 'hlod', 'data layer assignment'],
    members: {
      streaming: 'configure_level_streaming', streaming_distance: 'set_streaming_distance', world_partition: 'enable_world_partition',
      grid_size: 'configure_grid_size', hlod_layer: 'configure_hlod_layer', data_layer_assignment: 'assign_actor_to_data_layer',
    },
  },
  {
    primary: 'edit_level_blueprint', selector: 'edit',
    summary: 'Edit the level Blueprint: open it, add or remove nodes, connect nodes, set an input pin\'s default value.',
    topics: ['level blueprint', 'level blueprint node'],
    members: { open: 'open_level_blueprint', add_node: 'add_level_blueprint_node', connect_nodes: 'connect_level_blueprint_nodes', remove_node: 'remove_level_blueprint_node', set_pin_default_value: 'set_level_blueprint_pin_default' },
  },
  {
    primary: 'set_volume_properties', selector: 'volumeProperty',
    summary: 'Change a volume found by its label (volumeName), chosen by volumeProperty. properties sets what its class carries: a Physics Volume bWaterVolume, fluidFriction, terminalVelocity and priority; a Pain Causing Volume bPainCausing and damagePerSec; an Audio Volume bEnabled, reverbVolume and fadeTime; a Post Process Volume priority, blendWeight and bEnabled. The reply lists propertiesSet and propertiesIgnored (what this class lacks), and is NO_PROPERTIES_APPLIED when nothing applied. extent sets its half-size (default 100 on each axis); bounds moves and sizes it from a min and a max corner. The level is saved only with save true, which also writes its other unsaved changes.',
    topics: ['volume extent', 'volume bounds', 'volume properties', 'resize volume'],
    members: { properties: 'set_volume_properties', extent: 'set_volume_extent', bounds: 'set_volume_bounds' },
  },
];
