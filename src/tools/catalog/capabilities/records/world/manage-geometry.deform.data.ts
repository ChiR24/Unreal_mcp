/**
 * Geometry deform family records (13 actions).
 *
 * Grounded in manage-geometry-tool.ts deform verbs (bend/twist/taper/noise/
 * smooth/relax/stretch/spherify/cylindrify/lattice/displace/triangulate/poke)
 * and native Geometry dispatch. All require the GeometryScripting plugin.
 */
import type { CapabilityRecordSource } from '../../index.js';
import { buildWorldRecord } from './builder.js';
import { P } from './properties.js';

const F = 'deform';
const NR = 'Distinct manage_geometry deform verb and target; no cross-tool duplicate.';
const PLUGIN = ['GeometryScripting'] as const;

export const GEOMETRY_DEFORM_RECORDS: readonly CapabilityRecordSource[] = [
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'bend', plugins: PLUGIN,
    family: F, summary: 'Bend a dynamic mesh along an axis.', whenToUse: ['A mesh must be bent.'], whenNotToUse: ['A twist is needed; use twist.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, axis: P.axis, strength: P.strength }, required: ['targetActor', 'axis'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'bend', targetActor: 'DM_A', axis: 'X', strength: 0.5 }, exampleOutput: { success: true, message: 'Bend complete' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'twist', plugins: PLUGIN,
    family: F, summary: 'Twist a dynamic mesh along an axis.', whenToUse: ['A mesh must be twisted.'], whenNotToUse: ['A bend is needed; use bend.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, axis: P.axis, strength: P.strength }, required: ['targetActor', 'axis'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'twist', targetActor: 'DM_A', axis: 'Z', strength: 0.3 }, exampleOutput: { success: true, message: 'Twist complete' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'taper', plugins: PLUGIN,
    family: F, summary: 'Taper a dynamic mesh along an axis.', whenToUse: ['A mesh must be tapered.'], whenNotToUse: ['A stretch is needed; use stretch.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, axis: P.axis, strength: P.strength }, required: ['targetActor', 'axis'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'taper', targetActor: 'DM_A', axis: 'Y', strength: 0.2 }, exampleOutput: { success: true, message: 'Taper complete' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'noise_deform', plugins: PLUGIN,
    family: F, summary: 'Apply noise displacement to a dynamic mesh.', whenToUse: ['A mesh must be displaced by noise.'], whenNotToUse: ['A texture displacement is needed; use displace_by_texture.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, strength: P.strength }, required: ['targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'noise_deform', targetActor: 'DM_A', strength: 5 }, exampleOutput: { success: true, message: 'Noise deform complete' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'smooth', plugins: PLUGIN,
    family: F, summary: 'Smooth a dynamic mesh (iterative relaxation).', whenToUse: ['A mesh must be smoothed.'], whenNotToUse: ['A mesh must be relaxed; use relax.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, iterations: P.iterations }, required: ['targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'smooth', targetActor: 'DM_A', iterations: 4 }, exampleOutput: { success: true, message: 'Smooth complete' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'relax', plugins: PLUGIN,
    family: F, summary: 'Relax a dynamic mesh (shape-preserving).', whenToUse: ['A mesh must be relaxed without shrinking.'], whenNotToUse: ['A mesh must be smoothed; use smooth.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, iterations: P.iterations }, required: ['targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'relax', targetActor: 'DM_A', iterations: 2 }, exampleOutput: { success: true, message: 'Relax complete' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'stretch', plugins: PLUGIN,
    family: F, summary: 'Stretch a dynamic mesh along an axis.', whenToUse: ['A mesh must be stretched.'], whenNotToUse: ['A taper is needed; use taper.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, axis: P.axis, strength: P.strength }, required: ['targetActor', 'axis'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'stretch', targetActor: 'DM_A', axis: 'Z', strength: 1.5 }, exampleOutput: { success: true, message: 'Stretch complete' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'spherify', plugins: PLUGIN,
    family: F, summary: 'Spherify a dynamic mesh toward a sphere.', whenToUse: ['A mesh must be pushed toward spherical.'], whenNotToUse: ['A cylindrify is needed; use cylindrify.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, strength: P.strength }, required: ['targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'spherify', targetActor: 'DM_A', strength: 0.8 }, exampleOutput: { success: true, message: 'Spherify complete' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'cylindrify', plugins: PLUGIN,
    family: F, summary: 'Cylindrify a dynamic mesh toward a cylinder.', whenToUse: ['A mesh must be pushed toward cylindrical.'], whenNotToUse: ['A spherify is needed; use spherify.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, strength: P.strength }, required: ['targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'cylindrify', targetActor: 'DM_A', strength: 0.6 }, exampleOutput: { success: true, message: 'Cylindrify complete' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'lattice_deform', plugins: PLUGIN,
    family: F, summary: 'Deform a dynamic mesh via a control lattice.', whenToUse: ['A complex deformation must be applied via a lattice.'], whenNotToUse: ['A simple bend is needed; use bend.'],
    inputProps: { actorName: P.actorName, position: P.position, targetActor: P.targetActor, latticeResolution: P.latticeResolution, weight: P.weight }, required: ['targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'medium',
    exampleInput: { action: 'lattice_deform', targetActor: 'DM_A', latticeResolution: 4 }, exampleOutput: { success: true, message: 'Lattice deform complete' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'displace_by_texture', plugins: PLUGIN,
    family: F, summary: 'Displace a dynamic mesh by a heightmap texture.', whenToUse: ['A mesh must be displaced by a texture.'], whenNotToUse: ['A noise displacement is needed; use noise_deform.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, texturePath: P.texturePath, heightScale: P.heightScale, midpoint: P.midpoint }, required: ['targetActor', 'texturePath'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'displace_by_texture', targetActor: 'DM_A', texturePath: '/Game/Textures/T_Height' }, exampleOutput: { success: true, message: 'Displace by texture complete' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'triangulate', plugins: PLUGIN,
    family: F, summary: 'Triangulate polygons of a dynamic mesh.', whenToUse: ['Polygons must be converted to triangles.'], whenNotToUse: ['Quads must be created; use quadrangulate.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor }, required: ['targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'triangulate', targetActor: 'DM_A' }, exampleOutput: { success: true, message: 'Triangulate complete' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'poke', plugins: PLUGIN,
    family: F, summary: 'Poke faces of a dynamic mesh (create center vertices).', whenToUse: ['Face centers must be poked.'], whenNotToUse: ['A loop cut is needed; use loop_cut.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor }, required: ['targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'poke', targetActor: 'DM_A' }, exampleOutput: { success: true, message: 'Poke complete' },
    normalizationRationale: NR,
  }),
];
