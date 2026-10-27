#!/usr/bin/env node
/**
 * manage_geometry voxel suite: morphology (dilate, contract, close, open) and the voxel wrap of remesh_voxel.
 *
 * morphology offsets the surface through a signed-distance grid of voxelCount cells along the longest side;
 * close is the fillet that blends where unioned parts meet. remesh_voxel wraps the mesh in one watertight
 * surface. Both rebuild the surface, and both put the mesh back when the result is empty.
 */

import { runToolTests } from '../../test-runner.mjs';

const ts = Date.now();
const SETUP = `DM_VoxelSetup_${ts}`;
const VOXEL = `DM_Voxel_${ts}`;
const WRAP = `DM_VoxelWrap_${ts}`;
const CLAMP = `DM_VoxelClamp_${ts}`;

const CUBE_VERTICES = [
  { x: -50, y: -50, z: -50 }, { x: 50, y: -50, z: -50 }, { x: 50, y: 50, z: -50 }, { x: -50, y: 50, z: -50 },
  { x: -50, y: -50, z: 50 }, { x: 50, y: -50, z: 50 }, { x: 50, y: 50, z: 50 }, { x: -50, y: 50, z: 50 },
];
const CUBE_FACES = [[0, 3, 2, 1], [4, 5, 6, 7], [0, 1, 5, 4], [1, 2, 6, 5], [2, 3, 7, 6], [3, 0, 4, 7]];

const create = (name) => ({ action: 'create_procedural_mesh', name, actorName: name });
const cage = (actorName) => ({ action: 'append_polygons', actorName, vertices: CUBE_VERTICES, faces: CUBE_FACES });

const testCases = [
  // === SETUP ===
  { scenario: 'Setup: first edit_dynamic_mesh case, a throwaway mesh (its fold twin repeats it)', toolName: 'manage_geometry', arguments: create(SETUP), expected: 'success' },
  ...[VOXEL, WRAP, CLAMP].map((name) => ({ scenario: `Setup: create ${name}`, toolName: 'manage_geometry', arguments: create(name), expected: 'success' })),
  ...[VOXEL, WRAP, CLAMP].map((name) => ({ scenario: `Setup: a cube on ${name}`, toolName: 'manage_geometry', arguments: cage(name), expected: 'success' })),

  // === MORPHOLOGY ===
  {
    scenario: 'MORPH: close fillets a cube', toolName: 'manage_geometry', arguments: { action: 'morphology', actorName: VOXEL, operation: 'close', distance: 4, voxelCount: 64 },
    expected: 'success', timeoutMs: 30000,
    assertions: [{ path: 'structuredContent.result.voxelCount', equals: 64, label: 'grid resolution used' }, { path: 'structuredContent.result.operation', equals: 'close', label: 'operation reported' }],
  },
  { scenario: 'MORPH: dilate grows the surface', toolName: 'manage_geometry', arguments: { action: 'morphology', actorName: VOXEL, operation: 'dilate', distance: 3, voxelCount: 64 }, expected: 'success', timeoutMs: 30000 },
  { scenario: 'MORPH: contract shrinks the surface', toolName: 'manage_geometry', arguments: { action: 'morphology', actorName: VOXEL, operation: 'contract', distance: 2, voxelCount: 64 }, expected: 'success', timeoutMs: 30000 },
  { scenario: 'MORPH: open trims small features', toolName: 'manage_geometry', arguments: { action: 'morphology', actorName: VOXEL, operation: 'open', distance: 2, voxelCount: 64 }, expected: 'success', timeoutMs: 30000 },
  { scenario: 'MORPH: operation, distance and voxelCount default', toolName: 'manage_geometry', arguments: { action: 'morphology', actorName: VOXEL }, expected: 'success', timeoutMs: 30000 },
  {
    scenario: 'MORPH: a voxelCount below the minimum is clamped to 16 cells', toolName: 'manage_geometry', arguments: { action: 'morphology', actorName: CLAMP, operation: 'dilate', distance: 8, voxelCount: 1 },
    expected: 'success', timeoutMs: 30000, assertions: [{ path: 'structuredContent.result.voxelCount', equals: 16, label: 'clamped to the minimum' }],
  },
  { scenario: 'MORPH: a contract past the thickness leaves nothing and the mesh is kept', toolName: 'manage_geometry', arguments: { action: 'morphology', actorName: VOXEL, operation: 'contract', distance: 900, voxelCount: 32 }, expected: 'error|OPERATION_EMPTY', timeoutMs: 30000 },
  { scenario: 'MORPH: a missing actor is refused', toolName: 'manage_geometry', arguments: { action: 'morphology', actorName: `NoSuchMesh_${ts}` }, expected: 'error|ACTOR_NOT_FOUND' },

  // === VOXEL REMESH ===
  {
    scenario: 'VOXEL: remesh_voxel wraps the mesh on a voxelCount grid', toolName: 'manage_geometry', arguments: { action: 'remesh_voxel', actorName: WRAP, voxelCount: 48 },
    expected: 'success', timeoutMs: 30000, assertions: [{ path: 'structuredContent.result.trianglesAfter', gte: 1, label: 'a wrapped surface' }],
  },
  { scenario: 'VOXEL: remesh_voxel takes a target edge length as the grid cell', toolName: 'manage_geometry', arguments: { action: 'remesh_voxel', actorName: WRAP, targetEdgeLength: 4 }, expected: 'success', timeoutMs: 30000 },
  { scenario: 'VOXEL: remesh_voxel turns a triangle budget into a grid cell', toolName: 'manage_geometry', arguments: { action: 'remesh_voxel', actorName: WRAP, targetTriangleCount: 2000 }, expected: 'success', timeoutMs: 30000 },

  // === CLEANUP ===
  // The first edit_dynamic_mesh case's fold twin created a second actor under the SETUP label, so it is deleted twice.
  { scenario: 'Cleanup: delete the fold twin copy of the setup mesh', toolName: 'control_actor', arguments: { action: 'delete', actorName: SETUP }, expected: 'success|not found' },
  ...[SETUP, VOXEL, WRAP, CLAMP].map((actorName) => (
    { scenario: `Cleanup: delete ${actorName}`, toolName: 'control_actor', arguments: { action: 'delete', actorName }, expected: 'success|not found' }
  )),
];

runToolTests('manage-geometry-voxel', testCases);
