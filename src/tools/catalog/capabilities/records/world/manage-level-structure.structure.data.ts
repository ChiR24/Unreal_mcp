/**
 * Level structure family records (18 actions): the non-volume structural
 * operations of manage_level_structure.
 *
 * Grounded in the native LevelStructure domain dispatch (Private/Domains/LevelStructure/
 * McpLevelStructureActions.h). World Partition / data layer /
 * HLOD / level instance / packed level actor routes are editor-only and require
 * the editor state 'edit'.
 */
import type { CapabilityRecordSource } from '../../model.js';
import { buildWorldRecord } from './builder.js';
import { P } from './properties.js';

const F = 'structure';
// The loaded level whose level blueprint an edit targets; omit for the persistent level.
const BP_LEVEL = { type: 'string', description: 'Loaded level (persistent or streaming sublevel) whose level blueprint to edit; defaults to the persistent level.' };
// A saved level asset another level instances; never the open level itself.
const LEVEL_ASSET = { type: 'string', description: 'Saved level asset to instance or pack, e.g. /Game/Maps/Room01 (not the open level).' };
const SAVE_LEVEL = { type: 'boolean', description: 'Save the open level (default true unless it was never saved).' };
// A connect end: an exact name, title or id wins over a node that only contains it (an actor's bound event holds its label).
const LBP_NODE = (end: string) => ({ type: 'string', description: `${end} node: the nodeName or nodeId add_level_blueprint_node returned, or its title. An exact match wins; otherwise the last node whose name or title contains it.` });
// One node an edit acts on; remove_node and set_pin_default_value share it in the edit_level_blueprint fold.
const LBP_NODE_ID = { type: 'string', description: 'The node: the nodeId or nodeName add_level_blueprint_node returned, or its title, matched exactly (a node that only contains it is not taken). A name or title several nodes share is refused with their names (AMBIGUOUS_NODE).' };

export const LEVEL_STRUCTURE_RECORDS: readonly CapabilityRecordSource[] = [
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'create_level', dispatchAction: 'create_level',
    family: F, summary: 'Create an empty level asset, levelName in levelPath (default /Game/Maps), saved unless save is false. bCreateWorldPartition makes it a World Partition level (external actors then default on; bUseExternalActors false keeps them in the level). A level already at that path is reported (alreadyExisted), never replaced. loadAfterCreate opens it, refused with DIRTY_PACKAGES (naming them) when open levels have unsaved changes the load would drop.',
    whenToUse: ['A brand-new level must be created and opened.'], whenNotToUse: ['An existing level should be loaded instead.'],
    inputProps: { levelName: P.levelName, levelPath: P.levelPath, bCreateWorldPartition: P.bCreateWorldPartition, bUseExternalActors: P.bUseExternalActors, save: P.save, loadAfterCreate: { type: 'boolean', description: 'Open the new level in the editor after creating it (default false). When open levels have unsaved changes the load would drop, the call is refused with DIRTY_PACKAGES (listing them in unsavedPackages) before anything is created.' } },
    required: ['levelName'], effect: 'write', costLatency: 'interactive', costResources: 'medium',
    exampleInput: { action: 'create_level', levelName: 'NewMap', bCreateWorldPartition: false },
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'create_sublevel', dispatchAction: 'create_sublevel',
    topics: ['sublevel', 'sub level', 'streaming level', 'child level'],
    family: F, summary: 'Create an empty level asset (sublevelName, in sublevelPath, else beside the open level, or /Game/Maps when the open level was never saved) and stream it into the open level by streamingMethod (Blueprint, the default, or AlwaysLoaded). parentLevel, when given, must be the open level (PARENT_LEVEL_NOT_LOADED otherwise, nothing created). A level already at that path is reported (alreadyExisted, streamingAdded), not replaced; manage_level add_sublevel streams it. save (default true) saves the new level and the open level.',
    whenToUse: ['A streaming child level must be created.'], whenNotToUse: ['The level should be loaded as the main level; use create_level.'],
    inputProps: { sublevelName: P.sublevelName, sublevelPath: { ...P.sublevelPath, description: 'Folder for the new level, or its full package path ending in sublevelName (default: the open level\'s folder).' }, parentLevel: { ...P.parentLevel, description: 'The level to stream it into; must be the level open in the editor (omit for the open level).' }, streamingMethod: P.streamingMethod, save: { ...P.save, description: 'Save the new level and the open level that now streams it (default true).' } },
    required: ['sublevelName'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'create_sublevel', sublevelName: 'Sub01', parentLevel: '/Game/Maps/Demo' },
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'configure_level_streaming', dispatchAction: 'configure_level_streaming',
    family: F, summary: 'Change how the open level streams a sublevel (levelName: its package path or short name): streamingMethod Blueprint or AlwaysLoaded (switched as the Levels panel does), bShouldBeVisible, bShouldBlockOnLoad and bDisableDistanceStreaming; only the fields sent change. A level on disk that the open level does not stream yet is added to it first; one that does not exist is LEVEL_NOT_FOUND. The open level stays unsaved unless save is true.',
    whenToUse: ['A sub-level streaming method or load behavior must be set.'], whenNotToUse: ['A streaming volume should be created; use create_*_volume.'],
    inputProps: { levelName: P.levelName, streamingMethod: P.streamingMethod, bShouldBeVisible: P.bShouldBeVisible, bShouldBlockOnLoad: P.bShouldBlockOnLoad, bDisableDistanceStreaming: P.bDisableDistanceStreaming, save: P.levelEditSave },
    required: ['levelName'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'configure_level_streaming', levelName: 'Sub01', streamingMethod: 'Blueprint' },
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'set_streaming_distance', dispatchAction: 'set_streaming_distance',
    family: F, summary: 'Stream a sublevel by where the player is: place a Level Streaming Volume, a box reaching streamingDistance (default 10000) from volumeLocation (default the origin) on each axis, bound to the sublevel (levelName) so it loads while a viewer is inside it. streamingUsage picks what it controls (LoadingAndVisibility, the default, Loading, VisibilityBlockingOnLoad, BlockingOnLoad, LoadingNotVisible); an unknown one or a distance of 0 or less is refused before anything is placed. createVolume false only lists the volumes bound to the level. A level on disk the open level does not stream yet is added first. The open level stays unsaved unless save is true.',
    whenToUse: ['Distance-based streaming must be tuned for a sub-level.'], whenNotToUse: ['The streaming method should be changed; use configure_level_streaming.'],
    inputProps: { volumeLocation: { ...P.volumeLocation, description: 'World location of the volume\'s centre {x, y, z} (default the origin).' }, levelName: P.levelName, streamingDistance: { ...P.streamingDistance, description: 'How far the volume reaches from its centre on each axis, in world units, over 0 (default 10000).' }, streamingUsage: { type: 'string', enum: ['LoadingAndVisibility', 'Loading', 'VisibilityBlockingOnLoad', 'BlockingOnLoad', 'LoadingNotVisible'], description: 'What the volume controls (default LoadingAndVisibility: load and show the level while a viewer is inside).' }, createVolume: P.createVolume, save: P.levelEditSave },
    required: ['levelName', 'streamingDistance'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'set_streaming_distance', levelName: 'Sub01', streamingDistance: 5000 },
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'enable_world_partition', dispatchAction: 'enable_world_partition',
    family: F, summary: 'Turn World Partition on for the open level in place: it creates the World Partition object on the level\'s World Settings (save and reload the level before streaming works); bUseExternalActors also moves every actor into its own package (one file per actor), which data layers need. Turning it off on a partitioned level is refused (NOT_SUPPORTED), as is a level that cannot take it. The level stays unsaved unless save is true.',
    whenToUse: ['A level must be converted to use World Partition.'], whenNotToUse: ['World Partition is already enabled for the level.'],
    inputProps: { bEnableWorldPartition: P.bEnableWorldPartition, bUseExternalActors: { type: 'boolean', description: 'Also move the level actors into one-file-per-actor packages (default false).' }, save: P.levelEditSave },
    required: [], effect: 'write', costLatency: 'interactive', costResources: 'medium',
    exampleInput: { action: 'enable_world_partition', bEnableWorldPartition: true },
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'configure_grid_size', dispatchAction: 'configure_grid_size',
    family: F, summary: 'Set a World Partition streaming grid of the open level (gridName; omitted, the first grid, or MainPartition on a Runtime Hash Set level): gridCellSize, loadingRange (how far from the player its cells load), bBlockOnSlowStreaming and priority; only the fields sent change, and a grid missing by name is added with createIfMissing (default true), the others at their defaults (cell 12800, range 25600). On a Runtime Hash Set level the grid is a runtime partition row, which must have a Main Layer to hold the values (PARTITION_HAS_NO_LAYER otherwise). The reply gives the edited grid values as they now stand (on a Runtime Hash Set level, the values it set) and lists every grid; a grid found with nothing sent to change is refused. The level stays unsaved unless save is true.',
    whenToUse: ['World Partition grid resolution must be tuned.'], whenNotToUse: ['World Partition is not enabled for the level.'],
    inputProps: { gridName: P.gridName, createIfMissing: P.createIfMissing, bBlockOnSlowStreaming: P.bBlockOnSlowStreaming, gridCellSize: P.gridCellSize, loadingRange: P.loadingRange, priority: P.gridPriority, save: P.levelEditSave },
    required: [], effect: 'write', behavior: { idempotency: 'idempotent' }, costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'configure_grid_size', gridCellSize: 12800 },
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'create_data_layer', dispatchAction: 'create_data_layer',
    family: F, summary: 'Create a World Partition data layer: a DataLayerAsset named dataLayerName in dataLayerAssetPath (default /Game/DataLayers), saved, and its instance in the open level, Runtime (streams in the game, the default) or Editor (organises the editor only), visible and loaded in the editor by default. Needs World Partition with external actors (WORLD_PARTITION_NOT_ENABLED, EXTERNAL_ACTORS_NOT_ENABLED); an unknown dataLayerType is refused before anything is created. Add actors with assign_actor_to_data_layer.',
    whenToUse: ['A Runtime or Editor data layer must be created.'], whenNotToUse: ['The level does not use World Partition.'],
    inputProps: { dataLayerName: P.dataLayerName, dataLayerType: { type: 'string', enum: ['Runtime', 'Editor'], description: 'Runtime (default: loaded and unloaded in the game) or Editor (editor-only organisation).' }, bIsInitiallyVisible: P.bIsInitiallyVisible, bIsInitiallyLoaded: P.bIsInitiallyLoaded, dataLayerAssetPath: { type: 'string', description: 'Folder for the new DataLayerAsset (default /Game/DataLayers).' }, bIsPrivate: { type: 'boolean', description: 'Make the data layer private to this world (UE 5.3 or later).' }, save: { type: 'boolean', description: 'Save the level that holds the new data layer instance (the data layer asset itself is always saved).' } },
    required: ['dataLayerName'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'create_data_layer', dataLayerName: 'DL_Vegetation', dataLayerType: 'Runtime' },
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'assign_actor_to_data_layer', dispatchAction: 'assign_actor_to_data_layer',
    family: F, summary: 'Add a placed actor (actorName: label or name, or actorPath) to a data layer of the open level (dataLayerName: its short or full name), so it loads and shows with that layer. One already in the layer is reported (alreadyAssigned); a layer that is not there is refused with the ones that are. Needs World Partition with external actors; the level stays unsaved unless save is true.',
    whenToUse: ['An actor must be organized under a data layer.'], whenNotToUse: ['The actor should remain in the default layer.'],
    inputProps: { actorName: P.actorName, actorPath: P.actorPath, dataLayerName: P.dataLayerName, save: P.levelEditSave },
    required: ['dataLayerName'], requiredOneOf: ['actorName', 'actorPath'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'assign_actor_to_data_layer', actorName: 'Tree_01', dataLayerName: 'DL_Vegetation' },
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'configure_hlod_layer', dispatchAction: 'configure_hlod_layer',
    family: F, summary: 'Create an HLOD layer asset (hlodLayerName, in hlodLayerPath, default /Game/HLOD): layerType Instancing, MeshMerge (default), MeshSimplify or MeshApproximate, spatially loaded (default true) with its cellSize (default 25600) and loadingDistance (default 51200). An unknown layerType is refused before anything is created; each value reports whether it took (layerTypeApplied and the others). Saved unless save is false.',
    whenToUse: ['An HLOD layer must be configured for the level.'], whenNotToUse: ['HLOD is not used for the level.'],
    inputProps: { layerType: P.layerType, hlodLayerName: P.hlodLayerName, hlodLayerPath: P.hlodLayerPath, bIsSpatiallyLoaded: P.bIsSpatiallyLoaded, cellSize: P.cellSize, loadingDistance: P.loadingDistance, save: { type: 'boolean', description: 'Save the new HLOD layer asset (default true).' } },
    required: ['hlodLayerName'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'configure_hlod_layer', hlodLayerName: 'HLOD_01' },
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'create_minimap_volume', dispatchAction: 'create_minimap_volume',
    family: F, summary: 'Place a World Partition minimap volume, the area the minimap builder captures: a box brush of extent (half size, default 10000 on each axis) at location, labelled volumeName (default MinimapVolume). A level without World Partition is refused (WORLD_PARTITION_NOT_ENABLED); UE 5.1 or later. The level stays unsaved unless save is true.',
    whenToUse: ['A minimap capture volume must be placed.'], whenNotToUse: ['A generic volume is needed; use create_*_volume.'],
    inputProps: { volumeLocation: P.volumeLocation, volumeExtent: P.volumeExtent, volumeName: P.volumeName, location: P.location, extent: P.extent, save: P.levelEditSave },
    required: ['location'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'create_minimap_volume', location: { x: 0, y: 0, z: 0 } },
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'create_level_instance', dispatchAction: 'create_level_instance',
    topics: ['level instance', 'instance a level', 'place level in level', 'reuse room level'],
    family: F, summary: 'Place a Level Instance actor that loads an existing level asset at a location, so one saved level (a room, a building) can be reused inside another.',
    whenToUse: ['A saved level must appear inside the open level as one movable actor.'], whenNotToUse: ['The level should stream as a sublevel; use create_sublevel or configure_level_streaming.'],
    inputProps: {
      levelAssetPath: LEVEL_ASSET,
      location: P.location,
      rotation: P.rotation,
      label: { type: 'string', description: 'Actor label of the new instance (default LI_<LevelName>).' },
      save: SAVE_LEVEL,
    },
    required: ['levelAssetPath'], effect: 'write', costLatency: 'interactive', costResources: 'medium',
    outputProps: {
      actorName: { type: 'string', description: 'Object name of the placed Level Instance actor.' },
      actorLabel: { type: 'string', description: 'Its label in the outliner.' },
      worldAsset: { type: 'string', description: 'The level the instance loads.' },
      loaded: { type: 'boolean', description: 'Whether the instanced level is loaded in the editor now.' },
      childActorCount: { type: 'number', description: 'Actors in the loaded instance level.' },
      saved: { type: 'boolean', description: 'Whether the open level was saved.' },
    },
    exampleInput: { action: 'create_level_instance', levelAssetPath: '/Game/Maps/Room01', location: { x: 1000, y: 0, z: 0 } },
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'create_packed_level_actor', dispatchAction: 'create_packed_level_actor',
    topics: ['packed level actor', 'packed level blueprint', 'bake level into instanced meshes'],
    family: F, summary: 'Bake the static meshes of a level into a Packed Level Blueprint (instanced static mesh components) and optionally place it (UE 5.3 or later).',
    whenToUse: ['A level of repeated static meshes must become one cheap, reusable Blueprint actor.'], whenNotToUse: ['The level has gameplay actors that must stay live; use create_level_instance.'],
    inputProps: {
      levelAssetPath: LEVEL_ASSET,
      blueprintPath: { type: 'string', description: 'Packed Level Blueprint to create or update (default <level folder>/BPP_<LevelName>).' },
      spawn: { type: 'boolean', description: 'Place the Blueprint in the open level (default true).' },
      location: P.location,
      save: { type: 'boolean', description: 'Save the Blueprint (default true) and, when it is placed, the open level (default true unless the level was never saved).' },
    },
    required: ['levelAssetPath'], effect: 'write', costLatency: 'interactive', costResources: 'medium',
    outputProps: {
      blueprintPath: { type: 'string', description: 'The Packed Level Blueprint that was created or updated.' },
      blueprintCreated: { type: 'boolean', description: 'True when the Blueprint was new.' },
      ismComponentCount: { type: 'number', description: 'Instanced static mesh components on the Blueprint.' },
      instanceCount: { type: 'number', description: 'Mesh instances across those components.' },
      blueprintSaved: { type: 'boolean', description: 'Whether the Blueprint was saved.' },
      actorName: { type: 'string', description: 'The placed actor, when spawn is true.' },
      saved: { type: 'boolean', description: 'Whether the open level was saved.' },
    },
    exampleInput: { action: 'create_packed_level_actor', levelAssetPath: '/Game/Maps/Room01', location: { x: 0, y: 2000, z: 0 } },
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'open_level_blueprint', dispatchAction: 'open_level_blueprint',
    family: F, summary: 'Open the Level Blueprint editor of the open level (levelPath picks a loaded sublevel; default the persistent level), creating the Level Blueprint when the level has none yet. A level never saved (/Temp) has none to open, and an editor that does not open is EDITOR_NOT_OPENED; a headless editor only checks the Blueprint is there (headlessSafeMode). Nothing is changed or saved.',
    whenToUse: ['The level blueprint graph must be edited.'], whenNotToUse: ['A sub-level blueprint is needed; use the asset path directly.'],
    inputProps: { levelPath: P.levelPath },
    required: [], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'open_level_blueprint', levelPath: '/Game/Maps/Demo' },
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'add_level_blueprint_node', dispatchAction: 'add_level_blueprint_node',
    topics: ['actor bound event', 'level blueprint overlap event', 'level blueprint actor reference', 'level blueprint component reference'],
    family: F, summary: 'Add a node to the level blueprint graph, including an event bound to a placed actor (OnActorBeginOverlap of a trigger), a reference to one, or a getter of its component; the reply lists the node\'s pins.',
    whenToUse: [
      'A new node must be added to level scripting.',
      'Level logic must react to a placed actor (K2Node_ActorBoundEvent with actorName and eventName) or use one (K2Node_Literal with actorName).',
      'A function of a class outside the common Blueprint libraries must be called (K2Node_CallFunction with functionName and memberClass).',
      'A placed actor\'s component must be used, such as the light component whose SetLightColor changes a light (K2Node_VariableGet with memberClass and memberName, its self pin fed from the actor\'s K2Node_Literal).',
    ],
    whenNotToUse: ['An existing node should be connected; use connect_level_blueprint_nodes.', 'The level Blueprint\'s nodes must be read, or edited beyond these edits; blueprint.inspect_graph and blueprint.edit_graph take the open level\'s path (/Game/Maps/L_Main) as blueprintPath.'],
    inputProps: {
      nodeClass: P.nodeClass, nodeName: P.nodeName, nodePosition: P.nodePosition, levelPath: BP_LEVEL, save: P.levelEditSave,
      functionName: { type: 'string', description: 'Function to bind when nodeClass is K2Node_CallFunction (e.g. PrintString); short function names may also be passed as nodeClass.' },
      memberClass: { type: 'string', description: 'Class that declares functionName when it is not in the common Blueprint libraries, or memberName: a native class name or a Blueprint asset path. The node\'s Target (self pin) takes an object of that class, so an actor\'s function or component getter is fed from its K2Node_Literal, while a component\'s function (a light\'s SetLightColor is LightComponent\'s) takes the component that getter returns, not the actor. One that names no class is refused with INVALID_ARGUMENT.' },
      memberName: { type: 'string', description: 'Variable a K2Node_VariableGet or K2Node_VariableSet reads or writes, required by both and refused for other nodes: a level Blueprint variable (its own or inherited, such as Tags), or with memberClass a property of that class, such as a placed actor\'s component (PointLightComponent on PointLight, StaticMeshComponent on StaticMeshActor); with memberClass the node always has the self pin the actor\'s K2Node_Literal feeds, even for Actor. A wrong name is refused with that class\'s components listed (VARIABLE_NOT_FOUND), and one Blueprints cannot read, or for a VariableSet write (not Blueprint-visible, read-only), with VARIABLE_NOT_ACCESSIBLE.' },
      actorName: { type: 'string', description: 'Placed actor (label, name or object path) in the edited level that a K2Node_ActorBoundEvent fires for or a K2Node_Literal references; required by both, refused for other nodes. A label two actors share is refused with their names, and an actor of another loaded level with that level (ACTOR_IN_OTHER_LEVEL).' },
      eventName: { type: 'string', description: 'Event dispatcher of actorName that a K2Node_ActorBoundEvent binds, e.g. OnActorBeginOverlap or OnActorEndOverlap; a wrong name is refused with the actor\'s events listed. Refused for any other node: BeginPlay or Tick go in nodeClass (EventBeginPlay, EventTick).' },
    },
    required: ['nodeClass'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'add_level_blueprint_node', nodeClass: 'K2Node_CallFunction', nodeName: 'Print' },
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'remove_level_blueprint_node', dispatchAction: 'remove_level_blueprint_node',
    family: F, summary: 'Remove a level blueprint node by id, name or title (one several nodes share is refused), or purge unbound call nodes.',
    whenToUse: ['A level blueprint node must be deleted, or an unbound K2Node_CallFunction ("Could not find a function named None") must be repaired.'], whenNotToUse: ['The node only needs re-wiring; use connect_level_blueprint_nodes.'],
    inputProps: { nodeId: LBP_NODE_ID, nodeName: P.nodeName, unboundOnly: { type: 'boolean', description: 'Remove every call-function node that has no bound function instead of a named node.' }, levelPath: BP_LEVEL, save: P.levelEditSave },
    required: [], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'remove_level_blueprint_node', unboundOnly: true },
    // Authored after the gateway migration: git history has no pre-gateway
    // remove_level_blueprint_node occurrence, so extractOccurrences() must skip it.
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'connect_level_blueprint_nodes', dispatchAction: 'connect_level_blueprint_nodes',
    family: F, summary: 'Link sourceNodeName.sourcePinName to targetNodeName.targetPinName in the level Blueprint through the graph schema: incompatible types or directions are refused with the schema\'s reason (CONNECTION_REFUSED), a cast gets its conversion node, and a pin that takes one link drops its old one. A pin a node lacks is PIN_NOT_FOUND, naming the pins on both nodes. The level stays unsaved unless save is true (saved).',
    whenToUse: ['Two level blueprint nodes must be wired together.'], whenNotToUse: ['A node must be created first; use add_level_blueprint_node.', 'The nodes are in a Blueprint asset, not the level Blueprint; use blueprint.edit_graph (edit=connect_pins).'],
    inputProps: { sourceNodeName: LBP_NODE('Source'), sourcePinName: P.sourcePinName, targetNodeName: LBP_NODE('Target'), targetPinName: P.targetPinName, levelPath: BP_LEVEL, save: P.levelEditSave },
    required: ['sourceNodeName', 'targetNodeName'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'connect_level_blueprint_nodes', sourceNodeName: 'EventBegin', targetNodeName: 'Print' },
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'set_level_blueprint_pin_default', dispatchAction: 'set_level_blueprint_pin_default',
    topics: ['level blueprint pin default', 'level blueprint pin value', 'level blueprint literal'],
    family: F, summary: 'Set the default value of an input pin on a level blueprint node (in its event graph or one of its functions), such as the color SetLightColor sets; a read-only (const reference) pin gets it through a MakeLiteral node wired into it.',
    whenToUse: ['A level blueprint node\'s input pin needs a literal value instead of a wire (a light color, a print string, a number).'],
    whenNotToUse: ['The pin should take its value from another node; use connect_level_blueprint_nodes.', 'The node is in a Blueprint asset, not the level Blueprint; use blueprint.edit_graph (edit=set_pin_default_value).'],
    inputProps: {
      nodeId: LBP_NODE_ID,
      pinName: { type: 'string', description: 'Input pin name or display name, as add_level_blueprint_node lists them (NewLightColor on SetLightColor).' },
      propertyValue: { description: 'The pin\'s new default: a number, true or false, text, an enum value name, an asset path, a vector as X,Y,Z or {x,y,z}, a rotator as P,Y,R or {pitch,yaw,roll}, a color as {r,g,b,a}.' },
      levelPath: BP_LEVEL, save: P.levelEditSave,
    },
    required: ['nodeId', 'pinName', 'propertyValue'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    outputProps: {
      nodeName: { type: 'string', description: 'The node whose pin was set.' },
      pinName: { type: 'string', description: 'The pin that was set.' },
      appliedValue: { type: 'string', description: 'Literal actually stored on the pin (or the resolved object path for an object or class pin).' },
      literalNodeId: { type: 'string', description: 'Guid of the MakeLiteral node carrying the value when the pin itself takes no literal.' },
      saved: { type: 'boolean', description: 'Whether the level was saved.' },
    },
    exampleInput: { action: 'set_level_blueprint_pin_default', nodeId: 'K2Node_CallFunction_0', pinName: 'NewLightColor', propertyValue: { r: 1, g: 0, b: 0, a: 1 } },
    exampleOutput: { success: true, message: 'Pin default value set. The level is not saved yet; pass save: true, or save it with manage_level save.', nodeName: 'K2Node_CallFunction_0', pinName: 'NewLightColor', appliedValue: '(R=1.000000,G=0.000000,B=0.000000,A=1.000000)', saved: false },
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'get_level_structure_info', dispatchAction: 'get_level_structure_info',
    topics: ['list streaming sublevels', 'world partition enabled', 'list data layers', 'hlod layers'],
    family: F, summary: 'Read the open level\'s structure: streaming sub-levels (loaded, visible, actor counts), whether World Partition is on, its data layers, level instances and HLOD layers.',
    whenToUse: ['The level structure must be inspected.'], whenNotToUse: ['A specific volume list is needed; use get_volumes_info.'],
    inputProps: {},
    required: [], effect: 'read', costLatency: 'instant', costResources: 'low',
    exampleInput: { action: 'get_level_structure_info' },
    exampleOutput: { success: true, message: 'Retrieved level structure information', levelStructureInfo: { currentLevel: 'L_Main', persistentLevel: '/Game/Maps/L_Main', actorCount: 412, sublevelCount: 0, sublevels: [], streamingLevels: [], worldPartitionEnabled: false, levelInstances: [], hlodLayers: [] } },
    outputProps: { levelStructureInfo: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'currentLevel, persistentLevel, actorCount, sublevelCount, sublevels (package names), streamingLevels (name, packageName, isLoaded, isVisible, shouldBeLoaded, shouldBeVisible, streamingClass, alwaysLoaded, actorCount), worldPartitionEnabled, dataLayers when World Partition is on (name, fullName, isRuntime, runtimeState, initialRuntimeState), levelInstances (labels) and hlodLayers (name, type and the layer settings).' } },
  }),
];
