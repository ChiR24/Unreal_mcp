#!/usr/bin/env node
/**
 * manage_geometry append_polygons suite: author a polygon cage in one call.
 *
 * Covers the happy path (a six-quad cube, one polygroup per face by default), every parameter the record
 * declares (vertices, faces, faceGroups, faceMaterials), the ear clipping of a concave face, and the refusals a
 * bad cage owes its author: each names the face at fault and leaves the mesh as it was.
 */

import { runToolTests } from '../../test-runner.mjs';

const ts = Date.now();
const SETUP = `DM_PolySetup_${ts}`;
const CAGE = `DM_PolyCage_${ts}`;
const GROUPED = `DM_PolyGrouped_${ts}`;
const SCRATCH = `DM_PolyScratch_${ts}`;

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

  // === CLEANUP ===
  // The first edit_dynamic_mesh case's fold twin created a second actor under the SETUP label, so it is deleted twice.
  { scenario: 'Cleanup: delete the fold twin copy of the setup mesh', toolName: 'control_actor', arguments: { action: 'delete', actorName: SETUP }, expected: 'success|not found' },
  ...[SETUP, CAGE, GROUPED, SCRATCH].map((actorName) => (
    { scenario: `Cleanup: delete ${actorName}`, toolName: 'control_actor', arguments: { action: 'delete', actorName }, expected: 'success|not found' }
  )),
];

runToolTests('manage-geometry-polygons', testCases);
