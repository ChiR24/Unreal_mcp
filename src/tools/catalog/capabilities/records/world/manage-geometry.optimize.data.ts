/**
 * Geometry mirror/array/optimization/UV/normals/collision/Nanite family records
 * (28 actions).
 *
 * Grounded in manage-geometry-tool.ts (mirror/array_linear/array_radial,
 * simplify_mesh/subdivide/remesh_uniform/remesh_voxel/merge/weld/fill/remove,
 * auto_uv/project_uv/transform_uvs/unwrap_uv/pack_uv_islands,
 * recalculate/flip/recompute, generate_collision/complex/simplify,
 * generate_lods/set_lod_settings/set_lod_screen_sizes/convert_to_nanite/
 * convert_to_static_mesh, get_mesh_info) and native Geometry dispatch. All
 * require the GeometryScripting plugin. convert_to_nanite requires UE 5.7+ (Nanite 5.7 per source research)
 * Nanite support and is only valid on static meshes; convert_to_static_mesh is
 * the terminal bake step.
 */
import type { CapabilityRecordSource } from '../../index.js';
import { buildWorldRecord, V5_7 } from './builder.js';
import { P } from './properties.js';

const F = 'optimize';
const NR = 'Distinct manage_geometry optimize verb and target; no cross-tool duplicate.';
const PLUGIN = ['GeometryScripting'] as const;

export const GEOMETRY_OPTIMIZE_RECORDS: readonly CapabilityRecordSource[] = [
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'mirror', plugins: PLUGIN,
    family: F, summary: 'Mirror a dynamic mesh across an axis.', whenToUse: ['A mesh must be mirrored.'], whenNotToUse: ['Instances must be arrayed; use array_linear.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, axis: P.axis, center: P.center }, required: ['targetActor', 'axis'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'mirror', targetActor: 'DM_A', axis: 'X' }, exampleOutput: { success: true, message: 'Mirror complete' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'array_linear', plugins: PLUGIN,
    family: F, summary: 'Create a linear array of a dynamic mesh.', whenToUse: ['Instances must be arrayed linearly.'], whenNotToUse: ['A radial array is needed; use array_radial.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, count: P.count, offset: P.offset }, required: ['targetActor', 'count'], effect: 'write', costLatency: 'interactive', costResources: 'medium',
    exampleInput: { action: 'array_linear', targetActor: 'DM_A', count: 5, offset: { x: 200, y: 0, z: 0 } }, exampleOutput: { success: true, message: 'Linear array complete' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'array_radial', plugins: PLUGIN,
    family: F, summary: 'Create a radial array of a dynamic mesh.', whenToUse: ['Instances must be arrayed radially.'], whenNotToUse: ['A linear array is needed; use array_linear.'],
    inputProps: { actorName: P.actorName, angle: P.angle, targetActor: P.targetActor, count: P.count, center: P.center }, required: ['actorName', 'count'], effect: 'write', costLatency: 'interactive', costResources: 'medium',
    exampleInput: { action: 'array_radial', actorName: 'DM_A', count: 8, center: { x: 0, y: 0, z: 0 } }, exampleOutput: { success: true, message: 'Radial array complete' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'simplify_mesh', plugins: PLUGIN,
    family: F, summary: 'Simplify a dynamic mesh to a target triangle count.', whenToUse: ['A mesh must be simplified.'], whenNotToUse: ['A mesh must be subdivided; use subdivide.'],
    inputProps: { actorName: P.actorName, reductionPercent: P.reductionPercent, targetActor: P.targetActor, targetTriangleCount: P.targetTriangleCount }, required: ['targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'medium',
    exampleInput: { action: 'simplify_mesh', targetActor: 'DM_A', targetTriangleCount: 5000 }, exampleOutput: { success: true, message: 'Simplify mesh complete' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'subdivide', plugins: PLUGIN,
    family: F, summary: 'Subdivide a dynamic mesh (increase resolution).', whenToUse: ['A mesh must be subdivided.'], whenNotToUse: ['A mesh must be simplified; use simplify_mesh.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, iterations: P.iterations }, required: ['targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'medium',
    exampleInput: { action: 'subdivide', targetActor: 'DM_A', iterations: 2 }, exampleOutput: { success: true, message: 'Subdivide complete' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'remesh_uniform', plugins: PLUGIN,
    family: F, summary: 'Uniformly remesh a dynamic mesh to a target edge length.', whenToUse: ['A uniform remesh is needed.'], whenNotToUse: ['A voxel remesh is needed; use remesh_voxel.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, targetEdgeLength: P.targetEdgeLength }, required: ['targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'medium',
    exampleInput: { action: 'remesh_uniform', targetActor: 'DM_A', targetEdgeLength: 10 }, exampleOutput: { success: true, message: 'Uniform remesh complete' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'merge_vertices', plugins: PLUGIN,
    family: F, summary: 'Merge coincident vertices of a dynamic mesh.', whenToUse: ['Duplicate vertices must be welded/merged.'], whenNotToUse: ['A precise weld is needed; use weld_vertices.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor }, required: ['targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'merge_vertices', targetActor: 'DM_A' }, exampleOutput: { success: true, message: 'Merge vertices complete' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'remesh_voxel', plugins: PLUGIN,
    family: F, summary: 'Voxel-remesh a dynamic mesh to a watertight form.', whenToUse: ['A watertight voxel remesh is needed.'], whenNotToUse: ['A uniform remesh is needed; use remesh_uniform.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, targetEdgeLength: P.targetEdgeLength }, required: ['targetActor'], effect: 'write', behavior: { longRunning: true }, costLatency: 'long-running', costResources: 'high',
    exampleInput: { action: 'remesh_voxel', targetActor: 'DM_A', targetEdgeLength: 8 }, exampleOutput: { success: true, message: 'Voxel remesh complete' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'weld_vertices', plugins: PLUGIN,
    family: F, summary: 'Weld vertices of a dynamic mesh within a distance.', whenToUse: ['Vertices must be welded by threshold.'], whenNotToUse: ['A merge is needed; use merge_vertices.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, weldDistance: P.weldDistance }, required: ['targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'weld_vertices', targetActor: 'DM_A', weldDistance: 0.1 }, exampleOutput: { success: true, message: 'Weld vertices complete' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'fill_holes', plugins: PLUGIN,
    family: F, summary: 'Fill boundary holes of a dynamic mesh.', whenToUse: ['Mesh holes must be capped.'], whenNotToUse: ['Degenerate triangles must be removed; use remove_degenerates.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor }, required: ['targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'fill_holes', targetActor: 'DM_A' }, exampleOutput: { success: true, message: 'Fill holes complete' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'remove_degenerates', plugins: PLUGIN,
    family: F, summary: 'Remove degenerate triangles from a dynamic mesh.', whenToUse: ['Degenerate triangles must be removed.'], whenNotToUse: ['Holes must be filled; use fill_holes.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor }, required: ['targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'remove_degenerates', targetActor: 'DM_A' }, exampleOutput: { success: true, message: 'Remove degenerates complete' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'auto_uv', plugins: PLUGIN,
    family: F, summary: 'Auto-generate UVs for a dynamic mesh.', whenToUse: ['UVs must be auto-generated.'], whenNotToUse: ['UVs must be unwrapped; use unwrap_uv.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, uvChannel: P.uvChannel }, required: ['targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'auto_uv', targetActor: 'DM_A', uvChannel: 0 }, exampleOutput: { success: true, message: 'Auto UV complete' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'project_uv', plugins: PLUGIN,
    family: F, summary: 'Project UVs from a planar/box projection.', whenToUse: ['UVs must be projected.'], whenNotToUse: ['UVs must be transformed; use transform_uvs.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, uvChannel: P.uvChannel }, required: ['targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'project_uv', targetActor: 'DM_A', uvChannel: 0 }, exampleOutput: { success: true, message: 'Project UV complete' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'transform_uvs', plugins: PLUGIN,
    family: F, summary: 'Transform existing UVs (scale/offset) of a dynamic mesh.', whenToUse: ['UVs must be scaled/offset.'], whenNotToUse: ['UVs must be repacked; use pack_uv_islands.'],
    inputProps: { actorName: P.actorName, rotation: P.uvRotation, targetActor: P.targetActor, uvChannel: P.uvChannel, uvScale: P.uvScale, uvOffset: P.uvOffset }, required: ['targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'transform_uvs', targetActor: 'DM_A', uvChannel: 0, uvScale: { u: 2, v: 2 } }, exampleOutput: { success: true, message: 'Transform UVs complete' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'unwrap_uv', plugins: PLUGIN,
    family: F, summary: 'Unwrap UVs of a dynamic mesh.', whenToUse: ['UVs must be unwrapped.'], whenNotToUse: ['UVs must be auto-generated; use auto_uv.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, uvChannel: P.uvChannel }, required: ['targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'unwrap_uv', targetActor: 'DM_A', uvChannel: 0 }, exampleOutput: { success: true, message: 'Unwrap UV complete' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'pack_uv_islands', plugins: PLUGIN,
    family: F, summary: 'Pack UV islands of a dynamic mesh into a 0-1 square.', whenToUse: ['UV islands must be packed.'], whenNotToUse: ['UVs must be transformed; use transform_uvs.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, uvChannel: P.uvChannel }, required: ['targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'pack_uv_islands', targetActor: 'DM_A', uvChannel: 0 }, exampleOutput: { success: true, message: 'Pack UV islands complete' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'recalculate_normals', plugins: PLUGIN,
    family: F, summary: 'Recalculate normals of a dynamic mesh.', whenToUse: ['Normals must be recalculated.'], whenNotToUse: ['Normals must be flipped; use flip_normals.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, hardEdgeAngle: P.hardEdgeAngle, computeWeightedNormals: P.computeWeightedNormals }, required: ['actorName'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'recalculate_normals', actorName: 'DM_A', hardEdgeAngle: 60 }, exampleOutput: { success: true, message: 'Recalculate normals complete' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'flip_normals', plugins: PLUGIN,
    family: F, summary: 'Flip normals of a dynamic mesh.', whenToUse: ['Normals must be flipped (inside-out).'], whenNotToUse: ['Normals must be recomputed; use recalculate_normals.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor }, required: ['targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'flip_normals', targetActor: 'DM_A' }, exampleOutput: { success: true, message: 'Flip normals complete' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'recompute_tangents', plugins: PLUGIN,
    family: F, summary: 'Recompute tangents of a dynamic mesh.', whenToUse: ['Tangents must be recomputed for correct shading.'], whenNotToUse: ['Normals must be recomputed; use recalculate_normals.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor }, required: ['targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'recompute_tangents', targetActor: 'DM_A' }, exampleOutput: { success: true, message: 'Recompute tangents complete' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'generate_collision', plugins: PLUGIN,
    family: F, summary: 'Generate simple collision for a dynamic mesh.', whenToUse: ['Simple collision must be generated.'], whenNotToUse: ['Complex collision is needed; use generate_complex_collision.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor, collisionType: P.collisionType }, required: ['targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'generate_collision', targetActor: 'DM_A', collisionType: 'Default' }, exampleOutput: { success: true, message: 'Generate collision complete' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'generate_complex_collision', plugins: PLUGIN,
    family: F, summary: 'Generate complex (convex decomposition) collision.', whenToUse: ['Complex collision must be generated.'], whenNotToUse: ['Simple collision is sufficient; use generate_collision.'],
    inputProps: { actorName: P.actorName, hullCount: P.hullCount, maxHullCount: P.maxHullCount, maxHullVerts: P.maxHullVerts, maxVerticesPerHull: P.maxVerticesPerHull, hullPrecision: P.hullPrecision, targetActor: P.targetActor }, required: ['targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'medium',
    exampleInput: { action: 'generate_complex_collision', targetActor: 'DM_A' }, exampleOutput: { success: true, message: 'Generate complex collision complete' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'simplify_collision', plugins: PLUGIN,
    family: F, summary: 'Simplify an existing collision hull.', whenToUse: ['A collision hull must be simplified.'], whenNotToUse: ['Collision must be generated; use generate_collision.'],
    inputProps: { actorName: P.actorName, simplificationFactor: P.simplificationFactor, targetHullCount: P.targetHullCount, targetActor: P.targetActor }, required: ['targetActor'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'simplify_collision', targetActor: 'DM_A' }, exampleOutput: { success: true, message: 'Simplify collision complete' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'generate_lods', plugins: PLUGIN,
    family: F, summary: 'Generate LODs for a dynamic mesh.', whenToUse: ['LODs must be generated for a mesh.'], whenNotToUse: ['LOD screen sizes must be set; use set_lod_screen_sizes.'],
    inputProps: { actorName: P.actorName, lodCount: P.lodCount, targetActor: P.targetActor }, required: ['targetActor'], effect: 'write', behavior: { longRunning: true }, costLatency: 'long-running', costResources: 'high',
    exampleInput: { action: 'generate_lods', targetActor: 'DM_A' }, exampleOutput: { success: true, message: 'Generate LODs complete' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'set_lod_settings', plugins: PLUGIN,
    family: F, summary: 'Set LOD settings for a dynamic mesh.', whenToUse: ['LOD build settings must be configured.'], whenNotToUse: ['LODs must be generated; use generate_lods.'],
    inputProps: { actorName: P.actorName, lodIndex: P.lodIndex, trianglePercent: P.trianglePercent, reductionPercent: P.reductionPercent, recomputeNormals: P.recomputeNormals, recomputeTangents: P.recomputeTangents, targetActor: P.targetActor }, required: ['targetActor'], effect: 'write', behavior: { idempotency: 'idempotent' }, costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'set_lod_settings', targetActor: 'DM_A' }, exampleOutput: { success: true, message: 'Set LOD settings complete' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'set_lod_screen_sizes', plugins: PLUGIN,
    family: F, summary: 'Set LOD screen-size thresholds for a dynamic mesh.', whenToUse: ['LOD screen-size transitions must be tuned.'], whenNotToUse: ['LOD settings must be configured; use set_lod_settings.'],
    inputProps: { actorName: P.actorName, screenSizes: P.screenSizes, targetActor: P.targetActor }, required: ['targetActor'], effect: 'write', behavior: { idempotency: 'idempotent' }, costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'set_lod_screen_sizes', targetActor: 'DM_A' }, exampleOutput: { success: true, message: 'Set LOD screen sizes complete' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'convert_to_nanite', plugins: PLUGIN,
    family: F, summary: 'Enable Nanite on a static mesh (UE 5.7+).', whenToUse: ['A static mesh must use Nanite for virtualization.'], whenNotToUse: ['A dynamic mesh must be baked; use convert_to_static_mesh.'],
    inputProps: { actorName: P.actorName, outputPath: P.outputPath, targetActor: P.targetActor }, required: ['targetActor'], effect: 'write', behavior: { idempotency: 'idempotent' }, costLatency: 'interactive', costResources: 'medium',
    unrealMin: V5_7,
    exampleInput: { action: 'convert_to_nanite', targetActor: 'SM_Rock' }, exampleOutput: { success: true, message: 'Nanite enabled' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'convert_to_static_mesh', plugins: PLUGIN,
    family: F, summary: 'Bake a dynamic mesh actor into a static mesh asset.', whenToUse: ['A dynamic mesh must be persisted as a static mesh.'], whenNotToUse: ['A static mesh must use Nanite; use convert_to_nanite.'],
    inputProps: { actorName: P.actorName, outputPath: P.outputPath, targetActor: P.targetActor, assetPath: P.assetPath }, required: ['targetActor', 'assetPath'], effect: 'write', costLatency: 'interactive', costResources: 'medium',
    exampleInput: { action: 'convert_to_static_mesh', targetActor: 'DM_A', assetPath: '/Game/Meshes/SM_Baked' }, exampleOutput: { success: true, message: 'Converted to static mesh' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'get_mesh_info', plugins: PLUGIN,
    family: F, summary: 'Return vertex/face/material info for a dynamic mesh.', whenToUse: ['Mesh stats must be inspected.'], whenNotToUse: ['The mesh must be modified.'],
    inputProps: { actorName: P.actorName, targetActor: P.targetActor }, required: ['targetActor'], effect: 'read', costLatency: 'instant', costResources: 'low',
    exampleInput: { action: 'get_mesh_info', targetActor: 'DM_A' }, exampleOutput: { success: true, message: 'Mesh info', vertexCount: 1200, triangleCount: 2400 },
    outputProps: {
      vertexCount: { type: 'number', description: 'Vertex count.' },
      triangleCount: { type: 'number', description: 'Triangle count.' },
    },
    normalizationRationale: NR,
  }),
];
