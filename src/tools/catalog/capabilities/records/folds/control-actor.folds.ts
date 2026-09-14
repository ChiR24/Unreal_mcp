// Fold specs for control_actor. Data only; see ../shared/fold.ts.
import type { FoldSpec } from '../shared/fold-types.js';

export const CONTROL_ACTOR_FOLDS: readonly FoldSpec[] = [
  {
    primary: 'spawn', selector: 'spawnKind',
    summary: 'Spawn an actor from a class or mesh path, or from a Blueprint.',
    topics: ['spawn actor', 'spawn blueprint', 'place actor', 'add actor to level'],
    members: { class: 'spawn', blueprint: 'spawn_blueprint' },
    aliasMembers: ['spawn_actor'],
  },
  {
    primary: 'set_transform', selector: 'transformMode',
    summary: 'Set an actor\'s transform: full transform, or location, rotation or scale alone, or teleport it.',
    topics: ['move actor', 'rotate actor', 'scale actor', 'set actor location', 'set actor rotation', 'teleport actor'],
    members: { transform: 'set_transform', location: 'set_actor_location', rotation: 'set_actor_rotation', scale: 'set_actor_scale', teleport: 'teleport_actor' },
    aliasMembers: ['set_actor_transform'],
  },
  { primary: 'get_transform', summary: 'Read an actor\'s transform.', members: ['get_actor_transform'] },
  {
    primary: 'edit_component', selector: 'edit',
    summary: 'Add or remove an actor component, or set one or more of its properties.',
    topics: ['add component', 'remove component', 'component property', 'actor component'],
    members: { add: 'add_component', remove: 'remove_component', set_property: 'set_component_property', set_properties: 'set_component_properties' },
  },
  {
    primary: 'get_components', selector: 'info',
    summary: 'List an actor\'s components, or read its bounds.',
    topics: ['actor components', 'actor bounds', 'bounding box'],
    members: { components: 'get_components', bounds: 'get_actor_bounds' },
    aliasMembers: ['get_actor_components'],
  },
  {
    primary: 'add_tag', selector: 'tagOp',
    summary: 'Add a tag to an actor, or remove one.',
    members: { add: 'add_tag', remove: 'remove_tag' },
  },
  { primary: 'find_by_tag', summary: 'Find actors carrying a tag.', members: ['find_actors_by_tag'] },
  { primary: 'set_material', summary: 'Apply a material to an actor\'s mesh component(s).', members: ['apply_material', 'set_actor_material'] },
  { primary: 'attach', summary: 'Attach an actor to a parent actor.', members: ['attach_actor'] },
  { primary: 'detach', summary: 'Detach an actor from its parent.', members: ['detach_actor'] },
  {
    primary: 'delete', selector: 'deleteScope',
    summary: 'Delete actors by name, or every actor carrying a tag.',
    topics: ['delete actor', 'destroy actor', 'remove actor', 'delete by tag', 'delete spawned actor', 'delete selected actor', 'remove actor from level'],
    members: { actors: 'delete', by_tag: 'delete_by_tag' },
    aliasMembers: ['destroy_actor'],
  },
  {
    primary: 'find', selector: 'findBy',
    summary: 'Find actors by class or by name.',
    topics: ['find actor', 'find actor by name', 'find actors by class', 'search actors'],
    members: { class: 'find_by_class', name: 'find_by_name' },
    aliasMembers: { class: 'find_actors_by_class', name: 'find_actors_by_name' },
  },
  { primary: 'set_visibility', summary: 'Show or hide an actor.', members: ['set_actor_visible'] },
];
