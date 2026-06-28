import { commonSchemas } from '../../catalog/tool-definition-utils.js';
import type { ToolDefinition } from '../shared/tool-definition.js';

export const manageLayersToolDefinition: ToolDefinition = {
    name: 'manage_layers',
    category: 'core',
    description: 'Manage editor layers for actor organization. Actions: create_layer, delete_layer, rename_layer, list_layers, add_actor_to_layer, remove_actor_from_layer, get_actor_layers, set_layer_visibility, get_layer_actors.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'create_layer', 'delete_layer', 'rename_layer', 'list_layers',
            'add_actor_to_layer', 'remove_actor_from_layer', 'get_actor_layers',
            'set_layer_visibility', 'get_layer_actors'
          ],
          description: 'The layer action to perform.'
        },
        layerName: { type: 'string', description: 'Name of the layer.' },
        newName: { type: 'string', description: 'New name for rename_layer.' },
        actorName: { type: 'string', description: 'Actor name/label for add/remove/get operations.' },
        visible: { type: 'boolean', description: 'Visibility state for set_layer_visibility.' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        ...commonSchemas.outputBase,
        layers: { type: 'array', items: { type: 'string' }, description: 'List of layer names.' },
        actors: { type: 'array', items: { type: 'object' }, description: 'Actors in a layer.' }
      }
    }
};
