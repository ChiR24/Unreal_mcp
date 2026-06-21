import { commonSchemas } from '../../catalog/tool-definition-utils.js';
import type { ToolDefinition } from '../shared/tool-definition.js';

export const manageProjectSettingsToolDefinition: ToolDefinition = {
    name: 'manage_project_settings',
    category: 'core',
    description: 'Manage Unreal Engine project settings, including collision profiles, channels, object types, and physical materials.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'create_collision_channel',
            'create_collision_profile',
            'configure_channel_responses',
            'configure_object_type',
            'configure_trace_channel',
            'set_actor_collision_profile',
            'create_physical_material',
            'set_physical_material_properties'
          ],
          description: 'Project settings action'
        },
        name: commonSchemas.name,
        defaultResponse: {
          type: 'string',
          enum: ['Ignore', 'Overlap', 'Block'],
          description: 'Default collision response for channels.'
        },
        channelType: {
          type: 'string',
          enum: ['Object', 'Trace'],
          description: 'Type of collision channel.'
        },
        responses: {
          type: 'object',
          description: 'Map of channel names to their response (Ignore, Overlap, Block).'
        },
        objectType: {
          type: 'string',
          description: 'Object type for collision profile.'
        },
        profileName: {
          type: 'string',
          description: 'Name of the collision profile.'
        },
        actorName: commonSchemas.actorName,
        friction: commonSchemas.numberProp,
        restitution: commonSchemas.numberProp,
        density: commonSchemas.numberProp,
        path: commonSchemas.assetPath
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        ...commonSchemas.outputBase,
        profileName: commonSchemas.stringProp,
        channelName: commonSchemas.stringProp,
        path: commonSchemas.stringProp
      }
    }
  };
