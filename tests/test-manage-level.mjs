#!/usr/bin/env node
/**
 * Condensed Level Management Test Suite (15 cases) — safe for real Editor runs.
 * Tool: manage_level
 */

import { runToolTests } from './test-runner.mjs';

const testCases = [
  { scenario: 'Save current level', toolName: 'manage_level', arguments: { action: 'save' }, expected: 'failure - save failed' },
  { scenario: 'Create test level (best-effort)', toolName: 'manage_level', arguments: { action: 'create_level', levelName: 'TC_TestLevel', streaming: false }, expected: 'success - level created' },
  { scenario: 'Load test level', toolName: 'manage_level', arguments: { action: 'load', levelPath: '/Engine/Maps/Entry' }, expected: 'success - level loaded' },
  { scenario: 'Stream sublevel (create then stream)', toolName: 'manage_level', arguments: { action: 'create_level', levelName: 'TC_SubLevel', streaming: true }, expected: 'success - command executed' },
  { scenario: 'Stream sublevel (make visible)', toolName: 'manage_level', arguments: { action: 'stream', levelPath: '/Game/Maps/TC_SubLevel', shouldBeLoaded: true, shouldBeVisible: true }, expected: 'success - sublevel streamed' },
  { scenario: 'Create point light', toolName: 'manage_level', arguments: { action: 'create_light', lightType: 'Point', location: { x: 200, y: 200, z: 100 }, intensity: 5000.0 }, expected: 'success - light spawned' },
  { scenario: 'Create directional light', toolName: 'manage_level', arguments: { action: 'create_light', lightType: 'Directional', rotation: { pitch: -45, yaw: 0, roll: 0 }, intensity: 3.0 }, expected: 'success - light spawned' },
  { scenario: 'Quick build lighting (preview)', toolName: 'manage_level', arguments: { action: 'build_lighting', quality: 'Preview' }, expected: 'success - lighting built' },
  { scenario: 'Export level to package (best-effort)', toolName: 'manage_level', arguments: { action: 'export_level', levelPath: '/Engine/Maps/Entry', outputPath: './tests/reports/tc_testlevel_export' }, expected: 'not_implemented' },
  { scenario: 'Import level (no-op placeholder)', toolName: 'manage_level', arguments: { action: 'import_level', filePath: './tests/reports/tc_testlevel_export' }, expected: 'not_implemented' },
  { scenario: 'List open levels', toolName: 'manage_level', arguments: { action: 'list_levels' }, expected: 'not_implemented' },
  { scenario: 'Get level summary', toolName: 'manage_level', arguments: { action: 'get_summary', levelPath: '/Engine/Maps/Entry' }, expected: 'not_implemented' },
  { scenario: 'Save level as new', toolName: 'manage_level', arguments: { action: 'save', levelPath: '/Game/Maps/TC_TestLevel_SavedAs' }, expected: 'failure - save failed' },
  { scenario: 'Unload streaming level', toolName: 'manage_level', arguments: { action: 'stream', levelPath: '/Game/Maps/TC_SubLevel', shouldBeLoaded: false }, expected: 'success - sublevel unloaded' },
  { scenario: 'Delete test levels', toolName: 'manage_level', arguments: { action: 'delete', levelPath: '/Engine/Maps/Entry' }, expected: 'not_implemented' },
  // Additional
  { scenario: 'List actors in level', toolName: 'manage_level', arguments: { action: 'list_level_actors', levelPath: '/Engine/Maps/Entry' }, expected: 'not_implemented' },
  { scenario: 'Set world setting (best-effort)', toolName: 'manage_level', arguments: { action: 'set_world_settings', properties: { bEnableWorldComposition: false } }, expected: 'not_implemented' },
  { scenario: 'Rename level placeholder', toolName: 'manage_level', arguments: { action: 'rename_level', levelPath: '/Engine/Maps/Entry', newName: 'TC_TestLevel_Renamed' }, expected: 'not_implemented' },
  { scenario: 'Open level instance (placeholder)', toolName: 'manage_level', arguments: { action: 'open_level_instance', assetPath: '/Game/Maps/TC_SubLevel' }, expected: 'not_implemented' },
  { scenario: 'Validate level', toolName: 'manage_level', arguments: { action: 'validate_level', levelPath: '/Game/Maps/TC_TestLevel_Renamed' }, expected: 'not_implemented' }
];

await runToolTests('Manage Level', testCases);
