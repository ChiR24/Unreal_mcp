/**
 * DynamicMesh authoring records (11 actions), promoted from raw native routes.
 *
 * These edit a UDynamicMeshComponent on a placed actor vertex by vertex, rather
 * than running a modeling operator over a whole mesh, so they address the mesh
 * by `actorName` instead of the `targetActor`/`toolActor` pair the boolean and
 * modeling families use. Grounded in native Geometry dispatch
 * (Private/Domains/Geometry/**, HandleCreateProceduralMesh and friends).
 *
 * `difference` is an exact spelling alias of boolean_subtract: both dispatch to
 * HandleBooleanSubtract, so it mirrors that record's schema verbatim.
 */
import type { CapabilityRecordSource, JsonObject } from '../../model.js';
import { buildWorldRecord } from './builder.js';
import { P } from './properties.js';
import { str } from '../shared/schema-props.js';

const F = 'dynamicmesh';
const PLUGIN = ['GeometryScripting'] as const;

const int = (d: string): JsonObject => ({ type: 'integer', description: d });

const OUT_VERTEX_COUNT = int('Vertex count of the mesh after the call.');
const OUT_TRIANGLE_COUNT = int('Triangle count of the mesh after the call.');

const POINT = {
  type: 'object',
  properties: { x: { type: 'number', description: 'X' }, y: { type: 'number', description: 'Y' }, z: { type: 'number', description: 'Z' } },
  required: ['x', 'y', 'z'],
  additionalProperties: false,
};
const POLYGON_VERTICES = {
  type: 'array',
  items: POINT,
  minItems: 1,
  maxItems: 20000,
  description: 'New points {x, y, z} in the mesh\'s local space, at most 20000. The faces of this call index into this list, so its first point is index 0. Points are never merged with the mesh\'s existing ones, and a point no face uses is left out.',
};
const POLYGON_FACES = {
  type: 'array',
  items: { type: 'array', items: { type: 'integer', minimum: 0 }, minItems: 3, maxItems: 256 },
  minItems: 1,
  maxItems: 20000,
  description: 'Faces, each a list of three or more vertex indices into vertices (at most 20000 faces). List a face\'s corners so that (v1 - v0) x (v2 - v0) points outward, and let two faces that share an edge run along it in opposite directions; the call refuses a face wound the other way, or an edge shared by more than two faces, and names the face. A face with more than three corners is triangulated (it may be concave) and all its triangles keep the face\'s polygroup.',
};
const FACE_GROUPS = {
  type: 'array',
  items: { type: 'integer', minimum: 0 },
  maxItems: 20000,
  description: 'Polygroup id of each face, one entry per face, 0 to 1000000 (default: a new unique group per face). Subdivision with catmull_clark treats every polygroup as one face of the cage, so leave this out for a cage of separate faces; give several faces one id to make one flat region.',
};
const FACE_MATERIALS = {
  type: 'array',
  items: { type: 'integer', minimum: 0, maximum: 255 },
  maxItems: 20000,
  description: 'Material id of each face, one entry per face, 0 to 255 (default 0). A material id becomes a static-mesh material slot when the mesh is baked with convert_to_static_mesh or convert_to_nanite.',
};

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
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'get_vertex_position', plugins: PLUGIN,
    family: F, summary: 'Read the x, y, z position of one vertex, by vertexIndex, of a procedural DynamicMesh actor.',
    whenToUse: ['An authored vertex must be inspected before it is moved.'],
    whenNotToUse: ['Whole-mesh counts are wanted; use get_mesh_info.'],
    inputProps: { actorName: P.actorName, vertexIndex: P.vertexIndex },
    required: ['actorName', 'vertexIndex'], effect: 'read',
    costLatency: 'instant', costResources: 'low',
    outputProps: { actorName: P.actorName, vertexIndex: P.vertexIndex, position: P.position },
    outputRequired: ['actorName', 'vertexIndex', 'position'],
    exampleInput: { action: 'get_vertex_position', actorName: 'DM_Authored', vertexIndex: 1 },
    exampleOutput: { success: true, actorName: 'DM_Authored', vertexIndex: 1, position: { x: 100, y: 0, z: 0 } },
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
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'append_polygons', plugins: PLUGIN,
    topics: ['polygon cage', 'author a mesh from vertices and faces', 'quad mesh', 'hard surface cage', 'low poly blockout mesh', 'subdivision cage'],
    family: F, summary: 'Author a polygon cage on a DynamicMesh actor in one call: new vertices and faces of three or more corners, each with its own polygroup and material id.',
    whenToUse: ['A smooth, rounded model starts as a coarse cage: append its vertices and faces here (one polygroup per face), then optimize_mesh subdivide with scheme catmull_clark and iterations 2 or 3.',
      'Faces must be built from an explicit vertex list, quads and larger polygons included, with polygroups or material ids set up front.'],
    whenNotToUse: ['One triangle or one point is enough; use append_triangle or append_vertex.', 'A parametric shape is enough; use a create_* primitive.'],
    inputProps: { actorName: P.actorName, vertices: POLYGON_VERTICES, faces: POLYGON_FACES, faceGroups: FACE_GROUPS, faceMaterials: FACE_MATERIALS },
    required: ['actorName', 'vertices', 'faces'], effect: 'write', costLatency: 'interactive', costResources: 'medium',
    outputProps: {
      actorName: P.actorName,
      verticesAdded: int('Points appended: those some face uses.'),
      facesAdded: int('Faces appended.'),
      trianglesAdded: int('Triangles the faces came to once larger polygons were triangulated.'),
      vertexCount: OUT_VERTEX_COUNT,
      triangleCount: OUT_TRIANGLE_COUNT,
      groupCount: int('Distinct polygroups the mesh holds after the call.'),
    },
    outputRequired: ['actorName', 'verticesAdded', 'facesAdded', 'trianglesAdded', 'vertexCount', 'triangleCount', 'groupCount'],
    exampleInput: {
      action: 'append_polygons', actorName: 'DM_Cage',
      vertices: [
        { x: -50, y: -50, z: -50 }, { x: 50, y: -50, z: -50 }, { x: 50, y: 50, z: -50 }, { x: -50, y: 50, z: -50 },
        { x: -50, y: -50, z: 50 }, { x: 50, y: -50, z: 50 }, { x: 50, y: 50, z: 50 }, { x: -50, y: 50, z: 50 },
      ],
      faces: [[0, 3, 2, 1], [4, 5, 6, 7], [0, 1, 5, 4], [1, 2, 6, 5], [2, 3, 7, 6], [3, 0, 4, 7]],
    },
    exampleOutput: { success: true, actorName: 'DM_Cage', verticesAdded: 8, facesAdded: 6, trianglesAdded: 12, vertexCount: 8, triangleCount: 12, groupCount: 6 },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'difference', plugins: PLUGIN,
    family: F, summary: 'Subtract a tool mesh from a target mesh under the difference spelling of boolean_subtract.',
    whenToUse: ['A caller reaches for the CSG name for a subtraction.'],
    whenNotToUse: ['The canonical spelling is available; use boolean_subtract.'],
    inputProps: { targetActor: P.targetActor, toolActor: P.toolActor, keepTool: P.keepTool },
    required: ['targetActor', 'toolActor'], effect: 'write', costLatency: 'interactive', costResources: 'medium',
    exampleInput: { action: 'difference', targetActor: 'DM_A', toolActor: 'DM_B' },
  }),
];
