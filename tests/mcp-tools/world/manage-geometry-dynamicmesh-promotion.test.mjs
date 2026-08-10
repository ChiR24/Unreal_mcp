#!/usr/bin/env node
/**
 * manage_geometry DynamicMesh promotion suite.
 *
 * Covers the ten DynamicMesh routes promoted from raw native dispatch: their
 * happy path, their defaults, and every optional parameter their capability
 * record declares. The static parameter audit reads these definitions, so an
 * optional declared in a record but never named here fails --optional-strict.
 *
 * Every mutating case needs a live UDynamicMeshComponent actor, so setup builds
 * one with create_procedural_mesh and the vertex cases append geometry to it
 * before reading or editing an index.
 */

import { runToolTests } from '../../test-runner.mjs';

const TEST_FOLDER = '/Game/MCPTest/WorldAssets';
const ts = Date.now();
const MESH_ACTOR = `DM_Promo_${ts}`;
const BOOL_TARGET = `DM_BoolTarget_${ts}`;
const BOOL_TOOL = `DM_BoolTool_${ts}`;

const geo = (action, extra = {}) => ({ action, actorName: MESH_ACTOR, ...extra });

const testCases = [
  // === SETUP ===
  { scenario: 'Setup: create test folder', toolName: 'manage_asset', arguments: { action: 'create_folder', path: TEST_FOLDER }, expected: 'success|already exists' },

  // === CREATE: create_procedural_mesh (every param optional; name defaults to ProceduralMesh) ===
  { scenario: 'CREATE: create_procedural_mesh with all optionals', toolName: 'manage_geometry', arguments: { action: 'create_procedural_mesh', name: MESH_ACTOR, actorName: MESH_ACTOR, enableCollision: true }, expected: 'success' },
  { scenario: 'CREATE: create_procedural_mesh defaults every parameter', toolName: 'manage_geometry', arguments: { action: 'create_procedural_mesh' }, expected: 'success' },

  // === BUILD: append_vertex / append_triangle ===
  { scenario: 'BUILD: append_vertex at an explicit position', toolName: 'manage_geometry', arguments: geo('append_vertex', { position: { x: 0, y: 0, z: 0 } }), expected: 'success' },
  { scenario: 'BUILD: append_vertex defaults position to the origin', toolName: 'manage_geometry', arguments: geo('append_vertex'), expected: 'success' },
  { scenario: 'BUILD: append_triangle with explicit corners and group', toolName: 'manage_geometry', arguments: geo('append_triangle', { v0: { x: 0, y: 0, z: 0 }, v1: { x: 100, y: 0, z: 0 }, v2: { x: 50, y: 100, z: 0 }, groupID: 1 }), expected: 'success' },
  { scenario: 'BUILD: append_triangle defaults its corners', toolName: 'manage_geometry', arguments: geo('append_triangle'), expected: 'success' },

  // === READ: get_vertex_position (both params required) ===
  { scenario: 'READ: get_vertex_position', toolName: 'manage_geometry', arguments: geo('get_vertex_position', { vertexIndex: 0 }), expected: 'success' },
  { scenario: 'READ: get_vertex_position rejects an out-of-range index', toolName: 'manage_geometry', arguments: geo('get_vertex_position', { vertexIndex: 999999 }), expected: 'error' },

  // === EDIT: set_vertex_position ===
  { scenario: 'EDIT: set_vertex_position', toolName: 'manage_geometry', arguments: geo('set_vertex_position', { vertexIndex: 0, position: { x: 10, y: 20, z: 30 } }), expected: 'success' },
  { scenario: 'EDIT: set_vertex_position defaults position to the origin', toolName: 'manage_geometry', arguments: geo('set_vertex_position', { vertexIndex: 0 }), expected: 'success' },

  // === EDIT: set_vertex_color (vertexIndex optional; setAll covers the whole mesh) ===
  { scenario: 'EDIT: set_vertex_color on one vertex', toolName: 'manage_geometry', arguments: geo('set_vertex_color', { vertexIndex: 0, r: 1, g: 0.5, b: 0.25, a: 1, setAll: false }), expected: 'success' },
  { scenario: 'EDIT: set_vertex_color across every vertex', toolName: 'manage_geometry', arguments: geo('set_vertex_color', { setAll: true }), expected: 'success' },

  // === EDIT: set_uvs ===
  { scenario: 'EDIT: set_uvs on one vertex', toolName: 'manage_geometry', arguments: geo('set_uvs', { vertexIndex: 0, u: 0.5, v: 0.5, uvChannel: 0 }), expected: 'success|NO_UV_ELEMENTS' },
  { scenario: 'EDIT: set_uvs defaults u, v and channel', toolName: 'manage_geometry', arguments: geo('set_uvs', { vertexIndex: 0 }), expected: 'success|NO_UV_ELEMENTS' },

  // === EDIT: split_normals ===
  { scenario: 'EDIT: split_normals at an explicit angle', toolName: 'manage_geometry', arguments: geo('split_normals', { splitAngle: 45 }), expected: 'success' },
  { scenario: 'EDIT: split_normals defaults the angle to 60 degrees', toolName: 'manage_geometry', arguments: geo('split_normals'), expected: 'success' },

  // === EDIT: translate_mesh ===
  { scenario: 'EDIT: translate_mesh', toolName: 'manage_geometry', arguments: geo('translate_mesh', { translation: { x: 100, y: 0, z: 0 } }), expected: 'success' },
  { scenario: 'EDIT: translate_mesh defaults to no movement', toolName: 'manage_geometry', arguments: geo('translate_mesh'), expected: 'success' },

  // === BOOLEAN: difference (the boolean_subtract spelling) ===
  { scenario: 'Setup: create boolean target mesh', toolName: 'manage_geometry', arguments: { action: 'create_procedural_mesh', name: BOOL_TARGET, actorName: BOOL_TARGET }, expected: 'success' },
  { scenario: 'Setup: create boolean tool mesh', toolName: 'manage_geometry', arguments: { action: 'create_procedural_mesh', name: BOOL_TOOL, actorName: BOOL_TOOL }, expected: 'success' },
  { scenario: 'BOOLEAN: difference subtracts the tool from the target', toolName: 'manage_geometry', arguments: { action: 'difference', targetActor: BOOL_TARGET, toolActor: BOOL_TOOL, keepTool: true, keepInside: false }, expected: 'success' },

  // === CLEANUP ===
  { scenario: 'Cleanup: delete promoted mesh actor', toolName: 'control_actor', arguments: { action: 'delete', actorName: MESH_ACTOR }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete boolean target actor', toolName: 'control_actor', arguments: { action: 'delete', actorName: BOOL_TARGET }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete boolean tool actor', toolName: 'control_actor', arguments: { action: 'delete', actorName: BOOL_TOOL }, expected: 'success|not found' },
];

runToolTests('manage-geometry-dynamicmesh-promotion', testCases);
