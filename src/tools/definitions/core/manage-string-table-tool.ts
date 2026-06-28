import { commonSchemas } from '../../catalog/tool-definition-utils.js';
import type { ToolDefinition } from '../shared/tool-definition.js';

export const manageStringTableToolDefinition: ToolDefinition = {
    name: 'manage_string_table',
    category: 'core',
    description: 'Create and manage FStringTable assets for UI text and localization. Actions: create_string_table, add_entry, remove_entry, edit_entry, get_entry, list_entries, import_json, export_json, list_string_tables.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'create_string_table', 'add_entry', 'remove_entry', 'edit_entry',
            'get_entry', 'list_entries', 'import_json', 'export_json', 'list_string_tables'
          ],
          description: 'The string table action to perform.'
        },
        assetName: { type: 'string', description: 'Name for the new string table.' },
        folderPath: { type: 'string', description: 'Folder path for creation.' },
        assetPath: { type: 'string', description: 'Path to an existing string table asset.' },
        tableNamespace: { type: 'string', description: 'Namespace for the string table.' },
        key: { type: 'string', description: 'String key for add/remove/edit/get.' },
        value: { type: 'string', description: 'String value for add/edit.' },
        jsonData: { type: 'string', description: 'JSON string of key-value pairs for import_json.' },
        searchPath: { type: 'string', description: 'Content path to search for list_string_tables.' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        ...commonSchemas.outputBase,
        assetPath: { type: 'string', description: 'Path to the string table asset.' },
        entries: { type: 'object', description: 'Key-value entries from list/export.' },
        value: { type: 'string', description: 'String value for get_entry.' },
        tables: { type: 'array', items: { type: 'object' }, description: 'List of string table assets.' }
      }
    }
};
