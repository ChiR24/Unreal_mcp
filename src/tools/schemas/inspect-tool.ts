import { commonSchemas } from '../catalog/tool-definition-utils.js';
import type { ToolDefinition } from './core-tool-definition.js';

export const inspectToolDefinition: ToolDefinition = {
  name: 'inspect',
  category: 'core',
  description: 'Inspect any UObject: read/write properties, list components, export snapshots, and query class info. Actions: inspect_cdo (Blueprint CDO properties + all components without spawning an actor; use blueprintPath, optional detailed/componentName/propertyNames), inspect_class (class metadata), inspect_object (world actor), get_property/set_property, get_components, list_objects, find_by_class, find_by_tag, runtime_report.',
  inputSchema: {
    type: 'object',
    properties: {
      action: {
        type: 'string',
        enum: [
          'inspect_object', 'get_actor_details', 'get_blueprint_details', 'get_mesh_details',
          'get_texture_details', 'get_material_details', 'get_level_details', 'get_component_details',
          'set_property', 'get_property',
          'get_components', 'get_component_property', 'set_component_property',
          'inspect_class', 'inspect_cdo', 'runtime_report', 'pie_report', 'list_objects',
          'get_metadata', 'add_tag', 'find_by_tag',
          'create_snapshot', 'restore_snapshot', 'export', 'delete_object', 'find_by_class', 'get_bounding_box',
          'get_project_settings', 'get_world_settings', 'get_viewport_info', 'get_content_browser_state', 'get_selected_actors',
          'get_scene_stats', 'get_performance_stats', 'get_memory_stats', 'get_editor_settings'
        ],
        description: 'Action'
      },
      objectPath: commonSchemas.assetPath,
      propertyName: commonSchemas.propertyName,
      propertyPath: commonSchemas.stringProp,
      value: commonSchemas.value,
      actorName: commonSchemas.actorName,
      name: commonSchemas.name,
      componentName: commonSchemas.componentName,
      className: commonSchemas.stringProp,
      classPath: commonSchemas.assetPath,
      tag: commonSchemas.tagName,
      filter: commonSchemas.stringProp,
      snapshotName: commonSchemas.stringProp,
      blueprintPath: commonSchemas.blueprintPath,
      detailed: commonSchemas.booleanProp,
      propertyNames: commonSchemas.arrayOfStrings,
      componentNames: commonSchemas.arrayOfStrings
    },
    required: ['action']
  },
  outputSchema: {
    type: 'object',
    properties: {
      ...commonSchemas.outputBase,
      value: commonSchemas.value,
      blueprintPath: commonSchemas.blueprintPath,
      className: commonSchemas.stringProp,
      classPath: commonSchemas.stringProp,
      parentClass: commonSchemas.stringProp,
      cdoProperties: commonSchemas.objectProp,
      components: commonSchemas.arrayOfObjects,
      componentCount: commonSchemas.numberProp,
      componentName: commonSchemas.componentName,
      templateObjectName: commonSchemas.stringProp,
      componentClass: commonSchemas.stringProp,
      count: commonSchemas.numberProp,
      objects: commonSchemas.arrayOfObjects,
      actors: commonSchemas.arrayOfObjects,
      currentPath: commonSchemas.stringProp,
      selectedFolders: commonSchemas.arrayOfStrings,
      selectedFolderCount: commonSchemas.numberProp,
      selectedAssets: commonSchemas.arrayOfObjects,
      selectedAssetCount: commonSchemas.numberProp
    }
  }
};
