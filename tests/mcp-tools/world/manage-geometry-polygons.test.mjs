#!/usr/bin/env node
/**
 * manage_geometry append_polygons suite: author a polygon cage in one call.
 *
 * Covers the happy path (a six-quad cube, one polygroup per face by default), every parameter the record
 * declares (vertices, faces, faceGroups, faceMaterials), the ear clipping of a concave face, and the refusals a
 * bad cage owes its author: each names the face at fault and leaves the mesh as it was. The cage builders that
 * share its validation (append_cuboids, append_loft), copies of an asset (append_mesh) and delete_triangles
 * follow.
 */

import { runToolTests } from '../../test-runner.mjs';

const ts = Date.now();
const SETUP = `DM_PolySetup_${ts}`;
const CAGE = `DM_PolyCage_${ts}`;
const GROUPED = `DM_PolyGrouped_${ts}`;
const SCRATCH = `DM_PolyScratch_${ts}`;
const KIT = `DM_PolyKit_${ts}`;

// A cube cage, 100 cm, wound so (v1 - v0) x (v2 - v0) points outward on every face and shared edges run opposite ways.
const CUBE_VERTICES = [
  { x: -50, y: -50, z: -50 }, { x: 50, y: -50, z: -50 }, { x: 50, y: 50, z: -50 }, { x: -50, y: 50, z: -50 },
  { x: -50, y: -50, z: 50 }, { x: 50, y: -50, z: 50 }, { x: 50, y: 50, z: 50 }, { x: -50, y: 50, z: 50 },
];
const CUBE_FACES = [[0, 3, 2, 1], [4, 5, 6, 7], [0, 1, 5, 4], [1, 2, 6, 5], [2, 3, 7, 6], [3, 0, 4, 7]];
// An L-shaped hexagon: concave, so a fan would overlap and only ear clipping triangulates it.
const L_VERTICES = [{ x: 0, y: 0, z: 0 }, { x: 100, y: 0, z: 0 }, { x: 100, y: 50, z: 0 }, { x: 50, y: 50, z: 0 }, { x: 50, y: 100, z: 0 }, { x: 0, y: 100, z: 0 }];

const cage = (actorName, extra = {}) => ({ action: 'append_polygons', actorName, vertices: CUBE_VERTICES, faces: CUBE_FACES, ...extra });
const create = (name) => ({ action: 'create_procedural_mesh', name, actorName: name });

const testCases = [
  // === SETUP ===
  { scenario: 'Setup: first edit_dynamic_mesh case, a throwaway mesh (its fold twin repeats it)', toolName: 'manage_geometry', arguments: create(SETUP), expected: 'success' },
  { scenario: 'Setup: create the cage actor', toolName: 'manage_geometry', arguments: create(CAGE), expected: 'success' },
  { scenario: 'Setup: create the grouped actor', toolName: 'manage_geometry', arguments: create(GROUPED), expected: 'success' },
  { scenario: 'Setup: create the scratch actor', toolName: 'manage_geometry', arguments: create(SCRATCH), expected: 'success' },

  // === APPEND: a cube cage in one call, one polygroup per face by default ===
  {
    scenario: 'APPEND: append_polygons authors a six-quad cube cage in one call', toolName: 'manage_geometry', arguments: cage(CAGE), expected: 'success',
    assertions: [
      { path: 'structuredContent.result.verticesAdded', equals: 8, label: 'every vertex appended' },
      { path: 'structuredContent.result.facesAdded', equals: 6, label: 'every face appended' },
      { path: 'structuredContent.result.trianglesAdded', equals: 12, label: 'each quad is two triangles' },
      { path: 'structuredContent.result.vertexCount', equals: 8, label: 'the mesh holds the eight points' },
      { path: 'structuredContent.result.triangleCount', equals: 12, label: 'the mesh holds the twelve triangles' },
      { path: 'structuredContent.result.groupCount', equals: 6, label: 'one polygroup per face' },
    ],
  },
  {
    scenario: 'APPEND: faceGroups and faceMaterials set a polygroup and a material id per face', toolName: 'manage_geometry',
    arguments: cage(GROUPED, { faceGroups: [0, 1, 2, 3, 4, 5], faceMaterials: [0, 0, 1, 1, 2, 2] }), expected: 'success',
    assertions: [{ path: 'structuredContent.result.groupCount', equals: 6, label: 'the six given groups' }],
  },
  {
    scenario: 'APPEND: faces that share a polygroup id are one flat region', toolName: 'manage_geometry',
    arguments: cage(SCRATCH, { faceGroups: [0, 0, 1, 1, 1, 1] }), expected: 'success',
    assertions: [{ path: 'structuredContent.result.groupCount', equals: 2, label: 'two regions' }],
  },
  {
    scenario: 'APPEND: a concave L-shaped face is ear clipped into n - 2 triangles', toolName: 'manage_geometry',
    arguments: { action: 'append_polygons', actorName: SETUP, vertices: L_VERTICES, faces: [[0, 1, 2, 3, 4, 5]] }, expected: 'success',
    assertions: [{ path: 'structuredContent.result.trianglesAdded', equals: 4, label: 'a hexagon is four triangles' }],
  },

  {
    scenario: 'APPEND: points no face uses are left out, not added as isolated vertices', toolName: 'manage_geometry',
    arguments: { action: 'append_polygons', actorName: SETUP, vertices: CUBE_VERTICES, faces: [[0, 3, 2]] }, expected: 'success',
    assertions: [{ path: 'structuredContent.result.verticesAdded', equals: 3, label: 'only the three corners the face uses' }],
  },

  // === REFUSALS: nothing is appended, and the message names the face ===
  { scenario: 'APPEND: a vertex index outside vertices is refused', toolName: 'manage_geometry', arguments: { action: 'append_polygons', actorName: SETUP, vertices: CUBE_VERTICES, faces: [[0, 1, 99]] }, expected: 'error|INVALID_POLYGONS' },
  { scenario: 'APPEND: a face that repeats a corner is refused', toolName: 'manage_geometry', arguments: { action: 'append_polygons', actorName: SETUP, vertices: CUBE_VERTICES, faces: [[0, 0, 1]] }, expected: 'error|INVALID_POLYGONS' },
  { scenario: 'APPEND: two faces wound the same way along a shared edge are refused', toolName: 'manage_geometry', arguments: { action: 'append_polygons', actorName: SETUP, vertices: CUBE_VERTICES, faces: [[0, 1, 2], [0, 1, 3]] }, expected: 'error|INVALID_POLYGONS' },
  { scenario: 'APPEND: faceGroups of the wrong length is refused', toolName: 'manage_geometry', arguments: cage(SETUP, { faceGroups: [0, 1] }), expected: 'error|INVALID_POLYGONS' },
  { scenario: 'APPEND: faceMaterials of the wrong length is refused', toolName: 'manage_geometry', arguments: cage(SETUP, { faceMaterials: [0] }), expected: 'error|INVALID_POLYGONS' },
  { scenario: 'APPEND: a missing actor is refused', toolName: 'manage_geometry', arguments: cage(`NoSuchMesh_${ts}`), expected: 'error|ACTOR_NOT_FOUND' },

  // === BOXES: many boxes in one call, wound outward, repeated along up to three axes ===
  {
    scenario: 'BOXES: append_cuboids lays a railing of 11 posts (bottoms open) and one rail', toolName: 'manage_geometry',
    arguments: {
      action: 'append_cuboids', actorName: SCRATCH,
      boxes: [
        { min: { x: -2, y: -2, z: 0 }, max: { x: 2, y: 2, z: 100 }, open: ['-z'], repeat: { count: 11, offset: { x: 200 } } },
        { min: { x: 0, y: -2, z: 96 }, max: { x: 2000, y: 2, z: 100 }, materialId: 1 },
      ],
    },
    expected: 'success',
    assertions: [
      { path: 'structuredContent.result.facesAdded', equals: 61, label: 'five faces per post and six for the rail' },
      { path: 'structuredContent.result.verticesAdded', equals: 96, label: 'eight corners per box' },
    ],
  },
  {
    scenario: 'BOXES: a 2 x 2 x 2 block with rows, layers and a material cycle', toolName: 'manage_geometry',
    arguments: {
      action: 'append_cuboids', actorName: SCRATCH,
      boxes: [{ min: { x: 0, y: 500, z: 0 }, max: { x: 1220, y: 744, z: 259 }, materialIds: [0, 1, 2],
        repeat: { count: 2, offset: { x: 1240 }, rows: 2, rowOffset: { y: 254 }, layers: 2, layerOffset: { z: 262 } } }],
    },
    expected: 'success',
    assertions: [{ path: 'structuredContent.result.facesAdded', equals: 48, label: 'eight boxes of six faces' }],
  },
  { scenario: 'BOXES: a box whose max is not larger than its min is refused', toolName: 'manage_geometry', arguments: { action: 'append_cuboids', actorName: SCRATCH, boxes: [{ min: { x: 0, y: 0, z: 0 }, max: { x: 0, y: 10, z: 10 } }] }, expected: 'error|INVALID_CUBOIDS' },
  { scenario: 'BOXES: a box with every face open is refused', toolName: 'manage_geometry', arguments: { action: 'append_cuboids', actorName: SCRATCH, boxes: [{ min: { x: 0, y: 0, z: 0 }, max: { x: 10, y: 10, z: 10 }, open: ['-x', '+x', '-y', '+y', '-z', '+z'] }] }, expected: 'error|INVALID_CUBOIDS' },

  // === LOFT: a surface through cross sections, every face turned outward ===
  {
    scenario: 'LOFT: append_loft joins two sections and closes the second to a tip', toolName: 'manage_geometry',
    arguments: {
      action: 'append_loft', actorName: GROUPED,
      sections: [
        [{ x: 0, y: 400, z: 300 }, { x: 0, y: 400, z: -200 }, { x: 0, y: -400, z: -200 }, { x: 0, y: -400, z: 300 }],
        [{ x: 800, y: 300, z: 300 }, { x: 800, y: 200, z: -150 }, { x: 800, y: -200, z: -150 }, { x: 800, y: -300, z: 300 }],
      ],
      endPoint: { x: 1200, y: 0, z: 300 },
    },
    expected: 'success',
    assertions: [
      { path: 'structuredContent.result.facesAdded', equals: 9, label: 'four sides, one cap and a four-triangle tip' },
      { path: 'structuredContent.result.trianglesAdded', equals: 14, label: 'quads are two triangles each' },
    ],
  },
  {
    scenario: 'LOFT: both ends to tips, with a material per section edge and for the tips', toolName: 'manage_geometry',
    arguments: {
      action: 'append_loft', actorName: GROUPED,
      sections: [
        [{ x: 2000, y: 50, z: 50 }, { x: 2000, y: 50, z: -50 }, { x: 2000, y: -50, z: -50 }, { x: 2000, y: -50, z: 50 }],
        [{ x: 2200, y: 80, z: 80 }, { x: 2200, y: 80, z: -80 }, { x: 2200, y: -80, z: -80 }, { x: 2200, y: -80, z: 80 }],
        [{ x: 2400, y: 50, z: 50 }, { x: 2400, y: 50, z: -50 }, { x: 2400, y: -50, z: -50 }, { x: 2400, y: -50, z: 50 }],
      ],
      startPoint: { x: 1900, y: 0, z: 0 }, endPoint: { x: 2500, y: 0, z: 0 }, edgeMaterials: [0, 1, 0, 1], capMaterialId: 2,
    },
    expected: 'success',
    assertions: [{ path: 'structuredContent.result.facesAdded', equals: 16, label: 'eight sides and two four-triangle tips' }],
  },
  {
    scenario: 'LOFT: an open sheet (closed false), turned with flip, and a tube left uncapped (caps false)', toolName: 'manage_geometry',
    arguments: {
      action: 'append_loft', actorName: SETUP, closed: false, flip: true,
      sections: [[{ x: 0, y: 0, z: 500 }, { x: 0, y: 100, z: 550 }, { x: 0, y: 200, z: 500 }], [{ x: 300, y: 0, z: 500 }, { x: 300, y: 100, z: 550 }, { x: 300, y: 200, z: 500 }]],
    },
    expected: 'success',
    assertions: [{ path: 'structuredContent.result.facesAdded', equals: 2, label: 'one quad per edge of the open polyline' }],
  },
  {
    scenario: 'LOFT: caps false leaves a closed tube open at both ends', toolName: 'manage_geometry',
    arguments: {
      action: 'append_loft', actorName: SETUP, caps: false,
      sections: [[{ x: 0, y: 0, z: 800 }, { x: 0, y: 100, z: 800 }, { x: 0, y: 50, z: 900 }], [{ x: 200, y: 0, z: 800 }, { x: 200, y: 100, z: 800 }, { x: 200, y: 50, z: 900 }]],
    },
    expected: 'success',
    assertions: [{ path: 'structuredContent.result.facesAdded', equals: 3, label: 'three sides, no caps' }],
  },
  { scenario: 'LOFT: sections of different point counts are refused', toolName: 'manage_geometry', arguments: { action: 'append_loft', actorName: SETUP, sections: [[{ x: 0, y: 0, z: 0 }, { x: 0, y: 1, z: 0 }, { x: 0, y: 0, z: 1 }], [{ x: 1, y: 0, z: 0 }, { x: 1, y: 1, z: 0 }]] }, expected: 'error|INVALID_LOFT' },
  { scenario: 'LOFT: a tip point on an open sheet is refused', toolName: 'manage_geometry', arguments: { action: 'append_loft', actorName: SETUP, closed: false, endPoint: { x: 5, y: 0, z: 0 }, sections: [[{ x: 0, y: 0, z: 0 }, { x: 0, y: 1, z: 0 }], [{ x: 1, y: 0, z: 0 }, { x: 1, y: 1, z: 0 }]] }, expected: 'error|INVALID_LOFT' },

  // === MESH: copies of a static mesh asset, repeated, their slots mapped to cycling ids ===
  { scenario: 'Setup: create the kitbash actor', toolName: 'manage_geometry', arguments: create(KIT), expected: 'success' },
  {
    scenario: 'MESH: append_mesh lays 3 x 2 engine cubes, the slot cycling ids 3 and 4', toolName: 'manage_geometry',
    arguments: { action: 'append_mesh', actorName: KIT, meshPath: '/Engine/BasicShapes/Cube',
      placements: [{ location: { x: 0, y: 0, z: 50 }, materialIds: [[3, 4]], repeat: { count: 3, offset: { x: 150 }, rows: 2, rowOffset: { y: 150 } } }] },
    expected: 'success',
    assertions: [
      { path: 'structuredContent.result.copies', equals: 6, label: 'count x rows copies' },
      { path: 'structuredContent.result.sourceSlots', equals: 1, label: 'the cube has one material slot' },
      { path: 'structuredContent.result.trianglesAdded', equals: 72, label: 'twelve triangles a cube' },
      { path: 'structuredContent.result.materialIdsInUse', length: 2, label: 'copies alternate between two ids' },
      { path: 'structuredContent.result.materialIdsInUse.1', equals: 4, label: 'the second of them is 4' },
    ],
  },
  {
    scenario: 'MESH: a turned, mirrored copy (scale x -1) of the mesh as it draws (lod render) still appends', toolName: 'manage_geometry',
    arguments: { action: 'append_mesh', actorName: KIT, meshPath: '/Engine/BasicShapes/Cube', lod: 'render',
      placements: [{ location: { x: 0, y: 600, z: 50 }, rotation: { pitch: 0, yaw: 45, roll: 0 }, scale: { x: -1, y: 2, z: 1 } }] },
    expected: 'success',
    assertions: [{ path: 'structuredContent.result.copies', equals: 1, label: 'one copy' }],
  },
  { scenario: 'MESH: materialIds with an entry count other than the slot count is refused', toolName: 'manage_geometry', arguments: { action: 'append_mesh', actorName: KIT, meshPath: '/Engine/BasicShapes/Cube', placements: [{ location: { x: 0, y: 0, z: 0 }, materialIds: [1, 2] }] }, expected: 'error|INVALID_MESH_PLACEMENT' },
  { scenario: 'MESH: a mesh path that names no static mesh is refused', toolName: 'manage_geometry', arguments: { action: 'append_mesh', actorName: KIT, meshPath: `/Game/NoSuchMesh_${ts}`, placements: [{ location: { x: 0, y: 0, z: 0 } }] }, expected: 'error|ASSET_NOT_FOUND' },

  // === DELETE: the triangles a selection picks go, never the whole mesh ===
  {
    scenario: 'DELETE: delete_triangles by region removes the copies on id 4', toolName: 'manage_geometry',
    arguments: { action: 'delete_triangles', actorName: KIT, region: { materialIds: [4] } }, expected: 'success',
    assertions: [{ path: 'structuredContent.result.trianglesRemoved', equals: 36, label: 'three cubes of twelve triangles' }],
  },
  {
    scenario: 'DELETE: delete_triangles by triangleIndices removes the two named', toolName: 'manage_geometry',
    arguments: { action: 'delete_triangles', actorName: CAGE, triangleIndices: [0, 1] }, expected: 'success',
    assertions: [{ path: 'structuredContent.result.trianglesRemoved', equals: 2, label: 'the two listed triangles' }],
  },
  { scenario: 'DELETE: no selection is refused', toolName: 'manage_geometry', arguments: { action: 'delete_triangles', actorName: KIT }, expected: 'error|INVALID_SELECTION' },

  // === CLEANUP ===
  // The first edit_dynamic_mesh case's fold twin created a second actor under the SETUP label, so it is deleted twice.
  { scenario: 'Cleanup: delete the fold twin copy of the setup mesh', toolName: 'control_actor', arguments: { action: 'delete', actorName: SETUP }, expected: 'success|not found' },
  ...[SETUP, CAGE, GROUPED, SCRATCH, KIT].map((actorName) => (
    { scenario: `Cleanup: delete ${actorName}`, toolName: 'control_actor', arguments: { action: 'delete', actorName }, expected: 'success|not found' }
  )),
];

runToolTests('manage-geometry-polygons', testCases);
