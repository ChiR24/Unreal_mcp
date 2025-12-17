#!/usr/bin/env node
/**
 * Comprehensive Environment Building Test Suite
 * Tool: build_environment
 * Coverage: All 20 actions with success, error, and edge cases
 */

import { runToolTests } from './test-runner.mjs';

const testCases = [
  // === PRE-CLEANUP ===
  {
    scenario: 'Pre-cleanup: Delete test landscapes',
    toolName: 'build_environment',
    arguments: { action: 'delete', names: ['TC_Landscape', 'TC_ProcTerrain', 'TC_Sculpt_Landscape'] },
    expected: 'success|not_found'
  },

  // === CREATE LANDSCAPE ===
  {
    scenario: 'Create small landscape',
    toolName: 'build_environment',
    arguments: {
      action: 'create_landscape',
      name: 'TC_Landscape',
      sizeX: 4,
      sizeY: 4,
      sectionSize: 63,
      sectionsPerComponent: 1
    },
    expected: 'success - landscape created'
  },
  {
    scenario: 'Create medium landscape',
    toolName: 'build_environment',
    arguments: {
      action: 'create_landscape',
      name: 'TC_Landscape_Med',
      sizeX: 8,
      sizeY: 8,
      componentCount: { x: 2, y: 2 }
    },
    expected: 'success'
  },

  // === SCULPT LANDSCAPE ===
  {
    scenario: 'Sculpt landscape - smooth',
    toolName: 'build_environment',
    arguments: { action: 'sculpt', name: 'TC_Landscape', tool: 'Smooth' },
    expected: 'success or LOAD_FAILED'
  },
  {
    scenario: 'Sculpt landscape - flatten',
    toolName: 'build_environment',
    arguments: { action: 'sculpt', name: 'TC_Landscape', tool: 'Flatten' },
    expected: 'success or LOAD_FAILED'
  },
  {
    scenario: 'Sculpt landscape - noise',
    toolName: 'build_environment',
    arguments: { action: 'sculpt', name: 'TC_Landscape', tool: 'Noise', strength: 0.3 },
    expected: 'success or LOAD_FAILED'
  },
  {
    scenario: 'Sculpt landscape (alias)',
    toolName: 'build_environment',
    arguments: { action: 'sculpt_landscape', landscapeName: 'TC_Landscape', tool: 'Raise', strength: 0.5 },
    expected: 'success or LOAD_FAILED'
  },

  // === PAINT LANDSCAPE ===
  {
    scenario: 'Paint landscape layer',
    toolName: 'build_environment',
    arguments: {
      action: 'paint_landscape',
      landscapeName: 'TC_Landscape',
      layerName: 'Grass',
      position: { x: 0, y: 0, z: 0 },
      brushSize: 100,
      strength: 0.8
    },
    expected: 'success or LOAD_FAILED'
  },
  {
    scenario: 'Paint landscape layer (alias)',
    toolName: 'build_environment',
    arguments: {
      action: 'paint_landscape_layer',
      landscapeName: 'TC_Landscape',
      layerName: 'Rock',
      brushSize: 50,
      strength: 1.0
    },
    expected: 'success or LOAD_FAILED'
  },

  // === MODIFY HEIGHTMAP ===
  {
    scenario: 'Modify heightmap',
    toolName: 'build_environment',
    arguments: {
      action: 'modify_heightmap',
      landscapeName: 'TC_Landscape',
      heightData: [100, 110, 105, 95],
      updateNormals: true
    },
    expected: 'success or LOAD_FAILED'
  },

  // === SET LANDSCAPE MATERIAL ===
  {
    scenario: 'Set landscape material',
    toolName: 'build_environment',
    arguments: {
      action: 'set_landscape_material',
      name: 'TC_Landscape',
      materialPath: '/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial'
    },
    expected: 'success or LOAD_FAILED'
  },

  // === GRASS TYPE ===
  {
    scenario: 'Create landscape grass type',
    toolName: 'build_environment',
    arguments: {
      action: 'create_landscape_grass_type',
      name: 'TC_GrassType',
      meshPath: '/Engine/BasicShapes/Cube.Cube'
    },
    expected: 'success - grass type created'
  },

  // === FOLIAGE ===
  {
    scenario: 'Add foliage type',
    toolName: 'build_environment',
    arguments: {
      action: 'add_foliage',
      foliageType: 'TC_Tree',
      meshPath: '/Engine/BasicShapes/Cube.Cube',
      density: 0.1,
      minScale: 0.8,
      maxScale: 1.2
    },
    expected: 'success - foliage added'
  },
  {
    scenario: 'Paint foliage',
    toolName: 'build_environment',
    arguments: {
      action: 'paint_foliage',
      foliageType: 'TC_Tree',
      position: { x: 0, y: 0, z: 0 },
      brushSize: 100,
      density: 0.5
    },
    expected: 'success - foliage painted'
  },
  {
    scenario: 'Add foliage instances',
    toolName: 'build_environment',
    arguments: {
      action: 'add_foliage_instances',
      foliageType: 'TC_Tree',
      locations: [{ x: 0, y: 0, z: 0 }, { x: 100, y: 0, z: 0 }, { x: 200, y: 0, z: 0 }],
      randomYaw: true,
      alignToNormal: true
    },
    expected: 'success - instances added'
  },
  {
    scenario: 'Get foliage instances',
    toolName: 'build_environment',
    arguments: {
      action: 'get_foliage_instances',
      foliageType: 'TC_Tree',
      bounds: { minX: -1000, maxX: 1000, minY: -1000, maxY: 1000 }
    },
    expected: 'success'
  },
  {
    scenario: 'Remove foliage instances',
    toolName: 'build_environment',
    arguments: {
      action: 'remove_foliage',
      foliageType: 'TC_Tree',
      eraseMode: true,
      radius: 200,
      position: { x: 0, y: 0, z: 0 }
    },
    expected: 'success'
  },

  // === PROCEDURAL TERRAIN ===
  {
    scenario: 'Create procedural terrain',
    toolName: 'build_environment',
    arguments: {
      action: 'create_procedural_terrain',
      name: 'TC_ProcTerrain',
      location: { x: 5000, y: 0, z: 0 },
      seed: 12345,
      sizeX: 200,
      sizeY: 200
    },
    expected: 'success - procedural terrain created'
  },

  // === PROCEDURAL FOLIAGE ===
  {
    scenario: 'Create procedural foliage volume',
    toolName: 'build_environment',
    arguments: {
      action: 'create_procedural_foliage',
      volumeName: 'TC_ProcFoliage',
      bounds: { location: { x: 0, y: 0, z: 0 }, size: { x: 1000, y: 1000, z: 100 } },
      seed: 42,
      foliageTypes: []
    },
    expected: 'success - procedural foliage created'
  },

  // === LODS ===
  {
    scenario: 'Generate LODs',
    toolName: 'build_environment',
    arguments: {
      action: 'generate_lods',
      meshPath: '/Engine/BasicShapes/Cube',
      lodCount: 3,
      cullDistance: 5000
    },
    expected: 'success or ASSET_NOT_FOUND'
  },

  // === LIGHTMAP ===
  {
    scenario: 'Bake lightmap (preview)',
    toolName: 'build_environment',
    arguments: { action: 'bake_lightmap', quality: 'Preview' },
    expected: 'success - lightmap bake requested'
  },

  // === SNAPSHOTS ===
  {
    scenario: 'Export environment snapshot',
    toolName: 'build_environment',
    arguments: { action: 'export_snapshot', filename: 'tc_env_snapshot.json' },
    expected: 'success - snapshot exported'
  },
  {
    scenario: 'Import environment snapshot',
    toolName: 'build_environment',
    arguments: { action: 'import_snapshot', filename: 'tc_env_snapshot.json' },
    expected: 'success - import handled'
  },

  // === ERROR CASES ===
  {
    scenario: 'Error: Invalid sculpt tool',
    toolName: 'build_environment',
    arguments: { action: 'sculpt', tool: 'InvalidTool' },
    expected: 'error'
  },
  {
    scenario: 'Error: Landscape size 0',
    toolName: 'build_environment',
    arguments: { action: 'create_landscape', sizeX: 0, sizeY: 0 },
    expected: 'error|validation|required'
  },
  {
    scenario: 'Error: Invalid mesh path for foliage',
    toolName: 'build_environment',
    arguments: { action: 'add_foliage', meshPath: '/Game/Invalid/Mesh', density: 0.5 },
    expected: 'LOAD_FAILED|ASSET_NOT_FOUND|INVALID_ARGUMENT'
  },

  // === EDGE CASES ===
  {
    scenario: 'Edge: Zero density foliage',
    toolName: 'build_environment',
    arguments: { action: 'add_foliage', meshPath: '/Engine/BasicShapes/Cube', density: 0 },
    expected: 'success|no_instances|LOAD_FAILED|ASSET_NOT_FOUND|INVALID_ARGUMENT'
  },
  {
    scenario: 'Edge: Empty foliage types',
    toolName: 'build_environment',
    arguments: { action: 'create_procedural_foliage', foliageTypes: [] },
    expected: 'success|no_op'
  },
  {
    scenario: 'Edge: Negative seed',
    toolName: 'build_environment',
    arguments: { action: 'create_procedural_terrain', name: 'TC_NegSeed', seed: -1 },
    expected: 'success'
  },

  // === CLEANUP ===
  {
    scenario: 'Cleanup landscapes',
    toolName: 'build_environment',
    arguments: { action: 'delete', names: ['TC_Landscape', 'TC_Landscape_Med', 'TC_ProcTerrain'] },
    expected: 'success or DELETE_PARTIAL'
  },
  {
    scenario: 'Cleanup grass type asset',
    toolName: 'manage_asset',
    arguments: { action: 'delete', assetPaths: ['/Game/TC_GrassType'] },
    expected: 'success|not_found'
  }
];

await runToolTests('Environment Building', testCases);
