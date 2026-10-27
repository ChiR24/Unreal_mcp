#!/usr/bin/env node
/**
 * manage_geometry region suite: pick triangles by where they are, for the face operators and set_material_id.
 *
 * `region` (box, normal with normalAngle, groupIds, materialIds, all ANDed, in the mesh's local space) takes the
 * place of triangleIndices on extrude, inset, outset, offset_faces, bevel, chamfer and poke, and on
 * set_material_id, whose materialId becomes the static-mesh material slot. Each reply says how many triangles it
 * selected, a region that matches nothing is REGION_EMPTY, and giving both selectors is refused.
 */

import { runToolTests } from '../../test-runner.mjs';

const ts = Date.now();
const SETUP = `DM_RegionSetup_${ts}`;
const CAGE = `DM_RegionCage_${ts}`;
const SCRATCH = `DM_RegionScratch_${ts}`;
const SHAPE = `DM_RegionShape_${ts}`;

const CUBE_VERTICES = [
  { x: -50, y: -50, z: -50 }, { x: 50, y: -50, z: -50 }, { x: 50, y: 50, z: -50 }, { x: -50, y: 50, z: -50 },
  { x: -50, y: -50, z: 50 }, { x: 50, y: -50, z: 50 }, { x: 50, y: 50, z: 50 }, { x: -50, y: 50, z: 50 },
];
const CUBE_FACES = [[0, 3, 2, 1], [4, 5, 6, 7], [0, 1, 5, 4], [1, 2, 6, 5], [2, 3, 7, 6], [3, 0, 4, 7]];

const create = (name) => ({ action: 'create_procedural_mesh', name, actorName: name });
const mat = (extra) => ({ action: 'set_material_id', actorName: CAGE, ...extra });

const testCases = [
  // === SETUP ===
  { scenario: 'Setup: first edit_dynamic_mesh case, a throwaway mesh (its fold twin repeats it)', toolName: 'manage_geometry', arguments: create(SETUP), expected: 'success' },
  { scenario: 'Setup: create the cage actor', toolName: 'manage_geometry', arguments: create(CAGE), expected: 'success' },
  { scenario: 'Setup: create the scratch actor', toolName: 'manage_geometry', arguments: create(SCRATCH), expected: 'success' },
  { scenario: 'Setup: a cube cage, six quads and twelve triangles', toolName: 'manage_geometry', arguments: { action: 'append_polygons', actorName: CAGE, vertices: CUBE_VERTICES, faces: CUBE_FACES }, expected: 'success' },
  { scenario: 'Setup: one flat triangle for the whole-mesh case', toolName: 'manage_geometry', arguments: { action: 'append_polygons', actorName: SCRATCH, vertices: CUBE_VERTICES, faces: [[0, 3, 2]] }, expected: 'success' },

  // === SET MATERIAL ID: every triangle, listed triangles, or a region ===
  {
    scenario: 'MATERIAL: set_material_id on the top faces by region.normal', toolName: 'manage_geometry',
    arguments: mat({ materialId: 1, region: { normal: { x: 0, y: 0, z: 1 }, normalAngle: 30 } }), expected: 'success',
    assertions: [{ path: 'structuredContent.result.trianglesSelected', equals: 2, label: 'the top quad is two triangles' }],
  },
  {
    scenario: 'MATERIAL: set_material_id on listed triangles', toolName: 'manage_geometry',
    arguments: mat({ materialId: 2, triangleIndices: [0, 1] }), expected: 'success',
    assertions: [{ path: 'structuredContent.result.trianglesSelected', equals: 2, label: 'both listed triangles' }],
  },
  {
    scenario: 'MATERIAL: set_material_id on the lower half by region.box', toolName: 'manage_geometry',
    arguments: mat({ materialId: 3, region: { box: { min: { x: -100, y: -100, z: -100 }, max: { x: 100, y: 100, z: 0 } } } }), expected: 'success',
    assertions: [{ path: 'structuredContent.result.trianglesSelected', gte: 1, label: 'triangles whose centroid is below the middle' }],
  },
  {
    scenario: 'MATERIAL: set_material_id by region.groupIds and region.materialIds ANDed', toolName: 'manage_geometry',
    arguments: mat({ materialId: 3, region: { groupIds: [0, 1, 2, 3, 4, 5, 6], materialIds: [3] } }), expected: 'success',
    assertions: [{ path: 'structuredContent.result.materialIdsInUse', includes: 3, label: 'id 3 is on the mesh' }],
  },
  {
    scenario: 'MATERIAL: set_material_id with no selection sets every triangle', toolName: 'manage_geometry',
    arguments: { action: 'set_material_id', actorName: SCRATCH, materialId: 4 }, expected: 'success',
    assertions: [{ path: 'structuredContent.result.trianglesSelected', gte: 1, label: 'the whole mesh' }],
  },
  { scenario: 'MATERIAL: a region that matches nothing is refused with the local bounds', toolName: 'manage_geometry', arguments: mat({ materialId: 1, region: { box: { min: { x: 1000, y: 1000, z: 1000 }, max: { x: 2000, y: 2000, z: 2000 } } } }), expected: 'error|REGION_EMPTY' },
  { scenario: 'MATERIAL: triangleIndices and region together are refused', toolName: 'manage_geometry', arguments: mat({ materialId: 1, triangleIndices: [0], region: { groupIds: [0] } }), expected: 'error|INVALID_SELECTION' },
  { scenario: 'MATERIAL: an empty region is refused', toolName: 'manage_geometry', arguments: mat({ materialId: 1, region: {} }), expected: 'error|INVALID_REGION' },
  { scenario: 'MATERIAL: a missing materialId is refused', toolName: 'manage_geometry', arguments: { action: 'set_material_id', actorName: CAGE }, expected: 'error|MISSING_PARAMETER' },

  // === REGION on the face operators: extrude, inset, outset, offset_faces, bevel, chamfer, poke ===
  { scenario: 'Setup: create a box for the region operators', toolName: 'manage_geometry', arguments: { action: 'create_box', name: SHAPE, width: 100, height: 100, depth: 100 }, expected: 'success' },
  {
    scenario: 'REGION: extrude only the top faces', toolName: 'manage_geometry',
    arguments: { action: 'extrude', actorName: SHAPE, amount: 20, region: { normal: { x: 0, y: 0, z: 1 } } }, expected: 'success',
    assertions: [{ path: 'structuredContent.result.trianglesSelected', gte: 1, label: 'the top triangles' }],
  },
  {
    scenario: 'REGION: inset the faces inside a box', toolName: 'manage_geometry',
    arguments: { action: 'inset', actorName: SHAPE, distance: 3, region: { box: { min: { x: -200, y: -200, z: 20 }, max: { x: 200, y: 200, z: 200 } } } }, expected: 'success',
    assertions: [{ path: 'structuredContent.result.trianglesSelected', gte: 1, label: 'the raised triangles' }],
  },
  { scenario: 'REGION: outset the faces of material id 0', toolName: 'manage_geometry', arguments: { action: 'outset', actorName: SHAPE, distance: 1, region: { materialIds: [0] } }, expected: 'success', assertions: [{ path: 'structuredContent.result.trianglesSelected', gte: 1, label: 'every triangle is material 0' }] },
  { scenario: 'REGION: offset_faces facing +X', toolName: 'manage_geometry', arguments: { action: 'offset_faces', actorName: SHAPE, distance: 2, region: { normal: { x: 1, y: 0, z: 0 }, normalAngle: 20 } }, expected: 'success' },
  { scenario: 'REGION: bevel the faces facing +Y', toolName: 'manage_geometry', arguments: { action: 'bevel', actorName: SHAPE, distance: 2, region: { normal: { x: 0, y: 1, z: 0 }, normalAngle: 20 } }, expected: 'success' },
  { scenario: 'REGION: chamfer the faces of the box polygroups', toolName: 'manage_geometry', arguments: { action: 'chamfer', actorName: SHAPE, distance: 1, region: { groupIds: [0, 1, 2, 3, 4, 5, 6] } }, expected: 'success' },
  {
    scenario: 'REGION: poke the faces facing -Z', toolName: 'manage_geometry',
    arguments: { action: 'poke', actorName: SHAPE, distance: 5, region: { normal: { x: 0, y: 0, z: -1 }, normalAngle: 20 } }, expected: 'success',
    assertions: [{ path: 'structuredContent.result.trianglesSelected', gte: 1, label: 'the bottom triangles' }],
  },
  { scenario: 'REGION: an extrude region that matches nothing is refused', toolName: 'manage_geometry', arguments: { action: 'extrude', actorName: SHAPE, region: { box: { min: { x: 5000, y: 5000, z: 5000 }, max: { x: 6000, y: 6000, z: 6000 } } } }, expected: 'error|REGION_EMPTY' },
  { scenario: 'REGION: triangleIndices and region together are refused for an operator', toolName: 'manage_geometry', arguments: { action: 'inset', actorName: SHAPE, triangleIndices: [0, 1], region: { normal: { x: 0, y: 0, z: 1 } } }, expected: 'error|INVALID_SELECTION' },

  // === CLEANUP ===
  // The first edit_dynamic_mesh case's fold twin created a second actor under the SETUP label, so it is deleted twice.
  { scenario: 'Cleanup: delete the fold twin copy of the setup mesh', toolName: 'control_actor', arguments: { action: 'delete', actorName: SETUP }, expected: 'success|not found' },
  // create_box is a create_primitive member, so its fold twin made a second box under the SHAPE label too.
  { scenario: 'Cleanup: delete the fold twin copy of the box', toolName: 'control_actor', arguments: { action: 'delete', actorName: SHAPE }, expected: 'success|not found' },
  ...[SETUP, CAGE, SCRATCH, SHAPE].map((actorName) => (
    { scenario: `Cleanup: delete ${actorName}`, toolName: 'control_actor', arguments: { action: 'delete', actorName }, expected: 'success|not found' }
  )),
];

runToolTests('manage-geometry-region', testCases);
