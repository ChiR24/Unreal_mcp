/**
 * Relation records: describe_relations, the level read back as relations between its actors.
 */
import type { CapabilityRecordSource } from '../../model.js';
import { buildCoreRecord } from '../core/builder.js';
import { DOMAIN } from './properties.js';

const FAMILY_FIND = 'find';

const KINDS = ['on', 'mounted', 'inside', 'nextTo', 'faces', 'groups', 'rows', 'rings', 'roles'];

const rows = (description: string) => ({
  type: 'array',
  description: `${description} members names at most 24; memberCount says how many there are when more.`,
  items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true },
  'x-unreal-reflection-boundary': true,
});

export const RELATIONS_RECORDS: readonly CapabilityRecordSource[] = [
  buildCoreRecord({
    parentTool: 'control_actor',
    action: 'describe_relations',
    domain: DOMAIN,
    family: FAMILY_FIND,
    topics: ['spatial relations', 'scene graph', 'what rests on what', 'what is on the table', 'which actors touch each other', 'actors in a row', 'actors in a ring', 'what faces what', 'describe the room layout', 'objects arranged in a grid'],
    summary: 'Read the level back as relations between its actors, the way one describes a room: what rests on what (on), what hangs on a wall (mounted), what stands inside what (inside), what touches what side by side (nextTo) and what each actor\'s front points at (faces); which actors stand together (groups), which copies of one prop line up in rows or grids or stand in a ring; and the scene\'s roles: the ground, the sun\'s elevation and azimuth, the sky actors, player starts, cameras and the navigation area. Read only.',
    whenToUse: [
      'Checking that a layout reads as intended (the lamp on the table, the chairs facing it, the shelf against the wall) without reading coordinates.',
      'Learning how an existing level is arranged before adding to it: what stands on the floor, what forms a row or a ring, where the player starts.',
      'After moving or spawning props, confirming what each now rests on or touches.',
    ],
    whenNotToUse: [
      'Finding what is sunk, floating or intersecting: use audit_placement, and fix_placement to fix it.',
      'Reading one actor\'s exact location, rotation or bounds: use get_transform or get_bounding_box.',
    ],
    inputProps: {
      kinds: { type: 'array', items: { type: 'string', enum: KINDS }, description: 'Which relations and patterns to report: on (rests on the actor under it; read from short traces down from its bottom, so an actor without collision rests on nothing), mounted (held from the side, as a picture on a wall), inside (within another\'s bounds, the tightest such), nextTo (side by side, their sides within 2 cm), faces (its forward, X axis points at the nearest actor within 20 degrees that is not a copy of it; a prop without a front, such as a crate or a table, faces whatever lies along its X axis), groups, rows (with grids), rings and roles. Omit for all of them but faces, which is listed only when kinds names it or actorNames or nameFilter narrow the report: across a whole level it would name a front for most props.' },
      actorNames: { type: 'array', items: { type: 'string' }, description: 'Report only the relations these actors take part in, as subject or object, and the patterns they belong to; the rest of the level is still judged, so a lamp still rests on a table nobody named. Names not found come back under missing; when none is found the call fails with ACTOR_NOT_FOUND.' },
      nameFilter: { type: 'string', description: 'Report only the relations and patterns of actors whose label or object name contains this text; with actorNames, of the named actors that also match.' },
      center: { type: 'object', properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } }, additionalProperties: false, description: 'With radius: judge only the actors whose bounds come within radius cm of this point (named actors always count). Refused without radius.' },
      radius: { type: 'number', description: 'With center: the radius in cm of the area judged; above 0. Refused without center.' },
      maxActors: { type: 'number', description: 'At most this many actors are judged (1-400, default 300): named actors first, then the nearest to center, else the largest. A level cut short says so with truncated and actorsInScope.' },
    },
    required: [],
    outputProps: {
      relations: { type: 'array', items: { type: 'array', items: { type: 'string' } }, description: 'One row per relation as [subject, relation, object] or [subject, relation, object, detail], grouped by subject, at most 200: ["Lamp", "on", "Table"], ["Chair_1", "faces", "Table", "25 cm away"], ["Shelf", "nextTo", "Wall", "touching its +Y side"]. An on row whose detail says an edge of it rests there stands only partly on that actor. What rests on the ground is listed only for the actors asked about by actorNames or nameFilter; roles.ground counts the rest.' },
      groups: rows('Actors standing together, each {members, center, size}: every member within about a footprint of another, as a table with its chairs and the lamp on it. A level that is one group lists none.'),
      rows: rows('Copies of one prop (same class and size) in a line along X or Y, each {members, along (x or y), spacing in cm, even}: even is true when every step is within 5 percent of the spacing.'),
      grids: rows('Copies of one prop in rows and columns, each {members, columns, rows, spacing [x, y] in cm, even}; their rows are not listed again under rows.'),
      rings: rows('Copies of one prop standing evenly around a centre at one distance, each {members, center, radius in cm}.'),
      roles: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'The scene\'s roles: ground {name, supports (how many actors rest on it), top (its height)}, the ground-like actor most rest on; sun {name, elevation, azimuth}, the brightest directional light as the sun\'s angle above the horizon and its direction in degrees from +X toward +Y; sky (the sky atmosphere, sky light, fog, cloud and sky sphere actors); playerStarts and cameras [{name, location}], forPlayer on a camera a player views through from the start; playArea {center, size}, the navigation mesh bounds.' },
      actorsExamined: { type: 'number', description: 'Actors judged: visible actors with bounds in the area, lights, volumes and other bounds-only actors left out.' },
      truncated: { type: 'boolean', description: 'Present when maxActors cut the actors judged (actorsInScope says how many there were) or 200 relations cut the list (relationCount says how many there were).' },
      actorsInScope: { type: 'number', description: 'Present when maxActors cut the list: how many actors the area held.' },
      relationCount: { type: 'number', description: 'Present when the relations list was cut at 200: how many there were.' },
      missing: { type: 'array', items: { type: 'string' }, description: 'actorNames that matched no actor.' },
    },
    outputRequired: [],
    effect: 'read',
    costLatency: 'interactive',
    costResources: 'medium',
    exampleInput: { action: 'describe_relations', center: { x: 0, y: 0, z: 0 }, radius: 1500 },
    exampleOutput: { success: true, message: 'Among 9 actors: relations 4 (on 1, nextTo 1, faces 2), groups 1, rows 1.', relations: [['Lamp', 'on', 'Table'], ['Chair_1', 'faces', 'Table', '25 cm away'], ['Chair_2', 'faces', 'Table', '25 cm away'], ['Wall', 'nextTo', 'Shelf', 'touching its -Y side']], groups: [{ members: ['Table', 'Lamp', 'Chair_1', 'Chair_2'], center: [0, 0, 70], size: [300, 100, 130] }], rows: [{ members: ['Crate_1', 'Crate_2', 'Crate_3'], along: 'x', spacing: 150, even: true }], roles: { ground: { name: 'Floor', supports: 7, top: 10 } }, actorsExamined: 9 },
  }),
];
