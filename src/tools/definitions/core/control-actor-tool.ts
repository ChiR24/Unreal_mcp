import { commonSchemas } from '../../catalog/tool-definition-utils.js';
import type { ToolDefinition } from '../shared/tool-definition.js';

export const controlActorToolDefinition: ToolDefinition = {
    name: 'control_actor',
    category: 'core',
    description: 'Spawn actors, set transforms, enable physics, add components, manage tags, and attach actors.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'spawn', 'spawn_actor', 'spawn_blueprint',
            'delete', 'destroy_actor', 'delete_by_tag', 'duplicate',
            'apply_force',
            'set_transform', 'teleport_actor', 'set_actor_location', 'set_actor_rotation', 'set_actor_scale', 'set_actor_transform',
            'get_transform', 'get_actor_transform',
            'set_visibility', 'set_actor_visible',
            'add_component', 'remove_component',
            'set_component_properties', 'set_component_property', 'get_component_property',
            'set_material', 'set_actor_material', 'apply_material',
            'get_components', 'get_actor_components',
            'get_actor_bounds',
            'add_tag', 'remove_tag',
            'find_by_tag', 'find_actors_by_tag',
            'find_by_name', 'find_actors_by_name',
            'find_by_class', 'find_actors_by_class',
            'list', 'set_blueprint_variables',
            'create_snapshot',
            'attach', 'attach_actor',
            'detach', 'detach_actor',
            'set_actor_collision', 'call_actor_function',
            'select_actor', 'select_actors_by_class', 'select_actors_by_tag', 'select_actors_in_volume',
            'deselect_all', 'get_selected_actors', 'group_actors', 'ungroup_actors', 'run_actor_action_utility'
          ],
          description: 'Action'
        },
        actorName: commonSchemas.actorName,
        childActor: commonSchemas.childActorName,
        parentActor: commonSchemas.parentActorName,
        classPath: commonSchemas.assetPath,
        meshPath: commonSchemas.meshPath,
        materialPath: commonSchemas.materialPath,
        materialSlot: commonSchemas.integerProp,
        materialIndex: commonSchemas.integerProp,
        allComponents: commonSchemas.booleanProp,
        blueprintPath: commonSchemas.blueprintPath,
        location: commonSchemas.location,
        rotation: commonSchemas.rotation,
        scale: commonSchemas.scale,
        force: commonSchemas.vector3,
        componentType: commonSchemas.stringProp,
        componentName: commonSchemas.componentName,
        properties: commonSchemas.objectProp,
        visible: commonSchemas.visible,
        newName: commonSchemas.newName,
        tag: commonSchemas.tagName,
        variables: commonSchemas.objectProp,
        snapshotName: commonSchemas.stringProp,
        actorClass: commonSchemas.stringProp,
        actorNames: commonSchemas.arrayOfStrings,
        arguments: commonSchemas.value,
        className: commonSchemas.stringProp,
        collisionEnabled: commonSchemas.booleanProp,
        functionName: commonSchemas.functionName,
        limit: commonSchemas.numberProp,
        filter: commonSchemas.stringProp,
        name: commonSchemas.name,
        offset: commonSchemas.location,
        propertyName: commonSchemas.propertyName,
        value: commonSchemas.value,
        // Selection/Group utilities
        volumeName: commonSchemas.actorName,
        groupName: commonSchemas.stringProp,
        utilityPath: commonSchemas.assetPath
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        ...commonSchemas.outputWithActor,
        components: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              name: commonSchemas.stringProp,
              class: commonSchemas.stringProp,
              relativeLocation: commonSchemas.location,
              relativeRotation: commonSchemas.rotation,
              relativeScale: commonSchemas.scale
            }
          }
        },
        actors: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              label: commonSchemas.stringProp,
              name: commonSchemas.stringProp,
              path: commonSchemas.stringProp,
              class: commonSchemas.stringProp
            }
          }
        },
        count: commonSchemas.numberProp,
        totalCount: commonSchemas.numberProp,
        isPieWorld: commonSchemas.booleanProp,
        worldName: commonSchemas.stringProp,
        filter: commonSchemas.stringProp,
        data: commonSchemas.nullableObjectProp
      }
    }
  };
