/**
 * Decal family records (1 action): deferred decals laid onto surfaces.
 */
import type { CapabilityRecordSource } from '../../model.js';
import { buildRecord } from './helpers.js';

const F = 'decal';
const POINT = { type: 'object', additionalProperties: false, required: ['x', 'y', 'z'], properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } } };

export const DECAL_RECORDS: readonly CapabilityRecordSource[] = [
  buildRecord({
    id: 'build_environment.place_decals', action: 'place_decals', family: F,
    topics: ['add a decal', 'place decal', 'decal on a wall', 'stain on the floor', 'project a decal', 'scatter decals', 'dirt patches', 'cracks on the ground', 'puddle decal', 'poster on a wall', 'grime and wear'],
    summary: 'Lay deferred decals onto the surfaces under points, as a level artist drops stains, cracks, moss, puddles or posters: each point is traced along direction (default straight down) to the first surface with collision, and a decal actor is placed flat on what that actor draws there (where the line meets the triangles of its mesh, which can lie well inside a box used as its collision; a mesh that draws nothing along the line, as at the holes of a grate, is passed through), projecting into the surface, its texture upright as seen from outside (its top up a wall, toward world +X on a floor) and turned by angle. points places one per point; area scatters count of them over a disc facing the trace, on a floor or up a wall (spaced half a decal apart, each turned at random unless angle is given, the same for the same seed). The texture covers width x height cm of the surface (default 200 x 200; scaleJitter varies the size by up to that share) and projects depth cm through it (default half its smaller side, at least 20), so keep depth small on thin or curved surfaces or it spills onto what is behind; the box is centred where the line met the surface, so surface under the decal further than half the depth from that spot (behind a bump the line hit, as a fender on a quay wall) gets none of it. Each decal is labelled label_01, label_02 ... (default Decal) in the outliner folder (default Decals) and tagged mcp.decal; all of them are one undo step. onActors limits the surfaces a decal may land on (a point whose surface is another actor is listed in missed, not_listed); a point that hits nothing within traceCm (default 1000) is missed no_surface. dryRun traces and reports without placing. Refused, placing nothing: DECAL_MATERIAL_REQUIRED (the material\'s domain is not Deferred Decal: material.create_material kind decal makes one), MATERIAL_NOT_FOUND, NOTHING_PLACED (every point missed), INVALID_ARGUMENT.',
    whenToUse: ['Surfaces look too clean or too uniform and need grime, cracks, moss, puddles, scorch marks or signs without new meshes or textures.', 'A poster, sign or graffiti must sit flat on a wall.'],
    whenNotToUse: ['The decal material does not exist yet (material.create_material kind decal, then its graph).', 'Props or plants are to be scattered (the foliage tools).'],
    inputProps: {
      materialPath: { type: 'string', description: 'A decal material (Deferred Decal domain) or an instance of one.' },
      points: { type: 'array', minItems: 1, maxItems: 200, items: POINT, description: 'World points {x, y, z} in cm, one decal each, traced along direction from 200 cm before the point.' },
      area: { type: 'object', additionalProperties: false, required: ['center', 'radius', 'count'], properties: {
        center: POINT, radius: { type: 'number', minimum: 1, description: 'Disc radius in cm.' },
        count: { type: 'integer', minimum: 1, maximum: 200, description: 'Decals to scatter (fewer when the disc has no room for them half a decal apart).' },
        seed: { type: 'integer', description: 'Seed (default 1): the same seed gives the same points, turns and sizes.' } },
        description: 'Scatter instead of points: count points in the disc of radius around center facing direction (level at its height for a trace down or up; upright, across and up, for a trace into a wall), each traced along direction.' },
      direction: { type: 'object', additionalProperties: false, required: ['x', 'y', 'z'], properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } }, description: 'Direction the points are traced in (default {0, 0, -1}: down); {0, 1, 0} finds a wall in +Y.' },
      width: { type: 'number', minimum: 1, maximum: 100000, description: 'Width of the texture on the surface, in cm (default 200).' },
      height: { type: 'number', minimum: 1, maximum: 100000, description: 'Height of the texture on the surface (up a wall), in cm (default 200).' },
      depth: { type: 'number', minimum: 1, description: 'How far it projects through the surface, in cm (default half its smaller side, at least 20).' },
      angle: { type: 'number', description: 'Turn about the surface in degrees, counter-clockwise as seen from outside (default 0 for points, random per decal for area).' },
      scaleJitter: { type: 'number', minimum: 0, maximum: 0.5, description: 'Each decal is scaled by up to this share up or down (default 0).' },
      onActors: { type: 'array', maxItems: 100, items: { type: 'string' }, description: 'Labels or names of the only actors a decal may land on.' },
      traceCm: { type: 'number', minimum: 1, maximum: 100000, description: 'How far past each point to look for a surface (default 1000).' },
      sortOrder: { type: 'integer', description: 'Draw order among overlapping decals: higher draws on top (default 0).' },
      fadeScreenSize: { type: 'number', minimum: 0, description: 'Screen size below which the decal fades out (engine default 0.01; 0 never fades).' },
      label: { type: 'string', description: 'Label prefix (default Decal).' },
      folder: { type: 'string', description: 'Outliner folder (default Decals).' },
      dryRun: { type: 'boolean', description: 'Trace and report without placing (default false).' },
    },
    outputProps: {
      placed: { type: 'array', items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: '{index, name, hitActor, location [x, y, z], normal, rotation [pitch, yaw, roll]}.' }, description: 'The decals placed (under dryRun, the ones that would be).' },
      missed: { type: 'array', items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: '{index, point, reason (no_surface, not_listed, spawn_failed), hitActor}.' }, description: 'Points that got no decal, and why.' },
      placedCount: { type: 'integer', description: 'How many were placed.' },
      dryRun: { type: 'boolean', description: 'True when nothing was placed on purpose.' },
      undo: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'What editor undo takes back: every decal of the call is one step. Absent under dryRun.' },
    },
    required: ['materialPath'], requiredOneOf: ['points', 'area'],
    effect: 'write', latency: 'interactive', resources: 'low',
    exampleInput: { action: 'place_decals', materialPath: '/Game/Decals/M_Decal_Dirt', area: { center: { x: 0, y: 0, z: 200 }, radius: 800, count: 12, seed: 2 }, width: 150, height: 150, scaleJitter: 0.3 },
  }),
];
