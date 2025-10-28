#!/usr/bin/env node
/**
 * Asset Management Test Suite (condensed)
 * Tool: manage_asset
 *
 * This file contains a focused 15-case suite that bootstraps the
 * minimal assets required and cleans up after itself. It avoids
 * large, editor-version-specific operations.
 */

import { runToolTests } from './test-runner.mjs';

const testCases = [
  // 1. List root content (smoke test)
  {
    scenario: 'List /Game root assets',
    toolName: 'manage_asset',
    arguments: { action: 'list', directory: '/Game' },
    expected: 'returns list of assets'
  },

  // 2. Create a focused folder for tests
  {
    scenario: 'Create test folder',
    toolName: 'manage_asset',
    arguments: { action: 'create_folder', path: '/Game/Tests/ManageAsset' },
    expected: 'success - folder structure created'
  },

  // 3. Create a master material used by later cases
  {
    scenario: 'Create master material',
    toolName: 'manage_asset',
    arguments: { action: 'create_material', name: 'M_MasterMaterial_Test', path: '/Game/Tests/ManageAsset', materialType: 'Master' },
    expected: 'success - master material created'
  },

  // 4. Create a material instance referencing the master
  {
    scenario: 'Create material instance',
    toolName: 'manage_asset',
    arguments: { action: 'create_material_instance', name: 'MI_TestInstance', path: '/Game/Tests/ManageAsset', parentMaterial: '/Game/Tests/ManageAsset/M_MasterMaterial_Test', parameters: { BaseColor: [0.5,0.5,0.8,1.0] } },
    expected: 'success - material instance created'
  },

  // 5. List the test folder to verify created assets
  {
    scenario: 'List test folder assets',
    toolName: 'manage_asset',
    arguments: { action: 'list', directory: '/Game/Tests/ManageAsset' },
    expected: 'returns assets'
  },

  // 6. Duplicate material instance
  {
    scenario: 'Duplicate material instance',
    toolName: 'manage_asset',
    arguments: { action: 'duplicate', sourcePath: '/Game/Tests/ManageAsset/MI_TestInstance', destinationPath: '/Game/Tests/ManageAsset/Copies', newName: 'MI_TestInstance_Copy', save: true },
    expected: 'success - asset duplicated with new name'
  },

  // 7. Rename duplicated material
  {
    scenario: 'Rename duplicated material',
    toolName: 'manage_asset',
    arguments: { action: 'rename', assetPath: '/Game/Tests/ManageAsset/Copies/MI_TestInstance_Copy', newName: 'MI_TestInstance_Renamed' },
    expected: 'success - asset renamed'
  },

  // 8. Get dependencies for the created instance
  {
    scenario: 'Get dependencies of material instance',
    toolName: 'manage_asset',
    arguments: { action: 'get_dependencies', assetPath: '/Game/Tests/ManageAsset/MI_TestInstance' },
    expected: 'success - asset dependencies retrieved'
  },

  // 9. Create a thumbnail for the material
  {
    scenario: 'Create thumbnail for material',
    toolName: 'manage_asset',
    arguments: { action: 'create_thumbnail', assetPath: '/Game/Tests/ManageAsset/M_MasterMaterial_Test' },
    expected: 'success - thumbnail created'
  },

  // 10. Set asset tags
  {
    scenario: 'Set tags on material',
    toolName: 'manage_asset',
    arguments: { action: 'set_tags', assetPath: '/Game/Tests/ManageAsset/M_MasterMaterial_Test', tags: ['TestMaterial','AutoCreated'] },
    expected: 'success - asset tags set'
  },

  // 11. List assets with filter
  {
    scenario: 'List materials in test folder',
    toolName: 'manage_asset',
    arguments: { action: 'list', directory: '/Game/Tests/ManageAsset', filter: 'Material' },
    expected: 'success - filtered assets listed'
  },

  // 12. Generate a lightweight asset report (mock-friendly)
  {
    scenario: 'Generate asset report',
    toolName: 'manage_asset',
    arguments: { action: 'generate_report', directory: '/Game/Tests/ManageAsset', reportType: 'Size', outputPath: './tests/reports/asset_report_test.json' },
    expected: 'success - asset report generated'
  },

  // 13. Validate asset integrity (best-effort)
  {
    scenario: 'Validate material integrity',
    toolName: 'manage_asset',
    arguments: { action: 'validate', assetPath: '/Game/Tests/ManageAsset/M_MasterMaterial_Test' },
    expected: 'success - asset validated'
  },

  // 14. Delete created copies and instances (cleanup)
  {
    scenario: 'Delete test copies and instances',
    toolName: 'manage_asset',
    arguments: { action: 'delete', assetPaths: ['/Game/Tests/ManageAsset/MI_TestInstance', '/Game/Tests/ManageAsset/Copies/MI_TestInstance_Renamed', '/Game/Tests/ManageAsset/M_MasterMaterial_Test'] },
    expected: 'success - assets deleted'
  },

  // 15. Delete the test folder
  {
    scenario: 'Remove test folder',
    toolName: 'manage_asset',
    arguments: { action: 'delete', assetPath: '/Game/Tests/ManageAsset' },
    expected: 'success - asset deleted'
  },
  { scenario: 'Import placeholder asset (no-op safe)', toolName: 'manage_asset', arguments: { action: 'import', sourcePath: './tests/fixtures/missing.fbx', destinationPath: '/Game/Tests/ManageAsset' }, expected: 'success or handled' },
  { scenario: 'Fixup redirectors', toolName: 'manage_asset', arguments: { action: 'fixup_redirectors', directory: '/Game/Tests/ManageAsset' }, expected: 'NOT_IMPLEMENTED' },
  { scenario: 'Find assets by tag', toolName: 'manage_asset', arguments: { action: 'find_by_tag', tag: 'AutoCreated' }, expected: 'NOT_IMPLEMENTED' },
  { scenario: 'Get asset metadata', toolName: 'manage_asset', arguments: { action: 'get_metadata', assetPath: '/Game/Tests/ManageAsset/M_MasterMaterial_Test' }, expected: 'NOT_IMPLEMENTED' },
  { scenario: 'Set asset metadata', toolName: 'manage_asset', arguments: { action: 'set_metadata', assetPath: '/Game/Tests/ManageAsset/M_MasterMaterial_Test', metadata: { Author: 'TC' } }, expected: 'NOT_IMPLEMENTED' },
  { scenario: 'Bulk delete (final cleanup)', toolName: 'manage_asset', arguments: { action: 'delete', assetPaths: ['/Game/Tests/ManageAsset'] }, expected: 'success or handled' }
];

await runToolTests('Asset Management', testCases);
