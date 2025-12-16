#!/usr/bin/env node
/**
 * Condensed Level Management Test Suite (15 cases) — safe for real Editor runs.
 * Tool: manage_level
 */

import { runToolTests } from './test-runner.mjs';

const testCases = [
  { scenario: 'Save current level', toolName: 'manage_level', arguments: { action: 'save' }, expected: 'success' },
  { scenario: 'Create test level (best-effort)', toolName: 'manage_level', arguments: { action: 'create_level', levelName: 'TC_TestLevel', streaming: false }, expected: 'success - level created' },
  { scenario: 'Load test level', toolName: 'manage_level', arguments: { action: 'load', levelPath: '/Engine/Maps/Entry' }, expected: 'success - level loaded' },
  { scenario: 'Stream sublevel (create then stream)', toolName: 'manage_level', arguments: { action: 'create_level', levelName: 'TC_SubLevel', streaming: true }, expected: 'success - command executed' },
  { scenario: 'Stream sublevel (make visible)', toolName: 'manage_level', arguments: { action: 'stream', levelPath: '/Game/Maps/TC_SubLevel', shouldBeLoaded: true, shouldBeVisible: true }, expected: 'success - sublevel streamed' },
  { scenario: 'Create point light', toolName: 'manage_level', arguments: { action: 'create_light', lightType: 'Point', location: { x: 200, y: 200, z: 100 }, intensity: 5000.0 }, expected: 'success - light spawned' },
  { scenario: 'Create directional light', toolName: 'manage_level', arguments: { action: 'create_light', lightType: 'Directional', rotation: { pitch: -45, yaw: 0, roll: 0 }, intensity: 3.0 }, expected: 'success - light spawned' },
  { scenario: 'Quick build lighting (preview)', toolName: 'manage_level', arguments: { action: 'build_lighting', quality: 'Preview' }, expected: 'success - lighting built' },
  { scenario: 'Export level to package', toolName: 'manage_level', arguments: { action: 'export_level', levelPath: '/Engine/Maps/Entry', outputPath: './tests/reports/tc_testlevel_export' }, expected: 'success' },
  { scenario: 'Import level', toolName: 'manage_level', arguments: { action: 'import_level', filePath: './tests/reports/tc_testlevel_export' }, expected: 'success' },
  { scenario: 'List open levels', toolName: 'manage_level', arguments: { action: 'list_levels' }, expected: 'success' },
  { scenario: 'Get level summary', toolName: 'manage_level', arguments: { action: 'get_summary', levelPath: '/Engine/Maps/Entry' }, expected: 'success' },
  { scenario: 'Save level as new', toolName: 'manage_level', arguments: { action: 'save_level_as', savePath: '/Game/Maps/TC_TestLevel_SavedAs' }, expected: 'success' },
  { scenario: 'Unload streaming level', toolName: 'manage_level', arguments: { action: 'stream', levelPath: '/Game/Maps/TC_SubLevel', shouldBeLoaded: false }, expected: 'success - sublevel unloaded' },
  { scenario: 'Delete test levels', toolName: 'manage_level', arguments: { action: 'delete', levelPath: '/Game/Maps/TC_TestLevel_SavedAs' }, expected: 'success' },
  // Additional
  // Real-World Scenario: Lighting Setup
  { scenario: 'Lighting - Create Sky Light', toolName: 'manage_level', arguments: { action: 'create_light', lightType: 'Sky', intensity: 1.0 }, expected: 'success' },
  { scenario: 'Lighting - Build (Preview)', toolName: 'manage_level', arguments: { action: 'build_lighting', quality: 'Preview' }, expected: 'success' },

  // Real-World Scenario: Level Streaming
  { scenario: 'Streaming - Create Night Variant', toolName: 'manage_level', arguments: { action: 'create_level', levelName: 'TC_SubLevel_Night', streaming: true }, expected: 'success' },
  { scenario: 'Streaming - Load Night', toolName: 'manage_level', arguments: { action: 'stream', levelPath: '/Game/Maps/TC_SubLevel_Night', shouldBeLoaded: true, shouldBeVisible: true }, expected: 'success' },
  { scenario: 'Streaming - Unload Day (TC_SubLevel)', toolName: 'manage_level', arguments: { action: 'stream', levelPath: '/Game/Maps/TC_SubLevel', shouldBeLoaded: false }, expected: 'success' },

  // Cleanup
  { scenario: 'Cleanup - Unload Night', toolName: 'manage_level', arguments: { action: 'stream', levelPath: '/Game/Maps/TC_SubLevel_Night', shouldBeLoaded: false }, expected: 'success' },
  { scenario: 'Validate level', toolName: 'manage_level', arguments: { action: 'validate_level', levelPath: '/Game/Maps/TC_TestLevel_Renamed' }, expected: 'success' },
  {
    scenario: "Error: Invalid lighting quality",
    toolName: "manage_level",
    arguments: { action: "build_lighting", quality: "InvalidQuality" },
    expected: "error|unknown_quality"
  },
  {
    scenario: "Edge: Zero intensity light",
    toolName: "manage_level",
    arguments: { action: "create_light", lightType: "Point", intensity: 0 },
    expected: "success"
  },
  {
    scenario: "Border: Invalid level path",
    toolName: "manage_level",
    arguments: { action: "load", levelPath: "/Game/InvalidLevel" },
    expected: "not_found|error"
  },
  {
    scenario: "Edge: Stream unloaded level",
    toolName: "manage_level",
    arguments: { action: "stream", levelName: "NonLoadedLevel", shouldBeLoaded: true },
    expected: "success|handled"
  },
  {
    scenario: "Error: Missing action param",
    toolName: "manage_level",
    arguments: {},
    expected: "validation_error"
  }
];

await runToolTests('Manage Level', testCases);
