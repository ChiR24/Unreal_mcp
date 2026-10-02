#!/usr/bin/env node
/**
 * manage_geometry bake_vertex_colors suite: AO, edge, cavity and height masks in the vertex colours.
 *
 * Every channel is a mask in 0..1 with white meaning "no effect", except height: R is ambient occlusion (1 open),
 * G is 1 minus the convex-edge mask, B is 1 minus the concave-cavity mask, A is height up the local Z. A cube is
 * the reference: every vertex is open to the sky (R = 1), every vertex sits on a quarter-turn convex edge (G = 0),
 * no fold is concave (B = 1), and half the vertices are at the bottom and half at the top (A averages 0.5).
 */

import { runToolTests } from '../../test-runner.mjs';

const ts = Date.now();
const SETUP = `DM_BakeSetup_${ts}`;
const CUBE = `DM_BakeCube_${ts}`;
const FLAT = `DM_BakeFlat_${ts}`;
const EMPTY = `DM_BakeEmpty_${ts}`;

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
  { scenario: 'Setup: create the cube actor', toolName: 'manage_geometry', arguments: create(CUBE), expected: 'success' },
  { scenario: 'Setup: create the second cube actor', toolName: 'manage_geometry', arguments: create(FLAT), expected: 'success' },
  { scenario: 'Setup: create an actor that stays empty', toolName: 'manage_geometry', arguments: create(EMPTY), expected: 'success' },
  { scenario: 'Setup: a cube on the cube actor', toolName: 'manage_geometry', arguments: cage(CUBE), expected: 'success' },
  { scenario: 'Setup: a cube on the second actor', toolName: 'manage_geometry', arguments: cage(FLAT), expected: 'success' },

  // === BAKE: white is no effect, so a convex cube reads open, edged and cavity-free ===
  {
    scenario: 'BAKE: bake_vertex_colors defaults every parameter and reads each mask on a cube', toolName: 'manage_geometry',
    arguments: { action: 'bake_vertex_colors', actorName: CUBE }, expected: 'success', timeoutMs: 30000,
    assertions: [
      { path: 'structuredContent.result.aoRays', equals: 32, label: 'default rays' },
      { path: 'structuredContent.result.blurIterations', equals: 1, label: 'default blur' },
      { path: 'structuredContent.result.averages.r', approximately: 1, tolerance: 0.001, label: 'a convex cube is open to the sky' },
      { path: 'structuredContent.result.averages.g', approximately: 0, tolerance: 0.001, label: 'every cube vertex is on a quarter-turn convex edge' },
      { path: 'structuredContent.result.averages.b', approximately: 1, tolerance: 0.001, label: 'a convex cube has no cavities' },
      { path: 'structuredContent.result.averages.a', approximately: 0.5, tolerance: 0.01, label: 'half the vertices are at the top' },
    ],
  },
  {
    scenario: 'BAKE: bake_vertex_colors with every parameter', toolName: 'manage_geometry',
    arguments: { action: 'bake_vertex_colors', actorName: CUBE, aoRays: 24, aoDistance: 40, curvatureScale: 1.5, blurIterations: 2 }, expected: 'success', timeoutMs: 30000,
    assertions: [
      { path: 'structuredContent.result.aoRays', equals: 24, label: 'rays used' },
      { path: 'structuredContent.result.aoDistance', equals: 40, label: 'distance used' },
      { path: 'structuredContent.result.curvatureScale', equals: 1.5, label: 'scale used' },
      { path: 'structuredContent.result.blurIterations', equals: 2, label: 'blur passes used' },
    ],
  },
  {
    scenario: 'BAKE: curvatureScale 0 switches the edge and cavity masks off', toolName: 'manage_geometry',
    arguments: { action: 'bake_vertex_colors', actorName: FLAT, curvatureScale: 0, blurIterations: 0 }, expected: 'success', timeoutMs: 30000,
    assertions: [
      { path: 'structuredContent.result.averages.g', approximately: 1, tolerance: 0.001, label: 'no edge mask' },
      { path: 'structuredContent.result.averages.b', approximately: 1, tolerance: 0.001, label: 'no cavity mask' },
    ],
  },
  { scenario: 'BAKE: aoRays outside 8-256 is clamped', toolName: 'manage_geometry', arguments: { action: 'bake_vertex_colors', actorName: FLAT, aoRays: 5000 }, expected: 'success', timeoutMs: 30000, assertions: [{ path: 'structuredContent.result.aoRays', equals: 256, label: 'clamped to the maximum' }] },

  // === REFUSALS ===
  { scenario: 'BAKE: a mesh with no triangles is refused', toolName: 'manage_geometry', arguments: { action: 'bake_vertex_colors', actorName: EMPTY }, expected: 'error|MESH_EMPTY' },
  { scenario: 'BAKE: a zero occlusion distance is refused', toolName: 'manage_geometry', arguments: { action: 'bake_vertex_colors', actorName: CUBE, aoDistance: 0 }, expected: 'error|INVALID_ARGUMENT' },
  { scenario: 'BAKE: a missing actor is refused', toolName: 'manage_geometry', arguments: { action: 'bake_vertex_colors', actorName: `NoSuchMesh_${ts}` }, expected: 'error|ACTOR_NOT_FOUND' },

  // === CLEANUP ===
  // The first edit_dynamic_mesh case's fold twin created a second actor under the SETUP label, so it is deleted twice.
  { scenario: 'Cleanup: delete the fold twin copy of the setup mesh', toolName: 'control_actor', arguments: { action: 'delete', actorName: SETUP }, expected: 'success|not found' },
  ...[SETUP, CUBE, FLAT, EMPTY].map((actorName) => (
    { scenario: `Cleanup: delete ${actorName}`, toolName: 'control_actor', arguments: { action: 'delete', actorName }, expected: 'success|not found' }
  )),
];

runToolTests('manage-geometry-vertex-colors', testCases);
