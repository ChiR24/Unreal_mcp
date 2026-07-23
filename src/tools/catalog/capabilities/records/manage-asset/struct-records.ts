// Struct authoring records: Blueprint Struct CRUD, member management,
// comparison, usage search, recompilation, import/export, and
// FInstancedStruct property access (issue #510, #struct-ecosystem).

import type { RecordSpec } from './builder.js';
import { arrObj, bool, DESTRUCTIVE, DESTRUCTIVE_POLICY, ex, HIGH, LOW, MEDIUM, NON_IDEMPOTENT, READ, READ_POLICY, r, schema, str, WRITE, WRITE_POLICY } from './builder.js';

const STRUCT_PATH = str('Asset path of the Blueprint Struct (e.g. /Game/Structs/S_MyStruct).');
const MEMBER_NAME = str('Member (variable) name.');
const MEMBER_TYPE = str('Unreal property type: Bool, Int, Float, String, Name, Text, Vector, Rotator, Transform, Object, SoftObject, Class, SoftClass, Enum:<Name>, Struct:<Path>, or with container prefix Array:..., Set:..., Map:<K>,<V>:');
const OK = schema({ success: bool('Operation succeeded.'), details: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Operation details.' } }, ['success']);

const S = '/Game/Structs/S_WeaponRow';
const DONE = { success: true };

export const STRUCT_RECORDS: readonly RecordSpec[] = [
  r('create_struct', 'struct', 'Create a new Blueprint Struct asset.', schema({ name: str('Struct name.'), path: str('Package path.'), members: arrObj('Member definitions.') }, ['name']), OK, WRITE, WRITE_POLICY, MEDIUM,
    { dispatchMode: 'tool', examples: [ex('Create a weapon row struct', { name: 'S_WeaponRow', path: '/Game/Structs', members: [{ memberName: 'Damage', memberType: 'Float' }] }, DONE)] }),
  r('get_struct', 'struct', 'Retrieve Blueprint Struct metadata and member list.', schema({ structPath: STRUCT_PATH }, ['structPath']), OK, READ, READ_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Read a struct\'s metadata', { structPath: S }, DONE)] }),
  r('read_struct', 'struct', 'Read the full value of a Blueprint Struct asset.', schema({ structPath: STRUCT_PATH }, ['structPath']), OK, READ, READ_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Read a struct\'s full value', { structPath: S }, DONE)] }),
  r('list_struct_members', 'struct', 'List all members of a Blueprint Struct.', schema({ structPath: STRUCT_PATH }, ['structPath']), OK, READ, READ_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('List a struct\'s members', { structPath: S }, DONE)] }),
  r('add_struct_member', 'struct', 'Add a new member to a Blueprint Struct.', schema({ structPath: STRUCT_PATH, memberName: MEMBER_NAME, memberType: MEMBER_TYPE, defaultValue: str('Default value as string.'), tooltip: str('Member tooltip.'), metadata: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Metadata key/value pairs.' } }, ['structPath', 'memberName', 'memberType']), OK, WRITE, WRITE_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Add a float damage member', { structPath: S, memberName: 'Damage', memberType: 'Float', defaultValue: '25.0', tooltip: 'Base damage per hit.' }, DONE)] }),
  r('remove_struct_member', 'struct', 'Remove a member from a Blueprint Struct.', schema({ structPath: STRUCT_PATH, memberName: MEMBER_NAME, varGuid: str('Stable member GUID.') }, ['structPath', 'memberName']), OK, { ...DESTRUCTIVE, longRunning: false }, DESTRUCTIVE_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Remove an obsolete member', { structPath: S, memberName: 'LegacyFlag' }, DONE)] }),
  r('rename_struct_member', 'struct', 'Rename a member in a Blueprint Struct.', schema({ structPath: STRUCT_PATH, memberName: MEMBER_NAME, newMemberName: str('New member name.'), varGuid: str('Stable member GUID.') }, ['structPath', 'memberName', 'newMemberName']), OK, NON_IDEMPOTENT, WRITE_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Rename Damage to BaseDamage', { structPath: S, memberName: 'Damage', newMemberName: 'BaseDamage' }, DONE)] }),
  r('set_struct_member_type', 'struct', 'Change the type of a member in a Blueprint Struct.', schema({ structPath: STRUCT_PATH, memberName: MEMBER_NAME, memberType: MEMBER_TYPE, varGuid: str('Stable member GUID.') }, ['structPath', 'memberName', 'memberType']), OK, WRITE, WRITE_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Widen an int member to a float', { structPath: S, memberName: 'Damage', memberType: 'Float' }, DONE)] }),
  r('reorder_struct_members', 'struct', 'Reorder members in a Blueprint Struct.', schema({ structPath: STRUCT_PATH, position: { type: 'string', enum: ['first', 'last', 'before', 'after'], description: 'Reorder anchor position.' }, relativeTo: str('Target member GUID/name.'), varGuid: str('Member GUID to move.') }, ['structPath', 'position']), OK, WRITE, WRITE_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Move a member to the front', { structPath: S, position: 'first', relativeTo: 'Damage' }, DONE)] }),
  r('set_struct_member_default', 'struct', 'Set the default value of a member in a Blueprint Struct.', schema({ structPath: STRUCT_PATH, memberName: MEMBER_NAME, defaultValue: str('Default value as string.'), varGuid: str('Stable member GUID.') }, ['structPath', 'memberName', 'defaultValue']), OK, WRITE, WRITE_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Default damage to 25', { structPath: S, memberName: 'Damage', defaultValue: '25.0' }, DONE)] }),
  r('set_struct_member_metadata', 'struct', 'Set metadata (tooltip, etc.) on a member in a Blueprint Struct.', schema({ structPath: STRUCT_PATH, memberName: MEMBER_NAME, tooltip: str('Member tooltip.'), metadata: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Metadata key/value pairs.' }, varGuid: str('Stable member GUID.') }, ['structPath', 'memberName']), OK, WRITE, WRITE_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Document a member with a tooltip', { structPath: S, memberName: 'Damage', tooltip: 'Base damage per hit.' }, DONE)] }),
  r('compare_structs', 'struct', 'Compare two Blueprint Structs for differences.', schema({ structPath: STRUCT_PATH, otherStructPath: str('Second struct path for comparison.') }, ['structPath', 'otherStructPath']), OK, READ, READ_POLICY, MEDIUM,
    { dispatchMode: 'tool', examples: [ex('Diff two struct revisions', { structPath: S, otherStructPath: '/Game/Structs/S_WeaponRow_V2' }, DONE)] }),
  r('search_struct_usage', 'struct', 'Search for references to a Blueprint Struct across the project.', schema({ structPath: STRUCT_PATH, searchScope: str('Optional path scope.') }, ['structPath']), OK, READ, READ_POLICY, MEDIUM,
    { dispatchMode: 'tool', examples: [ex('Find every user of a struct', { structPath: S, searchScope: '/Game' }, DONE)] }),
  r('recompile_struct', 'struct', 'Recompile a Blueprint Struct to update generated headers.', schema({ structPath: STRUCT_PATH }, ['structPath']), OK, { ...WRITE, longRunning: true }, WRITE_POLICY, HIGH,
    { dispatchMode: 'tool', examples: [ex('Recompile after member changes', { structPath: S }, DONE)] }),
  r('rename_struct', 'struct', 'Rename a Blueprint Struct asset.', schema({ structPath: STRUCT_PATH, newName: str('New struct name.') }, ['structPath', 'newName']), OK, NON_IDEMPOTENT, WRITE_POLICY, MEDIUM,
    { dispatchMode: 'tool', examples: [ex('Rename the struct asset', { structPath: S, newName: 'S_WeaponStats' }, DONE)] }),
  r('duplicate_struct', 'struct', 'Duplicate a Blueprint Struct asset.', schema({ structPath: STRUCT_PATH, destinationPath: str('Destination /Game path.'), destinationName: str('New asset name.') }, ['structPath', 'destinationPath']), OK, WRITE, WRITE_POLICY, MEDIUM,
    { dispatchMode: 'tool', examples: [ex('Fork a struct for a second revision', { structPath: S, destinationPath: '/Game/Structs', destinationName: 'S_WeaponRow_V2' }, DONE)] }),
  r('delete_struct', 'struct', 'Delete a Blueprint Struct asset.', schema({ structPath: STRUCT_PATH }, ['structPath']), OK, { ...DESTRUCTIVE, longRunning: false }, DESTRUCTIVE_POLICY, MEDIUM,
    { dispatchMode: 'tool', examples: [ex('Delete an unreferenced struct', { structPath: '/Game/Structs/S_Deprecated' }, DONE)] }),
  r('refresh_struct_dependencies', 'struct', 'Refresh dependencies of a Blueprint Struct after external changes.', schema({ structPath: STRUCT_PATH }, ['structPath']), OK, WRITE, WRITE_POLICY, MEDIUM,
    { dispatchMode: 'tool', examples: [ex('Refresh dependents after a type change', { structPath: S }, DONE)] }),
  r('list_structs', 'struct', 'List all Blueprint Struct assets in a path.', schema({ path: str('Package path to search.'), searchScope: str('Optional path scope.') }, []), OK, READ, READ_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('List the project\'s structs', { path: '/Game/Structs' }, DONE)] }),
  r('export_struct', 'struct', 'Export a Blueprint Struct to a file.', schema({ structPath: STRUCT_PATH, destinationPath: str('Export file path.') }, ['structPath', 'destinationPath']), OK, READ, READ_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Export a struct definition', { structPath: S, destinationPath: '/tmp/S_WeaponRow.json' }, DONE)] }),
  r('import_struct', 'struct', 'Import a Blueprint Struct from a file or member definitions.', schema({ structPath: str('Destination struct path.'), sourcePath: str('Source file path.'), members: arrObj('Member definitions.') }, ['structPath']), OK, WRITE, WRITE_POLICY, MEDIUM,
    { dispatchMode: 'tool', examples: [ex('Import a struct definition from disk', { structPath: S, sourcePath: '/tmp/S_WeaponRow.json' }, DONE)] }),
  r('get_instanced_struct_property', 'struct', 'Get a property from an FInstancedStruct on an asset.', schema({ assetPath: str('Asset /Game path.'), propertyName: str('Property name.') }, ['assetPath', 'propertyName']), OK, READ, READ_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Read an instanced-struct property', { assetPath: '/Game/Blueprints/BP_Weapon', propertyName: 'Stats' }, DONE)] }),
  r('set_instanced_struct_property', 'struct', 'Set a property on an FInstancedStruct on an asset.', schema({ assetPath: str('Asset /Game path.'), propertyName: str('Property name.'), structType: str('Inner UScriptStruct asset path.'), structValues: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Field-name to value map.' } }, ['assetPath', 'propertyName', 'structType']), OK, WRITE, WRITE_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Set an instanced-struct property', { assetPath: '/Game/Blueprints/BP_Weapon', propertyName: 'Stats', structType: S, structValues: { Damage: 32 } }, DONE)] })
];
