#!/usr/bin/env node
/**
 * Comprehensive Level Management Test Suite
 * Tool: manage_level
 * Coverage: All 18 actions with success, error, and edge cases
 */

import { runToolTests } from './test-runner.mjs';

const testCases = [
  // === PRE-CLEANUP ===
  {
    scenario: 'Pre-cleanup: Remove Invalid Data Layers',
    toolName: 'manage_level',
    arguments: { action: 'cleanup_invalid_datalayers' },
    expected: 'success'
  },

  // === SAVE (save, save_as, save_level_as) ===
  { scenario: 'Save current level', toolName: 'manage_level', arguments: { action: 'save' }, expected: 'success' },
  { scenario: 'Save level as new', toolName: 'manage_level', arguments: { action: 'save_as', savePath: '/Game/Maps/TC_SaveAs_Test' }, expected: 'success' },
  { scenario: 'Save level as (alias)', toolName: 'manage_level', arguments: { action: 'save_level_as', savePath: '/Game/Maps/TC_SaveLevelAs_Test' }, expected: 'success' },

  // === CREATE / LOAD / DELETE ===
  { scenario: 'Create test level', toolName: 'manage_level', arguments: { action: 'create_level', levelName: 'TC_TestLevel', streaming: false }, expected: 'success - level created' },
  { scenario: 'Create streaming sublevel', toolName: 'manage_level', arguments: { action: 'create_level', levelName: 'TC_SubLevel', streaming: true }, expected: 'success' },
  { scenario: 'Load engine entry level', toolName: 'manage_level', arguments: { action: 'load', levelPath: '/Engine/Maps/Entry' }, expected: 'success - level loaded' },
  { scenario: 'List open levels', toolName: 'manage_level', arguments: { action: 'list_levels' }, expected: 'success' },
  { scenario: 'Get level summary', toolName: 'manage_level', arguments: { action: 'get_summary', levelPath: '/Engine/Maps/Entry' }, expected: 'success' },

  // === STREAMING (stream, add_sublevel) ===
  { scenario: 'Stream sublevel (load + visible)', toolName: 'manage_level', arguments: { action: 'stream', levelPath: '/Game/Maps/TC_SubLevel', shouldBeLoaded: true, shouldBeVisible: true }, expected: 'success - sublevel streamed' },
  { scenario: 'Stream sublevel (unload)', toolName: 'manage_level', arguments: { action: 'stream', levelPath: '/Game/Maps/TC_SubLevel', shouldBeLoaded: false }, expected: 'success' },
  { scenario: 'Add sublevel to parent', toolName: 'manage_level', arguments: { action: 'add_sublevel', parentLevel: '/Game/Maps/TC_TestLevel', subLevelPath: '/Game/Maps/TC_SubLevel' }, expected: 'success' },

  // === LIGHTS (create_light) ===
  { scenario: 'Create point light', toolName: 'manage_level', arguments: { action: 'create_light', lightType: 'Point', location: { x: 200, y: 200, z: 100 }, intensity: 5000.0 }, expected: 'success - light spawned' },
  { scenario: 'Create directional light', toolName: 'manage_level', arguments: { action: 'create_light', lightType: 'Directional', rotation: { pitch: -45, yaw: 0, roll: 0 }, intensity: 3.0 }, expected: 'success - light spawned' },
  { scenario: 'Create spot light', toolName: 'manage_level', arguments: { action: 'create_light', lightType: 'Spot', location: { x: 0, y: 0, z: 300 }, intensity: 8000.0 }, expected: 'success' },
  { scenario: 'Create rect light', toolName: 'manage_level', arguments: { action: 'create_light', lightType: 'Rect', location: { x: 100, y: 100, z: 200 }, intensity: 10000.0 }, expected: 'success' },

  // === LIGHTING BUILD ===
  { scenario: 'Build lighting (Preview)', toolName: 'manage_level', arguments: { action: 'build_lighting', quality: 'Preview' }, expected: 'success - lighting built' },
  { scenario: 'Build lighting (Medium)', toolName: 'manage_level', arguments: { action: 'build_lighting', quality: 'Medium' }, expected: 'success' },

  // === METADATA ===
  { scenario: 'Set level metadata', toolName: 'manage_level', arguments: { action: 'set_metadata', levelPath: '/Game/Maps/TC_TestLevel', note: 'Test level created by automation' }, expected: 'success' },

  // === WORLD PARTITION (load_cells, set_datalayer) ===
  { scenario: 'Load world partition cells', toolName: 'manage_level', arguments: { action: 'load_cells', min: [0, 0, 0], max: [1000, 1000, 1000] }, expected: 'success|handled' },
  { scenario: 'Set data layer', toolName: 'manage_level', arguments: { action: 'set_datalayer', dataLayerLabel: 'TC_DataLayer', dataLayerState: 'Activated' }, expected: 'success|handled' },

  // === EXPORT / IMPORT ===
  { scenario: 'Export level', toolName: 'manage_level', arguments: { action: 'export_level', levelPath: '/Engine/Maps/Entry', exportPath: './tests/reports/tc_testlevel_export' }, expected: 'success' },
  { scenario: 'Import level', toolName: 'manage_level', arguments: { action: 'import_level', filePath: './tests/reports/tc_testlevel_export' }, expected: 'success|handled' },

  // === VALIDATION ===
  { scenario: 'Validate level', toolName: 'manage_level', arguments: { action: 'validate_level', levelPath: '/Game/Maps/TC_TestLevel' }, expected: 'success|not_found' },

  // === REAL-WORLD SCENARIO: Lighting Setup ===
  { scenario: 'Lighting - Create Sky Light', toolName: 'manage_level', arguments: { action: 'create_light', lightType: 'Sky', intensity: 1.0 }, expected: 'success' },
  { scenario: 'Lighting - Build Preview', toolName: 'manage_level', arguments: { action: 'build_lighting', quality: 'Preview' }, expected: 'success' },

  // === REAL-WORLD SCENARIO: Day/Night Streaming ===
  { scenario: 'Streaming - Create Night Variant', toolName: 'manage_level', arguments: { action: 'create_level', levelName: 'TC_SubLevel_Night', streaming: true }, expected: 'success' },
  { scenario: 'Streaming - Load Night', toolName: 'manage_level', arguments: { action: 'stream', levelPath: '/Game/Maps/TC_SubLevel_Night', shouldBeLoaded: true, shouldBeVisible: true }, expected: 'success' },
  { scenario: 'Streaming - Unload Day', toolName: 'manage_level', arguments: { action: 'stream', levelPath: '/Game/Maps/TC_SubLevel', shouldBeLoaded: false }, expected: 'success' },
  { scenario: 'Streaming - Unload Night', toolName: 'manage_level', arguments: { action: 'stream', levelPath: '/Game/Maps/TC_SubLevel_Night', shouldBeLoaded: false }, expected: 'success' },

  // === ERROR CASES ===
  { scenario: 'Error: Invalid lighting quality', toolName: 'manage_level', arguments: { action: 'build_lighting', quality: 'InvalidQuality' }, expected: 'error|unknown_quality' },
  { scenario: 'Error: Invalid level path', toolName: 'manage_level', arguments: { action: 'load', levelPath: '/Game/InvalidLevel' }, expected: 'not_found|error' },
  { scenario: 'Error: Missing action param', toolName: 'manage_level', arguments: {}, expected: 'validation_error' },
  { scenario: 'Error: Delete non-existent level', toolName: 'manage_level', arguments: { action: 'delete', levelPath: '/Game/Maps/NonExistentLevel' }, expected: 'success|not_found' },
  { scenario: 'Error: Load non-existent sublevel', toolName: 'manage_level', arguments: { action: 'stream', levelPath: '/Game/Maps/NonExistentSublevel', shouldBeLoaded: true }, expected: 'error|not_found' },

  // === EDGE CASES ===
  { scenario: 'Edge: Zero intensity light', toolName: 'manage_level', arguments: { action: 'create_light', lightType: 'Point', intensity: 0 }, expected: 'success' },
  { scenario: 'Edge: Negative intensity light', toolName: 'manage_level', arguments: { action: 'create_light', lightType: 'Point', intensity: -1000 }, expected: 'success|handled' },
  { scenario: 'Edge: Empty level name', toolName: 'manage_level', arguments: { action: 'create_level', levelName: '' }, expected: 'error|validation' },

  // === CLEANUP ===
  { scenario: 'Cleanup - Delete test levels', toolName: 'manage_level', arguments: { action: 'delete', levelPath: '/Game/Maps/TC_TestLevel' }, expected: 'success|not_found' },
  { scenario: 'Cleanup - Delete sublevel', toolName: 'manage_level', arguments: { action: 'delete', levelPath: '/Game/Maps/TC_SubLevel' }, expected: 'success|not_found' },
  { scenario: 'Cleanup - Delete night sublevel', toolName: 'manage_level', arguments: { action: 'delete', levelPath: '/Game/Maps/TC_SubLevel_Night' }, expected: 'success|not_found' },
  { scenario: 'Cleanup - Delete saved levels', toolName: 'manage_asset', arguments: { action: 'delete', assetPaths: ['/Game/Maps/TC_SaveAs_Test', '/Game/Maps/TC_SaveLevelAs_Test'] }, expected: 'success|not_found' }
];

await runToolTests('Manage Level', testCases);
