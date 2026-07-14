import { commonSchemas } from '../../catalog/tool-definition-utils.js';
import type { ToolDefinition } from '../shared/tool-definition.js';

export const manageDataToolDefinition: ToolDefinition = {
    name: 'manage_data',
    category: 'gameplay',
    description: 'Create and manage data assets, data tables, save games, gameplay tags, and config files.',
    inputSchema: {
        type: 'object',
        properties: {
            action: {
                type: 'string',
                enum: [
                    'create_data_asset', 'create_primary_data_asset',
                    'create_data_table', 'add_data_table_row', 'modify_data_table_row', 'delete_data_table_row',
                    'import_data_table_csv', 'export_data_table_csv',
                    'create_curve_table', 'create_curve_float', 'create_curve_linear_color',
                    'create_save_game_class', 'add_save_variable', 'save_game_to_slot', 'load_game_from_slot',
                    'delete_save_slot', 'check_save_slot_exists', 'get_save_slot_names', 'configure_async_save_load',
                    'create_gameplay_tag', 'create_tag_container', 'add_tag_to_container', 'remove_tag_from_container',
                    'check_tag_match', 'register_native_tag', 'create_tag_table',
                    'read_config_value', 'write_config_value', 'get_section', 'create_config_section',
                    'flush_config', 'reload_config', 'get_config_hierarchy'
                ],
                description: 'Data action to perform.'
            },
            name: commonSchemas.assetNameForCreation,
            path: commonSchemas.directoryPathForCreation,
            save: commonSchemas.save,
            assetPath: commonSchemas.assetPath,
            rowName: { type: 'string', description: 'Name of the data table row.' },
            rowData: { type: 'object', description: 'JSON object representing the row data.' },
            csvPath: { type: 'string', description: 'Absolute path to the CSV file.' },
            slotName: { type: 'string', description: 'Name of the save slot.' },
            userIndex: { type: 'number', description: 'User index for save game.' },
            variableName: { type: 'string', description: 'Name of the variable to add to save game.' },
            variableType: { type: 'string', description: 'Type of the variable.' },
            tagName: { type: 'string', description: 'Gameplay tag name (e.g. Action.Fire).' },
            tagComment: { type: 'string', description: 'Comment for the gameplay tag.' },
            configSection: { type: 'string', description: 'Section in the config file.' },
            configKey: { type: 'string', description: 'Key in the config file.' },
            configValue: { type: 'string', description: 'Value to write to config file.' },
            configFilename: { type: 'string', description: 'Name of the config file (e.g. DefaultGame.ini).' },
            blueprintPath: commonSchemas.blueprintPath
        },
        required: ['action']
    },
    outputSchema: {
        type: 'object',
        properties: {
            ...commonSchemas.outputBase,
            assetPath: commonSchemas.assetPath,
            rowName: { type: 'string' },
            slotName: { type: 'string' },
            tagName: { type: 'string' },
            configValue: { type: 'string' },
            exists: { type: 'boolean' },
            success: commonSchemas.booleanProp,
            error: commonSchemas.stringProp
        }
    }
};
