import { commonSchemas } from '../../catalog/tool-definition-utils.js';
import type { ToolDefinition } from '../shared/tool-definition.js';

export const manageDataAssetToolDefinition: ToolDefinition = {
    name: 'manage_data_asset',
    category: 'core',
    description: `Create, read, edit, and manage UDataAsset / UPrimaryDataAsset instances and UCurveFloat assets.

set_data_asset_properties supports struct arrays via FJsonObjectConverter.
set_curve_keys / get_curve_keys: read/write keys on UCurveFloat assets.
  set_curve_keys: assetPath, keys: [{time: 1, value: 100}, {time: 5, value: 500}], append: false
  get_curve_keys: assetPath -> returns keys array with time, value, interpMode, tangents

Array mutation actions (TArray<T> on a data asset, where T may be a UStruct or primitive):
  append_array_item: assetPath, propertyName, value -> push value to end of array
  insert_array_item: assetPath, propertyName, index, value -> insert at index
  remove_array_item_at: assetPath, propertyName, index -> remove element at index
  remove_array_item_where: assetPath, propertyName, matchKey, matchValue -> remove first match
  update_array_item: assetPath, propertyName, matchKey, matchValue, newValue -> replace first match
  matchKey is a dotted path on struct elements (e.g. "Key.TagName"); leave empty for primitive arrays.`,
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'create_data_asset', 'create_data_asset_blueprint',
            'get_data_asset_properties', 'set_data_asset_properties',
            'list_data_assets', 'duplicate_data_asset',
            'get_curve_keys', 'set_curve_keys',
            'append_array_item', 'insert_array_item',
            'remove_array_item_at', 'remove_array_item_where',
            'update_array_item'
          ],
          description: 'The data asset action to perform.'
        },
        assetName: { type: 'string', description: 'Name for the new data asset.' },
        folderPath: { type: 'string', description: 'Folder path (e.g., /Game/DataAssets).' },
        assetPath: { type: 'string', description: 'Path to an existing data asset.' },
        className: { type: 'string', description: 'Class name or path for data asset type (e.g., /Script/Engine.PrimaryDataAsset).' },
        parentClass: { type: 'string', description: 'Parent class for create_data_asset_blueprint (default: PrimaryDataAsset).' },
        properties: { type: 'object', description: 'Key-value pairs of UPROPERTY names and values to set.' },
        filter: { type: 'string', description: 'Class filter for list_data_assets.' },
        searchPath: { type: 'string', description: 'Content path to search in for list_data_assets.' },
        newName: { type: 'string', description: 'New asset name for duplicate_data_asset.' },
        newPath: { type: 'string', description: 'Destination folder for duplicate_data_asset.' },
        keys: { type: 'array', items: { type: 'object' }, description: 'Curve keys array for set_curve_keys. Each key: {time: number, value: number, interpMode?: "Linear"|"Constant"|"Cubic"}' },
        append: { type: 'boolean', description: 'For set_curve_keys: append to existing keys (true) or replace all (false, default).' },
        propertyName: { type: 'string', description: 'TArray UPROPERTY name on the data asset, used by array mutation actions.' },
        index: { type: 'number', description: 'Array index for insert_array_item / remove_array_item_at.' },
        value: { description: 'Element value for append_array_item / insert_array_item. Accepts JSON object for struct elements (deserialized via FJsonObjectConverter), or primitive for simple-typed arrays.' },
        newValue: { description: 'Replacement value for update_array_item. Same format as value.' },
        matchKey: { type: 'string', description: 'Dotted property path on struct elements for remove_array_item_where / update_array_item (e.g. "Key.TagName"). Omit for primitive arrays.' },
        matchValue: { type: 'string', description: 'Value to match (compared against ExportText output) for remove_array_item_where / update_array_item.' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        ...commonSchemas.outputBase,
        assetPath: { type: 'string', description: 'Path to the created/modified asset.' },
        className: { type: 'string', description: 'Class name of the data asset.' },
        properties: { type: 'object', description: 'Property values from get_data_asset_properties.' },
        assets: { type: 'array', items: { type: 'object' }, description: 'List of data assets from list_data_assets.' }
      }
    }
};
