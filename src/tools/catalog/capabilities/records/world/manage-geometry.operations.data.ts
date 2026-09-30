/**
 * Geometry boolean/extract/edge family records (21 actions).
 *
 * Grounded in manage-geometry-tool.ts boolean/extrude/bevel/bridge/loft/sweep/
 * duplicate/loop/edge actions and native Geometry dispatch. All require the
 * GeometryScripting plugin. Boolean operations take targetActor/toolActor; some
 * consume a tool actor that may be kept or discarded (keepTool flag).
 */
import type { CapabilityRecordSource } from '../../model.js';
import { buildWorldRecord } from './builder.js';
import { P } from './properties.js';

const F = 'operations';
const PLUGIN = ['GeometryScripting'] as const;
const BEVEL_DISTANCE = { type: 'number', description: 'Bevel width in world units (default 5).' };
const BEVEL_SEGMENTS = { type: 'integer', description: 'Subdivisions for a rounded bevel (UE 5.4 or later); omit or 0 for a flat bevel.' };
const SWEEP_STEPS = { type: 'integer', description: 'Path steps along the sweep (default 16); the profile uses half as many sides.' };
// A fold keeps the first member's description, so text shared by sweep and revolve names both.
const STEPS = { type: 'integer', description: 'Path steps along a sweep (default 16; the profile uses half as many sides), or segments around the axis for a revolve (default 16, 3 to 512; 48 or more reads as round).' };
const CAP = { type: 'boolean', description: 'Close open ends: the tube ends of a loft, sweep or extrude along a spline, or for a revolve flat discs from the first and last profile points to the axis (revolve default true).' };

export const GEOMETRY_OPERATIONS_RECORDS: readonly CapabilityRecordSource[] = [
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'boolean_union', plugins: PLUGIN,
    family: F, summary: 'Boolean union of two dynamic mesh actors.', whenToUse: ['Two meshes must be merged via union.'], whenNotToUse: ['A subtraction is needed; use boolean_subtract.'],
    inputProps: { targetActor: P.targetActor, toolActor: P.toolActor, keepTool: P.keepTool }, required: ['targetActor', 'toolActor'], effect: 'write', costLatency: 'interactive', costResources: 'medium',
    exampleInput: { action: 'boolean_union', targetActor: 'DM_A', toolActor: 'DM_B' },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'boolean_subtract', plugins: PLUGIN,
    family: F, summary: 'Boolean subtract a tool mesh from a target mesh.', whenToUse: ['A tool mesh must be cut out of a target.'], whenNotToUse: ['A union is needed; use boolean_union.'],
    inputProps: { targetActor: P.targetActor, toolActor: P.toolActor, keepTool: P.keepTool }, required: ['targetActor', 'toolActor'], effect: 'write', costLatency: 'interactive', costResources: 'medium',
    exampleInput: { action: 'boolean_subtract', targetActor: 'DM_A', toolActor: 'DM_B' },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'boolean_intersection', plugins: PLUGIN,
    family: F, summary: 'Boolean intersection of two dynamic mesh actors.', whenToUse: ['The overlapping volume of two meshes is needed.'], whenNotToUse: ['A union is needed; use boolean_union.'],
    inputProps: { targetActor: P.targetActor, toolActor: P.toolActor, keepTool: P.keepTool }, required: ['targetActor', 'toolActor'], effect: 'write', costLatency: 'interactive', costResources: 'medium',
    exampleInput: { action: 'boolean_intersection', targetActor: 'DM_A', toolActor: 'DM_B' },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'boolean_trim', plugins: PLUGIN,
    family: F, summary: 'Trim a target mesh by a trim actor (keep inside/outside).', whenToUse: ['A mesh must be trimmed against a volume.'], whenNotToUse: ['A full boolean is needed; use boolean_subtract.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, trimActorName: P.trimActorName, keepInside: P.keepInside }, required: ['trimActorName'], requiredOneOf: ['actorName', 'targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'medium',
    exampleInput: { action: 'boolean_trim', targetActor: 'DM_A', trimActorName: 'Trim_01' },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'self_union', plugins: PLUGIN,
    family: F, summary: 'Self-union overlapping triangles of a dynamic mesh.', whenToUse: ['A self-intersecting mesh must be resolved.'], whenNotToUse: ['A boolean against another mesh is needed.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor }, required: [], requiredOneOf: ['actorName', 'targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'self_union', targetActor: 'DM_A' },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'extrude', plugins: PLUGIN,
    family: F, summary: 'Extrude selected faces of a dynamic mesh.', whenToUse: ['Faces must be extruded along a direction.'], whenNotToUse: ['A sweep along a spline is needed; use extrude_along_spline.'],
    inputProps: { actorName: P.actorName, amount: P.amount, targetActor: P.targetActor, offset: P.offset, triangleIndices: P.triangleIndices }, required: [], requiredOneOf: ['actorName', 'targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'extrude', targetActor: 'DM_A', offset: { x: 0, y: 0, z: 50 } },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'inset', plugins: PLUGIN,
    family: F, summary: 'Inset selected faces of a dynamic mesh.', whenToUse: ['Faces must be inset.'], whenNotToUse: ['Faces must be outset; use outset.'],
    inputProps: { actorName: P.actorName, distance: P.distance, targetActor: P.targetActor, triangleIndices: P.triangleIndices }, required: [], requiredOneOf: ['actorName', 'targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'inset', targetActor: 'DM_A' },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'outset', plugins: PLUGIN,
    family: F, summary: 'Outset selected faces of a dynamic mesh.', whenToUse: ['Faces must be outset.'], whenNotToUse: ['Faces must be inset; use inset.'],
    inputProps: { actorName: P.actorName, distance: P.distance, targetActor: P.targetActor, triangleIndices: P.triangleIndices }, required: [], requiredOneOf: ['actorName', 'targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'outset', targetActor: 'DM_A' },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'bevel', plugins: PLUGIN,
    family: F, summary: 'Bevel selected edges of a dynamic mesh.', whenToUse: ['Edges must be beveled.'], whenNotToUse: ['Edges must be split; use edge_split.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, triangleIndices: P.triangleIndices, distance: BEVEL_DISTANCE, segments: BEVEL_SEGMENTS }, required: [], requiredOneOf: ['actorName', 'targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'bevel', targetActor: 'DM_A' },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'offset_faces', plugins: PLUGIN,
    family: F, summary: 'Offset selected faces of a dynamic mesh.', whenToUse: ['Faces must be offset.'], whenNotToUse: ['Faces must be extruded; use extrude.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, distance: P.distance, triangleIndices: P.triangleIndices }, required: [], requiredOneOf: ['actorName', 'targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'offset_faces', targetActor: 'DM_A', distance: 10 },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'shell', plugins: PLUGIN,
    family: F, summary: 'Create a shell (inner offset) of a dynamic mesh.', whenToUse: ['A hollow shell must be generated.'], whenNotToUse: ['A simple offset is needed; use offset_faces.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, thickness: P.thickness }, required: [], requiredOneOf: ['actorName', 'targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'shell', targetActor: 'DM_A', thickness: 5 },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'revolve', plugins: PLUGIN,
    family: F, summary: 'Turn a profile of {radius, height} points around the vertical axis into a new dynamic mesh actor: a vase, column, bottle, tower or dome.', whenToUse: ['A lathe/revolve solid must be created.'], whenNotToUse: ['A sweep along a spline is needed; use sweep.'],
    inputProps: {
      name: P.name, location: P.location, rotation: P.rotation, scale: P.scale,
      profile: {
        type: 'array',
        items: {
          type: 'object',
          properties: { radius: { type: 'number', description: 'Distance from the axis in cm, 0 or more.' }, height: { type: 'number', description: 'Height above the base in cm.' } },
          required: ['radius', 'height'], additionalProperties: false,
        },
        description: 'Points from bottom to top, at least 2. For a hollow shape with walls, go up the outside and back down the inside. Omit for a small built-in vase.',
      },
      angle: { type: 'number', description: 'Degrees to turn the profile, 1 to 360 (default 360); less leaves an open wedge.' },
      steps: STEPS, cap: CAP,
    },
    required: [], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'revolve', name: 'Column', profile: [{ radius: 40, height: 0 }, { radius: 30, height: 300 }, { radius: 45, height: 320 }], steps: 32 },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'chamfer', plugins: PLUGIN,
    family: F, summary: 'Chamfer selected edges of a dynamic mesh.', whenToUse: ['Edges must be chamfered.'], whenNotToUse: ['Edges must be beveled; use bevel.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, triangleIndices: P.triangleIndices, distance: BEVEL_DISTANCE, segments: BEVEL_SEGMENTS }, required: [], requiredOneOf: ['actorName', 'targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'chamfer', targetActor: 'DM_A' },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'extrude_along_spline', plugins: PLUGIN,
    family: F, summary: 'Extrude a profile along a spline actor.', whenToUse: ['A mesh must be swept along a spline path.'], whenNotToUse: ['A straight extrude is needed; use extrude.'],
    inputProps: { actorName: P.actorName, cap: CAP, segments: P.segments, targetActor: P.targetActor, splineActorName: P.splineActorName }, required: ['splineActorName'], requiredOneOf: ['actorName', 'targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'medium',
    exampleInput: { action: 'extrude_along_spline', targetActor: 'DM_A', splineActorName: 'Spline_01' },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'bridge', plugins: PLUGIN,
    family: F, summary: 'Bridge two edge loops of a dynamic mesh.', whenToUse: ['Two edge loops must be bridged.'], whenNotToUse: ['A loft between profiles is needed; use loft.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor }, required: [], requiredOneOf: ['actorName', 'targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'bridge', targetActor: 'DM_A' },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'loft', plugins: PLUGIN,
    family: F, summary: 'Loft between two or more profile curves.', whenToUse: ['A lofted surface between profiles must be created.'], whenNotToUse: ['A sweep along a spline is needed; use sweep.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, splineActorName: P.splineActorName, segments: SWEEP_STEPS, cap: CAP }, required: [], requiredOneOf: ['actorName', 'targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'medium',
    exampleInput: { action: 'loft', targetActor: 'DM_A' },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'sweep', plugins: PLUGIN,
    family: F, summary: 'Sweep a profile along a path of a dynamic mesh.', whenToUse: ['A swept solid must be created.'], whenNotToUse: ['A revolve is needed; use revolve.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, splineActorName: P.splineActorName, steps: STEPS, cap: CAP }, required: [], requiredOneOf: ['actorName', 'targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'medium',
    exampleInput: { action: 'sweep', targetActor: 'DM_A' },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'duplicate_along_spline', plugins: PLUGIN,
    family: F, summary: 'Duplicate a mesh along a spline actor.', whenToUse: ['Copies of a mesh must be distributed along a spline.'], whenNotToUse: ['A single extrude is needed; use extrude_along_spline.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, splineActorName: P.splineActorName, count: P.count }, required: ['splineActorName'], requiredOneOf: ['actorName', 'targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'medium',
    exampleInput: { action: 'duplicate_along_spline', targetActor: 'DM_A', splineActorName: 'Spline_01', count: 10 },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'loop_cut', plugins: PLUGIN,
    family: F, summary: 'Insert evenly spaced edge loops across a dynamic mesh, perpendicular to an axis.', whenToUse: ['An edge loop must be inserted.'], whenNotToUse: ['An edge must be split; use edge_split.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, axis: P.axis, numCuts: P.numCuts }, required: [], requiredOneOf: ['actorName', 'targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'loop_cut', targetActor: 'DM_A', axis: 'Z', numCuts: 2 },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'edge_split', plugins: PLUGIN,
    family: F, summary: 'Split edges of a dynamic mesh at their midpoints.', whenToUse: ['Specific edges must be subdivided.'], whenNotToUse: ['An edge loop must be inserted; use loop_cut.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, edges: P.edges }, required: ['edges'], requiredOneOf: ['actorName', 'targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'edge_split', targetActor: 'DM_A', edges: [0, 1] },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'poke', plugins: PLUGIN,
    family: F, summary: 'Poke faces of a dynamic mesh: add a centre vertex to each triangle (optionally pushed out along its normal) and fan it into three triangles.',
    whenToUse: ['Faces must be split from their centre, or pushed out into pyramids or spikes.'], whenNotToUse: ['The whole mesh should be refined evenly; use subdivide.'],
    inputProps: {
      actorName: P.actorName, targetActor: P.targetActor, triangleIndices: P.triangleIndices,
      distance: { type: 'number', description: 'Distance in cm to push each new centre vertex along its face normal (default 0; negative pushes inward).' },
    },
    required: [], requiredOneOf: ['actorName', 'targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    outputProps: {
      trianglesPoked: { type: 'number', description: 'Triangles that were poked.' },
      trianglesBefore: { type: 'number', description: 'Triangle count before.' },
      trianglesAfter: { type: 'number', description: 'Triangle count after (before + 2 per poked triangle).' },
      verticesAdded: { type: 'number', description: 'New centre vertices.' },
      skipped: { type: 'number', description: 'Degenerate or missing triangles that were left alone.' },
    },
    exampleInput: { action: 'poke', targetActor: 'DM_A', distance: 10 },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'quadrangulate', plugins: PLUGIN,
    family: F, summary: 'Group pairs of adjacent triangles of a dynamic mesh into quad PolyGroups, so PolyGroup edits (extrude, inset, bevel by group) act on quads. The mesh itself stays triangles.',
    whenToUse: ['A triangulated mesh must be edited as quads.'], whenNotToUse: ['Real quad topology is needed in an exported asset; meshes in Unreal are always triangles.'],
    inputProps: {
      actorName: P.actorName, targetActor: P.targetActor,
      respectUVSeams: { type: 'boolean', description: 'Never pair two triangles across a UV seam (default true).' },
      respectHardNormals: { type: 'boolean', description: 'Never pair two triangles across a hard edge (default false).' },
    },
    required: [], requiredOneOf: ['actorName', 'targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    outputProps: {
      quadsFormed: { type: 'number', description: 'Triangle pairs grouped as quads.' },
      singleTriangleGroups: { type: 'number', description: 'Triangles left in a group of their own.' },
      largerGroups: { type: 'number', description: 'Groups of more than two triangles.' },
      triangleCount: { type: 'number', description: 'Triangles in the mesh (unchanged).' },
    },
    exampleInput: { action: 'quadrangulate', targetActor: 'DM_A' },
  }),
];
