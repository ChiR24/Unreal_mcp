import { commonSchemas } from '../../catalog/tool-definition-utils.js';
import type { ToolDefinition } from '../shared/tool-definition.js';

export const managePhysicsMaterialToolDefinition: ToolDefinition = {
    name: 'manage_physics_material',
    category: 'gameplay',
    description: 'Create and manage UPhysicalMaterial assets. Actions: create_physics_material, set_physics_material_properties, get_physics_material_properties, list_physics_materials, assign_physics_material.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'create_physics_material', 'set_physics_material_properties',
            'get_physics_material_properties', 'list_physics_materials',
            'assign_physics_material'
          ],
          description: 'The physics material action to perform.'
        },
        assetName: { type: 'string', description: 'Name for the new physics material.' },
        folderPath: { type: 'string', description: 'Folder path for creation.' },
        assetPath: { type: 'string', description: 'Path to an existing physics material.' },
        friction: { type: 'number', description: 'Friction coefficient (0-1).' },
        staticFriction: { type: 'number', description: 'Static friction override.' },
        restitution: { type: 'number', description: 'Bounciness (0-1).' },
        density: { type: 'number', description: 'Material density.' },
        surfaceType: { type: 'string', description: 'Physical surface type enum name.' },
        actorName: { type: 'string', description: 'Actor to assign material to.' },
        componentName: { type: 'string', description: 'Component to assign material to.' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        ...commonSchemas.outputBase,
        assetPath: { type: 'string', description: 'Path to the physics material.' },
        properties: { type: 'object', description: 'Physics material properties.' },
        materials: { type: 'array', items: { type: 'object' }, description: 'List of physics materials.' }
      }
    }
};
