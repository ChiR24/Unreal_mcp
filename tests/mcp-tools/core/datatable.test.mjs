#!/usr/bin/env node

import { runToolTests } from '../../test-runner.mjs';

const TEST_FOLDER = '/Game/MCPTest/CoreDataTables';
const ts = Date.now();

const testCases = [
  // === SETUP ===
  { scenario: 'Setup: create test folder', toolName: 'manage_asset', arguments: { action: 'create_folder', path: TEST_FOLDER }, expected: 'success|already exists' },

  // === DATATABLE AUTHORING (struct ecosystem) ===
  { scenario: 'DATATABLE: create_row_struct', toolName: 'manage_asset', arguments: { action: 'create_row_struct', name: `S_MCP_Row_${ts}`, path: TEST_FOLDER, save: true }, expected: 'success', captureResult: { key: 'rowStructPath', fromField: 'result.assetPath' }, assertions: [{ path: 'structuredContent.result.created', equals: true, label: 'row struct created' }] },
  { scenario: 'DATATABLE: set_struct_as_row_struct', toolName: 'manage_asset', arguments: { action: 'set_struct_as_row_struct', structPath: '${captured:rowStructPath}', save: true }, expected: 'success', assertions: [{ path: 'structuredContent.result.set', equals: true, label: 'struct finalized as row struct' }] },
  { scenario: 'DATATABLE: create_data_table', toolName: 'manage_asset', arguments: { action: 'create_data_table', name: `DT_MCP_${ts}`, path: TEST_FOLDER, rowStructPath: '${captured:rowStructPath}', save: true }, expected: 'success', captureResult: { key: 'dataTablePath', fromField: 'result.assetPath' }, assertions: [{ path: 'structuredContent.result.created', equals: true, label: 'data table created' }, { path: 'structuredContent.result.hasRowStruct', equals: true, label: 'row struct bound at creation' }] },
  { scenario: 'DATATABLE: get_row_struct', toolName: 'manage_asset', arguments: { action: 'get_row_struct', dataTablePath: '${captured:dataTablePath}' }, expected: 'success', assertions: [{ path: 'structuredContent.result.hasRowStruct', equals: true, label: 'row struct resolvable' }] },
  { scenario: 'DATATABLE: set_data_table_row_struct', toolName: 'manage_asset', arguments: { action: 'set_data_table_row_struct', dataTablePath: '${captured:dataTablePath}', rowStructPath: '${captured:rowStructPath}', save: true }, expected: 'success', assertions: [{ path: 'structuredContent.result.updated', equals: true, label: 'row struct rebound to table' }] },
  { scenario: 'DATATABLE: add_data_table_row', toolName: 'manage_asset', arguments: { action: 'add_data_table_row', dataTablePath: '${captured:dataTablePath}', rowName: 'RowOne', rowData: { SomeInt: 7, SomeName: 'Alpha' }, save: true }, expected: 'success', captureResult: { key: 'rowName', fromField: 'result.rowName' }, assertions: [{ path: 'structuredContent.result.added', equals: true, label: 'row added flag' }] },
  { scenario: 'DATATABLE: add_data_table_row (second)', toolName: 'manage_asset', arguments: { action: 'add_data_table_row', dataTablePath: '${captured:dataTablePath}', rowName: 'RowTwo', rowData: { SomeInt: 42, SomeName: 'Beta' }, save: true }, expected: 'success' },
  { scenario: 'DATATABLE: get_data_table_row', toolName: 'manage_asset', arguments: { action: 'get_data_table_row', dataTablePath: '${captured:dataTablePath}', rowName: '${captured:rowName}' }, expected: 'success', assertions: [{ path: 'structuredContent.result.found', equals: true, label: 'row found' }] },
  { scenario: 'DATATABLE: update_data_table_row', toolName: 'manage_asset', arguments: { action: 'update_data_table_row', dataTablePath: '${captured:dataTablePath}', rowName: 'RowOne', rowData: { SomeInt: 99, SomeName: 'Updated' }, save: true }, expected: 'success', assertions: [{ path: 'structuredContent.result.updated', equals: true, label: 'row updated flag' }] },
  { scenario: 'DATATABLE: list_data_table_rows', toolName: 'manage_asset', arguments: { action: 'list_data_table_rows', dataTablePath: '${captured:dataTablePath}' }, expected: 'success', assertions: [{ path: 'structuredContent.result.rows', length: 2, label: 'two rows listed' }] },
  { scenario: 'DATATABLE: delete_data_table_row', toolName: 'manage_asset', arguments: { action: 'delete_data_table_row', dataTablePath: '${captured:dataTablePath}', rowName: 'RowTwo', save: true }, expected: 'success', assertions: [{ path: 'structuredContent.result.removed', equals: true, label: 'row removed flag' }] },
  { scenario: 'DATATABLE: import_data_table_rows', toolName: 'manage_asset', arguments: { action: 'import_data_table_rows', dataTablePath: '${captured:dataTablePath}', rows: [{ rowName: 'RowThree', rowData: { SomeInt: 1, SomeName: 'Gamma' } }, { rowName: 'RowFour', rowData: { SomeInt: 2, SomeName: 'Delta' } }], clearExisting: false, save: true }, expected: 'success', assertions: [{ path: 'structuredContent.result.imported', equals: 2, label: 'two rows imported' }] },
  { scenario: 'DATATABLE: clear_data_table_rows', toolName: 'manage_asset', arguments: { action: 'clear_data_table_rows', dataTablePath: '${captured:dataTablePath}', save: true }, expected: 'success', assertions: [{ path: 'structuredContent.result.cleared', equals: true, label: 'rows cleared flag' }] },

  // === DATATABLE ERROR CASES ===
  { scenario: 'DATATABLE ERROR: add row missing name', toolName: 'manage_asset', arguments: { action: 'add_data_table_row', dataTablePath: '${captured:dataTablePath}', rowData: { SomeInt: 1 } }, expected: 'error', assertions: [{ path: 'structuredContent.error', includes: 'MISSING_PARAMETER', label: 'missing row name reported' }] },
  { scenario: 'DATATABLE ERROR: create data table missing path', toolName: 'manage_asset', arguments: { action: 'create_data_table', name: `DT_MCP_Bad_${ts}` }, expected: 'error', assertions: [{ path: 'structuredContent.error', includes: 'MISSING_PARAMETER', label: 'missing path reported' }] },
  { scenario: 'DATATABLE ERROR: get row on missing table', toolName: 'manage_asset', arguments: { action: 'get_data_table_row', dataTablePath: `${TEST_FOLDER}/DT_DoesNotExist_${ts}`, rowName: 'RowOne' }, expected: 'error', assertions: [{ path: 'structuredContent.error', includes: 'ASSET_NOT_FOUND', label: 'missing table reported' }] },
  { scenario: 'DATATABLE ERROR: delete row on missing table', toolName: 'manage_asset', arguments: { action: 'delete_data_table_row', dataTablePath: `${TEST_FOLDER}/DT_DoesNotExist_${ts}`, rowName: 'RowOne' }, expected: 'error', assertions: [{ path: 'structuredContent.error', includes: 'ASSET_NOT_FOUND', label: 'missing table reported' }] },

  // === CLEANUP ===
  { scenario: 'Cleanup: delete test folder', toolName: 'manage_asset', arguments: { action: 'delete', path: TEST_FOLDER, force: true }, expected: 'success|not found' },
];

runToolTests('manage_asset', testCases);
