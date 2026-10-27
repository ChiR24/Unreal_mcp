#!/usr/bin/env node
/**
 * manage_geometry subdivision suite: subdivide with scheme catmull_clark, loop, bilinear or pn.
 *
 * catmull_clark and bilinear read every polygroup as one face of a cage, so the cages here are an
 * append_polygons cube and a create_box primitive (one polygroup per face). loop refines triangles and needs no
 * cage; pn is the default and keeps its legacy reply. A closed surface that is a single polygroup is no cage and
 * is refused with how to build one, and a level past the triangle cap is refused before anything is refined.
 */

import { runToolTests } from '../../test-runner.mjs';

const ts = Date.now();
const SETUP = `DM_SubdivSetup_${ts}`;
const CAGE = `DM_SubdivCage_${ts}`;
const CAGE_LOOP = `DM_SubdivLoop_${ts}`;
const CAGE_BILINEAR = `DM_SubdivBilinear_${ts}`;
const CAGE_PN = `DM_SubdivPn_${ts}`;
const CAGE_FLAT = `DM_SubdivFlat_${ts}`;
const BOX_CAGE = `DM_SubdivBox_${ts}`;

const CUBE_VERTICES = [
  { x: -50, y: -50, z: -50 }, { x: 50, y: -50, z: -50 }, { x: 50, y: 50, z: -50 }, { x: -50, y: 50, z: -50 },
  { x: -50, y: -50, z: 50 }, { x: 50, y: -50, z: 50 }, { x: 50, y: 50, z: 50 }, { x: -50, y: 50, z: 50 },
];
const CUBE_FACES = [[0, 3, 2, 1], [4, 5, 6, 7], [0, 1, 5, 4], [1, 2, 6, 5], [2, 3, 7, 6], [3, 0, 4, 7]];
// A tetrahedron as one polygroup: a closed surface with no face boundaries, which is no cage for catmull_clark.
const TETRA_VERTICES = [{ x: 0, y: 0, z: 0 }, { x: 100, y: 0, z: 0 }, { x: 0, y: 100, z: 0 }, { x: 0, y: 0, z: 100 }];
const TETRA_FACES = [[0, 2, 1], [0, 1, 3], [0, 3, 2], [1, 2, 3]];

const create = (name) => ({ action: 'create_procedural_mesh', name, actorName: name });
const cage = (actorName) => ({ action: 'append_polygons', actorName, vertices: CUBE_VERTICES, faces: CUBE_FACES });

const testCases = [
  // === SETUP ===
  { scenario: 'Setup: first edit_dynamic_mesh case, a throwaway mesh (its fold twin repeats it)', toolName: 'manage_geometry', arguments: create(SETUP), expected: 'success' },
  ...[CAGE, CAGE_LOOP, CAGE_BILINEAR, CAGE_PN, CAGE_FLAT].map((name) => (
    { scenario: `Setup: create ${name}`, toolName: 'manage_geometry', arguments: create(name), expected: 'success' }
  )),
  ...[CAGE, CAGE_LOOP, CAGE_BILINEAR, CAGE_PN].map((name) => (
    { scenario: `Setup: a cube cage on ${name}`, toolName: 'manage_geometry', arguments: cage(name), expected: 'success' }
  )),
  { scenario: 'Setup: a tetrahedron given one polygroup is a closed surface with no faces to subdivide', toolName: 'manage_geometry', arguments: { action: 'append_polygons', actorName: CAGE_FLAT, vertices: TETRA_VERTICES, faces: TETRA_FACES, faceGroups: [0, 0, 0, 0] }, expected: 'success' },
  { scenario: 'Setup: a box primitive has one polygroup per face, so it is a cage already', toolName: 'manage_geometry', arguments: { action: 'create_box', name: BOX_CAGE, width: 100, height: 100, depth: 100 }, expected: 'success' },

  // === SUBDIVIDE: catmull_clark, loop, bilinear, pn ===
  {
    scenario: 'SUBDIVIDE: catmull_clark rounds a cube cage and quadruples its six quads', toolName: 'manage_geometry',
    arguments: { action: 'subdivide', actorName: CAGE, scheme: 'catmull_clark', iterations: 1 }, expected: 'success',
    assertions: [
      { path: 'structuredContent.result.scheme', equals: 'catmull_clark', label: 'scheme reported' },
      { path: 'structuredContent.result.level', equals: 1, label: 'level reported' },
      { path: 'structuredContent.result.cageFaces', equals: 6, label: 'six cage faces' },
      { path: 'structuredContent.result.trianglesBefore', equals: 12, label: 'twelve triangles in' },
      { path: 'structuredContent.result.trianglesAfter', equals: 48, label: 'twenty-four quads out' },
    ],
  },
  {
    scenario: 'SUBDIVIDE: catmull_clark on a create_box primitive', toolName: 'manage_geometry',
    arguments: { action: 'subdivide', actorName: BOX_CAGE, scheme: 'catmull_clark', iterations: 2 }, expected: 'success',
    assertions: [{ path: 'structuredContent.result.trianglesAfter', equals: 192, label: 'six quads at level two' }],
  },
  {
    scenario: 'SUBDIVIDE: loop subdivides the triangles as they are', toolName: 'manage_geometry',
    arguments: { action: 'subdivide', actorName: CAGE_LOOP, scheme: 'loop', iterations: 1 }, expected: 'success',
    assertions: [{ path: 'structuredContent.result.trianglesAfter', equals: 48, label: 'each triangle becomes four' }],
  },
  {
    scenario: 'SUBDIVIDE: bilinear cuts the cage into quads without smoothing', toolName: 'manage_geometry',
    arguments: { action: 'subdivide', actorName: CAGE_BILINEAR, scheme: 'bilinear', iterations: 1 }, expected: 'success',
    assertions: [{ path: 'structuredContent.result.trianglesAfter', equals: 48, label: 'twenty-four quads out' }],
  },
  {
    scenario: 'SUBDIVIDE: pn is still the default scheme and keeps its legacy reply', toolName: 'manage_geometry',
    arguments: { action: 'subdivide', actorName: CAGE_PN, scheme: 'pn', iterations: 1 }, expected: 'success',
    assertions: [
      { path: 'structuredContent.result.scheme', equals: 'pn', label: 'pn ran' },
      { path: 'structuredContent.result.trianglesAfter', gte: 12, label: 'refined' },
    ],
  },

  // === REFUSALS ===
  { scenario: 'SUBDIVIDE: catmull_clark on a closed single-polygroup surface is refused with how to build a cage', toolName: 'manage_geometry', arguments: { action: 'subdivide', actorName: CAGE_FLAT, scheme: 'catmull_clark' }, expected: 'error|INVALID_CAGE' },
  { scenario: 'SUBDIVIDE: loop needs no cage', toolName: 'manage_geometry', arguments: { action: 'subdivide', actorName: CAGE_FLAT, scheme: 'loop' }, expected: 'success' },
  { scenario: 'SUBDIVIDE: the triangle cap refuses a level that would exceed it', toolName: 'manage_geometry', arguments: { action: 'subdivide', actorName: BOX_CAGE, scheme: 'loop', iterations: 6 }, expected: 'error|POLYGON_LIMIT_EXCEEDED' },

  // === CLEANUP ===
  // The first edit_dynamic_mesh case's fold twin created a second actor under the SETUP label, so it is deleted twice.
  { scenario: 'Cleanup: delete the fold twin copy of the setup mesh', toolName: 'control_actor', arguments: { action: 'delete', actorName: SETUP }, expected: 'success|not found' },
  // create_box is a create_primitive member, so its fold twin made a second box under the BOX_CAGE label too.
  { scenario: 'Cleanup: delete the fold twin copy of the box', toolName: 'control_actor', arguments: { action: 'delete', actorName: BOX_CAGE }, expected: 'success|not found' },
  ...[SETUP, CAGE, CAGE_LOOP, CAGE_BILINEAR, CAGE_PN, CAGE_FLAT, BOX_CAGE].map((actorName) => (
    { scenario: `Cleanup: delete ${actorName}`, toolName: 'control_actor', arguments: { action: 'delete', actorName }, expected: 'success|not found' }
  )),
];

runToolTests('manage-geometry-subdivision', testCases);
