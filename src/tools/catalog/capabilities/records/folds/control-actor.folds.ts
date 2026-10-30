// Fold specs for control_actor. Data only; see ../shared/fold.ts.
import type { FoldSpec } from '../shared/fold-types.js';

export const CONTROL_ACTOR_FOLDS: readonly FoldSpec[] = [
  {
    primary: 'spawn', selector: 'spawnKind',
    summary: 'Place actors in the level, or in the running game during Play (gone when Play stops), chosen by spawnKind; each call is one undo step. class spawns classPath (or actorClass), such as /Script/Engine.PointLight or a Blueprint class, or with meshPath alone a Movable static or skeletal mesh actor showing that mesh; blueprint spawns blueprintPath; batch places up to 500 items (1000 once repeat rows are unrolled) in one call, each a spawn payload that can also take materialPath, variables, properties, folder and tags, with shared defaults. location and rotation default to zero and scale to 1; folder files the actor in the outliner and tags tag it. The actor is labelled actorName, else after its mesh or class; when another actor has the same label, the reply\'s actorName is the actor\'s unique object name, which later calls must use. An unknown class or mesh is refused (CLASS_NOT_FOUND, MESH_NOT_FOUND), and an actor that destroys itself at once is SPAWNED_THEN_DESTROYED. The reply checks the placement: groundZ and groundClearance, and placementWarning with suggestedLocation for an actor sunk, floating or intersecting (tag mcp.placement.ok to mark one deliberate).',
    topics: ['spawn actor', 'spawn blueprint', 'place actor', 'add actor to level', 'spawn many actors', 'add player start', 'spawn enemy', 'spawn actors in a row', 'spawn actors in a grid', 'a row of trees, lamps or posts', 'fill a room with props or furniture', 'place lamps or posts along a road or a path'],
    members: { class: 'spawn', blueprint: 'spawn_blueprint', batch: 'spawn_batch' },
    aliasMembers: ['spawn_actor'],
  },
  {
    primary: 'set_transform', selector: 'transformMode',
    summary: 'Move, rotate or scale placed actors (the playing copy during Play), chosen by transformMode; every member writes alike. location, rotation ([pitch, yaw, roll] degrees) and scale are world values, a part left out keeps its current value, and offset moves by [dx, dy, dz] instead of to a location (both together are refused). actors gives several actors their own values and actorNames gives many the same change in one call. A single move is one undo step and teleports, without sweeping. The reply reads the transform back off the actor; one that did not end up as asked (attached, simulating physics, constrained, or moved by game logic) fails with TRANSFORM_MISMATCH carrying where it is, and TRANSFORM_BATCH_INCOMPLETE names batch items that did not move. The placement is checked: groundZ, groundClearance, and placementWarning with suggestedLocation when it is sunk, floating or intersecting.',
    topics: ['move actor', 'rotate actor', 'scale actor', 'set actor location', 'set actor rotation', 'teleport actor', 'make actor bigger', 'make actor smaller', 'resize actor', 'shrink actor', 'scale up a tree or a rock', 'rotate a door or a wall by 90 degrees', 'move the box, the crate, the barrel or the statue', 'move or rotate the chair, the table or the lamp', 'move a light', 'mirror an actor', 'turn an actor around', 'make an actor twice as big or half the size', 'make an actor bigger or smaller', 'make the sword or the enemy bigger', 'make the player taller or smaller', 'turn or rotate the cube around'],
    members: { transform: 'set_transform', location: 'set_actor_location', rotation: 'set_actor_rotation', scale: 'set_actor_scale', teleport: 'teleport_actor' },
    aliasMembers: ['set_actor_transform'],
  },
  {
    primary: 'get_transform', selector: 'readMode',
    summary: 'Read where an actor is (the player\'s pawn too): its transform now, or watch it over game time in PIE (motion): location, velocity, rotation, chosen properties and points on it (bones, sockets, components, with their height over the ground) at every interval, to prove a jump, launch, patrol, fall, foot plant or a rocking hull in one call.',
    topics: ['actor location', 'where is the actor', 'where is the player', 'player location', 'actor position', 'sample motion', 'record trajectory', 'track actor over time', 'foot sliding', 'feet slide', 'foot contact', 'bone position over time', 'copy an actor\'s transform or position'],
    members: { transform: 'get_transform', motion: 'sample_motion' },
    aliasMembers: ['get_actor_transform'],
  },
  {
    primary: 'edit_component', selector: 'edit',
    summary: 'Add a component to a placed actor (edit add: componentType, componentName, meshPath, properties; attached under its root, a light made Movable), remove one (remove), or set one or more of its properties (set_property, set_properties: a placed mesh\'s StaticMesh or OverrideMaterials, a light\'s Intensity or LightColor to turn on a light, dim it or make it brighter, a TextRender\'s Text), or make its lights flicker (flicker: speed and depth, enabled false stops it). remove takes the exact name, refused with COMPONENT_NOT_FOUND naming the nearest, and only a component added to this actor: one from its class or Blueprint, or its root, is COMPONENT_NOT_REMOVABLE (a Blueprint component comes out with edit_scs). Each is one undo step and leaves the level unsaved; while Play runs they change the playing copy.',
    topics: ['add component', 'remove component', 'component property', 'set static mesh', 'change mesh', 'swap mesh', 'replace actor mesh', 'override materials', 'set component property value', 'actor component', 'text render text', 'change sign text', 'set light intensity', 'turn off a light', 'turn on a light', 'make a light brighter', 'dim the light', 'rotating platform', 'moving platform', 'spin an object', 'rotate an object forever', 'make a spinning coin or platform', 'make an actor movable or static', 'light color', 'light radius', 'change light color', '3d text', 'world text', 'floating text above actor', 'text above head', 'make a light flicker', 'a flickering torch, candle or lamp', 'a broken light flickering or stop it flickering', 'turn off the sun or a lamp', 'a blue or warmer light colour', 'a light made smaller or reaching further', 'make the cube spin or rotate'],
    members: { add: 'add_component', remove: 'remove_component', set_property: 'set_component_property', set_properties: 'set_component_properties', flicker: 'set_light_flicker' },
  },
  {
    primary: 'get_components', selector: 'info',
    summary: 'List an actor\'s components (info components, the default): each with name, class, path, relative location, rotation and scale (and worldScale when a parent scales it), attach parent and visibility; componentNames keeps only those, naming the others in missingComponents. A Blueprint asset path lists its class default, which holds only the native components, not the ones the Blueprint adds (read those with edit_scs). Or read its bounds (bounds): origin and half-size extent of every component, colliding or not. The running game copy while Play runs.',
    topics: ['actor components', 'actor bounds', 'bounding box'],
    members: { components: 'get_components', bounds: 'get_actor_bounds' },
    aliasMembers: ['get_actor_components'],
  },
  {
    primary: 'add_tag', selector: 'tagOp',
    summary: 'Add a tag to an actor (tagOp add, the default; with actorNames to many in one undo step) or remove one (remove), leaving its level unsaved. A tag is kept once and wasPresent says whether it was there; actorNames that match nothing come back in missing, and ACTOR_NOT_FOUND answers when none matched. Tags drive find_by_tag, delete_by_tag and the Actor Has Tag node; mcp.placement.ok makes the placement checks skip the actor.',
    members: { add: 'add_tag', remove: 'remove_tag' },
  },
  { primary: 'find_by_tag', summary: 'Find the actors in the level whose Tags list holds a tag (the whole tag, case-insensitive), each with name, path and class: the first limit of them (default 200), with truncated and totalCount when more match. The Play-In-Editor world is searched while a session runs.', members: ['find_actors_by_tag'] },
  { primary: 'set_material', summary: 'Put a material on an actor, or a flat colour with color ("#rrggbb", "#rgb" or a name such as red or white: a material instance of the engine\'s BasicShapeMaterial, made once under /Game/Materials/Colors and reused, which replaces the slot\'s material, textures and all), or a glowing one with glow (that colour, white without color, lit from within at that strength: an instance of M_ColorGlow in the same folder): slot materialSlot (default 0) of its first mesh component that has that slot, of componentName, or of every one with allComponents; materials sets slot i from entry i ("" skips one), and actorNames gives many actors the same material. One undo step. Refusals: MATERIAL_NOT_FOUND, ACTOR_NOT_FOUND, COMPONENT_NOT_FOUND, MATERIAL_SLOT_NOT_FOUND (no component has the slot), INVALID_ARGUMENT (a colour it cannot read, or color or glow with materialPath); a list that partly failed answers MATERIAL_BATCH_INCOMPLETE naming each failure. While Play runs it changes the playing copy, gone when play stops.', members: ['apply_material', 'set_actor_material'] },
  { primary: 'attach', summary: 'Attach childActor to parentActor, in one undo step: to its root, or to componentName (the mesh that owns a bone) at socketName (a bone or socket, SOCKET_NOT_FOUND when missing). It keeps its world placement, or with snapToTarget moves onto the socket (scale kept) and then takes relativeLocation and relativeRotation in the socket space, which is how a grip is tuned. The parent becomes its owner. ACTOR_NOT_FOUND, COMPONENT_NOT_FOUND, CYCLE_DETECTED for itself, ATTACH_FAILED when the engine refuses (a parent already under the child, or a Static child under a moving parent: make it Movable).', members: ['attach_actor'] },
  { primary: 'detach', topics: ['unparent actor', 'remove actor from parent'], summary: 'Detach an actor from its parent, keeping where it stands in the world (KeepWorldTransform) and clearing its owner, in one undo step. An actor attached to nothing answers success with detached false. While Play In Editor runs it detaches the playing copy, not the editor actor.', members: ['detach_actor'] },
  {
    primary: 'delete', selector: 'deleteScope',
    summary: 'Delete actors (deleteScope actors, the default: actorName or actorNames, exact names only, so TestActor never takes TestActor_Copy) or every actor carrying tag or any of tags (by_tag), in one undo step; every actor of a kind (all the cubes) goes by the names find lists. While Play runs they leave the running game, which nothing saves or undoes. Replies deletedCount and deleted; names not found refuse with NOT_FOUND, or DELETE_PARTIAL when some went, both carrying the deleted and missing lists.',
    topics: ['delete actor', 'destroy actor', 'remove actor', 'delete by tag', 'delete spawned actor', 'delete selected actor', 'remove actor from level', 'delete all cubes or all lights', 'delete everything in the level', 'remove the fog, the sky or the clouds', 'remove all the enemies or the props', 'clear the level'],
    members: { actors: 'delete', by_tag: 'delete_by_tag' },
    aliasMembers: ['destroy_actor'],
  },
  {
    primary: 'find', selector: 'findBy',
    // The summary says "in any slot", not "use": a plain "which meshes use this material" is the asset lookup, and a use word
    // here counts toward this record on the native door.
    summary: 'Find actors by class, by name, by the static mesh they draw (mesh) or by a material in any of their slots (material); the mesh and material finds name the matching components.',
    topics: ['find actor', 'find actor by name', 'find actors by class', 'search actors'],
    members: { class: 'find_by_class', name: 'find_by_name', mesh: 'find_by_mesh', material: 'find_by_material' },
    aliasMembers: { class: 'find_actors_by_class', name: 'find_actors_by_name' },
  },
  { primary: 'set_visibility', summary: 'Show or hide an actor in the editor and in play (visible, default true), or many actors at once with actorNames, as one undo step. Collision follows visibility: hiding turns its collision off, and showing turns it back on, even where set_actor_collision had turned it off. Showing also shows every primitive component, including one hidden on purpose. The single form replies visible as read back plus actorName; the many form replies updatedActors, affectedActors and the names not found under missing. A name that matches nothing is ACTOR_NOT_FOUND; an actor that does not read back as asked is VISIBILITY_MISMATCH.', members: ['set_actor_visible'] },
];
