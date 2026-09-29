// DataTable records: create, row-struct binding, row CRUD, import, clear
// (issue #struct-ecosystem).

import type { RecordSpec } from './builder.js';
import { arrObj, bool, DESTRUCTIVE, DESTRUCTIVE_POLICY, ex, LOW, MEDIUM, NON_IDEMPOTENT, READ, READ_POLICY, r, str, WRITE, WRITE_POLICY } from './builder.js';
import { schema } from '../shared/record-presets.js';

const DT_PATH = str('Asset path of the DataTable (e.g. /Game/DataTables/DT_MyTable).');
const ROW_STRUCT = str('Asset path of the row UScriptStruct.');
const ROW_NAME = str('Name of the row.');
// Every DataTable write reads `save` (McpDataTableSaveIfRequested) and
// defaults it to true: an edit kept only in memory died with the editor.
const SAVE = bool('Persist the asset to disk. Defaults to true; pass false to keep the change in memory only.');
const MIGRATE = bool('Re-import the existing rows under the new struct (default true); rows that do not fit are reported in invalidRows.');
const CLEAR_ROWS = bool('Drop every existing row first (default false): before an import, or instead of migrating rows to a new row struct.');
const OK = schema({ success: bool('Operation succeeded.'), details: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Operation details.' } }, ['success']);

const DT = '/Game/DataTables/DT_Weapons';
const ROW = '/Game/Structs/S_WeaponRow';
const DONE = { success: true };

export const DATATABLE_RECORDS: readonly RecordSpec[] = [
  r('create_data_table', 'datatable', 'Create a new DataTable asset.', schema({ name: str('DataTable name.'), path: str('Package path (default /Game/DataTables).'), dataTablePath: str('Full asset path of the new table, e.g. /Game/DataTables/DT_Weapons; alternative to name and path.'), rowStructPath: ROW_STRUCT, save: SAVE }, ['rowStructPath'], ['name', 'dataTablePath']), OK, WRITE, WRITE_POLICY, MEDIUM,
    { examples: [ex('Create a weapons DataTable', { name: 'DT_Weapons', path: '/Game/DataTables', rowStructPath: ROW }, DONE)],
      whenToUse: ['A new DataTable is needed to hold rows of game data such as weapon stats or item lists; the row struct must already exist.'],
      whenNotToUse: ['A copy of an existing table is wanted, rows included (use asset.duplicate).'] }),
  r('set_data_table_row_struct', 'datatable', 'Bind a row UScriptStruct to an existing DataTable.', schema({ dataTablePath: DT_PATH, rowStructPath: ROW_STRUCT, migrateExistingRows: MIGRATE, clearExisting: CLEAR_ROWS, save: SAVE }, ['dataTablePath', 'rowStructPath']), OK, WRITE, WRITE_POLICY, LOW,
    { examples: [ex('Bind the row struct after creation', { dataTablePath: DT, rowStructPath: ROW }, DONE)],
      whenToUse: ['A table must be bound to a different row struct; existing rows are migrated unless clearExisting drops them.'],
      whenNotToUse: ['The struct itself needs new or changed columns first (use struct.edit_struct).'] }),
  r('create_row_struct', 'datatable', 'Create a new UScriptStruct suitable as a DataTable row type.', schema({ name: str('Struct name.'), path: str('Package path (default /Game/Structs).'), rowStructPath: str('Full asset path of the new struct, e.g. /Game/Structs/S_WeaponRow; alternative to name and path.'), members: arrObj('Member definitions.'), save: SAVE }, [], ['name', 'rowStructPath']), OK, WRITE, WRITE_POLICY, MEDIUM,
    { examples: [ex('Create a weapon row struct', { name: 'S_WeaponRow', path: '/Game/Structs', members: [{ memberName: 'Damage', memberType: 'Float' }] }, DONE)],
      whenToUse: ['A new struct with named, typed columns is needed as the row type of a table.'],
      whenNotToUse: ['An existing struct only needs columns added, renamed or retyped (use struct.edit_struct).'] }),
  r('get_row_struct', 'datatable', 'Retrieve the row UScriptStruct bound to a DataTable.', schema({ dataTablePath: DT_PATH }, ['dataTablePath']), OK, READ, READ_POLICY, LOW,
    { examples: [ex('Read which struct backs a table', { dataTablePath: DT }, DONE)],
      whenToUse: ['The row struct behind a table must be identified before rows are added or read.'],
      whenNotToUse: ['The columns and types of that struct must be read (use struct.get_struct).'] }),
  r('set_struct_as_row_struct', 'datatable', 'Set an existing UScriptStruct as the row struct for a DataTable (a user-defined struct is validated and compiled first).', schema({ dataTablePath: DT_PATH, rowStructPath: ROW_STRUCT, migrateExistingRows: MIGRATE, clearExisting: CLEAR_ROWS, save: SAVE }, ['dataTablePath', 'rowStructPath']), OK, WRITE, WRITE_POLICY, LOW,
    { examples: [ex('Repoint a table at an existing struct', { dataTablePath: DT, rowStructPath: ROW }, DONE)],
      whenToUse: ['A table must be bound to a different row struct; existing rows are migrated unless clearExisting drops them.'],
      whenNotToUse: ['The struct itself needs new or changed columns first (use struct.edit_struct).'] }),
  r('add_data_table_row', 'datatable', 'Add a new row to a DataTable.', schema({ dataTablePath: DT_PATH, rowName: ROW_NAME, rowData: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Row data key/value map.' }, save: SAVE }, ['dataTablePath', 'rowName', 'rowData']), OK, WRITE, WRITE_POLICY, LOW,
    { examples: [ex('Add a rifle row', { dataTablePath: DT, rowName: 'Rifle', rowData: { Damage: 32, FireRate: 0.12 } }, DONE)],
      whenToUse: ['A new row must be added by name; columns left out of rowData take the row struct defaults.'],
      whenNotToUse: ['The row must be read or listed, not written (use datatable.inspect_data_table).'] }),
  r('get_data_table_row', 'datatable', 'Retrieve a row from a DataTable by name.', schema({ dataTablePath: DT_PATH, rowName: ROW_NAME }, ['dataTablePath', 'rowName']), OK, READ, READ_POLICY, LOW,
    { examples: [ex('Read the rifle row', { dataTablePath: DT, rowName: 'Rifle' }, DONE)],
      whenToUse: ['One row must be read by name to check its column values; a missing row comes back as found false.'],
      whenNotToUse: ['The DataTable asset itself, not its rows, must be examined (use asset.inspect_asset).'] }),
  r('update_data_table_row', 'datatable', 'Update an existing row in a DataTable.', schema({ dataTablePath: DT_PATH, rowName: ROW_NAME, rowData: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Updated row data.' }, save: SAVE }, ['dataTablePath', 'rowName', 'rowData']), OK, NON_IDEMPOTENT, WRITE_POLICY, LOW,
    { examples: [ex('Rebalance the rifle damage', { dataTablePath: DT, rowName: 'Rifle', rowData: { Damage: 28 } }, DONE)],
      whenToUse: ['Some columns of an existing row must change while the columns not named keep their values.'],
      whenNotToUse: ['A row must be removed instead of changed (use datatable.delete_data_table_row).'] }),
  r('delete_data_table_row', 'datatable', 'Delete a row from a DataTable.', schema({ dataTablePath: DT_PATH, rowName: ROW_NAME, save: SAVE }, ['dataTablePath', 'rowName']), OK, { ...DESTRUCTIVE, longRunning: false }, DESTRUCTIVE_POLICY, LOW,
    { examples: [ex('Remove a retired weapon row', { dataTablePath: DT, rowName: 'Musket' }, DONE)],
      whenToUse: ['One named row must be removed from a table; a rowName that does not exist is reported as ROW_NOT_FOUND.'],
      whenNotToUse: ['The whole DataTable asset must be deleted, not one row (use asset.delete).', 'A row must be changed rather than removed (use datatable.edit_data_table).'] }),
  r('list_data_table_rows', 'datatable', 'List all rows in a DataTable.', schema({ dataTablePath: DT_PATH }, ['dataTablePath']), OK, READ, READ_POLICY, LOW,
    { examples: [ex('List every weapon row', { dataTablePath: DT }, DONE)],
      whenToUse: ['Every row and its values must be seen at once; at most 200 rows are returned, with the full rowCount.'],
      whenNotToUse: ['The DataTable asset must first be found in the project (use asset.query_asset).'] }),
  r('import_data_table_rows', 'datatable', 'Import multiple rows into a DataTable from an array.', schema({ dataTablePath: DT_PATH, rows: arrObj('Rows to import, each { rowName: <name>, rowData: { <field>: <value>, ... } }. The field names inside rowData are the row struct own property names, e.g. { rowName: ArcRifle, rowData: { DisplayName: Arc Rifle, Damage: 42 } }. A flat entry such as { rowName, Damage } is rejected as missing rowData.'), clearExisting: CLEAR_ROWS, save: SAVE }, ['dataTablePath', 'rows']), OK, WRITE, WRITE_POLICY, MEDIUM,
    { examples: [ex('Replace the table contents in bulk', { dataTablePath: DT, rows: [{ rowName: 'Pistol', rowData: { Damage: 18 } }, { rowName: 'Rifle', rowData: { Damage: 32 } }], clearExisting: true }, DONE)],
      whenToUse: ['Many rows must be loaded in one call; each named row is replaced whole and columns it omits reset to defaults.'],
      whenNotToUse: ['The table only needs emptying, with nothing loaded afterwards (use datatable.delete_data_table_row with deleteScope=all).'] }),
  r('clear_data_table_rows', 'datatable', 'Clear all rows from a DataTable.', schema({ dataTablePath: DT_PATH, save: SAVE }, ['dataTablePath']), OK, { ...DESTRUCTIVE, longRunning: false }, DESTRUCTIVE_POLICY, LOW,
    { examples: [ex('Empty a table before reimport', { dataTablePath: DT }, DONE)],
      whenToUse: ['Every row must go while the table asset and its row struct stay, with nothing loaded afterwards.'],
      whenNotToUse: ['The table asset itself must go, not just its rows (use asset.delete).', 'The table is being reloaded with new rows (use datatable.edit_data_table with edit=import_rows and clearExisting=true, which validates first).'] })
];
