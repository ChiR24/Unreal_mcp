/**
 * Geometry boolean/extract/edge family records (21 actions).
 *
 * Grounded in manage-geometry-tool.ts boolean/extrude/bevel/bridge/loft/sweep/
 * duplicate/loop/edge actions and native Geometry dispatch. All require the
 * GeometryScripting plugin. Boolean operations take targetActor/toolActor; some
 * consume a tool actor that may be kept or discarded (keepTool flag).
 */
import type { CapabilityRecordSource } from '../../index.js';
import { buildWorldRecord } from './builder.js';
import { P } from './properties.js';

const F = 'operations';
const NR = 'Distinct manage_geometry operation verb and target; no cross-tool duplicate.';
const PLUGIN = ['GeometryScripting'] as const;

export const GEOMETRY_OPERATIONS_RECORDS: readonly CapabilityRecordSource[] = [
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'boolean_union', plugins: PLUGIN,
    family: F, summary: 'Boolean union of two dynamic mesh actors.', whenToUse: ['Two meshes must be merged via union.'], whenNotToUse: ['A subtraction is needed; use boolean_subtract.'],
    inputProps: { targetActor: P.targetActor, toolActor: P.toolActor, keepTool: P.keepTool }, required: ['targetActor', 'toolActor'], effect: 'write', costLatency: 'interactive', costResources: 'medium',
    exampleInput: { action: 'boolean_union', targetActor: 'DM_A', toolActor: 'DM_B' }, exampleOutput: { success: true, message: 'Boolean union complete' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'boolean_subtract', plugins: PLUGIN,
    family: F, summary: 'Boolean subtract a tool mesh from a target mesh.', whenToUse: ['A tool mesh must be cut out of a target.'], whenNotToUse: ['A union is needed; use boolean_union.'],
    inputProps: { targetActor: P.targetActor, toolActor: P.toolActor, keepTool: P.keepTool, keepInside: P.keepInside }, required: ['targetActor', 'toolActor'], effect: 'write', costLatency: 'interactive', costResources: 'medium',
    exampleInput: { action: 'boolean_subtract', targetActor: 'DM_A', toolActor: 'DM_B' }, exampleOutput: { success: true, message: 'Boolean subtract complete' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'boolean_intersection', plugins: PLUGIN,
    family: F, summary: 'Boolean intersection of two dynamic mesh actors.', whenToUse: ['The overlapping volume of two meshes is needed.'], whenNotToUse: ['A union is needed; use boolean_union.'],
    inputProps: { targetActor: P.targetActor, toolActor: P.toolActor, keepTool: P.keepTool }, required: ['targetActor', 'toolActor'], effect: 'write', costLatency: 'interactive', costResources: 'medium',
    exampleInput: { action: 'boolean_intersection', targetActor: 'DM_A', toolActor: 'DM_B' }, exampleOutput: { success: true, message: 'Boolean intersection complete' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'boolean_trim', plugins: PLUGIN,
    family: F, summary: 'Trim a target mesh by a trim actor (keep inside/outside).', whenToUse: ['A mesh must be trimmed against a volume.'], whenNotToUse: ['A full boolean is needed; use boolean_subtract.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, trimActorName: P.trimActorName, keepInside: P.keepInside }, required: ['targetActor', 'trimActorName'], effect: 'write', costLatency: 'interactive', costResources: 'medium',
    exampleInput: { action: 'boolean_trim', targetActor: 'DM_A', trimActorName: 'Trim_01' }, exampleOutput: { success: true, message: 'Boolean trim complete' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'self_union', plugins: PLUGIN,
    family: F, summary: 'Self-union overlapping triangles of a dynamic mesh.', whenToUse: ['A self-intersecting mesh must be resolved.'], whenNotToUse: ['A boolean against another mesh is needed.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor }, required: ['targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'self_union', targetActor: 'DM_A' }, exampleOutput: { success: true, message: 'Self-union complete' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'extrude', plugins: PLUGIN,
    family: F, summary: 'Extrude selected faces of a dynamic mesh.', whenToUse: ['Faces must be extruded along a direction.'], whenNotToUse: ['A sweep along a spline is needed; use extrude_along_spline.'],
    inputProps: { actorName: P.actorName, amount: P.amount, targetActor: P.targetActor, offset: P.offset, triangleIndices: P.triangleIndices }, required: ['targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'extrude', targetActor: 'DM_A', offset: { x: 0, y: 0, z: 50 } }, exampleOutput: { success: true, message: 'Extrude complete' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'inset', plugins: PLUGIN,
    family: F, summary: 'Inset selected faces of a dynamic mesh.', whenToUse: ['Faces must be inset.'], whenNotToUse: ['Faces must be outset; use outset.'],
    inputProps: { actorName: P.actorName, distance: P.distance, targetActor: P.targetActor, triangleIndices: P.triangleIndices }, required: ['targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'inset', targetActor: 'DM_A' }, exampleOutput: { success: true, message: 'Inset complete' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'outset', plugins: PLUGIN,
    family: F, summary: 'Outset selected faces of a dynamic mesh.', whenToUse: ['Faces must be outset.'], whenNotToUse: ['Faces must be inset; use inset.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, triangleIndices: P.triangleIndices }, required: ['targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'outset', targetActor: 'DM_A' }, exampleOutput: { success: true, message: 'Outset complete' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'bevel', plugins: PLUGIN,
    family: F, summary: 'Bevel selected edges of a dynamic mesh.', whenToUse: ['Edges must be beveled.'], whenNotToUse: ['Edges must be split; use edge_split.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, triangleIndices: P.triangleIndices }, required: ['targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'bevel', targetActor: 'DM_A' }, exampleOutput: { success: true, message: 'Bevel complete' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'offset_faces', plugins: PLUGIN,
    family: F, summary: 'Offset selected faces of a dynamic mesh.', whenToUse: ['Faces must be offset.'], whenNotToUse: ['Faces must be extruded; use extrude.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, offset: P.offset, triangleIndices: P.triangleIndices }, required: ['targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'offset_faces', targetActor: 'DM_A', offset: { x: 0, y: 0, z: 10 } }, exampleOutput: { success: true, message: 'Offset faces complete' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'shell', plugins: PLUGIN,
    family: F, summary: 'Create a shell (inner offset) of a dynamic mesh.', whenToUse: ['A hollow shell must be generated.'], whenNotToUse: ['A simple offset is needed; use offset_faces.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, thickness: P.thickness }, required: ['targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'shell', targetActor: 'DM_A', thickness: 5 }, exampleOutput: { success: true, message: 'Shell complete' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'revolve', plugins: PLUGIN,
    family: F, summary: 'Revolve a profile curve into a solid mesh.', whenToUse: ['A lathe/revolve solid must be created.'], whenNotToUse: ['A sweep along a spline is needed; use sweep.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, steps: P.steps }, required: ['targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'revolve', targetActor: 'DM_A', steps: 32 }, exampleOutput: { success: true, message: 'Revolve complete' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'chamfer', plugins: PLUGIN,
    family: F, summary: 'Chamfer selected edges of a dynamic mesh.', whenToUse: ['Edges must be chamfered.'], whenNotToUse: ['Edges must be beveled; use bevel.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, triangleIndices: P.triangleIndices }, required: ['targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'chamfer', targetActor: 'DM_A' }, exampleOutput: { success: true, message: 'Chamfer complete' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'extrude_along_spline', plugins: PLUGIN,
    family: F, summary: 'Extrude a profile along a spline actor.', whenToUse: ['A mesh must be swept along a spline path.'], whenNotToUse: ['A straight extrude is needed; use extrude.'],
    inputProps: { actorName: P.actorName, cap: P.cap, segments: P.segments, targetActor: P.targetActor, splineActorName: P.splineActorName }, required: ['targetActor', 'splineActorName'], effect: 'write', costLatency: 'interactive', costResources: 'medium',
    exampleInput: { action: 'extrude_along_spline', targetActor: 'DM_A', splineActorName: 'Spline_01' }, exampleOutput: { success: true, message: 'Extrude along spline complete' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'bridge', plugins: PLUGIN,
    family: F, summary: 'Bridge two edge loops of a dynamic mesh.', whenToUse: ['Two edge loops must be bridged.'], whenNotToUse: ['A loft between profiles is needed; use loft.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor }, required: ['targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'bridge', targetActor: 'DM_A' }, exampleOutput: { success: true, message: 'Bridge complete' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'loft', plugins: PLUGIN,
    family: F, summary: 'Loft between two or more profile curves.', whenToUse: ['A lofted surface between profiles must be created.'], whenNotToUse: ['A sweep along a spline is needed; use sweep.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor }, required: ['targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'medium',
    exampleInput: { action: 'loft', targetActor: 'DM_A' }, exampleOutput: { success: true, message: 'Loft complete' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'sweep', plugins: PLUGIN,
    family: F, summary: 'Sweep a profile along a path of a dynamic mesh.', whenToUse: ['A swept solid must be created.'], whenNotToUse: ['A revolve is needed; use revolve.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, splineActorName: P.splineActorName }, required: ['targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'medium',
    exampleInput: { action: 'sweep', targetActor: 'DM_A' }, exampleOutput: { success: true, message: 'Sweep complete' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'duplicate_along_spline', plugins: PLUGIN,
    family: F, summary: 'Duplicate a mesh along a spline actor.', whenToUse: ['Copies of a mesh must be distributed along a spline.'], whenNotToUse: ['A single extrude is needed; use extrude_along_spline.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, splineActorName: P.splineActorName, count: P.count }, required: ['targetActor', 'splineActorName'], effect: 'write', costLatency: 'interactive', costResources: 'medium',
    exampleInput: { action: 'duplicate_along_spline', targetActor: 'DM_A', splineActorName: 'Spline_01', count: 10 }, exampleOutput: { success: true, message: 'Duplicate along spline complete' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'loop_cut', plugins: PLUGIN,
    family: F, summary: 'Insert a loop cut into a dynamic mesh.', whenToUse: ['An edge loop must be inserted.'], whenNotToUse: ['An edge must be split; use edge_split.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor }, required: ['targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'loop_cut', targetActor: 'DM_A' }, exampleOutput: { success: true, message: 'Loop cut complete' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'edge_split', plugins: PLUGIN,
    family: F, summary: 'Split selected edges of a dynamic mesh.', whenToUse: ['Edges must be split into separate vertices.'], whenNotToUse: ['An edge loop must be inserted; use loop_cut.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor }, required: ['targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'edge_split', targetActor: 'DM_A' }, exampleOutput: { success: true, message: 'Edge split complete' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'quadrangulate', plugins: PLUGIN,
    family: F, summary: 'Quadrangulate triangles of a dynamic mesh.', whenToUse: ['Triangles must be converted to quads.'], whenNotToUse: ['Triangles must be created; use triangulate.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor }, required: ['targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'quadrangulate', targetActor: 'DM_A' }, exampleOutput: { success: true, message: 'Quadrangulate complete' },
    normalizationRationale: NR,
  }),
];
