// Enum records: UserDefinedEnum CRUD, value management, metadata,
// reordering, and split (issue #struct-ecosystem).

import type { RecordSpec } from './builder.js';
import { arr, bool, DESTRUCTIVE, DESTRUCTIVE_POLICY, LOW, MEDIUM, NON_IDEMPOTENT, num, READ, READ_POLICY, r, schema, str, WRITE, WRITE_POLICY } from './builder.js';

const ENUM_PATH = str('Asset path of the UserDefinedEnum (e.g. /Game/Enums/E_MyEnum).');
const VALUE_NAME = str('Enum value (entry) name.');
const OK = schema({ success: bool('Operation succeeded.'), details: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Operation details.' } }, ['success']);

export const ENUM_RECORDS: readonly RecordSpec[] = [
  r('create_enum', 'enum', 'Create a new UserDefinedEnum asset.', schema({ name: str('Enum name.'), path: str('Package path.'), values: arr('Initial enum value names.') }, ['name']), OK, WRITE, WRITE_POLICY, MEDIUM, { dispatchMode: 'tool' }),
  r('delete_enum', 'enum', 'Delete a UserDefinedEnum asset.', schema({ enumPath: ENUM_PATH }, ['enumPath']), OK, { ...DESTRUCTIVE, longRunning: false }, DESTRUCTIVE_POLICY, LOW, { dispatchMode: 'tool' }),
  r('get_enum', 'enum', 'Retrieve UserDefinedEnum metadata and values.', schema({ enumPath: ENUM_PATH }, ['enumPath']), OK, READ, READ_POLICY, LOW, { dispatchMode: 'tool' }),
  r('add_enum_value', 'enum', 'Add a new value to a UserDefinedEnum.', schema({ enumPath: ENUM_PATH, valueName: VALUE_NAME }, ['enumPath', 'valueName']), OK, WRITE, WRITE_POLICY, LOW, { dispatchMode: 'tool' }),
  r('remove_enum_value', 'enum', 'Remove a value from a UserDefinedEnum.', schema({ enumPath: ENUM_PATH, valueName: VALUE_NAME }, ['enumPath', 'valueName']), OK, { ...DESTRUCTIVE, longRunning: false }, DESTRUCTIVE_POLICY, LOW, { dispatchMode: 'tool' }),
  r('rename_enum_value', 'enum', 'Rename a value in a UserDefinedEnum.', schema({ enumPath: ENUM_PATH, valueName: VALUE_NAME, newValueName: str('New enum value name.') }, ['enumPath', 'valueName', 'newValueName']), OK, NON_IDEMPOTENT, WRITE_POLICY, LOW, { dispatchMode: 'tool' }),
  r('reorder_enum_values', 'enum', 'Reorder values in a UserDefinedEnum.', schema({ enumPath: ENUM_PATH, order: arr('Desired value name order.') }, ['enumPath', 'order']), OK, WRITE, WRITE_POLICY, LOW, { dispatchMode: 'tool' }),
  r('set_enum_value_metadata', 'enum', 'Set metadata (tooltip, etc.) on an enum value.', schema({ enumPath: ENUM_PATH, valueName: VALUE_NAME, key: str('Metadata key.'), value: { description: 'Metadata value.' } }, ['enumPath', 'valueName', 'key']), OK, WRITE, WRITE_POLICY, LOW, { dispatchMode: 'tool' }),
  r('split_enum', 'enum', 'Split a UserDefinedEnum into a new enum with selected values.', schema({ enumPath: ENUM_PATH, newEnumName: str('Name for the new enum.'), values: arr('Value names to move to the new enum.'), index: num('Split index.') }, ['enumPath', 'newEnumName']), OK, WRITE, WRITE_POLICY, MEDIUM, { dispatchMode: 'tool' })
];
