// Fold specs for control_actor. Data only; see ../shared/fold.ts.
import type { FoldSpec } from '../shared/fold-types.js';

export const CONTROL_ACTOR_FOLDS: readonly FoldSpec[] = [
  {
    primary: 'spawn', selector: 'spawnKind',
    summary: 'Spawn an actor from a class or mesh path, or from a Blueprint, or many actors in one batch.',
    topics: ['spawn actor', 'spawn blueprint', 'place actor', 'add actor to level', 'spawn many actors', 'add player start', 'spawn enemy', 'spawn actors in a row', 'spawn actors in a grid'],
    members: { class: 'spawn', blueprint: 'spawn_blueprint', batch: 'spawn_batch' },
    aliasMembers: ['spawn_actor'],
  },
  {
    primary: 'set_transform', selector: 'transformMode',
    summary: 'Set an actor\'s transform: full transform, or location, rotation or scale alone, or teleport it.',
    topics: ['move actor', 'rotate actor', 'scale actor', 'set actor location', 'set actor rotation', 'teleport actor', 'make actor bigger', 'make actor smaller', 'resize actor', 'shrink actor'],
    members: { transform: 'set_transform', location: 'set_actor_location', rotation: 'set_actor_rotation', scale: 'set_actor_scale', teleport: 'teleport_actor' },
    aliasMembers: ['set_actor_transform'],
  },
  {
    primary: 'get_transform', selector: 'readMode',
    summary: 'Read where an actor is (the player\'s pawn too): its transform now, or watch it over game time in PIE (motion): location, velocity, rotation, chosen properties and points on it (bones, sockets, components, with their height over the ground) at every interval, to prove a jump, launch, patrol, fall, foot plant or a rocking hull in one call.',
    topics: ['actor location', 'where is the actor', 'where is the player', 'player location', 'actor position', 'sample motion', 'record trajectory', 'track actor over time', 'foot sliding', 'feet slide', 'foot contact', 'bone position over time'],
    members: { transform: 'get_transform', motion: 'sample_motion' },
    aliasMembers: ['get_actor_transform'],
  },
  {
    primary: 'edit_component', selector: 'edit',
    summary: 'Add a component to a placed actor (edit add: componentType, componentName, meshPath, properties; attached under its root, a light made Movable), remove one (remove), or set one or more of its properties (set_property, set_properties: a placed mesh\'s StaticMesh or OverrideMaterials, a light\'s Intensity or LightColor to turn on a light, dim it or make it brighter, a TextRender\'s Text). remove takes the exact name, refused with COMPONENT_NOT_FOUND naming the nearest, and only a component added to this actor: one from its class or Blueprint, or its root, is COMPONENT_NOT_REMOVABLE (a Blueprint component comes out with edit_scs). Each is one undo step and leaves the level unsaved; while Play runs they change the playing copy.',
    topics: ['add component', 'remove component', 'component property', 'set static mesh', 'change mesh', 'swap mesh', 'replace actor mesh', 'override materials', 'set component property value', 'actor component', 'text render text', 'change sign text', 'set light intensity', 'turn off a light', 'turn on a light', 'make a light brighter', 'dim the light', 'rotating platform', 'moving platform', 'spin an object', 'rotate an object forever', 'light color', 'light radius', 'change light color', '3d text', 'world text', 'floating text above actor', 'text above head'],
    members: { add: 'add_component', remove: 'remove_component', set_property: 'set_component_property', set_properties: 'set_component_properties' },
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
  { primary: 'set_material', summary: 'Put a material on an actor: slot materialSlot (default 0) of its first mesh component that has that slot, of componentName, or of every one with allComponents; materials sets slot i from entry i ("" skips one), and actorNames gives many actors the same material. One undo step. Refusals: MATERIAL_NOT_FOUND, ACTOR_NOT_FOUND, COMPONENT_NOT_FOUND, MATERIAL_SLOT_NOT_FOUND (no component has the slot); a list that partly failed answers MATERIAL_BATCH_INCOMPLETE naming each failure. While Play runs it changes the playing copy, gone when play stops.', members: ['apply_material', 'set_actor_material'] },
  { primary: 'attach', summary: 'Attach childActor to parentActor, in one undo step: to its root, or to componentName (the mesh that owns a bone) at socketName (a bone or socket, SOCKET_NOT_FOUND when missing). It keeps its world placement, or with snapToTarget moves onto the socket (scale kept) and then takes relativeLocation and relativeRotation in the socket space, which is how a grip is tuned. The parent becomes its owner. ACTOR_NOT_FOUND, COMPONENT_NOT_FOUND, CYCLE_DETECTED for itself, ATTACH_FAILED when the engine refuses (a parent already under the child, or a Static child under a moving parent: make it Movable).', members: ['attach_actor'] },
  { primary: 'detach', topics: ['unparent actor', 'remove actor from parent'], summary: 'Detach an actor from its parent, keeping where it stands in the world (KeepWorldTransform) and clearing its owner, in one undo step. An actor attached to nothing answers success with detached false. While Play In Editor runs it detaches the playing copy, not the editor actor.', members: ['detach_actor'] },
  {
    primary: 'delete', selector: 'deleteScope',
    summary: 'Delete actors (deleteScope actors, the default: actorName or actorNames, exact names only, so TestActor never takes TestActor_Copy) or every actor carrying tag or any of tags (by_tag), in one undo step. While Play runs they leave the running game, which nothing saves or undoes. Replies deletedCount and deleted; names not found refuse with NOT_FOUND, or DELETE_PARTIAL when some went, both carrying the deleted and missing lists.',
    topics: ['delete actor', 'destroy actor', 'remove actor', 'delete by tag', 'delete spawned actor', 'delete selected actor', 'remove actor from level'],
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
  { primary: 'set_visibility', summary: 'Show or hide an actor, or many at once with actorNames.', members: ['set_actor_visible'] },
];
