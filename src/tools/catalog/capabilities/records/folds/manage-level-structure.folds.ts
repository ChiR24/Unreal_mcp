// Fold specs for manage_level_structure. Data only; see ../shared/fold.ts.
import type { FoldSpec } from '../shared/fold-types.js';

export const MANAGE_LEVEL_STRUCTURE_FOLDS: readonly FoldSpec[] = [
  {
    primary: 'create_level_structure', selector: 'kind',
    summary: 'Create level structure, chosen by kind: level (a new map asset, optionally opened), sublevel (a new level streamed into the open one), level_instance (an actor loading a saved level, to reuse a room or building), packed_level_actor (a saved level baked into one instanced-mesh Blueprint), data_layer (World Partition), or minimap_volume (the area a World Partition minimap captures).',
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
    summary: 'Edit the level Blueprint of the open level (levelPath names another loaded level; by default the persistent level), chosen by edit. open opens its editor; add_node adds a node by class or alias (an event bound to a placed actor with actorName and eventName, a reference to one, a component getter with memberClass and memberName) and lists its pins; connect_nodes links two pins through the graph schema, which refuses incompatible ones (CONNECTION_REFUSED, with its reason) and adds the conversion node a cast needs; remove_node removes a node by id, name or title; set_pin_default_value sets an input pin. The level stays unsaved unless save is true (saved), and a transient /Temp level is refused.',
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
