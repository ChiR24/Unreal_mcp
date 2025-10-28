#!/usr/bin/env node
/**
 * Condensed Materials Test Suite (15 cases) — minimal safe operations.
 * Tool: manage_asset
 */

import { runToolTests } from './test-runner.mjs';

const testCases = [
  { scenario: 'Create base material', toolName: 'manage_asset', arguments: { action: 'create_material', name: 'M_BaseMaterial_TC', path: '/Game/Materials' }, expected: 'success - material created' },
  { scenario: 'Create master material', toolName: 'manage_asset', arguments: { action: 'create_material', name: 'M_MasterMaterial_TC', path: '/Game/Materials', materialType: 'Master' }, expected: 'success - master material created' },
  { scenario: 'Create material instance', toolName: 'manage_asset', arguments: { action: 'create_material_instance', name: 'MI_TestInstance_TC', path: '/Game/Materials', parentMaterial: '/Game/Materials/M_MasterMaterial_TC', parameters: { BaseColor: [0.8,0.2,0.2,1.0] } }, expected: 'success - material instance created' },
  { scenario: 'List materials folder', toolName: 'manage_asset', arguments: { action: 'list', directory: '/Game/Materials' }, expected: 'success - materials listed' },
  { scenario: 'Duplicate material instance', toolName: 'manage_asset', arguments: { action: 'duplicate', sourcePath: '/Game/Materials/MI_TestInstance_TC', destinationPath: '/Game/Materials/Copies', newName: 'MI_TestInstance_Copy_TC', save: true }, expected: 'success - duplicated' },
  { scenario: 'Rename duplicated material', toolName: 'manage_asset', arguments: { action: 'rename', assetPath: '/Game/Materials/Copies/MI_TestInstance_Copy_TC', newName: 'MI_TestInstance_Renamed_TC' }, expected: 'success - renamed' },
  { scenario: 'Get dependencies', toolName: 'manage_asset', arguments: { action: 'get_dependencies', assetPath: '/Game/Materials/MI_TestInstance_TC' }, expected: 'success - dependencies returned' },
  { scenario: 'Create thumbnail', toolName: 'manage_asset', arguments: { action: 'create_thumbnail', assetPath: '/Game/Materials/M_BaseMaterial_TC' }, expected: 'success - thumbnail created' },
  { scenario: 'Set tags', toolName: 'manage_asset', arguments: { action: 'set_tags', assetPath: '/Game/Materials/M_BaseMaterial_TC', tags: ['TC','Material'] }, expected: 'success - tags set' },
  { scenario: 'Validate asset', toolName: 'manage_asset', arguments: { action: 'validate', assetPath: '/Game/Materials/M_BaseMaterial_TC' }, expected: 'success - asset validated' },
  { scenario: 'Generate small report', toolName: 'manage_asset', arguments: { action: 'generate_report', directory: '/Game/Materials', reportType: 'Size', outputPath: './tests/reports/materials_report_tc.json' }, expected: 'success - report generated' },
  { scenario: 'Move material to subfolder', toolName: 'manage_asset', arguments: { action: 'move', assetPath: '/Game/Materials/M_BaseMaterial_TC', destinationPath: '/Game/Materials/TC' }, expected: 'success - moved' },
  { scenario: 'Delete duplicated assets', toolName: 'manage_asset', arguments: { action: 'delete', assetPaths: ['/Game/Materials/Copies/MI_TestInstance_Renamed_TC'] }, expected: 'success - deleted' },
  { scenario: 'Cleanup materials created', toolName: 'manage_asset', arguments: { action: 'delete', assetPaths: ['/Game/Materials/TC/M_BaseMaterial_TC', '/Game/Materials/MI_TestInstance_TC', '/Game/Materials/M_MasterMaterial_TC'] }, expected: 'success - cleanup done' },
  { scenario: 'Remove materials folder', toolName: 'manage_asset', arguments: { action: 'delete', assetPath: '/Game/Materials/TC' }, expected: 'success - folder removed' },
  // Additional
  { scenario: 'Set scalar parameter on MI', toolName: 'manage_asset', arguments: { action: 'set_material_parameter', assetPath: '/Game/Materials/MI_TestInstance_TC', parameterName: 'Scalar01', value: 0.25 }, expected: 'NOT_IMPLEMENTED' },
  { scenario: 'Set vector parameter on MI', toolName: 'manage_asset', arguments: { action: 'set_material_parameter', assetPath: '/Game/Materials/MI_TestInstance_TC', parameterName: 'BaseColor', value: [0.2,0.3,0.9,1.0] }, expected: 'NOT_IMPLEMENTED' },
  { scenario: 'Copy parameters from MI to MI copy', toolName: 'manage_asset', arguments: { action: 'copy_material_parameters', sourcePath: '/Game/Materials/MI_TestInstance_TC', destinationPath: '/Game/Materials/Copies/MI_TestInstance_Renamed_TC' }, expected: 'NOT_IMPLEMENTED' },
  { scenario: 'Reparent material instance', toolName: 'manage_asset', arguments: { action: 'reparent_material_instance', assetPath: '/Game/Materials/MI_TestInstance_TC', newParent: '/Game/Materials/M_MasterMaterial_TC' }, expected: 'NOT_IMPLEMENTED' },
  { scenario: 'Get material parameter values', toolName: 'manage_asset', arguments: { action: 'get_material_parameters', assetPath: '/Game/Materials/MI_TestInstance_TC' }, expected: 'NOT_IMPLEMENTED' }
];

await runToolTests('Materials', testCases);
