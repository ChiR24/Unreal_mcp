/**
 * Geometry deform family records (13 actions).
 *
 * Grounded in manage-geometry-tool.ts deform verbs (bend/twist/taper/noise/
 * smooth/relax/stretch/spherify/cylindrify/lattice/displace)
 * and native Geometry dispatch. All require the GeometryScripting plugin.
 */
import type { CapabilityRecordSource } from '../../model.js';
import { buildWorldRecord } from './builder.js';
import { P } from './properties.js';

const F = 'deform';
const PLUGIN = ['GeometryScripting'] as const;
// smooth and relax read strength as the Laplacian step size (0-1) per iteration.
const SMOOTH_STEP = { type: 'number', description: 'Smoothing step per iteration, 0 to 1 (how far each vertex moves toward its neighbours).' };

export const GEOMETRY_DEFORM_RECORDS: readonly CapabilityRecordSource[] = [
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'bend', plugins: PLUGIN,
    family: F, summary: 'Bend a dynamic mesh along axis (X, Y or Z, default Z) by strength degrees (default 45), the bend spread over its length along that axis and centred on its local origin, the pivot. The reply gives the angle.', whenToUse: ['A mesh must be bent.'], whenNotToUse: ['A twist is needed; use twist.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, axis: { ...P.axis, description: 'Axis to deform along: X, Y or Z (default Z).' }, strength: P.strength }, required: [], requiredOneOf: ['actorName', 'targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'bend', targetActor: 'DM_A', axis: 'X', strength: 30 },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'twist', plugins: PLUGIN,
    family: F, summary: 'Twist a dynamic mesh about axis (X, Y or Z, default Z) by strength degrees (default 45) across its length along that axis, centred on its local origin. The reply gives the angle.', whenToUse: ['A mesh must be twisted.'], whenNotToUse: ['A bend is needed; use bend.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, axis: { ...P.axis, description: 'Axis to deform along: X, Y or Z (default Z).' }, strength: P.strength }, required: [], requiredOneOf: ['actorName', 'targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'twist', targetActor: 'DM_A', axis: 'Z', strength: 45 },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'taper', plugins: PLUGIN,
    family: F, summary: 'Flare a dynamic mesh around axis (X, Y or Z, default Z): its cross-section grows by strength percent (default 50) at the middle of its length and not at all at the ends, so a positive strength bulges it and a negative one pinches it (a sine profile, not a straight taper).', whenToUse: ['A mesh must be tapered.'], whenNotToUse: ['A stretch is needed; use stretch.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, axis: { ...P.axis, description: 'Axis to deform along: X, Y or Z (default Z).' }, strength: P.strength }, required: [], requiredOneOf: ['actorName', 'targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'taper', targetActor: 'DM_A', axis: 'Y', strength: 20 },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'noise_deform', plugins: PLUGIN,
    family: F, summary: 'Displace a dynamic mesh along its normals by Perlin noise: strength is the height in cm (default 5) and frequency the noise cycles per cm (default three bumps across its largest dimension). The reply gives both.', whenToUse: ['A mesh must be displaced by noise.'], whenNotToUse: ['A texture displacement is needed; use displace_by_texture.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, strength: P.strength, frequency: P.frequency }, required: [], requiredOneOf: ['actorName', 'targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'noise_deform', targetActor: 'DM_A', strength: 5 },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'smooth', plugins: PLUGIN,
    family: F, summary: 'Smooth a dynamic mesh: iterations (default 10) of Laplacian smoothing, each moving every vertex strength (default 0.2) of the way toward its neighbours; the mesh shrinks a little. Fewer than 1 iteration or a strength of 0 is refused.', whenToUse: ['A mesh must be smoothed.'], whenNotToUse: ['A mesh must be relaxed; use relax.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, iterations: P.iterations, strength: SMOOTH_STEP }, required: [], requiredOneOf: ['actorName', 'targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'smooth', targetActor: 'DM_A', iterations: 4 },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'relax', plugins: PLUGIN,
    family: F, summary: 'Relax a dynamic mesh: the same smoothing as smooth with other defaults (3 iterations, strength 0.5), to even out vertex spacing. Fewer than 1 iteration or a strength of 0 is refused.', whenToUse: ['A mesh must be relaxed without shrinking.'], whenNotToUse: ['A mesh must be smoothed; use smooth.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, iterations: P.iterations, strength: SMOOTH_STEP }, required: [], requiredOneOf: ['actorName', 'targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'relax', targetActor: 'DM_A', iterations: 2 },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'stretch', plugins: PLUGIN,
    family: F, summary: 'Scale a dynamic mesh along one axis (X, Y or Z, default Z) by strength (default 1.5: 2 doubles it, 0.5 halves it) about its local origin, the other axes unchanged.', whenToUse: ['A mesh must be stretched.'], whenNotToUse: ['A taper is needed; use taper.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, axis: { ...P.axis, description: 'Axis to deform along: X, Y or Z (default Z).' }, strength: P.strength }, required: [], requiredOneOf: ['actorName', 'targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'stretch', targetActor: 'DM_A', axis: 'Z', strength: 1.5 },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'spherify', plugins: PLUGIN,
    family: F, summary: 'Pull every vertex of a dynamic mesh toward a sphere around its bounds centre, as large as its largest half-extent: strength 0 to 1 (default 1, clamped) is how far, 1 making it that sphere.', whenToUse: ['A mesh must be pushed toward spherical.'], whenNotToUse: ['A cylindrify is needed; use cylindrify.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, strength: P.strength }, required: [], requiredOneOf: ['actorName', 'targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'spherify', targetActor: 'DM_A', strength: 0.8 },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'cylindrify', plugins: PLUGIN,
    family: F, summary: 'Pull every vertex of a dynamic mesh toward a cylinder around axis (X, Y or Z, default Z) through its bounds centre, its radius the average distance of the vertices from that axis: strength 0 to 1 (default 1, clamped) is how far. The reply gives avgRadius and verticesModified.', whenToUse: ['A mesh must be pushed toward cylindrical.'], whenNotToUse: ['A spherify is needed; use spherify.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, strength: P.strength, axis: P.axis }, required: [], requiredOneOf: ['actorName', 'targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'cylindrify', targetActor: 'DM_A', strength: 0.6 },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'lattice_deform', plugins: PLUGIN,
    family: F, summary: 'Ripple a dynamic mesh with a sine wave: every vertex moves along axis (X, Y or Z, default Z) by a wave latticeResolution cycles across its bounds (2 to 16, default 3), at most weight (-2 to 2, default 0.25) times a quarter of its largest size, fading to nothing three quarters of that size from position (default the bounds centre). A procedural ripple, not a cage of control points.', whenToUse: ['A complex deformation must be applied via a lattice.'], whenNotToUse: ['A simple bend is needed; use bend.'],
    inputProps: { actorName: P.actorName, position: P.position, targetActor: P.targetActor, latticeResolution: P.latticeResolution, weight: P.weight, axis: P.axis }, required: [], requiredOneOf: ['actorName', 'targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'medium',
    exampleInput: { action: 'lattice_deform', targetActor: 'DM_A', latticeResolution: 4 },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'displace_by_texture', plugins: PLUGIN,
    family: F, summary: 'Raise a dynamic mesh by a heightmap texture projected straight down over its X/Y bounds: each vertex moves along axis (X, Y or Z, default Z) by (luminance - midpoint) x heightScale (midpoint 0 to 1, default 0.5; heightScale cm, default 10). A texture that does not load is TEXTURE_NOT_FOUND, one whose source format cannot be read TEXTURE_FORMAT_UNSUPPORTED.', whenToUse: ['A mesh must be displaced by a texture.'], whenNotToUse: ['A noise displacement is needed; use noise_deform.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, texturePath: P.texturePath, heightScale: P.heightScale, midpoint: P.midpoint, axis: P.axis }, required: ['texturePath'], requiredOneOf: ['actorName', 'targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'displace_by_texture', targetActor: 'DM_A', texturePath: '/Game/Textures/T_Height' },
  }),
];
