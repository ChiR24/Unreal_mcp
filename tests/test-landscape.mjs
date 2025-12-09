#!/usr/bin/env node
/**
 * Condensed Environment Building Test Suite (15 cases)
 * Tool: build_environment
 */

import { runToolTests } from './test-runner.mjs';

const testCases = [
  { scenario: 'Create small landscape', toolName: 'build_environment', arguments: { action: 'create_landscape', name: 'TC_Landscape', sizeX: 4, sizeY: 4 }, expected: 'success - landscape created' },
  { scenario: 'Sculpt landscape - smooth', toolName: 'build_environment', arguments: { action: 'sculpt', name: 'TC_Landscape', tool: 'Smooth' }, expected: 'success or LOAD_FAILED' },
  { scenario: 'Flatten small area', toolName: 'build_environment', arguments: { action: 'sculpt', name: 'TC_Landscape', tool: 'Flatten' }, expected: 'success or LOAD_FAILED' },
  { scenario: 'Add foliage type (trees)', toolName: 'build_environment', arguments: { action: 'add_foliage', foliageType: 'TC_Tree', meshPath: '/Engine/BasicShapes/Cube.Cube', density: 0.1 }, expected: 'success - foliage added' },
  { scenario: 'Paint a small foliage patch', toolName: 'build_environment', arguments: { action: 'paint_foliage', foliageType: 'TC_Tree', position: { x: 0, y: 0, z: 0 }, brushSize: 100, strength: 0.5 }, expected: 'success - foliage painted' },
  { scenario: 'Create procedural terrain', toolName: 'build_environment', arguments: { action: 'create_procedural_terrain', name: 'TC_ProcTerrain', location: { x: 500, y: 0, z: 0 }, subdivisions: 8 }, expected: 'success - procedural terrain created' },
  { scenario: 'Create procedural foliage (small)', toolName: 'build_environment', arguments: { action: 'create_procedural_foliage', bounds: { location: { x: 0, y: 0, z: 0 }, size: { x: 1000, y: 1000, z: 100 } }, seed: 42 }, expected: 'success - procedural foliage created' },
  { scenario: 'Add grass foliage', toolName: 'build_environment', arguments: { action: 'add_foliage', meshPath: '/Engine/BasicShapes/Cube.Cube', density: 0.5 }, expected: 'success - grass added' },
  { scenario: 'Generate small LODs (best-effort)', toolName: 'build_environment', arguments: { action: 'generate_lods', assetPath: '/Game/Meshes/SM_Test', lodCount: 2 }, expected: 'success or ASSET_NOT_FOUND' },
  { scenario: 'Bake lightmap for a small area (preview)', toolName: 'build_environment', arguments: { action: 'bake_lightmap', quality: 'Preview' }, expected: 'success - lightmap bake requested' },
  { scenario: 'Create landscape grass type', toolName: 'build_environment', arguments: { action: 'create_landscape_grass_type', name: 'TC_Grass', meshPath: '/Engine/BasicShapes/Cube.Cube' }, expected: 'success - grass type created' },
  { scenario: 'Add foliage instances', toolName: 'build_environment', arguments: { action: 'add_foliage', foliageType: 'TC_Tree', locations: [{ x: 0, y: 0, z: 0 }] }, expected: 'success - instances added' },
  { scenario: 'Export environment snapshot', toolName: 'build_environment', arguments: { action: 'export_snapshot', path: './tests/reports/env_snapshot.json' }, expected: 'success - snapshot exported' },
  { scenario: 'Import environment snapshot', toolName: 'build_environment', arguments: { action: 'import_snapshot', path: './tests/reports/env_snapshot.json' }, expected: 'success - import handled' },
  { scenario: 'Cleanup landscape artifacts', toolName: 'build_environment', arguments: { action: 'delete', names: ['TC_Landscape', 'TC_ProcTerrain'] }, expected: 'success or DELETE_PARTIAL' },
  // Additional
  // Real-World Scenario: Terrain Sculpting
  { scenario: 'Sculpting - Create Landscape', toolName: 'build_environment', arguments: { action: 'create_landscape', name: 'TC_Sculpt_Landscape', sizeX: 8, sizeY: 8 }, expected: 'success' },
  { scenario: 'Sculpting - Apply Noise', toolName: 'build_environment', arguments: { action: 'sculpt', name: 'TC_Sculpt_Landscape', tool: 'Noise', strength: 0.2 }, expected: 'success or LOAD_FAILED' },
  { scenario: 'Sculpting - Assign Material', toolName: 'build_environment', arguments: { action: 'set_landscape_material', name: 'TC_Sculpt_Landscape', materialPath: '/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial' }, expected: 'success or LOAD_FAILED' },

  // Cleanup
  { scenario: 'Cleanup - Delete Sculpt Landscape', toolName: 'build_environment', arguments: { action: 'delete', names: ['TC_Sculpt_Landscape'] }, expected: 'success or DELETE_PARTIAL' },

  {
    scenario: "Error: Invalid sculpt tool",
    toolName: "build_environment",
    arguments: { action: "sculpt", tool: "InvalidTool" },
    expected: "error"
  },
  {
    scenario: "Edge: Landscape size 0",
    toolName: "build_environment",
    arguments: { action: "create_landscape", sizeX: 0, sizeY: 0 },
    expected: "error|validation|required"
  },
  {
    scenario: "Border: Zero density foliage",
    toolName: "build_environment",
    arguments: { action: "add_foliage", meshPath: "/Game/Valid", density: 0 },
    expected: "success|no_instances|LOAD_FAILED|ASSET_NOT_FOUND|INVALID_ARGUMENT"
  },
  {
    scenario: "Edge: Empty foliage types",
    toolName: "build_environment",
    arguments: { action: "create_procedural_foliage", foliageTypes: [] },
    expected: "success|no_op"
  }
];

await runToolTests('Environment Building', testCases);
