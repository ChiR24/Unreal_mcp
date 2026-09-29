/**
 * Visibility and query records: set_visibility/set_actor_visible,
 * get_components/get_actor_components, get_actor_bounds, list.
 */
import type { CapabilityRecordSource } from '../../model.js';
import { buildCoreRecord } from '../core/builder.js';
import { DOMAIN, P } from './properties.js';

const FAMILY_VISIBILITY = 'visibility';
const FAMILY_QUERY = 'query';
const COMPONENT_NAMES = { type: 'array', items: { type: 'string' }, description: 'Return only these components, by name (case-insensitive); a name that matches none is listed under missingComponents.' };
const MISSING_COMPONENTS = { type: 'array', items: { type: 'string' }, description: 'componentNames that matched no component.' };
// The listed rows of control_actor.list: the shared matched-actor row, plus the distance near adds.
const LISTED_ACTORS = {
  ...P.actors,
  items: {
    ...P.actors.items,
    properties: {
      ...P.actors.items.properties,
      distance: { type: 'number', description: 'With near: world units from the point to the actor\'s bounding box, 0 when the box contains it. Rows are sorted by it, nearest first.' },
    },
  },
  description: 'Matched actors; with near, nearest first, each with its distance.',
};
const COUNT_ROW = {
  type: 'object',
  properties: { name: { type: 'string', description: 'Class, tag or folder.' }, count: { type: 'number', description: 'Matching actors.' } },
  required: ['name', 'count'],
  additionalProperties: false,
};

export const STATE_RECORDS: readonly CapabilityRecordSource[] = [
  buildCoreRecord({
    parentTool: 'control_actor',
    action: 'set_visibility',
    domain: DOMAIN,
    family: FAMILY_VISIBILITY,
    topics: ['hide actor', 'show actor', 'toggle visibility', 'hidden in game'],
    aliases: ['control_actor.hide_actor', 'control_actor.show_actor'],
    summary: 'Toggle the visibility of an actor in the level.',
    whenToUse: ['An actor must be shown or hidden without being deleted.'],
    whenNotToUse: ['The actor should be removed (use delete).'],
    inputProps: { actorName: P.actorName, visible: P.visible },
    required: ['actorName'],
    effect: 'write',
    behavior: { idempotency: 'idempotent' },
    exampleInput: { action: 'set_visibility', actorName: 'Cube1', visible: false },
  }),
  buildCoreRecord({
    parentTool: 'control_actor',
    action: 'set_actor_visible',
    domain: DOMAIN,
    family: FAMILY_VISIBILITY,
    summary: 'Alias of set_visibility. The bridge dispatches both names to the same handler.',
    whenToUse: ['Preferred when callers use the explicit set_actor_visible verb.'],
    whenNotToUse: ['Use set_visibility to avoid alias normalization.'],
    inputProps: { actorName: P.actorName, visible: P.visible },
    required: ['actorName'],
    effect: 'write',
    behavior: { idempotency: 'idempotent' },
    exampleInput: { action: 'set_actor_visible', actorName: 'Cube1', visible: true },
  }),
  buildCoreRecord({
    parentTool: 'control_actor',
    action: 'get_components',
    domain: DOMAIN,
    family: FAMILY_QUERY,
    summary: 'List all components attached to an actor with their relative transforms.',
    whenToUse: ['The component composition of an actor must be inspected.'],
    whenNotToUse: ['A single component property is needed (use get_component_property).'],
    inputProps: { actorName: P.actorName, componentNames: COMPONENT_NAMES },
    required: ['actorName'],
    outputProps: { components: P.components, missingComponents: MISSING_COMPONENTS },
    outputRequired: [],
    effect: 'read',
    exampleInput: { action: 'get_components', actorName: 'Cube1' },
    exampleOutput: { success: true, message: 'Components for Cube1', components: [{ name: 'StaticMesh', class: 'StaticMeshComponent' }] },
  }),
  buildCoreRecord({
    parentTool: 'control_actor',
    action: 'get_actor_bounds',
    domain: DOMAIN,
    family: FAMILY_QUERY,
    summary: 'Read the world-space bounding box of an actor; normalizes to get_bounding_box.',
    whenToUse: ['The spatial extent of an actor must be measured.'],
    whenNotToUse: ['The actor has no renderable components (bounds are undefined).'],
    inputProps: { actorName: P.actorName },
    required: ['actorName'],
    // The handler (McpAutomationBridge_ControlActorQuery.cpp,
    // HandleControlActorGetBoundingBox) emits `origin` and `extent`. This record
    // declared `location`/`scale`, which the handler never sets, so output
    // projection dropped BOTH and the capability answered "Bounding box
    // retrieved" with no bounds at all — leaving procedural geometry sizes
    // unverifiable through the API.
    outputProps: {
      origin: { type: 'array', items: { type: 'number' }, minItems: 3, maxItems: 3, description: 'World-space centre of the bounding box as [x, y, z].' },
      extent: { type: 'array', items: { type: 'number' }, minItems: 3, maxItems: 3, description: 'Half-size of the bounding box along each axis as [x, y, z].' },
    },
    outputRequired: [],
    effect: 'read',
    exampleInput: { action: 'get_actor_bounds', actorName: 'Cube1' },
    exampleOutput: { success: true, message: 'Bounds for Cube1', origin: [0, 0, 50], extent: [50, 50, 50] },
  }),
  buildCoreRecord({
    parentTool: 'control_actor',
    action: 'list',
    domain: DOMAIN,
    family: FAMILY_QUERY,
    topics: ['list actors', 'all actors in level', 'actors in scene', 'enumerate actors', 'world outliner', 'actors in level', 'level actors', 'actor positions', 'actor locations', 'actor transforms', 'level layout', 'variable values of many actors', 'how many actors', 'count actors', 'actors in the level'],
    aliases: ['control_actor.list_actors'],
    summary: 'List actors in the current level with their label, class, location, rotation and scale, plus any propertyNames values, narrowed by name filter, tag, class or outliner folder; page on with offset. near (a world point) with radius is how to find what is near a point, nearest first, each with its distance. summary counts the level by class, tag and folder instead.',
    whenToUse: ['The actors present in the level must be enumerated.', 'Every actor with one tag, of one class or in one outliner folder must be found, for example to see what a delete_by_tag would remove.', 'Something seen in a screenshot or at a coordinate must be identified: list the actors near that point, nearest first.'],
    whenNotToUse: ['A specific known actor name is already available (use find_by_name).'],
    inputProps: {
      limit: P.limit, filter: P.filter, offset: { type: 'number', description: 'Skip this many matching actors; the next page starts at nextOffset from the previous reply.' },
      tag: { type: 'string', description: 'Only actors carrying this actor tag. With summary, shows what the tag covers before a delete_by_tag removes it.' },
      className: { type: 'string', description: 'Only actors of this class or a subclass, by name or path: TextRenderActor, Light (every light type), or a Blueprint such as BP_Sign, with or without _C.' },
      folder: { type: 'string', description: 'Only actors in this outliner folder or a folder under it (Level/Stage matches Level/Stage/Signs); "(none)" for the actors at the root.' },
      propertyNames: { type: 'array', items: { type: 'string' }, description: 'Property or Blueprint variable names to read on every listed actor, returned per actor under properties, e.g. Kind and Content, or a component\'s as "Component.Property" (StaticMeshComponent.LDMaxDrawDistance, Visual.RelativeScale3D; the component by its name), keyed as asked. A name that an actor lacks, or whose component it lacks, is listed under missingProperties for that actor.' },
      near: { ...P.location, description: 'A world point as [x, y, z] (an {x, y, z} object works too): list what is close to it, nearest first, each row with its distance. With radius, only actors whose bounds come within radius of the point; without it, every matching actor sorted by distance. Combines with filter, tag, className and folder.' },
      radius: { type: 'number', minimum: 0, description: 'World units from near: an actor is listed when its bounding box comes within this distance of the point (the distance is 0 for a box that contains the point, such as a floor slab under it). Needs near.' },
      summary: { type: 'boolean', description: 'Count the matching actors by class, actor tag and outliner folder (byClass, byTag, byFolder) instead of listing them; limit and offset do not apply. The cheap first look at an unfamiliar level.' },
    },
    required: [],
    outputProps: {
      actors: LISTED_ACTORS, count: P.count, totalCount: P.totalCount, excludedCount: P.excludedCount, isPieWorld: P.isPieWorld, worldName: P.worldName, filter: P.filter,
      hasMore: { type: 'boolean', description: 'More matching actors exist past this page.' },
      nextOffset: { type: 'number', description: 'The offset of the next page; present only when hasMore.' },
      byClass: { type: 'array', items: COUNT_ROW, description: 'summary: matching actors per class, as {name, count} rows sorted by name.' },
      byTag: { type: 'array', items: COUNT_ROW, description: 'summary: matching actors per actor tag, as {name, count} rows sorted by name.' },
      byFolder: { type: 'array', items: COUNT_ROW, description: 'summary: matching actors per outliner folder, as {name, count} rows sorted by name ("(none)" for the root).' },
    },
    outputRequired: [],
    effect: 'read',
    exampleInput: { action: 'list', limit: 50, filter: 'Cube' },
    exampleOutput: { success: true, message: 'Found 1 actors: Cube1', actors: [{ label: 'Cube1', name: 'Cube1', location: { x: 0, y: 0, z: 50 }, rotation: { pitch: 0, yaw: 0, roll: 0 }, scale: { x: 1, y: 1, z: 1 } }], count: 1, totalCount: 1, excludedCount: 9 },
  }),
];
