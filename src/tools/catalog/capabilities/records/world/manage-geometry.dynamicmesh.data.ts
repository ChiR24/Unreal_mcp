/**
 * DynamicMesh authoring records (17 actions), promoted from raw native routes.
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
  description: 'Faces, each a list of three or more vertex indices into vertices (at most 20000 faces). List a face\'s corners so that (v1 - v0) x (v2 - v0) points outward, and let two faces that share an edge run along it in opposite directions; the call refuses a face wound the other way, or an edge shared by more than two faces, and names the face. A face with more than three corners is triangulated (it may be concave) and all its triangles keep the face\'s polygroup. The new faces get smooth normals, shared where they meet; split_normals creases the hard edges.',
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

const MATERIAL_ID = { type: 'integer', minimum: 0, maximum: 255 };
const OFFSET = (d: string): JsonObject => ({ ...POINT, required: [], description: d });
const COUNT = (d: string): JsonObject => ({ type: 'integer', minimum: 1, maximum: 1000, description: d });
const REPEAT = (part: string): JsonObject => ({
  type: 'object',
  properties: {
    count: COUNT(`Copies along offset, the ${part} itself included (default 1).`),
    offset: OFFSET('Step from one copy to the next, e.g. {x: 200} for one every 2 m.'),
    rows: COUNT('Rows of those copies (default 1).'),
    rowOffset: OFFSET('Step from one row to the next.'),
    layers: COUNT('Layers of those rows (default 1), e.g. the tiers of a stack.'),
    layerOffset: OFFSET('Step from one layer to the next.'),
  },
  additionalProperties: false,
  description: `Lay the ${part} out count x rows x layers times, each copy moved by offset per step, rowOffset per row and layerOffset per layer. A step larger than the ${part} leaves a gap between copies.`,
});
const BOXES = {
  type: 'array',
  minItems: 1,
  maxItems: 500,
  items: {
    type: 'object',
    properties: {
      min: { ...POINT, description: 'Corner with the smallest x, y and z, in the mesh\'s local space.' },
      max: { ...POINT, description: 'Corner with the largest x, y and z; larger than min on every axis.' },
      materialId: { ...MATERIAL_ID, description: 'Material id of every face of the box and its copies (default 0); a baked asset gets one slot per id.' },
      materialIds: { type: 'array', items: MATERIAL_ID, maxItems: 256, description: 'Material ids the copies take in turn (copy k takes entry k modulo the list length), counted along count, then rows, then layers: colour variety across a stack or a row. Replaces materialId.' },
      open: { type: 'array', items: { type: 'string', enum: ['-x', '+x', '-y', '+y', '-z', '+z'] }, maxItems: 6, description: 'Faces to leave out, by the direction they face: a face nobody sees (the bottom of a post on a floor, the faces two touching boxes share) costs triangles and can flicker.' },
      repeat: REPEAT('box'),
    },
    required: ['min', 'max'],
    additionalProperties: false,
  },
  description: 'Boxes to append, at most 2500 once repeats are unrolled: each an axis-aligned box from min to max, its six faces wound outward and each face its own polygroup.',
};
const LOFT_SECTIONS = {
  type: 'array',
  minItems: 2,
  maxItems: 256,
  items: { type: 'array', items: POINT, minItems: 2, maxItems: 256 },
  description: 'Cross sections in order along the shape, 2 to 256, each a list of {x, y, z} points in the mesh\'s local space, the same count in every section (3 or more for a closed loop). Point k of one section joins point k of the next, so list every section in the same turning order from the same starting place. A section need not be flat (a raked bow); a section a few cm across gives a blunt end, endPoint a sharp one.',
};
const PLACEMENTS = {
  type: 'array',
  minItems: 1,
  maxItems: 500,
  items: {
    type: 'object',
    properties: {
      location: { ...POINT, description: 'Where the copy\'s pivot goes, in the target mesh\'s local space (default 0, 0, 0).' },
      rotation: {
        type: 'object',
        properties: { pitch: { type: 'number', description: 'Pitch' }, yaw: { type: 'number', description: 'Yaw' }, roll: { type: 'number', description: 'Roll' } },
        additionalProperties: false,
        description: 'Turn of the copy in degrees (default none).',
      },
      scale: { ...POINT, description: 'Scale of the copy on each axis (default 1, 1, 1); a negative axis mirrors it, faces still turned outward.' },
      materialIds: {
        type: 'array',
        maxItems: 256,
        items: { type: ['integer', 'array'], minimum: 0, maximum: 255, items: MATERIAL_ID, minItems: 1, maxItems: 256 },
        description: 'The target material id of each material slot of the copied mesh, one entry per slot in slot order (the reply\'s sourceSlots, or inspect_object objectKind=mesh, says how many): an id, or a list of ids the copies take in turn (copy k takes entry k modulo the list length) for colour variety across a stack or a row. Omitted, each slot keeps its own index as its id.',
      },
      repeat: REPEAT('copy'),
    },
    additionalProperties: false,
  },
  description: 'Where the copies go, at most 2500 once repeats are unrolled: each placement a transform in the target mesh\'s local space, an optional repeat on up to three axes (its steps add to location and are not turned by rotation), and how the copy\'s material slots map to the target\'s material ids.',
};
const LOFT_TIP = (which: string): JsonObject => ({ ...POINT, description: `Close the ${which} section as a fan of triangles to this point instead of a flat cap: a bow, a nose cone, a spire. Needs closed true.` });
const APPEND_OUT = {
  actorName: P.actorName,
  verticesAdded: int('Points appended: those some face uses.'),
  facesAdded: int('Faces appended.'),
  trianglesAdded: int('Triangles the faces came to once larger polygons were triangulated.'),
  vertexCount: OUT_VERTEX_COUNT,
  triangleCount: OUT_TRIANGLE_COUNT,
  groupCount: int('Distinct polygroups the mesh holds after the call.'),
};
const APPEND_OUT_REQUIRED = ['actorName', 'verticesAdded', 'facesAdded', 'trianglesAdded', 'vertexCount', 'triangleCount', 'groupCount'];

export const GEOMETRY_DYNAMICMESH_RECORDS: readonly CapabilityRecordSource[] = [
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'create_procedural_mesh', plugins: PLUGIN,
    family: F, summary: 'Spawn a DynamicMesh actor to author geometry into: empty, or a copy of a static mesh asset. One undo step.',
    whenToUse: ['A mesh must be built vertex by vertex rather than from a primitive.',
      'An existing static mesh asset must be cut, trimmed or edited: create with meshPath, edit, then bake it with convert_to_static_mesh.'],
    whenNotToUse: ['A parametric shape is enough; use a create_* primitive.'],
    inputProps: { name: P.name, actorName: P.actorName, enableCollision: P.enableCollision,
      meshPath: str('Static mesh asset to start from: its geometry is copied in, each section as the material id of its slot, in the mesh\'s own local space. Omitted, the mesh starts empty.') },
    required: [], effect: 'write', costLatency: 'interactive', costResources: 'low',
    outputProps: { name: P.name, class: str('Class of the spawned actor.'), enableCollision: P.enableCollision,
      triangleCount: int('With meshPath: triangles copied from the asset.'),
      undo: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'What editor undo takes back: {undoable, transactionScope}.' } },
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
    outputProps: APPEND_OUT, outputRequired: APPEND_OUT_REQUIRED,
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
    parentTool: 'manage_geometry', action: 'append_cuboids', plugins: PLUGIN,
    topics: ['railing posts and rails', 'stack of crates', 'grid of windows', 'shelves and frames', 'beams and pillars', 'repeat a part in rows'],
    family: F, summary: 'Append axis-aligned boxes to a DynamicMesh actor in one call, each with a material id, faces left open and a repeat along up to three axes (a railing, a stack of crates, a grid of windows, shelves, pillars), wound outward for you.',
    whenToUse: ['A model is built from rectangular parts: posts and rails, beams, pillars, crates and containers, panels, windows, steps or furniture.',
      'The same part repeats in a row, a grid or a block (repeat count with offset, rows with rowOffset, layers with layerOffset), each copy taking the next id of materialIds for colour variety.'],
    whenNotToUse: ['Boxes are rotated, or the shape is curved: use append_loft for a shape through cross sections, or append_polygons.',
      'Separate actors are wanted rather than one mesh: use control_actor.spawn.'],
    inputProps: { actorName: P.actorName, boxes: BOXES },
    required: ['actorName', 'boxes'], effect: 'write', costLatency: 'interactive', costResources: 'medium',
    outputProps: APPEND_OUT, outputRequired: APPEND_OUT_REQUIRED,
    exampleInput: {
      action: 'append_cuboids', actorName: 'DM_Railing',
      boxes: [
        { min: { x: -2, y: -2, z: 0 }, max: { x: 2, y: 2, z: 100 }, open: ['-z'], repeat: { count: 11, offset: { x: 200, y: 0, z: 0 } } },
        { min: { x: 0, y: -2, z: 96 }, max: { x: 2000, y: 2, z: 100 }, materialId: 1 },
      ],
    },
    exampleOutput: { success: true, actorName: 'DM_Railing', verticesAdded: 96, facesAdded: 61, trianglesAdded: 122, vertexCount: 96, triangleCount: 122, groupCount: 61 },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'append_loft', plugins: PLUGIN,
    topics: ['loft through cross sections', 'hull from sections', 'car body shape', 'tapered tube', 'bottle or vase profile', 'skin between profiles'],
    family: F, summary: 'Append a surface lofted through cross sections to a DynamicMesh actor: each section a loop of points (the same count in every one), joined in order with every face turned outward, the ends capped flat or closed to a tip point.',
    whenToUse: ['A shape changes its cross section along its length: a ship hull or a car body (sections narrowing to the bow or nose), a fuselage, a bottle, a horn, a tapered column.',
      'A pointed end is wanted: give endPoint (or startPoint) and the last section closes as a fan to it.',
      'Parts of the skin need their own materials: edgeMaterials gives each edge of the section loop an id, so the strip it sweeps (a waterline band, a stripe, a window row) keeps it.'],
    whenNotToUse: ['The shape is round about one axis: use model_mesh with modeling=revolve.', 'The shape follows a curved path with one profile: use model_mesh with modeling=sweep or extrude_along_spline.'],
    inputProps: {
      actorName: P.actorName, sections: LOFT_SECTIONS,
      closed: { type: 'boolean', description: 'Each section is a closed loop and the loft a closed tube (default true). false joins open polylines into a sheet (an awning, a curved panel), its faces turned by the order of the points; flip reverses them.' },
      caps: { type: 'boolean', description: 'Close a closed tube\'s first and last sections with a flat face each (default true), unless startPoint or endPoint closes that end.' },
      startPoint: LOFT_TIP('first'), endPoint: LOFT_TIP('last'),
      edgeMaterials: { type: 'array', items: MATERIAL_ID, maxItems: 256, description: 'Material id of each edge of the section loop, one per edge (point k to point k+1; a closed loop has as many edges as points, an open one one fewer): the strip each edge sweeps keeps it, for a painted band, a stripe or a row of windows. Default 0.' },
      capMaterialId: { ...MATERIAL_ID, description: 'Material id of the end caps and tip fans (default 0).' },
      flip: { type: 'boolean', description: 'Reverse every face. A closed tube is turned outward from the first section\'s turning order and the way the sections run, so flip is for an open sheet or an inside surface (a tunnel seen from within).' },
    },
    required: ['actorName', 'sections'], effect: 'write', costLatency: 'interactive', costResources: 'medium',
    outputProps: APPEND_OUT, outputRequired: APPEND_OUT_REQUIRED,
    exampleInput: {
      action: 'append_loft', actorName: 'DM_Hull',
      sections: [
        [{ x: 0, y: 400, z: 300 }, { x: 0, y: 400, z: -200 }, { x: 0, y: -400, z: -200 }, { x: 0, y: -400, z: 300 }],
        [{ x: 800, y: 300, z: 300 }, { x: 800, y: 200, z: -150 }, { x: 800, y: -200, z: -150 }, { x: 800, y: -300, z: 300 }],
      ],
      endPoint: { x: 1200, y: 0, z: 300 },
    },
    exampleOutput: { success: true, actorName: 'DM_Hull', verticesAdded: 9, facesAdded: 9, trianglesAdded: 14, vertexCount: 9, triangleCount: 14, groupCount: 9 },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'append_mesh', plugins: PLUGIN,
    topics: ['kitbash from existing parts', 'copy a mesh many times', 'detailed parts in rows', 'assemble a model from props', 'cargo on a deck', 'repeat a detailed part'],
    family: F, summary: 'Copy a static mesh asset into a DynamicMesh actor at placements, each repeated along up to three axes, with its material slots mapped to the target\'s material ids (a list cycles copy by copy for colour variety): a model assembled from detailed parts in one call, its copies keeping the asset\'s normals and UVs.',
    whenToUse: ['A model needs detail an existing mesh already has: containers on a ship, window frames on a facade, seats in rows, bolts on a plate, crates in a stack. Copy the detailed part here instead of approximating it with boxes, then bake with convert_to_static_mesh.',
      'The copies need different colours: give materialIds a list for the painted slot and each copy takes the next id.'],
    whenNotToUse: ['Each copy must stay a separate actor (moved, hidden or destroyed on its own): use control_actor.spawn.',
      'The part is a plain box: append_cuboids builds it without an asset.'],
    inputProps: {
      actorName: P.actorName,
      meshPath: str('Static mesh asset to copy, with the normals and UVs it was built with, each section on the material id of its slot unless materialIds maps it.'),
      placements: PLACEMENTS,
      lod: { type: 'string', enum: ['source', 'render'], description: 'Which geometry is copied: source (default) is the full-detail source mesh; render is the mesh as it draws, for a Nanite mesh its fallback, often a twentieth of the source triangles: use it to lay out many copies of a Nanite part within the mesh\'s 500000-triangle budget.' },
    },
    required: ['actorName', 'meshPath', 'placements'], effect: 'write', costLatency: 'interactive', costResources: 'medium',
    outputProps: {
      actorName: P.actorName,
      meshPath: str('Static mesh that was copied.'),
      copies: int('Copies appended once repeats were unrolled.'),
      sourceSlots: int('Material slots of the copied mesh: the entries each placement\'s materialIds needs.'),
      trianglesAdded: int('Triangles the copies added.'),
      vertexCount: OUT_VERTEX_COUNT,
      triangleCount: OUT_TRIANGLE_COUNT,
      materialIdsInUse: { type: 'array', items: { type: 'integer' }, description: 'Every material id the mesh carries now, ascending: one material each when it is baked.' },
    },
    outputRequired: ['actorName', 'meshPath', 'copies', 'sourceSlots', 'trianglesAdded', 'vertexCount', 'triangleCount', 'materialIdsInUse'],
    exampleInput: {
      action: 'append_mesh', actorName: 'DM_Deck', meshPath: '/Game/Props/SM_Container', lod: 'render',
      placements: [{ location: { x: 0, y: -250, z: 0 }, materialIds: [[3, 4, 5], 6, 7], repeat: { count: 4, offset: { x: 620, y: 0, z: 0 }, rows: 2, rowOffset: { x: 0, y: 250, z: 0 }, layers: 3, layerOffset: { x: 0, y: 0, z: 262 } } }],
    },
    exampleOutput: { success: true, actorName: 'DM_Deck', meshPath: '/Game/Props/SM_Container', copies: 24, sourceSlots: 3, trianglesAdded: 62784, vertexCount: 31536, triangleCount: 62784, materialIdsInUse: [3, 4, 5, 6, 7] },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'delete_triangles', plugins: PLUGIN,
    topics: ['delete faces', 'remove part of a mesh', 'cut away faces', 'delete hidden faces'],
    family: F, summary: 'Delete the triangles of a DynamicMesh actor that triangleIndices or a region picks (by box, facing direction, polygroup or material id), with the points only they used.',
    whenToUse: ['A part of an authored mesh must go: faces nobody sees, a piece built in the wrong place, every triangle of one material id before it is rebuilt.'],
    whenNotToUse: ['A solid must be cut by another shape: use boolean_subtract.', 'Only the material must change: use set_material_id.'],
    inputProps: { actorName: P.actorName, triangleIndices: P.triangleIndices, region: P.region },
    required: ['actorName'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    outputProps: {
      actorName: P.actorName,
      trianglesRemoved: int('Triangles deleted.'),
      vertexCount: OUT_VERTEX_COUNT,
      triangleCount: OUT_TRIANGLE_COUNT,
      materialIdsInUse: { type: 'array', items: { type: 'integer' }, description: 'Every material id the mesh carries now, ascending.' },
    },
    outputRequired: ['actorName', 'trianglesRemoved', 'vertexCount', 'triangleCount', 'materialIdsInUse'],
    exampleInput: { action: 'delete_triangles', actorName: 'DM_Ship', region: { materialIds: [9, 10, 11] } },
    exampleOutput: { success: true, actorName: 'DM_Ship', trianglesRemoved: 1656, vertexCount: 812, triangleCount: 1248, materialIdsInUse: [0, 1, 2, 3, 4, 5, 6, 7, 8] },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'set_material_id', plugins: PLUGIN,
    topics: ['assign a material to faces', 'material slots on a mesh', 'multi material mesh', 'material per face', 'paint faces with a material'],
    family: F, summary: 'Set the material id (the static-mesh material slot) of triangles of a DynamicMesh actor: every triangle, the triangleIndices you list, or a region.',
    whenToUse: ['One mesh needs several materials: give each part an id (0 is the default), then bake with convert_to_static_mesh or convert_to_nanite and list one material per id in materials; entry i is the material of id i.',
      'Triangles must be picked by where they are: region.box, region.normal (the top faces are {x:0,y:0,z:1}), region.groupIds or region.materialIds.'],
    whenNotToUse: ['A smooth mask for a material to read is needed rather than a slot; use bake_vertex_colors.'],
    inputProps: { actorName: P.actorName, materialId: { type: 'integer', minimum: 0, maximum: 255, description: 'Material id to set, 0 to 255. The baked asset gets the highest id used + 1 slots.' }, triangleIndices: P.triangleIndices, region: P.region },
    required: ['actorName', 'materialId'], effect: 'write',
    behavior: { idempotency: 'idempotent' }, costLatency: 'interactive', costResources: 'low',
    outputProps: {
      actorName: P.actorName,
      materialId: int('Material id that was set.'),
      trianglesSelected: int('Triangles that received the id.'),
      triangleCount: OUT_TRIANGLE_COUNT,
      materialIdsInUse: { type: 'array', items: { type: 'integer' }, description: 'Every material id the mesh carries now, ascending.' },
    },
    outputRequired: ['actorName', 'materialId', 'trianglesSelected', 'triangleCount', 'materialIdsInUse'],
    exampleInput: { action: 'set_material_id', actorName: 'DM_Cage', materialId: 1, region: { normal: { x: 0, y: 0, z: 1 }, normalAngle: 30 } },
    exampleOutput: { success: true, actorName: 'DM_Cage', materialId: 1, trianglesSelected: 2, triangleCount: 12, materialIdsInUse: [0, 1] },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'bake_vertex_colors', plugins: PLUGIN,
    topics: ['ambient occlusion mask', 'edge wear mask', 'cavity mask', 'height gradient mask', 'bake masks for materials'],
    family: F,
    summary: 'Bake ambient occlusion, convex-edge, concave-cavity and height masks into the vertex colours of a DynamicMesh actor, for a material to read through its VertexColor node.',
    whenToUse: ['A material needs masks (dirt in crevices, wear on edges, a height gradient) without textures: bake once the shape is final, then convert_to_static_mesh or convert_to_nanite.',
      'Channels: R is ambient occlusion (1 open, 0 occluded); G is 1 minus the convex edge (1 on flat or concave surface, lower along an outward fold such as a rim); B is 1 minus the concave cavity (1 on flat or convex surface, lower in a crease); A is height (0 at the bottom of the bounds, 1 at the top, along local Z).',
      'White means no effect for every channel except height, so a mesh that was never baked, which reads (1, 1, 1, 1), behaves like one whose masks are all quiet.',
      'The values reach the static mesh unchanged: the conversion cancels the sRGB encoding the static-mesh build applies (UE 5.1 and later), so a material\'s VertexColor node reads the baked value and a stored byte is the value times 255.'],
    whenNotToUse: ['One flat colour on every vertex is wanted; use set_vertex_color.', 'The mesh will be rebuilt afterwards by morphology or remesh_voxel, which drop vertex colours; bake after them.'],
    inputProps: {
      actorName: P.actorName,
      aoRays: { type: 'integer', description: 'Hemisphere rays per vertex for occlusion (default 32, clamped to 8-256); the bake cuts them on a very dense mesh to keep the call short, and the reply says how many it used.' },
      aoDistance: { type: 'number', description: 'How far an occluder can be, in cm (default 15% of the bounds diagonal); nearer geometry darkens R, farther geometry is ignored.' },
      curvatureScale: { type: 'number', description: 'Strength of the G and B masks (default 1): a fold of a quarter turn (90 degrees) is a full edge or cavity at 1; use 2 to make a 45 degree fold full, 0 to switch both masks off.' },
      blurIterations: { type: 'integer', description: 'Neighbour-averaging passes over R, G and B (default 1, 0 to 16); 0 keeps the masks per vertex and noisy.' },
    },
    required: ['actorName'], effect: 'write', behavior: { idempotency: 'idempotent' }, costLatency: 'interactive', costResources: 'medium',
    outputProps: {
      actorName: P.actorName,
      vertexCount: OUT_VERTEX_COUNT,
      aoRays: int('Rays per vertex that were traced.'),
      aoDistance: { type: 'number', description: 'Occlusion distance used, in cm.' },
      curvatureScale: { type: 'number', description: 'Curvature scale used.' },
      blurIterations: int('Blur passes that ran.'),
      averages: { type: 'object', description: 'Mean of each channel over the vertices, {r, g, b, a}, to see at a glance whether a mask did anything.', properties: { r: { type: 'number' }, g: { type: 'number' }, b: { type: 'number' }, a: { type: 'number' } }, additionalProperties: false },
      channels: { type: 'string', description: 'What each channel holds.' },
    },
    outputRequired: ['actorName', 'vertexCount', 'aoRays', 'aoDistance'],
    exampleInput: { action: 'bake_vertex_colors', actorName: 'DM_Cage', aoRays: 64, blurIterations: 2 },
    exampleOutput: { success: true, actorName: 'DM_Cage', vertexCount: 386, aoRays: 64, aoDistance: 25.4, curvatureScale: 1, blurIterations: 2 },
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
