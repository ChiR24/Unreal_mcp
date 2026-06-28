import { commonSchemas } from '../../catalog/tool-definition-utils.js';
import type { ToolDefinition } from '../shared/tool-definition.js';

export const manageBlueprintInterfaceToolDefinition: ToolDefinition = {
    name: 'manage_blueprint_interface',
    category: 'core',
    description: 'Create and manage Blueprint Interfaces. Actions: create_blueprint_interface, add_function, remove_function, list_functions, implement_interface, remove_interface, list_interfaces.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'create_blueprint_interface', 'add_function', 'remove_function',
            'list_functions', 'implement_interface', 'remove_interface', 'list_interfaces'
          ],
          description: 'The blueprint interface action to perform.'
        },
        assetName: { type: 'string', description: 'Name for the new interface asset.' },
        folderPath: { type: 'string', description: 'Folder path for creation.' },
        interfacePath: { type: 'string', description: 'Path to the interface asset.' },
        blueprintPath: { type: 'string', description: 'Path to the Blueprint to modify.' },
        functionName: { type: 'string', description: 'Function name for add/remove.' },
        inputs: { type: 'array', items: { type: 'object' }, description: 'Input parameters for add_function: [{name, type}].' },
        outputs: { type: 'array', items: { type: 'object' }, description: 'Output parameters for add_function: [{name, type}].' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        ...commonSchemas.outputBase,
        assetPath: { type: 'string', description: 'Path to the interface asset.' },
        functions: { type: 'array', items: { type: 'object' }, description: 'Functions in the interface.' },
        interfaces: { type: 'array', items: { type: 'string' }, description: 'Interfaces on a Blueprint.' }
      }
    }
};
