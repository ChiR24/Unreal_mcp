#!/usr/bin/env node
/**
 * manage_geometry conversion suite: bake a dynamic mesh to a static mesh with a material per slot and a collision choice.
 *
 * A material id on a triangle becomes a static-mesh material slot, and `materials` fills those slots (entry i is
 * the material of slot i, "" keeps the default). `collision` is box (the default), complex or none. A bad material
 * path, or a list longer than the slot count, is refused before any asset is created.
 *
 * The cage is built with append_polygons and its faceMaterials, so the mesh carries four material ids (0 to 3).
 */

import { runToolTests } from '../../test-runner.mjs';

const ts = Date.now();
const SETUP = `DM_ConvSetup_${ts}`;
const CAGE = `DM_ConvCage_${ts}`;
const SECOND = `DM_ConvSecond_${ts}`;
const SINGLE = `DM_ConvSingle_${ts}`;
const PAIR = `DM_ConvPair_${ts}`;
const SPLIT_FOLDER = `/Game/GeneratedMeshes/Split_${ts}`;
const MATERIAL = '/Engine/BasicShapes/BasicShapeMaterial';
const GRID_MATERIAL = '/Engine/EngineMaterials/WorldGridMaterial';

const CUBE_VERTICES = [
  { x: -50, y: -50, z: -50 }, { x: 50, y: -50, z: -50 }, { x: 50, y: 50, z: -50 }, { x: -50, y: 50, z: -50 },
  { x: -50, y: -50, z: 50 }, { x: 50, y: -50, z: 50 }, { x: 50, y: 50, z: 50 }, { x: -50, y: 50, z: 50 },
];
const CUBE_FACES = [[0, 3, 2, 1], [4, 5, 6, 7], [0, 1, 5, 4], [1, 2, 6, 5], [2, 3, 7, 6], [3, 0, 4, 7]];

const create = (name) => ({ action: 'create_procedural_mesh', name, actorName: name });
const cage = (actorName, faceMaterials) => ({ action: 'append_polygons', actorName, vertices: CUBE_VERTICES, faces: CUBE_FACES, faceMaterials });

const testCases = [
  // === SETUP ===
  { scenario: 'Setup: first edit_dynamic_mesh case, a throwaway mesh (its fold twin repeats it)', toolName: 'manage_geometry', arguments: create(SETUP), expected: 'success' },
  { scenario: 'Setup: create the cage actor', toolName: 'manage_geometry', arguments: create(CAGE), expected: 'success' },
  { scenario: 'Setup: create the second actor', toolName: 'manage_geometry', arguments: create(SECOND), expected: 'success' },
  { scenario: 'Setup: create the single-material actor', toolName: 'manage_geometry', arguments: create(SINGLE), expected: 'success' },
  { scenario: 'Setup: a cube cage with material ids 0 to 3', toolName: 'manage_geometry', arguments: cage(CAGE, [0, 1, 2, 3, 0, 1]), expected: 'success' },
  { scenario: 'Setup: a second cube cage with material ids 0 to 2', toolName: 'manage_geometry', arguments: cage(SECOND, [0, 0, 1, 1, 2, 2]), expected: 'success' },
  { scenario: 'Setup: a cube cage with only the default material id', toolName: 'manage_geometry', arguments: { action: 'append_polygons', actorName: SINGLE, vertices: CUBE_VERTICES, faces: CUBE_FACES }, expected: 'success' },
  { scenario: 'Setup: create the actor holding two cubes apart', toolName: 'manage_geometry', arguments: create(PAIR), expected: 'success' },
  { scenario: 'Setup: the first cube of the pair', toolName: 'manage_geometry', arguments: { action: 'append_polygons', actorName: PAIR, vertices: CUBE_VERTICES, faces: CUBE_FACES }, expected: 'success' },
  { scenario: 'Setup: the second cube of the pair, 300 along X', toolName: 'manage_geometry', arguments: { action: 'append_polygons', actorName: PAIR, vertices: CUBE_VERTICES.map((v) => ({ ...v, x: v.x + 300 })), faces: CUBE_FACES }, expected: 'success' },

  // === CONVERT: a material per slot and a collision choice ===
  {
    scenario: 'CONVERT: convert_to_static_mesh with materials per slot and complex collision', toolName: 'manage_geometry',
    arguments: { action: 'convert_to_static_mesh', actorName: CAGE, outputPath: `Game/GeneratedMeshes/DM_ConvCage_${ts}`, materials: [MATERIAL, '', GRID_MATERIAL], collision: 'complex' },
    expected: 'success', timeoutMs: 30000,
    assertions: [
      { path: 'structuredContent.result.collision', equals: 'complex', label: 'collision reported' },
      { path: 'structuredContent.result.slots', minLength: 4, label: 'max material id 3 means four slots' },
    ],
  },
  {
    scenario: 'CONVERT: convert_to_nanite with materials and no collision', toolName: 'manage_geometry',
    arguments: { action: 'convert_to_nanite', actorName: SECOND, outputPath: `Game/GeneratedMeshes/DM_ConvSecond_${ts}`, materials: [MATERIAL, GRID_MATERIAL, MATERIAL], collision: 'none' },
    expected: 'success', timeoutMs: 30000,
    assertions: [
      { path: 'structuredContent.result.naniteEnabled', equals: true, label: 'nanite on' },
      { path: 'structuredContent.result.slots', minLength: 3, label: 'max material id 2 means three slots' },
    ],
  },
  {
    scenario: 'CONVERT: convert_to_static_mesh with the default box collision and the default material', toolName: 'manage_geometry',
    arguments: { action: 'convert_to_static_mesh', actorName: SINGLE, outputPath: `Game/GeneratedMeshes/DM_ConvSingle_${ts}`, collision: 'box' },
    expected: 'success', timeoutMs: 30000,
    assertions: [{ path: 'structuredContent.result.collision', equals: 'box', label: 'box collision' }],
  },

  { scenario: 'Setup: bake the pair into one static mesh', toolName: 'manage_geometry', arguments: { action: 'convert_to_static_mesh', actorName: PAIR, outputPath: `Game/GeneratedMeshes/DM_ConvPair_${ts}` }, expected: 'success', timeoutMs: 30000 },

  // === SPLIT: one static mesh per connected part or per material slot ===
  {
    scenario: 'SPLIT: split_mesh dryRun answers the two meshes and creates nothing', toolName: 'manage_geometry',
    arguments: { action: 'split_mesh', meshPath: `/Game/GeneratedMeshes/DM_ConvPair_${ts}`, outputPath: SPLIT_FOLDER, namePrefix: 'SM_Piece', dryRun: true },
    expected: 'success', timeoutMs: 30000,
    assertions: [{ path: 'structuredContent.result.meshes', length: 2, label: 'two meshes would be made' }, { path: 'structuredContent.result.meshes.0.exists', equals: false, label: 'and none exists yet' }],
  },
  {
    scenario: 'SPLIT: split_mesh gives one mesh per cube, each standing on its own bottom centre', toolName: 'manage_geometry',
    arguments: { action: 'split_mesh', meshPath: `/Game/GeneratedMeshes/DM_ConvPair_${ts}`, outputPath: SPLIT_FOLDER, namePrefix: 'SM_Piece', by: 'parts', gap: 2, pivot: 'bottom', maxParts: 8, collision: 'box' },
    expected: 'success', timeoutMs: 30000,
    assertions: [
      { path: 'structuredContent.result.meshes', length: 2, label: 'two cubes, two meshes' },
      { path: 'structuredContent.result.meshes.0.offset.2', equals: -50, label: 'the pivot is the bottom of the cube' },
    ],
  },
  {
    scenario: 'SPLIT: split_mesh by material gives one mesh per material slot', toolName: 'manage_geometry',
    arguments: { action: 'split_mesh', meshPath: `/Game/GeneratedMeshes/DM_ConvCage_${ts}`, outputPath: SPLIT_FOLDER, namePrefix: 'SM_Slot', by: 'material', pivot: 'center', minTriangles: 1, dropRepeats: false, collision: 'none' },
    expected: 'success', timeoutMs: 30000,
    assertions: [{ path: 'structuredContent.result.meshes', length: 4, label: 'material ids 0 to 3' }],
  },
  { scenario: 'SPLIT: a name already taken refuses the split before anything is made', toolName: 'manage_geometry', arguments: { action: 'split_mesh', meshPath: `/Game/GeneratedMeshes/DM_ConvPair_${ts}`, outputPath: SPLIT_FOLDER, namePrefix: 'SM_Piece' }, expected: 'error|ASSET_EXISTS' },
  { scenario: 'SPLIT: more parts than maxParts refuses the split', toolName: 'manage_geometry', arguments: { action: 'split_mesh', meshPath: `/Game/GeneratedMeshes/DM_ConvPair_${ts}`, outputPath: SPLIT_FOLDER, namePrefix: 'SM_Many', maxParts: 1 }, expected: 'error|INVALID_ARGUMENT' },

  // === REFUSALS: before anything is created ===
  { scenario: 'CONVERT: a material path that does not load is refused', toolName: 'manage_geometry', arguments: { action: 'convert_to_static_mesh', actorName: CAGE, outputPath: `Game/GeneratedMeshes/DM_ConvBad_${ts}`, materials: ['/Game/DoesNotExist/M_Nope'] }, expected: 'error|INVALID_MATERIALS' },
  { scenario: 'CONVERT: a materials list longer than the slot count is refused', toolName: 'manage_geometry', arguments: { action: 'convert_to_nanite', actorName: SINGLE, outputPath: `Game/GeneratedMeshes/DM_ConvLong_${ts}`, materials: [MATERIAL, GRID_MATERIAL] }, expected: 'error|INVALID_MATERIALS' },
  { scenario: 'CONVERT: a traversal in a material path is refused by the path sanitizer', toolName: 'manage_geometry', arguments: { action: 'convert_to_static_mesh', actorName: CAGE, outputPath: `Game/GeneratedMeshes/DM_ConvTrav_${ts}`, materials: ['/Game/../Secrets/M_Nope'] }, expected: 'error|INVALID_MATERIALS' },

  // === CLEANUP ===
  // The first edit_dynamic_mesh case's fold twin created a second actor under the SETUP label, so it is deleted twice.
  { scenario: 'Cleanup: delete the fold twin copy of the setup mesh', toolName: 'control_actor', arguments: { action: 'delete', actorName: SETUP }, expected: 'success|not found' },
  ...[SETUP, CAGE, SECOND, SINGLE, PAIR].map((actorName) => (
    { scenario: `Cleanup: delete ${actorName}`, toolName: 'control_actor', arguments: { action: 'delete', actorName }, expected: 'success|not found' }
  )),
];

runToolTests('manage-geometry-conversion', testCases);
