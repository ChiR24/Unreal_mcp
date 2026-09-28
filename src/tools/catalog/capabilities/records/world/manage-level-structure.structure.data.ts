/**
 * Level structure family records (18 actions): the non-volume structural
 * operations of manage_level_structure.
 *
 * Grounded in src/tools/definitions/world/manage-level-structure-tool.ts and
 * the native LevelStructure domain dispatch (Private/Domains/LevelStructure/
 * McpAutomationBridge_LevelStructureActions.h). World Partition / data layer /
 * HLOD / level instance / packed level actor routes are editor-only and require
 * the editor state 'edit'.
 */
import type { CapabilityRecordSource } from '../../model.js';
import { buildWorldRecord } from './builder.js';
import { P } from './properties.js';

const F = 'structure';
// The loaded level whose level blueprint an edit targets; omit for the persistent level.
const BP_LEVEL = { type: 'string', description: 'Loaded level (persistent or streaming sublevel) whose level blueprint to edit; defaults to the persistent level.' };

export const LEVEL_STRUCTURE_RECORDS: readonly CapabilityRecordSource[] = [
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'create_level', dispatchAction: 'create_level',
    family: F, summary: 'Create a new level asset and load it into the editor.',
    whenToUse: ['A brand-new level must be created and opened.'], whenNotToUse: ['An existing level should be loaded instead.'],
    inputProps: { levelName: P.levelName, levelPath: P.levelPath, bCreateWorldPartition: P.bCreateWorldPartition, bUseExternalActors: P.bUseExternalActors, save: P.save, loadAfterCreate: { type: 'boolean', description: 'Open the new level in the editor after creating it (default false).' } },
    required: ['levelName'], effect: 'write', costLatency: 'interactive', costResources: 'medium',
    exampleInput: { action: 'create_level', levelName: 'NewMap', bCreateWorldPartition: false },
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'create_sublevel', dispatchAction: 'create_sublevel',
    topics: ['sublevel', 'sub level', 'streaming level', 'child level'],
    family: F, summary: 'Create a sub-level asset as a streaming child of a parent level.',
    whenToUse: ['A streaming child level must be created.'], whenNotToUse: ['The level should be loaded as the main level; use create_level.'],
    inputProps: { sublevelName: P.sublevelName, sublevelPath: P.sublevelPath, parentLevel: P.parentLevel, streamingMethod: P.streamingMethod, save: P.save },
    required: ['sublevelName'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'create_sublevel', sublevelName: 'Sub01', parentLevel: '/Game/Maps/Demo' },
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'configure_level_streaming', dispatchAction: 'configure_level_streaming',
    family: F, summary: 'Configure streaming settings for a sub-level (method, visibility, blocking).',
    whenToUse: ['A sub-level streaming method or load behavior must be set.'], whenNotToUse: ['A streaming volume should be created; use create_*_volume.'],
    inputProps: { levelName: P.levelName, streamingMethod: P.streamingMethod, bShouldBeVisible: P.bShouldBeVisible, bShouldBlockOnLoad: P.bShouldBlockOnLoad, bDisableDistanceStreaming: P.bDisableDistanceStreaming, save: P.levelEditSave },
    required: ['levelName'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'configure_level_streaming', levelName: 'Sub01', streamingMethod: 'Blueprint' },
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'set_streaming_distance', dispatchAction: 'set_streaming_distance',
    family: F, summary: 'Set the distance-based streaming radius for a sub-level via a streaming volume.',
    whenToUse: ['Distance-based streaming must be tuned for a sub-level.'], whenNotToUse: ['The streaming method should be changed; use configure_level_streaming.'],
    inputProps: { volumeLocation: P.volumeLocation, levelName: P.levelName, streamingDistance: P.streamingDistance, streamingUsage: P.streamingUsage, createVolume: P.createVolume, save: P.levelEditSave },
    required: ['levelName', 'streamingDistance'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'set_streaming_distance', levelName: 'Sub01', streamingDistance: 5000 },
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'enable_world_partition', dispatchAction: 'enable_world_partition',
    family: F, summary: 'Enable World Partition for a level (requires editor, one-time conversion).',
    whenToUse: ['A level must be converted to use World Partition.'], whenNotToUse: ['World Partition is already enabled for the level.'],
    inputProps: { bEnableWorldPartition: P.bEnableWorldPartition, bUseExternalActors: { type: 'boolean', description: 'Also move the level actors into one-file-per-actor packages (default false).' }, save: P.levelEditSave },
    required: [], effect: 'write', costLatency: 'interactive', costResources: 'medium',
    exampleInput: { action: 'enable_world_partition', bEnableWorldPartition: true },
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'configure_grid_size', dispatchAction: 'configure_grid_size',
    family: F, summary: 'Configure the World Partition grid cell size and loading range.',
    whenToUse: ['World Partition grid resolution must be tuned.'], whenNotToUse: ['World Partition is not enabled for the level.'],
    inputProps: { gridName: P.gridName, createIfMissing: P.createIfMissing, bBlockOnSlowStreaming: P.bBlockOnSlowStreaming, gridCellSize: P.gridCellSize, loadingRange: P.loadingRange, priority: P.gridPriority, save: P.levelEditSave },
    required: [], effect: 'write', behavior: { idempotency: 'idempotent' }, costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'configure_grid_size', gridCellSize: 12800 },
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'create_data_layer', dispatchAction: 'create_data_layer',
    family: F, summary: 'Create a World Partition data layer asset.',
    whenToUse: ['A Runtime or Editor data layer must be created.'], whenNotToUse: ['The level does not use World Partition.'],
    inputProps: { dataLayerName: P.dataLayerName, dataLayerType: P.dataLayerType, bIsInitiallyVisible: P.bIsInitiallyVisible, bIsInitiallyLoaded: P.bIsInitiallyLoaded, dataLayerAssetPath: { type: 'string', description: 'Folder for the new DataLayerAsset (default /Game/DataLayers).' }, bIsPrivate: { type: 'boolean', description: 'Make the data layer private to this world (UE 5.3 or later).' }, save: { type: 'boolean', description: 'Save the level that holds the new data layer instance (the data layer asset itself is always saved).' } },
    required: ['dataLayerName'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'create_data_layer', dataLayerName: 'DL_Vegetation', dataLayerType: 'Runtime' },
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'assign_actor_to_data_layer', dispatchAction: 'assign_actor_to_data_layer',
    family: F, summary: 'Assign an actor to a World Partition data layer.',
    whenToUse: ['An actor must be organized under a data layer.'], whenNotToUse: ['The actor should remain in the default layer.'],
    inputProps: { actorName: P.actorName, actorPath: P.actorPath, dataLayerName: P.dataLayerName, save: P.levelEditSave },
    required: ['dataLayerName'], requiredOneOf: ['actorName', 'actorPath'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'assign_actor_to_data_layer', actorName: 'Tree_01', dataLayerName: 'DL_Vegetation' },
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'configure_hlod_layer', dispatchAction: 'configure_hlod_layer',
    family: F, summary: 'Configure an HLOD layer (spatial loading, cell size, loading distance).',
    whenToUse: ['An HLOD layer must be configured for the level.'], whenNotToUse: ['HLOD is not used for the level.'],
    inputProps: { layerType: P.layerType, hlodLayerName: P.hlodLayerName, hlodLayerPath: P.hlodLayerPath, bIsSpatiallyLoaded: P.bIsSpatiallyLoaded, cellSize: P.cellSize, loadingDistance: P.loadingDistance, save: { type: 'boolean', description: 'Save the new HLOD layer asset (default true).' } },
    required: ['hlodLayerName'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'configure_hlod_layer', hlodLayerName: 'HLOD_01' },
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'create_minimap_volume', dispatchAction: 'create_minimap_volume',
    family: F, summary: 'Create a minimap capture volume for the level.',
    whenToUse: ['A minimap capture volume must be placed.'], whenNotToUse: ['A generic volume is needed; use create_*_volume.'],
    inputProps: { volumeLocation: P.volumeLocation, volumeExtent: P.volumeExtent, volumeName: P.volumeName, location: P.location, extent: P.extent, save: P.levelEditSave },
    required: ['location'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'create_minimap_volume', location: { x: 0, y: 0, z: 0 } },
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'open_level_blueprint', dispatchAction: 'open_level_blueprint',
    family: F, summary: 'Open the level blueprint (level scripting) editor.',
    whenToUse: ['The level blueprint graph must be edited.'], whenNotToUse: ['A sub-level blueprint is needed; use the asset path directly.'],
    inputProps: { levelPath: P.levelPath },
    required: [], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'open_level_blueprint', levelPath: '/Game/Maps/Demo' },
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'add_level_blueprint_node', dispatchAction: 'add_level_blueprint_node',
    family: F, summary: 'Add a node to the level blueprint graph.',
    whenToUse: ['A new node must be added to level scripting.'], whenNotToUse: ['An existing node should be connected; use connect_level_blueprint_nodes.'],
    inputProps: { nodeClass: P.nodeClass, nodeName: P.nodeName, nodePosition: P.nodePosition, levelPath: BP_LEVEL, save: P.levelEditSave, functionName: { type: 'string', description: 'Function to bind when nodeClass is K2Node_CallFunction (e.g. PrintString); short function names may also be passed as nodeClass.' } },
    required: ['nodeClass'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'add_level_blueprint_node', nodeClass: 'K2Node_CallFunction', nodeName: 'Print' },
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'remove_level_blueprint_node', dispatchAction: 'remove_level_blueprint_node',
    family: F, summary: 'Remove nodes from the level blueprint graph by id or name, or purge unbound call nodes.',
    whenToUse: ['A level blueprint node must be deleted, or an unbound K2Node_CallFunction ("Could not find a function named None") must be repaired.'], whenNotToUse: ['The node only needs re-wiring; use connect_level_blueprint_nodes.'],
    inputProps: { nodeId: { type: 'string', description: 'Node GUID returned by add_level_blueprint_node.' }, nodeName: P.nodeName, unboundOnly: { type: 'boolean', description: 'Remove every call-function node that has no bound function instead of a named node.' }, levelPath: BP_LEVEL, save: P.levelEditSave },
    required: [], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'remove_level_blueprint_node', unboundOnly: true },
    // Authored after the gateway migration: git history has no pre-gateway
    // remove_level_blueprint_node occurrence, so extractOccurrences() must skip it.
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'connect_level_blueprint_nodes', dispatchAction: 'connect_level_blueprint_nodes',
    family: F, summary: 'Connect two level blueprint graph nodes by pin names.',
    whenToUse: ['Two level blueprint nodes must be wired together.'], whenNotToUse: ['A node must be created first; use add_level_blueprint_node.'],
    inputProps: { sourceNodeName: P.sourceNodeName, sourcePinName: P.sourcePinName, targetNodeName: P.targetNodeName, targetPinName: P.targetPinName, levelPath: BP_LEVEL, save: P.levelEditSave },
    required: ['sourceNodeName', 'targetNodeName'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'connect_level_blueprint_nodes', sourceNodeName: 'EventBegin', targetNodeName: 'Print' },
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'get_level_structure_info', dispatchAction: 'get_level_structure_info',
    family: F, summary: 'Return structural info for the current level (sub-levels, data layers, WP state).',
    whenToUse: ['The level structure must be inspected.'], whenNotToUse: ['A specific volume list is needed; use get_volumes_info.'],
    inputProps: {},
    required: [], effect: 'read', costLatency: 'instant', costResources: 'low',
    exampleInput: { action: 'get_level_structure_info' },
    exampleOutput: { success: true, message: 'Level structure info', worldPartition: false },
    outputProps: { worldPartition: { type: 'boolean', description: 'Whether World Partition is enabled.' } },
  }),
];
