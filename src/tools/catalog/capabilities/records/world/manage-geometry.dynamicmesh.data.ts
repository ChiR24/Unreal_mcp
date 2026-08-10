/**
 * DynamicMesh authoring records (12 actions), promoted from raw native routes.
 *
 * These edit a UDynamicMeshComponent on a placed actor vertex by vertex, rather
 * than running a modeling operator over a whole mesh, so they address the mesh
 * by `actorName` instead of the `targetActor`/`toolActor` pair the boolean and
 * modeling families use. Grounded in native Geometry dispatch
 * (Private/Domains/Geometry/**, HandleCreateProceduralMesh and friends).
 *
 * `difference` is an exact spelling alias of boolean_subtract: both dispatch to
 * HandleBooleanSubtract, so it mirrors that record's schema verbatim.
 *
 * Authored after the gateway migration, so every record declares
 * `post-migration` and contributes no occurrence to the normalization audit.
 */
import type { CapabilityRecordSource, JsonObject } from '../../index.js';
import { buildWorldRecord } from './builder.js';
import { P } from './properties.js';

const F = 'dynamicmesh';
const PLUGIN = ['GeometryScripting'] as const;
const POST = 'post-migration' as const;
const NR = 'Promoted from a raw native DynamicMesh route after the gateway migration.';

const int = (d: string): JsonObject => ({ type: 'integer', description: d });
const num = (d: string): JsonObject => ({ type: 'number', description: d });
const str = (d: string): JsonObject => ({ type: 'string', description: d });
const bool = (d: string): JsonObject => ({ type: 'boolean', description: d });

const OUT_VERTEX_COUNT = int('Vertex count of the mesh after the call.');
const OUT_TRIANGLE_COUNT = int('Triangle count of the mesh after the call.');

export const GEOMETRY_DYNAMICMESH_RECORDS: readonly CapabilityRecordSource[] = [
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'create_procedural_mesh', plugins: PLUGIN,
    family: F, summary: 'Spawn an empty DynamicMesh actor to author geometry into.',
    whenToUse: ['A mesh must be built vertex by vertex rather than from a primitive.'],
    whenNotToUse: ['A parametric shape is enough; use a create_* primitive.'],
    inputProps: { name: P.name, actorName: P.actorName, enableCollision: P.enableCollision },
    required: [], effect: 'write', costLatency: 'interactive', costResources: 'low',
    outputProps: { name: P.name, class: str('Class of the spawned actor.'), enableCollision: P.enableCollision },
    outputRequired: ['name'],
    exampleInput: { action: 'create_procedural_mesh', name: 'DM_Authored', enableCollision: true },
    exampleOutput: { success: true, name: 'DM_Authored', class: 'DynamicMeshActor', enableCollision: true },
    normalizationRationale: NR, normalizationProvenance: POST,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'append_vertex', plugins: PLUGIN,
    family: F, summary: 'Append one vertex to a DynamicMesh actor and return its index.',
    whenToUse: ['A mesh is being authored point by point.'],
    whenNotToUse: ['A whole triangle is being added; use append_triangle.'],
    inputProps: { actorName: P.actorName, position: P.position },
    required: ['actorName'], effect: 'write', costLatency: 'instant', costResources: 'low',
    outputProps: { actorName: P.actorName, vertexIndex: P.vertexIndex, vertexCount: OUT_VERTEX_COUNT },
    outputRequired: ['actorName', 'vertexIndex', 'vertexCount'],
    exampleInput: { action: 'append_vertex', actorName: 'DM_Authored', position: { x: 100, y: 0, z: 0 } },
    exampleOutput: { success: true, actorName: 'DM_Authored', vertexIndex: 3, vertexCount: 4 },
    normalizationRationale: NR, normalizationProvenance: POST,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'append_triangle', plugins: PLUGIN,
    family: F, summary: 'Append a triangle to a DynamicMesh actor from three corner positions.',
    whenToUse: ['A face must be added to an authored mesh in one call.'],
    whenNotToUse: ['Only a point is needed; use append_vertex.'],
    inputProps: { actorName: P.actorName, v0: P.v0, v1: P.v1, v2: P.v2, groupID: P.groupID },
    required: ['actorName'], effect: 'write', costLatency: 'instant', costResources: 'low',
    outputProps: {
      actorName: P.actorName, triangleIndex: P.triangleIndex,
      vertexIndex0: int('Index of the first appended corner.'),
      vertexIndex1: int('Index of the second appended corner.'),
      vertexIndex2: int('Index of the third appended corner.'),
      triangleCount: OUT_TRIANGLE_COUNT,
    },
    outputRequired: ['actorName', 'triangleIndex', 'triangleCount'],
    exampleInput: { action: 'append_triangle', actorName: 'DM_Authored', v0: { x: 0, y: 0, z: 0 }, v1: { x: 100, y: 0, z: 0 }, v2: { x: 50, y: 100, z: 0 } },
    exampleOutput: { success: true, actorName: 'DM_Authored', triangleIndex: 0, vertexIndex0: 0, vertexIndex1: 1, vertexIndex2: 2, triangleCount: 1 },
    normalizationRationale: NR, normalizationProvenance: POST,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'get_vertex_position', plugins: PLUGIN,
    family: F, summary: 'Read the position of one vertex of a DynamicMesh actor.',
    whenToUse: ['An authored vertex must be inspected before it is moved.'],
    whenNotToUse: ['Whole-mesh counts are wanted; use get_mesh_info.'],
    inputProps: { actorName: P.actorName, vertexIndex: P.vertexIndex },
    required: ['actorName', 'vertexIndex'], effect: 'read',
    costLatency: 'instant', costResources: 'low',
    outputProps: { actorName: P.actorName, vertexIndex: P.vertexIndex, position: P.position },
    outputRequired: ['actorName', 'vertexIndex', 'position'],
    exampleInput: { action: 'get_vertex_position', actorName: 'DM_Authored', vertexIndex: 1 },
    exampleOutput: { success: true, actorName: 'DM_Authored', vertexIndex: 1, position: { x: 100, y: 0, z: 0 } },
    normalizationRationale: NR, normalizationProvenance: POST,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'set_vertex_position', plugins: PLUGIN,
    family: F, summary: 'Move one vertex of a DynamicMesh actor to a new position.',
    whenToUse: ['An authored vertex must be nudged without rebuilding the mesh.'],
    whenNotToUse: ['The whole mesh must move; use translate_mesh.'],
    inputProps: { actorName: P.actorName, vertexIndex: P.vertexIndex, position: P.position },
    required: ['actorName', 'vertexIndex'], effect: 'write',
    behavior: { idempotency: 'idempotent' }, costLatency: 'instant', costResources: 'low',
    outputProps: { actorName: P.actorName, vertexIndex: P.vertexIndex, position: P.position },
    outputRequired: ['actorName', 'vertexIndex', 'position'],
    exampleInput: { action: 'set_vertex_position', actorName: 'DM_Authored', vertexIndex: 1, position: { x: 120, y: 0, z: 0 } },
    exampleOutput: { success: true, actorName: 'DM_Authored', vertexIndex: 1, position: { x: 120, y: 0, z: 0 } },
    normalizationRationale: NR, normalizationProvenance: POST,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'set_vertex_color', plugins: PLUGIN,
    family: F, summary: 'Set the vertex colour on one vertex, or on every vertex, of a DynamicMesh actor.',
    whenToUse: ['Authored geometry must carry colour the material reads.'],
    whenNotToUse: ['A material parameter is the right place for the colour.'],
    inputProps: { actorName: P.actorName, vertexIndex: P.vertexIndex, r: P.r, g: P.g, b: P.b, a: P.a, setAll: P.setAll },
    required: ['actorName'], effect: 'write',
    behavior: { idempotency: 'idempotent' }, costLatency: 'instant', costResources: 'low',
    outputProps: {
      actorName: P.actorName,
      verticesModified: int('Number of vertices whose colour changed.'),
      r: P.r, g: P.g, b: P.b, a: P.a,
    },
    outputRequired: ['actorName', 'verticesModified'],
    exampleInput: { action: 'set_vertex_color', actorName: 'DM_Authored', r: 1, g: 0, b: 0, a: 1, setAll: true },
    exampleOutput: { success: true, actorName: 'DM_Authored', verticesModified: 4, r: 1, g: 0, b: 0, a: 1 },
    normalizationRationale: NR, normalizationProvenance: POST,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'set_uvs', plugins: PLUGIN,
    family: F, summary: 'Set the UV coordinate of one vertex on a UV channel of a DynamicMesh actor.',
    whenToUse: ['An authored vertex needs an exact UV rather than a generated one.'],
    whenNotToUse: ['The whole mesh needs unwrapping; use auto_uv or unwrap_uv.'],
    inputProps: { actorName: P.actorName, vertexIndex: P.vertexIndex, u: P.u, v: P.v, uvChannel: P.uvChannel },
    required: ['actorName'], effect: 'write',
    behavior: { idempotency: 'idempotent' }, costLatency: 'instant', costResources: 'low',
    outputProps: {
      actorName: P.actorName, vertexIndex: P.vertexIndex, u: P.u, v: P.v, uvChannel: P.uvChannel,
      elementsModified: int('Number of UV elements written.'),
    },
    outputRequired: ['actorName', 'elementsModified'],
    exampleInput: { action: 'set_uvs', actorName: 'DM_Authored', vertexIndex: 1, u: 0.5, v: 0.25, uvChannel: 0 },
    exampleOutput: { success: true, actorName: 'DM_Authored', vertexIndex: 1, u: 0.5, v: 0.25, uvChannel: 0, elementsModified: 1 },
    normalizationRationale: NR, normalizationProvenance: POST,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'split_normals', plugins: PLUGIN,
    family: F, summary: 'Split the normals of a DynamicMesh actor above an angle threshold to harden edges.',
    whenToUse: ['Authored geometry shades too soft across its creases.'],
    whenNotToUse: ['Normals only need recomputing; use recalculate_normals.'],
    inputProps: { actorName: P.actorName, splitAngle: P.splitAngle },
    required: ['actorName'], effect: 'write',
    behavior: { idempotency: 'idempotent' }, costLatency: 'interactive', costResources: 'low',
    outputProps: { actorName: P.actorName, splitAngle: P.splitAngle },
    outputRequired: ['actorName', 'splitAngle'],
    exampleInput: { action: 'split_normals', actorName: 'DM_Authored', splitAngle: 45 },
    exampleOutput: { success: true, actorName: 'DM_Authored', splitAngle: 45 },
    normalizationRationale: NR, normalizationProvenance: POST,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'translate_mesh', plugins: PLUGIN,
    family: F, summary: 'Translate every vertex of a DynamicMesh actor, leaving the actor transform alone.',
    whenToUse: ['Authored geometry must shift inside its own local space.'],
    whenNotToUse: ['The actor itself should move; set its transform instead.'],
    inputProps: { actorName: P.actorName, translation: P.translation },
    required: ['actorName'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    outputProps: { actorName: P.actorName, translation: P.translation },
    outputRequired: ['actorName', 'translation'],
    exampleInput: { action: 'translate_mesh', actorName: 'DM_Authored', translation: { x: 0, y: 0, z: 50 } },
    exampleOutput: { success: true, actorName: 'DM_Authored', translation: { x: 0, y: 0, z: 50 } },
    normalizationRationale: NR, normalizationProvenance: POST,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'difference', plugins: PLUGIN,
    family: F, summary: 'Subtract a tool mesh from a target mesh under the difference spelling of boolean_subtract.',
    whenToUse: ['A caller reaches for the CSG name for a subtraction.'],
    whenNotToUse: ['The canonical spelling is available; use boolean_subtract.'],
    inputProps: { targetActor: P.targetActor, toolActor: P.toolActor, keepTool: P.keepTool, keepInside: P.keepInside },
    required: ['targetActor', 'toolActor'], effect: 'write', costLatency: 'interactive', costResources: 'medium',
    exampleInput: { action: 'difference', targetActor: 'DM_A', toolActor: 'DM_B' },
    exampleOutput: { success: true, message: 'Boolean subtract complete' },
    normalizationRationale: 'Alias of boolean_subtract: both spellings dispatch to HandleBooleanSubtract.',
    normalizationProvenance: POST,
  }),
];

export const GEOMETRY_DYNAMICMESH_RECORD_COUNT = GEOMETRY_DYNAMICMESH_RECORDS.length;

const _unusedNum: (d: string) => JsonObject = num;
const _unusedBool: (d: string) => JsonObject = bool;
void _unusedNum;
void _unusedBool;
