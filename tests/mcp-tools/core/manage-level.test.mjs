#!/usr/bin/env node
/**
 * manage_level Tool Integration Tests
 * Covers all manage_level actions with real level package paths and deterministic cleanup.
 */

import { runToolTests } from '../../test-runner.mjs';

const TEST_FOLDER = '/Game/MCPTest/WorldAssets';
const ts = Date.now();
const MAIN_LEVEL = `${TEST_FOLDER}/LevelMain_${ts}`;
const SUB_LEVEL = `${TEST_FOLDER}/LevelSub_${ts}`;
const SAVE_AS_LEVEL = `${TEST_FOLDER}/LevelSaveAs_${ts}`;
const SAVE_LEVEL_AS = `${TEST_FOLDER}/LevelSaveLevelAs_${ts}`;
const EXPORTED_LEVEL = `${TEST_FOLDER}/LevelExported_${ts}`;
const IMPORTED_LEVEL = `${TEST_FOLDER}/LevelImported_${ts}`;
const DUPLICATED_LEVEL = `${TEST_FOLDER}/LevelDuplicated_${ts}`;
const RENAMED_LEVEL_NAME = `LevelRenamed_${ts}`;
const RENAMED_LEVEL = `${TEST_FOLDER}/${RENAMED_LEVEL_NAME}`;
const TEST_ACTOR = `LevelActor_${ts}`;

const testCases = [
  // === SETUP ===
  { scenario: 'Setup: create test folder', toolName: 'manage_asset', arguments: { action: 'create_folder', path: TEST_FOLDER }, expected: 'success|already exists' },

  // === CREATE / SAVE / LOAD ===
  { scenario: 'CREATE: create_level', toolName: 'manage_level', arguments: { action: 'create_level', levelName: `LevelMain_${ts}`, levelPath: TEST_FOLDER, template: '/Engine/Maps/Templates/OpenWorld', useWorldPartition: false, saveDirtyPackages: true }, expected: 'success|already exists' },
  { scenario: 'ACTION: save', toolName: 'manage_level', arguments: { action: 'save' }, expected: 'success' },
  { scenario: 'ACTION: save_level alias', toolName: 'manage_level', arguments: { action: 'save_level' }, expected: 'success' },
  { scenario: 'ACTION: save_as', toolName: 'manage_level', arguments: { action: 'save_as', savePath: SAVE_AS_LEVEL }, expected: 'success' },
  { scenario: 'ACTION: save_level_as', toolName: 'manage_level', arguments: { action: 'save_level_as', savePath: SAVE_LEVEL_AS }, expected: 'success' },
  { scenario: 'Setup: create sublevel', toolName: 'manage_level', arguments: { action: 'create_level', levelName: `LevelSub_${ts}`, levelPath: TEST_FOLDER, useWorldPartition: false, saveDirtyPackages: true }, expected: 'success|already exists' },
  { scenario: 'ACTION: load', toolName: 'manage_level', arguments: { action: 'load', levelPath: MAIN_LEVEL, streaming: false, saveDirtyPackages: true }, expected: 'success' },
  { scenario: 'ACTION: load_level alias', toolName: 'manage_level', arguments: { action: 'load_level', levelPath: MAIN_LEVEL, streaming: false, saveDirtyPackages: true }, expected: 'success' },
  { scenario: 'Setup: spawn actor in level', toolName: 'control_actor', arguments: { action: 'spawn', classPath: '/Engine/BasicShapes/Cube', actorName: TEST_ACTOR, location: { x: 0, y: 0, z: 100 } }, expected: 'success' },

  // === STREAMING ===
  { scenario: 'ACTION: stream', toolName: 'manage_level', arguments: { action: 'stream', levelPath: SUB_LEVEL, shouldBeLoaded: true, shouldBeVisible: true }, expected: 'success|already loaded' },
  { scenario: 'ACTION: unload', toolName: 'manage_level', arguments: { action: 'unload', levelPath: SUB_LEVEL }, expected: 'success|not loaded' },
  { scenario: 'ACTION: unload_level alias', toolName: 'manage_level', arguments: { action: 'unload_level', levelPath: SUB_LEVEL }, expected: 'success|not loaded' },
  { scenario: 'ADD: add_sublevel', toolName: 'manage_level', arguments: { action: 'add_sublevel', subLevelPath: SUB_LEVEL, parentLevel: MAIN_LEVEL, parentPath: TEST_FOLDER, streamingMethod: 'AlwaysLoaded' }, expected: 'success|already exists' },
  { scenario: 'ADD: add_sublevel via sublevelPath alias', toolName: 'manage_level', arguments: { action: 'add_sublevel', sublevelPath: SUB_LEVEL, streamingMethod: 'AlwaysLoaded' }, expected: 'success|already exists' },

  // === LIGHTING ===
  { scenario: 'CREATE: create_light', toolName: 'manage_level', arguments: { action: 'create_light', lightType: 'Point', name: `LevelLight_${ts}`, location: { x: 150, y: 0, z: 250 }, rotation: { pitch: -20, yaw: 0, roll: 0 }, intensity: 1500, color: [1, 0.85, 0.6, 1] }, expected: 'success|already exists' },
  { scenario: 'CREATE: build_lighting', toolName: 'manage_level', arguments: { action: 'build_lighting', quality: 'Preview' }, expected: 'success|already exists' },

  // === METADATA ===
  { scenario: 'CONFIG: set_metadata', toolName: 'manage_level', arguments: { action: 'set_metadata', assetPath: MAIN_LEVEL, metadata: { suite: 'manage_level', timestamp: ts } }, expected: 'success' },

  // === IMPORT / EXPORT / INFO ===
  { scenario: 'ACTION: export_level', toolName: 'manage_level', arguments: { action: 'export_level', levelPath: MAIN_LEVEL, exportPath: EXPORTED_LEVEL, timeoutMs: 45000 }, expected: 'success' },
  { scenario: 'ACTION: import_level', toolName: 'manage_level', arguments: { action: 'import_level', packagePath: MAIN_LEVEL, destinationPath: IMPORTED_LEVEL, timeoutMs: 45000 }, expected: 'success|already exists' },
  { scenario: 'INFO: list_levels', toolName: 'manage_level', arguments: { action: 'list_levels' }, expected: 'success' },
  { scenario: 'INFO: get_summary', toolName: 'manage_level', arguments: { action: 'get_summary', levelPath: MAIN_LEVEL }, expected: 'success' },
  { scenario: 'ACTION: validate_level', toolName: 'manage_level', arguments: { action: 'validate_level', levelPath: MAIN_LEVEL }, expected: 'success' },
  { scenario: 'INFO: get_current_level', toolName: 'manage_level', arguments: { action: 'get_current_level' }, expected: 'success' },

  // === ASSET OPERATIONS ===
  { scenario: 'ACTION: duplicate_level', toolName: 'manage_level', arguments: { action: 'duplicate_level', sourcePath: MAIN_LEVEL, destinationPath: DUPLICATED_LEVEL, targetPath: TEST_FOLDER, overwrite: false }, expected: 'success|already exists' },
  { scenario: 'ACTION: rename_level', toolName: 'manage_level', arguments: { action: 'rename_level', levelPath: DUPLICATED_LEVEL, newName: RENAMED_LEVEL_NAME, overwrite: false }, expected: 'success|already exists' },
  { scenario: 'DELETE: delete multiple levels', toolName: 'manage_level', arguments: { action: 'delete', levelPaths: [SAVE_AS_LEVEL, SAVE_LEVEL_AS, EXPORTED_LEVEL] }, expected: 'success|not found' },
  { scenario: 'DELETE: delete', toolName: 'manage_level', arguments: { action: 'delete', levelPath: RENAMED_LEVEL }, expected: 'success|not found' },
  { scenario: 'DELETE: delete_level path alias', toolName: 'manage_level', arguments: { action: 'delete_level', path: IMPORTED_LEVEL }, expected: 'success|not found' },

  // === CLEANUP ===
  { scenario: 'Cleanup: delete test actor', toolName: 'control_actor', arguments: { action: 'delete', actorName: TEST_ACTOR }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete test folder', toolName: 'manage_asset', arguments: { action: 'delete', path: TEST_FOLDER, force: true }, expected: 'success|not found' },
];

runToolTests('manage-level', testCases);
