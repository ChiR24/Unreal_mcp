// DataTable records: create, row-struct binding, row CRUD, import, clear
// (issue #struct-ecosystem).

import type { RecordSpec } from './builder.js';
import { arrObj, bool, DESTRUCTIVE, DESTRUCTIVE_POLICY, LOW, MEDIUM, NON_IDEMPOTENT, READ, READ_POLICY, r, schema, str, WRITE, WRITE_POLICY } from './builder.js';

const DT_PATH = str('Asset path of the DataTable (e.g. /Game/DataTables/DT_MyTable).');
const ROW_STRUCT = str('Asset path of the row UScriptStruct.');
const ROW_NAME = str('Name of the row.');
const OK = schema({ success: bool('Operation succeeded.'), details: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Operation details.' } }, ['success']);

export const DATATABLE_RECORDS: readonly RecordSpec[] = [
  r('create_data_table', 'datatable', 'Create a new DataTable asset.', schema({ name: str('DataTable name.'), path: str('Package path.'), rowStructPath: ROW_STRUCT }, ['name']), OK, WRITE, WRITE_POLICY, MEDIUM, { dispatchMode: 'tool' }),
  r('set_data_table_row_struct', 'datatable', 'Bind a row UScriptStruct to an existing DataTable.', schema({ dataTablePath: DT_PATH, rowStructPath: ROW_STRUCT }, ['dataTablePath', 'rowStructPath']), OK, WRITE, WRITE_POLICY, LOW, { dispatchMode: 'tool' }),
  r('create_row_struct', 'datatable', 'Create a new UScriptStruct suitable as a DataTable row type.', schema({ name: str('Struct name.'), path: str('Package path.'), members: arrObj('Member definitions.') }, ['name']), OK, WRITE, WRITE_POLICY, MEDIUM, { dispatchMode: 'tool' }),
  r('get_row_struct', 'datatable', 'Retrieve the row UScriptStruct bound to a DataTable.', schema({ dataTablePath: DT_PATH }, ['dataTablePath']), OK, READ, READ_POLICY, LOW, { dispatchMode: 'tool' }),
  r('set_struct_as_row_struct', 'datatable', 'Set an existing UScriptStruct as the row struct for a DataTable.', schema({ dataTablePath: DT_PATH, rowStructPath: ROW_STRUCT }, ['dataTablePath', 'rowStructPath']), OK, WRITE, WRITE_POLICY, LOW, { dispatchMode: 'tool' }),
  r('add_data_table_row', 'datatable', 'Add a new row to a DataTable.', schema({ dataTablePath: DT_PATH, rowName: ROW_NAME, rowData: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Row data key/value map.' } }, ['dataTablePath', 'rowName', 'rowData']), OK, WRITE, WRITE_POLICY, LOW, { dispatchMode: 'tool' }),
  r('get_data_table_row', 'datatable', 'Retrieve a row from a DataTable by name.', schema({ dataTablePath: DT_PATH, rowName: ROW_NAME }, ['dataTablePath', 'rowName']), OK, READ, READ_POLICY, LOW, { dispatchMode: 'tool' }),
  r('update_data_table_row', 'datatable', 'Update an existing row in a DataTable.', schema({ dataTablePath: DT_PATH, rowName: ROW_NAME, rowData: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Updated row data.' } }, ['dataTablePath', 'rowName', 'rowData']), OK, NON_IDEMPOTENT, WRITE_POLICY, LOW, { dispatchMode: 'tool' }),
  r('delete_data_table_row', 'datatable', 'Delete a row from a DataTable.', schema({ dataTablePath: DT_PATH, rowName: ROW_NAME }, ['dataTablePath', 'rowName']), OK, { ...DESTRUCTIVE, longRunning: false }, DESTRUCTIVE_POLICY, LOW, { dispatchMode: 'tool' }),
  r('list_data_table_rows', 'datatable', 'List all rows in a DataTable.', schema({ dataTablePath: DT_PATH }, ['dataTablePath']), OK, READ, READ_POLICY, LOW, { dispatchMode: 'tool' }),
  r('import_data_table_rows', 'datatable', 'Import multiple rows into a DataTable from an array.', schema({ dataTablePath: DT_PATH, rows: arrObj('Row data objects.'), clearExisting: bool('Clear existing rows before import.') }, ['dataTablePath', 'rows']), OK, WRITE, WRITE_POLICY, MEDIUM, { dispatchMode: 'tool' }),
  r('clear_data_table_rows', 'datatable', 'Clear all rows from a DataTable.', schema({ dataTablePath: DT_PATH }, ['dataTablePath']), OK, { ...DESTRUCTIVE, longRunning: false }, DESTRUCTIVE_POLICY, LOW, { dispatchMode: 'tool' })
];
