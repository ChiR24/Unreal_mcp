/**
 * Core Tool Schemas - Essential MCP tools for pipeline, asset, actor, editor, level control
 * 
 * Tools included:
 * - manage_pipeline: Build automation and pipeline control
 * - manage_tools: Dynamic MCP tool management
 * - manage_asset: Asset creation, import, manipulation
 * - control_actor: Actor spawn, transform, physics, components
 * - control_editor: PIE, camera, console commands, screenshots
 * - manage_level: Level load/save, streaming, World Partition
 * - system_control: Profiling, CVars, UBT, tests
 * - inspect: Object introspection
 */

import { commonSchemas } from '../tool-definition-utils.js';

/** MCP Tool Definition type for explicit annotation to avoid TS7056 */
export interface ToolDefinition {
  category?: 'core' | 'world' | 'authoring' | 'gameplay' | 'utility';
  name: string;
  description: string;
  inputSchema: Record<string, unknown>;
  outputSchema?: Record<string, unknown>;
  [key: string]: unknown;
}

export const coreToolDefinitions: ToolDefinition[] = [
  {
    name: 'manage_pipeline',
    description: 'Build automation and pipeline control. Actions: run_ubt (compile targets), list_categories (show tool categories), get_status (bridge status). Routes to system_control internally.',
    category: 'core',
    inputSchema: {
      type: 'object',
      properties: {
        action: { type: 'string', enum: ['run_ubt', 'list_categories', 'get_status'], description: 'run_ubt: compile with UnrealBuildTool. list_categories: show available tool categories. get_status: get bridge status.' },
        target: { type: 'string', description: 'Build target name (e.g., MyProjectEditor)' },
        platform: { type: 'string', description: 'Target platform (Win64, Linux, Mac)' },
        configuration: { type: 'string', description: 'Build configuration (Development, Shipping, Debug)' },
        arguments: { type: 'string', description: 'Additional UBT arguments' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        ...commonSchemas.outputBase,
        output: { type: 'string', description: 'Build output' },
        command: { type: 'string', description: 'Executed command' }
      }
    }
  },
  {
    name: 'manage_tools',
    description: 'Dynamic MCP tool management. Enable/disable tools and categories at runtime. Actions: list_tools, list_categories, enable_tools, disable_tools, enable_category, disable_category, get_status, reset.',
    category: 'core',
    inputSchema: {
      type: 'object',
      properties: {
        action: { 
          type: 'string', 
          enum: ['list_tools', 'list_categories', 'enable_tools', 'disable_tools', 'enable_category', 'disable_category', 'get_status', 'reset'],
          description: 'list_tools: show all tools with status. list_categories: show categories. enable/disable_tools: toggle specific tools. enable/disable_category: toggle category. get_status: current state. reset: restore defaults.'
        },
        tools: { type: 'array', items: commonSchemas.stringProp, description: 'Tool names to enable/disable' },
        category: { type: 'string', description: 'Category name to enable/disable (core, world, authoring, gameplay, utility)' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        ...commonSchemas.outputBase,
        tools: { type: 'array', items: { type: 'object', properties: { name: { type: 'string' }, enabled: { type: 'boolean' }, category: { type: 'string' } } } },
        categories: { type: 'array', items: { type: 'object', properties: { name: { type: 'string' }, enabled: { type: 'boolean' }, toolCount: { type: 'number' } } } },
        enabledCount: { type: 'number' },
        disabledCount: { type: 'number' }
      }
    }
  },
  {
    name: 'manage_asset',
    category: 'core',
    description: 'Create, import, duplicate, rename, delete assets. Edit Material graphs and instances. Analyze dependencies.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'list', 'import', 'duplicate', 'duplicate_asset', 'rename', 'rename_asset', 'move', 'move_asset', 'delete', 'delete_asset', 'delete_assets', 'create_folder', 'search_assets',
            'get_dependencies', 'get_source_control_state', 'analyze_graph', 'get_asset_graph', 'create_thumbnail', 'set_tags', 'get_metadata', 'set_metadata', 'validate', 'fixup_redirectors', 'find_by_tag', 'generate_report',
            'create_material', 'create_material_instance', 'create_render_target', 'generate_lods', 'add_material_parameter', 'list_instances', 'reset_instance_parameters', 'exists', 'get_material_stats',
            'nanite_rebuild_mesh', 'bulk_rename', 'bulk_delete', 'source_control_checkout', 'source_control_submit',
            'add_material_node', 'connect_material_pins', 'remove_material_node', 'break_material_connections', 'get_material_node_details', 'rebuild_material'
          ],
          description: 'Action to perform'
        },
        assetPath: commonSchemas.assetPath,
        directory: commonSchemas.directoryPath,
        classNames: commonSchemas.arrayOfStrings,
        packagePaths: commonSchemas.arrayOfStrings,
        recursivePaths: commonSchemas.booleanProp,
        recursiveClasses: commonSchemas.booleanProp,
        limit: commonSchemas.numberProp,
        sourcePath: commonSchemas.sourcePath,
        destinationPath: commonSchemas.destinationPath,
        assetPaths: commonSchemas.arrayOfStrings,
        lodCount: commonSchemas.numberProp,
        reductionSettings: commonSchemas.objectProp,
        nodeName: commonSchemas.name,
        eventName: commonSchemas.eventName,
        memberClass: commonSchemas.stringProp,
        posX: commonSchemas.numberProp,
        posY: commonSchemas.numberProp,
        newName: commonSchemas.newName,
        overwrite: commonSchemas.overwrite,
        save: commonSchemas.save,
        fixupRedirectors: commonSchemas.booleanProp,
        directoryPath: commonSchemas.directoryPath,
        name: commonSchemas.name,
        path: commonSchemas.directoryPath,
        parentMaterial: commonSchemas.materialPath,
        parameters: commonSchemas.objectProp,
        width: commonSchemas.numberProp,
        height: commonSchemas.numberProp,
        format: commonSchemas.stringProp,
        meshPath: commonSchemas.meshPath,
        tag: commonSchemas.tagName,
        metadata: commonSchemas.objectProp,
        graphName: commonSchemas.graphName,
        nodeType: commonSchemas.stringProp,
        nodeId: commonSchemas.nodeId,
        sourceNodeId: commonSchemas.sourceNodeId,
        targetNodeId: commonSchemas.targetNodeId,
        inputName: commonSchemas.pinName,
        fromNodeId: commonSchemas.sourceNodeId,
        fromPin: commonSchemas.sourcePin,
        toNodeId: commonSchemas.targetNodeId,
        toPin: commonSchemas.targetPin,
        parameterName: commonSchemas.parameterName,
        value: commonSchemas.value,
        x: commonSchemas.numberProp,
        y: commonSchemas.numberProp,
        comment: commonSchemas.stringProp,
        parentNodeId: commonSchemas.nodeId,
        childNodeId: commonSchemas.nodeId,
        maxDepth: commonSchemas.numberProp,
        // Bulk operations (C++ TryGetStringField)
        prefix: commonSchemas.stringProp,
        suffix: commonSchemas.stringProp,
        searchText: commonSchemas.stringProp,
        replaceText: commonSchemas.stringProp,
        paths: commonSchemas.arrayOfStrings,
        // Source control (C++ TryGetStringField)
        description: commonSchemas.stringProp,
        checkoutFiles: commonSchemas.booleanProp,
        // Bulk delete
        showConfirmation: commonSchemas.booleanProp,
        // Material graph operations
        pinName: commonSchemas.pinName,
        desc: commonSchemas.stringProp,
        materialPath: commonSchemas.materialPath,
        texturePath: commonSchemas.texturePath,
        expressionClass: commonSchemas.stringProp,
        coordinateIndex: commonSchemas.numberProp,
        parameterType: commonSchemas.stringProp,
        nodes: commonSchemas.arrayOfObjects,
        tags: commonSchemas.arrayOfStrings,
        // Handler aliases (alternative parameter names accepted by handler)
        folderPath: commonSchemas.directoryPath,
        sourceNode: commonSchemas.sourceNodeId,
        targetNode: commonSchemas.targetNodeId,
        outputPin: commonSchemas.sourcePin,
        inputPin: commonSchemas.targetPin,
        type: commonSchemas.stringProp,
        defaultValue: commonSchemas.value,
        expressionIndex: commonSchemas.nodeId
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        ...commonSchemas.outputBase,
        assets: commonSchemas.arrayOfObjects,
        paths: commonSchemas.arrayOfStrings,
        path: commonSchemas.stringProp,
        nodeId: commonSchemas.nodeId,
        details: commonSchemas.objectProp
      }
    }
  },
  {
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
            'set_actor_collision', 'call_actor_function'
          ],
          description: 'Action'
        },
        actorName: commonSchemas.actorName,
        childActor: commonSchemas.childActorName,
        parentActor: commonSchemas.parentActorName,
        classPath: commonSchemas.assetPath,
        meshPath: commonSchemas.meshPath,
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
        snapshotName: commonSchemas.stringProp
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
        data: commonSchemas.nullableObjectProp
      }
    }
  },
  {
    name: 'control_editor',
    category: 'core',
    description: 'Start/stop PIE, control viewport camera, run console commands, take screenshots, simulate input.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'play', 'stop', 'stop_pie', 'pause', 'resume', 'eject', 'possess',
            'set_game_speed', 'set_fixed_delta_time',
            'set_camera', 'set_camera_position', 'set_viewport_camera', 'set_camera_fov',
            'set_view_mode', 'set_viewport_resolution',
            'console_command', 'execute_command',
            'screenshot', 'take_screenshot', 'step_frame', 'single_frame_step',
            'start_recording', 'stop_recording',
            'create_bookmark', 'jump_to_bookmark',
            'set_preferences', 'set_viewport_realtime',
            'open_asset', 'close_asset', 'simulate_input',
            'open_level', 'focus_actor',
            'show_stats', 'hide_stats',
            'set_editor_mode', 'set_immersive_mode', 'set_game_view',
            'undo', 'redo', 'save_all'
          ],
          description: 'Editor action'
        },
        location: commonSchemas.location,
        rotation: commonSchemas.rotation,
        viewMode: commonSchemas.stringProp,
        enabled: commonSchemas.enabled,
        speed: commonSchemas.numberProp,
        filename: commonSchemas.stringProp,
        fov: commonSchemas.numberProp,
        width: commonSchemas.numberProp,
        height: commonSchemas.numberProp,
        command: commonSchemas.stringProp,
        steps: commonSchemas.integerProp,
        bookmarkName: commonSchemas.stringProp,
        // Path parameters for various actions
        assetPath: commonSchemas.assetPath,
        levelPath: commonSchemas.levelPath,
        path: commonSchemas.directoryPath,
        // Actor-related parameters
        actorName: commonSchemas.actorName,
        name: commonSchemas.name,
        // Action-specific parameters
        mode: commonSchemas.stringProp,
        deltaTime: commonSchemas.numberProp,
        resolution: commonSchemas.resolution,
        realtime: commonSchemas.booleanProp,
        stat: commonSchemas.stringProp,
        category: commonSchemas.stringProp,
        preferences: commonSchemas.objectProp,
        section: commonSchemas.stringProp,
        key: commonSchemas.stringProp,
        value: commonSchemas.value,
        // simulate_input parameters
        inputAction: commonSchemas.stringProp,
        axis: commonSchemas.stringProp
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        ...commonSchemas.outputBase
      }
    }
  },
  {
    name: 'manage_level',
    category: 'core',
    description: 'Load/save levels, configure streaming, manage World Partition cells, and build lighting.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'load', 'save', 'save_as', 'save_level_as', 'stream', 'unload', 'create_level', 'create_light', 'build_lighting',
            'set_metadata', 'load_cells', 'set_datalayer', 'create_datalayer',
            'export_level', 'import_level', 'list_levels', 'get_summary', 'delete', 'delete_level', 'validate_level',
            'cleanup_invalid_datalayers', 'add_sublevel', 'rename_level', 'duplicate_level', 'get_current_level'
          ],
          description: 'Action'
        },
        // Level path parameters
        levelPath: commonSchemas.levelPath,
        levelPaths: commonSchemas.arrayOfStrings,
        levelName: commonSchemas.stringProp,
        path: commonSchemas.directoryPathForCreation,
        // Save/export/import paths
        savePath: commonSchemas.savePath,
        destinationPath: commonSchemas.destinationPath,
        targetPath: commonSchemas.directoryPath,
        exportPath: commonSchemas.exportPath,
        packagePath: commonSchemas.directoryPath,
        sourcePath: commonSchemas.sourcePath,
        // Sublevel parameters
        sublevelPath: commonSchemas.levelPath,
        parentLevel: commonSchemas.parentLevel,
        parentPath: commonSchemas.directoryPath,
        streamingMethod: commonSchemas.stringProp,
        // Streaming control
        streaming: commonSchemas.booleanProp,
        shouldBeLoaded: commonSchemas.booleanProp,
        shouldBeVisible: commonSchemas.booleanProp,
        // Light creation
        lightType: { type: 'string', enum: ['Directional', 'Point', 'Spot', 'Rect', 'DirectionalLight', 'PointLight', 'SpotLight', 'RectLight', 'directional', 'point', 'spot', 'rect'], description: 'Light type. Accepts short names (Point), class names (PointLight), or lowercase (point).' },
        intensity: commonSchemas.numberProp,
        color: commonSchemas.color,
        location: commonSchemas.location,
        rotation: commonSchemas.rotation,
        // World Partition / Data Layers
        cells: commonSchemas.arrayOfStrings,
        dataLayerName: commonSchemas.dataLayerName,
        dataLayerLabel: commonSchemas.stringProp,
        dataLayerState: commonSchemas.stringProp,
        actorPath: commonSchemas.actorPath,
        // Cell bounds
        min: commonSchemas.location,
        max: commonSchemas.location,
        origin: commonSchemas.location,
        extent: commonSchemas.extent,
        // Level creation
        template: commonSchemas.stringProp,
        useWorldPartition: commonSchemas.booleanProp,
        // Metadata & utilities
        metadata: commonSchemas.objectProp,
        newName: commonSchemas.stringProp,
        timeoutMs: commonSchemas.numberProp
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        ...commonSchemas.outputBase,
        assetPath: commonSchemas.assetPath,
        emitterName: commonSchemas.stringProp,
        moduleName: commonSchemas.stringProp,
        niagaraInfo: commonSchemas.objectProp,
        validationResult: commonSchemas.objectProp
      }
    }
  },
  {
    name: 'system_control',
    category: 'core',
    description: 'Run profiling, set quality/CVars, execute console commands, run UBT, and manage widgets.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'profile', 'show_fps', 'set_quality', 'screenshot', 'set_resolution', 'set_fullscreen', 'execute_command', 'console_command',
            'run_ubt', 'run_tests', 'subscribe', 'unsubscribe', 'spawn_category', 'start_session', 'lumen_update_scene',
            'play_sound', 'create_widget', 'show_widget', 'add_widget_child',
            'set_cvar', 'get_project_settings', 'validate_assets',
            'set_project_setting'
          ],
          description: 'Action'
        },
        profileType: commonSchemas.stringProp,
        category: commonSchemas.stringProp,
        level: commonSchemas.numberProp,
        enabled: commonSchemas.enabled,
        resolution: commonSchemas.resolution,
        command: commonSchemas.stringProp,
        target: commonSchemas.stringProp,
        platform: commonSchemas.stringProp,
        configuration: commonSchemas.stringProp,
        arguments: commonSchemas.stringProp,
        filter: commonSchemas.stringProp,
        channels: commonSchemas.stringProp,
        widgetPath: commonSchemas.widgetPath,
        childClass: commonSchemas.stringProp,
        parentName: commonSchemas.stringProp,
        section: commonSchemas.stringProp,
        key: commonSchemas.stringProp,
        value: commonSchemas.stringProp,
        configName: commonSchemas.stringProp
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        ...commonSchemas.outputBase,
        output: commonSchemas.stringProp
      }
    }
  },
  {
    name: 'inspect',
    category: 'core',
    description: 'Inspect any UObject: read/write properties, list components, export snapshots, and query class info.',
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
            'inspect_class', 'list_objects',
            'get_metadata', 'add_tag', 'find_by_tag',
            'create_snapshot', 'restore_snapshot', 'export', 'delete_object', 'find_by_class', 'get_bounding_box',
            'get_project_settings', 'get_world_settings', 'get_viewport_info', 'get_selected_actors',
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
        destinationPath: commonSchemas.destinationPath,
        outputPath: commonSchemas.outputPath,
        format: commonSchemas.stringProp
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        ...commonSchemas.outputBase,
        value: commonSchemas.value
      }
    }
  }
];
