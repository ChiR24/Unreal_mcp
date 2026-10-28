// Data registry records: registries listed, read (sources, schema, items and their values) and made over data tables.
// The DataRegistry plugin is reached by reflection (native Domains/AssetWorkflow/DataRegistries), so nothing links it
// and the plugin is never turned on for a project; one that leaves it off answers DATA_REGISTRY_DISABLED.

import type { RecordSpec } from './builder.js';
import { bool, ex, LOW, MEDIUM, NON_IDEMPOTENT, num, READ, READ_POLICY, r, str, WRITE_POLICY } from './builder.js';
import { schema } from '../shared/record-presets.js';

const PLUGINS = ['DataRegistry'];
const SCANNED = bool('Whether the game sets it up: only registries under the DataRegistry settings\' DirectoriesToScan are.');
const STRINGS = (description: string) => ({ type: 'array', items: { type: 'string' }, description });

export const DATA_REGISTRY_RECORDS: readonly RecordSpec[] = [
  r('list_data_registries', 'datatable', 'List the project\'s data registries (DataRegistry assets, found through the asset registry without loading them), each with its registryType (the name the game looks its items up by), itemStruct and scanned: the game sets up only registries under the DataRegistry settings\' directoriesToScan (in the reply, with initializeAllLoaded). A project that leaves the DataRegistry plugin off answers DATA_REGISTRY_DISABLED.',
    schema({}),
    schema({
      success: bool('Operation succeeded.'),
      dataRegistries: { type: 'array', description: 'By asset path.', items: { type: 'object', additionalProperties: false, properties: {
        assetPath: str('The registry asset.'),
        registryType: str('The name the game looks its items up by.'),
        itemStruct: str('The struct of its items.'),
        scanned: SCANNED,
      } } },
      directoriesToScan: STRINGS('The DataRegistry settings\' folders whose registries the game sets up.'),
      initializeAllLoaded: bool('The settings set up every registry that loads, wherever it is.'),
    }, ['success']),
    READ, READ_POLICY, LOW,
    { requiredPlugins: PLUGINS,
      topics: ['data registries', 'list data registries', 'registry types', 'data registry assets'],
      whenToUse: ['Which data registries a project has, and whether the game will find them, must be known before reading one.'],
      whenNotToUse: ['Data tables are listed (use asset.list with class DataTable).'],
      examples: [ex('List the registries', {}, { success: true, dataRegistries: [{ assetPath: '/Game/Data/DR_Weapons', registryType: 'Weapons', itemStruct: '/Game/Data/S_Weapon.S_Weapon', scanned: true }], directoriesToScan: ['/Game/Data'], initializeAllLoaded: false })] }
  ),

  r('get_data_registry', 'datatable', 'Read one data registry, by asset path or by its registryType: its item struct and schema (each field\'s name and type), id format tag, sources (each source\'s class and the table it reads), whether the game sets it up (scanned) and whether the registry has set up its runtime sources (initialized: then meta sources show the tables they found), every item name in lookup order (the first source holding a name serves it; up to 500 named, itemCount counts all), and the values of up to 50 items asked for by name (items: a curve table\'s row is named, not read; names no source holds are in itemsNotFound). DATA_REGISTRY_NOT_FOUND names nothing found; DATA_REGISTRY_DISABLED when the project leaves the DataRegistry plugin off.',
    schema({
      registry: str('The registry: its asset path (/Game/Data/DR_Weapons) or its registryType (Weapons).'),
      items: { type: 'array', maxItems: 50, items: { type: 'string' }, description: 'Item names whose values to read (up to 50).' },
    }, ['registry']),
    schema({
      success: bool('Operation succeeded.'),
      assetPath: str('The registry asset.'),
      registryType: str('The name the game looks its items up by.'),
      itemStruct: str('The struct of its items.'),
      idFormatTag: str('Its id format\'s base gameplay tag, empty when it has none.'),
      scanned: SCANNED,
      initialized: bool('The registry has set up its runtime sources.'),
      schema: { type: 'array', description: 'The item struct\'s fields.', items: { type: 'object', additionalProperties: false, properties: {
        name: str('The field.'),
        type: str('Its C++ type.'),
      } } },
      sources: { type: 'array', description: 'The sources as authored, in order.', items: { type: 'object', additionalProperties: false, properties: {
        class: str('The source class (DataRegistrySource_DataTable, DataRegistrySource_CurveTable, a meta source...).'),
        table: str('The table it reads; empty for a source of another kind.'),
      } } },
      itemNames: STRINGS('Item names in lookup order, up to 500.'),
      itemCount: num('Items in all.'),
      items: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Each asked item\'s field values by name.' },
      itemsNotFound: STRINGS('Asked names no source holds.'),
    }, ['success']),
    READ, READ_POLICY, MEDIUM,
    { requiredPlugins: PLUGINS,
      topics: ['read a data registry', 'data registry items', 'data registry schema', 'registry item values'],
      whenToUse: ['A registry\'s items, their values or its sources must be read (what the game gets for an item id).'],
      whenNotToUse: ['One data table\'s rows are read (use datatable.inspect_data_table).'],
      examples: [ex('Read two weapons', { registry: 'Weapons', items: ['Sword', 'Bow'] }, { success: true, registryType: 'Weapons', itemCount: 12, items: { Sword: { Damage: 40 }, Bow: { Damage: 25 } } })] }
  ),

  r('create_data_registry', 'datatable', 'Make a data registry over data tables: the asset at assetPath with registryType (the name the game looks items up by, one registry each) and one DataTable source per table, in the order given (the first holding an item serves it), its item struct the tables\' shared row struct; saved. The game sets up only registries under the DataRegistry settings\' DirectoriesToScan, so scanned says whether this one is (the project settings stay as they are). Nothing is made on a refusal: INVALID_ARGUMENT, ASSET_EXISTS, DATA_REGISTRY_EXISTS (the type is another registry\'s), DATA_TABLE_NOT_FOUND, ROW_STRUCT_MISMATCH; DATA_REGISTRY_DISABLED when the project leaves the DataRegistry plugin off.',
    schema({
      assetPath: str('The new registry asset (/Game/Data/DR_Weapons).'),
      registryType: str('The name the game looks its items up by; no other registry may have it.'),
      dataTables: { type: 'array', minItems: 1, maxItems: 16, items: { type: 'string' }, description: 'Data tables to read items from, in lookup order (1-16, one row struct).' },
    }, ['assetPath', 'registryType', 'dataTables']),
    schema({
      success: bool('Operation succeeded.'),
      assetPath: str('The registry made.'),
      registryType: str('Its type.'),
      itemStruct: str('Its item struct, the tables\' row struct.'),
      dataTables: STRINGS('Its tables, in lookup order.'),
      saved: bool('Saved to disk.'),
      scanned: SCANNED,
    }, ['success']),
    NON_IDEMPOTENT, WRITE_POLICY, MEDIUM,
    { requiredPlugins: PLUGINS,
      topics: ['make a data registry', 'new data registry', 'create data registry'],
      whenToUse: ['Game code looks items up through a data registry, and one must be made over existing data tables.'],
      whenNotToUse: ['A data table itself is made (use datatable.edit_data_table with edit create).'],
      examples: [ex('A weapons registry over two tables', { assetPath: '/Game/Data/DR_Weapons', registryType: 'Weapons', dataTables: ['/Game/Data/DT_Melee', '/Game/Data/DT_Ranged'] },
        { success: true, assetPath: '/Game/Data/DR_Weapons', registryType: 'Weapons', itemStruct: '/Game/Data/S_Weapon.S_Weapon', saved: true, scanned: true })] }
  ),
];
