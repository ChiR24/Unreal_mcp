/**
 * Geometry mirror/array/optimization/UV/normals/collision/Nanite family records
 * (29 actions).
 *
 * Grounded in manage-geometry-tool.ts (mirror/array_linear/array_radial,
 * simplify_mesh/subdivide/remesh_uniform/remesh_voxel/merge/weld/fill/remove,
 * auto_uv/project_uv/transform_uvs/unwrap_uv/pack_uv_islands,
 * recalculate/flip/recompute, generate_collision/complex/simplify,
 * generate_lods/set_lod_settings/set_lod_screen_sizes/convert_to_nanite/
 * convert_to_static_mesh, get_mesh_info) and native Geometry dispatch. All
 * require the GeometryScripting plugin. convert_to_static_mesh and
 * convert_to_nanite are the two terminal bake steps: both create a new
 * StaticMesh asset from a DynamicMesh actor, differing only in whether
 * Nanite is enabled on the result (see GeometryAssetConversion.cpp).
 */
import type { CapabilityRecordSource } from '../../model.js';
import { buildWorldRecord } from './builder.js';
import { P } from './properties.js';

const F = 'optimize';
const PLUGIN = ['GeometryScripting'] as const;
// remesh targets a triangle count when no targetEdgeLength is given.
const REMESH_TRIS = { type: 'number', description: 'Triangle budget when targetEdgeLength is omitted (remesh_uniform default 5000). remesh_voxel only approximates it, picking the grid cell from the surface area, and uses voxelCount first when that is given.' };
// A fold keeps the first member's text, so this one describes the grid for both remesh_voxel and morphology.
const VOXEL_COUNT = { type: 'integer', description: 'Grid cells along the mesh\'s longest side for the voxel operations (default 128, clamped to 16-256): more cells keep thinner detail and cost more time and triangles. The reply\'s voxelSize is the resulting cell width in cm.' };
const MORPH_OPERATION = { type: 'string', enum: ['dilate', 'contract', 'close', 'open'], description: 'dilate grows the surface outward by distance, contract shrinks it inward; close (the default) fills creases and rounds the seams where parts meet, a fillet up to about twice distance wide; open shaves off bumps and thin parts smaller than distance.' };
const MORPH_DISTANCE = { type: 'number', description: 'Offset distance in cm (default 2% of the mesh\'s longest side). Keep it above one voxel (the longest side divided by voxelCount) or the effect barely registers.' };
const SUBDIVIDE_SCHEME = { type: 'string', enum: ['pn', 'catmull_clark', 'loop', 'bilinear'], description: 'Subdivision scheme (default pn). pn: PN tessellation, which adds triangles and keeps the shape. catmull_clark: a smooth subdivision surface over a polygon cage, where every polygroup is one face (build the cage with edit_dynamic_mesh append_polygons, or start from create_box, which has one polygroup per face); the surface rounds toward the cage, and polygroups and material ids carry through. loop: smooths the triangles as they are and needs no cage. bilinear: cuts the cage into quads without smoothing. iterations is the level.' };
const SUBDIVIDE_LEVEL = { type: 'integer', description: 'Subdivision level, 1 to 6 (default 1). Each level quadruples the face count, so 2 or 3 is enough for a smooth result; the call is refused past 500000 triangles.' };
const CONVERT_MATERIALS = { type: 'array', items: { type: 'string' }, description: 'Materials for the baked asset\'s slots as asset paths (a material or material instance): entry i is the material of slot i, which holds every triangle with material id i (set ids with edit_dynamic_mesh set_material_id). The asset gets the mesh\'s highest material id + 1 slots; slots you do not list, or list as "", keep the default material. A path that is unsafe or does not load as a material is refused before anything is created, and so is a list longer than the slot count.' };
const SLOT = {
  type: 'object',
  properties: {
    slot: { type: 'integer', description: 'Slot index, equal to the material id it holds.' },
    name: { type: 'string', description: 'Slot name.' },
    material: { type: 'string', description: 'Material asset path; empty for the default material.' },
  },
  additionalProperties: false,
};
// A mesh replaced in place reshapes every Blueprint part that draws it: a body grown 4 cm buries a rider's legs.
const MESH_USER_WARNINGS = { type: 'array', items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true }, 'x-unreal-reflection-boundary': true, description: 'When outputPath replaced a mesh that loaded Blueprints already draw: the parts drawing it that now sink, worst first (at most 8, each also a warnings[] sentence): blueprintPath, componentName, kind (buried: inside otherComponent; sunk: below a Character\'s capsule bottom), depth in cm, insideShare and issue. Measured like edit_scs partWarnings; a part tagged mcp.placement.ok is left out, and mcp.placement.ok:<component> accepts the embed in that one part only.' };
const CONVERT_COLLISION ={ type: 'string', enum: ['box', 'complex', 'none'], description: 'Collision for the baked asset: box (default) is the bounds as one convex hull, which pawns can stand on; complex uses the render triangles as simple collision too, exact but costly; none gives the asset no collision at all.' };
const COLLISION_TYPE = { type: 'string', enum: ['box', 'sphere', 'capsule', 'convex', 'convex_decomposition'], description: 'Collision shapes to generate (default convex).' };

export const GEOMETRY_OPTIMIZE_RECORDS: readonly CapabilityRecordSource[] = [
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'mirror', topics: ['mirror a mesh'], plugins: PLUGIN,
    family: F, summary: 'Mirror a dynamic mesh across an axis.', whenToUse: ['A mesh must be mirrored.'], whenNotToUse: ['Instances must be arrayed; use array_linear.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, axis: P.axis, center: P.center }, required: ['axis'], requiredOneOf: ['actorName', 'targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'mirror', targetActor: 'DM_A', axis: 'X' },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'array_linear', plugins: PLUGIN,
    family: F, summary: 'Create a linear array of a dynamic mesh.', whenToUse: ['Instances must be arrayed linearly.'], whenNotToUse: ['A radial array is needed; use array_radial.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, count: P.count, offset: P.offset }, required: ['count'], requiredOneOf: ['actorName', 'targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'medium',
    exampleInput: { action: 'array_linear', targetActor: 'DM_A', count: 5, offset: { x: 200, y: 0, z: 0 } },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'array_radial', plugins: PLUGIN,
    family: F, summary: 'Create a radial array of a dynamic mesh.', whenToUse: ['Instances must be arrayed radially.'], whenNotToUse: ['A linear array is needed; use array_linear.'],
    inputProps: { actorName: P.actorName, angle: P.angle, targetActor: P.targetActor, count: P.count, center: P.center }, required: ['count'], requiredOneOf: ['actorName', 'targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'medium',
    exampleInput: { action: 'array_radial', actorName: 'DM_A', count: 8, center: { x: 0, y: 0, z: 0 } },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'simplify_mesh', plugins: PLUGIN,
    family: F, summary: 'Simplify a dynamic mesh to a target triangle count.', whenToUse: ['A mesh must be simplified.'], whenNotToUse: ['A mesh must be subdivided; use subdivide.'],
    inputProps: { actorName: P.actorName, reductionPercent: P.reductionPercent, targetActor: P.targetActor, targetTriangleCount: P.targetTriangleCount }, required: [], requiredOneOf: ['actorName', 'targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'medium',
    exampleInput: { action: 'simplify_mesh', targetActor: 'DM_A', targetTriangleCount: 5000 },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'subdivide', plugins: PLUGIN,
    topics: ['catmull clark', 'loop subdivision', 'subdivision surface', 'smooth subdivision', 'round the edges of a cage'],
    family: F, summary: 'Subdivide a dynamic mesh: pn tessellation (default), or a Catmull-Clark, Loop or bilinear subdivision surface.',
    whenToUse: ['A mesh must be refined. For a smooth, rounded model, author a coarse polygon cage (edit_dynamic_mesh append_polygons, one polygroup per face, or create_box) and subdivide it with scheme catmull_clark, iterations 2 or 3.',
      'The cage carries material ids and polygroups through the subdivision, so assign them with set_material_id first.'],
    whenNotToUse: ['A mesh must be simplified; use simplify_mesh.', 'Edges must stay sharp; subdivision rounds every edge of the cage.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, iterations: SUBDIVIDE_LEVEL, scheme: SUBDIVIDE_SCHEME }, required: [], requiredOneOf: ['actorName', 'targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'medium',
    outputProps: {
      scheme: { type: 'string', description: 'Scheme that ran.' },
      level: { type: 'integer', description: 'Subdivision level that ran.' },
      trianglesBefore: { type: 'integer', description: 'Triangle count before.' },
      trianglesAfter: { type: 'integer', description: 'Triangle count after.' },
      cageFaces: { type: 'integer', description: 'Cage faces: polygroups for catmull_clark and bilinear, triangles for loop; absent for pn.' },
    },
    exampleInput: { action: 'subdivide', targetActor: 'DM_Cage', scheme: 'catmull_clark', iterations: 2 },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'remesh_uniform', plugins: PLUGIN,
    family: F, summary: 'Uniformly remesh a dynamic mesh to a target edge length.', whenToUse: ['A uniform remesh is needed.'], whenNotToUse: ['A voxel remesh is needed; use remesh_voxel.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, targetEdgeLength: P.targetEdgeLength, targetTriangleCount: REMESH_TRIS }, required: [], requiredOneOf: ['actorName', 'targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'medium',
    exampleInput: { action: 'remesh_uniform', targetActor: 'DM_A', targetEdgeLength: 10 },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'merge_vertices', plugins: PLUGIN,
    family: F, summary: 'Merge coincident vertices of a dynamic mesh.', whenToUse: ['Duplicate vertices must be welded/merged.'], whenNotToUse: ['A precise weld is needed; use weld_vertices.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, weldDistance: P.weldDistance }, required: [], requiredOneOf: ['actorName', 'targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'merge_vertices', targetActor: 'DM_A' },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'remesh_voxel', plugins: PLUGIN,
    family: F, summary: 'Voxel-wrap a dynamic mesh into one watertight, smooth-shaded surface (the engine\'s solidify): the mesh is sampled into a grid and rebuilt, closing holes and merging overlapping parts.',
    whenToUse: ['A watertight voxel remesh is needed, for example to fuse unioned or overlapping parts into one skin. The grid cell is targetEdgeLength, or the longest side divided by voxelCount (default 128). The surface is rebuilt, so UVs, material ids, polygroups and vertex colours are dropped: run auto_uv, set_material_id and bake_vertex_colors afterwards.'],
    whenNotToUse: ['A uniform remesh is needed; use remesh_uniform.', 'Seams between parts must be rounded rather than fused; use morphology close.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, targetEdgeLength: P.targetEdgeLength, targetTriangleCount: REMESH_TRIS, voxelCount: VOXEL_COUNT }, required: [], requiredOneOf: ['actorName', 'targetActor'], effect: 'write', behavior: { longRunning: true }, costLatency: 'long-running', costResources: 'high',
    exampleInput: { action: 'remesh_voxel', targetActor: 'DM_A', voxelCount: 96 },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'morphology', plugins: PLUGIN,
    topics: ['fillet', 'blend parts together', 'round the seams', 'dilate', 'contract', 'offset a mesh', 'close gaps in a mesh'],
    family: F, summary: 'Offset a dynamic mesh\'s surface through a voxel grid: dilate, contract, close (fillet the seams where unioned parts meet) or open.',
    whenToUse: ['Joined parts need a smooth blend: boolean_union them, then morphology close with a distance about the fillet radius, then auto_uv, set_material_id and bake_vertex_colors.',
      'A mesh must grow or shrink evenly (dilate, contract), or small bumps must go (open).'],
    whenNotToUse: ['Edges must stay crisp; the grid rounds every feature narrower than about twice distance.', 'UVs, material ids, polygroups or vertex colours must survive; the surface is rebuilt from the grid, so set them afterwards.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, operation: MORPH_OPERATION, distance: MORPH_DISTANCE, voxelCount: VOXEL_COUNT }, required: [], requiredOneOf: ['actorName', 'targetActor'], effect: 'write', behavior: { longRunning: true }, costLatency: 'long-running', costResources: 'high',
    outputProps: {
      operation: { type: 'string', description: 'Operation that ran.' },
      distance: { type: 'number', description: 'Offset distance used, in cm.' },
      voxelCount: { type: 'integer', description: 'Grid cells along the longest side after clamping.' },
      voxelSize: { type: 'number', description: 'Width of one grid cell in cm.' },
      trianglesBefore: { type: 'integer', description: 'Triangle count before.' },
      trianglesAfter: { type: 'integer', description: 'Triangle count after.' },
      note: { type: 'string', description: 'What the rebuild dropped, and a hint when distance is under one voxel.' },
    },
    exampleInput: { action: 'morphology', targetActor: 'DM_A', operation: 'close', distance: 4, voxelCount: 128 },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'weld_vertices', plugins: PLUGIN,
    family: F, summary: 'Weld vertices of a dynamic mesh within a distance.', whenToUse: ['Vertices must be welded by threshold.'], whenNotToUse: ['A merge is needed; use merge_vertices.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, weldDistance: P.weldDistance }, required: [], requiredOneOf: ['actorName', 'targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'weld_vertices', targetActor: 'DM_A', weldDistance: 0.1 },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'fill_holes', plugins: PLUGIN,
    family: F, summary: 'Fill boundary holes of a dynamic mesh.', whenToUse: ['Mesh holes must be capped.'], whenNotToUse: ['Degenerate triangles must be removed; use remove_degenerates.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor }, required: [], requiredOneOf: ['actorName', 'targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'fill_holes', targetActor: 'DM_A' },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'remove_degenerates', plugins: PLUGIN,
    family: F, summary: 'Remove degenerate triangles from a dynamic mesh.', whenToUse: ['Degenerate triangles must be removed.'], whenNotToUse: ['Holes must be filled; use fill_holes.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor }, required: [], requiredOneOf: ['actorName', 'targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'remove_degenerates', targetActor: 'DM_A' },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'auto_uv', plugins: PLUGIN,
    family: F, summary: 'Auto-generate UVs for a dynamic mesh.', whenToUse: ['UVs must be auto-generated.'], whenNotToUse: ['UVs must be unwrapped; use unwrap_uv.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, uvChannel: P.uvChannel }, required: [], requiredOneOf: ['actorName', 'targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'auto_uv', targetActor: 'DM_A', uvChannel: 0 },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'project_uv', plugins: PLUGIN,
    family: F, summary: 'Project UVs from a planar/box projection.', whenToUse: ['UVs must be projected.'], whenNotToUse: ['UVs must be transformed; use transform_uvs.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, uvChannel: P.uvChannel }, required: [], requiredOneOf: ['actorName', 'targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'project_uv', targetActor: 'DM_A', uvChannel: 0 },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'transform_uvs', plugins: PLUGIN,
    family: F, summary: 'Transform existing UVs (scale/offset) of a dynamic mesh.', whenToUse: ['UVs must be scaled/offset.'], whenNotToUse: ['UVs must be repacked; use pack_uv_islands.'],
    inputProps: { actorName: P.actorName, rotation: P.uvRotation, targetActor: P.targetActor, uvChannel: P.uvChannel, uvScale: P.uvScale, uvOffset: P.uvOffset }, required: [], requiredOneOf: ['actorName', 'targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'transform_uvs', targetActor: 'DM_A', uvChannel: 0, uvScale: { u: 2, v: 2 } },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'unwrap_uv', plugins: PLUGIN,
    family: F, summary: 'Unwrap UVs of a dynamic mesh.', whenToUse: ['UVs must be unwrapped.'], whenNotToUse: ['UVs must be auto-generated; use auto_uv.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, uvChannel: P.uvChannel }, required: [], requiredOneOf: ['actorName', 'targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'unwrap_uv', targetActor: 'DM_A', uvChannel: 0 },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'pack_uv_islands', plugins: PLUGIN,
    family: F, summary: 'Pack UV islands of a dynamic mesh into a 0-1 square.', whenToUse: ['UV islands must be packed.'], whenNotToUse: ['UVs must be transformed; use transform_uvs.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, uvChannel: P.uvChannel }, required: [], requiredOneOf: ['actorName', 'targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'pack_uv_islands', targetActor: 'DM_A', uvChannel: 0 },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'recalculate_normals', plugins: PLUGIN,
    family: F, summary: 'Recalculate normals of a dynamic mesh.', whenToUse: ['Normals must be recalculated.'], whenNotToUse: ['Normals must be flipped; use flip_normals.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, hardEdgeAngle: P.hardEdgeAngle, computeWeightedNormals: P.computeWeightedNormals }, required: [], requiredOneOf: ['actorName', 'targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'recalculate_normals', actorName: 'DM_A', hardEdgeAngle: 60 },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'flip_normals', plugins: PLUGIN,
    family: F, summary: 'Flip normals of a dynamic mesh.', whenToUse: ['Normals must be flipped (inside-out).'], whenNotToUse: ['Normals must be recomputed; use recalculate_normals.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor }, required: [], requiredOneOf: ['actorName', 'targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'flip_normals', targetActor: 'DM_A' },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'recompute_tangents', plugins: PLUGIN,
    family: F, summary: 'Recompute tangents of a dynamic mesh.', whenToUse: ['Tangents must be recomputed for correct shading.'], whenNotToUse: ['Normals must be recomputed; use recalculate_normals.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor }, required: [], requiredOneOf: ['actorName', 'targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'recompute_tangents', targetActor: 'DM_A' },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'generate_collision', plugins: PLUGIN,
    family: F, summary: 'Generate simple collision for a dynamic mesh.', whenToUse: ['Simple collision must be generated.'], whenNotToUse: ['Complex collision is needed; use generate_complex_collision.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, collisionType: COLLISION_TYPE, maxHullCount: { type: 'integer', description: 'Hull budget (1 to 64, default 8) when collisionType is convex_decomposition.' } }, required: [], requiredOneOf: ['actorName', 'targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'generate_collision', targetActor: 'DM_A', collisionType: 'convex' },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'generate_complex_collision', plugins: PLUGIN,
    family: F, summary: 'Generate complex (convex decomposition) collision.', whenToUse: ['Complex collision must be generated.'], whenNotToUse: ['Simple collision is sufficient; use generate_collision.'],
    inputProps: { actorName: P.actorName, hullCount: P.hullCount, maxHullCount: P.maxHullCount, targetActor: P.targetActor }, required: [], requiredOneOf: ['actorName', 'targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'medium',
    exampleInput: { action: 'generate_complex_collision', targetActor: 'DM_A' },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'simplify_collision', plugins: PLUGIN,
    family: F, summary: 'Simplify an existing collision hull.', whenToUse: ['A collision hull must be simplified.'], whenNotToUse: ['Collision must be generated; use generate_collision.'],
    inputProps: { actorName: P.actorName, simplificationFactor: P.simplificationFactor, targetHullCount: P.targetHullCount, targetActor: P.targetActor }, required: [], requiredOneOf: ['actorName', 'targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'simplify_collision', targetActor: 'DM_A' },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'generate_lods', plugins: PLUGIN,
    family: F, summary: 'Generate LODs for a dynamic mesh.', whenToUse: ['LODs must be generated for a mesh.'], whenNotToUse: ['LOD screen sizes must be set; use set_lod_screen_sizes.'],
    inputProps: { actorName: P.actorName, lodCount: P.lodCount, targetActor: P.targetActor, outputPath: { ...P.outputPath, description: 'Where the baked static mesh (the LODs need one) is saved: a folder or a full asset path; default /Game/GeneratedMeshes/<actorName>_LOD.' } }, required: [], requiredOneOf: ['actorName', 'targetActor'], effect: 'write', behavior: { longRunning: true }, costLatency: 'long-running', costResources: 'high',
    exampleInput: { action: 'generate_lods', targetActor: 'DM_A' },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'set_lod_settings', plugins: PLUGIN,
    family: F, summary: 'Set LOD settings for a dynamic mesh.', whenToUse: ['LOD build settings must be configured.'], whenNotToUse: ['LODs must be generated; use generate_lods.'],
    inputProps: { actorName: P.actorName, lodIndex: P.lodIndex, trianglePercent: P.trianglePercent, reductionPercent: P.reductionPercent, recomputeNormals: P.recomputeNormals, recomputeTangents: P.recomputeTangents, targetActor: P.targetActor }, required: [], requiredOneOf: ['actorName', 'targetActor'], effect: 'write', behavior: { idempotency: 'idempotent' }, costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'set_lod_settings', targetActor: 'DM_A' },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'set_lod_screen_sizes', plugins: PLUGIN,
    family: F, summary: 'Set LOD screen-size thresholds for a dynamic mesh.', whenToUse: ['LOD screen-size transitions must be tuned.'], whenNotToUse: ['LOD settings must be configured; use set_lod_settings.'],
    inputProps: { actorName: P.actorName, screenSizes: P.screenSizes, targetActor: P.targetActor }, required: [], requiredOneOf: ['actorName', 'targetActor'], effect: 'write', behavior: { idempotency: 'idempotent' }, costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'set_lod_screen_sizes', targetActor: 'DM_A' },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'convert_to_nanite', plugins: PLUGIN,
    family: F, summary: 'Bake a dynamic mesh into a Nanite-enabled static mesh asset, with a material per slot and a collision choice; the reply reads naniteEnabled back from the saved mesh. An asset already at outputPath is replaced, and its material slots come back empty unless materials fills them (asset.process_asset process=mesh_materials sets them later).',
    whenToUse: ['A baked static mesh must use Nanite for virtualized geometry. The end of the subdivision-surface workflow: append_polygons a cage, subdivide with catmull_clark, set_material_id per part, bake_vertex_colors, then convert here with materials (entry i is the material of id i).'],
    whenNotToUse: ['The baked mesh must not use Nanite; use convert_to_static_mesh.'],
    inputProps: { actorName: P.actorName, outputPath: P.outputPath, targetActor: P.targetActor, materials: CONVERT_MATERIALS, collision: CONVERT_COLLISION }, required: [], requiredOneOf: ['actorName', 'targetActor'], effect: 'write', behavior: { idempotency: 'idempotent' }, costLatency: 'interactive', costResources: 'medium',
    outputProps: {
      assetPath: { type: 'string', description: 'Path of the new static mesh asset.' },
      naniteEnabled: { type: 'boolean', description: 'Whether Nanite is on for the asset.' },
      collision: { type: 'string', description: 'Collision the asset got.' },
      slots: { type: 'array', description: 'The asset\'s material slots; an empty material is the default material.', items: SLOT },
      partWarnings: MESH_USER_WARNINGS,
    },
    exampleInput: { action: 'convert_to_nanite', targetActor: 'DM_Cage', materials: ['/Engine/BasicShapes/BasicShapeMaterial'], collision: 'complex' },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'convert_to_static_mesh', plugins: PLUGIN,
    topics: ['static mesh from geometry', 'bake to static mesh', 'convert mesh', 'dynamic mesh to static'],
    family: F, summary: 'Bake a dynamic mesh actor into a static mesh asset, with a material per slot and a collision choice.',
    whenToUse: ['A dynamic mesh must be persisted as a static mesh. Give the triangles material ids with edit_dynamic_mesh set_material_id first, then list one material per id in materials (entry i is the material of id i).',
      'Colours baked with bake_vertex_colors come through to the material\'s VertexColor node unchanged.'],
    whenNotToUse: ['A static mesh must use Nanite; use convert_to_nanite.'],
    inputProps: { actorName: P.actorName, outputPath: P.outputPath, targetActor: P.targetActor, materials: CONVERT_MATERIALS, collision: CONVERT_COLLISION }, required: [], requiredOneOf: ['actorName', 'targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'medium',
    outputProps: {
      assetPath: { type: 'string', description: 'Path of the new static mesh asset.' },
      naniteEnabled: { type: 'boolean', description: 'Whether Nanite is on for the asset.' },
      collision: { type: 'string', description: 'Collision the asset got.' },
      slots: { type: 'array', description: 'The asset\'s material slots; an empty material is the default material.', items: SLOT },
      partWarnings: MESH_USER_WARNINGS,
    },
    exampleInput: { action: 'convert_to_static_mesh', targetActor: 'DM_A', outputPath: '/Game/Meshes/SM_Baked', materials: ['/Engine/BasicShapes/BasicShapeMaterial'] },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'get_mesh_info', plugins: PLUGIN,
    topics: ['vertex count', 'triangle count', 'has uvs and normals', 'how many vertices does a mesh have'],
    family: F, summary: 'Read a procedural DynamicMesh actor: vertex count and triangle count, and whether it has normals, UV sets, vertex colors and polygroups.', whenToUse: ['Mesh stats must be inspected.'], whenNotToUse: ['The mesh must be modified.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor }, required: [], requiredOneOf: ['actorName', 'targetActor'], effect: 'read', costLatency: 'instant', costResources: 'low',
    exampleInput: { action: 'get_mesh_info', targetActor: 'DM_A' }, exampleOutput: { success: true, message: 'Mesh info', vertexCount: 1200, triangleCount: 2400 },
    outputProps: {
      vertexCount: { type: 'number', description: 'Vertex count.' },
      triangleCount: { type: 'number', description: 'Triangle count.' },
    },
  }),
];
