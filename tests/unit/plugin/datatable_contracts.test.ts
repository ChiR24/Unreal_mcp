import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

const pluginDataTablesDir = resolve(
  process.cwd(),
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/AssetWorkflow/DataTables',
);

const read = (file: string): string => readFileSync(file, 'utf8');

const lifecycle = read(`${pluginDataTablesDir}/LifecycleDataTables.cpp`);
const rows = read(`${pluginDataTablesDir}/Rows.cpp`);
const shared = read(`${pluginDataTablesDir}/Shared.h`);

// All DataTable C++ shards concatenated, so the contract greps see the union of
// required UE API usage and the canonical action-name string literals regardless
// of which shard owns a given token.
const allDataTableSources = lifecycle + '\n' + rows + '\n' + shared;

// Every DataTable action-name literal the TS surface must send verbatim.
const actionLiterals = [
  'create_data_table',
  'set_data_table_row_struct',
  'create_row_struct',
  'get_row_struct',
  'set_struct_as_row_struct',
  'add_data_table_row',
  'get_data_table_row',
  'update_data_table_row',
  'delete_data_table_row',
  'list_data_table_rows',
  'import_data_table_rows',
  'clear_data_table_rows',
];

describe('DataTable + RowStruct authoring contracts (struct ecosystem)', () => {
  it('exposes a single HandleDataTableAction entry point', () => {
    // Given / When: all 12 DataTable actions funnel through one dispatcher.
    // Then
    expect(allDataTableSources).toContain('HandleDataTableAction');
  });

  it('declares every DataTable action-name literal verbatim', () => {
    // Given / When: the C++ shards must branch on the exact action strings
    // the TypeScript layer emits (TEXT("create_data_table") etc.).
    // Then
    for (const action of actionLiterals) {
      expect(allDataTableSources, `missing action literal: ${action}`).toContain(action);
    }
  });

  it('uses the UDataTable creation API', () => {
    // Given / When: create_data_table must construct a UDataTable and bind a
    // row struct rather than hand-rolling the package.
    // Then
    expect(allDataTableSources).toContain('NewObject<UDataTable>');
  });

  it('adds rows through UDataTable::AddRow', () => {
    // Given / When: add_data_table_row and import_data_table_rows must append
    // rows via the engine API, not manual map mutation.
    // Then
    expect(rows).toContain('AddRow');
  });

  it('reads rows through UDataTable::FindRow', () => {
    // Given / When: get_data_table_row resolves a single row instance.
    // Then
    expect(rows).toContain('FindRow');
  });

  it('removes rows through UDataTable::RemoveRow', () => {
    // Given / When: delete_data_table_row and clear_data_table_rows delete rows.
    // Then
    expect(rows).toContain('RemoveRow');
  });

  it('binds and resolves the row struct via DataTable->RowStruct', () => {
    // Given / When: set_data_table_row_struct / create_row_struct / get_row_struct
    // all operate on the table's RowStruct member.
    // Then
    expect(allDataTableSources).toContain('RowStruct');
  });

  it('persists only through the safe save wrapper', () => {
    // Given / When: every save-gated handler must call McpSafeAssetSave and
    // must never call UPackage::SavePackage directly.
    // Then
    expect(allDataTableSources).toContain('McpSafeAssetSave');
  });

  it('never calls UPackage::SavePackage in any DataTable shard', () => {
    // Given / When: the AGENTS safety rule forbids direct package saves in
    // domain handlers.
    // Then
    expect(lifecycle).not.toContain('UPackage::SavePackage');
    expect(rows).not.toContain('UPackage::SavePackage');
  });
});
