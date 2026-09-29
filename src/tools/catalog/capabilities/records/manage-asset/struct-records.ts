// Struct authoring records: Blueprint Struct CRUD, member management,
// comparison, usage search, recompilation, import/export, and
// FInstancedStruct property access (issue #510, #struct-ecosystem).

import type { RecordSpec } from './builder.js';
import { arrObj, bool, DESTRUCTIVE, DESTRUCTIVE_POLICY, ex, HIGH, LOW, MEDIUM, NON_IDEMPOTENT, READ, READ_POLICY, r, str, WRITE, WRITE_POLICY } from './builder.js';
import { schema } from '../shared/record-presets.js';

const STRUCT_PATH = str('Asset path of the Blueprint Struct (e.g. /Game/Structs/S_MyStruct).');
const MEMBER_NAME = str('Member (variable) name.');
const MEMBER_TYPE = str('Unreal property type: Bool, Int, Float, String, Name, Text, Vector, Rotator, Transform, Object, SoftObject, Class, SoftClass, Enum:<Name>, Struct:<Path>, or with container prefix Array:..., Set:..., Map:<K>,<V>:');
// Every struct edit reads `save` and defaults it to true: members added in
// memory only reverted on the next editor restart.
const SAVE = bool('Persist the struct to disk. Defaults to true; pass false to keep the change in memory only.');
const MEMBERS = arrObj('Member definitions, each { name, type } (or memberName, memberType) with optional defaultValue, tooltip and metadata.');
const OK = schema({ success: bool('Operation succeeded.'), details: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Operation details.' } }, ['success']);

const S = '/Game/Structs/S_WeaponRow';
const DONE = { success: true };

export const STRUCT_RECORDS: readonly RecordSpec[] = [
  r('create_struct', 'struct', 'Create a new Blueprint Struct asset.', schema({ name: str('Struct name.'), path: str('Package path (default /Game/Structs).'), structPath: str('Full asset path of the new struct, e.g. /Game/Structs/S_WeaponRow; alternative to name and path.'), members: MEMBERS, save: SAVE }, [], ['name', 'structPath']), OK, WRITE, WRITE_POLICY, MEDIUM,
    { examples: [ex('Create a weapon row struct', { name: 'S_WeaponRow', path: '/Game/Structs', members: [{ memberName: 'Damage', memberType: 'Float' }] }, DONE)],
      whenToUse: ['A new Blueprint struct is needed; give its members up front, otherwise it keeps one placeholder member.'],
      whenNotToUse: ['A DataTable is wanted as well as its row struct (use datatable.edit_data_table to create the table and bind the struct).'] }),
  r('get_struct', 'struct', 'Read a Blueprint or native struct: name, status, validity and every member as { guid, name, type, default, tooltip, containerType, metaData }.', schema({ structPath: STRUCT_PATH }, ['structPath']), OK, READ, READ_POLICY, LOW,
    { examples: [ex('Read a struct\'s metadata', { structPath: S }, DONE)],
      whenToUse: ['A struct\'s member names, types, defaults and tooltips must be known before rows or edits are written; native /Script structs such as Vector can be read too.'],
      whenNotToUse: ['A struct must be changed rather than read (use struct.edit_struct).'] }),
  r('read_struct', 'struct', 'Read a struct definition: the same reply as get_struct, with the name, status, validity and every member. It returns no row or instance values.', schema({ structPath: STRUCT_PATH }, ['structPath']), OK, READ, READ_POLICY, LOW,
    { examples: [ex('Read a struct definition', { structPath: S }, DONE)],
      whenToUse: ['A struct\'s member names, types, defaults and tooltips must be known before rows or edits are written; native /Script structs such as Vector can be read too.'],
      whenNotToUse: ['A struct must be changed rather than read (use struct.edit_struct).'] }),
  r('list_struct_members', 'struct', 'List a struct\'s members with guid, type, default, tooltip and metadata. The reply is the same as get_struct, so it also carries the struct name, status and validity.', schema({ structPath: STRUCT_PATH }, ['structPath']), OK, READ, READ_POLICY, LOW,
    { examples: [ex('List a struct\'s members', { structPath: S }, DONE)],
      whenToUse: ['A struct\'s member names, types, defaults and tooltips must be known before rows or edits are written; native /Script structs such as Vector can be read too.'],
      whenNotToUse: ['A struct must be changed rather than read (use struct.edit_struct).'] }),
  r('add_struct_member', 'struct', 'Add a new member to a Blueprint Struct.', schema({ structPath: STRUCT_PATH, memberName: MEMBER_NAME, memberType: MEMBER_TYPE, defaultValue: str('Default value as string.'), tooltip: str('Member tooltip.'), metadata: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Metadata key/value pairs.' }, members: arrObj('Several members at once, each { name, type } (or memberName, memberType) with optional defaultValue, tooltip and metadata; replaces the single memberName form.'), save: SAVE }, ['structPath'], ['memberName', 'members']), OK, WRITE, WRITE_POLICY, LOW,
    { examples: [ex('Add a float damage member', { structPath: S, memberName: 'Damage', memberType: 'Float', defaultValue: '25.0', tooltip: 'Base damage per hit.' }, DONE)],
      whenToUse: ['One or several fields, each with a type, optional default and tooltip, must be added to an existing struct.'],
      whenNotToUse: ['A variable is needed on a Blueprint, not a field on a struct (use blueprint.edit_variable).'] }),
  r('remove_struct_member', 'struct', 'Remove a member from a Blueprint Struct.', schema({ structPath: STRUCT_PATH, memberName: MEMBER_NAME, varGuid: str('Stable member GUID.'), save: SAVE }, ['structPath', 'memberName']), OK, { ...DESTRUCTIVE, longRunning: false }, DESTRUCTIVE_POLICY, LOW,
    { examples: [ex('Remove an obsolete member', { structPath: S, memberName: 'LegacyFlag' }, DONE)],
      whenToUse: ['One field must be removed from a struct; referencing Blueprints and DataTables are refreshed afterwards.'],
      whenNotToUse: ['A field only needs a new name or type (use struct.edit_struct).'] }),
  r('rename_struct_member', 'struct', 'Rename a member in a Blueprint Struct.', schema({ structPath: STRUCT_PATH, memberName: MEMBER_NAME, newMemberName: str('New member name.'), varGuid: str('Stable member GUID.'), save: SAVE }, ['structPath', 'memberName', 'newMemberName']), OK, NON_IDEMPOTENT, WRITE_POLICY, LOW,
    { examples: [ex('Rename Damage to BaseDamage', { structPath: S, memberName: 'Damage', newMemberName: 'BaseDamage' }, DONE)],
      whenToUse: ['A field must be renamed while keeping its type and default; it is found by memberName or varGuid.'],
      whenNotToUse: ['A field must be removed instead of renamed (use struct.delete_struct with deleteScope=member).'] }),
  r('set_struct_member_type', 'struct', 'Change the type of a member in a Blueprint Struct.', schema({ structPath: STRUCT_PATH, memberName: MEMBER_NAME, memberType: MEMBER_TYPE, varGuid: str('Stable member GUID.'), save: SAVE }, ['structPath', 'memberName', 'memberType']), OK, WRITE, WRITE_POLICY, LOW,
    { examples: [ex('Widen an int member to a float', { structPath: S, memberName: 'Damage', memberType: 'Float' }, DONE)],
      whenToUse: ['A field needs a different type, for example Int to Float or a container; self-referencing and cyclic types are refused.'],
      whenNotToUse: ['The assets that use the struct must be found before a type change (use struct.get_struct with info=usage).'] }),
  r('reorder_struct_members', 'struct', 'Reorder members in a Blueprint Struct.', schema({ structPath: STRUCT_PATH, position: { type: 'string', enum: ['first', 'last', 'before', 'after'], description: 'Where the moved member goes: first or last of the struct, or before or after the member named in relativeTo.' }, relativeTo: str('Name or GUID of the member to place the moved member next to. Required for before and after; ignored for first and last.'), memberName: str('Name of the member to move. Pass this or varGuid.'), varGuid: str('Stable GUID of the member to move. Pass this or memberName.'), save: SAVE }, ['structPath', 'position'], ['memberName', 'varGuid']), OK, WRITE, WRITE_POLICY, LOW,
    { examples: [ex('Move a member to the front', { structPath: S, memberName: 'Damage', position: 'first' }, DONE), ex('Move a member after another', { structPath: S, memberName: 'Damage', position: 'after', relativeTo: 'Range' }, DONE)],
      whenToUse: ['One field, named by memberName or varGuid, must move to first, last, or before or after another field.'],
      whenNotToUse: ['The current member order must be read before moving anything (use struct.get_struct).'] }),
  r('set_struct_member_default', 'struct', 'Set the default value of a member in a Blueprint Struct.', schema({ structPath: STRUCT_PATH, memberName: MEMBER_NAME, defaultValue: str('Default value as string.'), varGuid: str('Stable member GUID.'), save: SAVE }, ['structPath', 'memberName', 'defaultValue']), OK, WRITE, WRITE_POLICY, LOW,
    { examples: [ex('Default damage to 25', { structPath: S, memberName: 'Damage', defaultValue: '25.0' }, DONE)],
      whenToUse: ['A field needs a new default value; a value that cannot convert to the field type is rejected.'],
      whenNotToUse: ['Values stored in a DataTable row must change, not the struct default (use datatable.edit_data_table).'] }),
  r('set_struct_member_metadata', 'struct', 'Set metadata (tooltip, etc.) on a member in a Blueprint Struct.', schema({ structPath: STRUCT_PATH, memberName: MEMBER_NAME, tooltip: str('Member tooltip.'), metadata: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Metadata key/value pairs.' }, varGuid: str('Stable member GUID.'), save: SAVE }, ['structPath', 'memberName']), OK, WRITE, WRITE_POLICY, LOW,
    { examples: [ex('Document a member with a tooltip', { structPath: S, memberName: 'Damage', tooltip: 'Base damage per hit.' }, DONE)],
      whenToUse: ['A field needs a tooltip or metadata key and value pairs.'],
      whenNotToUse: ['Metadata for an enum entry is wanted (use enum.edit_enum).'] }),
  r('compare_structs', 'struct', 'Compare two Blueprint Structs for differences.', schema({ structPath: STRUCT_PATH, otherStructPath: str('Second struct path for comparison.') }, ['structPath', 'otherStructPath']), OK, READ, READ_POLICY, MEDIUM,
    { examples: [ex('Diff two struct revisions', { structPath: S, otherStructPath: '/Game/Structs/S_WeaponRow_V2' }, DONE)],
      whenToUse: ['Two structs must be diffed for added, removed, renamed or retyped members and changed defaults, tooltips or order.'],
      whenNotToUse: ['A struct must be brought in line with another; the comparison only reports differences (use struct.edit_struct).'] }),
  r('search_struct_usage', 'struct', 'Search for references to a Blueprint Struct across the project.', schema({ structPath: STRUCT_PATH, searchScope: str('Optional path scope.') }, ['structPath']), OK, READ, READ_POLICY, MEDIUM,
    { examples: [ex('Find every user of a struct', { structPath: S, searchScope: '/Game' }, DONE)],
      whenToUse: ['Every asset that references a struct, such as Blueprints, other structs and DataTables, must be found before changing or deleting it.'],
      whenNotToUse: ['References to an asset that is not a struct are needed (use asset.inspect_asset).'] }),
  r('recompile_struct', 'struct', 'Recompile a Blueprint Struct to update generated headers.', schema({ structPath: STRUCT_PATH, save: SAVE }, ['structPath']), OK, { ...WRITE, longRunning: true }, WRITE_POLICY, HIGH,
    { examples: [ex('Recompile after member changes', { structPath: S }, DONE)],
      whenToUse: ['Dependent Blueprints must be recompiled after a struct change and their compile errors listed; the changed assets are saved unless save is false.'],
      whenNotToUse: ['One Blueprint must be compiled rather than a struct (use blueprint.compile).'] }),
  r('rename_struct', 'struct', 'Rename a Blueprint Struct asset.', schema({ structPath: STRUCT_PATH, newName: str('New struct name; the struct stays in its folder unless destinationFolder is given.'), destinationFolder: str('Folder to move the renamed struct into, e.g. /Game/Data.'), newStructPath: str('Full new object path, e.g. /Game/Data/S_WeaponStats.S_WeaponStats; alternative to newName.') }, ['structPath'], ['newName', 'newStructPath']), OK, NON_IDEMPOTENT, WRITE_POLICY, MEDIUM,
    { examples: [ex('Rename the struct asset', { structPath: S, newName: 'S_WeaponStats' }, DONE)],
      whenToUse: ['The struct asset needs a new name or folder; a redirector is left at the old path when it is referenced, and dependent Blueprints recompile.'],
      whenNotToUse: ['A non-struct asset must be renamed or moved (use asset.rename or asset.move).'] }),
  r('duplicate_struct', 'struct', 'Duplicate a Blueprint Struct asset.', schema({ structPath: STRUCT_PATH, destinationPath: str('Folder for the copy, e.g. /Game/Structs, when destinationName is given, the path ends in "/" or the folder exists; the copy is destinationPath/destinationName, or keeps the source name without one. Otherwise the full new asset path, e.g. /Game/Structs/S_WeaponRow_V2. A copy onto an existing struct is refused with ALREADY_EXISTS.'), destinationName: str('Name of the new asset. With destinationPath it is placed in that folder; on its own the copy stays beside the source.') }, ['structPath'], ['destinationPath', 'destinationName']), OK, WRITE, WRITE_POLICY, MEDIUM,
    { examples: [ex('Fork a struct for a second revision', { structPath: S, destinationPath: '/Game/Structs', destinationName: 'S_WeaponRow_V2' }, DONE)],
      whenToUse: ['A struct must be forked into a new asset with the same members, for example to try a second revision.'],
      whenNotToUse: ['An asset that is not a struct must be copied (use asset.duplicate).'] }),
  r('delete_struct', 'struct', 'Delete a Blueprint Struct asset.', schema({ structPath: STRUCT_PATH, force: bool('Delete even when other assets still reference the struct (their references break). Default false, which refuses and lists the referencers.') }, ['structPath']), OK, { ...DESTRUCTIVE, longRunning: false }, DESTRUCTIVE_POLICY, MEDIUM,
    { examples: [ex('Delete an unreferenced struct', { structPath: '/Game/Structs/S_Deprecated' }, DONE)],
      whenToUse: ['An unused struct asset must be deleted; a struct still referenced is refused, with its referencers listed, unless force is set.'],
      whenNotToUse: ['The struct should be renamed or moved instead of removed (use struct.edit_struct).', 'The assets that use the struct must be found first (use struct.get_struct with info=usage).'] }),
  r('refresh_struct_dependencies', 'struct', 'Refresh dependencies of a Blueprint Struct after external changes.', schema({ structPath: STRUCT_PATH }, ['structPath']), OK, WRITE, WRITE_POLICY, MEDIUM,
    { examples: [ex('Refresh dependents after a type change', { structPath: S }, DONE)],
      whenToUse: ['A struct was edited outside these tools and its Blueprints and DataTables must be rebuilt against the new layout; the affected assets are listed.'],
      whenNotToUse: ['Only the struct status and validity must be read (use struct.get_struct).'] }),
  r('list_structs', 'struct', 'List all Blueprint Struct assets in a path.', schema({ path: str('Package path to search (default /Game/Structs), recursive.') }, []), OK, READ, READ_POLICY, LOW,
    { examples: [ex('List the project\'s structs', { path: '/Game/Structs' }, DONE)],
      whenToUse: ['The Blueprint structs under a folder must be found; the search is recursive and defaults to /Game/Structs.'],
      whenNotToUse: ['Assets of another type must be found (use asset.query_asset).'] }),
  r('export_struct', 'struct', 'Return a struct definition as JSON: the same reply as get_struct, whose members array (name, type, default, tooltip, metaData) import_struct accepts as members or as a sourcePath file. Nothing is written to disk.', schema({ structPath: STRUCT_PATH }, ['structPath']), OK, READ, READ_POLICY, LOW,
    { examples: [ex('Export a struct definition', { structPath: S }, DONE)],
      whenToUse: ['A struct definition must be returned as JSON members that import_struct accepts, to reuse or restore it.'],
      whenNotToUse: ['A struct must be copied as a new asset (use struct.edit_struct with edit=duplicate_struct).'] }),
  r('import_struct', 'struct', 'Replace the members of an existing Blueprint Struct, or create a new one, from member definitions or a JSON file.', schema({ structPath: str('Existing struct whose members are replaced.'), name: str('Name of a new struct to create instead (with path).'), path: str('Package path for a new struct (default /Game/Structs).'), sourcePath: str('Project-relative JSON file holding the members: a bare array, or an object with a members array as export_struct returns. Used when members is omitted.'), members: MEMBERS, save: SAVE }, [], ['structPath', 'name']), OK, WRITE, WRITE_POLICY, MEDIUM,
    { examples: [ex('Import a struct definition from disk', { structPath: S, sourcePath: 'Saved/Structs/S_WeaponRow.json' }, DONE)],
      whenToUse: ['A struct must be rebuilt from a member list or a JSON file; members are replaced and a failed apply is rolled back.'],
      whenNotToUse: ['The definition must be exported as JSON, not imported (use struct.get_struct with info=export).'] }),
  r('get_instanced_struct_property', 'struct', 'Get a property from an FInstancedStruct on an asset.', schema({ assetPath: str('Asset /Game path.'), propertyName: str('Property name.') }, ['assetPath', 'propertyName']), OK, READ, READ_POLICY, LOW,
    { examples: [ex('Read an instanced-struct property', { assetPath: '/Game/Blueprints/BP_Weapon', propertyName: 'Stats' }, DONE)],
      whenToUse: ['The struct type and field values held in an FInstancedStruct property of an asset must be read.'],
      whenNotToUse: ['A plain property on an actor or asset must be read (use inspect.get_property).'] }),
  r('set_instanced_struct_property', 'struct', 'Set a property on an FInstancedStruct on an asset.', schema({ assetPath: str('Asset /Game path.'), propertyName: str('Property name.'), structType: str('Inner UScriptStruct asset path.'), structValues: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Field-name to value map.' }, save: bool('Save the asset afterwards. Defaults to true.') }, ['assetPath', 'propertyName', 'structType']), OK, WRITE, WRITE_POLICY, LOW,
    { examples: [ex('Set an instanced-struct property', { assetPath: '/Game/Blueprints/BP_Weapon', propertyName: 'Stats', structType: S, structValues: { Damage: 32 } }, DONE)],
      whenToUse: ['An FInstancedStruct property on an asset must be set to a chosen struct type with given field values.'],
      whenNotToUse: ['A plain property on an actor or asset must be written (use inspect.set_property).'] })
];
