import { commonSchemas } from '../../catalog/tool-definition-utils.js';
import type { ToolDefinition } from '../shared/tool-definition.js';

export const manageDataTableToolDefinition: ToolDefinition = {
    name: 'manage_data_table',
    category: 'core',
    description: 'Create and edit UDataTable assets. Actions: create_data_table, list_rows, get_row, add_row, edit_row, remove_row, get_structure, import_json, export_json.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'create_data_table',
            'list_rows', 'get_row', 'add_row', 'edit_row', 'remove_row',
            'get_structure', 'import_json', 'export_json'
          ],
          description: 'Data table action to perform.'
        },
        tablePath: { type: 'string', description: 'Asset path of the data table (e.g. /Game/Data/WeaponStats).' },
        structType: { type: 'string', description: 'Row struct type for create_data_table (e.g. /Script/MyProject.FWeaponRow or a built-in struct path).' },
        rowName: { type: 'string', description: 'Row name for get_row, add_row, edit_row, remove_row.' },
        rowData: { type: 'object', description: 'Column values as JSON object for add_row and edit_row.' },
        jsonData: { type: 'string', description: 'JSON string for import_json (array of objects with __rowName field).' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        ...commonSchemas.outputBase,
        rows: { type: 'array', items: { type: 'object' }, description: 'Row data for list/export operations.' },
        row: { type: 'object', description: 'Single row data.' },
        columns: { type: 'array', items: { type: 'object' }, description: 'Column definitions for get_structure.' },
        rowCount: { type: 'number', description: 'Number of rows in the table.' }
      }
    }
};
