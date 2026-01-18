import { commonSchemas } from './tool-definition-utils.js';

/**
 * Tool annotation schema for MCP 2025-11-25 spec compliance
 * Phase E4: Add structured metadata to tools
 */
export interface ToolAnnotation {
  /** Target audience for this tool */
  audience?: string[];
  /** Priority for sorting (1-10, higher = more important) */
  priority?: number;
  /** Semantic tags for categorization and discovery */
  tags?: string[];
}

/** MCP Tool Definition type for explicit annotation to avoid TS7056 */
export interface ToolDefinition {
  category?: 'core' | 'world' | 'authoring' | 'gameplay' | 'utility';
  name: string;
  description: string;
  inputSchema: Record<string, unknown>;
  outputSchema?: Record<string, unknown>;
  /** Phase E4: Structured tool annotations */
  annotations?: ToolAnnotation;
  /** Mark tool as experimental (hidden unless EXPERIMENTAL_TOOLS=true) */
  experimental?: boolean;
  [key: string]: unknown;
}
export const consolidatedToolDefinitions: ToolDefinition[] = [
  {
    name: 'configure_tools',
    description: 'MCP meta-tool: filter which MCP tools are visible by category. NOT an Unreal Engine tool.',
    category: 'core',
    annotations: {
      audience: ['developer'],
      priority: 10,
      tags: ['system', 'meta', 'mcp', 'configuration']
    },
    inputSchema: {
      type: 'object',
      properties: {
        action: { type: 'string', enum: ['set_categories', 'list_categories', 'get_status'], description: 'list_categories: show available. set_categories: enable categories. get_status: current state.' },
        categories: { type: 'array', items: commonSchemas.stringProp, description: 'Categories: core, world, authoring, gameplay, utility, all' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        ...commonSchemas.outputBase,
        categories: { type: 'array', items: commonSchemas.stringProp }
      }
    }
  },
  {
    name: 'manage_asset',
    category: 'core',
    description: 'Assets, Materials, dependencies; Blueprints (SCS, graph nodes).',
    annotations: {
      audience: ['developer', 'designer'],
      priority: 9,
      tags: ['asset', 'blueprint', 'material', 'metasound', 'nanite']
    },
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'list', 'import', 'duplicate', 'rename', 'move', 'delete', 'delete_asset', 'delete_assets', 'create_folder', 'search_assets',
            'get_dependencies', 'get_source_control_state', 'analyze_graph', 'get_asset_graph', 'create_thumbnail', 'set_tags', 'get_metadata', 'set_metadata', 'validate', 'fixup_redirectors', 'find_by_tag', 'generate_report',
            'create_material', 'create_material_instance', 'create_render_target', 'generate_lods', 'add_material_parameter', 'list_instances', 'reset_instance_parameters', 'exists', 'get_material_stats',
            'nanite_rebuild_mesh',
            // Nanite (Phase 3G.2)
            'enable_nanite_mesh', 'set_nanite_settings', 'batch_nanite_convert',
            'add_material_node', 'connect_material_pins', 'remove_material_node', 'break_material_connections', 'get_material_node_details', 'rebuild_material',
            // MetaSounds (Phase 3C.1 & 3C.2)
            'create_metasound', 'add_metasound_node', 'connect_metasound_nodes', 'remove_metasound_node', 'set_metasound_variable',
            'create_oscillator', 'create_envelope', 'create_filter', 'create_sequencer_node', 'create_procedural_music',
            'import_audio_to_metasound', 'export_metasound_preset', 'configure_audio_modulation',
            // Blueprints (merged from manage_blueprint)
            'bp_create', 'bp_get', 'bp_compile', 'bp_add_component', 'bp_set_default', 'bp_modify_scs', 'bp_get_scs',
            'bp_add_scs_component', 'bp_remove_scs_component', 'bp_reparent_scs_component', 'bp_set_scs_transform', 'bp_set_scs_property',
            'bp_ensure_exists', 'bp_probe_handle', 'bp_add_variable', 'bp_remove_variable', 'bp_rename_variable',
            'bp_add_function', 'bp_add_event', 'bp_remove_event', 'bp_add_construction_script', 'bp_set_variable_metadata',
            'bp_create_node', 'bp_add_node', 'bp_delete_node', 'bp_connect_pins', 'bp_break_pin_links',
            'bp_set_node_property', 'bp_create_reroute_node', 'bp_get_node_details', 'bp_get_graph_details',
            'bp_get_pin_details', 'bp_list_node_types', 'bp_set_pin_default_value',
            // Wave 1.14: Query Enhancement
            'query_assets_by_predicate',
            // Wave 2.1-2.10: Blueprint Enhancement
            'bp_implement_interface', 'bp_add_macro', 'bp_create_widget_binding', 'bp_add_custom_event',
            'bp_set_replication_settings', 'bp_add_event_dispatcher', 'bp_bind_event',
            'get_blueprint_dependencies', 'validate_blueprint', 'compile_blueprint_batch'
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
        // MetaSound properties
        metaSoundName: commonSchemas.name,
        metaSoundPath: commonSchemas.assetPath,
        // Nanite properties
        enableNanite: { type: 'boolean', description: 'Enable/Disable Nanite.' },
        nanitePositionPrecision: { type: 'number', description: 'Nanite position precision.' },
        nanitePercentTriangles: { type: 'number', description: 'Nanite keep triangle percent.' },
        naniteFallbackRelativeError: { type: 'number', description: 'Fallback relative error.' },
        refresh: { type: 'boolean', description: 'Force refresh cache.' }
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
  // [MERGED] manage_blueprint actions now in manage_asset
  {
    name: 'control_actor',
    category: 'core',
    description: 'Spawn actors, transforms, physics, components, tags, attachments.',
    annotations: {
      audience: ['developer', 'designer'],
      priority: 9,
      tags: ['actor', 'spawn', 'transform', 'component', 'physics']
    },
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'spawn', 'spawn_blueprint', 'delete', 'delete_by_tag', 'duplicate',
            'apply_force', 'set_transform', 'get_transform', 'set_visibility',
            'add_component', 'set_component_properties', 'get_components',
            'add_tag', 'remove_tag', 'find_by_tag', 'find_by_name', 'list', 'set_blueprint_variables',
            'create_snapshot', 'attach', 'detach',
            // Inspect (merged from inspect tool)
            'inspect_object', 'set_property', 'get_property', 'inspect_class', 'list_objects',
            'get_component_property', 'set_component_property', 'get_metadata',
            'restore_snapshot', 'export', 'delete_object', 'find_by_class', 'get_bounding_box',
            // Wave 1.13: Query Enhancement
            'query_actors_by_predicate',
            // Wave 2.11-2.20: Actor Enhancement
            'get_all_component_properties', 'batch_set_component_properties', 'clone_component_hierarchy',
            'serialize_actor_state', 'deserialize_actor_state', 'get_actor_bounds',
            'batch_transform_actors', 'get_actor_references', 'replace_actor_class', 'merge_actors'
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
        snapshotName: commonSchemas.stringProp,
        // Inspect (merged from inspect tool)
        objectPath: commonSchemas.assetPath,
        propertyName: commonSchemas.propertyName,
        propertyPath: commonSchemas.stringProp,
        value: commonSchemas.value,
        name: commonSchemas.name,
        className: commonSchemas.stringProp,
        filter: commonSchemas.stringProp,
        destinationPath: commonSchemas.destinationPath,
        outputPath: commonSchemas.outputPath,
        format: commonSchemas.stringProp,
        refresh: { type: 'boolean', description: 'Force refresh cache.' }
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
        data: commonSchemas.objectProp
      }
    }
  },
  {
    name: 'control_editor',
    category: 'core',
    description: 'PIE, viewport, console, screenshots, profiling, CVars, UBT, widgets.',
    annotations: {
      audience: ['developer'],
      priority: 9,
      tags: ['editor', 'viewport', 'pie', 'console', 'input']
    },
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'play', 'stop', 'stop_pie', 'pause', 'resume', 'set_game_speed', 'eject', 'possess',
            'set_camera', 'set_camera_position', 'set_camera_fov', 'set_view_mode',
            'set_viewport_resolution', 'console_command', 'execute_command', 'screenshot', 'step_frame',
            'start_recording', 'stop_recording', 'create_bookmark', 'jump_to_bookmark',
            'set_preferences', 'set_viewport_realtime', 'open_asset', 'simulate_input',
            // Enhanced Input System (merged from manage_input)
            'create_input_action', 'create_input_mapping_context', 'add_mapping', 'remove_mapping',
            // System control (merged from system_control)
            'profile', 'show_fps', 'set_quality', 'set_resolution', 'set_fullscreen',
            'run_ubt', 'run_tests', 'subscribe', 'unsubscribe', 'spawn_category', 'start_session', 'lumen_update_scene',
            // Event Push System (Phase 4.1)
            'subscribe_to_event', 'unsubscribe_from_event', 'get_subscribed_events', 'configure_event_channel', 'get_event_history', 'clear_event_subscriptions',
            // Background Jobs (Phase 4.3)
            'start_background_job', 'get_job_status', 'cancel_job', 'get_active_jobs',
            'play_sound', 'create_widget', 'show_widget', 'add_widget_child',
            'set_cvar', 'get_project_settings', 'validate_assets', 'set_project_setting',
            'batch_execute', 'parallel_execute', 'queue_operations', 'flush_operation_queue',
            // Viewport capture (G1)
            'capture_viewport',
            // Wave 1.1-1.4: Error Recovery
            'get_last_error_details', 'suggest_fix_for_error', 'validate_operation_preconditions', 'get_operation_history',
            // Wave 1.9-1.12: Introspection
            'get_available_actions', 'explain_action_parameters', 'get_class_hierarchy', 'validate_action_input',
            // Wave 1.15-1.16: Query Enhancements
            'get_action_statistics', 'get_bridge_health',
            // Wave 2.21-2.30: Editor Enhancement (incl. 5.7 features)
            'configure_megalights', 'get_light_budget_stats', 'convert_to_substrate', 'batch_substrate_migration',
            'record_input_session', 'playback_input_session', 'capture_viewport_sequence',
            'set_editor_mode', 'get_selection_info', 'toggle_realtime_rendering'
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
        assetPath: commonSchemas.assetPath,
        keyName: commonSchemas.stringProp,
        eventType: { type: 'string', enum: ['KeyDown', 'KeyUp', 'Both'] },
        // Enhanced Input (merged from manage_input)
        contextPath: commonSchemas.assetPath,
        actionPath: commonSchemas.assetPath,
        key: commonSchemas.stringProp,
        // System control (merged from system_control)
        profileType: commonSchemas.stringProp,
        category: commonSchemas.stringProp,
        level: commonSchemas.numberProp,
        resolution: commonSchemas.resolution,
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
        value: commonSchemas.stringProp,
        configName: commonSchemas.stringProp,
        // capture_viewport options (G1)
        outputPath: commonSchemas.stringProp,
        format: { type: 'string', enum: ['png', 'jpg', 'bmp'] },
        returnBase64: commonSchemas.booleanProp,
        captureHDR: commonSchemas.booleanProp,
        showUI: commonSchemas.booleanProp,
        requests: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              tool: { type: 'string' },
              action: { type: 'string' },
              payload: { type: 'object' }
            },
            required: ['tool', 'action']
          },
          description: 'List of requests to execute in batch.'
        },
        // Batch operation properties (Wave 1.5-1.8)
        operations: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              tool: { type: 'string' },
              action: { type: 'string' },
              parameters: { type: 'object' }
            },
            required: ['tool', 'action']
          },
          description: 'Operations for batch_execute, parallel_execute, queue_operations.'
        },
        stopOnError: commonSchemas.booleanProp,
        maxConcurrency: { type: 'integer', description: 'Max concurrent operations for parallel_execute (default 10, max 10).' },
        queueId: commonSchemas.stringProp
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
    description: 'Levels, streaming, World Partition, data layers, HLOD; PCG graphs.',
    annotations: {
      audience: ['developer', 'designer'],
      priority: 8,
      tags: ['level', 'streaming', 'world-partition', 'pcg', 'hlod']
    },
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            // Core level operations
            'load', 'save', 'save_as', 'save_level_as', 'stream', 'create_level', 'create_light', 'build_lighting',
            'set_metadata', 'load_cells', 'set_datalayer', 'export_level', 'import_level', 'list_levels', 'get_summary',
            'delete', 'validate_level', 'cleanup_invalid_datalayers', 'add_sublevel',
            // Level structure (merged from manage_level_structure)
            'create_sublevel', 'configure_level_streaming', 'set_streaming_distance', 'configure_level_bounds',
            'enable_world_partition', 'configure_grid_size', 'create_data_layer', 'assign_actor_to_data_layer',
            'configure_hlod_layer', 'create_minimap_volume', 'open_level_blueprint', 'add_level_blueprint_node',
            'connect_level_blueprint_nodes', 'create_level_instance', 'create_packed_level_actor', 'get_level_structure_info',
            // World Partition Phase 3H
            'configure_world_partition', 'create_streaming_volume', 'configure_large_world_coordinates',
            'create_world_partition_cell', 'configure_runtime_loading', 'configure_world_settings',
            // PCG (merged from manage_pcg)
            'create_pcg_graph', 'create_pcg_subgraph', 'add_pcg_node', 'connect_pcg_pins', 'set_pcg_node_settings',
            'add_landscape_data_node', 'add_spline_data_node', 'add_volume_data_node', 'add_actor_data_node', 'add_texture_data_node',
            'add_surface_sampler', 'add_mesh_sampler', 'add_spline_sampler', 'add_volume_sampler',
            'add_bounds_modifier', 'add_density_filter', 'add_height_filter', 'add_slope_filter',
            'add_distance_filter', 'add_bounds_filter', 'add_self_pruning',
            'add_transform_points', 'add_project_to_surface', 'add_copy_points', 'add_merge_points',
            'add_static_mesh_spawner', 'add_actor_spawner', 'add_spline_spawner',
            'execute_pcg_graph', 'set_pcg_partition_grid_size', 'get_pcg_info',
            // Advanced PCG (Phase 3A.2)
            'create_biome_rules', 'blend_biomes', 'export_pcg_to_static', 'import_pcg_preset', 'debug_pcg_execution',
            // Wave 2.31-2.40: Level Enhancement (incl. PCG GPU)
            'create_pcg_hlsl_node', 'enable_pcg_gpu_processing', 'configure_pcg_mode_brush',
            'export_pcg_hlsl_template', 'batch_execute_pcg_with_gpu',
            'get_world_partition_cells', 'stream_level_async', 'get_streaming_levels_status',
            'configure_hlod_settings', 'build_hlod_for_level'
          ],
          description: 'Level action'
        },
        // Core properties
        graphPath: commonSchemas.directoryPath,
        levelPath: commonSchemas.levelPath,
        levelName: commonSchemas.stringProp,
        streaming: commonSchemas.booleanProp,
        shouldBeLoaded: commonSchemas.booleanProp,
        shouldBeVisible: commonSchemas.booleanProp,
        lightType: { type: 'string', enum: ['Directional', 'Point', 'Spot', 'Rect'] },
        location: commonSchemas.location,
        intensity: commonSchemas.numberProp,
        quality: commonSchemas.stringProp,
        min: commonSchemas.arrayOfNumbers,
        max: commonSchemas.arrayOfNumbers,
        dataLayerLabel: commonSchemas.stringProp,
        dataLayerState: commonSchemas.stringProp,
        recursive: commonSchemas.recursive,
        exportPath: commonSchemas.outputPath,
        packagePath: commonSchemas.assetPath,
        destinationPath: commonSchemas.destinationPath,
        note: commonSchemas.stringProp,
        levelPaths: commonSchemas.arrayOfStrings,
        subLevelPath: commonSchemas.levelPath,
        parentLevel: commonSchemas.stringProp,
        streamingMethod: { type: 'string', enum: ['Blueprint', 'AlwaysLoaded', 'Disabled'] },
        // Level structure properties (merged)
        templateLevel: commonSchemas.templateLevel,
        bCreateWorldPartition: { type: 'boolean', description: 'Create with World Partition.' },
        sublevelName: commonSchemas.sublevelName,
        sublevelPath: commonSchemas.levelPath,
        bShouldBeVisible: { type: 'boolean', description: 'Level visible when loaded.' },
        bShouldBlockOnLoad: { type: 'boolean', description: 'Block until loaded.' },
        bDisableDistanceStreaming: { type: 'boolean', description: 'Disable distance streaming.' },
        streamingDistance: { type: 'number', description: 'Streaming volume distance.' },
        streamingUsage: { type: 'string', enum: ['Loading', 'LoadingAndVisibility', 'VisibilityBlockingOnLoad', 'BlockingOnLoad', 'LoadingNotVisible'], description: 'Streaming usage.' },
        createVolume: { type: 'boolean', description: 'Create streaming volume.' },
        boundsOrigin: commonSchemas.location,
        boundsExtent: commonSchemas.location,
        bAutoCalculateBounds: { type: 'boolean', description: 'Auto-calculate bounds.' },
        bEnableWorldPartition: { type: 'boolean', description: 'Enable World Partition.' },
        gridCellSize: { type: 'number', description: 'WP grid cell size.' },
        loadingRange: { type: 'number', description: 'WP loading range.' },
        dataLayerName: commonSchemas.dataLayerName,
        bIsInitiallyVisible: { type: 'boolean', description: 'Data layer visible.' },
        bIsInitiallyLoaded: { type: 'boolean', description: 'Data layer loaded.' },
        dataLayerType: { type: 'string', enum: ['Runtime', 'Editor'], description: 'Data layer type.' },
        actorName: commonSchemas.actorName,
        actorPath: commonSchemas.actorPath,
        hlodLayerName: { type: 'string', description: 'HLOD layer name.' },
        hlodLayerPath: commonSchemas.hlodLayerPath,
        bIsSpatiallyLoaded: { type: 'boolean', description: 'HLOD spatially loaded.' },
        cellSize: { type: 'number', description: 'HLOD cell size.' },
        loadingDistance: { type: 'number', description: 'HLOD loading distance.' },
        volumeName: commonSchemas.volumeName,
        volumeLocation: commonSchemas.location,
        volumeExtent: commonSchemas.location,
        nodeClass: commonSchemas.nodeClass,
        nodePosition: commonSchemas.vector2,
        nodeName: commonSchemas.nodeName,
        sourceNodeName: commonSchemas.sourceNode,
        sourcePinName: commonSchemas.sourcePin,
        targetNodeName: commonSchemas.targetNode,
        targetPinName: commonSchemas.targetPin,
        levelInstanceName: commonSchemas.levelInstanceName,
        levelAssetPath: { type: 'string', description: 'Level asset for instancing.' },
        instanceLocation: commonSchemas.location,
        instanceRotation: commonSchemas.rotation,
        instanceScale: commonSchemas.scale,
        packedLevelName: { type: 'string', description: 'Packed level actor name.' },
        bPackBlueprints: { type: 'boolean', description: 'Include blueprints.' },
        bPackStaticMeshes: { type: 'boolean', description: 'Include static meshes.' },
        // World Partition Phase 3H properties
        enableLargeWorlds: { type: 'boolean', description: 'Enable Large World Coordinates.' },
        defaultGameMode: { type: 'string', description: 'Default game mode class path.' },
        killZ: { type: 'number', description: 'Kill Z height for level.' },
        worldGravityZ: { type: 'number', description: 'World gravity Z value.' },
        runtimeCellSize: { type: 'number', description: 'Runtime World Partition cell size.' },
        streamingVolumeExtent: commonSchemas.location,
        save: commonSchemas.save
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        ...commonSchemas.outputBase,
        data: commonSchemas.objectProp,
        levelPath: commonSchemas.levelPath,
        sublevelPath: commonSchemas.levelPath
      }
    }
  },
  {
    name: 'manage_motion_design',
    category: 'authoring',
    description: 'Motion Design (Avalanche) tools: Cloners, Effectors, Mograph.',
    annotations: {
      audience: ['designer'],
      priority: 6,
      tags: ['mograph', 'cloner', 'effector', 'animation']
    },
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'create_cloner', 'configure_cloner_pattern', 'add_effector', 'animate_effector', 'create_mograph_sequence',
            'create_radial_cloner', 'create_spline_cloner', 'add_noise_effector', 'configure_step_effector', 'export_mograph_to_sequence'
          ],
          description: 'Motion Design action'
        },
        clonerName: commonSchemas.actorName,
        clonerType: { type: 'string', enum: ['Grid', 'Linear', 'Radial', 'Honeycomb', 'Mesh'], description: 'Cloner distribution type.' },
        sourceActor: { type: 'string', description: 'Source actor to clone.' },
        location: commonSchemas.location,
        clonerActor: { type: 'string', description: 'Target cloner actor.' },
        countX: { type: 'number', description: 'Grid count X.' },
        countY: { type: 'number', description: 'Grid count Y.' },
        countZ: { type: 'number', description: 'Grid count Z.' },
        offset: commonSchemas.vector3,
        rotation: commonSchemas.rotation,
        scale: commonSchemas.scale,
        radius: { type: 'number', description: 'Radial radius.' },
        count: { type: 'number', description: 'Radial count.' },
        axis: { type: 'string', enum: ['X', 'Y', 'Z'], description: 'Radial axis.' },
        align: { type: 'boolean', description: 'Radial align.' },
        splineActor: { type: 'string', description: 'Spline actor for distribution.' },
        effectorType: { type: 'string', enum: ['Noise', 'Step', 'Push', 'Spherical', 'Plain'], description: 'Effector type.' },
        effectorName: { type: 'string', description: 'Name of effector.' },
        effectorActor: { type: 'string', description: 'Target effector actor.' },
        strength: { type: 'number', description: 'Effector strength.' },
        frequency: { type: 'number', description: 'Effector frequency.' },
        stepCount: { type: 'number', description: 'Step count.' },
        operation: { type: 'string', enum: ['Add', 'Multiply'], description: 'Effector operation.' },
        propertyName: commonSchemas.propertyName,
        startValue: { type: 'number', description: 'Start value.' },
        endValue: { type: 'number', description: 'End value.' },
        duration: { type: 'number', description: 'Duration in seconds.' },
        sequencePath: commonSchemas.sequencePath
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        ...commonSchemas.outputBase,
        clonerActor: commonSchemas.actorName,
        effectorActor: commonSchemas.actorName,
        sequencePath: commonSchemas.sequencePath
      }
    }
  },
  {
    name: 'animation_physics',
    category: 'utility',
    description: 'Animation BPs, Montages, IK, retargeting + Chaos destruction/vehicles.',
    annotations: {
      audience: ['developer', 'animator'],
      priority: 7,
      tags: ['animation', 'physics', 'chaos', 'vehicle', 'cloth', 'ragdoll']
    },
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            // Runtime animation & physics
            'create_animation_bp', 'play_montage', 'setup_ragdoll', 'activate_ragdoll', 'configure_vehicle',
            'create_blend_space', 'create_state_machine', 'setup_ik', 'create_procedural_anim',
            'create_blend_tree', 'setup_retargeting', 'setup_physics_simulation', 'cleanup',
            'create_animation_asset', 'add_notify',
            // Animation authoring (merged from manage_animation_authoring)
            'create_animation_sequence', 'set_sequence_length', 'add_bone_track', 'set_bone_key', 'set_curve_key',
            'add_notify_state', 'add_sync_marker', 'set_root_motion_settings', 'set_additive_settings',
            'create_montage', 'add_montage_section', 'add_montage_slot', 'set_section_timing',
            'add_montage_notify', 'set_blend_in', 'set_blend_out', 'link_sections',
            'create_blend_space_1d', 'create_blend_space_2d', 'add_blend_sample', 'set_axis_settings', 'set_interpolation_settings',
            'create_aim_offset', 'add_aim_offset_sample',
            'create_anim_blueprint', 'add_state_machine', 'add_state', 'add_transition', 'set_transition_rules',
            'add_blend_node', 'add_cached_pose', 'add_slot_node', 'add_layered_blend_per_bone', 'set_anim_graph_node_value',
            'create_control_rig', 'add_control', 'add_rig_unit', 'connect_rig_elements', 'create_pose_library',
            'create_ik_rig', 'add_ik_chain', 'add_ik_goal', 'create_ik_retargeter', 'set_retarget_chain_mapping',
            'get_animation_info',
            // Motion Matching & Advanced (Phase 3F.2)
            'create_pose_search_database', 'configure_motion_matching', 'add_trajectory_prediction',
            'create_animation_modifier', 'setup_ml_deformer', 'configure_ml_deformer_training',
            // Motion Matching Queries (A4)
            'get_motion_matching_state', 'set_motion_matching_goal', 'list_pose_search_databases',
            // Control Rig Queries (A5)
            'get_control_rig_controls', 'set_control_value', 'reset_control_rig',
            // [PhysicsDestruction] CHAOS DESTRUCTION (29 actions - merged)
            'chaos_create_geometry_collection', 'chaos_fracture_uniform', 'chaos_fracture_clustered', 'chaos_fracture_radial',
            'chaos_fracture_slice', 'chaos_fracture_brick', 'chaos_flatten_fracture', 'chaos_set_geometry_collection_materials',
            'chaos_set_damage_thresholds', 'chaos_set_cluster_connection_type', 'chaos_set_collision_particles_fraction',
            'chaos_set_remove_on_break', 'chaos_create_field_system_actor', 'chaos_add_transient_field', 'chaos_add_persistent_field',
            'chaos_add_construction_field', 'chaos_add_field_radial_falloff', 'chaos_add_field_radial_vector',
            'chaos_add_field_uniform_vector', 'chaos_add_field_noise', 'chaos_add_field_strain', 'chaos_create_anchor_field',
            'chaos_set_dynamic_state', 'chaos_enable_clustering', 'chaos_get_geometry_collection_stats',
            'chaos_create_geometry_collection_cache', 'chaos_record_geometry_collection_cache',
            'chaos_apply_cache_to_collection', 'chaos_remove_geometry_collection_cache',
            // [PhysicsDestruction] CHAOS VEHICLES (19 actions - merged)
            'chaos_create_wheeled_vehicle_bp', 'chaos_add_vehicle_wheel', 'chaos_remove_wheel_from_vehicle',
            'chaos_configure_engine_setup', 'chaos_configure_transmission_setup', 'chaos_configure_steering_setup',
            'chaos_configure_differential_setup', 'chaos_configure_suspension_setup', 'chaos_configure_brake_setup',
            'chaos_set_vehicle_mesh', 'chaos_set_wheel_class', 'chaos_set_wheel_offset', 'chaos_set_wheel_radius',
            'chaos_set_vehicle_mass', 'chaos_set_drag_coefficient', 'chaos_set_center_of_mass',
            'chaos_create_vehicle_animation_instance', 'chaos_set_vehicle_animation_bp', 'chaos_get_vehicle_config',
            // [PhysicsDestruction] CHAOS CLOTH (15 actions - merged)
            'chaos_create_cloth_config', 'chaos_create_cloth_shared_sim_config',
            'chaos_apply_cloth_to_skeletal_mesh', 'chaos_remove_cloth_from_skeletal_mesh',
            'chaos_set_cloth_mass_properties', 'chaos_set_cloth_gravity', 'chaos_set_cloth_damping',
            'chaos_set_cloth_collision_properties', 'chaos_set_cloth_stiffness', 'chaos_set_cloth_tether_stiffness',
            'chaos_set_cloth_aerodynamics', 'chaos_set_cloth_anim_drive', 'chaos_set_cloth_long_range_attachment',
            'chaos_get_cloth_config', 'chaos_get_cloth_stats',
            // [PhysicsDestruction] CHAOS FLESH (13 actions - merged)
            'chaos_create_flesh_asset', 'chaos_create_flesh_component', 'chaos_set_flesh_simulation_properties',
            'chaos_set_flesh_stiffness', 'chaos_set_flesh_damping', 'chaos_set_flesh_incompressibility',
            'chaos_set_flesh_inflation', 'chaos_set_flesh_solver_iterations', 'chaos_bind_flesh_to_skeleton',
            'chaos_set_flesh_rest_state', 'chaos_create_flesh_cache', 'chaos_record_flesh_simulation',
            'chaos_get_flesh_asset_info',
            // [PhysicsDestruction] Utility (4 actions - merged)
            'chaos_get_physics_destruction_info', 'chaos_list_geometry_collections', 'chaos_list_chaos_vehicles', 'chaos_get_plugin_status',
            // Wave 2.41-2.50: Animation Enhancement (incl. Animator Kit)
            'create_anim_layer', 'stack_anim_layers', 'configure_squash_stretch',
            'create_rigging_layer', 'configure_layer_blend_mode',
            'create_control_rig_physics', 'configure_ragdoll_profile', 'blend_ragdoll_to_animation',
            'get_bone_transforms', 'apply_pose_asset'
          ],
          description: 'Action'
        },
        name: commonSchemas.name,
        actorName: commonSchemas.actorName,
        skeletonPath: commonSchemas.skeletonPath,
        montagePath: commonSchemas.animationPath,
        animationPath: commonSchemas.animationPath,
        playRate: commonSchemas.numberProp,
        physicsAssetName: commonSchemas.stringProp,
        meshPath: commonSchemas.meshPath,
        vehicleName: commonSchemas.stringProp,
        vehicleType: commonSchemas.stringProp,
        savePath: commonSchemas.savePath,
        // Authoring properties (merged from manage_animation_authoring)
        path: commonSchemas.directoryPathForCreation,
        assetPath: commonSchemas.assetPath,
        skeletalMeshPath: commonSchemas.skeletalMeshPath,
        blueprintPath: commonSchemas.blueprintPath,
        save: commonSchemas.save,
        numFrames: commonSchemas.numFrames,
        frameRate: commonSchemas.frameRate,
        boneName: commonSchemas.boneName,
        frame: commonSchemas.frame,
        location: commonSchemas.location,
        rotation: commonSchemas.rotation,
        scale: commonSchemas.scale,
        curveName: commonSchemas.curveName,
        value: commonSchemas.numberProp,
        createIfMissing: commonSchemas.booleanProp,
        notifyClass: commonSchemas.notifyClass,
        notifyName: commonSchemas.notifyName,
        trackIndex: commonSchemas.trackIndex,
        startFrame: commonSchemas.startFrame,
        endFrame: commonSchemas.endFrame,
        markerName: commonSchemas.markerName,
        enableRootMotion: commonSchemas.booleanProp,
        rootMotionRootLock: commonSchemas.stringProp,
        forceRootLock: commonSchemas.booleanProp,
        additiveAnimType: commonSchemas.stringProp,
        basePoseType: commonSchemas.stringProp,
        basePoseAnimation: commonSchemas.stringProp,
        basePoseFrame: commonSchemas.numberProp,
        slotName: commonSchemas.slotName,
        sectionName: commonSchemas.sectionName,
        startTime: commonSchemas.startTime,
        length: commonSchemas.numberProp,
        time: commonSchemas.numberProp,
        blendTime: commonSchemas.blendTime,
        blendOption: commonSchemas.stringProp,
        fromSection: commonSchemas.fromSection,
        toSection: commonSchemas.toSection,
        axisName: commonSchemas.axisName,
        axisMin: commonSchemas.minValue,
        axisMax: commonSchemas.maxValue,
        horizontalAxisName: commonSchemas.horizontalAxisName,
        horizontalMin: commonSchemas.numberProp,
        horizontalMax: commonSchemas.numberProp,
        verticalAxisName: commonSchemas.verticalAxisName,
        verticalMin: commonSchemas.numberProp,
        verticalMax: commonSchemas.numberProp,
        sampleValue: commonSchemas.value,
        axis: commonSchemas.stringProp,
        minValue: commonSchemas.numberProp,
        maxValue: commonSchemas.numberProp,
        gridDivisions: commonSchemas.numberProp,
        interpolationType: commonSchemas.stringProp,
        targetWeightInterpolationSpeed: commonSchemas.numberProp,
        yaw: commonSchemas.numberProp,
        pitch: commonSchemas.numberProp,
        parentClass: commonSchemas.parentClass,
        stateMachineName: commonSchemas.stateMachineName,
        stateName: commonSchemas.stateName,
        isEntryState: commonSchemas.booleanProp,
        fromState: commonSchemas.stringProp,
        toState: commonSchemas.stringProp,
        blendLogicType: commonSchemas.stringProp,
        automaticTriggerRule: commonSchemas.stringProp,
        automaticTriggerTime: commonSchemas.numberProp,
        blendType: commonSchemas.stringProp,
        nodeName: commonSchemas.nodeName,
        x: commonSchemas.nodeX,
        y: commonSchemas.nodeY,
        cacheName: commonSchemas.cacheName,
        layerSetup: commonSchemas.arrayOfObjects,
        propertyName: commonSchemas.propertyName,
        controlName: commonSchemas.controlName,
        controlType: commonSchemas.stringProp,
        parentBone: commonSchemas.parentBone,
        parentControl: commonSchemas.parentControl,
        unitType: commonSchemas.stringProp,
        unitName: commonSchemas.unitName,
        settings: commonSchemas.objectProp,
        sourceElement: commonSchemas.sourceElement,
        sourcePin: commonSchemas.sourcePin,
        targetElement: commonSchemas.targetElement,
        targetPin: commonSchemas.targetPin,
        chainName: commonSchemas.chainName,
        startBone: commonSchemas.startBone,
        endBone: commonSchemas.endBone,
        goal: commonSchemas.goal,
        sourceIKRigPath: commonSchemas.sourceIKRigPath,
        targetIKRigPath: commonSchemas.targetIKRigPath,
        sourceChain: commonSchemas.sourceChain,
        targetChain: commonSchemas.targetChain
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        ...commonSchemas.outputBase,
        assetPath: commonSchemas.assetPath,
        animationInfo: commonSchemas.objectProp
      }
    }
  },
  {
    name: 'manage_effect',
    category: 'utility',
    description: 'Niagara/Cascade particles, debug shapes, VFX graph authoring.',
    annotations: {
      audience: ['developer', 'vfx-artist'],
      priority: 7,
      tags: ['niagara', 'vfx', 'particles', 'debug', 'fluids']
    },
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            // Runtime VFX control
            'particle', 'niagara', 'debug_shape', 'spawn_niagara', 'create_dynamic_light',
            'create_niagara_system', 'create_niagara_emitter',
            'create_volumetric_fog', 'create_particle_trail', 'create_environment_effect', 'create_impact_effect', 'create_niagara_ribbon',
            'activate', 'activate_effect', 'deactivate', 'reset', 'advance_simulation',
            'add_niagara_module', 'connect_niagara_pins', 'remove_niagara_node', 'set_niagara_parameter',
            'clear_debug_shapes', 'cleanup', 'list_debug_shapes',
            // Niagara authoring (merged from manage_niagara_authoring)
            'add_emitter_to_system', 'set_emitter_properties',
            'add_spawn_rate_module', 'add_spawn_burst_module', 'add_spawn_per_unit_module',
            'add_initialize_particle_module', 'add_particle_state_module',
            'add_force_module', 'add_velocity_module', 'add_acceleration_module',
            'add_size_module', 'add_color_module',
            'add_sprite_renderer_module', 'add_mesh_renderer_module', 'add_ribbon_renderer_module', 'add_light_renderer_module',
            'add_collision_module', 'add_kill_particles_module', 'add_camera_offset_module',
            'add_user_parameter', 'set_parameter_value', 'bind_parameter_to_source',
            'add_skeletal_mesh_data_interface', 'add_static_mesh_data_interface', 'add_spline_data_interface',
            'add_audio_spectrum_data_interface', 'add_collision_query_data_interface',
            'add_event_generator', 'add_event_receiver', 'configure_event_payload',
            'enable_gpu_simulation', 'add_simulation_stage',
            'get_niagara_info', 'validate_niagara_system',
            // Niagara Advanced (Phase 3E)
            'create_niagara_module', 'add_niagara_script', 'add_data_interface',
            'setup_niagara_fluids', 'create_fluid_simulation', 'add_chaos_integration',
            // Wave 4.1-4.10: Niagara Enhancement
            'create_niagara_sim_cache', 'configure_niagara_lod', 'export_niagara_system', 'import_niagara_module',
            'configure_niagara_determinism', 'create_niagara_data_interface', 'configure_gpu_simulation',
            'batch_compile_niagara', 'get_niagara_parameters', 'set_niagara_variable'
          ],
          description: 'Action'
        },
        name: commonSchemas.name,
        systemName: commonSchemas.stringProp,
        systemPath: commonSchemas.niagaraPath,
        preset: { type: 'string', description: 'Path to particle system asset.' },
        location: commonSchemas.location,
        scale: commonSchemas.numberProp,
        shape: { type: 'string', description: 'Supported: sphere, box, cylinder, line, cone, capsule, arrow, plane' },
        size: commonSchemas.numberProp,
        color: commonSchemas.color,
        modulePath: commonSchemas.assetPath,
        emitterName: commonSchemas.stringProp,
        pinName: commonSchemas.pinName,
        linkedTo: commonSchemas.stringProp,
        parameterName: commonSchemas.parameterName,
        parameterType: commonSchemas.stringProp,
        type: commonSchemas.stringProp,
        value: commonSchemas.value,
        filter: commonSchemas.filter,
        // Advanced properties
        fluidType: { type: 'string', enum: ['2D', '3D'], description: 'Fluid simulation type.' },
        stage: { type: 'string', description: 'Script usage stage (Spawn, Update, Event, Simulation).' },
        className: { type: 'string', description: 'Class name for data interfaces.' },
        // Authoring properties (merged from manage_niagara_authoring)
        path: commonSchemas.directoryPathForCreation,
        assetPath: commonSchemas.assetPath,
        emitterPath: commonSchemas.emitterPath,
        save: commonSchemas.save,
        emitterProperties: commonSchemas.objectProp,
        spawnRate: commonSchemas.numberProp,
        burstCount: commonSchemas.numberProp,
        burstTime: commonSchemas.numberProp,
        burstInterval: commonSchemas.numberProp,
        spawnPerUnit: commonSchemas.numberProp,
        lifetime: commonSchemas.numberProp,
        lifetimeMin: commonSchemas.numberProp,
        lifetimeMax: commonSchemas.numberProp,
        mass: commonSchemas.numberProp,
        spriteSize: commonSchemas.objectProp,
        meshScale: commonSchemas.objectProp,
        forceType: commonSchemas.stringProp,
        forceStrength: commonSchemas.numberProp,
        forceVector: commonSchemas.objectProp,
        dragCoefficient: commonSchemas.numberProp,
        velocity: commonSchemas.objectProp,
        velocityMin: commonSchemas.objectProp,
        velocityMax: commonSchemas.objectProp,
        acceleration: commonSchemas.objectProp,
        velocityMode: commonSchemas.stringProp,
        sizeMode: commonSchemas.stringProp,
        uniformSize: commonSchemas.numberProp,
        sizeScale: commonSchemas.objectProp,
        sizeCurve: commonSchemas.arrayOfObjects,
        colorMin: commonSchemas.objectProp,
        colorMax: commonSchemas.objectProp,
        colorMode: commonSchemas.stringProp,
        colorCurve: commonSchemas.arrayOfObjects,
        materialPath: commonSchemas.materialPath,
        meshPath: commonSchemas.meshPath,
        sortMode: commonSchemas.stringProp,
        alignment: commonSchemas.stringProp,
        facingMode: commonSchemas.stringProp,
        ribbonWidth: commonSchemas.numberProp,
        ribbonTwist: commonSchemas.numberProp,
        ribbonFacingMode: commonSchemas.stringProp,
        tessellationFactor: commonSchemas.numberProp,
        lightRadius: commonSchemas.numberProp,
        lightIntensity: commonSchemas.numberProp,
        lightColor: commonSchemas.objectProp,
        volumetricScattering: commonSchemas.numberProp,
        lightExponent: commonSchemas.numberProp,
        affectsTranslucency: commonSchemas.booleanProp,
        collisionMode: commonSchemas.stringProp,
        restitution: commonSchemas.numberProp,
        friction: commonSchemas.numberProp,
        radiusScale: commonSchemas.numberProp,
        dieOnCollision: commonSchemas.booleanProp,
        killCondition: commonSchemas.stringProp,
        killBox: commonSchemas.objectProp,
        invertKillZone: commonSchemas.booleanProp,
        cameraOffset: commonSchemas.numberProp,
        cameraOffsetMode: commonSchemas.stringProp,
        parameterValue: commonSchemas.value,
        sourceBinding: commonSchemas.stringProp,
        skeletalMeshPath: commonSchemas.skeletalMeshPath,
        staticMeshPath: commonSchemas.meshAssetPath,
        useWholeSkeletonOrBones: commonSchemas.stringProp,
        specificBones: commonSchemas.arrayOfStrings,
        samplingMode: commonSchemas.stringProp,
        eventName: commonSchemas.eventName,
        eventPayload: commonSchemas.arrayOfObjects,
        spawnOnEvent: commonSchemas.booleanProp,
        eventSpawnCount: commonSchemas.numberProp,
        gpuEnabled: commonSchemas.booleanProp,
        fixedBoundsEnabled: commonSchemas.booleanProp,
        deterministicEnabled: commonSchemas.booleanProp,
        stageName: commonSchemas.stageName,
        stageIterationSource: commonSchemas.stringProp
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
    name: 'build_environment',
    category: 'world',
    description: 'Landscapes, foliage, procedural terrain, sky/fog, water, weather.',
    annotations: {
      audience: ['developer', 'environment-artist'],
      priority: 8,
      tags: ['landscape', 'foliage', 'terrain', 'sky', 'fog', 'water', 'weather']
    },
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            // Landscape
            'create_landscape', 'sculpt', 'sculpt_landscape', 'add_foliage', 'paint_foliage',
            'create_procedural_terrain', 'create_procedural_foliage', 'add_foliage_instances',
            'get_foliage_instances', 'remove_foliage', 'paint_landscape', 'paint_landscape_layer',
            'modify_heightmap', 'set_landscape_material', 'create_landscape_grass_type',
            'generate_lods', 'bake_lightmap', 'export_snapshot', 'import_snapshot', 'delete',
            // Phase 28: Sky/Atmosphere
            'create_sky_sphere', 'create_sky_atmosphere', 'configure_sky_atmosphere',
            // Phase 28: Fog
            'create_fog_volume', 'create_exponential_height_fog', 'configure_exponential_height_fog',
            // Phase 28: Clouds
            'create_volumetric_cloud', 'configure_volumetric_cloud',
            // Time of Day
            'set_time_of_day',
            // Water (merged from manage_water - Phase 54)
            'create_water_body_ocean', 'create_water_body_lake', 'create_water_body_river',
            'configure_water_body', 'configure_water_waves', 'get_water_body_info', 'list_water_bodies',
            'set_river_depth', 'set_ocean_extent', 'set_water_static_mesh', 'set_river_transitions',
            'set_water_zone', 'get_water_surface_info', 'get_wave_info',
            // Weather (merged from manage_weather - Phase 54)
            'configure_wind', 'create_weather_system',
            'configure_rain_particles', 'configure_snow_particles', 'configure_lightning',
            // Wave 5: Environment Actions
            'configure_weather_preset', 'query_water_bodies', 'configure_ocean_waves',
            'create_landscape_spline', 'configure_foliage_density', 'batch_paint_foliage',
            'configure_sky_atmosphere', 'create_volumetric_cloud', 'configure_wind_directional',
            'get_terrain_height_at'
          ],
          description: 'Action'
        },
        name: commonSchemas.name,
        landscapeName: commonSchemas.stringProp,
        heightData: commonSchemas.arrayOfNumbers,
        minX: commonSchemas.numberProp,
        minY: commonSchemas.numberProp,
        maxX: commonSchemas.numberProp,
        maxY: commonSchemas.numberProp,
        updateNormals: commonSchemas.booleanProp,
        location: commonSchemas.location,
        rotation: commonSchemas.rotation,
        scale: commonSchemas.scale,
        sizeX: commonSchemas.numberProp,
        sizeY: commonSchemas.numberProp,
        sectionSize: commonSchemas.numberProp,
        sectionsPerComponent: commonSchemas.numberProp,
        componentCount: commonSchemas.vector2,
        materialPath: commonSchemas.materialPath,
        tool: commonSchemas.stringProp,
        radius: commonSchemas.numberProp,
        strength: commonSchemas.numberProp,
        falloff: commonSchemas.numberProp,
        brushSize: commonSchemas.numberProp,
        layerName: commonSchemas.stringProp,
        eraseMode: commonSchemas.booleanProp,
        foliageType: commonSchemas.stringProp,
        foliageTypePath: commonSchemas.assetPath,
        meshPath: commonSchemas.meshPath,
        density: commonSchemas.numberProp,
        minScale: commonSchemas.numberProp,
        maxScale: commonSchemas.numberProp,
        cullDistance: commonSchemas.numberProp,
        alignToNormal: commonSchemas.booleanProp,
        randomYaw: commonSchemas.booleanProp,
        locations: { type: 'array', items: commonSchemas.location },
        transforms: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              location: commonSchemas.location,
              rotation: commonSchemas.rotation,
              scale: commonSchemas.scale
            }
          }
        },
        position: commonSchemas.location,
        bounds: commonSchemas.objectProp,
        volumeName: commonSchemas.stringProp,
        seed: commonSchemas.numberProp,
        foliageTypes: commonSchemas.arrayOfObjects,
        path: commonSchemas.directoryPath,
        filename: commonSchemas.stringProp,
        assetPaths: commonSchemas.arrayOfStrings,
        // Phase 28: Sky Atmosphere properties
        bottomRadius: { type: 'number', description: 'Atmosphere bottom radius (km)' },
        atmosphereHeight: { type: 'number', description: 'Atmosphere height (km)' },
        mieAnisotropy: { type: 'number', description: 'Mie anisotropy (-1 to 1)' },
        mieScatteringScale: { type: 'number', description: 'Mie scattering scale' },
        rayleighScatteringScale: { type: 'number', description: 'Rayleigh scattering scale' },
        multiScatteringFactor: { type: 'number', description: 'Multi-scattering factor' },
        rayleighExponentialDistribution: { type: 'number', description: 'Rayleigh exponential dist' },
        mieExponentialDistribution: { type: 'number', description: 'Mie exponential dist' },
        mieAbsorptionScale: { type: 'number', description: 'Mie absorption scale' },
        otherAbsorptionScale: { type: 'number', description: 'Other absorption (ozone)' },
        heightFogContribution: { type: 'number', description: 'Height fog contribution' },
        aerialPerspectiveViewDistanceScale: { type: 'number', description: 'Aerial perspective dist scale' },
        transmittanceMinLightElevationAngle: { type: 'number', description: 'Min light elevation (transmittance)' },
        aerialPerspectiveStartDepth: { type: 'number', description: 'Aerial perspective start depth' },
        rayleighScattering: { 
          type: 'object', 
          description: 'Rayleigh color {r,g,b}',
          properties: { r: { type: 'number' }, g: { type: 'number' }, b: { type: 'number' } }
        },
        mieScattering: { 
          type: 'object', 
          description: 'Mie scattering {r,g,b}',
          properties: { r: { type: 'number' }, g: { type: 'number' }, b: { type: 'number' } }
        },
        mieAbsorption: { 
          type: 'object', 
          description: 'Mie absorption {r,g,b}',
          properties: { r: { type: 'number' }, g: { type: 'number' }, b: { type: 'number' } }
        },
        skyLuminanceFactor: { 
          type: 'object', 
          description: 'Sky luminance {r,g,b}',
          properties: { r: { type: 'number' }, g: { type: 'number' }, b: { type: 'number' } }
        },
        // Phase 28: Exponential Height Fog properties
        fogDensity: { type: 'number', description: 'Fog density (0-1)' },
        fogHeightFalloff: { type: 'number', description: 'Fog height falloff rate' },
        fogMaxOpacity: { type: 'number', description: 'Max fog opacity (0-1)' },
        startDistance: { type: 'number', description: 'Fog start distance' },
        endDistance: { type: 'number', description: 'Fog end distance' },
        fogCutoffDistance: { type: 'number', description: 'Fog cutoff distance' },
        volumetricFog: { type: 'boolean', description: 'Enable volumetric fog' },
        volumetricFogScatteringDistribution: { type: 'number', description: 'Volumetric fog phase' },
        volumetricFogExtinctionScale: { type: 'number', description: 'Volumetric fog extinction scale' },
        volumetricFogDistance: { type: 'number', description: 'Volumetric fog distance' },
        volumetricFogStartDistance: { type: 'number', description: 'Volumetric fog start' },
        volumetricFogNearFadeInDistance: { type: 'number', description: 'Volumetric fog near fade-in' },
        fogInscatteringColor: { 
          type: 'object', 
          description: 'Fog inscattering {r,g,b}',
          properties: { r: { type: 'number' }, g: { type: 'number' }, b: { type: 'number' } }
        },
        directionalInscatteringColor: { 
          type: 'object', 
          description: 'Directional inscattering {r,g,b}',
          properties: { r: { type: 'number' }, g: { type: 'number' }, b: { type: 'number' } }
        },
        volumetricFogAlbedo: { 
          type: 'object', 
          description: 'Volumetric fog albedo {r,g,b}',
          properties: { r: { type: 'number' }, g: { type: 'number' }, b: { type: 'number' } }
        },
        volumetricFogEmissive: { 
          type: 'object', 
          description: 'Volumetric fog emissive {r,g,b}',
          properties: { r: { type: 'number' }, g: { type: 'number' }, b: { type: 'number' } }
        },
        directionalInscatteringExponent: { type: 'number', description: 'Directional inscattering exp' },
        directionalInscatteringStartDistance: { type: 'number', description: 'Directional inscattering start' },
        secondFogDensity: { type: 'number', description: '2nd fog layer density' },
        secondFogHeightFalloff: { type: 'number', description: '2nd fog height falloff' },
        secondFogHeightOffset: { type: 'number', description: '2nd fog height offset' },
        inscatteringColorCubemapAngle: { type: 'number', description: 'Inscattering cubemap angle' },
        fullyDirectionalInscatteringColorDistance: { type: 'number', description: 'Full directional inscatter dist' },
        nonDirectionalInscatteringColorDistance: { type: 'number', description: 'Non-directional inscatter dist' },
        inscatteringTextureTint: { 
          type: 'object', 
          description: 'Inscattering texture tint {r,g,b}',
          properties: { r: { type: 'number' }, g: { type: 'number' }, b: { type: 'number' } }
        },
        // Phase 28: Volumetric Cloud properties
        layerBottomAltitude: { type: 'number', description: 'Cloud layer bottom (km)' },
        layerHeight: { type: 'number', description: 'Cloud layer height (km)' },
        tracingStartMaxDistance: { type: 'number', description: 'Cloud tracing max start dist' },
        tracingStartDistanceFromCamera: { type: 'number', description: 'Cloud tracing camera dist' },
        tracingMaxDistance: { type: 'number', description: 'Cloud max ray distance' },
        planetRadius: { type: 'number', description: 'Planet radius (km)' },
        groundAlbedo: { 
          type: 'object', 
          description: 'Ground albedo {r,g,b}',
          properties: { r: { type: 'number' }, g: { type: 'number' }, b: { type: 'number' } }
        },
        usePerSampleAtmosphericLightTransmittance: { type: 'boolean', description: 'Per-sample light transmittance' },
        skyLightCloudBottomOcclusion: { type: 'number', description: 'Sky light cloud occlusion (0-1)' },
        viewSampleCountScale: { type: 'number', description: 'View sample count scale' },
        reflectionViewSampleCountScale: { type: 'number', description: 'Reflection sample count scale' },
        shadowViewSampleCountScale: { type: 'number', description: 'Sample count scale for shadow view' },
        shadowReflectionViewSampleCountScale: { type: 'number', description: 'Sample count scale for shadow reflections' },
        shadowTracingDistance: { type: 'number', description: 'Shadow tracing distance in km' },
        stopTracingTransmittanceThreshold: { type: 'number', description: 'Transmittance threshold to stop tracing (0.0 - 1.0)' },
        holdout: { type: 'boolean', description: 'Render as holdout (black with alpha 0)' },
        renderInMainPass: { type: 'boolean', description: 'Render in main pass' },
        visibleInRealTimeSkyCaptures: { type: 'boolean', description: 'Visible in real-time sky captures' },
        // Time of Day
        time: { type: 'number', description: 'Time of day (0.0 - 24.0)' },
        hour: { type: 'number', description: 'Hour of day (0.0 - 24.0)' }
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
    name: 'manage_sequence',
    category: 'utility',
    description: 'Sequencer cinematics, Level Sequences, keyframes, MRQ renders.',
    annotations: {
      audience: ['developer', 'cinematographer'],
      priority: 7,
      tags: ['sequencer', 'cinematic', 'mrq', 'keyframe', 'camera']
    },
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            // Core sequence management
            'create', 'open', 'duplicate', 'rename', 'delete', 'list', 'get_metadata', 'set_metadata',
            'create_master_sequence', 'add_subsequence', 'remove_subsequence', 'get_subsequences', 'export_sequence',
            // Actor binding
            'add_actor', 'add_actors', 'remove_actors', 'bind_actor', 'unbind_actor', 'get_bindings',
            'add_spawnable_from_class',
            // Tracks & sections
            'add_track', 'remove_track', 'list_tracks', 'list_track_types', 'add_section', 'remove_section', 'get_tracks',
            'set_track_muted', 'set_track_solo', 'set_track_locked',
            // Shot tracks
            'add_shot_track', 'add_shot', 'remove_shot', 'get_shots',
            // Camera
            'add_camera', 'create_cine_camera_actor', 'configure_camera_settings', 'add_camera_cut_track', 'add_camera_cut',
            // Keyframes
            'add_keyframe', 'remove_keyframe', 'get_keyframes',
            // Properties & timing
            'get_properties', 'set_properties', 'set_display_rate', 'set_tick_resolution',
            'set_work_range', 'set_view_range', 'set_playback_range', 'get_playback_range', 'get_sequence_info',
            // Playback control
            'play', 'pause', 'stop', 'set_playback_speed', 'play_sequence', 'pause_sequence', 'stop_sequence', 'scrub_to_time',
            // Movie Render Queue
            'create_queue', 'add_job', 'remove_job', 'clear_queue', 'get_queue', 'configure_job', 'set_sequence', 'set_map',
            'configure_output', 'set_resolution', 'set_frame_rate', 'set_output_directory', 'set_file_name_format',
            'add_render_pass', 'remove_render_pass', 'get_render_passes', 'configure_render_pass',
            'configure_anti_aliasing', 'set_spatial_sample_count', 'set_temporal_sample_count',
            'add_burn_in', 'remove_burn_in', 'configure_burn_in',
            'start_render', 'stop_render', 'get_render_status', 'get_render_progress',
'add_console_variable', 'remove_console_variable', 'configure_high_res_settings', 'set_tile_count',
            // Wave 4.11-4.20: Sequencer Enhancement Actions
            'create_media_track', 'configure_sequence_streaming', 'create_event_trigger_track',
            'add_procedural_camera_shake', 'configure_sequence_lod', 'create_camera_cut_track',
            'configure_mrq_settings', 'batch_render_sequences', 'get_sequence_bindings', 'configure_audio_track'
          ],
          description: 'Sequencer/MRQ action'
        },
        // Sequence identification
        name: commonSchemas.name,
        path: commonSchemas.assetPath,
        sequencePath: commonSchemas.sequencePath,
        sequenceName: commonSchemas.name,
        savePath: commonSchemas.savePath,
        assetPath: commonSchemas.assetPath,
        destinationPath: commonSchemas.destinationPath,
        newName: commonSchemas.newName,
        // Actor binding
        actorName: commonSchemas.actorName,
        actorNames: commonSchemas.arrayOfStrings,
        bindingName: { type: 'string', description: 'Binding display name.' },
        bindingId: { type: 'string', description: 'Binding GUID.' },
        spawnable: commonSchemas.booleanProp,
        className: commonSchemas.stringProp,
        // Tracks
        trackType: { type: 'string', enum: ['Transform', 'Animation', 'Audio', 'Event', 'Property', 'Fade', 'LevelVisibility', 'CameraCut', 'Skeletal', 'Material'], description: 'Track type.' },
        trackName: commonSchemas.stringProp,
        propertyPath: { type: 'string', description: 'Property path for tracks.' },
        muted: commonSchemas.booleanProp,
        solo: commonSchemas.booleanProp,
        locked: commonSchemas.booleanProp,
        // Shots & subsequences
        subsequencePath: { type: 'string', description: 'Subsequence path.' },
        shotName: { type: 'string', description: 'Shot name.' },
        shotNumber: { type: 'number', description: 'Shot ordering number.' },
        // Camera settings
        cameraActorName: commonSchemas.actorName,
        filmbackPreset: { type: 'string', enum: ['16:9_DSLR', '16:9_Film', '35mm_Academy', '35mm_VistaVision', '65mm_IMAX', 'Super_35', 'Custom'], description: 'Filmback preset.' },
        sensorWidth: { type: 'number', description: 'Sensor width mm.' },
        sensorHeight: { type: 'number', description: 'Sensor height mm.' },
        focalLength: { type: 'number', description: 'Focal length mm.' },
        aperture: { type: 'number', description: 'Aperture f-stop.' },
        focusDistance: { type: 'number', description: 'Focus distance.' },
        autoFocus: { type: 'boolean', description: 'Enable auto focus.' },
        focusMethod: { type: 'string', enum: ['DoNotOverride', 'Manual', 'Tracking'], description: 'Focus method.' },
        focusTarget: { type: 'string', description: 'Focus target actor.' },
        // Keyframes & timing
        frame: commonSchemas.numberProp,
        time: { type: 'number', description: 'Time in seconds.' },
        value: commonSchemas.value,
        property: commonSchemas.propertyName,
        interpolation: { type: 'string', enum: ['Auto', 'User', 'Break', 'Linear', 'Constant'], description: 'Interpolation mode.' },
        startFrame: commonSchemas.numberProp,
        endFrame: commonSchemas.numberProp,
        startTime: commonSchemas.numberProp,
        endTime: commonSchemas.numberProp,
        start: commonSchemas.numberProp,
        end: commonSchemas.numberProp,
        displayRate: { type: 'number', description: 'Display rate FPS.' },
        tickResolution: { type: 'number', description: 'Tick resolution.' },
        lengthInFrames: commonSchemas.numberProp,
        playbackStart: commonSchemas.numberProp,
        playbackEnd: commonSchemas.numberProp,
        // Playback
        speed: commonSchemas.numberProp,
        loopMode: commonSchemas.stringProp,
        // Movie Render Queue
        jobName: { type: 'string', description: 'Render job name.' },
        jobIndex: { type: 'number', description: 'Job index.' },
        mapPath: { type: 'string', description: 'Map path to render.' },
        outputDirectory: { type: 'string', description: 'Output directory.' },
        fileNameFormat: { type: 'string', description: 'Filename format with tokens.' },
        outputFormat: { type: 'string', enum: ['PNG', 'JPG', 'EXR', 'BMP', 'ProRes', 'AVI'], description: 'Output format.' },
        resolutionX: { type: 'number', description: 'Resolution width.' },
        resolutionY: { type: 'number', description: 'Resolution height.' },
        frameRate: commonSchemas.numberProp,
        resolution: commonSchemas.stringProp,
        passType: { type: 'string', enum: ['FinalImage', 'ObjectId', 'MaterialId', 'Depth', 'WorldNormal', 'BaseColor', 'Roughness', 'Metallic', 'AmbientOcclusion', 'Cryptomatte'], description: 'Render pass type.' },
        passName: { type: 'string', description: 'Render pass name.' },
        passEnabled: { type: 'boolean', description: 'Enable pass.' },
        spatialSampleCount: { type: 'number', description: 'Spatial AA samples.' },
        temporalSampleCount: { type: 'number', description: 'Temporal samples.' },
        overrideAntiAliasing: { type: 'boolean', description: 'Override AA.' },
        antiAliasingMethod: { type: 'string', enum: ['None', 'FXAA', 'TAA', 'MSAA'], description: 'AA method.' },
        burnInClass: { type: 'string', description: 'Burn-in widget class.' },
        burnInText: { type: 'string', description: 'Burn-in text.' },
        burnInPosition: { type: 'string', enum: ['TopLeft', 'TopCenter', 'TopRight', 'BottomLeft', 'BottomCenter', 'BottomRight'], description: 'Burn-in position.' },
        cvarName: { type: 'string', description: 'Console var name.' },
        cvarValue: { type: 'string', description: 'Console var value.' },
        tileCountX: { type: 'number', description: 'High-res tile count X.' },
        tileCountY: { type: 'number', description: 'High-res tile count Y.' },
        overlapRatio: { type: 'number', description: 'Tile overlap ratio.' },
        // Common
        location: commonSchemas.location,
        rotation: commonSchemas.rotation,
        overwrite: commonSchemas.overwrite,
        metadata: commonSchemas.objectProp,
exportPath: commonSchemas.exportPath,
        exportFormat: { type: 'string', enum: ['FBX', 'USD'], description: 'Export format.' },
        save: commonSchemas.save,
        // Wave 4.11-4.20: Sequencer Enhancement properties
        mediaPath: { type: 'string', description: 'Path to media file (video/image sequence).' },
        mediaSourceName: { type: 'string', description: 'Name for the media source.' },
        streamingSettings: { type: 'object', description: 'Streaming configuration for sequence playback.' },
        eventTriggerName: { type: 'string', description: 'Name for event trigger track.' },
        eventPayload: { type: 'object', description: 'Payload data for event trigger.' },
        shakeIntensity: { type: 'number', description: 'Camera shake intensity (0-1).' },
        shakeFrequency: { type: 'number', description: 'Camera shake frequency.' },
        shakeBlendIn: { type: 'number', description: 'Camera shake blend in time (seconds).' },
        shakeBlendOut: { type: 'number', description: 'Camera shake blend out time (seconds).' },
        shakeDuration: { type: 'number', description: 'Camera shake duration (seconds).' },
        lodBias: { type: 'number', description: 'LOD bias for sequence.' },
        forceLod: { type: 'number', description: 'Force specific LOD level (0-based).' },
        cameraCutName: { type: 'string', description: 'Name for camera cut.' },
        targetCamera: { type: 'string', description: 'Target camera actor for cut.' },
        blendTime: { type: 'number', description: 'Blend time between camera cuts.' },
        mrqPreset: { type: 'string', description: 'MRQ preset name to apply.' },
        mrqSettings: { type: 'object', description: 'MRQ configuration settings object.' },
        sequencePaths: { type: 'array', items: { type: 'string' }, description: 'Array of sequence paths for batch operations.' },
        outputSettings: { type: 'object', description: 'Output settings for batch render.' },
        parallelRenders: { type: 'number', description: 'Number of parallel render jobs.' },
        audioTrackName: { type: 'string', description: 'Name for audio track.' },
        audioAssetPath: { type: 'string', description: 'Path to audio asset for track.' },
        audioStartOffset: { type: 'number', description: 'Audio start offset in seconds.' },
        audioVolumeMultiplier: { type: 'number', description: 'Volume multiplier for audio track (0-1).' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        ...commonSchemas.outputBase,
        sequencePath: commonSchemas.sequencePath,
        bindingId: commonSchemas.stringProp,
        trackId: commonSchemas.stringProp,
        jobName: commonSchemas.stringProp,
        queueSize: commonSchemas.numberProp
      }
    }
  },
  // [MERGED] manage_input actions now in control_editor (Phase 54: Strategic Tool Merging)
  // [MERGED] inspect actions now in control_actor
  {
    name: 'manage_audio',
    category: 'utility',
    description: 'Audio playback, mixes, MetaSounds + Wwise/FMOD/Bink middleware.',
    annotations: {
      audience: ['developer', 'audio-designer'],
      priority: 7,
      tags: ['audio', 'sound', 'metasound', 'wwise', 'fmod', 'middleware']
    },
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            // Runtime audio control
            'create_sound_cue', 'play_sound_at_location', 'play_sound_2d', 'create_audio_component',
            'create_sound_mix', 'push_sound_mix', 'pop_sound_mix',
            'set_sound_mix_class_override', 'clear_sound_mix_class_override', 'set_base_sound_mix',
            'prime_sound', 'play_sound_attached', 'spawn_sound_at_location',
            'fade_sound_in', 'fade_sound_out', 'create_ambient_sound',
            'create_sound_class', 'set_sound_attenuation', 'create_reverb_zone',
            'enable_audio_analysis', 'fade_sound', 'set_doppler_effect', 'set_audio_occlusion',
            // Sound Cue authoring (merged from manage_audio_authoring)
            'add_cue_node', 'connect_cue_nodes', 'set_cue_attenuation', 'set_cue_concurrency',
            // MetaSound authoring
            'create_metasound', 'add_metasound_node', 'connect_metasound_nodes',
            'add_metasound_input', 'add_metasound_output', 'set_metasound_default',
            // Sound class/mix authoring
            'set_class_properties', 'set_class_parent', 'add_mix_modifier', 'configure_mix_eq',
            // Attenuation authoring
            'create_attenuation_settings', 'configure_distance_attenuation',
            'configure_spatialization', 'configure_occlusion', 'configure_reverb_send',
            // Dialogue system
            'create_dialogue_voice', 'create_dialogue_wave', 'set_dialogue_context',
            // Effects
            'create_reverb_effect', 'create_source_effect_chain', 'add_source_effect', 'create_submix_effect',
            // Utility
            'get_audio_info',
            // Audio Middleware - Wwise (merged from manage_audio_middleware with mw_ prefix)
            'mw_connect_wwise_project', 'mw_post_wwise_event', 'mw_post_wwise_event_at_location',
            'mw_stop_wwise_event', 'mw_set_rtpc_value', 'mw_set_rtpc_value_on_actor', 'mw_get_rtpc_value',
            'mw_set_wwise_switch', 'mw_set_wwise_switch_on_actor', 'mw_set_wwise_state',
            'mw_load_wwise_bank', 'mw_unload_wwise_bank', 'mw_get_loaded_banks',
            'mw_create_wwise_component', 'mw_configure_wwise_component', 'mw_configure_spatial_audio',
            'mw_configure_room', 'mw_configure_portal', 'mw_set_listener_position',
            'mw_get_wwise_event_duration', 'mw_create_wwise_trigger', 'mw_set_wwise_game_object',
            'mw_unset_wwise_game_object', 'mw_post_wwise_trigger', 'mw_set_aux_send',
            'mw_configure_wwise_occlusion', 'mw_set_wwise_project_path', 'mw_get_wwise_status',
            'mw_configure_wwise_init', 'mw_restart_wwise_engine',
            // Audio Middleware - FMOD (merged from manage_audio_middleware with mw_ prefix)
            'mw_connect_fmod_project', 'mw_play_fmod_event', 'mw_play_fmod_event_at_location',
            'mw_stop_fmod_event', 'mw_set_fmod_parameter', 'mw_set_fmod_global_parameter',
            'mw_get_fmod_parameter', 'mw_load_fmod_bank', 'mw_unload_fmod_bank', 'mw_get_fmod_loaded_banks',
            'mw_create_fmod_component', 'mw_configure_fmod_component', 'mw_set_fmod_bus_volume',
            'mw_set_fmod_bus_paused', 'mw_set_fmod_bus_mute', 'mw_set_fmod_vca_volume',
            'mw_apply_fmod_snapshot', 'mw_release_fmod_snapshot', 'mw_set_fmod_listener_attributes',
            'mw_get_fmod_event_info', 'mw_configure_fmod_occlusion', 'mw_configure_fmod_attenuation',
            'mw_set_fmod_studio_path', 'mw_get_fmod_status', 'mw_configure_fmod_init',
            'mw_restart_fmod_engine', 'mw_set_fmod_3d_attributes', 'mw_get_fmod_memory_usage',
            'mw_pause_all_fmod_events', 'mw_resume_all_fmod_events',
            // Audio Middleware - Bink Video (merged from manage_audio_middleware with mw_ prefix)
            'mw_create_bink_media_player', 'mw_open_bink_video', 'mw_play_bink', 'mw_pause_bink',
            'mw_stop_bink', 'mw_seek_bink', 'mw_set_bink_looping', 'mw_set_bink_rate',
            'mw_set_bink_volume', 'mw_get_bink_duration', 'mw_get_bink_time', 'mw_get_bink_status',
            'mw_create_bink_texture', 'mw_configure_bink_texture', 'mw_set_bink_texture_player',
            'mw_draw_bink_to_texture', 'mw_configure_bink_buffer_mode', 'mw_configure_bink_sound_track',
            'mw_configure_bink_draw_style', 'mw_get_bink_dimensions',
// Audio Middleware - Utility
            'mw_get_audio_middleware_info',
            // MetaSounds Queries (A6)
            'list_metasound_assets', 'get_metasound_inputs', 'trigger_metasound'
          ],
          description: 'Action'
        },
        name: commonSchemas.name,
        soundPath: commonSchemas.soundPath,
        location: commonSchemas.location,
        rotation: commonSchemas.rotation,
        volume: commonSchemas.numberProp,
        pitch: commonSchemas.numberProp,
        startTime: commonSchemas.numberProp,
        attenuationPath: commonSchemas.assetPath,
        concurrencyPath: commonSchemas.assetPath,
        mixName: commonSchemas.stringProp,
        soundClassName: commonSchemas.stringProp,
        fadeInTime: commonSchemas.numberProp,
        fadeOutTime: commonSchemas.numberProp,
        fadeTime: commonSchemas.numberProp,
        targetVolume: commonSchemas.numberProp,
        attachPointName: commonSchemas.socketName,
        actorName: commonSchemas.actorName,
        componentName: commonSchemas.componentName,
        parentClass: commonSchemas.stringProp,
        properties: commonSchemas.objectProp,
        innerRadius: commonSchemas.numberProp,
        falloffDistance: commonSchemas.numberProp,
        attenuationShape: commonSchemas.stringProp,
        falloffMode: commonSchemas.stringProp,
        reverbEffect: commonSchemas.stringProp,
        size: commonSchemas.scale,
        fftSize: commonSchemas.numberProp,
        outputType: commonSchemas.stringProp,
        soundName: commonSchemas.stringProp,
        fadeType: commonSchemas.stringProp,
        scale: commonSchemas.numberProp,
        lowPassFilterFrequency: commonSchemas.numberProp,
        volumeAttenuation: commonSchemas.numberProp,
        enabled: commonSchemas.enabled,
        // Authoring properties (merged from manage_audio_authoring)
        path: commonSchemas.directoryPathForCreation,
        assetPath: commonSchemas.assetPath,
        save: commonSchemas.save,
        wavePath: commonSchemas.wavePath,
        nodeType: commonSchemas.stringProp,
        nodeId: commonSchemas.nodeId,
        sourceNodeId: commonSchemas.sourceNodeId,
        targetNodeId: commonSchemas.targetNodeId,
        outputPin: commonSchemas.numberProp,
        inputPin: commonSchemas.numberProp,
        looping: commonSchemas.looping,
        x: commonSchemas.numberProp,
        y: commonSchemas.numberProp,
        metasoundType: commonSchemas.stringProp,
        inputName: commonSchemas.inputName,
        inputType: commonSchemas.stringProp,
        outputName: commonSchemas.outputName,
        sourceNode: commonSchemas.sourceNode,
        sourcePin: commonSchemas.sourcePin,
        targetNode: commonSchemas.targetNode,
        targetPin: commonSchemas.targetPin,
        defaultValue: commonSchemas.value,
        metasoundNodeType: commonSchemas.stringProp,
        soundClassPath: commonSchemas.soundClassPath,
        parentClassPath: commonSchemas.parentClassPath
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        ...commonSchemas.outputBase,
        assetPath: commonSchemas.assetPath,
        nodeId: commonSchemas.nodeId
      }
    }
  },
  // [MERGED] manage_behavior_tree actions now in manage_ai (Phase 54: Strategic Tool Merging)
  // [MERGED] manage_blueprint_graph actions now in manage_blueprint (Phase 53: Strategic Tool Merging)
  {
    name: 'manage_lighting',
    category: 'world',
    description: 'Lights (point, spot, rect, sky), GI, shadows, volumetric fog.',
    annotations: {
      audience: ['developer', 'lighting-artist'],
      priority: 8,
      tags: ['lighting', 'gi', 'shadows', 'lumen', 'post-process', 'fog']
    },
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'spawn_light', 'create_light', 'spawn_sky_light', 'create_sky_light', 'ensure_single_sky_light',
            'create_lightmass_volume', 'create_lighting_enabled_level', 'create_dynamic_light',
            'setup_global_illumination', 'configure_shadows', 'set_exposure', 'set_ambient_occlusion', 'setup_volumetric_fog',
            'build_lighting', 'list_light_types',
            // Post-process (merged from manage_post_process)
            'create_post_process_volume', 'configure_pp_blend', 'configure_pp_priority', 'get_post_process_settings',
            'configure_bloom', 'configure_dof', 'configure_motion_blur',
            'configure_color_grading', 'configure_white_balance', 'configure_vignette',
            'configure_chromatic_aberration', 'configure_film_grain', 'configure_lens_flares',
            'create_sphere_reflection_capture', 'create_box_reflection_capture', 'create_planar_reflection', 'recapture_scene',
            'configure_ray_traced_shadows', 'configure_ray_traced_gi', 'configure_ray_traced_reflections',
            'configure_ray_traced_ao', 'configure_path_tracing',
            'create_scene_capture_2d', 'create_scene_capture_cube', 'capture_scene',
            'set_light_channel', 'set_actor_light_channel',
            'configure_lightmass_settings', 'build_lighting_quality', 'configure_indirect_lighting_cache', 'configure_volumetric_lightmap',
            // Lumen (Phase 3G.2)
            'configure_lumen_gi', 'set_lumen_reflections', 'tune_lumen_performance',
            'create_lumen_volume', 'set_virtual_shadow_maps',
            // Wave 5.11-5.20: MegaLights & Advanced Lighting
            'configure_megalights_scene', 'get_megalights_budget', 'optimize_lights_for_megalights',
            'configure_gi_settings', 'bake_lighting_preview', 'get_light_complexity',
            'configure_volumetric_fog', 'create_light_batch', 'configure_shadow_settings', 'validate_lighting_setup'
          ],
          description: 'Action'
        },
        name: commonSchemas.name,
        location: commonSchemas.location,
        rotation: commonSchemas.rotation,
        lightType: { type: 'string', enum: ['Directional', 'Point', 'Spot', 'Rect'] },
        intensity: commonSchemas.numberProp,
        color: commonSchemas.color,
        castShadows: commonSchemas.booleanProp,
        useAsAtmosphereSunLight: { type: 'boolean', description: 'For Directional Lights, use as Atmosphere Sun Light.' },
        temperature: commonSchemas.numberProp,
        radius: commonSchemas.numberProp,
        falloffExponent: commonSchemas.numberProp,
        innerCone: commonSchemas.numberProp,
        outerCone: commonSchemas.numberProp,
        width: commonSchemas.numberProp,
        height: commonSchemas.numberProp,
        sourceType: { type: 'string', enum: ['CapturedScene', 'SpecifiedCubemap'] },
        cubemapPath: commonSchemas.texturePath,
        recapture: commonSchemas.booleanProp,
        method: { type: 'string', enum: ['Lightmass', 'LumenGI', 'ScreenSpace', 'None'] },
        quality: commonSchemas.stringProp,
        indirectLightingIntensity: commonSchemas.numberProp,
        bounces: commonSchemas.numberProp,
        shadowQuality: commonSchemas.stringProp,
        cascadedShadows: commonSchemas.booleanProp,
        shadowDistance: commonSchemas.numberProp,
        contactShadows: commonSchemas.booleanProp,
        rayTracedShadows: commonSchemas.booleanProp,
        compensationValue: commonSchemas.numberProp,
        minBrightness: commonSchemas.numberProp,
        maxBrightness: commonSchemas.numberProp,
        enabled: commonSchemas.enabled,
        density: commonSchemas.numberProp,
        scatteringIntensity: commonSchemas.numberProp,
        fogHeight: commonSchemas.numberProp,
        buildOnlySelected: commonSchemas.booleanProp,
        buildReflectionCaptures: commonSchemas.booleanProp,
        levelName: commonSchemas.stringProp,
        copyActors: commonSchemas.booleanProp,
        useTemplate: commonSchemas.booleanProp,
        size: commonSchemas.scale,
        // Post-process (merged from manage_post_process)
        volumeName: commonSchemas.volumeName,
        actorName: commonSchemas.actorName,
        infinite: commonSchemas.booleanProp,
        blendRadius: commonSchemas.numberProp,
        blendWeight: commonSchemas.numberProp,
        priority: commonSchemas.numberProp,
        extent: commonSchemas.extent,
        bloomIntensity: commonSchemas.numberProp,
        bloomThreshold: commonSchemas.numberProp,
        focalDistance: commonSchemas.numberProp,
        focalRegion: commonSchemas.numberProp,
        motionBlurAmount: commonSchemas.numberProp,
        globalSaturation: commonSchemas.colorObject,
        whiteTemp: commonSchemas.numberProp,
        vignetteIntensity: commonSchemas.numberProp,
        chromaticAberrationIntensity: commonSchemas.numberProp,
        filmGrainIntensity: commonSchemas.numberProp,
        influenceRadius: commonSchemas.numberProp,
        boxExtent: commonSchemas.extent,
        captureOffset: commonSchemas.location,
        brightness: commonSchemas.numberProp,
        screenPercentage: commonSchemas.numberProp,
        rayTracedShadowsEnabled: commonSchemas.booleanProp,
        rayTracedGIEnabled: commonSchemas.booleanProp,
        rayTracedReflectionsEnabled: commonSchemas.booleanProp,
        rayTracedAOEnabled: commonSchemas.booleanProp,
        pathTracingEnabled: commonSchemas.booleanProp,
        fov: commonSchemas.numberProp,
        captureResolution: commonSchemas.numberProp,
        captureSource: commonSchemas.stringProp,
        textureTargetPath: commonSchemas.stringProp,
        savePath: commonSchemas.savePath,
        lightActorName: commonSchemas.stringProp,
        channel0: commonSchemas.booleanProp,
        channel1: commonSchemas.booleanProp,
        channel2: commonSchemas.booleanProp,
        numIndirectBounces: commonSchemas.numberProp,
        indirectLightingQuality: commonSchemas.numberProp,
        volumetricLightmapEnabled: commonSchemas.booleanProp,
        // Lumen properties
        lumenQuality: { type: 'number', description: 'Lumen GI quality.' },
        lumenDetailTrace: { type: 'boolean', description: 'Lumen detail trace.' },
        lumenReflectionQuality: { type: 'number', description: 'Lumen reflection quality.' },
        lumenUpdateSpeed: { type: 'number', description: 'Lumen update speed.' },
        lumenFinalGatherQuality: { type: 'number', description: 'Lumen final gather quality.' },
        virtualShadowMapResolution: { type: 'number', description: 'Virtual shadow map resolution.' },
        virtualShadowMapQuality: { type: 'number', description: 'Virtual shadow map quality.' },
        // Wave 5.11-5.20: MegaLights & Advanced Lighting properties
        megalightsEnabled: { type: 'boolean', description: 'Enable MegaLights for scene (5.7+).' },
        megalightsBudget: { type: 'number', description: 'MegaLights budget (max lights).' },
        megalightsQuality: { type: 'string', description: 'MegaLights quality preset.' },
        giMethod: { type: 'string', enum: ['lumen', 'screenspace', 'raytraced', 'baked', 'none'], description: 'Global illumination method.' },
        lightQuality: { type: 'string', description: 'Light quality preset for baking.' },
        previewBake: { type: 'boolean', description: 'Quick preview bake instead of full build.' },
        fogInscatteringColor: commonSchemas.color,
        fogExtinctionScale: commonSchemas.numberProp,
        fogViewDistance: commonSchemas.numberProp,
        fogStartDistance: commonSchemas.numberProp,
        lights: { type: 'array', items: { type: 'object' }, description: 'Array of light definitions for batch creation.' },
        shadowBias: commonSchemas.numberProp,
        shadowSlopeBias: commonSchemas.numberProp,
        shadowResolution: commonSchemas.numberProp,
        dynamicShadowCascades: commonSchemas.numberProp,
        insetShadows: commonSchemas.booleanProp,
        validatePerformance: { type: 'boolean', description: 'Validate lighting performance.' },
        validateOverlap: { type: 'boolean', description: 'Check for overlapping lights.' },
        validateShadows: { type: 'boolean', description: 'Validate shadow casting settings.' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        ...commonSchemas.outputBase,
        actorName: commonSchemas.actorName,
        volumeName: commonSchemas.stringProp,
        capturePath: commonSchemas.stringProp
      }
    }
  },
  {
    name: 'manage_performance',
    category: 'utility',
    description: 'Profiling, benchmarks, scalability, LOD, Nanite optimization.',
    annotations: {
      audience: ['developer'],
      priority: 6,
      tags: ['profiling', 'performance', 'lod', 'nanite', 'optimization']
    },
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'start_profiling', 'stop_profiling', 'run_benchmark', 'show_fps', 'show_stats', 'generate_memory_report',
            'set_scalability', 'set_resolution_scale', 'set_vsync', 'set_frame_rate_limit', 'enable_gpu_timing',
            'configure_texture_streaming', 'configure_lod', 'apply_baseline_settings', 'optimize_draw_calls', 'merge_actors',
            'configure_occlusion_culling', 'optimize_shaders', 'configure_nanite', 'configure_world_partition'
          ],
          description: 'Action'
        },
        type: { type: 'string', enum: ['CPU', 'GPU', 'Memory', 'RenderThread', 'GameThread', 'All'] },
        duration: commonSchemas.numberProp,
        outputPath: commonSchemas.outputPath,
        detailed: commonSchemas.booleanProp,
        category: commonSchemas.stringProp,
        level: commonSchemas.numberProp,
        scale: commonSchemas.numberProp,
        enabled: commonSchemas.enabled,
        maxFPS: commonSchemas.numberProp,
        verbose: commonSchemas.booleanProp,
        poolSize: commonSchemas.numberProp,
        boostPlayerLocation: commonSchemas.booleanProp,
        forceLOD: commonSchemas.numberProp,
        lodBias: commonSchemas.numberProp,
        distanceScale: commonSchemas.numberProp,
        skeletalBias: commonSchemas.numberProp,
        hzb: commonSchemas.booleanProp,
        enableInstancing: commonSchemas.booleanProp,
        enableBatching: commonSchemas.booleanProp,
        mergeActors: commonSchemas.booleanProp,
        actors: commonSchemas.arrayOfStrings,
        freezeRendering: commonSchemas.booleanProp,
        compileOnDemand: commonSchemas.booleanProp,
        cacheShaders: commonSchemas.booleanProp,
        reducePermutations: commonSchemas.booleanProp,
        maxPixelsPerEdge: commonSchemas.numberProp,
        streamingPoolSize: commonSchemas.numberProp,
        streamingDistance: commonSchemas.numberProp,
        cellSize: commonSchemas.numberProp
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        ...commonSchemas.outputBase,
        params: commonSchemas.objectProp
      }
    }
  },
  {
    name: 'manage_geometry',
    category: 'world',
    description: 'Procedural meshes via Geometry Script: booleans, UVs, collision.',
    annotations: {
      audience: ['developer', 'tech-artist'],
      priority: 6,
      tags: ['geometry', 'procedural', 'mesh', 'boolean', 'uv']
    },
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'create_box', 'create_sphere', 'create_cylinder', 'create_cone', 'create_capsule',
            'create_torus', 'create_plane', 'create_disc',
            'create_stairs', 'create_spiral_stairs', 'create_ring',
            'create_arch', 'create_pipe', 'create_ramp',
            'boolean_union', 'boolean_subtract', 'boolean_intersection',
            'boolean_trim', 'self_union',
            'extrude', 'inset', 'outset', 'bevel', 'offset_faces', 'shell', 'revolve', 'chamfer',
            'extrude_along_spline', 'bridge', 'loft', 'sweep',
            'duplicate_along_spline', 'loop_cut', 'edge_split', 'quadrangulate',
            'bend', 'twist', 'taper', 'noise_deform', 'smooth', 'relax',
            'stretch', 'spherify', 'cylindrify',
            'triangulate', 'poke',
            'mirror', 'array_linear', 'array_radial',
            'simplify_mesh', 'subdivide', 'remesh_uniform', 'merge_vertices', 'remesh_voxel',
            'weld_vertices', 'fill_holes', 'remove_degenerates',
            'auto_uv', 'project_uv', 'transform_uvs', 'unwrap_uv', 'pack_uv_islands',
            'recalculate_normals', 'flip_normals', 'recompute_tangents',
            'generate_collision', 'generate_complex_collision', 'simplify_collision',
            'generate_lods', 'set_lod_settings', 'set_lod_screen_sizes', 'convert_to_nanite',
'convert_to_static_mesh',
            'get_mesh_info',
            // Wave 5.29-5.35: Geometry Actions
            'create_procedural_box', 'boolean_mesh_operation', 'generate_mesh_uvs',
            'create_mesh_from_spline', 'configure_nanite_settings', 'export_geometry_to_file'
          ],
          description: 'Geometry action to perform'
        },
        meshPath: commonSchemas.meshPath,
        targetMeshPath: { type: 'string', description: 'Path to second mesh for boolean operations.' },
        outputPath: commonSchemas.outputPath,
        actorName: commonSchemas.actorName,
        width: commonSchemas.width,
        height: commonSchemas.height,
        depth: commonSchemas.depth,
        radius: commonSchemas.radius,
        innerRadius: { type: 'number', description: 'Inner radius for torus.' },
        numSides: { type: 'number', description: 'Number of sides for cylinder, cone, etc.' },
        numRings: { type: 'number', description: 'Number of rings for sphere, torus.' },
        numSteps: { type: 'number', description: 'Number of steps for stairs.' },
        stepWidth: { type: 'number', description: 'Width of each stair step.' },
        stepHeight: { type: 'number', description: 'Height of each stair step.' },
        stepDepth: { type: 'number', description: 'Depth of each stair step.' },
        numTurns: { type: 'number', description: 'Number of turns for spiral.' },
        widthSegments: { type: 'number', description: 'Segments along width.' },
        heightSegments: { type: 'number', description: 'Segments along height.' },
        depthSegments: { type: 'number', description: 'Segments along depth.' },
        radialSegments: { type: 'number', description: 'Radial segments for circular shapes.' },
        location: commonSchemas.location,
        rotation: commonSchemas.rotation,
        scale: commonSchemas.scale,
        distance: commonSchemas.distance,
        amount: { type: 'number', description: 'Generic amount for operations (bevel size, inset distance, etc.).' },
        segments: { type: 'number', description: 'Number of segments for bevel, subdivide.' },
        angle: commonSchemas.angle,
        axis: { type: 'string', enum: ['X', 'Y', 'Z'], description: 'Axis for deformation operations.' },
        strength: commonSchemas.strength,
        iterations: { type: 'number', description: 'Number of iterations for smooth, remesh.' },
        targetTriangleCount: { type: 'number', description: 'Target triangle count for simplification.' },
        targetEdgeLength: { type: 'number', description: 'Target edge length for remeshing.' },
        weldDistance: { type: 'number', description: 'Distance threshold for vertex welding.' },
        faceIndices: { type: 'array', items: commonSchemas.numberProp, description: 'Array of face indices.' },
        edgeIndices: { type: 'array', items: commonSchemas.numberProp, description: 'Array of edge indices.' },
        vertexIndices: { type: 'array', items: commonSchemas.numberProp, description: 'Array of vertex indices.' },
        selectionBox: { type: 'object', properties: { min: commonSchemas.objectProp, max: commonSchemas.objectProp }, description: 'Bounding box for selection.' },
        uvChannel: { type: 'number', description: 'UV channel index (0-7).' },
        uvScale: { type: 'object', properties: { u: commonSchemas.numberProp, v: commonSchemas.numberProp }, description: 'UV scale.' },
        uvOffset: { type: 'object', properties: { u: commonSchemas.numberProp, v: commonSchemas.numberProp }, description: 'UV offset.' },
        projectionDirection: { type: 'string', enum: ['X', 'Y', 'Z', 'Auto'], description: 'Projection direction for UV.' },
        hardEdgeAngle: { type: 'number', description: 'Angle threshold for hard edges (degrees).' },
        computeWeightedNormals: { type: 'boolean', description: 'Use area-weighted normals.' },
        smoothingGroupId: { type: 'number', description: 'Smoothing group ID.' },
        collisionType: { type: 'string', enum: ['Default', 'Simple', 'Complex', 'UseComplexAsSimple', 'UseSimpleAsComplex'], description: 'Collision complexity type.' },
        hullCount: { type: 'number', description: 'Number of convex hulls for decomposition.' },
        hullPrecision: { type: 'number', description: 'Precision for convex hull generation (0-1).' },
        maxVerticesPerHull: { type: 'number', description: 'Maximum vertices per convex hull.' },
        lodCount: { type: 'number', description: 'Number of LOD levels to generate.' },
        lodIndex: { type: 'number', description: 'Specific LOD index to configure.' },
        reductionPercent: { type: 'number', description: 'Percent of triangles to reduce per LOD.' },
        screenSize: { type: 'number', description: 'Screen size threshold for LOD switching.' },
        screenSizes: { type: 'array', items: commonSchemas.numberProp, description: 'Array of screen sizes for each LOD.' },
        preserveBorders: { type: 'boolean', description: 'Preserve mesh borders during LOD generation.' },
        preserveUVs: { type: 'boolean', description: 'Preserve UV seams during LOD generation.' },
        exportFormat: { type: 'string', enum: ['FBX', 'OBJ', 'glTF', 'USD'], description: 'Export file format.' },
        exportPath: commonSchemas.exportPath,
        includeNormals: { type: 'boolean', description: 'Include normals in export.' },
        includeUVs: { type: 'boolean', description: 'Include UVs in export.' },
        includeTangents: { type: 'boolean', description: 'Include tangents in export.' },
createAsset: { type: 'boolean', description: 'Create as persistent asset.' },
        overwrite: commonSchemas.overwrite,
        save: commonSchemas.save,
        enableNanite: { type: 'boolean', description: 'Enable Nanite for the output mesh.' },
        // Wave 5.29-5.35: Additional properties
        booleanOperation: { type: 'string', enum: ['Union', 'Subtract', 'Intersection'], description: 'Boolean operation type for boolean_mesh_operation.' },
        uvMethod: { type: 'string', enum: ['Auto', 'Box', 'Cylindrical', 'Spherical', 'Planar'], description: 'UV generation method for generate_mesh_uvs.' },
        splinePath: { type: 'string', description: 'Path to spline actor for create_mesh_from_spline.' },
        profileShape: { type: 'string', enum: ['Circle', 'Square', 'Custom'], description: 'Profile shape for spline mesh generation.' },
        profileSize: { type: 'number', description: 'Size of the profile for spline mesh.' },
        nanitePositionPrecision: { type: 'number', description: 'Nanite position precision (0=auto, 1-4=explicit).' },
        nanitePercentTriangles: { type: 'number', description: 'Percent of triangles to keep for Nanite fallback (0-100).' },
        naniteFallbackRelativeError: { type: 'number', description: 'Fallback relative error threshold for Nanite.' },
        naniteTrimRelativeError: { type: 'number', description: 'Trim relative error for Nanite.' },
        filePath: { type: 'string', description: 'File path for export_geometry_to_file.' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        ...commonSchemas.outputBase,
        meshPath: commonSchemas.meshPath,
        actorName: commonSchemas.actorName,
        meshInfo: {
          type: 'object',
          properties: {
            vertexCount: commonSchemas.numberProp,
            triangleCount: commonSchemas.numberProp,
            uvChannels: commonSchemas.numberProp,
            hasNormals: commonSchemas.booleanProp,
            hasTangents: commonSchemas.booleanProp,
            boundingBox: commonSchemas.objectProp,
            lodCount: commonSchemas.numberProp
          }
        },
        error: commonSchemas.stringProp
      }
    }
  },
  {
    name: 'manage_skeleton',
    category: 'authoring',
    description: 'Skeletal meshes, sockets, physics assets; media players/sources.',
    annotations: {
      audience: ['developer', 'animator', 'tech-artist'],
      priority: 6,
      tags: ['skeleton', 'socket', 'physics-asset', 'media', 'morph']
    },
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'create_skeleton', 'add_bone', 'remove_bone', 'rename_bone',
            'set_bone_transform', 'set_bone_parent',
            'create_virtual_bone',
            'create_socket', 'configure_socket',
            'auto_skin_weights', 'set_vertex_weights',
            'normalize_weights', 'prune_weights',
            'copy_weights', 'mirror_weights',
            'create_physics_asset',
            'add_physics_body', 'configure_physics_body',
            'add_physics_constraint', 'configure_constraint_limits',
            'bind_cloth_to_skeletal_mesh', 'assign_cloth_asset_to_mesh',
            'create_morph_target', 'set_morph_target_deltas', 'import_morph_targets',
            'get_skeleton_info', 'list_bones', 'list_sockets', 'list_physics_bodies',
            // Media (merged from manage_media)
            'create_media_player', 'create_file_media_source', 'create_stream_media_source', 'create_media_texture',
            'create_media_playlist', 'create_media_sound_wave',
            'delete_media_asset', 'get_media_info',
            'add_to_playlist', 'remove_from_playlist', 'get_playlist',
            'open_source', 'open_url', 'close', 'play', 'pause', 'stop', 'seek', 'set_rate',
            'set_looping', 'get_duration', 'get_time', 'get_state',
            'bind_to_texture', 'unbind_from_texture'
          ],
          description: 'Skeleton action to perform'
        },
        skeletonPath: commonSchemas.skeletonPath,
        skeletalMeshPath: commonSchemas.skeletalMeshPath,
        physicsAssetPath: commonSchemas.physicsAssetPath,
        morphTargetPath: { type: 'string', description: 'Path to morph target or FBX file for import.' },
        clothAssetPath: commonSchemas.clothAssetPath,
        outputPath: commonSchemas.outputPath,
        boneName: commonSchemas.boneName,
        newBoneName: commonSchemas.newName,
        parentBoneName: commonSchemas.parentBoneName,
        sourceBoneName: commonSchemas.sourceBoneName,
        targetBoneName: commonSchemas.targetBoneName,
        boneIndex: { type: 'number', description: 'Bone index for operations.' },
        location: commonSchemas.location,
        rotation: commonSchemas.rotation,
        scale: commonSchemas.scale,
        socketName: commonSchemas.socketName,
        attachBoneName: commonSchemas.attachBoneName,
        relativeLocation: commonSchemas.location,
        relativeRotation: commonSchemas.rotation,
        relativeScale: commonSchemas.scale,
        vertexIndex: { type: 'number', description: 'Vertex index for weight operations.' },
        vertexIndices: { type: 'array', items: commonSchemas.numberProp, description: 'Array of vertex indices.' },
        weights: { type: 'array', items: commonSchemas.objectProp, description: 'Array of {boneIndex, weight} pairs.' },
        threshold: { type: 'number', description: 'Weight threshold for pruning (0-1).' },
        mirrorAxis: { type: 'string', enum: ['X', 'Y', 'Z'], description: 'Axis for weight mirroring.' },
        mirrorTable: { type: 'object', description: 'Bone name mapping for mirroring.' },
        bodyType: { type: 'string', enum: ['Capsule', 'Sphere', 'Box', 'Convex', 'Sphyl'], description: 'Physics body shape type.' },
        bodyName: commonSchemas.bodyName,
        mass: commonSchemas.mass,
        linearDamping: { type: 'number', description: 'Linear damping factor.' },
        angularDamping: { type: 'number', description: 'Angular damping factor.' },
        collisionEnabled: { type: 'boolean', description: 'Enable collision for this body.' },
        simulatePhysics: { type: 'boolean', description: 'Enable physics simulation.' },
        constraintName: commonSchemas.constraintName,
        bodyA: commonSchemas.bodyA,
        bodyB: commonSchemas.bodyB,
        limits: {
          type: 'object',
          properties: {
            swing1LimitAngle: { type: 'number', description: 'Swing 1 limit in degrees.' },
            swing2LimitAngle: { type: 'number', description: 'Swing 2 limit in degrees.' },
            twistLimitAngle: { type: 'number', description: 'Twist limit in degrees.' },
            swing1Motion: { type: 'string', enum: ['Free', 'Limited', 'Locked'] },
            swing2Motion: { type: 'string', enum: ['Free', 'Limited', 'Locked'] },
            twistMotion: { type: 'string', enum: ['Free', 'Limited', 'Locked'] }
          },
          description: 'Constraint angular limits.'
        },
        morphTargetName: commonSchemas.morphTargetName,
        deltas: { type: 'array', items: commonSchemas.objectProp, description: 'Array of {vertexIndex, delta} for morph target.' },
        paintValue: { type: 'number', description: 'Cloth weight paint value (0-1).' },
        save: commonSchemas.save,
        overwrite: commonSchemas.overwrite,
        // Media (merged from manage_media)
        mediaPlayerPath: commonSchemas.stringProp,
        mediaSourcePath: commonSchemas.stringProp,
        mediaTexturePath: commonSchemas.stringProp,
        playlistPath: commonSchemas.stringProp,
        filePath: commonSchemas.stringProp,
        url: commonSchemas.stringProp,
        time: commonSchemas.numberProp,
        rate: commonSchemas.numberProp,
        looping: commonSchemas.booleanProp,
        autoPlay: commonSchemas.booleanProp
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        ...commonSchemas.outputBase,
        skeletonPath: commonSchemas.skeletonPath,
        physicsAssetPath: commonSchemas.assetPath,
        socketName: commonSchemas.socketName,
        // Media outputs
        mediaPlayerPath: commonSchemas.stringProp,
        mediaInfo: commonSchemas.objectProp,
        playbackState: commonSchemas.objectProp,
        error: commonSchemas.stringProp
      }
    }
  },
  {
    name: 'manage_material_authoring',
    category: 'authoring',
    description: 'Materials, expressions, parameters, landscape layers, textures.',
    annotations: {
      audience: ['developer', 'material-artist'],
      priority: 7,
      tags: ['material', 'texture', 'shader', 'substrate', 'landscape-layer']
    },
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'create_material', 'set_blend_mode', 'set_shading_model', 'set_material_domain',
            'add_texture_sample', 'add_texture_coordinate', 'add_scalar_parameter', 'add_vector_parameter',
            'add_static_switch_parameter', 'add_math_node', 'add_world_position', 'add_vertex_normal',
            'add_pixel_depth', 'add_fresnel', 'add_reflection_vector', 'add_panner', 'add_rotator',
            'add_noise', 'add_voronoi', 'add_if', 'add_switch', 'add_custom_expression',
            'connect_nodes', 'disconnect_nodes',
            'create_material_function', 'add_function_input', 'add_function_output', 'use_material_function',
            'create_material_instance', 'set_scalar_parameter_value', 'set_vector_parameter_value', 'set_texture_parameter_value',
            'create_landscape_material', 'create_decal_material', 'create_post_process_material',
            'add_landscape_layer', 'configure_layer_blend',
            'compile_material', 'get_material_info',
            // Textures (merged from manage_texture)
            'create_noise_texture', 'create_gradient_texture', 'create_pattern_texture',
            'create_normal_from_height', 'create_ao_from_mesh',
            'resize_texture', 'adjust_levels', 'adjust_curves', 'blur', 'sharpen',
            'invert', 'desaturate', 'channel_pack', 'channel_extract', 'combine_textures',
            'set_compression_settings', 'set_texture_group', 'set_lod_bias',
            'configure_virtual_texture', 'set_streaming_priority',
            'get_texture_info',
            // Substrate (Phase 3G.1)
            'create_substrate_material', 'set_substrate_properties',
            'configure_sss_profile', 'configure_exposure',
            // Wave 4.21-4.30: Material Enhancement Actions
            'convert_material_to_substrate', 'batch_convert_to_substrate',
            'create_material_expression_template', 'configure_landscape_material_layer',
            'create_material_instance_batch', 'get_material_dependencies',
            'validate_material', 'configure_material_lod', 'export_material_template'
          ],
          description: 'Material authoring action to perform'
        },
        assetPath: commonSchemas.assetPath,
        name: commonSchemas.name,
        path: commonSchemas.directoryPathForCreation,
        materialDomain: { type: 'string', enum: ['Surface', 'DeferredDecal', 'LightFunction', 'Volume', 'PostProcess', 'UI'], description: 'Material domain type.' },
        blendMode: { type: 'string', enum: ['Opaque', 'Masked', 'Translucent', 'Additive', 'Modulate', 'AlphaComposite', 'AlphaHoldout'], description: 'Blend mode.' },
        shadingModel: { type: 'string', enum: ['DefaultLit', 'Unlit', 'Subsurface', 'SubsurfaceProfile', 'PreintegratedSkin', 'ClearCoat', 'Hair', 'Cloth', 'Eye', 'TwoSidedFoliage', 'ThinTranslucent'], description: 'Shading model.' },
        twoSided: { type: 'boolean', description: 'Enable two-sided rendering.' },
        x: commonSchemas.nodeX,
        y: commonSchemas.nodeY,
        texturePath: commonSchemas.texturePath,
        samplerType: { type: 'string', enum: ['Color', 'LinearColor', 'Normal', 'Masks', 'Alpha', 'VirtualColor', 'VirtualNormal'], description: 'Texture sampler type.' },
        coordinateIndex: { type: 'number', description: 'UV channel index (0-7).' },
        uTiling: { type: 'number', description: 'U tiling factor.' },
        vTiling: { type: 'number', description: 'V tiling factor.' },
        parameterName: commonSchemas.parameterName,
        defaultValue: { description: 'Default value for parameter (number for scalar, object for vector, bool for switch).' },
        group: commonSchemas.group,
        value: { description: 'Value to set (number, vector object, or texture path).' },
        operation: { type: 'string', enum: ['Add', 'Subtract', 'Multiply', 'Divide', 'Lerp', 'Clamp', 'Power', 'SquareRoot', 'Abs', 'Floor', 'Ceil', 'Frac', 'Sine', 'Cosine', 'Saturate', 'OneMinus', 'Min', 'Max', 'Dot', 'Cross', 'Normalize', 'Append'], description: 'Math operation type.' },
        constA: { type: 'number', description: 'Constant A input value.' },
        constB: { type: 'number', description: 'Constant B input value.' },
        code: commonSchemas.code,
        outputType: { type: 'string', enum: ['Float1', 'Float2', 'Float3', 'Float4', 'MaterialAttributes'], description: 'Output type of custom expression.' },
        description: { type: 'string', description: 'Description for custom expression or function.' },
        sourceNodeId: { type: 'string', description: 'Source node ID for connection.' },
        sourcePin: { type: 'string', description: 'Source pin name (output).' },
        targetNodeId: { type: 'string', description: 'Target node ID for connection.' },
        targetPin: { type: 'string', description: 'Target pin name (input).' },
        nodeId: commonSchemas.nodeId,
        pinName: commonSchemas.pinName,
        functionPath: commonSchemas.functionPath,
        exposeToLibrary: { type: 'boolean', description: 'Expose function to material library.' },
        inputName: commonSchemas.inputName,
        inputType: { type: 'string', enum: ['Float1', 'Float2', 'Float3', 'Float4', 'Texture2D', 'TextureCube', 'Bool', 'MaterialAttributes'], description: 'Type of function input/output.' },
        parentMaterial: { type: 'string', description: 'Path to parent material for instances.' },
        layerName: commonSchemas.layerName,
        blendType: { type: 'string', enum: ['LB_WeightBlend', 'LB_AlphaBlend', 'LB_HeightBlend'], description: 'Landscape layer blend type.' },
        layers: { type: 'array', items: commonSchemas.objectProp, description: 'Array of layer configurations for layer blend.' },
        save: commonSchemas.save,
        // Texture properties (merged from manage_texture)
        width: commonSchemas.width,
        height: commonSchemas.height,
        newWidth: commonSchemas.numberProp,
        newHeight: commonSchemas.numberProp,
        noiseType: { type: 'string', enum: ['Perlin', 'Simplex', 'Worley', 'Voronoi'] },
        scale: commonSchemas.numberProp,
        octaves: commonSchemas.numberProp,
        seed: commonSchemas.numberProp,
        seamless: commonSchemas.booleanProp,
        gradientType: { type: 'string', enum: ['Linear', 'Radial', 'Angular'] },
        startColor: commonSchemas.colorObject,
        endColor: commonSchemas.colorObject,
        patternType: { type: 'string', enum: ['Checker', 'Grid', 'Brick', 'Tile', 'Dots', 'Stripes'] },
        primaryColor: commonSchemas.colorObject,
        secondaryColor: commonSchemas.colorObject,
        sourceTexture: commonSchemas.stringProp,
        strength: commonSchemas.strength,
        filterMethod: { type: 'string', enum: ['Nearest', 'Bilinear', 'Bicubic', 'Lanczos'] },
        outputPath: commonSchemas.outputPath,
        channel: { type: 'string', enum: ['All', 'Red', 'Green', 'Blue', 'Alpha'] },
        compressionSettings: commonSchemas.stringProp,
        textureGroup: commonSchemas.stringProp,
        lodBias: commonSchemas.numberProp,
        virtualTextureStreaming: commonSchemas.booleanProp,
        neverStream: commonSchemas.booleanProp,
        // Substrate properties
        slabType: { type: 'string', enum: ['Simple', 'VerticalLayer', 'HorizontalLayer'], description: 'Substrate slab type.' },
        thickness: { type: 'number', description: 'Slab thickness.' },
        fuzzAmount: { type: 'number', description: 'Fuzz amount.' },
        scatterRadius: { type: 'number', description: 'SSS scatter radius.' },
        postProcessVolumeName: { type: 'string', description: 'PPV name for exposure.' },
        minBrightness: { type: 'number', description: 'Auto exposure min brightness.' },
        maxBrightness: { type: 'number', description: 'Auto exposure max brightness.' },
        speedUp: { type: 'number', description: 'Exposure speed up.' },
        speedDown: { type: 'number', description: 'Exposure speed down.' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        ...commonSchemas.outputBase,
        assetPath: commonSchemas.assetPath,
        nodeId: commonSchemas.nodeId,
        // Texture info (merged from manage_texture)
        textureInfo: {
          type: 'object',
          properties: {
            width: commonSchemas.numberProp,
            height: commonSchemas.numberProp,
            format: commonSchemas.stringProp,
            compression: commonSchemas.stringProp
          }
        },
        error: commonSchemas.stringProp
      }
    }
  },
  // [MERGED] manage_texture actions now in manage_material_authoring
  // [MERGED] manage_animation_authoring actions now in animation_physics (Phase 53: Strategic Tool Merging)
  // [MERGED] manage_audio_authoring actions now in manage_audio (Phase 53: Strategic Tool Merging)
  // [MERGED] manage_niagara_authoring actions now in manage_effect (Phase 53: Strategic Tool Merging)
  // [MERGED] manage_gas actions now in manage_combat
  {
    name: 'manage_character',
    category: 'gameplay',
    description: 'Characters, movement, locomotion + Inventory (items, equipment).',
    annotations: {
      audience: ['developer', 'game-designer'],
      priority: 7,
      tags: ['character', 'movement', 'inventory', 'interaction', 'locomotion']
    },
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'create_character_blueprint',
            'configure_capsule_component',
            'configure_mesh_component',
            'configure_camera_component',
            'configure_movement_speeds',
            'configure_jump',
            'configure_rotation',
            'add_custom_movement_mode',
            'configure_nav_movement',
            'setup_mantling',
            'setup_vaulting',
            'setup_climbing',
            'setup_sliding',
            'setup_wall_running',
            'setup_grappling',
            'setup_footstep_system',
            'map_surface_to_sound',
            'configure_footstep_fx',
            'get_character_info',
            // Interaction (merged from manage_interaction)
            'create_interaction_component',
            'configure_interaction_trace',
            'configure_interaction_widget',
            'add_interaction_events',
            'create_interactable_interface',
            'create_door_actor',
            'configure_door_properties',
            'create_switch_actor',
            'configure_switch_properties',
            'create_chest_actor',
            'configure_chest_properties',
            'create_lever_actor',
            'setup_destructible_mesh',
            'configure_destruction_levels',
            'configure_destruction_effects',
            'configure_destruction_damage',
            'add_destruction_component',
            'create_trigger_actor',
            'configure_trigger_events',
            'configure_trigger_filter',
            'configure_trigger_response',
            'get_interaction_info',
            // Inventory (merged from manage_inventory)
            'inv_create_item_data_asset', 'inv_set_item_properties', 'inv_create_item_category',
            'inv_assign_item_category', 'inv_create_inventory_component', 'inv_configure_inventory_slots',
            'inv_add_inventory_functions', 'inv_configure_inventory_events', 'inv_set_inventory_replication',
            'inv_create_pickup_actor', 'inv_configure_pickup_interaction', 'inv_configure_pickup_respawn',
            'inv_configure_pickup_effects', 'inv_create_equipment_component', 'inv_define_equipment_slots',
            'inv_configure_equipment_effects', 'inv_add_equipment_functions', 'inv_configure_equipment_visuals',
            'inv_create_loot_table', 'inv_add_loot_entry', 'inv_configure_loot_drop', 'inv_set_loot_quality_tiers',
            'inv_create_crafting_recipe', 'inv_configure_recipe_requirements', 'inv_create_crafting_station',
            'inv_add_crafting_component', 'inv_get_inventory_info',
            // Wave 3.21-3.30: Character System Actions
            'configure_locomotion_state', 'query_interaction_targets', 'configure_inventory_slot',
            'batch_add_inventory_items', 'configure_equipment_socket', 'get_character_stats_snapshot',
            'apply_status_effect', 'configure_footstep_system', 'set_movement_mode', 'configure_mantle_vault'
          ],
          description: 'Character action to perform.'
        },
        name: commonSchemas.assetNameForCreation,
        path: commonSchemas.directoryPathForCreation,
        blueprintPath: commonSchemas.blueprintPath,
        save: commonSchemas.save,
        parentClass: {
          type: 'string',
          enum: ['Character', 'ACharacter', 'PlayerCharacter', 'AICharacter'],
          description: 'Parent class for character blueprint.'
        },
        skeletalMeshPath: commonSchemas.skeletalMeshPath,
        animBlueprintPath: commonSchemas.animBlueprintPath,
        capsuleRadius: commonSchemas.numberProp,
        capsuleHalfHeight: commonSchemas.numberProp,
        meshOffset: {
          type: 'object',
          properties: {
            x: commonSchemas.numberProp,
            y: commonSchemas.numberProp,
            z: commonSchemas.numberProp
          },
          description: 'Mesh location offset.'
        },
        meshRotation: {
          type: 'object',
          properties: {
            pitch: commonSchemas.numberProp,
            yaw: commonSchemas.numberProp,
            roll: commonSchemas.numberProp
          },
          description: 'Mesh rotation offset.'
        },
        cameraSocketName: commonSchemas.cameraSocketName,
        cameraOffset: {
          type: 'object',
          properties: {
            x: commonSchemas.numberProp,
            y: commonSchemas.numberProp,
            z: commonSchemas.numberProp
          },
          description: 'Camera location offset.'
        },
        cameraUsePawnControlRotation: { type: 'boolean', description: 'Camera follows controller rotation.' },
        springArmLength: commonSchemas.numberProp,
        springArmLagEnabled: { type: 'boolean', description: 'Enable camera lag.' },
        springArmLagSpeed: { type: 'number', description: 'Camera lag speed.' },
        walkSpeed: commonSchemas.numberProp,
        runSpeed: commonSchemas.numberProp,
        sprintSpeed: commonSchemas.numberProp,
        crouchSpeed: commonSchemas.numberProp,
        swimSpeed: commonSchemas.numberProp,
        flySpeed: commonSchemas.numberProp,
        acceleration: commonSchemas.numberProp,
        deceleration: commonSchemas.numberProp,
        groundFriction: commonSchemas.numberProp,
        jumpHeight: commonSchemas.numberProp,
        airControl: commonSchemas.numberProp,
        doubleJumpEnabled: { type: 'boolean', description: 'Enable double jump.' },
        maxJumpCount: commonSchemas.numberProp,
        jumpHoldTime: { type: 'number', description: 'Max hold time for variable jump.' },
        gravityScale: commonSchemas.numberProp,
        fallingLateralFriction: { type: 'number', description: 'Air friction.' },
        orientToMovement: { type: 'boolean', description: 'Orient rotation to movement direction.' },
        useControllerRotationYaw: { type: 'boolean', description: 'Use controller yaw rotation.' },
        useControllerRotationPitch: { type: 'boolean', description: 'Use controller pitch rotation.' },
        useControllerRotationRoll: { type: 'boolean', description: 'Use controller roll rotation.' },
        rotationRate: commonSchemas.numberProp,
        modeName: { type: 'string', description: 'Name for custom movement mode.' },
        modeId: { type: 'number', description: 'Custom movement mode ID.' },
        navAgentRadius: commonSchemas.numberProp,
        navAgentHeight: commonSchemas.numberProp,
        avoidanceEnabled: { type: 'boolean', description: 'Enable AI avoidance.' },
        pathFollowingEnabled: { type: 'boolean', description: 'Enable path following.' },
        mantleHeight: { type: 'number', description: 'Maximum mantle height.' },
        mantleReachDistance: { type: 'number', description: 'Forward reach for mantle check.' },
        mantleAnimationPath: { type: 'string', description: 'Path to mantle animation montage.' },
        vaultHeight: { type: 'number', description: 'Maximum vault obstacle height.' },
        vaultDepth: { type: 'number', description: 'Obstacle depth to check.' },
        vaultAnimationPath: { type: 'string', description: 'Path to vault animation montage.' },
        climbSpeed: commonSchemas.numberProp,
        climbableTag: { type: 'string', description: 'Tag for climbable surfaces.' },
        climbAnimationPath: { type: 'string', description: 'Path to climb animation.' },
        slideSpeed: commonSchemas.numberProp,
        slideDuration: commonSchemas.numberProp,
        slideCooldown: commonSchemas.numberProp,
        slideAnimationPath: { type: 'string', description: 'Path to slide animation.' },
        wallRunSpeed: { type: 'number', description: 'Wall running speed.' },
        wallRunDuration: { type: 'number', description: 'Maximum wall run duration.' },
        wallRunGravityScale: { type: 'number', description: 'Gravity during wall run.' },
        wallRunAnimationPath: { type: 'string', description: 'Path to wall run animation.' },
        grappleRange: { type: 'number', description: 'Maximum grapple distance.' },
        grappleSpeed: { type: 'number', description: 'Grapple pull speed.' },
        grappleTargetTag: { type: 'string', description: 'Tag for grapple targets.' },
        grappleCablePath: { type: 'string', description: 'Path to cable mesh/material.' },
        footstepEnabled: { type: 'boolean', description: 'Enable footstep system.' },
        footstepSocketLeft: { type: 'string', description: 'Left foot socket name.' },
        footstepSocketRight: { type: 'string', description: 'Right foot socket name.' },
        footstepTraceDistance: { type: 'number', description: 'Ground trace distance.' },
        surfaceType: {
          type: 'string',
          enum: ['Default', 'Concrete', 'Grass', 'Dirt', 'Metal', 'Wood', 'Water', 'Snow', 'Sand', 'Gravel', 'Custom'],
          description: 'Physical surface type.'
        },
        footstepSoundPath: { type: 'string', description: 'Path to footstep sound cue.' },
        footstepParticlePath: { type: 'string', description: 'Path to footstep particle.' },
        footstepDecalPath: { type: 'string', description: 'Path to footstep decal.' },
        // Wave 3.21-3.30: Character System properties
        locomotionState: { type: 'string', description: 'Locomotion state name (Idle, Walking, Running, Sprinting, Jumping, Falling, etc.).' },
        stateMachineBlueprint: { type: 'string', description: 'Path to locomotion state machine blueprint.' },
        stateTransitions: { type: 'array', items: { type: 'object' }, description: 'Array of state transition definitions.' },
        blendTime: { type: 'number', description: 'Blend time between locomotion states.' },
        interactionRange: { type: 'number', description: 'Range for interaction target query.' },
        interactionTypes: { type: 'array', items: { type: 'string' }, description: 'Filter by interaction types (e.g., Pickup, Use, Talk).' },
        slotIndex: { type: 'number', description: 'Inventory slot index.' },
        slotType: { type: 'string', enum: ['Primary', 'Secondary', 'Consumable', 'Quest', 'Equipment', 'Misc'], description: 'Inventory slot type.' },
        slotCapacity: { type: 'number', description: 'Maximum items in slot.' },
        slotFilter: { type: 'array', items: { type: 'string' }, description: 'Allowed item categories for slot.' },
        itemDataAssets: { type: 'array', items: { type: 'string' }, description: 'Array of item data asset paths to add.' },
        targetSlot: { type: 'string', description: 'Target slot for batch add (optional).' },
        autoStack: { type: 'boolean', description: 'Automatically stack items if possible.' },
        socketName: { type: 'string', description: 'Equipment socket name for attachment.' },
        socketBone: { type: 'string', description: 'Bone to attach equipment socket to.' },
        socketOffset: { type: 'object', properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } }, description: 'Socket location offset.' },
        socketRotation: { type: 'object', properties: { pitch: { type: 'number' }, yaw: { type: 'number' }, roll: { type: 'number' } }, description: 'Socket rotation offset.' },
        socketScale: { type: 'object', properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } }, description: 'Socket scale.' },
        includeStats: { type: 'array', items: { type: 'string' }, description: 'Stats to include in snapshot (empty = all).' },
        statusEffectId: { type: 'string', description: 'Status effect ID or class path to apply.' },
        effectDuration: { type: 'number', description: 'Duration of status effect (-1 = infinite).' },
        effectMagnitude: { type: 'number', description: 'Magnitude/strength of status effect.' },
        effectStacks: { type: 'number', description: 'Number of effect stacks to apply.' },
        effectSource: { type: 'string', description: 'Source actor of status effect.' },
        footstepMaterialMap: { type: 'object', description: 'Map of surface types to sound/VFX paths.' },
        footstepVolumeScale: { type: 'number', description: 'Volume multiplier for footstep sounds.' },
        footstepInterval: { type: 'number', description: 'Minimum time between footstep triggers.' },
        movementMode: { type: 'string', enum: ['Walking', 'NavWalking', 'Falling', 'Swimming', 'Flying', 'Custom'], description: 'Character movement mode.' },
        customModeIndex: { type: 'number', description: 'Index for custom movement mode (if movementMode is Custom).' },
        mantleEnabled: { type: 'boolean', description: 'Enable mantling.' },
        vaultEnabled: { type: 'boolean', description: 'Enable vaulting.' },
        mantleMinHeight: { type: 'number', description: 'Minimum height for mantle.' },
        mantleMaxHeight: { type: 'number', description: 'Maximum height for mantle.' },
        vaultMaxWidth: { type: 'number', description: 'Maximum obstacle width for vault.' },
        mantleSpeed: { type: 'number', description: 'Mantle animation speed multiplier.' },
        vaultSpeed: { type: 'number', description: 'Vault animation speed multiplier.' },
        actorName: commonSchemas.actorName
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        ...commonSchemas.outputBase,
        blueprintPath: commonSchemas.blueprintPath,
        componentName: commonSchemas.componentName,
        modeName: commonSchemas.stringProp,
        characterInfo: {
          type: 'object',
          properties: {
            capsuleRadius: commonSchemas.numberProp,
            capsuleHalfHeight: commonSchemas.numberProp,
            walkSpeed: commonSchemas.numberProp,
            jumpZVelocity: commonSchemas.numberProp,
            airControl: commonSchemas.numberProp,
            orientToMovement: commonSchemas.booleanProp,
            hasSpringArm: commonSchemas.booleanProp,
            hasCamera: commonSchemas.booleanProp,
            customMovementModes: commonSchemas.arrayOfStrings
          }
        },
        error: commonSchemas.stringProp
      }
    }
  },
  {
    name: 'manage_combat',
    category: 'gameplay',
    description: 'Weapons, projectiles, damage, melee; GAS abilities and effects.',
    annotations: {
      audience: ['developer', 'game-designer'],
      priority: 7,
      tags: ['combat', 'weapon', 'projectile', 'gas', 'damage', 'melee']
    },
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'create_weapon_blueprint', 'configure_weapon_mesh', 'configure_weapon_sockets', 'set_weapon_stats',
            'configure_hitscan', 'configure_projectile', 'configure_spread_pattern', 'configure_recoil_pattern', 'configure_aim_down_sights',
            'create_projectile_blueprint', 'configure_projectile_movement', 'configure_projectile_collision', 'configure_projectile_homing',
            'create_damage_type', 'configure_damage_execution', 'setup_hitbox_component',
            'setup_reload_system', 'setup_ammo_system', 'setup_attachment_system', 'setup_weapon_switching',
            'configure_muzzle_flash', 'configure_tracer', 'configure_impact_effects', 'configure_shell_ejection',
            'create_melee_trace', 'configure_combo_system', 'create_hit_pause', 'configure_hit_reaction', 'setup_parry_block_system', 'configure_weapon_trails',
            'get_combat_info',
            // Wave 3.11-3.20: Combat System Actions
            'create_combo_sequence', 'apply_damage_with_effects', 'configure_weapon_trace',
            'create_projectile_pool', 'configure_gas_effect', 'grant_gas_ability',
            'configure_melee_trace', 'get_combat_stats', 'configure_block_parry',
            // GAS (merged from manage_gas)
            'add_ability_system_component', 'configure_asc', 'create_attribute_set', 'add_attribute',
            'set_attribute_base_value', 'set_attribute_clamping', 'create_gameplay_ability', 'set_ability_tags',
            'set_ability_costs', 'set_ability_cooldown', 'set_ability_targeting', 'add_ability_task',
            'set_activation_policy', 'set_instancing_policy', 'create_gameplay_effect', 'set_effect_duration',
            'add_effect_modifier', 'set_modifier_magnitude', 'add_effect_execution_calculation', 'add_effect_cue',
            'set_effect_stacking', 'set_effect_tags', 'create_gameplay_cue_notify', 'configure_cue_trigger',
            'set_cue_effects', 'add_tag_to_asset', 'get_gas_info'
          ],
          description: 'Combat action to perform'
        },
        blueprintPath: commonSchemas.blueprintPath,
        name: commonSchemas.name,
        path: commonSchemas.directoryPathForCreation,
        weaponMeshPath: { type: 'string', description: 'Path to weapon static/skeletal mesh.' },
        muzzleSocketName: commonSchemas.muzzleSocketName,
        ejectionSocketName: commonSchemas.ejectionSocketName,
        attachmentSocketNames: {
          type: 'array',
          items: commonSchemas.stringProp,
          description: 'List of attachment socket names.'
        },
        baseDamage: commonSchemas.numberProp,
        fireRate: commonSchemas.numberProp,
        range: commonSchemas.numberProp,
        spread: commonSchemas.numberProp,
        hitscanEnabled: { type: 'boolean', description: 'Enable hitscan firing.' },
        traceChannel: {
          type: 'string',
          enum: ['Visibility', 'Camera', 'Weapon', 'Custom'],
          description: 'Trace channel for hitscan.'
        },
        projectileClass: commonSchemas.projectileClass,
        spreadPattern: {
          type: 'string',
          enum: ['Random', 'Fixed', 'FixedWithRandom', 'Shotgun'],
          description: 'Spread pattern type.'
        },
        spreadIncrease: { type: 'number', description: 'Spread increase per shot.' },
        spreadRecovery: { type: 'number', description: 'Spread recovery rate.' },
        recoilPitch: { type: 'number', description: 'Vertical recoil (degrees).' },
        recoilYaw: { type: 'number', description: 'Horizontal recoil (degrees).' },
        recoilRecovery: { type: 'number', description: 'Recoil recovery speed.' },
        adsEnabled: { type: 'boolean', description: 'Enable aim down sights.' },
        adsFov: { type: 'number', description: 'FOV when aiming.' },
        adsSpeed: { type: 'number', description: 'Time to aim down sights.' },
        adsSpreadMultiplier: { type: 'number', description: 'Spread multiplier when aiming.' },
        projectileSpeed: commonSchemas.numberProp,
        projectileGravityScale: commonSchemas.numberProp,
        projectileLifespan: commonSchemas.numberProp,
        projectileMeshPath: { type: 'string', description: 'Path to projectile mesh.' },
        collisionRadius: commonSchemas.numberProp,
        bounceEnabled: { type: 'boolean', description: 'Enable projectile bouncing.' },
        bounceVelocityRatio: { type: 'number', description: 'Velocity retained on bounce (0-1).' },
        homingEnabled: { type: 'boolean', description: 'Enable homing behavior.' },
        homingAcceleration: { type: 'number', description: 'Homing turn rate.' },
        homingTargetTag: { type: 'string', description: 'Tag for homing targets.' },
        damageTypeName: { type: 'string', description: 'Name for damage type.' },
        damageCategory: {
          type: 'string',
          enum: ['Physical', 'Fire', 'Ice', 'Electric', 'Poison', 'Explosion', 'Radial', 'Custom'],
          description: 'Damage category.'
        },
        damageImpulse: { type: 'number', description: 'Impulse applied on hit.' },
        criticalMultiplier: { type: 'number', description: 'Critical hit damage multiplier.' },
        headshotMultiplier: { type: 'number', description: 'Headshot damage multiplier.' },
        hitboxBoneName: { type: 'string', description: 'Bone name for hitbox.' },
        hitboxType: {
          type: 'string',
          enum: ['Capsule', 'Box', 'Sphere'],
          description: 'Hitbox collision shape.'
        },
        hitboxSize: {
          type: 'object',
          properties: {
            radius: commonSchemas.numberProp,
            halfHeight: commonSchemas.numberProp,
            extent: commonSchemas.extent
          },
          description: 'Hitbox dimensions.'
        },
        isDamageZoneHead: { type: 'boolean', description: 'Mark as headshot zone.' },
        damageMultiplier: { type: 'number', description: 'Damage multiplier for this hitbox.' },
        magazineSize: commonSchemas.numberProp,
        reloadTime: commonSchemas.numberProp,
        reloadAnimationPath: { type: 'string', description: 'Path to reload animation.' },
        ammoType: { type: 'string', description: 'Ammo type identifier.' },
        maxAmmo: commonSchemas.numberProp,
        startingAmmo: commonSchemas.numberProp,
        attachmentSlots: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              slotName: commonSchemas.stringProp,
              socketName: commonSchemas.stringProp,
              allowedTypes: commonSchemas.arrayOfStrings
            }
          },
          description: 'Attachment slot definitions.'
        },
        switchInTime: { type: 'number', description: 'Time to equip weapon.' },
        switchOutTime: { type: 'number', description: 'Time to unequip weapon.' },
        switchInAnimationPath: { type: 'string', description: 'Path to equip animation.' },
        switchOutAnimationPath: { type: 'string', description: 'Path to unequip animation.' },
        muzzleFlashParticlePath: { type: 'string', description: 'Path to muzzle flash particle.' },
        muzzleFlashScale: { type: 'number', description: 'Muzzle flash scale.' },
        muzzleSoundPath: { type: 'string', description: 'Path to firing sound.' },
        tracerParticlePath: { type: 'string', description: 'Path to tracer particle.' },
        tracerSpeed: { type: 'number', description: 'Tracer travel speed.' },
        impactParticlePath: { type: 'string', description: 'Path to impact particle.' },
        impactSoundPath: { type: 'string', description: 'Path to impact sound.' },
        impactDecalPath: { type: 'string', description: 'Path to impact decal.' },
        shellMeshPath: { type: 'string', description: 'Path to shell casing mesh.' },
        shellEjectionForce: { type: 'number', description: 'Shell ejection impulse.' },
        shellLifespan: { type: 'number', description: 'Shell casing lifetime.' },
        meleeTraceStartSocket: { type: 'string', description: 'Socket for trace start.' },
        meleeTraceEndSocket: { type: 'string', description: 'Socket for trace end.' },
        meleeTraceRadius: { type: 'number', description: 'Sphere trace radius.' },
        meleeTraceChannel: { type: 'string', description: 'Trace channel for melee.' },
        comboWindowTime: { type: 'number', description: 'Time window for combo input.' },
        maxComboCount: { type: 'number', description: 'Maximum combo length.' },
        comboAnimations: {
          type: 'array',
          items: commonSchemas.stringProp,
          description: 'Paths to combo attack animations.'
        },
        hitPauseDuration: { type: 'number', description: 'Hitstop duration in seconds.' },
        hitPauseTimeDilation: { type: 'number', description: 'Time dilation during hitstop.' },
        hitReactionMontage: { type: 'string', description: 'Path to hit reaction montage.' },
        hitReactionStunTime: { type: 'number', description: 'Stun duration on hit.' },
        parryWindowStart: { type: 'number', description: 'Parry window start time (normalized).' },
        parryWindowEnd: { type: 'number', description: 'Parry window end time (normalized).' },
        parryAnimationPath: { type: 'string', description: 'Path to parry animation.' },
        blockDamageReduction: { type: 'number', description: 'Damage reduction when blocking (0-1).' },
        blockStaminaCost: { type: 'number', description: 'Stamina cost per blocked hit.' },
        weaponTrailParticlePath: { type: 'string', description: 'Path to weapon trail particle.' },
        weaponTrailStartSocket: { type: 'string', description: 'Trail start socket.' },
        weaponTrailEndSocket: { type: 'string', description: 'Trail end socket.' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        ...commonSchemas.outputBase,
        blueprintPath: commonSchemas.blueprintPath,
        damageTypePath: commonSchemas.stringProp,
        combatInfo: {
          type: 'object',
          properties: {
            weaponType: commonSchemas.stringProp,
            firingMode: commonSchemas.stringProp,
            baseDamage: commonSchemas.numberProp,
            fireRate: commonSchemas.numberProp,
            magazineSize: commonSchemas.numberProp,
            hasADS: commonSchemas.booleanProp,
            hasReload: commonSchemas.booleanProp,
            isMelee: commonSchemas.booleanProp,
            comboCount: commonSchemas.numberProp,
            attachmentSlots: commonSchemas.arrayOfStrings
          },
          description: 'Combat configuration info (for get_combat_info).'
        }
      }
    }
  },
  {
    name: 'manage_ai',
    category: 'gameplay',
    description: 'AI Controllers, BT, EQS, perception, State Trees, MassAI, NPCs.',
    annotations: {
      audience: ['developer', 'designer'],
      priority: 8,
      tags: ['ai', 'behavior-tree', 'eqs', 'perception', 'state-tree', 'npc', 'navigation']
    },
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'create_ai_controller', 'assign_behavior_tree', 'assign_blackboard',
            'create_blackboard_asset', 'add_blackboard_key', 'set_key_instance_synced',
            'create_behavior_tree', 'add_composite_node', 'add_task_node', 'add_decorator', 'add_service', 'configure_bt_node',
            'create_eqs_query', 'add_eqs_generator', 'add_eqs_context', 'add_eqs_test', 'configure_test_scoring',
            'add_ai_perception_component', 'configure_sight_config', 'configure_hearing_config', 'configure_damage_sense_config', 'set_perception_team',
            'create_state_tree', 'add_state_tree_state', 'add_state_tree_transition', 'configure_state_tree_task',
            'create_smart_object_definition', 'add_smart_object_slot', 'configure_slot_behavior', 'add_smart_object_component',
            'bind_statetree',
            'create_mass_entity_config', 'configure_mass_entity', 'add_mass_spawner', 'spawn_mass_entity',
            'destroy_mass_entity', 'query_mass_entities', 'set_mass_entity_fragment',
            // StateTree Query/Control (A2)
            'get_statetree_state', 'trigger_statetree_transition', 'list_statetree_states',
            // Smart Objects Integration (A3)
            'create_smart_object', 'query_smart_objects', 'claim_smart_object', 'release_smart_object',
            'get_ai_info',
            // Behavior Tree graph operations (merged from manage_behavior_tree)
            'bt_add_node', 'bt_connect_nodes', 'bt_remove_node', 'bt_break_connections', 'bt_set_node_properties',
            // Navigation (merged from manage_navigation - Phase 54)
            'configure_nav_mesh_settings', 'set_nav_agent_properties', 'rebuild_navigation',
            'create_nav_modifier_component', 'set_nav_area_class', 'configure_nav_area_cost',
            'create_nav_link_proxy', 'configure_nav_link', 'set_nav_link_type',
            'create_smart_link', 'configure_smart_link_behavior', 'get_navigation_info',
            // AI NPC Plugins (merged from manage_ai_npc)
            'create_convai_character', 'configure_character_backstory', 'configure_character_voice',
            'configure_convai_lipsync', 'start_convai_session', 'stop_convai_session',
            'send_text_to_character', 'get_character_response', 'configure_convai_actions', 'get_convai_info',
            'create_inworld_character', 'configure_inworld_settings', 'configure_inworld_scene',
            'start_inworld_session', 'stop_inworld_session', 'send_message_to_character',
            'get_character_emotion', 'get_character_goals', 'trigger_inworld_event', 'get_inworld_info',
            'configure_audio2face', 'process_audio_to_blendshapes', 'configure_blendshape_mapping',
            'start_audio2face_stream', 'stop_audio2face_stream', 'get_audio2face_status',
'configure_ace_emotions', 'get_ace_info', 'get_ai_npc_info', 'list_available_ai_backends',
            // AI Enhancement (Wave 3.1-3.10)
            'ai_assistant_query', 'ai_assistant_explain_feature', 'ai_assistant_suggest_fix',
            'configure_state_tree_node', 'debug_behavior_tree', 'query_eqs_results',
            'configure_mass_ai_fragment', 'spawn_mass_ai_entities', 'get_ai_perception_data', 'configure_smart_object'
          ],
          description: 'AI action to perform'
        },
        name: commonSchemas.name,
        path: commonSchemas.directoryPathForCreation,
        blueprintPath: commonSchemas.blueprintPath,
        controllerPath: commonSchemas.controllerPath,
        behaviorTreePath: commonSchemas.behaviorTreePath,
        blackboardPath: commonSchemas.blackboardPath,
        parentClass: {
          type: 'string',
          enum: ['AAIController', 'APlayerController'],
          description: 'Parent class for AI controller (default: AAIController).'
        },
        autoRunBehaviorTree: { type: 'boolean', description: 'Start behavior tree automatically on possess.' },
        keyName: commonSchemas.keyName,
        keyType: {
          type: 'string',
          enum: ['Bool', 'Int', 'Float', 'Vector', 'Rotator', 'Object', 'Class', 'Enum', 'Name', 'String'],
          description: 'Blackboard key data type.'
        },
        isInstanceSynced: { type: 'boolean', description: 'Sync key across instances.' },
        baseObjectClass: { type: 'string', description: 'Base class for Object/Class keys.' },
        enumClass: { type: 'string', description: 'Enum class for Enum keys.' },
        compositeType: {
          type: 'string',
          enum: ['Selector', 'Sequence', 'Parallel', 'SimpleParallel'],
          description: 'Composite node type.'
        },
        taskType: {
          type: 'string',
          enum: [
            'MoveTo', 'MoveDirectlyToward', 'RotateToFaceBBEntry', 'Wait', 'WaitBlackboardTime',
            'PlayAnimation', 'PlaySound', 'RunEQSQuery', 'RunBehaviorDynamic', 'SetBlackboardValue',
            'PushPawnAction', 'FinishWithResult', 'MakeNoise', 'GameplayTaskBase', 'Custom'
          ],
          description: 'Task node type.'
        },
        decoratorType: {
          type: 'string',
          enum: [
            'Blackboard', 'BlackboardBased', 'CompareBBEntries', 'Cooldown', 'ConeCheck',
            'DoesPathExist', 'IsAtLocation', 'IsBBEntryOfClass', 'KeepInCone', 'Loop',
            'SetTagCooldown', 'TagCooldown', 'TimeLimit', 'ForceSuccess', 'ConditionalLoop', 'Custom'
          ],
          description: 'Decorator node type.'
        },
        serviceType: {
          type: 'string',
          enum: ['DefaultFocus', 'RunEQS', 'Custom'],
          description: 'Service node type.'
        },
        parentNodeId: commonSchemas.nodeId,
        nodeId: commonSchemas.nodeId,
        nodeProperties: {
          type: 'object',
          additionalProperties: true,
          description: 'Properties to set on the node.'
        },
        customTaskClass: { type: 'string', description: 'Custom task class path for Custom task type.' },
        customDecoratorClass: { type: 'string', description: 'Custom decorator class path.' },
        customServiceClass: { type: 'string', description: 'Custom service class path.' },
        queryPath: commonSchemas.queryPath,
        generatorType: {
          type: 'string',
          enum: ['ActorsOfClass', 'CurrentLocation', 'Donut', 'OnCircle', 'PathingGrid', 'SimpleGrid', 'Composite', 'Custom'],
          description: 'EQS generator type.'
        },
        contextType: {
          type: 'string',
          enum: ['Querier', 'Item', 'EnvQueryContext_BlueprintBase', 'Custom'],
          description: 'EQS context type.'
        },
        testType: {
          type: 'string',
          enum: ['Distance', 'Dot', 'GameplayTags', 'Overlap', 'Pathfinding', 'PathfindingBatch', 'Project', 'Random', 'Trace', 'Custom'],
          description: 'EQS test type.'
        },
        generatorSettings: {
          type: 'object',
          properties: {
            searchRadius: commonSchemas.numberProp,
            searchCenter: commonSchemas.stringProp,
            actorClass: commonSchemas.stringProp,
            gridSize: commonSchemas.numberProp,
            spacesBetween: commonSchemas.numberProp,
            innerRadius: commonSchemas.numberProp,
            outerRadius: commonSchemas.numberProp
          },
          description: 'Generator-specific settings.'
        },
        testSettings: {
          type: 'object',
          properties: {
            scoringEquation: { type: 'string', enum: ['Linear', 'Square', 'InverseLinear', 'Constant'] },
            clampMin: commonSchemas.numberProp,
            clampMax: commonSchemas.numberProp,
            filterType: { type: 'string', enum: ['Minimum', 'Maximum', 'Range'] },
            floatMin: commonSchemas.numberProp,
            floatMax: commonSchemas.numberProp
          },
          description: 'Test scoring and filter settings.'
        },
        testIndex: { type: 'number', description: 'Index of test to configure.' },
        sightConfig: {
          type: 'object',
          properties: {
            sightRadius: commonSchemas.numberProp,
            loseSightRadius: commonSchemas.numberProp,
            peripheralVisionAngle: commonSchemas.numberProp,
            pointOfViewBackwardOffset: commonSchemas.numberProp,
            nearClippingRadius: commonSchemas.numberProp,
            autoSuccessRange: commonSchemas.numberProp,
            maxAge: commonSchemas.numberProp,
            detectionByAffiliation: {
              type: 'object',
              properties: {
                enemies: commonSchemas.booleanProp,
                neutrals: commonSchemas.booleanProp,
                friendlies: commonSchemas.booleanProp
              }
            }
          },
          description: 'AI sight sense configuration.'
        },
        hearingConfig: {
          type: 'object',
          properties: {
            hearingRange: commonSchemas.numberProp,
            loSHearingRange: commonSchemas.numberProp,
            detectFriendly: commonSchemas.booleanProp,
            maxAge: commonSchemas.numberProp
          },
          description: 'AI hearing sense configuration.'
        },
        damageConfig: {
          type: 'object',
          properties: {
            maxAge: commonSchemas.numberProp
          },
          description: 'AI damage sense configuration.'
        },
        teamId: { type: 'number', description: 'Team ID for perception affiliation (0=Neutral, 1=Player, 2=Enemy, etc.).' },
        dominantSense: {
          type: 'string',
          enum: ['Sight', 'Hearing', 'Damage', 'Touch', 'None'],
          description: 'Dominant sense for perception prioritization.'
        },
        stateTreePath: commonSchemas.stateTreePath,
        stateName: commonSchemas.stateName,
        fromState: commonSchemas.fromState,
        toState: commonSchemas.toState,
        transitionCondition: { type: 'string', description: 'Condition expression for transition.' },
        stateTaskClass: { type: 'string', description: 'Task class for state.' },
        stateEvaluatorClass: { type: 'string', description: 'Evaluator class for state.' },
        definitionPath: commonSchemas.definitionPath,
        slotIndex: { type: 'number', description: 'Index of slot to configure.' },
        slotOffset: {
          type: 'object',
          properties: commonSchemas.vector3.properties,
          description: 'Local offset for slot.'
        },
        slotRotation: {
          type: 'object',
          properties: commonSchemas.rotation.properties,
          description: 'Local rotation for slot.'
        },
        slotBehaviorDefinition: { type: 'string', description: 'Gameplay behavior definition for slot.' },
        slotActivityTags: {
          type: 'array',
          items: commonSchemas.stringProp,
          description: 'Activity tags for the slot.'
        },
        slotUserTags: {
          type: 'array',
          items: commonSchemas.stringProp,
          description: 'Required user tags for slot.'
        },
        slotEnabled: { type: 'boolean', description: 'Whether slot is enabled.' },
        configPath: commonSchemas.configPath,
        target: { type: 'string', description: 'Target actor name or path.' },
        count: commonSchemas.numberProp,
        massTraits: {
          type: 'array',
          items: commonSchemas.stringProp,
          description: 'List of Mass traits to add.'
        },
        massProcessors: {
          type: 'array',
          items: commonSchemas.stringProp,
          description: 'List of Mass processors to configure.'
        },
        spawnerSettings: {
          type: 'object',
          properties: {
            entityCount: commonSchemas.numberProp,
            spawnRadius: commonSchemas.numberProp,
            entityConfig: commonSchemas.stringProp,
            spawnOnBeginPlay: commonSchemas.booleanProp
          },
          description: 'Mass spawner configuration.'
        },
        // BT graph operations (merged from manage_behavior_tree)
        savePath: commonSchemas.savePath,
        x: commonSchemas.numberProp,
        y: commonSchemas.numberProp,
        comment: commonSchemas.stringProp,
        btNodeProperties: commonSchemas.objectProp,
        // Navigation properties (merged from manage_navigation - Phase 54)
        navMeshPath: { type: 'string', description: 'Path to NavMesh data asset.' },
        actorPath: commonSchemas.actorPath,
        agentRadius: { type: 'number', description: 'Navigation agent radius.' },
        agentHeight: { type: 'number', description: 'Navigation agent height.' },
        agentStepHeight: { type: 'number', description: 'Maximum step height agent can climb.' },
        agentMaxSlope: { type: 'number', description: 'Maximum slope angle in degrees.' },
        cellSize: { type: 'number', description: 'NavMesh cell size.' },
        cellHeight: { type: 'number', description: 'NavMesh cell height.' },
        areaClass: commonSchemas.areaClass,
        areaCost: { type: 'number', description: 'Pathfinding cost multiplier for area.' },
        linkName: commonSchemas.linkName,
        startPoint: commonSchemas.vector3,
        endPoint: commonSchemas.vector3,
        direction: { type: 'string', enum: ['BothWays', 'LeftToRight', 'RightToLeft'] },
        linkEnabled: { type: 'boolean', description: 'Whether the link is enabled.' },
        linkType: { type: 'string', enum: ['simple', 'smart'] }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        ...commonSchemas.outputBase,
        assetPath: commonSchemas.assetPath,
        controllerPath: commonSchemas.stringProp,
        behaviorTreePath: commonSchemas.stringProp,
        blackboardPath: commonSchemas.stringProp,
        queryPath: commonSchemas.stringProp,
        stateTreePath: commonSchemas.stringProp,
        definitionPath: commonSchemas.stringProp,
        configPath: commonSchemas.stringProp,
        nodeId: commonSchemas.nodeId,
        keyIndex: commonSchemas.integerProp,
        testIndex: commonSchemas.integerProp,
        slotIndex: commonSchemas.integerProp,
        aiInfo: {
          type: 'object',
          properties: {
            controllerClass: commonSchemas.stringProp,
            assignedBehaviorTree: commonSchemas.stringProp,
            assignedBlackboard: commonSchemas.stringProp,
            blackboardKeys: commonSchemas.arrayOfObjects,
            btNodeCount: commonSchemas.integerProp,
            perceptionSenses: commonSchemas.arrayOfStrings,
            teamId: commonSchemas.integerProp,
            stateTreeStates: commonSchemas.arrayOfStrings,
            smartObjectSlots: commonSchemas.numberProp,
            massTraits: commonSchemas.arrayOfStrings
          },
          description: 'AI configuration info (for get_ai_info).'
        },
        error: commonSchemas.stringProp
      }
    }
  },
  // [MERGED] manage_inventory actions now in manage_character (with inv_ prefix)
  // [MERGED] manage_interaction actions now in manage_character
  {
    name: 'manage_widget_authoring',
    category: 'utility',
    description: 'UMG widgets: buttons, text, sliders. Layouts, bindings, HUDs.',
    annotations: {
      audience: ['developer', 'designer'],
      priority: 6,
      tags: ['widget', 'umg', 'ui', 'layout', 'hud', 'binding']
    },
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'create_widget_blueprint',
            'set_widget_parent_class',
            'add_canvas_panel',
            'add_horizontal_box',
            'add_vertical_box',
            'add_overlay',
            'add_grid_panel',
            'add_uniform_grid',
            'add_wrap_box',
            'add_scroll_box',
            'add_size_box',
            'add_scale_box',
            'add_border',
            'add_text_block',
            'add_rich_text_block',
            'add_image',
            'add_button',
            'add_check_box',
            'add_slider',
            'add_progress_bar',
            'add_text_input',
            'add_combo_box',
            'add_spin_box',
            'add_list_view',
            'add_tree_view',
            'set_anchor',
            'set_alignment',
            'set_position',
            'set_size',
            'set_padding',
            'set_z_order',
            'set_render_transform',
            'set_visibility',
            'set_style',
            'set_clipping',
            'create_property_binding',
            'bind_text',
            'bind_visibility',
            'bind_color',
            'bind_enabled',
            'bind_on_clicked',
            'bind_on_hovered',
            'bind_on_value_changed',
            'create_widget_animation',
            'add_animation_track',
            'add_animation_keyframe',
            'set_animation_loop',
            'create_main_menu',
            'create_pause_menu',
            'create_settings_menu',
            'create_loading_screen',
            'create_hud_widget',
            'add_health_bar',
            'add_ammo_counter',
            'add_minimap',
            'add_crosshair',
            'add_compass',
            'add_interaction_prompt',
            'add_objective_tracker',
            'add_damage_indicator',
            'create_inventory_ui',
            'create_dialog_widget',
            'create_radial_menu',
            'get_widget_info',
            'preview_widget',
            // Wave 4.31-4.38: Widget Enhancement Actions
            'create_widget_template',
            'configure_widget_binding_batch',
            'create_widget_animation_advanced',
            'configure_widget_navigation',
            'validate_widget_accessibility',
            'create_hud_layout',
            'configure_safe_zone',
            'batch_localize_widgets'
          ],
          description: 'The widget authoring action to perform.'
        },
        name: commonSchemas.name,
        folder: commonSchemas.directoryPath,
        widgetPath: commonSchemas.widgetPath,
        slotName: commonSchemas.slotName,
        parentSlot: { type: 'string', description: 'Parent slot to add widget to.' },
        parentClass: commonSchemas.parentClass,
        anchorMin: {
          type: 'object',
          properties: commonSchemas.vector2.properties,
          description: 'Minimum anchor point (0-1).'
        },
        anchorMax: {
          type: 'object',
          properties: commonSchemas.vector2.properties,
          description: 'Maximum anchor point (0-1).'
        },
        alignment: {
          type: 'object',
          properties: commonSchemas.vector2.properties,
          description: 'Widget alignment (0-1).'
        },
        alignmentX: { type: 'number', description: 'Horizontal alignment (0-1).' },
        alignmentY: { type: 'number', description: 'Vertical alignment (0-1).' },
        positionX: { type: 'number', description: 'X position.' },
        positionY: { type: 'number', description: 'Y position.' },
        sizeX: { type: 'number', description: 'Width.' },
        sizeY: { type: 'number', description: 'Height.' },
        sizeToContent: { type: 'boolean', description: 'Size to content.' },
        left: { type: 'number', description: 'Left padding.' },
        top: { type: 'number', description: 'Top padding.' },
        right: { type: 'number', description: 'Right padding.' },
        bottom: { type: 'number', description: 'Bottom padding.' },
        zOrder: { type: 'number', description: 'Z-order for canvas slot.' },
        translation: {
          type: 'object',
          properties: commonSchemas.vector2.properties,
          description: 'Render translation.'
        },
        scale: {
          type: 'object',
          properties: commonSchemas.vector2.properties,
          description: 'Render scale.'
        },
        shear: {
          type: 'object',
          properties: commonSchemas.vector2.properties,
          description: 'Render shear.'
        },
        angle: commonSchemas.angle,
        pivot: {
          type: 'object',
          properties: commonSchemas.vector2.properties,
          description: 'Rotation/scale pivot.'
        },
        visibility: {
          type: 'string',
          enum: ['Visible', 'Collapsed', 'Hidden', 'HitTestInvisible', 'SelfHitTestInvisible'],
          description: 'Widget visibility state.'
        },
        clipping: {
          type: 'string',
          enum: ['Inherit', 'ClipToBounds', 'ClipToBoundsWithoutIntersecting', 'ClipToBoundsAlways', 'OnDemand'],
          description: 'Widget clipping mode.'
        },
        text: commonSchemas.text,
        font: { type: 'string', description: 'Font asset path.' },
        fontSize: { type: 'number', description: 'Font size.' },
        colorAndOpacity: {
          type: 'object',
          properties: commonSchemas.colorObject.properties,
          description: 'Color and opacity (0-1 values).'
        },
        justification: {
          type: 'string',
          enum: ['Left', 'Center', 'Right'],
          description: 'Text justification.'
        },
        autoWrap: { type: 'boolean', description: 'Enable text auto-wrap.' },
        texturePath: commonSchemas.texturePath,
        brushSize: {
          type: 'object',
          properties: commonSchemas.vector2.properties,
          description: 'Brush/image size.'
        },
        brushTiling: {
          type: 'string',
          enum: ['NoTile', 'Horizontal', 'Vertical', 'Both'],
          description: 'Image tiling mode.'
        },
        isEnabled: { type: 'boolean', description: 'Widget enabled state.' },
        isChecked: { type: 'boolean', description: 'Checkbox checked state.' },
        value: { type: 'number', description: 'Slider/spinbox value.' },
        minValue: commonSchemas.minValue,
        maxValue: commonSchemas.maxValue,
        stepSize: { type: 'number', description: 'Value step size.' },
        delta: { type: 'number', description: 'Spinbox increment.' },
        percent: { type: 'number', description: 'Progress bar percentage (0-1).' },
        fillColorAndOpacity: {
          type: 'object',
          properties: commonSchemas.colorObject.properties,
          description: 'Fill color for progress bar.'
        },
        barFillType: {
          type: 'string',
          enum: ['LeftToRight', 'RightToLeft', 'TopToBottom', 'BottomToTop', 'FillFromCenter'],
          description: 'Progress bar fill direction.'
        },
        isMarquee: { type: 'boolean', description: 'Progress bar marquee mode.' },
        inputType: { type: 'string', enum: ['single', 'multi'], description: 'Text input type.' },
        hintText: { type: 'string', description: 'Placeholder hint text.' },
        isPassword: { type: 'boolean', description: 'Password masking.' },
        options: {
          type: 'array',
          items: commonSchemas.stringProp,
          description: 'Combo box options.'
        },
        selectedOption: { type: 'string', description: 'Selected combo box option.' },
        entryWidgetClass: { type: 'string', description: 'List/tree view entry widget class.' },
        orientation: {
          type: 'string',
          enum: ['Horizontal', 'Vertical'],
          description: 'Widget orientation.'
        },
        selectionMode: {
          type: 'string',
          enum: ['None', 'Single', 'Multi'],
          description: 'Selection mode for list/tree.'
        },
        scrollBarVisibility: {
          type: 'string',
          enum: ['Visible', 'Collapsed', 'Auto'],
          description: 'Scroll bar visibility.'
        },
        alwaysShowScrollbar: { type: 'boolean', description: 'Always show scrollbar.' },
        columnCount: { type: 'number', description: 'Number of columns.' },
        rowCount: { type: 'number', description: 'Number of rows.' },
        slotPadding: { type: 'number', description: 'Padding between slots.' },
        minDesiredSlotWidth: { type: 'number', description: 'Minimum slot width.' },
        minDesiredSlotHeight: { type: 'number', description: 'Minimum slot height.' },
        innerSlotPadding: { type: 'number', description: 'Inner slot padding.' },
        wrapWidth: { type: 'number', description: 'Wrap width for wrap box.' },
        explicitWrapWidth: { type: 'boolean', description: 'Use explicit wrap width.' },
        widthOverride: { type: 'number', description: 'Width override for size box.' },
        heightOverride: { type: 'number', description: 'Height override for size box.' },
        minDesiredWidth: { type: 'number', description: 'Minimum desired width.' },
        minDesiredHeight: { type: 'number', description: 'Minimum desired height.' },
        stretch: {
          type: 'string',
          enum: ['None', 'Fill', 'ScaleToFit', 'ScaleToFitX', 'ScaleToFitY', 'ScaleToFill', 'UserSpecified'],
          description: 'Scale box stretch mode.'
        },
        stretchDirection: {
          type: 'string',
          enum: ['Both', 'DownOnly', 'UpOnly'],
          description: 'Scale box stretch direction.'
        },
        userSpecifiedScale: { type: 'number', description: 'User specified scale value.' },
        brushColor: {
          type: 'object',
          properties: commonSchemas.colorObject.properties,
          description: 'Border brush color.'
        },
        padding: { type: 'number', description: 'Uniform padding.' },
        horizontalAlignment: {
          type: 'string',
          enum: ['Fill', 'Left', 'Center', 'Right'],
          description: 'Horizontal alignment.'
        },
        verticalAlignment: {
          type: 'string',
          enum: ['Fill', 'Top', 'Center', 'Bottom'],
          description: 'Vertical alignment.'
        },
        color: {
          type: 'object',
          properties: commonSchemas.colorObject.properties,
          description: 'Widget color.'
        },
        opacity: { type: 'number', description: 'Widget opacity (0-1).' },
        brush: { type: 'string', description: 'Brush asset path.' },
        backgroundImage: { type: 'string', description: 'Background image path.' },
        style: { type: 'string', description: 'Style preset name.' },
        propertyName: commonSchemas.propertyName,
        bindingType: { type: 'string', enum: ['function', 'variable'], description: 'Binding type.' },
        bindingSource: { type: 'string', description: 'Variable or function name to bind to.' },
        functionName: commonSchemas.functionName,
        onHoveredFunction: { type: 'string', description: 'Function to call on hover.' },
        onUnhoveredFunction: { type: 'string', description: 'Function to call on unhover.' },
        animationName: commonSchemas.animationName,
        length: { type: 'number', description: 'Animation length in seconds.' },
        trackType: {
          type: 'string',
          enum: ['transform', 'color', 'opacity', 'renderOpacity', 'material'],
          description: 'Animation track type.'
        },
        time: { type: 'number', description: 'Keyframe time.' },
        interpolation: {
          type: 'string',
          enum: ['linear', 'cubic', 'constant'],
          description: 'Keyframe interpolation.'
        },
        loopCount: { type: 'number', description: 'Number of loops (-1 for infinite).' },
        playMode: {
          type: 'string',
          enum: ['forward', 'reverse', 'pingpong'],
          description: 'Animation play mode.'
        },
        includePlayButton: { type: 'boolean', description: 'Include play button in menu.' },
        includeSettingsButton: { type: 'boolean', description: 'Include settings button.' },
        includeQuitButton: { type: 'boolean', description: 'Include quit button.' },
        includeResumeButton: { type: 'boolean', description: 'Include resume button.' },
        includeQuitToMenuButton: { type: 'boolean', description: 'Include quit to menu button.' },
        settingsType: {
          type: 'string',
          enum: ['video', 'audio', 'controls', 'gameplay', 'all'],
          description: 'Settings menu type.'
        },
        includeApplyButton: { type: 'boolean', description: 'Include apply button.' },
        includeResetButton: { type: 'boolean', description: 'Include reset button.' },
        includeProgressBar: { type: 'boolean', description: 'Include progress bar.' },
        includeTipText: { type: 'boolean', description: 'Include tip text.' },
        includeBackgroundImage: { type: 'boolean', description: 'Include background image.' },
        titleText: { type: 'string', description: 'Menu title text.' },
        elements: {
          type: 'array',
          items: commonSchemas.stringProp,
          description: 'HUD elements to include.'
        },
        barStyle: {
          type: 'string',
          enum: ['simple', 'segmented', 'radial'],
          description: 'Health bar style.'
        },
        showNumbers: { type: 'boolean', description: 'Show numeric values.' },
        barColor: {
          type: 'object',
          properties: commonSchemas.colorObject.properties,
          description: 'Bar color.'
        },
        ammoStyle: {
          type: 'string',
          enum: ['numeric', 'icon'],
          description: 'Ammo counter style.'
        },
        showReserve: { type: 'boolean', description: 'Show reserve ammo.' },
        ammoIcon: { type: 'string', description: 'Ammo icon texture.' },
        minimapSize: { type: 'number', description: 'Minimap size.' },
        minimapShape: {
          type: 'string',
          enum: ['circle', 'square'],
          description: 'Minimap shape.'
        },
        rotateWithPlayer: { type: 'boolean', description: 'Rotate minimap with player.' },
        showObjectives: { type: 'boolean', description: 'Show objectives on minimap.' },
        crosshairStyle: {
          type: 'string',
          enum: ['dot', 'cross', 'circle', 'custom'],
          description: 'Crosshair style.'
        },
        crosshairSize: { type: 'number', description: 'Crosshair size.' },
        spreadMultiplier: { type: 'number', description: 'Crosshair spread multiplier.' },
        showDegrees: { type: 'boolean', description: 'Show compass degrees.' },
        showCardinals: { type: 'boolean', description: 'Show cardinal directions.' },
        promptFormat: { type: 'string', description: 'Interaction prompt format.' },
        showKeyIcon: { type: 'boolean', description: 'Show key icon in prompt.' },
        keyIconStyle: { type: 'string', description: 'Key icon style.' },
        maxVisibleObjectives: { type: 'number', description: 'Maximum visible objectives.' },
        showProgress: { type: 'boolean', description: 'Show objective progress.' },
        animateUpdates: { type: 'boolean', description: 'Animate objective updates.' },
        indicatorStyle: {
          type: 'string',
          enum: ['directional', 'vignette', 'both'],
          description: 'Damage indicator style.'
        },
        fadeTime: commonSchemas.fadeTime,
        gridSize: {
          type: 'object',
          properties: { columns: commonSchemas.numberProp, rows: commonSchemas.numberProp },
          description: 'Inventory grid size.'
        },
        slotSize: { type: 'number', description: 'Inventory slot size.' },
        showEquipment: { type: 'boolean', description: 'Show equipment panel.' },
        showDetails: { type: 'boolean', description: 'Show item details panel.' },
        showPortrait: { type: 'boolean', description: 'Show speaker portrait.' },
        showSpeakerName: { type: 'boolean', description: 'Show speaker name.' },
        choiceLayout: {
          type: 'string',
          enum: ['vertical', 'horizontal', 'radial'],
          description: 'Dialog choice layout.'
        },
        segmentCount: { type: 'number', description: 'Number of radial segments.' },
        innerRadius: { type: 'number', description: 'Inner radius of radial menu.' },
        outerRadius: { type: 'number', description: 'Outer radius of radial menu.' },
        showIcons: { type: 'boolean', description: 'Show icons in radial menu.' },
        showLabels: { type: 'boolean', description: 'Show labels in radial menu.' },
        previewSize: {
          type: 'string',
          enum: ['1080p', '720p', 'mobile', 'custom'],
          description: 'Preview resolution preset.'
        },
        customWidth: { type: 'number', description: 'Custom preview width.' },
        customHeight: { type: 'number', description: 'Custom preview height.' },
        // Wave 4.31-4.38: Widget Enhancement Properties
        templateName: { type: 'string', description: 'Widget template name.' },
        templateType: {
          type: 'string',
          enum: ['button', 'panel', 'list_item', 'dialog', 'tooltip', 'hud_element', 'menu_item', 'custom'],
          description: 'Type of widget template.'
        },
        templateConfig: {
          type: 'object',
          description: 'Template configuration settings.'
        },
        bindings: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              slot: commonSchemas.stringProp,
              property: commonSchemas.stringProp,
              source: commonSchemas.stringProp,
              bindingType: commonSchemas.stringProp
            }
          },
          description: 'Array of binding configurations for batch binding.'
        },
        animationDuration: { type: 'number', description: 'Animation duration in seconds.' },
        easing: {
          type: 'string',
          enum: ['Linear', 'EaseIn', 'EaseOut', 'EaseInOut', 'Cubic', 'Bounce', 'Elastic'],
          description: 'Animation easing function.'
        },
        animationProperties: {
          type: 'array',
          items: commonSchemas.stringProp,
          description: 'Properties to animate (position, opacity, scale, color, etc.).'
        },
        animationKeyframes: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              time: commonSchemas.numberProp,
              value: { type: 'object' }
            }
          },
          description: 'Animation keyframe data.'
        },
        navigationMap: {
          type: 'object',
          description: 'Navigation flow map between widgets.'
        },
        navigationMode: {
          type: 'string',
          enum: ['Explicit', 'Automatic', 'Custom', 'Escape'],
          description: 'Navigation mode.'
        },
        defaultFocus: { type: 'string', description: 'Default focused widget slot.' },
        wrapNavigation: { type: 'boolean', description: 'Wrap navigation at edges.' },
        accessibilityRules: {
          type: 'array',
          items: commonSchemas.stringProp,
          description: 'Accessibility rules to validate.'
        },
        wcagLevel: {
          type: 'string',
          enum: ['A', 'AA', 'AAA'],
          description: 'WCAG compliance level to check.'
        },
        checkContrast: { type: 'boolean', description: 'Check color contrast ratios.' },
        checkFocusOrder: { type: 'boolean', description: 'Check focus/tab order.' },
        checkTextSize: { type: 'boolean', description: 'Check minimum text sizes.' },
        checkScreenReader: { type: 'boolean', description: 'Check screen reader compatibility.' },
        hudLayoutName: { type: 'string', description: 'HUD layout preset name.' },
        hudZones: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              zone: commonSchemas.stringProp,
              widgets: commonSchemas.arrayOfStrings
            }
          },
          description: 'HUD zone assignments (top-left, top-center, bottom-left, etc.).'
        },
        safeZoneType: {
          type: 'string',
          enum: ['ActionSafe', 'TitleSafe', 'Custom'],
          description: 'Safe zone type preset.'
        },
        safeZonePadding: {
          type: 'object',
          properties: {
            left: commonSchemas.numberProp,
            top: commonSchemas.numberProp,
            right: commonSchemas.numberProp,
            bottom: commonSchemas.numberProp
          },
          description: 'Safe zone padding percentages.'
        },
        scalingRule: {
          type: 'string',
          enum: ['ShortestSide', 'LongestSide', 'Horizontal', 'Vertical', 'Custom'],
          description: 'DPI scaling rule.'
        },
        targetWidgets: {
          type: 'array',
          items: commonSchemas.stringProp,
          description: 'Widget slots to localize.'
        },
        localizationNamespace: { type: 'string', description: 'Localization namespace.' },
        localizationKey: { type: 'string', description: 'Localization key prefix.' },
        extractStrings: { type: 'boolean', description: 'Extract text strings for localization.' },
        updateBindings: { type: 'boolean', description: 'Update text bindings to use localization.' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: commonSchemas.booleanProp,
        message: commonSchemas.stringProp,
        widgetPath: commonSchemas.widgetPath,
        slotName: commonSchemas.slotName,
        animationName: commonSchemas.animationName,
        trackIndex: { type: 'number', description: 'Index of created track.' },
        keyframeIndex: { type: 'number', description: 'Index of created keyframe.' },
        bindingCreated: { type: 'boolean', description: 'Whether binding was created.' },
        widgetInfo: {
          type: 'object',
          properties: {
            widgetClass: commonSchemas.stringProp,
            parentClass: commonSchemas.stringProp,
            slots: commonSchemas.arrayOfStrings,
            animations: commonSchemas.arrayOfStrings,
            variables: commonSchemas.arrayOfStrings,
            functions: commonSchemas.arrayOfStrings,
            eventDispatchers: commonSchemas.arrayOfStrings
          },
          description: 'Widget info (for get_widget_info).'
        },
        error: commonSchemas.stringProp
      }
    }
  },
  {
    name: 'manage_networking',
    category: 'utility',
    description: 'Replication, RPCs, prediction, sessions; GameModes, teams.',
    annotations: {
      audience: ['developer'],
      priority: 7,
      tags: ['networking', 'replication', 'rpc', 'sessions', 'multiplayer']
    },
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'set_property_replicated',
            'set_replication_condition',
            'configure_net_update_frequency',
            'configure_net_priority',
            'set_net_dormancy',
            'configure_replication_graph',
            'create_rpc_function',
            'configure_rpc_validation',
            'set_rpc_reliability',
            'set_owner',
            'set_autonomous_proxy',
            'check_has_authority',
            'check_is_locally_controlled',
            'configure_net_cull_distance',
            'set_always_relevant',
            'set_only_relevant_to_owner',
            'configure_net_serialization',
            'set_replicated_using',
            'configure_push_model',
            'configure_client_prediction',
            'configure_server_correction',
            'add_network_prediction_data',
            'configure_movement_prediction',
            'configure_net_driver',
            'set_net_role',
            'configure_replicated_movement',
            'get_networking_info',
            // Wave 3.31-3.40: Networking System Actions
            'debug_replication_graph',
            'configure_net_relevancy',
            'get_rpc_statistics',
            'configure_prediction_settings',
            'simulate_network_conditions',
            'get_session_players',
            'configure_team_settings',
            'send_server_rpc',
            'get_net_role_info',
            'configure_dormancy',
            // Sessions (merged from manage_sessions - Phase 54)
            'configure_local_session_settings', 'configure_session_interface',
            'configure_split_screen', 'set_split_screen_type', 'add_local_player', 'remove_local_player',
            'configure_lan_play', 'host_lan_server', 'join_lan_server',
            'enable_voice_chat', 'configure_voice_settings', 'set_voice_channel',
            'mute_player', 'set_voice_attenuation', 'configure_push_to_talk',
            'get_sessions_info',
            // Game Framework (merged from manage_game_framework)
            'create_game_mode', 'create_game_state', 'create_player_controller',
            'create_player_state', 'create_game_instance', 'create_hud_class',
            'set_default_pawn_class', 'set_player_controller_class',
            'set_game_state_class', 'set_player_state_class', 'configure_game_rules',
            'setup_match_states', 'configure_round_system', 'configure_team_system',
            'configure_scoring_system', 'configure_spawn_system',
            'configure_player_start', 'set_respawn_rules', 'configure_spectating',
            'get_game_framework_info'
          ],
          description: 'Networking action to perform'
        },
        blueprintPath: commonSchemas.blueprintPath,
        actorName: commonSchemas.actorName,
        propertyName: commonSchemas.propertyName,
        replicated: { type: 'boolean', description: 'Whether property should be replicated.' },
        condition: {
          type: 'string',
          enum: [
            'COND_None',
            'COND_InitialOnly',
            'COND_OwnerOnly',
            'COND_SkipOwner',
            'COND_SimulatedOnly',
            'COND_AutonomousOnly',
            'COND_SimulatedOrPhysics',
            'COND_InitialOrOwner',
            'COND_Custom',
            'COND_ReplayOrOwner',
            'COND_ReplayOnly',
            'COND_SimulatedOnlyNoReplay',
            'COND_SimulatedOrPhysicsNoReplay',
            'COND_SkipReplay',
            'COND_Never'
          ],
          description: 'Replication condition.'
        },
        repNotifyFunc: commonSchemas.repNotifyFunc,
        netUpdateFrequency: { type: 'number', description: 'How often actor replicates (Hz, default 100).' },
        minNetUpdateFrequency: { type: 'number', description: 'Minimum update frequency when idle (Hz, default 2).' },
        netPriority: { type: 'number', description: 'Network priority for bandwidth (default 1.0).' },
        dormancy: {
          type: 'string',
          enum: [
            'DORM_Never',
            'DORM_Awake',
            'DORM_DormantAll',
            'DORM_DormantPartial',
            'DORM_Initial'
          ],
          description: 'Net dormancy mode.'
        },
        nodeClass: commonSchemas.nodeClass,
        spatialBias: { type: 'number', description: 'Spatial bias for replication graph.' },
        defaultSettingsClass: { type: 'string', description: 'Default replication settings class.' },
        functionName: commonSchemas.functionName,
        rpcType: {
          type: 'string',
          enum: ['Server', 'Client', 'NetMulticast'],
          description: 'Type of RPC.'
        },
        reliable: commonSchemas.reliable,
        parameters: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              name: commonSchemas.stringProp,
              type: commonSchemas.stringProp
            }
          },
          description: 'RPC function parameters.'
        },
        returnType: { type: 'string', description: 'RPC return type (usually void).' },
        validationFunctionName: { type: 'string', description: 'Name of validation function.' },
        withValidation: { type: 'boolean', description: 'Enable RPC validation.' },
        ownerActorName: { type: 'string', description: 'Name of owner actor (null to clear).' },
        isAutonomousProxy: { type: 'boolean', description: 'Configure as autonomous proxy.' },
        netCullDistanceSquared: { type: 'number', description: 'Network cull distance squared.' },
        useOwnerNetRelevancy: { type: 'boolean', description: 'Use owner relevancy.' },
        alwaysRelevant: { type: 'boolean', description: 'Always relevant to all clients.' },
        onlyRelevantToOwner: { type: 'boolean', description: 'Only relevant to owner.' },
        structName: { type: 'string', description: 'Name of struct for custom serialization.' },
        useNetSerialize: { type: 'boolean', description: 'Use custom NetSerialize.' },
        usePushModel: { type: 'boolean', description: 'Use push-model replication.' },
        propertyNames: {
          type: 'array',
          items: commonSchemas.stringProp,
          description: 'Properties for push model.'
        },
        enablePrediction: { type: 'boolean', description: 'Enable client-side prediction.' },
        predictionKey: { type: 'string', description: 'Prediction key identifier.' },
        correctionThreshold: { type: 'number', description: 'Server correction threshold.' },
        smoothingRate: { type: 'number', description: 'Smoothing rate for corrections.' },
        dataType: {
          type: 'string',
          enum: ['InputCmd', 'SyncState', 'AuxState'],
          description: 'Network prediction data type.'
        },
        properties: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              name: commonSchemas.stringProp,
              type: commonSchemas.stringProp
            }
          },
          description: 'Predicted properties.'
        },
        networkSmoothingMode: {
          type: 'string',
          enum: ['Disabled', 'Linear', 'Exponential'],
          description: 'Movement smoothing mode.'
        },
        networkMaxSmoothUpdateDistance: { type: 'number', description: 'Max smooth update distance.' },
        networkNoSmoothUpdateDistance: { type: 'number', description: 'No smooth update distance.' },
        maxClientRate: { type: 'number', description: 'Max client rate.' },
        maxInternetClientRate: { type: 'number', description: 'Max internet client rate.' },
        netServerMaxTickRate: { type: 'number', description: 'Server max tick rate.' },
        role: {
          type: 'string',
          enum: ['ROLE_None', 'ROLE_SimulatedProxy', 'ROLE_AutonomousProxy', 'ROLE_Authority'],
          description: 'Net role.'
        },
        replicateMovement: { type: 'boolean', description: 'Replicate movement.' },
        replicatedMovementMode: {
          type: 'string',
          enum: ['Default', 'SkipPhysics', 'FullMovement'],
          description: 'Replicated movement mode.'
        },
        locationQuantizationLevel: {
          type: 'string',
          enum: ['RoundWholeNumber', 'RoundOneDecimal', 'RoundTwoDecimals'],
          description: 'Location quantization level.'
        },
        save: commonSchemas.save,
        // Sessions properties (merged from manage_sessions - Phase 54)
        sessionName: commonSchemas.sessionName,
        sessionId: commonSchemas.sessionId,
        maxPlayers: commonSchemas.numberProp,
        bIsLANMatch: { type: 'boolean', description: 'Whether this is a LAN match.' },
        bAllowJoinInProgress: { type: 'boolean', description: 'Allow joining games in progress.' },
        splitScreenType: { type: 'string', enum: ['None', 'TwoPlayer_Horizontal', 'TwoPlayer_Vertical', 'ThreePlayer_FavorTop', 'ThreePlayer_FavorBottom', 'FourPlayer_Grid'] },
        playerIndex: commonSchemas.numberProp,
        controllerId: commonSchemas.numberProp,
        serverAddress: commonSchemas.serverAddress,
        serverPort: commonSchemas.numberProp,
        serverPassword: { type: 'string', description: 'Server password for protected games.' },
        serverName: { type: 'string', description: 'Display name for the server.' },
        mapName: { type: 'string', description: 'Map to load for hosting.' },
        voiceEnabled: { type: 'boolean', description: 'Enable/disable voice chat.' },
        voiceSettings: commonSchemas.objectProp,
        channelName: commonSchemas.channelName,
        channelType: { type: 'string', enum: ['Team', 'Global', 'Proximity', 'Party'] },
        targetPlayerId: { type: 'string', description: 'Target player ID.' },
        muted: commonSchemas.muted,
        attenuationRadius: { type: 'number', description: 'Radius for voice attenuation.' },
        pushToTalkEnabled: { type: 'boolean', description: 'Enable push-to-talk mode.' },
        pushToTalkKey: { type: 'string', description: 'Key binding for push-to-talk.' },
        // Wave 3.31-3.40: Networking System Actions input properties
        showConnections: { type: 'boolean', description: 'Show connection details in replication graph debug.' },
        showActorList: { type: 'boolean', description: 'Show actor list in replication graph debug.' },
        relevancyRadius: { type: 'number', description: 'Relevancy check radius.' },
        relevancyMode: { type: 'string', enum: ['Default', 'Custom', 'AlwaysRelevant', 'OwnerOnly', 'ConnectionOnly'], description: 'Relevancy mode.' },
        includeRpcDetails: { type: 'boolean', description: 'Include detailed RPC stats.' },
        resetStats: { type: 'boolean', description: 'Reset RPC statistics after fetch.' },
        predictionEnabled: { type: 'boolean', description: 'Enable network prediction.' },
        predictionAmount: { type: 'number', description: 'Prediction time amount in ms.' },
        interpolationEnabled: { type: 'boolean', description: 'Enable interpolation for smoothing.' },
        latencyMs: { type: 'number', description: 'Simulated latency in milliseconds.' },
        packetLoss: { type: 'number', description: 'Simulated packet loss percentage (0-100).' },
        jitterMs: { type: 'number', description: 'Simulated jitter in milliseconds.' },
        bandwidthLimit: { type: 'number', description: 'Simulated bandwidth limit in bytes/sec.' },
        includeInactive: { type: 'boolean', description: 'Include inactive players in list.' },
        teamId: { type: 'number', description: 'Team ID to assign.' },
        teamName: { type: 'string', description: 'Team name.' },
        autoBalance: { type: 'boolean', description: 'Enable auto team balancing.' },
        rpcName: { type: 'string', description: 'RPC function name to call.' },
        rpcParameters: { type: 'object', description: 'Parameters to pass to the RPC.' },
        targetActor: { type: 'string', description: 'Target actor for the RPC call.' },
        dormancyMode: { type: 'string', enum: ['DORM_Never', 'DORM_Awake', 'DORM_DormantAll', 'DORM_DormantPartial', 'DORM_Initial'], description: 'Dormancy mode to set.' },
        flushDormancy: { type: 'boolean', description: 'Flush dormancy state.' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: commonSchemas.booleanProp,
        message: commonSchemas.stringProp,
        blueprintPath: commonSchemas.blueprintPath,
        functionName: { type: 'string', description: 'Created RPC function name.' },
        hasAuthority: { type: 'boolean', description: 'Authority check result.' },
        isLocallyControlled: { type: 'boolean', description: 'Local control check result.' },
        role: { type: 'string', description: 'Current net role.' },
        remoteRole: { type: 'string', description: 'Current remote role.' },
        // Wave 3.31-3.40: Networking System Actions output properties
        replicationGraph: {
          type: 'object',
          properties: {
            nodes: { type: 'array' },
            connections: { type: 'array' },
            actorCount: commonSchemas.numberProp
          },
          description: 'Replication graph debug info.'
        },
        rpcStatistics: {
          type: 'object',
          properties: {
            totalCalls: commonSchemas.numberProp,
            callsByType: { type: 'object' },
            bandwidth: commonSchemas.numberProp
          },
          description: 'RPC statistics.'
        },
        sessionPlayers: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              playerId: commonSchemas.stringProp,
              playerName: commonSchemas.stringProp,
              teamId: commonSchemas.numberProp,
              isActive: commonSchemas.booleanProp
            }
          },
          description: 'List of session players.'
        },
        netRoleInfo: {
          type: 'object',
          properties: {
            role: commonSchemas.stringProp,
            remoteRole: commonSchemas.stringProp,
            isNetRelevant: commonSchemas.booleanProp,
            dormancyState: commonSchemas.stringProp
          },
          description: 'Network role information.'
        },
        networkingInfo: {
          type: 'object',
          properties: {
            bReplicates: commonSchemas.booleanProp,
            bAlwaysRelevant: commonSchemas.booleanProp,
            bOnlyRelevantToOwner: commonSchemas.booleanProp,
            netUpdateFrequency: commonSchemas.numberProp,
            minNetUpdateFrequency: commonSchemas.numberProp,
            netPriority: commonSchemas.numberProp,
            netDormancy: commonSchemas.stringProp,
            netCullDistanceSquared: commonSchemas.numberProp,
            replicatedProperties: {
              type: 'array',
              items: {
                type: 'object',
                properties: {
                  name: commonSchemas.stringProp,
                  condition: commonSchemas.stringProp,
                  repNotifyFunc: commonSchemas.stringProp
                }
              }
            },
            rpcFunctions: {
              type: 'array',
              items: {
                type: 'object',
                properties: {
                  name: commonSchemas.stringProp,
                  type: commonSchemas.stringProp,
                  reliable: commonSchemas.booleanProp
                }
              }
            }
          },
          description: 'Networking info (for get_networking_info).'
        },
        error: commonSchemas.stringProp
      }
    }
  },
  // [MERGED] manage_game_framework actions now in manage_networking
  // [MERGED] manage_sessions actions now in manage_networking (Phase 54: Strategic Tool Merging)
  // [MERGED] manage_level_structure actions now in manage_level
  {
    name: 'manage_volumes',
    category: 'world',
    description: 'Volumes (trigger, physics, audio, nav) and splines (meshes).',
    annotations: {
      audience: ['developer', 'level-designer'],
      priority: 6,
      tags: ['volume', 'trigger', 'spline', 'navigation', 'blocking']
    },
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'create_trigger_volume', 'create_trigger_box', 'create_trigger_sphere', 'create_trigger_capsule',
            'create_blocking_volume', 'create_kill_z_volume', 'create_pain_causing_volume', 'create_physics_volume',
            'create_audio_volume', 'create_reverb_volume',
            'create_cull_distance_volume', 'create_precomputed_visibility_volume', 'create_lightmass_importance_volume',
            'create_nav_mesh_bounds_volume', 'create_nav_modifier_volume', 'create_camera_blocking_volume',
            'set_volume_extent', 'set_volume_properties',
            'get_volumes_info',
            // Splines (merged from manage_splines)
            'create_spline_actor', 'add_spline_point', 'remove_spline_point',
            'set_spline_point_position', 'set_spline_point_tangents',
            'set_spline_point_rotation', 'set_spline_point_scale', 'set_spline_type',
            'create_spline_mesh_component', 'set_spline_mesh_asset',
            'configure_spline_mesh_axis', 'set_spline_mesh_material',
            'scatter_meshes_along_spline', 'configure_mesh_spacing', 'configure_mesh_randomization',
            'create_road_spline', 'create_river_spline', 'create_fence_spline',
            'create_wall_spline', 'create_cable_spline', 'create_pipe_spline',
            'get_splines_info'
          ],
          description: 'Volume action to perform'
        },
        volumeName: commonSchemas.volumeName,
        volumePath: commonSchemas.volumePath,
        location: {
          type: 'object',
          properties: {
            x: commonSchemas.numberProp, y: commonSchemas.numberProp, z: commonSchemas.numberProp
          },
          description: 'World location for the volume.'
        },
        rotation: {
          type: 'object',
          properties: {
            pitch: commonSchemas.numberProp, yaw: commonSchemas.numberProp, roll: commonSchemas.numberProp
          },
          description: 'Rotation of the volume.'
        },
        extent: {
          type: 'object',
          properties: {
            x: commonSchemas.numberProp, y: commonSchemas.numberProp, z: commonSchemas.numberProp
          },
          description: 'Extent (half-size) of the volume in each axis.'
        },
        sphereRadius: { type: 'number', description: 'Radius for sphere trigger volumes.' },
        capsuleRadius: { type: 'number', description: 'Radius for capsule trigger volumes.' },
        capsuleHalfHeight: { type: 'number', description: 'Half-height for capsule trigger volumes.' },
        boxExtent: {
          type: 'object',
          properties: {
            x: commonSchemas.numberProp, y: commonSchemas.numberProp, z: commonSchemas.numberProp
          },
          description: 'Extent for box trigger volumes.'
        },
        bPainCausing: { type: 'boolean', description: 'Whether the volume causes pain/damage.' },
        damagePerSec: { type: 'number', description: 'Damage per second for pain volumes.' },
        damageType: { type: 'string', description: 'Damage type class path for pain volumes.' },
        bWaterVolume: { type: 'boolean', description: 'Whether this is a water volume.' },
        fluidFriction: { type: 'number', description: 'Fluid friction for physics volumes.' },
        terminalVelocity: { type: 'number', description: 'Terminal velocity in the volume.' },
        priority: commonSchemas.priority,
        bEnabled: { type: 'boolean', description: 'Whether the audio volume is enabled.' },
        reverbEffect: { type: 'string', description: 'Reverb effect asset path.' },
        reverbVolume: { type: 'number', description: 'Volume level for reverb (0.0-1.0).' },
        fadeTime: commonSchemas.fadeTime,
        cullDistances: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              size: { type: 'number', description: 'Object size threshold.' },
              cullDistance: { type: 'number', description: 'Distance at which to cull.' }
            }
          },
          description: 'Array of size/distance pairs for cull distance volumes.'
        },
        areaClass: commonSchemas.areaClass,
        bDynamicModifier: { type: 'boolean', description: 'Whether nav modifier updates dynamically.' },
        bUnbound: { type: 'boolean', description: 'Whether post process volume affects entire world.' },
        blendRadius: { type: 'number', description: 'Blend radius for post process volume.' },
        blendWeight: { type: 'number', description: 'Blend weight (0.0-1.0) for post process.' },
        properties: {
          type: 'object',
          description: 'Additional volume-specific properties as key-value pairs.'
        },
        filter: commonSchemas.filter,
        volumeType: { type: 'string', description: 'Type filter for get_volumes_info (e.g., "Trigger", "Physics").' },
        save: commonSchemas.save,
        // Splines (merged from manage_splines)
        actorName: commonSchemas.actorName,
        actorPath: commonSchemas.actorPath,
        splineName: commonSchemas.stringProp,
        componentName: commonSchemas.componentName,
        blueprintPath: commonSchemas.blueprintPath,
        pointIndex: commonSchemas.numberProp,
        position: commonSchemas.vector3,
        arriveTangent: commonSchemas.vector3,
        leaveTangent: commonSchemas.vector3,
        tangent: commonSchemas.vector3,
        coordinateSpace: { type: 'string', enum: ['Local', 'World'] },
        splineType: { type: 'string', enum: ['Linear', 'Curve', 'Constant', 'CurveClamped', 'CurveCustomTangent'] },
        bClosedLoop: commonSchemas.booleanProp,
        meshPath: commonSchemas.meshPath,
        materialPath: commonSchemas.materialPath,
        forwardAxis: { type: 'string', enum: ['X', 'Y', 'Z'] },
        spacing: commonSchemas.numberProp,
        templateType: { type: 'string', enum: ['road', 'river', 'fence', 'wall', 'cable', 'pipe'] },
        width: commonSchemas.width,
        points: { type: 'array', items: { type: 'object' }, description: 'Spline points array.' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: commonSchemas.booleanProp,
        message: commonSchemas.stringProp,
        volumeName: { type: 'string', description: 'Name of created/modified volume.' },
        volumePath: commonSchemas.volumePath,
        volumeClass: { type: 'string', description: 'Class of the volume.' },
        // Spline outputs
        pointCount: commonSchemas.numberProp,
        splineLength: commonSchemas.numberProp,
        scatteredMeshes: commonSchemas.numberProp,
        error: commonSchemas.stringProp
      }
    }
  },
  // [MERGED] manage_navigation actions now in manage_ai (Phase 54: Strategic Tool Merging)
  // [MERGED] manage_splines actions now in manage_volumes
  // [MERGED] manage_pcg actions now in manage_level
  // [MERGED] manage_post_process actions now in manage_lighting
  // ============================================================================
  // Phase 30: Cinematics & Media
  // ============================================================================
  // NOTE: manage_sequencer and manage_movie_render have been merged into manage_sequence
  // [MERGED] manage_media actions now in manage_skeleton
  // Phase 31: Data & Persistence
  {
    name: 'manage_data',
    category: 'utility',
    description: 'Data assets, tables, save games, tags, config; modding/PAK/UGC.',
    annotations: {
      audience: ['developer'],
      priority: 6,
      tags: ['data', 'save-game', 'data-table', 'config', 'modding']
    },
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            // Data Assets
            'create_data_asset', 'create_primary_data_asset', 'get_data_asset_info', 'set_data_asset_property',
            // Data Tables
            'create_data_table', 'add_data_table_row', 'remove_data_table_row', 'get_data_table_row',
            'get_data_table_rows', 'import_data_table_csv', 'export_data_table_csv', 'empty_data_table',
            // Curve Tables
            'create_curve_table', 'add_curve_row', 'get_curve_value', 'import_curve_table_csv', 'export_curve_table_csv',
            // Save Game
            'create_save_game_blueprint', 'save_game_to_slot', 'load_game_from_slot', 'delete_save_slot',
            'does_save_exist', 'get_save_slot_names',
            // Gameplay Tags
            'create_gameplay_tag', 'add_native_gameplay_tag', 'request_gameplay_tag', 'check_tag_match',
            'create_tag_container', 'add_tag_to_container', 'remove_tag_from_container', 'has_tag',
            'get_all_gameplay_tags',
            // Config
            'read_config_value', 'write_config_value', 'get_config_section', 'flush_config', 'reload_config',
            // Modding/UGC (merged from manage_modding)
            'configure_mod_loading_paths', 'scan_for_mod_paks', 'load_mod_pak', 'unload_mod_pak',
            'validate_mod_pak', 'configure_mod_load_order', 'list_installed_mods', 'enable_mod', 'disable_mod',
            'check_mod_compatibility', 'get_mod_info', 'configure_asset_override_paths', 'register_mod_asset_redirect',
            'restore_original_asset', 'list_asset_overrides', 'export_moddable_headers', 'create_mod_template_project',
            'configure_exposed_classes', 'get_sdk_config', 'configure_mod_sandbox', 'set_allowed_mod_operations',
            'validate_mod_content', 'get_security_config', 'get_modding_info', 'reset_mod_system'
          ],
          description: 'Data action to perform.'
        },
        // Data Asset parameters
        assetPath: commonSchemas.assetPath,
        assetName: commonSchemas.name,
        savePath: commonSchemas.savePath,
        dataAssetClass: { type: 'string', description: 'Class path for data asset (e.g., /Script/Engine.DataAsset).' },
        primaryAssetType: { type: 'string', description: 'Primary asset type name for PrimaryDataAsset.' },
        primaryAssetName: { type: 'string', description: 'Primary asset name for PrimaryDataAsset.' },
        properties: commonSchemas.objectProp,
        propertyName: commonSchemas.propertyName,
        propertyValue: commonSchemas.value,

        // Data Table parameters
        dataTablePath: { type: 'string', description: 'Path to data table asset.' },
        rowStructPath: { type: 'string', description: 'Path to row struct (e.g., /Script/MyProject.FMyRowStruct).' },
        rowName: { type: 'string', description: 'Name of the row in data table.' },
        rowData: { type: 'object', description: 'Row data as JSON object matching the row struct.' },
        csvString: { type: 'string', description: 'CSV string for import/export.' },
        csvFilePath: { type: 'string', description: 'File path for CSV import/export.' },

        // Curve Table parameters
        curveTablePath: { type: 'string', description: 'Path to curve table asset.' },
        curveName: commonSchemas.curveName,
        curveType: { type: 'string', enum: ['RichCurve', 'SimpleCurve'], description: 'Type of curve to create.' },
        keyTime: { type: 'number', description: 'Time value for curve key.' },
        keyValue: { type: 'number', description: 'Value at curve key.' },
        interpMode: { type: 'string', enum: ['Linear', 'Constant', 'Cubic', 'Auto'], description: 'Interpolation mode for curve.' },

        // Save Game parameters
        slotName: commonSchemas.slotName,
        userIndex: { type: 'number', description: 'User index for save game (default 0).' },
        saveGameClass: { type: 'string', description: 'Class path for custom SaveGame class.' },
        saveData: { type: 'object', description: 'Data to save (serializable properties).' },

        // Gameplay Tag parameters
        tagName: commonSchemas.tagName,
        tagString: { type: 'string', description: 'Full tag string (e.g., Character.State.Dead).' },
        tagDevComment: { type: 'string', description: 'Developer comment for native tag registration.' },
        tagToCheck: { type: 'string', description: 'Tag to check for matching.' },
        containerName: { type: 'string', description: 'Name identifier for tag container.' },
        exactMatch: { type: 'boolean', description: 'Use exact tag matching instead of hierarchical.' },
        tags: { type: 'array', items: { type: 'string' }, description: 'Array of tag strings.' },

        // Config parameters
        configName: { type: 'string', description: 'Config file base name (e.g., Engine, Game, Input).' },
        configSection: { type: 'string', description: 'Config section name (e.g., /Script/Engine.Engine).' },
        configKey: { type: 'string', description: 'Config key name.' },
        configValue: { type: 'string', description: 'Config value to set.' },

        // Common
        save: commonSchemas.save
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        ...commonSchemas.outputBase,
        assetPath: commonSchemas.stringProp,
        dataAssetInfo: {
          type: 'object',
          properties: {
            className: commonSchemas.stringProp,
            primaryAssetId: commonSchemas.stringProp,
            properties: commonSchemas.objectProp
          }
        },
        rows: commonSchemas.arrayOfObjects,
        rowData: commonSchemas.objectProp,
        csvData: commonSchemas.stringProp,
        curveValue: commonSchemas.numberProp,
        saveExists: commonSchemas.booleanProp,
        slotNames: commonSchemas.arrayOfStrings,
        loadedData: commonSchemas.objectProp,
        tag: commonSchemas.stringProp,
        tagValid: commonSchemas.booleanProp,
        tagMatches: commonSchemas.booleanProp,
        containerTags: commonSchemas.arrayOfStrings,
        allTags: commonSchemas.arrayOfStrings,
        configValue: commonSchemas.stringProp,
        sectionData: commonSchemas.objectProp
      }
    }
  },
  // Phase 32: Build & Deployment
  {
    name: 'manage_build',
    category: 'utility',
    description: 'UBT, cook/package, plugins, DDC; tests, profiling, validation.',
    annotations: {
      audience: ['developer'],
      priority: 6,
      tags: ['build', 'cook', 'package', 'plugin', 'ddc', 'testing']
    },
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            // Pipeline (7 actions)
            'run_ubt', 'generate_project_files', 'compile_shaders', 'cook_content',
            'package_project', 'configure_build_settings', 'get_build_info',
            // Platform Configuration (3 actions)
            'configure_platform', 'get_platform_settings', 'get_target_platforms',
            // Asset Validation & Auditing (4 actions)
            'validate_assets', 'audit_assets', 'get_asset_size_info', 'get_asset_references',
            // PAK & Chunking (3 actions)
            'configure_chunking', 'create_pak_file', 'configure_encryption',
            // Plugin Management (4 actions)
            'list_plugins', 'enable_plugin', 'disable_plugin', 'get_plugin_info',
            // DDC Management (3 actions)
            'clear_ddc', 'get_ddc_stats', 'configure_ddc',
            // Testing (merged from manage_testing)
            'list_tests', 'run_tests', 'run_test', 'get_test_results', 'get_test_info',
            'list_functional_tests', 'run_functional_test', 'get_functional_test_results',
            'start_trace', 'stop_trace', 'get_trace_status',
            'enable_visual_logger', 'disable_visual_logger', 'get_visual_logger_status',
            'start_stats_capture', 'stop_stats_capture',
            'get_memory_report', 'get_performance_stats',
            'validate_asset', 'validate_assets_in_path', 'validate_blueprint',
            'check_map_errors', 'fix_redirectors', 'get_redirectors'
          ],
          description: 'Build action to perform.'
        },
        // Pipeline parameters
        target: { type: 'string', description: 'Build target name (e.g., MyProjectEditor).' },
        platform: { type: 'string', enum: ['Win64', 'Linux', 'Mac', 'Android', 'IOS'], description: 'Target platform.' },
        configuration: { type: 'string', enum: ['Debug', 'DebugGame', 'Development', 'Shipping', 'Test'], description: 'Build configuration.' },
        ubtArgs: { type: 'string', description: 'Additional arguments to pass to UBT.' },
        clean: { type: 'boolean', description: 'Perform a clean build.' },

        // Cook/Package parameters
        cookFlavor: { type: 'string', description: 'Cook flavor (e.g., ASTC, ETC2).' },
        iterativeCook: { type: 'boolean', description: 'Enable iterative cooking.' },
        compressContent: { type: 'boolean', description: 'Compress cooked content.' },
        outputDirectory: commonSchemas.outputPath,
        stagingDirectory: { type: 'string', description: 'Staging directory for packaging.' },
        maps: { type: 'array', items: { type: 'string' }, description: 'List of maps to cook.' },

        // Build settings
        settingName: { type: 'string', description: 'Build setting name.' },
        settingValue: { type: 'string', description: 'Build setting value.' },

        // Asset validation parameters
        assetPaths: { type: 'array', items: { type: 'string' }, description: 'Array of asset paths to validate/audit.' },
        assetPath: commonSchemas.assetPath,
        validateOnSave: { type: 'boolean', description: 'Enable validation on asset save.' },
        validationRules: { type: 'array', items: { type: 'string' }, description: 'Validation rules to apply.' },

        // Chunking parameters
        chunkId: { type: 'number', description: 'Chunk ID for asset assignment.' },
        chunkAssets: { type: 'array', items: { type: 'string' }, description: 'Assets to assign to chunk.' },

        // PAK parameters
        pakFilePath: { type: 'string', description: 'Output PAK file path.' },
        pakContentPaths: { type: 'array', items: { type: 'string' }, description: 'Content paths to include in PAK.' },
        signPak: { type: 'boolean', description: 'Sign the PAK file.' },
        encryptPak: { type: 'boolean', description: 'Encrypt the PAK file.' },
        encryptionKey: { type: 'string', description: 'Encryption key (AES-256 base64).' },

        // Plugin parameters
        pluginName: { type: 'string', description: 'Plugin name.' },
        pluginCategory: { type: 'string', description: 'Filter plugins by category.' },
        includeEngine: { type: 'boolean', description: 'Include engine plugins in list.' },
        includeProject: { type: 'boolean', description: 'Include project plugins in list.' },

        // DDC parameters
        ddcBackend: { type: 'string', description: 'DDC backend name (Local, Shared, etc.).' },
        ddcPath: { type: 'string', description: 'DDC storage path.' },
        clearLocal: { type: 'boolean', description: 'Clear local DDC.' },
        clearShared: { type: 'boolean', description: 'Clear shared DDC.' },

        // Common
        save: commonSchemas.save
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        ...commonSchemas.outputBase,
        // Pipeline results
        exitCode: commonSchemas.numberProp,
        buildLog: commonSchemas.stringProp,
        buildTime: commonSchemas.numberProp,
        shadersCompiled: commonSchemas.numberProp,
        shadersRemaining: commonSchemas.numberProp,

        // Platform results
        platforms: commonSchemas.arrayOfStrings,
        platformSettings: commonSchemas.objectProp,

        // Validation results
        validationResults: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              assetPath: commonSchemas.stringProp,
              isValid: commonSchemas.booleanProp,
              errors: commonSchemas.arrayOfStrings,
              warnings: commonSchemas.arrayOfStrings
            }
          }
        },
        assetSizeInfo: {
          type: 'object',
          properties: {
            totalSize: commonSchemas.numberProp,
            diskSize: commonSchemas.numberProp,
            memorySize: commonSchemas.numberProp
          }
        },
        assetReferences: commonSchemas.arrayOfStrings,
        assetDependencies: commonSchemas.arrayOfStrings,

        // Chunking results
        chunkAssignments: commonSchemas.objectProp,

        // PAK results
        pakFilePath: commonSchemas.stringProp,
        pakFileSize: commonSchemas.numberProp,

        // Plugin results
        plugins: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              name: commonSchemas.stringProp,
              friendlyName: commonSchemas.stringProp,
              description: commonSchemas.stringProp,
              category: commonSchemas.stringProp,
              version: commonSchemas.stringProp,
              isEnabled: commonSchemas.booleanProp,
              isEnginePlugin: commonSchemas.booleanProp,
              canContainContent: commonSchemas.booleanProp,
              installedPath: commonSchemas.stringProp
            }
          }
        },
        pluginInfo: commonSchemas.objectProp,

        // DDC results
        ddcStats: {
          type: 'object',
          properties: {
            hitRate: commonSchemas.numberProp,
            totalRequests: commonSchemas.numberProp,
            cacheSize: commonSchemas.numberProp,
            localSize: commonSchemas.numberProp,
            sharedSize: commonSchemas.numberProp
          }
        }
      }
    }
  },
  // [MERGED] manage_testing actions now in manage_build
  
  // ===== PHASE 34: EDITOR UTILITIES =====
  {
    name: 'manage_editor_utilities',
    description: 'Editor modes, content browser, selection, collision, subsystems.',
    annotations: {
      audience: ['developer'],
      priority: 5,
      tags: ['editor', 'selection', 'collision', 'subsystem', 'timer']
    },
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            // Editor Modes
            'set_editor_mode', 'configure_editor_preferences', 'set_grid_settings', 'set_snap_settings',
            // Content Browser
            'navigate_to_path', 'sync_to_asset', 'create_collection', 'add_to_collection', 'show_in_explorer',
            // Selection
            'select_actor', 'select_actors_by_class', 'select_actors_by_tag', 'deselect_all', 
            'group_actors', 'ungroup_actors', 'get_selected_actors',
            // Collision
            'create_collision_channel', 'create_collision_profile', 'configure_channel_responses', 'get_collision_info',
            // Physical Materials
            'create_physical_material', 'set_friction', 'set_restitution', 'configure_surface_type', 'get_physical_material_info',
            // Subsystems
            'create_game_instance_subsystem', 'create_world_subsystem', 'create_local_player_subsystem', 'get_subsystem_info',
            // Timers
            'set_timer', 'clear_timer', 'clear_all_timers', 'get_active_timers',
            // Delegates
            'create_event_dispatcher', 'bind_to_event', 'unbind_from_event', 'broadcast_event', 'create_blueprint_interface',
            // Transactions
            'begin_transaction', 'end_transaction', 'cancel_transaction', 'undo', 'redo', 'get_transaction_history',
            // Utility
            'get_editor_utilities_info'
          ],
          description: 'Editor utility action.'
        },
        
        // Editor Mode parameters
        modeName: { type: 'string', description: 'Editor mode name (Default, Landscape, Foliage, Mesh, etc.).' },
        category: { type: 'string', description: 'Preference category.' },
        preferences: { type: 'object', description: 'Preference key-value pairs.' },
        gridSize: { type: 'number', description: 'Grid size in units.' },
        rotationSnap: { type: 'number', description: 'Rotation snap in degrees.' },
        scaleSnap: { type: 'number', description: 'Scale snap increment.' },
        
        // Content Browser parameters
        path: { type: 'string', description: 'Content browser path.' },
        assetPath: commonSchemas.assetPath,
        collectionName: { type: 'string', description: 'Collection name.' },
        collectionType: { type: 'string', enum: ['Local', 'Private', 'Shared'], description: 'Collection type.' },
        assetPaths: { type: 'array', items: { type: 'string' }, description: 'Array of asset paths.' },
        
        // Selection parameters
        actorName: commonSchemas.actorName,
        className: { type: 'string', description: 'Actor class name.' },
        tag: { type: 'string', description: 'Actor tag.' },
        addToSelection: { type: 'boolean', description: 'Add to existing selection instead of replacing.' },
        groupName: { type: 'string', description: 'Group name for actor grouping.' },
        
        // Collision parameters
        channelName: { type: 'string', description: 'Collision channel name.' },
        channelType: { type: 'string', enum: ['Object', 'Trace'], description: 'Channel type.' },
        profileName: { type: 'string', description: 'Collision profile name.' },
        collisionEnabled: { type: 'boolean', description: 'Enable/disable collision.' },
        objectType: { type: 'string', description: 'Object type for collision.' },
        responses: { type: 'object', description: 'Channel response mappings (channel -> Ignore/Overlap/Block).' },
        
        // Physical Material parameters
        materialName: { type: 'string', description: 'Physical material name.' },
        friction: { type: 'number', description: 'Friction coefficient (0-1).' },
        staticFriction: { type: 'number', description: 'Static friction coefficient.' },
        restitution: { type: 'number', description: 'Restitution/bounciness (0-1).' },
        density: { type: 'number', description: 'Material density.' },
        surfaceType: { type: 'string', description: 'Surface type (Default, Grass, Metal, Wood, etc.).' },
        
        // Subsystem parameters
        subsystemClass: { type: 'string', description: 'Subsystem class name.' },
        parentClass: { type: 'string', description: 'Parent class for subsystem.' },
        
        // Timer parameters
        timerHandle: { type: 'string', description: 'Timer handle identifier.' },
        duration: { type: 'number', description: 'Timer duration in seconds.' },
        looping: { type: 'boolean', description: 'Whether timer loops.' },
        functionName: { type: 'string', description: 'Function to call.' },
        targetActor: { type: 'string', description: 'Target actor for timer.' },
        
        // Delegate parameters
        dispatcherName: { type: 'string', description: 'Event dispatcher name.' },
        eventName: { type: 'string', description: 'Event name.' },
        blueprintPath: commonSchemas.blueprintPath,
        interfaceName: { type: 'string', description: 'Blueprint interface name.' },
        functions: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              name: { type: 'string' },
              inputs: { type: 'array', items: { type: 'object' } },
              outputs: { type: 'array', items: { type: 'object' } }
            }
          },
          description: 'Interface function definitions.'
        },
        
        // Transaction parameters
        transactionName: { type: 'string', description: 'Transaction name for undo/redo.' },
        transactionId: { type: 'string', description: 'Transaction identifier.' },
        
        // Common
        save: commonSchemas.save
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        ...commonSchemas.outputBase,
        
        // Editor mode info
        currentMode: commonSchemas.stringProp,
        availableModes: commonSchemas.arrayOfStrings,
        
        // Grid/snap settings
        gridSettings: {
          type: 'object',
          properties: {
            gridSize: commonSchemas.numberProp,
            rotationSnap: commonSchemas.numberProp,
            scaleSnap: commonSchemas.numberProp,
            gridEnabled: commonSchemas.booleanProp
          }
        },
        
        // Selection results
        selectedActors: commonSchemas.arrayOfStrings,
        selectionCount: commonSchemas.numberProp,
        
        // Collision info
        collisionChannels: commonSchemas.arrayOfStrings,
        collisionProfiles: commonSchemas.arrayOfStrings,
        
        // Physical material info
        physicalMaterialInfo: {
          type: 'object',
          properties: {
            friction: commonSchemas.numberProp,
            restitution: commonSchemas.numberProp,
            density: commonSchemas.numberProp,
            surfaceType: commonSchemas.stringProp
          }
        },
        
        // Subsystem info
        subsystems: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              className: commonSchemas.stringProp,
              type: commonSchemas.stringProp
            }
          }
        },
        
        // Timer info
        timerHandle: commonSchemas.stringProp,
        activeTimers: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              handle: commonSchemas.stringProp,
              functionName: commonSchemas.stringProp,
              remainingTime: commonSchemas.numberProp,
              looping: commonSchemas.booleanProp
            }
          }
        },
        
        // Transaction info
        transactionId: commonSchemas.stringProp,
        transactionHistory: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              id: commonSchemas.stringProp,
              name: commonSchemas.stringProp,
              timestamp: commonSchemas.stringProp
            }
          }
        },
        canUndo: commonSchemas.booleanProp,
        canRedo: commonSchemas.booleanProp
      }
    }
  },
  
  // ==================== PHASE 35: ADDITIONAL GAMEPLAY SYSTEMS ====================
  {
    name: 'manage_gameplay_systems',
    category: 'gameplay',
    description: 'Targeting, checkpoints, objectives, photo mode, dialogue, HLOD.',
    annotations: {
      audience: ['developer', 'designer'],
      priority: 7,
      tags: ['targeting', 'checkpoint', 'dialogue', 'photo-mode', 'hlod']
    },
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            // Targeting
            'create_targeting_component', 'configure_lock_on_target', 'configure_aim_assist',
            // Checkpoints
            'create_checkpoint_actor', 'save_checkpoint', 'load_checkpoint',
            // Objectives
            'create_objective', 'set_objective_state', 'configure_objective_markers',
            // World Markers
            'create_world_marker', 'create_ping_system', 'configure_marker_widget',
            // Photo Mode
            'enable_photo_mode', 'configure_photo_mode_camera', 'take_photo_mode_screenshot',
            // Quest/Dialogue
            'create_quest_data_asset', 'create_dialogue_tree', 'add_dialogue_node', 'play_dialogue',
            // Instancing
            'create_instanced_static_mesh_component', 'create_hierarchical_instanced_static_mesh',
            'add_instance', 'remove_instance', 'get_instance_count',
            // HLOD
            'create_hlod_layer', 'configure_hlod_settings', 'build_hlod', 'assign_actor_to_hlod',
            // Localization
            'create_string_table', 'add_string_entry', 'get_string_entry',
            'import_localization', 'export_localization', 'set_culture', 'get_available_cultures',
            // Scalability
            'create_device_profile', 'configure_scalability_group', 'set_quality_level',
            'get_scalability_settings', 'set_resolution_scale',
            // Utility
            'get_gameplay_systems_info',
            // Wave 3.41-3.50: Additional Gameplay Actions
            'create_objective_chain', 'configure_checkpoint_data', 'create_dialogue_node',
            'configure_targeting_priority', 'configure_localization_entry', 'create_quest_stage',
            'configure_minimap_icon', 'set_game_state', 'configure_save_system'
          ],
          description: 'Gameplay systems action to perform.'
        },
        
        // Targeting parameters
        actorName: commonSchemas.actorName,
        componentName: { type: 'string', description: 'Component name.' },
        maxTargetingRange: { type: 'number', description: 'Max targeting range.' },
        targetingConeAngle: { type: 'number', description: 'Targeting cone angle in degrees.' },
        autoTargetNearest: { type: 'boolean', description: 'Auto-target nearest valid target.' },
        lockOnRange: { type: 'number', description: 'Lock-on range.' },
        lockOnAngle: { type: 'number', description: 'Lock-on cone angle.' },
        breakLockOnDistance: { type: 'number', description: 'Distance to break lock-on.' },
        stickyLockOn: { type: 'boolean', description: 'Whether lock-on is sticky.' },
        lockOnSpeed: { type: 'number', description: 'Lock-on camera speed.' },
        aimAssistStrength: { type: 'number', description: 'Aim assist strength (0-1).' },
        aimAssistRadius: { type: 'number', description: 'Aim assist radius.' },
        magnetismStrength: { type: 'number', description: 'Bullet magnetism strength.' },
        bulletMagnetism: { type: 'boolean', description: 'Enable bullet magnetism.' },
        frictionZoneScale: { type: 'number', description: 'Aim friction zone scale.' },
        
        // Checkpoint parameters
        location: commonSchemas.location,
        rotation: commonSchemas.rotation,
        checkpointId: { type: 'string', description: 'Unique checkpoint identifier.' },
        autoActivate: { type: 'boolean', description: 'Auto-activate on overlap.' },
        triggerRadius: { type: 'number', description: 'Trigger volume radius.' },
        slotName: { type: 'string', description: 'Save slot name.' },
        playerIndex: { type: 'number', description: 'Player index.' },
        saveWorldState: { type: 'boolean', description: 'Save world state with checkpoint.' },
        
        // Objective parameters
        objectiveId: { type: 'string', description: 'Unique objective identifier.' },
        objectiveName: { type: 'string', description: 'Objective display name.' },
        description: { type: 'string', description: 'Objective description.' },
        objectiveType: { type: 'string', enum: ['Primary', 'Secondary', 'Optional'], description: 'Objective type.' },
        initialState: { type: 'string', enum: ['Inactive', 'Active', 'Completed', 'Failed'], description: 'Initial state.' },
        parentObjectiveId: { type: 'string', description: 'Parent objective ID for sub-objectives.' },
        trackingType: { type: 'string', enum: ['None', 'Count', 'Boolean', 'Custom'], description: 'Progress tracking type.' },
        state: { type: 'string', description: 'Objective state.' },
        progress: { type: 'number', description: 'Objective progress (0-1).' },
        notify: { type: 'boolean', description: 'Show notification on state change.' },
        showOnCompass: { type: 'boolean', description: 'Show on compass.' },
        showOnMap: { type: 'boolean', description: 'Show on map.' },
        showInWorld: { type: 'boolean', description: 'Show in world as 3D marker.' },
        markerIcon: { type: 'string', description: 'Marker icon path.' },
        markerColor: { type: 'object', properties: { r: { type: 'number' }, g: { type: 'number' }, b: { type: 'number' }, a: { type: 'number' } }, description: 'Marker color.' },
        distanceDisplay: { type: 'boolean', description: 'Show distance to objective.' },
        
        // World Marker parameters
        markerId: { type: 'string', description: 'Unique marker identifier.' },
        markerType: { type: 'string', enum: ['Generic', 'Enemy', 'Loot', 'Objective', 'Custom'], description: 'Marker type.' },
        iconPath: { type: 'string', description: 'Icon texture path.' },
        label: { type: 'string', description: 'Marker label text.' },
        color: { type: 'object', properties: { r: { type: 'number' }, g: { type: 'number' }, b: { type: 'number' }, a: { type: 'number' } }, description: 'Marker color.' },
        lifetime: { type: 'number', description: 'Marker lifetime (0 = persistent).' },
        visibleRange: { type: 'number', description: 'Visible range (0 = always).' },
        maxPingsPerPlayer: { type: 'number', description: 'Max pings per player.' },
        pingLifetime: { type: 'number', description: 'Ping lifetime in seconds.' },
        pingCooldown: { type: 'number', description: 'Ping cooldown in seconds.' },
        replicatedPings: { type: 'boolean', description: 'Replicate pings to other players.' },
        contextualPings: { type: 'boolean', description: 'Enable contextual ping types.' },
        widgetClass: { type: 'string', description: 'Widget class path.' },
        clampToScreen: { type: 'boolean', description: 'Clamp marker to screen edge.' },
        fadeWithDistance: { type: 'boolean', description: 'Fade marker with distance.' },
        fadeStartDistance: { type: 'number', description: 'Distance to start fading.' },
        fadeEndDistance: { type: 'number', description: 'Distance to fully fade.' },
        scaleWithDistance: { type: 'boolean', description: 'Scale marker with distance.' },
        minScale: { type: 'number', description: 'Minimum scale.' },
        maxScale: { type: 'number', description: 'Maximum scale.' },
        
        // Photo Mode parameters
        enabled: { type: 'boolean', description: 'Enable/disable photo mode.' },
        pauseGame: { type: 'boolean', description: 'Pause game in photo mode.' },
        hideUI: { type: 'boolean', description: 'Hide UI in photo mode.' },
        hidePlayer: { type: 'boolean', description: 'Hide player character.' },
        allowCameraMovement: { type: 'boolean', description: 'Allow free camera movement.' },
        maxCameraDistance: { type: 'number', description: 'Max camera distance from player.' },
        fov: { type: 'number', description: 'Field of view.' },
        aperture: { type: 'number', description: 'Camera aperture.' },
        focalDistance: { type: 'number', description: 'Focal distance.' },
        depthOfField: { type: 'boolean', description: 'Enable depth of field.' },
        exposure: { type: 'number', description: 'Exposure compensation.' },
        contrast: { type: 'number', description: 'Contrast multiplier.' },
        saturation: { type: 'number', description: 'Saturation multiplier.' },
        vignetteIntensity: { type: 'number', description: 'Vignette intensity.' },
        filmGrain: { type: 'number', description: 'Film grain intensity.' },
        filename: { type: 'string', description: 'Screenshot filename.' },
        resolution: { type: 'string', description: 'Screenshot resolution (e.g. 1920x1080).' },
        format: { type: 'string', enum: ['PNG', 'JPEG', 'EXR'], description: 'Image format.' },
        superSampling: { type: 'number', description: 'Super sampling multiplier.' },
        includeUI: { type: 'boolean', description: 'Include UI in screenshot.' },
        
        // Quest/Dialogue parameters
        assetPath: commonSchemas.assetPath,
        questId: { type: 'string', description: 'Unique quest identifier.' },
        questName: { type: 'string', description: 'Quest display name.' },
        questType: { type: 'string', enum: ['MainQuest', 'SideQuest', 'Daily', 'Event'], description: 'Quest type.' },
        prerequisites: { type: 'array', items: { type: 'string' }, description: 'Quest prerequisites.' },
        rewards: { type: 'array', items: { type: 'object' }, description: 'Quest rewards.' },
        dialogueName: { type: 'string', description: 'Dialogue tree name.' },
        startNodeId: { type: 'string', description: 'Starting node ID.' },
        nodeId: { type: 'string', description: 'Dialogue node ID.' },
        speakerId: { type: 'string', description: 'Speaker identifier.' },
        text: { type: 'string', description: 'Dialogue text.' },
        audioAsset: { type: 'string', description: 'Audio asset path.' },
        duration: { type: 'number', description: 'Node duration (0 = auto from audio).' },
        choices: { type: 'array', items: { type: 'object', properties: { text: { type: 'string' }, nextNodeId: { type: 'string' }, condition: { type: 'string' } } }, description: 'Dialogue choices.' },
        nextNodeId: { type: 'string', description: 'Next node ID.' },
        events: { type: 'array', items: { type: 'string' }, description: 'Events to trigger.' },
        targetActor: { type: 'string', description: 'Target actor for dialogue.' },
        skipable: { type: 'boolean', description: 'Whether dialogue is skipable.' },
        
        // Instancing parameters
        meshPath: { type: 'string', description: 'Static mesh asset path.' },
        materialPath: { type: 'string', description: 'Material asset path.' },
        cullDistance: { type: 'number', description: 'Cull distance (0 = no culling).' },
        castShadow: { type: 'boolean', description: 'Cast shadows.' },
        minLOD: { type: 'number', description: 'Minimum LOD level.' },
        useGpuLodSelection: { type: 'boolean', description: 'Use GPU LOD selection.' },
        transform: {
          type: 'object',
          properties: {
            location: commonSchemas.location,
            rotation: commonSchemas.rotation,
            scale: commonSchemas.scale
          },
          description: 'Instance transform.'
        },
        instances: { type: 'array', items: { type: 'object' }, description: 'Array of instance transforms.' },
        instanceIndex: { type: 'number', description: 'Instance index.' },
        instanceIndices: { type: 'array', items: { type: 'number' }, description: 'Array of instance indices.' },
        
        // HLOD parameters
        layerName: { type: 'string', description: 'HLOD layer name.' },
        cellSize: { type: 'number', description: 'HLOD cell size.' },
        loadingRange: { type: 'number', description: 'HLOD loading range.' },
        parentLayer: { type: 'string', description: 'Parent HLOD layer.' },
        hlodBuildMethod: { type: 'string', enum: ['MeshMerge', 'MeshSimplify', 'MeshApproximate', 'Custom'], description: 'HLOD build method.' },
        minDrawDistance: { type: 'number', description: 'Minimum draw distance.' },
        spatiallyLoaded: { type: 'boolean', description: 'Spatially loaded.' },
        alwaysLoaded: { type: 'boolean', description: 'Always loaded.' },
        buildAll: { type: 'boolean', description: 'Build all HLOD layers.' },
        forceRebuild: { type: 'boolean', description: 'Force rebuild.' },
        
        // Localization parameters
        tableName: { type: 'string', description: 'String table name.' },
        namespace: { type: 'string', description: 'Localization namespace.' },
        key: { type: 'string', description: 'String key.' },
        sourceString: { type: 'string', description: 'Source string.' },
        comment: { type: 'string', description: 'Translator comment.' },
        sourcePath: { type: 'string', description: 'Source file path.' },
        targetPath: { type: 'string', description: 'Target asset path.' },
        outputPath: { type: 'string', description: 'Output file path.' },
        culture: { type: 'string', description: 'Culture code (e.g. en, fr, de).' },
        saveToConfig: { type: 'boolean', description: 'Save culture to config.' },
        
        // Scalability parameters
        profileName: { type: 'string', description: 'Device profile name.' },
        baseProfile: { type: 'string', description: 'Base device profile.' },
        deviceType: { type: 'string', enum: ['Desktop', 'Mobile', 'Console'], description: 'Device type.' },
        cvars: { type: 'object', additionalProperties: true, description: 'Console variables.' },
        groupName: { type: 'string', description: 'Scalability group name.' },
        qualityLevel: { type: 'number', description: 'Quality level (0-4).' },
        overallQuality: { type: 'number', description: 'Overall quality level (0-4).' },
        applyImmediately: { type: 'boolean', description: 'Apply changes immediately.' },
        scale: { type: 'number', description: 'Resolution scale (0-100).' },
        
        // Common
        save: commonSchemas.save
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        ...commonSchemas.outputBase,
        
        // Targeting info
        targetingRange: commonSchemas.numberProp,
        lockOnEnabled: commonSchemas.booleanProp,
        currentTarget: commonSchemas.stringProp,
        
        // Checkpoint info
        checkpointSaved: commonSchemas.booleanProp,
        checkpointLoaded: commonSchemas.booleanProp,
        
        // Objective info
        objectives: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              id: commonSchemas.stringProp,
              name: commonSchemas.stringProp,
              state: commonSchemas.stringProp,
              progress: commonSchemas.numberProp
            }
          }
        },
        
        // Marker info
        markersActive: commonSchemas.numberProp,
        
        // Photo mode info
        photoModeActive: commonSchemas.booleanProp,
        screenshotPath: commonSchemas.stringProp,
        
        // Dialogue info
        dialogueActive: commonSchemas.booleanProp,
        currentNode: commonSchemas.stringProp,
        
        // Instance info
        instanceCount: commonSchemas.numberProp,
        instancesAdded: commonSchemas.numberProp,
        instancesRemoved: commonSchemas.numberProp,
        
        // HLOD info
        hlodLayers: commonSchemas.arrayOfStrings,
        hlodBuilt: commonSchemas.booleanProp,
        
        // Localization info
        stringTableEntries: commonSchemas.numberProp,
        availableCultures: commonSchemas.arrayOfStrings,
        currentCulture: commonSchemas.stringProp,
        localizedString: commonSchemas.stringProp,
        
        // Scalability info
        deviceProfiles: commonSchemas.arrayOfStrings,
        currentQuality: commonSchemas.numberProp,
        resolutionScale: commonSchemas.numberProp,
        scalabilitySettings: {
          type: 'object',
          properties: {
            viewDistance: commonSchemas.numberProp,
            antiAliasing: commonSchemas.numberProp,
            postProcess: commonSchemas.numberProp,
            shadow: commonSchemas.numberProp,
            texture: commonSchemas.numberProp,
            effects: commonSchemas.numberProp,
            foliage: commonSchemas.numberProp,
            shading: commonSchemas.numberProp
          }
        }
      }
    }
  },

  // ===== PHASE 35B: UNIVERSAL GAMEPLAY PRIMITIVES =====
  {
    name: 'manage_gameplay_primitives',
    category: 'gameplay',
    description: 'Universal gameplay building blocks: state machines, values, factions, zones, conditions, spawners.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            // Value Tracker (8 actions)
            'create_value_tracker', 'modify_value', 'set_value', 'get_value',
            'add_value_threshold', 'configure_value_decay', 'configure_value_regen', 'pause_value_changes',
            // State Machine (6 actions)
            'create_actor_state_machine', 'add_actor_state', 'add_actor_state_transition',
            'set_actor_state', 'get_actor_state', 'configure_state_timer',
            // Faction/Reputation (8 actions)
            'create_faction', 'set_faction_relationship', 'assign_to_faction', 'get_faction',
            'modify_reputation', 'get_reputation', 'add_reputation_threshold', 'check_faction_relationship',
            // Actor Attachment (6 actions)
            'attach_to_socket', 'detach_from_parent', 'transfer_control',
            'configure_attachment_rules', 'get_attached_actors', 'get_attachment_parent',
            // Schedule System (5 actions)
            'create_schedule', 'add_schedule_entry', 'set_schedule_active',
            'get_current_schedule_entry', 'skip_to_schedule_entry',
            // World Time (7 actions)
            'create_world_time', 'set_world_time', 'get_world_time', 'set_time_scale',
            'pause_world_time', 'add_time_event', 'get_time_period',
            // Zone System (6 actions)
            'create_zone', 'set_zone_property', 'get_zone_property',
            'get_actor_zone', 'add_zone_enter_event', 'add_zone_exit_event',
            // Condition/Rules (4 actions)
            'create_condition', 'create_compound_condition', 'evaluate_condition', 'add_condition_listener',
            // Interaction System (6 actions)
            'add_interactable_component', 'configure_interaction', 'set_interaction_enabled',
            'get_nearby_interactables', 'focus_interaction', 'execute_interaction',
            // Spawn System (6 actions)
            'create_spawner', 'configure_spawner', 'set_spawner_enabled',
            'configure_spawn_conditions', 'despawn_managed_actors', 'get_spawned_count'
          ],
          description: 'Gameplay primitive action to perform.'
        },
        
        // Common parameters
        actorName: commonSchemas.actorName,
        componentName: { type: 'string', description: 'Optional component name for disambiguation.' },
        
        // Value Tracker parameters
        trackerKey: { type: 'string', description: 'Unique key for the value tracker (e.g., "Health", "Stamina").' },
        value: { type: 'number', description: 'Value to set.' },
        delta: { type: 'number', description: 'Delta to add/subtract from current value.' },
        initialValue: { type: 'number', description: 'Initial value when creating tracker.' },
        minValue: { type: 'number', description: 'Minimum allowed value.' },
        maxValue: { type: 'number', description: 'Maximum allowed value.' },
        threshold: { type: 'number', description: 'Threshold value for events.' },
        direction: { type: 'string', enum: ['rising', 'falling', 'both'], description: 'Threshold crossing direction.' },
        rate: { type: 'number', description: 'Rate per second for decay/regen.' },
        interval: { type: 'number', description: 'Update interval in seconds.' },
        paused: { type: 'boolean', description: 'Whether changes are paused.' },
        
        // State Machine parameters
        stateName: { type: 'string', description: 'State name.' },
        stateData: { type: 'object', additionalProperties: true, description: 'Custom state data.' },
        fromState: { type: 'string', description: 'Source state for transition.' },
        toState: { type: 'string', description: 'Target state for transition.' },
        conditions: { type: 'object', description: 'Structured condition predicate (JSON AST).' },
        force: { type: 'boolean', description: 'Force state change even if invalid transition.' },
        duration: { type: 'number', description: 'Duration in seconds.' },
        autoTransition: { type: 'boolean', description: 'Auto-transition when timer expires.' },
        targetState: { type: 'string', description: 'Target state for auto-transition.' },
        
        // Faction parameters
        factionId: { type: 'string', description: 'Unique faction identifier.' },
        displayName: { type: 'string', description: 'Display name.' },
        factionA: { type: 'string', description: 'First faction ID.' },
        factionB: { type: 'string', description: 'Second faction ID.' },
        relationship: { type: 'string', enum: ['friendly', 'neutral', 'hostile'], description: 'Faction relationship.' },
        bidirectional: { type: 'boolean', description: 'Apply relationship in both directions.' },
        actorA: { type: 'string', description: 'First actor name.' },
        actorB: { type: 'string', description: 'Second actor name.' },
        
        // Attachment parameters
        childActor: { type: 'string', description: 'Child actor to attach.' },
        parentActor: { type: 'string', description: 'Parent actor to attach to.' },
        socketName: { type: 'string', description: 'Socket name on parent.' },
        attachRules: { type: 'object', description: 'Attachment rules (locationRule, rotationRule, scaleRule).' },
        detachRules: { type: 'object', description: 'Detachment rules.' },
        newController: { type: 'string', description: 'New controller actor for transfer.' },
        locationRule: { type: 'string', enum: ['KeepRelative', 'KeepWorld', 'SnapToTarget'], description: 'Location rule.' },
        rotationRule: { type: 'string', enum: ['KeepRelative', 'KeepWorld', 'SnapToTarget'], description: 'Rotation rule.' },
        scaleRule: { type: 'string', enum: ['KeepRelative', 'KeepWorld', 'SnapToTarget'], description: 'Scale rule.' },
        recursive: { type: 'boolean', description: 'Include recursively attached actors.' },
        
        // Schedule parameters
        scheduleId: { type: 'string', description: 'Schedule identifier.' },
        startTime: { type: 'number', description: 'Start time (world time units).' },
        endTime: { type: 'number', description: 'End time (world time units).' },
        scheduleAction: { type: 'string', description: 'Action to perform at schedule time.' },
        location: commonSchemas.location,
        active: { type: 'boolean', description: 'Whether schedule is active.' },
        entryIndex: { type: 'number', description: 'Schedule entry index.' },
        
        // World Time parameters
        time: { type: 'number', description: 'World time value.' },
        dayLength: { type: 'number', description: 'Real seconds per in-game day.' },
        timeScale: { type: 'number', description: 'Time scale multiplier.' },
        startPaused: { type: 'boolean', description: 'Start with time paused.' },
        eventId: { type: 'string', description: 'Time event identifier.' },
        triggerTime: { type: 'number', description: 'Time to trigger event.' },
        recurring: { type: 'boolean', description: 'Whether event recurs.' },
        
        // Zone parameters
        zoneId: { type: 'string', description: 'Zone identifier.' },
        zoneName: { type: 'string', description: 'Zone display name.' },
        volumeActor: { type: 'string', description: 'Volume actor that defines zone bounds.' },
        propertyKey: { type: 'string', description: 'Zone property key.' },
        propertyValue: { type: ['string', 'number', 'boolean'], description: 'Zone property value.' },
        properties: { type: 'object', additionalProperties: true, description: 'Zone properties.' },
        
        // Condition parameters
        conditionId: { type: 'string', description: 'Condition identifier.' },
        predicate: { type: 'object', description: 'Structured condition predicate (JSON AST).' },
        operator: { type: 'string', enum: ['all', 'any', 'not'], description: 'Compound condition operator.' },
        conditionIds: { type: 'array', items: { type: 'string' }, description: 'Array of condition IDs to combine.' },
        context: { type: 'object', additionalProperties: true, description: 'Evaluation context.' },
        listenerId: { type: 'string', description: 'Listener identifier.' },
        oneShot: { type: 'boolean', description: 'Remove listener after first trigger.' },
        
        // Interaction parameters
        interactionType: { type: 'string', enum: ['instant', 'hold', 'toggle'], description: 'Interaction type.' },
        range: { type: 'number', description: 'Interaction range.' },
        prompt: { type: 'string', description: 'Interaction prompt text.' },
        enabled: { type: 'boolean', description: 'Whether interaction is enabled.' },
        filterType: { type: 'string', description: 'Filter type for nearby interactables.' },
        targetActor: { type: 'string', description: 'Target actor for interaction.' },
        interactionData: { type: 'object', additionalProperties: true, description: 'Custom interaction data.' },
        
        // Spawner parameters
        spawnClass: { type: 'string', description: 'Class to spawn.' },
        spawnRadius: { type: 'number', description: 'Spawn radius around spawner.' },
        maxSpawned: { type: 'number', description: 'Maximum spawned actors.' },
        respawnDelay: { type: 'number', description: 'Respawn delay in seconds.' },
        filter: { type: 'object', description: 'Filter for despawn operation.' },
        
        // Color (used by factions)
        color: { type: 'object', properties: { r: { type: 'number' }, g: { type: 'number' }, b: { type: 'number' }, a: { type: 'number' } }, description: 'Color (RGBA).' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        ...commonSchemas.outputBase,
        
        // Value Tracker output
        trackerKey: commonSchemas.stringProp,
        value: commonSchemas.numberProp,
        previousValue: commonSchemas.numberProp,
        newValue: commonSchemas.numberProp,
        percentage: commonSchemas.numberProp,
        isPaused: commonSchemas.booleanProp,
        thresholdId: commonSchemas.stringProp,
        configured: commonSchemas.booleanProp,
        
        // State Machine output
        currentState: commonSchemas.stringProp,
        previousState: commonSchemas.stringProp,
        stateStartTime: commonSchemas.numberProp,
        stateId: commonSchemas.stringProp,
        transitionId: commonSchemas.stringProp,
        
        // Faction output
        factionId: commonSchemas.stringProp,
        factionData: { type: 'object' },
        reputation: commonSchemas.numberProp,
        standing: commonSchemas.stringProp,
        isFriendly: commonSchemas.booleanProp,
        isHostile: commonSchemas.booleanProp,
        
        // Attachment output
        attached: commonSchemas.booleanProp,
        detached: commonSchemas.booleanProp,
        transferred: commonSchemas.booleanProp,
        attachedActors: commonSchemas.arrayOfStrings,
        parentActorName: commonSchemas.stringProp,
        
        // Schedule output
        entryId: commonSchemas.stringProp,
        entry: { type: 'object' },
        timeRemaining: commonSchemas.numberProp,
        skipped: commonSchemas.booleanProp,
        newEntry: { type: 'object' },
        previousActive: commonSchemas.booleanProp,
        newActive: commonSchemas.booleanProp,
        
        // World Time output
        created: commonSchemas.booleanProp,
        day: commonSchemas.numberProp,
        hour: commonSchemas.numberProp,
        minute: commonSchemas.numberProp,
        period: commonSchemas.stringProp,
        previousTime: commonSchemas.numberProp,
        newTime: commonSchemas.numberProp,
        previousScale: commonSchemas.numberProp,
        newScale: commonSchemas.numberProp,
        previousPaused: commonSchemas.booleanProp,
        newPaused: commonSchemas.booleanProp,
        periodStart: commonSchemas.numberProp,
        periodEnd: commonSchemas.numberProp,
        
        // Zone output
        zoneId: commonSchemas.stringProp,
        zoneName: commonSchemas.stringProp,
        updated: commonSchemas.booleanProp,
        
        // Condition output
        conditionId: commonSchemas.stringProp,
        result: commonSchemas.booleanProp,
        evaluatedAt: commonSchemas.numberProp,
        listenerId: commonSchemas.stringProp,
        
        // Interaction output
        focused: commonSchemas.booleanProp,
        interactables: { type: 'array', items: { type: 'object' } },
        cooldownRemaining: commonSchemas.numberProp,
        previousEnabled: commonSchemas.booleanProp,
        newEnabled: commonSchemas.booleanProp,
        
        // Spawner output
        count: commonSchemas.numberProp,
        maxCount: commonSchemas.numberProp,
        activeActors: commonSchemas.arrayOfStrings,
        despawnedCount: commonSchemas.numberProp,
        
        // Common
        componentId: commonSchemas.stringProp
      }
    }
  },

  // ===== PHASE 36: CHARACTER & AVATAR PLUGINS =====
  {
    name: 'manage_character_avatar',
    category: 'authoring',
    description: 'MetaHuman, Groom/Hair, Mutable, Ready Player Me avatar systems.',
    annotations: {
      audience: ['developer', 'artist'],
      priority: 6,
      tags: ['metahuman', 'groom', 'hair', 'mutable', 'rpm', 'avatar']
    },
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            // MetaHuman (18 actions)
            'import_metahuman', 'spawn_metahuman_actor', 'get_metahuman_component',
            'set_body_type', 'set_face_parameter', 'set_skin_tone', 'set_hair_style', 'set_eye_color',
            'configure_metahuman_lod', 'enable_body_correctives', 'enable_neck_correctives',
            'set_quality_level', 'configure_face_rig', 'set_body_part',
            'get_metahuman_info', 'list_available_presets', 'apply_preset', 'export_metahuman_settings',
            // Groom/Hair (14 actions)
            'create_groom_asset', 'import_groom', 'create_groom_binding', 'spawn_groom_actor',
            'attach_groom_to_skeletal_mesh', 'configure_hair_simulation', 'set_hair_width',
            'set_hair_root_scale', 'set_hair_tip_scale', 'set_hair_color',
            'configure_hair_physics', 'configure_hair_rendering', 'enable_hair_simulation', 'get_groom_info',
            // Mutable/Customizable (16 actions)
            'create_customizable_object', 'compile_customizable_object', 'create_customizable_instance',
            'set_bool_parameter', 'set_int_parameter', 'set_float_parameter', 'set_color_parameter',
            'set_vector_parameter', 'set_texture_parameter', 'set_transform_parameter', 'set_projector_parameter',
            'update_skeletal_mesh', 'bake_customizable_instance', 'get_parameter_info', 'get_instance_info',
            'spawn_customizable_actor',
            // Ready Player Me (12 actions)
            'load_avatar_from_url', 'load_avatar_from_glb', 'create_rpm_actor', 'apply_avatar_to_character',
            'configure_rpm_materials', 'set_rpm_outfit', 'get_avatar_metadata', 'cache_avatar',
            'clear_avatar_cache', 'create_rpm_animation_blueprint', 'retarget_rpm_animation', 'get_rpm_info'
          ],
          description: 'Character avatar action to perform.'
        },
        
        // Common parameters
        name: commonSchemas.name,
        actorName: commonSchemas.actorName,
        sourcePath: commonSchemas.sourcePath,
        destinationPath: commonSchemas.destinationPath,
        location: commonSchemas.location,
        rotation: commonSchemas.rotation,
        scale: commonSchemas.scale,
        
        // MetaHuman parameters
        metahumanPath: { type: 'string', description: 'Path to MetaHuman asset.' },
        bodyType: { type: 'string', enum: ['Masculine', 'Feminine'], description: 'MetaHuman body type.' },
        parameterName: commonSchemas.parameterName,
        parameterValue: { type: 'number', description: 'Parameter value (0-1 for face params).' },
        skinTone: { type: 'number', description: 'Skin tone index or value.' },
        hairStylePath: { type: 'string', description: 'Path to hair style asset.' },
        eyeColor: {
          type: 'object',
          properties: { r: { type: 'number' }, g: { type: 'number' }, b: { type: 'number' }, a: { type: 'number' } },
          description: 'Eye color (RGBA 0-1).'
        },
        lodLevel: { type: 'number', description: 'LOD level (0 = highest quality).' },
        lodScreenSize: { type: 'number', description: 'LOD screen size threshold.' },
        rigLogicLODThreshold: { type: 'number', description: 'RigLogic LOD threshold.' },
        enableBodyCorrectives: { type: 'boolean', description: 'Enable body correctives.' },
        enableNeckCorrectives: { type: 'boolean', description: 'Enable neck correctives.' },
        qualityLevel: { type: 'string', enum: ['Low', 'Medium', 'High', 'Cinematic'], description: 'Quality preset.' },
        bodyPartType: { type: 'string', enum: ['Face', 'Body', 'Outfit', 'Accessories'], description: 'Body part type.' },
        bodyPartPath: { type: 'string', description: 'Path to body part asset.' },
        presetName: { type: 'string', description: 'Preset name.' },
        exportPath: commonSchemas.outputPath,
        
        // Groom/Hair parameters
        groomAssetPath: { type: 'string', description: 'Path to groom asset.' },
        groomBindingPath: { type: 'string', description: 'Path to groom binding asset.' },
        skeletalMeshPath: { type: 'string', description: 'Path to skeletal mesh.' },
        hairWidth: { type: 'number', description: 'Hair strand width.' },
        hairRootScale: { type: 'number', description: 'Hair root scale.' },
        hairTipScale: { type: 'number', description: 'Hair tip scale.' },
        hairColor: {
          type: 'object',
          properties: { r: { type: 'number' }, g: { type: 'number' }, b: { type: 'number' }, a: { type: 'number' } },
          description: 'Hair color (RGBA 0-1).'
        },
        enableSimulation: { type: 'boolean', description: 'Enable hair simulation.' },
        simulationSettings: {
          type: 'object',
          properties: {
            damping: { type: 'number', description: 'Damping (0-1).' },
            stiffness: { type: 'number', description: 'Stiffness (0-1).' },
            gravity: { type: 'number', description: 'Gravity scale.' },
            iterations: { type: 'number', description: 'Solver iterations.' }
          },
          description: 'Hair simulation settings.'
        },
        physicsSettings: {
          type: 'object',
          properties: {
            solverIterations: { type: 'number' },
            subSteps: { type: 'number' },
            maxStretch: { type: 'number' },
            inertiaScale: { type: 'number' }
          },
          description: 'Hair physics settings.'
        },
        renderingSettings: {
          type: 'object',
          properties: {
            renderMode: { type: 'string', enum: ['Strands', 'Cards', 'Meshes'] },
            shadowMode: { type: 'string', enum: ['None', 'CastShadow', 'CastDeepShadow'] },
            geometryType: { type: 'string', enum: ['Strands', 'Cards', 'Meshes'] },
            hairDensity: { type: 'number' }
          },
          description: 'Hair rendering settings.'
        },
        
        // Mutable/Customizable parameters
        objectPath: { type: 'string', description: 'Path to Customizable Object.' },
        instancePath: { type: 'string', description: 'Path to Customizable Object Instance.' },
        boolValue: { type: 'boolean', description: 'Boolean parameter value.' },
        intValue: { type: 'number', description: 'Integer parameter value.' },
        floatValue: { type: 'number', description: 'Float parameter value.' },
        colorValue: {
          type: 'object',
          properties: { r: { type: 'number' }, g: { type: 'number' }, b: { type: 'number' }, a: { type: 'number' } },
          description: 'Color parameter value (RGBA 0-1).'
        },
        vectorValue: {
          type: 'object',
          properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } },
          description: 'Vector parameter value.'
        },
        texturePath: commonSchemas.texturePath,
        transformValue: {
          type: 'object',
          properties: {
            location: commonSchemas.location,
            rotation: commonSchemas.rotation,
            scale: commonSchemas.scale
          },
          description: 'Transform parameter value.'
        },
        projectorValue: {
          type: 'object',
          properties: {
            position: { type: 'object', properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } } },
            direction: { type: 'object', properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } } },
            up: { type: 'object', properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } } },
            scale: { type: 'object', properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } } },
            projectionAngle: { type: 'number' }
          },
          description: 'Projector parameter value.'
        },
        bakeOutputPath: { type: 'string', description: 'Output path for baked mesh.' },
        componentIndex: { type: 'number', description: 'Component index for mesh update.' },
        
        // Ready Player Me parameters
        avatarUrl: { type: 'string', description: 'Ready Player Me avatar URL.' },
        glbPath: { type: 'string', description: 'Path to GLB file.' },
        avatarAssetPath: { type: 'string', description: 'Path to avatar asset.' },
        characterPath: { type: 'string', description: 'Path to character Blueprint.' },
        materialSettings: {
          type: 'object',
          properties: {
            usePBR: { type: 'boolean' },
            materialPath: { type: 'string' },
            skinMaterialPath: { type: 'string' },
            hairMaterialPath: { type: 'string' },
            eyeMaterialPath: { type: 'string' }
          },
          description: 'RPM material configuration.'
        },
        outfitId: { type: 'string', description: 'Outfit asset ID or path.' },
        cacheKey: { type: 'string', description: 'Cache key for avatar.' },
        animBlueprintPath: { type: 'string', description: 'Animation Blueprint path.' },
        sourceAnimationPath: { type: 'string', description: 'Source animation path.' },
        targetSkeletonPath: { type: 'string', description: 'Target skeleton path.' },
        
        // Common options
        save: commonSchemas.save,
        overwrite: commonSchemas.overwrite
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        ...commonSchemas.outputBase,
        
        // Common outputs
        actorName: commonSchemas.actorName,
        assetPath: commonSchemas.stringProp,
        
        // MetaHuman outputs
        metahumanInfo: {
          type: 'object',
          properties: {
            bodyType: commonSchemas.stringProp,
            qualityLevel: commonSchemas.stringProp,
            lodLevel: commonSchemas.numberProp,
            bodyCorrectives: commonSchemas.booleanProp,
            neckCorrectives: commonSchemas.booleanProp,
            faceParameters: { type: 'object' },
            skinTone: commonSchemas.numberProp,
            hairStyle: commonSchemas.stringProp,
            eyeColor: { type: 'object' }
          },
          description: 'MetaHuman component info (for get_metahuman_info).'
        },
        presets: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              name: commonSchemas.stringProp,
              category: commonSchemas.stringProp,
              description: commonSchemas.stringProp
            }
          },
          description: 'Available presets (for list_available_presets).'
        },
        exportedSettings: { type: 'object', description: 'Exported settings JSON.' },
        
        // Groom outputs
        groomInfo: {
          type: 'object',
          properties: {
            assetPath: commonSchemas.stringProp,
            bindingPath: commonSchemas.stringProp,
            hairWidth: commonSchemas.numberProp,
            rootScale: commonSchemas.numberProp,
            tipScale: commonSchemas.numberProp,
            simulationEnabled: commonSchemas.booleanProp,
            strandCount: commonSchemas.numberProp,
            guideCount: commonSchemas.numberProp
          },
          description: 'Groom component info (for get_groom_info).'
        },
        
        // Mutable outputs
        parameterInfo: {
          type: 'object',
          properties: {
            parameters: {
              type: 'array',
              items: {
                type: 'object',
                properties: {
                  name: commonSchemas.stringProp,
                  type: commonSchemas.stringProp,
                  defaultValue: {
                    anyOf: [
                      { type: 'string' },
                      { type: 'number' },
                      { type: 'boolean' },
                      { type: 'object', additionalProperties: true }
                    ]
                  },
                  options: commonSchemas.arrayOfStrings
                }
              }
            }
          },
          description: 'Customizable object parameters (for get_parameter_info).'
        },
        instanceInfo: {
          type: 'object',
          properties: {
            objectPath: commonSchemas.stringProp,
            isCompiled: commonSchemas.booleanProp,
            parameterValues: { type: 'object' },
            skeletalMeshPath: commonSchemas.stringProp
          },
          description: 'Customizable instance info (for get_instance_info).'
        },
        bakedMeshPath: commonSchemas.stringProp,
        
        // Ready Player Me outputs
        avatarMetadata: {
          type: 'object',
          properties: {
            avatarId: commonSchemas.stringProp,
            gender: commonSchemas.stringProp,
            outfit: commonSchemas.stringProp,
            bodyType: commonSchemas.stringProp,
            createdAt: commonSchemas.stringProp
          },
          description: 'Avatar metadata (for get_avatar_metadata).'
        },
        rpmInfo: {
          type: 'object',
          properties: {
            isAvailable: commonSchemas.booleanProp,
            version: commonSchemas.stringProp,
            cachedAvatars: commonSchemas.numberProp,
            supportedFormats: commonSchemas.arrayOfStrings
          },
          description: 'RPM system info (for get_rpm_info).'
        },
        cacheInfo: {
          type: 'object',
          properties: {
            cacheKey: commonSchemas.stringProp,
            cacheSize: commonSchemas.numberProp,
            itemsCached: commonSchemas.numberProp
          },
          description: 'Cache info (for cache operations).'
        }
      }
    }
  },

  // ===== PHASE 37: ASSET & CONTENT PLUGINS =====
  {
    name: 'manage_asset_plugins',
    category: 'utility',
    description: 'Import plugins (USD, Alembic, glTF, Datasmith, Houdini, Substance).',
    annotations: {
      audience: ['developer'],
      priority: 6,
      tags: ['usd', 'alembic', 'gltf', 'datasmith', 'houdini', 'substance']
    },
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            // Interchange Framework (18 actions)
            'create_interchange_pipeline', 'configure_interchange_pipeline', 'import_with_interchange',
            'import_fbx_with_interchange', 'import_obj_with_interchange', 'export_with_interchange',
            'set_interchange_translator', 'get_interchange_translators', 'configure_import_asset_type',
            'set_interchange_result_container', 'get_interchange_import_result', 'cancel_interchange_import',
            'create_interchange_source_data', 'set_interchange_pipeline_stack',
            'configure_static_mesh_settings', 'configure_skeletal_mesh_settings',
            'configure_animation_settings', 'configure_material_settings',
            // USD (24 actions)
            'create_usd_stage', 'open_usd_stage', 'close_usd_stage', 'get_usd_stage_info',
            'create_usd_prim', 'get_usd_prim', 'set_usd_prim_attribute', 'get_usd_prim_attribute',
            'add_usd_reference', 'add_usd_payload', 'set_usd_variant', 'create_usd_layer',
            'set_edit_target_layer', 'save_usd_stage', 'export_actor_to_usd', 'export_level_to_usd',
            'export_static_mesh_to_usd', 'export_skeletal_mesh_to_usd', 'export_material_to_usd',
            'export_animation_to_usd', 'enable_usd_live_edit', 'spawn_usd_stage_actor',
            'configure_usd_asset_cache', 'get_usd_prim_children',
            // Alembic (15 actions)
            'import_alembic_file', 'import_alembic_static_mesh', 'import_alembic_skeletal_mesh',
            'import_alembic_geometry_cache', 'import_alembic_groom', 'configure_alembic_import_settings',
            'set_alembic_sampling_settings', 'set_alembic_compression_type', 'set_alembic_normal_generation',
            'reimport_alembic_asset', 'get_alembic_info', 'create_geometry_cache_track',
            'play_geometry_cache', 'set_geometry_cache_time', 'export_to_alembic',
            // glTF (16 actions)
            'import_gltf', 'import_glb', 'import_gltf_static_mesh', 'import_gltf_skeletal_mesh',
            'export_to_gltf', 'export_to_glb', 'export_level_to_gltf', 'export_actor_to_gltf',
            'configure_gltf_export_options', 'set_gltf_export_scale', 'set_gltf_texture_format',
            'set_draco_compression', 'export_material_to_gltf', 'export_animation_to_gltf',
            'get_gltf_export_messages', 'configure_gltf_material_baking',
            // Datasmith (18 actions)
            'import_datasmith_file', 'import_datasmith_cad', 'import_datasmith_revit',
            'import_datasmith_sketchup', 'import_datasmith_3dsmax', 'import_datasmith_rhino',
            'import_datasmith_solidworks', 'import_datasmith_archicad', 'configure_datasmith_import_options',
            'set_datasmith_tessellation_quality', 'reimport_datasmith_scene', 'get_datasmith_scene_info',
            'update_datasmith_scene', 'configure_datasmith_lightmap', 'create_datasmith_runtime_actor',
            'configure_datasmith_materials', 'export_datasmith_scene', 'sync_datasmith_changes',
            // SpeedTree (12 actions)
            'import_speedtree_model', 'import_speedtree_9', 'import_speedtree_atlas',
            'configure_speedtree_wind', 'set_speedtree_wind_type', 'set_speedtree_wind_speed',
            'configure_speedtree_lod', 'set_speedtree_lod_distances', 'set_speedtree_lod_transition',
            'create_speedtree_material', 'configure_speedtree_collision', 'get_speedtree_info',
            // Quixel/Fab (12 actions)
            'connect_to_bridge', 'disconnect_bridge', 'get_bridge_status',
            'import_megascan_surface', 'import_megascan_3d_asset', 'import_megascan_3d_plant',
            'import_megascan_decal', 'import_megascan_atlas', 'import_megascan_brush',
            'search_fab_assets', 'download_fab_asset', 'configure_megascan_import_settings',
            // Houdini Engine (22 actions)
            'import_hda', 'instantiate_hda', 'spawn_hda_actor', 'get_hda_parameters',
            'set_hda_float_parameter', 'set_hda_int_parameter', 'set_hda_bool_parameter',
            'set_hda_string_parameter', 'set_hda_color_parameter', 'set_hda_vector_parameter',
            'set_hda_ramp_parameter', 'set_hda_multi_parameter', 'cook_hda',
            'bake_hda_to_actors', 'bake_hda_to_blueprint', 'configure_hda_input',
            'set_hda_world_input', 'set_hda_geometry_input', 'set_hda_curve_input',
            'get_hda_outputs', 'get_hda_cook_status', 'connect_to_houdini_session',
            // Substance (20 actions)
            'import_sbsar_file', 'create_substance_instance', 'get_substance_parameters',
            'set_substance_float_parameter', 'set_substance_int_parameter', 'set_substance_bool_parameter',
            'set_substance_color_parameter', 'set_substance_string_parameter', 'set_substance_image_input',
            'render_substance_textures', 'get_substance_outputs', 'create_material_from_substance',
            'apply_substance_to_material', 'configure_substance_output_size', 'randomize_substance_seed',
            'export_substance_textures', 'reimport_sbsar', 'get_substance_graph_info',
            'set_substance_output_format', 'batch_render_substances',
            // Utility
            'get_asset_plugins_info',
            // [UtilityPlugins] Python Scripting (15 actions - merged)
            'util_execute_python_script', 'util_execute_python_file', 'util_execute_python_command',
            'util_configure_python_paths', 'util_add_python_path', 'util_remove_python_path',
            'util_get_python_paths', 'util_create_python_editor_utility', 'util_run_startup_scripts',
            'util_get_python_output', 'util_clear_python_output', 'util_is_python_available',
            'util_get_python_version', 'util_reload_python_module', 'util_get_python_info',
            // [UtilityPlugins] Editor Scripting (12 actions - merged)
            'util_create_editor_utility_widget', 'util_create_editor_utility_blueprint',
            'util_add_menu_entry', 'util_remove_menu_entry', 'util_add_toolbar_button', 'util_remove_toolbar_button',
            'util_register_editor_command', 'util_unregister_editor_command', 'util_execute_editor_command',
            'util_create_blutility_action', 'util_run_editor_utility', 'util_get_editor_scripting_info',
            // [UtilityPlugins] Modeling Tools (18 actions - merged)
            'util_activate_modeling_tool', 'util_deactivate_modeling_tool', 'util_get_active_tool',
            'util_select_mesh_elements', 'util_clear_mesh_selection', 'util_get_mesh_selection',
            'util_set_sculpt_brush', 'util_configure_sculpt_brush', 'util_execute_sculpt_stroke',
            'util_apply_mesh_operation', 'util_undo_mesh_operation', 'util_accept_tool_result',
            'util_cancel_tool', 'util_set_tool_property', 'util_get_tool_properties',
            'util_list_available_tools', 'util_enter_modeling_mode', 'util_get_modeling_tools_info',
            // [UtilityPlugins] Paper2D (12 actions - merged)
            'util_create_sprite', 'util_create_flipbook', 'util_add_flipbook_keyframe',
            'util_create_tile_map', 'util_create_tile_set', 'util_set_tile_map_layer',
            'util_spawn_paper_sprite_actor', 'util_spawn_paper_flipbook_actor',
            'util_configure_sprite_collision', 'util_configure_sprite_material',
            'util_get_sprite_info', 'util_get_paper2d_info',
            // [UtilityPlugins] Procedural Mesh (15 actions - merged)
            'util_create_procedural_mesh_component', 'util_create_mesh_section',
            'util_update_mesh_section', 'util_clear_mesh_section', 'util_clear_all_mesh_sections',
            'util_set_mesh_section_visible', 'util_set_mesh_collision',
            'util_set_mesh_vertices', 'util_set_mesh_triangles', 'util_set_mesh_normals',
            'util_set_mesh_uvs', 'util_set_mesh_colors', 'util_set_mesh_tangents',
            'util_convert_procedural_to_static_mesh', 'util_get_procedural_mesh_info',
            // [UtilityPlugins] Variant Manager (15 actions - merged)
            'util_create_level_variant_sets', 'util_create_variant_set', 'util_delete_variant_set',
            'util_add_variant', 'util_remove_variant', 'util_duplicate_variant',
            'util_activate_variant', 'util_deactivate_variant', 'util_get_active_variant',
            'util_add_actor_binding', 'util_remove_actor_binding', 'util_capture_property',
            'util_configure_variant_dependency', 'util_export_variant_configuration',
            'util_get_variant_manager_info',
            // [UtilityPlugins] Utilities (3 actions - merged)
            'util_get_utility_plugins_info', 'util_list_utility_plugins', 'util_get_plugin_status'
          ],
          description: 'Asset plugin or utility plugin action.'
        },
        
        // Common file parameters
        filePath: { type: 'string', description: 'Source file path (absolute or project-relative).' },
        destinationPath: { type: 'string', description: 'Destination path in content browser (/Game/...).' },
        assetPath: commonSchemas.assetPath,
        actorName: commonSchemas.actorName,
        
        // Interchange parameters
        pipelineName: { type: 'string', description: 'Name for the interchange pipeline.' },
        pipelineClass: { type: 'string', description: 'Pipeline class to use.' },
        translatorClass: { type: 'string', description: 'Translator class for specific file type.' },
        importOptions: { type: 'object', description: 'Import configuration options.' },
        exportOptions: { type: 'object', description: 'Export configuration options.' },
        assetType: { type: 'string', enum: ['StaticMesh', 'SkeletalMesh', 'Animation', 'Material', 'Texture'], description: 'Asset type to import.' },
        
        // USD parameters
        rootLayerPath: { type: 'string', description: 'USD root layer file path.' },
        primPath: { type: 'string', description: 'USD prim path (e.g., /Root/MyPrim).' },
        primType: { type: 'string', description: 'USD prim type (Xform, Mesh, Scope, etc.).' },
        attributeName: { type: 'string', description: 'USD attribute name.' },
        attributeValue: { type: ['string', 'number', 'boolean', 'object'], description: 'Attribute value.' },
        referencePath: { type: 'string', description: 'Path to USD file to reference.' },
        payloadPath: { type: 'string', description: 'Path to USD payload file.' },
        variantSetName: { type: 'string', description: 'USD variant set name.' },
        variantName: { type: 'string', description: 'USD variant name.' },
        layerIdentifier: { type: 'string', description: 'USD layer identifier.' },
        stageState: { type: 'string', enum: ['Closed', 'Opened', 'OpenedAndLoaded'], description: 'USD stage state.' },
        enableLiveEdit: { type: 'boolean', description: 'Enable USD live editing.' },
        
        // Alembic parameters
        importType: { type: 'string', enum: ['StaticMesh', 'GeometryCache', 'Skeletal'], description: 'Alembic import type.' },
        samplingType: { type: 'string', enum: ['PerFrame', 'PerXFrames', 'PerTimeStep'], description: 'Animation sampling type.' },
        frameSteps: { type: 'number', description: 'Frame steps for sampling.' },
        timeSteps: { type: 'number', description: 'Time steps for sampling.' },
        frameStart: { type: 'number', description: 'Start frame.' },
        frameEnd: { type: 'number', description: 'End frame.' },
        recomputeNormals: { type: 'boolean', description: 'Recompute normals on import.' },
        compressionType: { type: 'string', enum: ['PercentageBased', 'FixedNumber', 'NoCompression'], description: 'Compression type.' },
        
        // glTF parameters
        exportScale: { type: 'number', description: 'Export scale factor (default 0.01 for cm to m).' },
        textureFormat: { type: 'string', enum: ['None', 'PNG', 'JPEG'], description: 'Texture image format.' },
        textureQuality: { type: 'number', description: 'JPEG texture quality (1-100).' },
        useDracoCompression: { type: 'boolean', description: 'Enable Draco mesh compression.' },
        exportVertexColors: { type: 'boolean', description: 'Export vertex colors.' },
        exportMorphTargets: { type: 'boolean', description: 'Export morph targets/blend shapes.' },
        bakeMaterialInputs: { type: 'string', enum: ['Disabled', 'Simple', 'UseMeshData'], description: 'Material baking mode.' },
        
        // Datasmith parameters
        tessellationQuality: { type: 'string', enum: ['Draft', 'Low', 'Medium', 'High', 'Custom'], description: 'CAD tessellation quality.' },
        lightmapResolution: { type: 'number', description: 'Lightmap resolution for imported assets.' },
        importHierarchy: { type: 'boolean', description: 'Preserve source hierarchy.' },
        minLightmapResolution: { type: 'number', description: 'Minimum lightmap resolution.' },
        
        // SpeedTree parameters
        modelPath: { type: 'string', description: 'SpeedTree model file path.' },
        windType: { type: 'string', enum: ['None', 'Fastest', 'Fast', 'Better', 'Best', 'Palm'], description: 'Wind animation type.' },
        windSpeed: { type: 'number', description: 'Wind speed multiplier.' },
        lodDistances: { type: 'array', items: { type: 'number' }, description: 'LOD distance thresholds.' },
        enableCollision: { type: 'boolean', description: 'Generate collision.' },
        
        // Quixel/Fab parameters
        assetId: { type: 'string', description: 'Megascan/Fab asset ID.' },
        searchQuery: { type: 'string', description: 'Search query for Fab assets.' },
        quality: { type: 'string', enum: ['Low', 'Medium', 'High', 'Ultra'], description: 'Asset quality tier.' },
        applyMaterials: { type: 'boolean', description: 'Auto-apply materials on import.' },
        
        // Houdini Engine parameters
        hdaPath: { type: 'string', description: 'HDA file path.' },
        parameterName: { type: 'string', description: 'HDA parameter name.' },
        parameterValue: { type: ['string', 'number', 'boolean', 'array', 'object'], description: 'Parameter value.' },
        inputIndex: { type: 'number', description: 'HDA input index.' },
        inputActors: { type: 'array', items: { type: 'string' }, description: 'Actors to use as HDA input.' },
        bakeToBlueprint: { type: 'boolean', description: 'Bake output to Blueprint.' },
        bakeToActors: { type: 'boolean', description: 'Bake output to level actors.' },
        cookMode: { type: 'string', enum: ['OnAssetChange', 'OnParameterChange', 'Manual'], description: 'HDA cook mode.' },
        
        // Substance parameters
        sbsarPath: { type: 'string', description: 'SBSAR file path.' },
        graphName: { type: 'string', description: 'Substance graph name.' },
        outputSize: { type: 'number', description: 'Output texture size (power of 2).' },
        seed: { type: 'number', description: 'Random seed.' },
        outputFormat: { type: 'string', enum: ['DXT1', 'DXT5', 'BC7', 'RGBA8'], description: 'Output texture format.' },
        parameterValues: { type: 'object', description: 'Batch of parameter name-value pairs.' },
        
        // Common
        save: commonSchemas.save
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: commonSchemas.booleanProp,
        message: commonSchemas.stringProp,
        error: commonSchemas.stringProp,
        
        // Common outputs
        assetPath: commonSchemas.stringProp,
        actorName: commonSchemas.stringProp,
        importedAssets: { type: 'array', items: { type: 'string' }, description: 'List of imported asset paths.' },
        exportedFile: { type: 'string', description: 'Exported file path.' },
        
        // Plugin availability
        pluginInfo: {
          type: 'object',
          properties: {
            interchangeAvailable: commonSchemas.booleanProp,
            usdAvailable: commonSchemas.booleanProp,
            alembicAvailable: commonSchemas.booleanProp,
            gltfAvailable: commonSchemas.booleanProp,
            datasmithAvailable: commonSchemas.booleanProp,
            speedtreeAvailable: commonSchemas.booleanProp,
            quixelAvailable: commonSchemas.booleanProp,
            houdiniAvailable: commonSchemas.booleanProp,
            substanceAvailable: commonSchemas.booleanProp
          },
          description: 'Plugin availability status (for get_asset_plugins_info).'
        },
        
        // Interchange outputs
        translators: { type: 'array', items: { type: 'string' }, description: 'Available translators.' },
        pipelinePath: commonSchemas.stringProp,
        importResult: {
          type: 'object',
          properties: {
            status: { type: 'string', enum: ['Invalid', 'InProgress', 'Done'] },
            importedObjects: { type: 'array', items: { type: 'string' } },
            errors: commonSchemas.arrayOfStrings
          }
        },
        
        // USD outputs
        stageInfo: {
          type: 'object',
          properties: {
            rootLayerPath: commonSchemas.stringProp,
            stageState: commonSchemas.stringProp,
            primCount: commonSchemas.numberProp,
            timeCodesPerSecond: commonSchemas.numberProp,
            startTimeCode: commonSchemas.numberProp,
            endTimeCode: commonSchemas.numberProp
          },
          description: 'USD stage information.'
        },
        primInfo: {
          type: 'object',
          properties: {
            primPath: commonSchemas.stringProp,
            primType: commonSchemas.stringProp,
            isActive: commonSchemas.booleanProp,
            hasPayload: commonSchemas.booleanProp,
            children: { type: 'array', items: { type: 'string' } },
            attributes: { type: 'object' }
          },
          description: 'USD prim information.'
        },
        
        // Alembic outputs
        alembicInfo: {
          type: 'object',
          properties: {
            filePath: commonSchemas.stringProp,
            frameRange: { type: 'object', properties: { start: commonSchemas.numberProp, end: commonSchemas.numberProp } },
            meshCount: commonSchemas.numberProp,
            hasAnimation: commonSchemas.booleanProp
          },
          description: 'Alembic file information.'
        },
        
        // glTF outputs
        gltfMessages: { type: 'array', items: { type: 'string' }, description: 'Export warnings/messages.' },
        exportOptions: {
          type: 'object',
          properties: {
            scale: commonSchemas.numberProp,
            textureFormat: commonSchemas.stringProp,
            useDraco: commonSchemas.booleanProp
          }
        },
        
        // Datasmith outputs
        sceneInfo: {
          type: 'object',
          properties: {
            actorCount: commonSchemas.numberProp,
            meshCount: commonSchemas.numberProp,
            materialCount: commonSchemas.numberProp,
            textureCount: commonSchemas.numberProp,
            lightCount: commonSchemas.numberProp
          },
          description: 'Datasmith scene statistics.'
        },
        
        // SpeedTree outputs
        speedtreeInfo: {
          type: 'object',
          properties: {
            meshPath: commonSchemas.stringProp,
            materialPath: commonSchemas.stringProp,
            lodCount: commonSchemas.numberProp,
            hasWind: commonSchemas.booleanProp,
            windType: commonSchemas.stringProp
          }
        },
        
        // Quixel/Fab outputs
        bridgeStatus: {
          type: 'object',
          properties: {
            connected: commonSchemas.booleanProp,
            bridgeVersion: commonSchemas.stringProp,
            accountEmail: commonSchemas.stringProp
          }
        },
        searchResults: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              assetId: commonSchemas.stringProp,
              name: commonSchemas.stringProp,
              category: commonSchemas.stringProp,
              thumbnailUrl: commonSchemas.stringProp
            }
          }
        },
        
        // Houdini outputs
        hdaParameters: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              name: commonSchemas.stringProp,
              type: commonSchemas.stringProp,
              value: {
                anyOf: [
                  { type: 'string' },
                  { type: 'number' },
                  { type: 'boolean' },
                  { type: 'array', items: true }
                ]
              },
              label: commonSchemas.stringProp
            }
          },
          description: 'HDA parameters list.'
        },
        hdaOutputs: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              outputIndex: commonSchemas.numberProp,
              outputType: commonSchemas.stringProp,
              meshCount: commonSchemas.numberProp
            }
          }
        },
        cookStatus: { type: 'string', enum: ['Idle', 'Cooking', 'Cooked', 'Failed'] },
        
        // Substance outputs
        substanceParameters: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              name: commonSchemas.stringProp,
              type: { type: 'string', enum: ['Float', 'Int', 'Bool', 'Color', 'String', 'Image'] },
              defaultValue: {
                anyOf: [
                  { type: 'string' },
                  { type: 'number' },
                  { type: 'boolean' },
                  { type: 'object', additionalProperties: true }
                ]
              },
              minValue: commonSchemas.numberProp,
              maxValue: commonSchemas.numberProp
            }
          }
        },
        substanceOutputs: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              name: commonSchemas.stringProp,
              usage: commonSchemas.stringProp,
              texturePath: commonSchemas.stringProp
            }
          }
        },
        graphInfo: {
          type: 'object',
          properties: {
            graphName: commonSchemas.stringProp,
            author: commonSchemas.stringProp,
            description: commonSchemas.stringProp,
            category: commonSchemas.stringProp,
            parameterCount: commonSchemas.numberProp,
            outputCount: commonSchemas.numberProp
          }
        }
      }
    }
  },

  // [MERGED] manage_audio_middleware actions now in manage_audio (with mw_ prefix)

  // ===== PHASE 39: MOTION CAPTURE & LIVE LINK =====
  {
    name: 'manage_livelink',
    category: 'utility',
    description: 'Live Link motion capture: sources, subjects, presets, face tracking.',
    annotations: {
      audience: ['developer'],
      priority: 6,
      tags: ['livelink', 'mocap', 'face-tracking', 'motion-capture']
    },
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            // SOURCES (9 actions)
            'add_livelink_source', 'remove_livelink_source', 'list_livelink_sources',
            'get_source_status', 'get_source_type', 'configure_source_settings',
            'add_messagebus_source', 'discover_messagebus_sources', 'remove_all_sources',
            // SUBJECTS (15 actions)
            'list_livelink_subjects', 'get_subject_role', 'get_subject_state',
            'enable_subject', 'disable_subject', 'pause_subject', 'unpause_subject',
            'clear_subject_frames', 'get_subject_static_data', 'get_subject_frame_data',
            'add_virtual_subject', 'remove_virtual_subject', 'configure_subject_settings',
            'get_subject_frame_times', 'get_subjects_by_role',
            // PRESETS (8 actions)
            'create_livelink_preset', 'load_livelink_preset', 'apply_livelink_preset',
            'add_preset_to_client', 'build_preset_from_client', 'save_livelink_preset',
            'get_preset_sources', 'get_preset_subjects',
            // COMPONENTS (8 actions)
            'add_livelink_controller', 'configure_livelink_controller', 'set_controller_subject',
            'set_controller_role', 'enable_controller_evaluation', 'disable_controller_evaluation',
            'set_controlled_component', 'get_controller_info',
            // TIMECODE (6 actions)
            'configure_livelink_timecode', 'set_timecode_provider', 'get_livelink_timecode',
            'configure_time_sync', 'set_buffer_settings', 'configure_frame_interpolation',
            // FACE TRACKING (8 actions)
            'configure_face_source', 'configure_arkit_mapping', 'set_face_neutral_pose',
            'get_face_blendshapes', 'configure_blendshape_remap', 'apply_face_to_skeletal_mesh',
            'configure_face_retargeting', 'get_face_tracking_status',
            // SKELETON MAPPING (6 actions)
            'configure_skeleton_mapping', 'create_retarget_asset', 'configure_bone_mapping',
            'configure_curve_mapping', 'apply_mocap_to_character', 'get_skeleton_mapping_info',
            // UTILITY (4 actions)
            'get_livelink_info', 'list_available_roles', 'list_source_factories', 'force_livelink_tick'
          ],
          description: 'Live Link action.'
        },
        
        // Source identification
        sourceGuid: { type: 'string', description: 'Live Link source GUID.' },
        sourceType: { type: 'string', description: 'Source type (MessageBus, etc.).' },
        sourceName: { type: 'string', description: 'Display name for the source.' },
        connectionString: { type: 'string', description: 'Connection string for source factory.' },
        
        // Subject identification
        subjectName: { type: 'string', description: 'Live Link subject name.' },
        subjectKey: {
          type: 'object',
          properties: {
            source: { type: 'string', description: 'Source GUID.' },
            subjectName: { type: 'string', description: 'Subject name.' }
          },
          description: 'Full subject key (source + name).'
        },
        
        // Role configuration
        roleName: { 
          type: 'string',
          enum: ['Animation', 'Transform', 'Camera', 'Light', 'Basic', 'InputDevice', 'Locator'],
          description: 'Live Link role type.'
        },
        
        // Preset management
        presetPath: commonSchemas.assetPath,
        presetName: { type: 'string', description: 'Preset asset name.' },
        recreateExisting: { type: 'boolean', description: 'Recreate existing sources/subjects when applying preset.' },
        
        // Component controller
        actorName: commonSchemas.actorName,
        componentName: { type: 'string', description: 'Component to control.' },
        controllerClass: { type: 'string', description: 'Controller class name.' },
        updateInEditor: { type: 'boolean', description: 'Update in editor mode.' },
        disableWhenSpawnable: { type: 'boolean', description: 'Disable evaluation when actor is spawnable.' },
        
        // Source settings
        sourceSettings: {
          type: 'object',
          properties: {
            mode: { type: 'string', enum: ['LatestFrame', 'TimeSynchronized', 'ClosestToWorldTime'] },
            bufferOffset: { type: 'number' },
            timecodeFrameOffset: { type: 'number' },
            subjectTimeOffset: { type: 'number' }
          },
          description: 'Source settings configuration.'
        },
        
        // Subject settings
        subjectSettings: {
          type: 'object',
          properties: {
            enabled: { type: 'boolean' },
            rebroadcastSubjectName: { type: 'string' },
            translateToRole: { type: 'string' }
          },
          description: 'Subject settings configuration.'
        },
        
        // Message Bus discovery
        discoveryTimeout: { type: 'number', description: 'Discovery timeout in seconds.' },
        machineAddress: { type: 'string', description: 'Machine address for message bus source.' },
        
        // Virtual subject
        virtualSubjectClass: { type: 'string', description: 'Virtual subject class name.' },
        
        // Timecode
        timecodeSettings: {
          type: 'object',
          properties: {
            useTimecodeProvider: { type: 'boolean' },
            frameRate: { type: 'string' },
            useSystemTime: { type: 'boolean' },
            timecodeSubject: { type: 'string' }
          },
          description: 'Timecode configuration.'
        },
        
        // Buffer/Interpolation settings
        bufferSettings: {
          type: 'object',
          properties: {
            bufferMode: { type: 'string', enum: ['LatestFrame', 'ClosestToTimecode', 'Interpolate'] },
            bufferSize: { type: 'number' },
            maxLatenessCorrectionTime: { type: 'number' }
          },
          description: 'Frame buffer settings.'
        },
        
        interpolationSettings: {
          type: 'object',
          properties: {
            useInterpolation: { type: 'boolean' },
            interpolationOffset: { type: 'number' }
          },
          description: 'Frame interpolation settings.'
        },
        
        // Face tracking (ARKit/Live Link Face)
        blendshapeMapping: {
          type: 'object',
          additionalProperties: { type: 'string' },
          description: 'ARKit blendshape to morph target mapping.'
        },
        neutralPose: {
          type: 'object',
          additionalProperties: { type: 'number' },
          description: 'Neutral pose blendshape values.'
        },
        faceRemapAsset: { type: 'string', description: 'Face remap asset path.' },
        skeletalMeshPath: { type: 'string', description: 'Target skeletal mesh for face data.' },
        
        // Skeleton/Retargeting
        retargetAssetPath: { type: 'string', description: 'Retarget asset path.' },
        boneMapping: {
          type: 'object',
          additionalProperties: { type: 'string' },
          description: 'Source to target bone name mapping.'
        },
        curveMapping: {
          type: 'object',
          additionalProperties: { type: 'string' },
          description: 'Source to target curve name mapping.'
        },
        targetSkeleton: { type: 'string', description: 'Target skeleton asset path.' },
        sourceSkeleton: { type: 'string', description: 'Source skeleton reference.' },
        characterActor: { type: 'string', description: 'Target character actor name.' },
        
        // Include filters
        includeDisabledSubjects: { type: 'boolean', description: 'Include disabled subjects in listings.' },
        includeVirtualSubjects: { type: 'boolean', description: 'Include virtual subjects in listings.' },
        
        // Common
        save: commonSchemas.save
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: commonSchemas.booleanProp,
        message: commonSchemas.stringProp,
        error: commonSchemas.stringProp,
        
        // Source outputs
        sourceGuid: { type: 'string', description: 'Created/queried source GUID.' },
        sources: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              guid: { type: 'string' },
              type: { type: 'string' },
              status: { type: 'string' },
              machineName: { type: 'string' }
            }
          },
          description: 'List of Live Link sources.'
        },
        
        // Subject outputs
        subjects: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              sourceGuid: { type: 'string' },
              subjectName: { type: 'string' },
              role: { type: 'string' },
              enabled: { type: 'boolean' },
              state: { type: 'string' }
            }
          },
          description: 'List of Live Link subjects.'
        },
        subjectState: { 
          type: 'string', 
          enum: ['Connected', 'Unresponsive', 'Disconnected', 'InvalidOrDisabled', 'Paused', 'Unknown'],
          description: 'Subject state.' 
        },
        subjectRole: { type: 'string', description: 'Subject role name.' },
        
        // Frame data outputs
        staticData: {
          type: 'object',
          properties: {
            boneNames: { type: 'array', items: { type: 'string' } },
            boneParents: { type: 'array', items: { type: 'number' } },
            curveNames: { type: 'array', items: { type: 'string' } }
          },
          description: 'Subject static data (skeleton info).'
        },
        frameData: {
          type: 'object',
          properties: {
            worldTime: { type: 'number' },
            timecode: { type: 'string' },
            transforms: { type: 'array', items: { type: 'object' } },
            curveValues: { type: 'array', items: { type: 'number' } }
          },
          description: 'Current frame data.'
        },
        frameTimes: { type: 'array', items: { type: 'number' }, description: 'Available frame times.' },
        
        // Preset outputs
        presetPath: { type: 'string', description: 'Created preset path.' },
        presetSources: { type: 'array', items: { type: 'object' }, description: 'Sources in preset.' },
        presetSubjects: { type: 'array', items: { type: 'object' }, description: 'Subjects in preset.' },
        
        // Controller outputs
        controllerInfo: {
          type: 'object',
          properties: {
            subjectName: { type: 'string' },
            role: { type: 'string' },
            evaluating: { type: 'boolean' },
            controlledComponent: { type: 'string' }
          },
          description: 'Controller component info.'
        },
        
        // Timecode outputs
        currentTimecode: { type: 'string', description: 'Current timecode value.' },
        frameRate: { type: 'string', description: 'Current frame rate.' },
        
        // Face tracking outputs
        blendshapes: { type: 'array', items: { type: 'string' }, description: 'Available blendshape names.' },
        faceTrackingStatus: {
          type: 'object',
          properties: {
            isTracking: { type: 'boolean' },
            deviceName: { type: 'string' },
            subjectName: { type: 'string' }
          },
          description: 'Face tracking status.'
        },
        
        // Skeleton outputs
        mappingInfo: {
          type: 'object',
          properties: {
            boneMappingCount: { type: 'number' },
            curveMappingCount: { type: 'number' },
            targetSkeleton: { type: 'string' }
          },
          description: 'Skeleton mapping info.'
        },
        retargetAssetPath: { type: 'string', description: 'Created retarget asset path.' },
        
        // Discovery outputs
        discoveredSources: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              machineName: { type: 'string' },
              machineAddress: { type: 'string' },
              sourceType: { type: 'string' }
            }
          },
          description: 'Discovered message bus sources.'
        },
        
        // Role/Factory outputs
        availableRoles: { type: 'array', items: { type: 'string' }, description: 'Available Live Link role types.' },
        sourceFactories: { type: 'array', items: { type: 'string' }, description: 'Available source factory types.' },
        
        // Live Link info
        liveLinkInfo: {
          type: 'object',
          properties: {
            isAvailable: { type: 'boolean' },
            sourceCount: { type: 'number' },
            subjectCount: { type: 'number' },
            enabledSubjectCount: { type: 'number' }
          },
          description: 'General Live Link system info.'
        }
      }
    }
  },

  // [MERGED] manage_virtual_production actions now in manage_xr (with vp_ prefix)

  // ============================================
  // Phase 41: XR + Virtual Production - manage_xr
  // ============================================
  // NOTE: VP actions deleted from this location, merged into manage_xr above
  // Original VP actions: create_ndisplay_config, add_cluster_node, remove_cluster_node,
  //                      add_viewport, remove_viewport, set_viewport_camera,
  //                      configure_viewport_region, set_projection_policy, configure_warp_blend, list_cluster_nodes,
  //                      create_led_wall, configure_led_wall_size, configure_icvfx_camera,
  //                      add_icvfx_camera, remove_icvfx_camera, configure_inner_frustum,
  //                      configure_outer_viewport, set_chromakey_settings, configure_light_cards, set_stage_settings,
  //                      set_sync_policy, configure_genlock, set_primary_node,
  //                      configure_network_settings, get_ndisplay_info,
  //                      create_composure_element, delete_composure_element, add_composure_layer,
  //                      remove_composure_layer, attach_child_layer, detach_child_layer,
  //                      add_input_pass, add_transform_pass, add_output_pass,
  //                      configure_chroma_keyer, bind_render_target, get_composure_info,
  //                      create_ocio_config, load_ocio_config, get_ocio_colorspaces,
  //                      get_ocio_displays, set_display_view, add_colorspace_transform,
  //                      apply_ocio_look, configure_viewport_ocio, set_ocio_working_colorspace, get_ocio_info,
  //                      create_remote_control_preset, load_remote_control_preset, expose_property,
  //                      unexpose_property, expose_function, create_controller,
  //                      bind_controller, get_exposed_properties, set_exposed_property_value,
  //                      get_exposed_property_value, start_web_server, stop_web_server,
  //                      get_web_server_status, create_layout_group, get_remote_control_info,
  //                      create_dmx_library, import_gdtf, create_fixture_type,
  //                      add_fixture_mode, add_fixture_function, create_fixture_patch,
  //                      assign_fixture_to_universe, configure_dmx_port, create_artnet_port,
  //                      create_sacn_port, send_dmx, receive_dmx,
  //                      set_fixture_channel_value, get_fixture_channel_value, add_dmx_component,
  //                      configure_dmx_component, list_dmx_universes, list_dmx_fixtures,
  //                      create_dmx_sequencer_track, get_dmx_info,
  //                      create_osc_server, stop_osc_server, create_osc_client, ... (rest merged with vp_ prefix)
  // VP input/output schemas are retained in manage_xr since they share common concepts

  {
    name: 'manage_xr',
    description: 'XR (VR/AR/MR) + Virtual Production (nDisplay, Composure, DMX).',
    annotations: {
      audience: ['developer'],
      priority: 6,
      tags: ['xr', 'vr', 'ar', 'ndisplay', 'virtual-production', 'dmx']
    },
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            // OpenXR Core (20 actions)
            'get_openxr_info', 'configure_openxr_settings', 'set_tracking_origin', 'get_tracking_origin',
            'create_xr_action_set', 'add_xr_action', 'bind_xr_action', 'get_xr_action_state',
            'trigger_haptic_feedback', 'stop_haptic_feedback', 'get_hmd_pose', 'get_controller_pose',
            'get_hand_tracking_data', 'enable_hand_tracking', 'disable_hand_tracking',
            'get_eye_tracking_data', 'enable_eye_tracking', 'get_view_configuration',
            'set_render_scale', 'get_supported_extensions',
            // Meta Quest (22 actions)
            'get_quest_info', 'configure_quest_settings', 'enable_passthrough', 'disable_passthrough',
            'configure_passthrough_style', 'enable_scene_capture', 'get_scene_anchors', 'get_room_layout',
            'enable_quest_hand_tracking', 'get_quest_hand_pose', 'enable_quest_face_tracking',
            'get_quest_face_state', 'enable_quest_eye_tracking', 'get_quest_eye_gaze',
            'enable_quest_body_tracking', 'get_quest_body_state', 'create_spatial_anchor',
            'save_spatial_anchor', 'load_spatial_anchors', 'delete_spatial_anchor',
            'configure_guardian_bounds', 'get_guardian_geometry',
            // SteamVR (18 actions)
            'get_steamvr_info', 'configure_steamvr_settings', 'configure_chaperone_bounds',
            'get_chaperone_geometry', 'create_steamvr_overlay', 'set_overlay_texture',
            'show_overlay', 'hide_overlay', 'destroy_overlay', 'get_tracked_device_count',
            'get_tracked_device_info', 'get_lighthouse_info', 'trigger_steamvr_haptic',
            'get_steamvr_action_manifest', 'set_steamvr_action_manifest', 'enable_steamvr_skeletal_input',
            'get_skeletal_bone_data', 'configure_steamvr_render',
            // Apple ARKit (22 actions)
            'get_arkit_info', 'configure_arkit_session', 'start_arkit_session', 'pause_arkit_session',
            'configure_world_tracking', 'get_tracked_planes', 'get_tracked_images', 'add_reference_image',
            'enable_people_occlusion', 'disable_people_occlusion', 'enable_arkit_face_tracking',
            'get_arkit_face_blendshapes', 'get_arkit_face_geometry', 'enable_body_tracking',
            'get_body_skeleton', 'create_arkit_anchor', 'remove_arkit_anchor', 'get_light_estimation',
            'enable_scene_reconstruction', 'get_scene_mesh', 'perform_raycast', 'get_camera_intrinsics',
            // Google ARCore (18 actions)
            'get_arcore_info', 'configure_arcore_session', 'start_arcore_session', 'pause_arcore_session',
            'get_arcore_planes', 'get_arcore_points', 'create_arcore_anchor', 'remove_arcore_anchor',
            'enable_depth_api', 'get_depth_image', 'enable_geospatial', 'get_geospatial_pose',
            'create_geospatial_anchor', 'resolve_cloud_anchor', 'host_cloud_anchor',
            'enable_arcore_augmented_images', 'get_arcore_light_estimate', 'perform_arcore_raycast',
            // Varjo (16 actions)
            'get_varjo_info', 'configure_varjo_settings', 'enable_varjo_passthrough',
            'disable_varjo_passthrough', 'configure_varjo_depth_test', 'enable_varjo_eye_tracking',
            'get_varjo_gaze_data', 'calibrate_varjo_eye_tracking', 'enable_foveated_rendering',
            'configure_foveated_rendering', 'enable_varjo_mixed_reality', 'configure_varjo_chroma_key',
            'get_varjo_camera_intrinsics', 'enable_varjo_depth_estimation',
            'get_varjo_environment_cubemap', 'configure_varjo_markers',
            // HoloLens (20 actions)
            'get_hololens_info', 'configure_hololens_settings', 'enable_spatial_mapping',
            'disable_spatial_mapping', 'get_spatial_mesh', 'configure_spatial_mapping_quality',
            'enable_scene_understanding', 'get_scene_objects', 'enable_qr_tracking',
            'get_tracked_qr_codes', 'create_world_anchor', 'save_world_anchor', 'load_world_anchors',
            'enable_hololens_hand_tracking', 'get_hololens_hand_mesh', 'enable_hololens_eye_tracking',
            'get_hololens_gaze_ray', 'register_voice_command', 'unregister_voice_command',
            'get_registered_voice_commands',
            // Utilities (6 actions)
            'get_xr_system_info', 'list_xr_devices', 'set_xr_device_priority',
            'reset_xr_orientation', 'configure_xr_spectator', 'get_xr_runtime_name',
            // [VirtualProduction] nDISPLAY - Cluster (10)
            'vp_create_ndisplay_config', 'vp_add_cluster_node', 'vp_remove_cluster_node',
            'vp_add_viewport', 'vp_remove_viewport', 'vp_set_viewport_camera',
            'vp_configure_viewport_region', 'vp_set_projection_policy', 'vp_configure_warp_blend', 'vp_list_cluster_nodes',
            // [VirtualProduction] nDISPLAY - LED/ICVFX (10)
            'vp_create_led_wall', 'vp_configure_led_wall_size', 'vp_configure_icvfx_camera',
            'vp_add_icvfx_camera', 'vp_remove_icvfx_camera', 'vp_configure_inner_frustum',
            'vp_configure_outer_viewport', 'vp_set_chromakey_settings', 'vp_configure_light_cards', 'vp_set_stage_settings',
            // [VirtualProduction] nDISPLAY - Sync (5)
            'vp_set_sync_policy', 'vp_configure_genlock', 'vp_set_primary_node',
            'vp_configure_network_settings', 'vp_get_ndisplay_info',
            // [VirtualProduction] COMPOSURE (12)
            'vp_create_composure_element', 'vp_delete_composure_element', 'vp_add_composure_layer',
            'vp_remove_composure_layer', 'vp_attach_child_layer', 'vp_detach_child_layer',
            'vp_add_input_pass', 'vp_add_transform_pass', 'vp_add_output_pass',
            'vp_configure_chroma_keyer', 'vp_bind_render_target', 'vp_get_composure_info',
            // [VirtualProduction] OCIO (10)
            'vp_create_ocio_config', 'vp_load_ocio_config', 'vp_get_ocio_colorspaces',
            'vp_get_ocio_displays', 'vp_set_display_view', 'vp_add_colorspace_transform',
            'vp_apply_ocio_look', 'vp_configure_viewport_ocio', 'vp_set_ocio_working_colorspace', 'vp_get_ocio_info',
            // [VirtualProduction] REMOTE CONTROL (15)
            'vp_create_remote_control_preset', 'vp_load_remote_control_preset', 'vp_expose_property',
            'vp_unexpose_property', 'vp_expose_function', 'vp_create_controller',
            'vp_bind_controller', 'vp_get_exposed_properties', 'vp_set_exposed_property_value',
            'vp_get_exposed_property_value', 'vp_start_web_server', 'vp_stop_web_server',
            'vp_get_web_server_status', 'vp_create_layout_group', 'vp_get_remote_control_info',
            // [VirtualProduction] DMX (20)
            'vp_create_dmx_library', 'vp_import_gdtf', 'vp_create_fixture_type',
            'vp_add_fixture_mode', 'vp_add_fixture_function', 'vp_create_fixture_patch',
            'vp_assign_fixture_to_universe', 'vp_configure_dmx_port', 'vp_create_artnet_port',
            'vp_create_sacn_port', 'vp_send_dmx', 'vp_receive_dmx',
            'vp_set_fixture_channel_value', 'vp_get_fixture_channel_value', 'vp_add_dmx_component',
            'vp_configure_dmx_component', 'vp_list_dmx_universes', 'vp_list_dmx_fixtures',
            'vp_create_dmx_sequencer_track', 'vp_get_dmx_info',
            // [VirtualProduction] OSC (12)
            'vp_create_osc_server', 'vp_stop_osc_server', 'vp_create_osc_client',
            'send_osc_message', 'send_osc_bundle', 'bind_osc_address',
            'unbind_osc_address', 'bind_osc_to_property', 'list_osc_servers',
            'list_osc_clients', 'configure_osc_dispatcher', 'get_osc_info',
            // MIDI (15)
            'list_midi_devices', 'open_midi_input', 'close_midi_input',
            'open_midi_output', 'close_midi_output', 'send_midi_note_on',
            'send_midi_note_off', 'send_midi_cc', 'send_midi_pitch_bend',
            'send_midi_program_change', 'bind_midi_to_property', 'unbind_midi',
            'configure_midi_learn', 'add_midi_device_component', 'get_midi_info',
            // TIMECODE (18)
            'create_timecode_provider', 'set_timecode_provider', 'get_current_timecode',
            'set_frame_rate', 'configure_ltc_timecode', 'configure_aja_timecode',
            'configure_blackmagic_timecode', 'configure_system_time_timecode', 'enable_timecode_genlock',
            'disable_timecode_genlock', 'set_custom_timestep', 'configure_genlock_source',
            'get_timecode_provider_status', 'synchronize_timecode', 'create_timecode_synchronizer',
            'add_timecode_source', 'list_timecode_providers', 'get_timecode_info',
            // UTILITY (3)
            'get_virtual_production_info', 'list_active_vp_sessions', 'reset_vp_state'
          ],
          description: 'Virtual production action.'
        },

        // Common identifiers
        name: commonSchemas.name,
        actorName: commonSchemas.actorName,
        assetPath: commonSchemas.assetPath,

        // nDisplay configuration
        configPath: { type: 'string', description: 'nDisplay configuration asset path.' },
        nodeName: { type: 'string', description: 'Cluster node name.' },
        nodeId: { type: 'string', description: 'Cluster node ID.' },
        viewportId: { type: 'string', description: 'Viewport ID.' },
        viewportName: { type: 'string', description: 'Viewport name.' },
        cameraComponent: { type: 'string', description: 'Camera component name for viewport.' },
        projectionPolicy: {
          type: 'string',
          enum: ['simple', 'mesh', 'mpcdi', 'camera', 'manual', 'link'],
          description: 'Projection policy type.'
        },
        projectionMesh: { type: 'string', description: 'Projection mesh path for mesh policy.' },
        mpcdiFilePath: { type: 'string', description: 'MPCDI file path.' },
        mpcdiBufferId: { type: 'string', description: 'MPCDI buffer ID.' },
        mpcdiRegionId: { type: 'string', description: 'MPCDI region ID.' },
        viewportRegion: {
          type: 'object',
          properties: {
            x: { type: 'number' }, y: { type: 'number' },
            width: { type: 'number' }, height: { type: 'number' }
          },
          description: 'Viewport region (normalized 0-1).'
        },

        // nDisplay cluster settings
        hostAddress: { type: 'string', description: 'Node host IP address.' },
        hostPort: { type: 'number', description: 'Node port number.' },
        isPrimary: { type: 'boolean', description: 'Mark as primary/master node.' },
        syncPolicy: {
          type: 'string',
          enum: ['ethernet', 'nvidia', 'none'],
          description: 'Render sync policy.'
        },
        swapSyncType: {
          type: 'string',
          enum: ['none', 'soft', 'nvidia'],
          description: 'Swap synchronization type.'
        },

        // LED Wall / ICVFX
        ledWallSize: {
          type: 'object',
          properties: {
            width: { type: 'number' }, height: { type: 'number' }
          },
          description: 'LED wall dimensions in cm.'
        },
        icvfxCameraName: { type: 'string', description: 'ICVFX camera name.' },
        innerFrustumSettings: {
          type: 'object',
          properties: {
            enabled: { type: 'boolean' },
            fov: { type: 'number' },
            renderTargetRatio: { type: 'number' },
            overscanPercent: { type: 'number' }
          },
          description: 'Inner frustum settings.'
        },
        chromakeySettings: {
          type: 'object',
          properties: {
            enabled: { type: 'boolean' },
            color: { type: 'object', properties: { r: { type: 'number' }, g: { type: 'number' }, b: { type: 'number' } } },
            markerColor: { type: 'object', properties: { r: { type: 'number' }, g: { type: 'number' }, b: { type: 'number' } } }
          },
          description: 'Chromakey/greenscreen settings.'
        },
        lightCardActor: { type: 'string', description: 'Light card actor name.' },

        // Composure
        elementName: { type: 'string', description: 'Composure element name.' },
        elementClass: {
          type: 'string',
          enum: ['CompositingElement', 'CompositingCaptureBase', 'CompositingMediaCaptureOutput'],
          description: 'Composure element class.'
        },
        parentElement: { type: 'string', description: 'Parent composure element.' },
        childElement: { type: 'string', description: 'Child composure element.' },
        passType: {
          type: 'string',
          enum: ['MediaCapture', 'SceneCapture', 'Texture'],
          description: 'Input pass type.'
        },
        transformPassClass: {
          type: 'string',
          enum: ['ChromaKeying', 'ColorCorrect', 'SetAlpha', 'Custom'],
          description: 'Transform pass type.'
        },
        chromaKeyerSettings: {
          type: 'object',
          properties: {
            keyColorA: { type: 'object', properties: { r: { type: 'number' }, g: { type: 'number' }, b: { type: 'number' } } },
            keyColorB: { type: 'object', properties: { r: { type: 'number' }, g: { type: 'number' }, b: { type: 'number' } } },
            luminanceRangeMin: { type: 'number' },
            luminanceRangeMax: { type: 'number' },
            clipBlack: { type: 'number' },
            clipWhite: { type: 'number' }
          },
          description: 'Chroma keyer settings.'
        },
        renderTargetPath: { type: 'string', description: 'Render target asset path.' },

        // OCIO
        ocioConfigPath: { type: 'string', description: 'Path to OCIO .ocio config file.' },
        sourceColorspace: { type: 'string', description: 'Source colorspace name.' },
        destColorspace: { type: 'string', description: 'Destination colorspace name.' },
        displayName: { type: 'string', description: 'OCIO display name.' },
        viewName: { type: 'string', description: 'OCIO view name.' },
        lookName: { type: 'string', description: 'OCIO look name.' },
        workingColorspace: { type: 'string', description: 'Working colorspace for project.' },

        // Remote Control
        presetName: { type: 'string', description: 'Remote Control preset name.' },
        presetPath: { type: 'string', description: 'Remote Control preset asset path.' },
        propertyPath: { type: 'string', description: 'Property path to expose.' },
        propertyLabel: { type: 'string', description: 'Display label for exposed property.' },
        functionPath: { type: 'string', description: 'Function path to expose.' },
        controllerType: {
          type: 'string',
          enum: ['Slider', 'Toggle', 'ColorPicker', 'Dropdown', 'TextBox'],
          description: 'Remote control UI widget type.'
        },
        controllerSettings: {
          type: 'object',
          properties: {
            min: { type: 'number' },
            max: { type: 'number' },
            step: { type: 'number' }
          },
          description: 'Controller widget settings.'
        },
        webServerPort: { type: 'number', description: 'Web server port (default 30010).' },
        layoutGroupName: { type: 'string', description: 'Layout group name.' },

        // DMX
        libraryPath: { type: 'string', description: 'DMX library asset path.' },
        gdtfFilePath: { type: 'string', description: 'GDTF file path to import.' },
        fixtureTypeName: { type: 'string', description: 'Fixture type name.' },
        fixtureMode: { type: 'string', description: 'Fixture mode name.' },
        channelCount: { type: 'number', description: 'Number of DMX channels.' },
        fixtureFunction: {
          type: 'object',
          properties: {
            name: { type: 'string' },
            channel: { type: 'number' },
            channelSpan: { type: 'number' },
            defaultValue: { type: 'number' },
            functionType: { type: 'string', enum: ['Intensity', 'Color', 'Pan', 'Tilt', 'Gobo', 'Other'] }
          },
          description: 'Fixture function definition.'
        },
        patchName: { type: 'string', description: 'Fixture patch name.' },
        fixtureId: { type: 'number', description: 'Fixture ID in patch.' },
        universeId: { type: 'number', description: 'DMX universe ID.' },
        startingChannel: { type: 'number', description: 'Starting DMX channel (1-512).' },
        portType: {
          type: 'string',
          enum: ['Input', 'Output'],
          description: 'DMX port direction.'
        },
        protocol: {
          type: 'string',
          enum: ['ArtNet', 'sACN'],
          description: 'DMX protocol.'
        },
        networkInterface: { type: 'string', description: 'Network interface IP.' },
        dmxData: {
          type: 'array',
          items: { type: 'number' },
          description: 'Array of DMX channel values (0-255).'
        },
        channelIndex: { type: 'number', description: 'DMX channel index (0-511).' },
        channelValue: { type: 'number', description: 'DMX channel value (0-255).' },
        sequencePath: { type: 'string', description: 'Level sequence path for DMX track.' },

        // OSC
        oscServerName: { type: 'string', description: 'OSC server name.' },
        oscClientName: { type: 'string', description: 'OSC client name.' },
        ipAddress: { type: 'string', description: 'IP address for OSC.' },
        port: { type: 'number', description: 'Port number.' },
        oscAddress: { type: 'string', description: 'OSC address pattern (e.g., /my/address).' },
        oscArgs: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              type: { type: 'string', enum: ['float', 'int', 'string', 'bool', 'blob'] },
              value: {}
            }
          },
          description: 'OSC message arguments.'
        },
        targetProperty: { type: 'string', description: 'Target property path for OSC binding.' },
        multicast: { type: 'boolean', description: 'Enable multicast.' },
        loopback: { type: 'boolean', description: 'Enable loopback.' },

        // MIDI
        midiDeviceId: { type: 'number', description: 'MIDI device ID.' },
        midiDeviceName: { type: 'string', description: 'MIDI device name.' },
        midiChannel: { type: 'number', description: 'MIDI channel (0-15).' },
        noteNumber: { type: 'number', description: 'MIDI note number (0-127).' },
        velocity: { type: 'number', description: 'MIDI velocity (0-127).' },
        controlNumber: { type: 'number', description: 'MIDI CC number (0-127).' },
        controlValue: { type: 'number', description: 'MIDI CC value (0-127).' },
        pitchBendValue: { type: 'number', description: 'MIDI pitch bend (-8192 to 8191).' },
        programNumber: { type: 'number', description: 'MIDI program number (0-127).' },
        midiLearnEnabled: { type: 'boolean', description: 'Enable MIDI learn mode.' },

        // Timecode
        timecodeProviderType: {
          type: 'string',
          enum: ['System', 'LTC', 'AJA', 'Blackmagic', 'Custom'],
          description: 'Timecode provider type.'
        },
        frameRate: {
          type: 'string',
          enum: ['23.976', '24', '25', '29.97', '29.97df', '30', '30df', '48', '50', '59.94', '60'],
          description: 'Frame rate.'
        },
        ltcSettings: {
          type: 'object',
          properties: {
            audioInputDevice: { type: 'string' },
            channel: { type: 'number' },
            volume: { type: 'number' }
          },
          description: 'LTC timecode settings.'
        },
        ajaSettings: {
          type: 'object',
          properties: {
            deviceIndex: { type: 'number' },
            inputChannel: { type: 'number' },
            ltcSource: { type: 'boolean' }
          },
          description: 'AJA timecode settings.'
        },
        blackmagicSettings: {
          type: 'object',
          properties: {
            deviceIndex: { type: 'number' },
            timecodeFormat: { type: 'string' }
          },
          description: 'Blackmagic timecode settings.'
        },
        genlockEnabled: { type: 'boolean', description: 'Enable genlock.' },
        genlockSource: {
          type: 'string',
          enum: ['Internal', 'External', 'AJA', 'Blackmagic', 'nDisplay'],
          description: 'Genlock source.'
        },
        customTimestepClass: { type: 'string', description: 'Custom timestep class path.' },
        synchronizerId: { type: 'string', description: 'Timecode synchronizer ID.' },

        // Common
        save: commonSchemas.save
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: commonSchemas.booleanProp,
        message: commonSchemas.stringProp,
        error: commonSchemas.stringProp,

        // nDisplay outputs
        configPath: { type: 'string', description: 'Created config path.' },
        nodeId: { type: 'string', description: 'Created/modified node ID.' },
        viewportId: { type: 'string', description: 'Created/modified viewport ID.' },
        clusterNodes: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              id: { type: 'string' },
              hostAddress: { type: 'string' },
              isPrimary: { type: 'boolean' },
              viewports: { type: 'array', items: { type: 'string' } }
            }
          },
          description: 'List of cluster nodes.'
        },
        ndisplayInfo: {
          type: 'object',
          properties: {
            configPath: { type: 'string' },
            nodeCount: { type: 'number' },
            viewportCount: { type: 'number' },
            syncPolicy: { type: 'string' },
            primaryNode: { type: 'string' }
          },
          description: 'nDisplay configuration info.'
        },

        // Composure outputs
        elementPath: { type: 'string', description: 'Created composure element path.' },
        passIndex: { type: 'number', description: 'Created pass index.' },
        composureInfo: {
          type: 'object',
          properties: {
            elementCount: { type: 'number' },
            elements: { type: 'array', items: { type: 'string' } },
            activeElement: { type: 'string' }
          },
          description: 'Composure elements info.'
        },

        // OCIO outputs
        colorspaces: { type: 'array', items: { type: 'string' }, description: 'Available colorspaces.' },
        displays: { type: 'array', items: { type: 'string' }, description: 'Available displays.' },
        views: { type: 'array', items: { type: 'string' }, description: 'Available views.' },
        ocioInfo: {
          type: 'object',
          properties: {
            configPath: { type: 'string' },
            workingColorspace: { type: 'string' },
            colorspaceCount: { type: 'number' },
            displayCount: { type: 'number' }
          },
          description: 'OCIO configuration info.'
        },

        // Remote Control outputs
        presetPath: { type: 'string', description: 'Preset path.' },
        exposedProperties: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              label: { type: 'string' },
              path: { type: 'string' },
              type: { type: 'string' }
            }
          },
          description: 'Exposed properties.'
        },
        propertyValue: { description: 'Retrieved property value.' },
        webServerStatus: {
          type: 'object',
          properties: {
            running: { type: 'boolean' },
            port: { type: 'number' },
            url: { type: 'string' }
          },
          description: 'Web server status.'
        },
        remoteControlInfo: {
          type: 'object',
          properties: {
            presetCount: { type: 'number' },
            webServerRunning: { type: 'boolean' },
            webServerPort: { type: 'number' }
          },
          description: 'Remote control info.'
        },

        // DMX outputs
        libraryPath: { type: 'string', description: 'DMX library path.' },
        fixtureTypeId: { type: 'string', description: 'Created fixture type ID.' },
        patchId: { type: 'string', description: 'Created fixture patch ID.' },
        universes: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              id: { type: 'number' },
              name: { type: 'string' },
              protocol: { type: 'string' }
            }
          },
          description: 'DMX universes.'
        },
        fixtures: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              id: { type: 'number' },
              name: { type: 'string' },
              type: { type: 'string' },
              universe: { type: 'number' },
              startChannel: { type: 'number' }
            }
          },
          description: 'DMX fixtures.'
        },
        dmxData: { type: 'array', items: { type: 'number' }, description: 'Received DMX data.' },
        dmxInfo: {
          type: 'object',
          properties: {
            universeCount: { type: 'number' },
            fixtureCount: { type: 'number' },
            artnetPortCount: { type: 'number' },
            sacnPortCount: { type: 'number' }
          },
          description: 'DMX system info.'
        },

        // OSC outputs
        serverId: { type: 'string', description: 'OSC server ID.' },
        clientId: { type: 'string', description: 'OSC client ID.' },
        oscServers: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              name: { type: 'string' },
              port: { type: 'number' },
              listening: { type: 'boolean' }
            }
          },
          description: 'OSC servers.'
        },
        oscClients: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              name: { type: 'string' },
              address: { type: 'string' },
              port: { type: 'number' }
            }
          },
          description: 'OSC clients.'
        },
        oscInfo: {
          type: 'object',
          properties: {
            serverCount: { type: 'number' },
            clientCount: { type: 'number' },
            bindingCount: { type: 'number' }
          },
          description: 'OSC system info.'
        },

        // MIDI outputs
        midiDevices: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              id: { type: 'number' },
              name: { type: 'string' },
              isInput: { type: 'boolean' },
              isOutput: { type: 'boolean' }
            }
          },
          description: 'Available MIDI devices.'
        },
        midiInputId: { type: 'number', description: 'Opened MIDI input controller ID.' },
        midiOutputId: { type: 'number', description: 'Opened MIDI output controller ID.' },
        midiInfo: {
          type: 'object',
          properties: {
            inputDeviceCount: { type: 'number' },
            outputDeviceCount: { type: 'number' },
            activeInputs: { type: 'number' },
            activeOutputs: { type: 'number' }
          },
          description: 'MIDI system info.'
        },

        // Timecode outputs
        currentTimecode: { type: 'string', description: 'Current timecode (HH:MM:SS:FF).' },
        frameRate: { type: 'string', description: 'Current frame rate.' },
        providerStatus: {
          type: 'object',
          properties: {
            type: { type: 'string' },
            synchronized: { type: 'boolean' },
            frameRate: { type: 'string' },
            dropFrame: { type: 'boolean' }
          },
          description: 'Timecode provider status.'
        },
        timecodeProviders: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              type: { type: 'string' },
              active: { type: 'boolean' }
            }
          },
          description: 'Available timecode providers.'
        },
        timecodeInfo: {
          type: 'object',
          properties: {
            providerType: { type: 'string' },
            frameRate: { type: 'string' },
            genlockEnabled: { type: 'boolean' },
            genlockSource: { type: 'string' }
          },
          description: 'Timecode system info.'
        },

        // Virtual Production info
        vpInfo: {
          type: 'object',
          properties: {
            ndisplayAvailable: { type: 'boolean' },
            composureAvailable: { type: 'boolean' },
            ocioAvailable: { type: 'boolean' },
            remoteControlAvailable: { type: 'boolean' },
            dmxAvailable: { type: 'boolean' },
            oscAvailable: { type: 'boolean' },
            midiAvailable: { type: 'boolean' },
            timecodeAvailable: { type: 'boolean' }
          },
          description: 'Virtual production plugin availability.'
        },
        activeSessions: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              type: { type: 'string' },
              name: { type: 'string' },
              status: { type: 'string' }
            }
          },
          description: 'Active VP sessions.'
        }
      }
    }
  },

  // [MERGED] manage_ai_npc actions now in manage_ai

  // [MERGED] manage_utility_plugins actions now in manage_asset_plugins (with util_ prefix)

  // [MERGED] manage_physics_destruction actions now in animation_physics (with chaos_ prefix)

  // ===== PHASE 45: ACCESSIBILITY SYSTEM =====
  {
    name: 'manage_accessibility',
    category: 'utility',
    description: 'Accessibility: colorblind, subtitles, audio, motor, cognitive.',
    annotations: {
      audience: ['developer', 'designer'],
      priority: 6,
      tags: ['accessibility', 'colorblind', 'subtitles', 'motor', 'cognitive']
    },
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            // VISUAL ACCESSIBILITY (10 actions)
            'create_colorblind_filter', 'configure_colorblind_mode', 'set_colorblind_severity',
            'configure_high_contrast_mode', 'set_high_contrast_colors', 'set_ui_scale',
            'configure_text_to_speech', 'set_font_size', 'configure_screen_reader',
            'set_visual_accessibility_preset',
            // SUBTITLE ACCESSIBILITY (8 actions)
            'create_subtitle_widget', 'configure_subtitle_style', 'set_subtitle_font_size',
            'configure_subtitle_background', 'configure_speaker_identification',
            'add_directional_indicators', 'configure_subtitle_timing', 'set_subtitle_preset',
            // AUDIO ACCESSIBILITY (8 actions)
            'configure_mono_audio', 'configure_audio_visualization', 'create_sound_indicator_widget',
            'configure_visual_sound_cues', 'set_audio_ducking', 'configure_screen_narrator',
            'set_audio_balance', 'set_audio_accessibility_preset',
            // MOTOR ACCESSIBILITY (10 actions)
            'configure_control_remapping', 'create_control_remapping_ui', 'configure_hold_vs_toggle',
            'configure_auto_aim_strength', 'configure_one_handed_mode', 'set_input_timing_tolerance',
            'configure_button_holds', 'configure_quick_time_events', 'set_cursor_size',
            'set_motor_accessibility_preset',
            // COGNITIVE ACCESSIBILITY (8 actions)
            'configure_difficulty_presets', 'configure_objective_reminders', 'configure_navigation_assistance',
            'configure_motion_sickness_options', 'set_game_speed', 'configure_tutorial_options',
            'configure_ui_simplification', 'set_cognitive_accessibility_preset',
            // PRESETS & UTILITIES (6 actions)
            'create_accessibility_preset', 'apply_accessibility_preset', 'export_accessibility_settings',
            'import_accessibility_settings', 'get_accessibility_info', 'reset_accessibility_defaults'
          ],
          description: 'Accessibility action.'
        },

        // === Common Parameters ===
        actorName: commonSchemas.actorName,
        assetPath: commonSchemas.assetPath,
        assetName: { type: 'string', description: 'Name for the new asset.' },
        widgetName: { type: 'string', description: 'Widget name.' },
        presetName: { type: 'string', description: 'Accessibility preset name.' },
        save: commonSchemas.save,

        // === Visual Accessibility Parameters ===
        colorblindMode: {
          type: 'string',
          enum: ['None', 'Deuteranopia', 'Protanopia', 'Tritanopia'],
          description: 'Colorblind filter mode.'
        },
        colorblindSeverity: { type: 'number', description: 'Colorblind filter severity (0.0-1.0).' },
        highContrastEnabled: { type: 'boolean', description: 'Enable high contrast mode.' },
        highContrastColors: {
          type: 'object',
          properties: {
            background: { type: 'string', description: 'Background color (hex).' },
            foreground: { type: 'string', description: 'Foreground/text color (hex).' },
            highlight: { type: 'string', description: 'Highlight color (hex).' },
            interactive: { type: 'string', description: 'Interactive elements color (hex).' }
          },
          description: 'High contrast color scheme.'
        },
        uiScale: { type: 'number', description: 'UI scale factor (0.5-3.0).' },
        textToSpeechEnabled: { type: 'boolean', description: 'Enable text-to-speech.' },
        textToSpeechRate: { type: 'number', description: 'TTS speech rate (0.5-2.0).' },
        textToSpeechVolume: { type: 'number', description: 'TTS volume (0.0-1.0).' },
        fontSize: { type: 'number', description: 'Font size in points.' },
        fontSizeMultiplier: { type: 'number', description: 'Font size multiplier.' },
        screenReaderEnabled: { type: 'boolean', description: 'Enable screen reader support.' },

        // === Subtitle Parameters ===
        subtitleEnabled: { type: 'boolean', description: 'Enable subtitles.' },
        subtitleFontSize: { type: 'number', description: 'Subtitle font size.' },
        subtitleFontFamily: { type: 'string', description: 'Subtitle font family.' },
        subtitleColor: { type: 'string', description: 'Subtitle text color (hex).' },
        subtitleBackgroundEnabled: { type: 'boolean', description: 'Enable subtitle background.' },
        subtitleBackgroundColor: { type: 'string', description: 'Subtitle background color (hex).' },
        subtitleBackgroundOpacity: { type: 'number', description: 'Background opacity (0.0-1.0).' },
        speakerIdentificationEnabled: { type: 'boolean', description: 'Show speaker names.' },
        speakerColorCodingEnabled: { type: 'boolean', description: 'Color-code speakers.' },
        directionalIndicatorsEnabled: { type: 'boolean', description: 'Show directional indicators for sounds.' },
        subtitleDisplayTime: { type: 'number', description: 'Minimum display time in seconds.' },
        subtitlePosition: {
          type: 'string',
          enum: ['Bottom', 'Top', 'BottomLeft', 'BottomRight'],
          description: 'Subtitle position on screen.'
        },

        // === Audio Accessibility Parameters ===
        monoAudioEnabled: { type: 'boolean', description: 'Enable mono audio.' },
        audioVisualizationEnabled: { type: 'boolean', description: 'Enable visual audio cues.' },
        visualSoundCuesEnabled: { type: 'boolean', description: 'Show visual indicators for sounds.' },
        soundIndicatorPosition: {
          type: 'string',
          enum: ['TopRight', 'TopLeft', 'BottomRight', 'BottomLeft', 'Center'],
          description: 'Sound indicator widget position.'
        },
        audioDuckingEnabled: { type: 'boolean', description: 'Enable audio ducking for speech.' },
        audioDuckingAmount: { type: 'number', description: 'Audio ducking amount (0.0-1.0).' },
        screenNarratorEnabled: { type: 'boolean', description: 'Enable screen narrator.' },
        audioBalance: { type: 'number', description: 'Audio balance (-1.0 left to 1.0 right).' },

        // === Motor Accessibility Parameters ===
        holdToToggleEnabled: { type: 'boolean', description: 'Convert hold actions to toggle.' },
        autoAimEnabled: { type: 'boolean', description: 'Enable auto-aim assistance.' },
        autoAimStrength: { type: 'number', description: 'Auto-aim strength (0.0-1.0).' },
        oneHandedModeEnabled: { type: 'boolean', description: 'Enable one-handed mode.' },
        oneHandedModeHand: { type: 'string', enum: ['Left', 'Right'], description: 'Which hand for one-handed mode.' },
        inputTimingTolerance: { type: 'number', description: 'Input timing tolerance multiplier.' },
        buttonHoldTime: { type: 'number', description: 'Button hold time in seconds.' },
        qteTimeMultiplier: { type: 'number', description: 'QTE time multiplier.' },
        qteAutoComplete: { type: 'boolean', description: 'Auto-complete QTEs.' },
        cursorSize: { type: 'number', description: 'Cursor size multiplier.' },
        cursorHighContrastEnabled: { type: 'boolean', description: 'High contrast cursor.' },

        // === Cognitive Accessibility Parameters ===
        difficultyPreset: {
          type: 'string',
          enum: ['Easy', 'Normal', 'Hard', 'Custom', 'Assisted'],
          description: 'Difficulty preset.'
        },
        objectiveRemindersEnabled: { type: 'boolean', description: 'Enable objective reminders.' },
        objectiveReminderInterval: { type: 'number', description: 'Reminder interval in seconds.' },
        navigationAssistanceEnabled: { type: 'boolean', description: 'Enable navigation assistance.' },
        navigationAssistanceType: {
          type: 'string',
          enum: ['Waypoint', 'PathLine', 'Compass', 'All'],
          description: 'Type of navigation assistance.'
        },
        motionSicknessReductionEnabled: { type: 'boolean', description: 'Enable motion sickness reduction.' },
        cameraShakeEnabled: { type: 'boolean', description: 'Enable camera shake effects.' },
        headBobEnabled: { type: 'boolean', description: 'Enable head bob.' },
        motionBlurEnabled: { type: 'boolean', description: 'Enable motion blur.' },
        fovAdjustment: { type: 'number', description: 'FOV adjustment in degrees.' },
        gameSpeedMultiplier: { type: 'number', description: 'Game speed multiplier (0.5-2.0).' },
        tutorialHintsEnabled: { type: 'boolean', description: 'Enable tutorial hints.' },
        simplifiedUIEnabled: { type: 'boolean', description: 'Enable simplified UI mode.' },

        // === Control Remapping Parameters ===
        actionName: { type: 'string', description: 'Input action name to remap.' },
        newBinding: { type: 'string', description: 'New key/button binding.' },
        inputMappingContext: { type: 'string', description: 'Input mapping context path.' },

        // === Preset Parameters ===
        presetPath: { type: 'string', description: 'Preset asset or file path.' },
        exportPath: { type: 'string', description: 'Export file path.' },
        importPath: { type: 'string', description: 'Import file path.' },
        exportFormat: { type: 'string', enum: ['json', 'ini'], description: 'Export format.' },

        // === Filter ===
        category: {
          type: 'string',
          enum: ['Visual', 'Subtitle', 'Audio', 'Motor', 'Cognitive', 'All'],
          description: 'Accessibility category filter.'
        }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        ...commonSchemas.outputBase,
        message: { type: 'string', description: 'Status message.' },

        // === Visual Outputs ===
        colorblindFilterApplied: { type: 'boolean', description: 'Whether colorblind filter was applied.' },
        currentColorblindMode: { type: 'string', description: 'Current colorblind mode.' },
        highContrastApplied: { type: 'boolean', description: 'Whether high contrast was applied.' },
        uiScaleApplied: { type: 'boolean', description: 'Whether UI scale was applied.' },
        currentUIScale: { type: 'number', description: 'Current UI scale.' },

        // === Subtitle Outputs ===
        subtitleWidgetCreated: { type: 'boolean', description: 'Whether subtitle widget was created.' },
        subtitleWidgetPath: { type: 'string', description: 'Subtitle widget asset path.' },
        subtitleConfigApplied: { type: 'boolean', description: 'Whether subtitle config was applied.' },

        // === Audio Outputs ===
        monoAudioApplied: { type: 'boolean', description: 'Whether mono audio was applied.' },
        audioVisualizationEnabled: { type: 'boolean', description: 'Whether audio visualization is enabled.' },
        soundIndicatorWidgetCreated: { type: 'boolean', description: 'Whether sound indicator widget was created.' },

        // === Motor Outputs ===
        remappingApplied: { type: 'boolean', description: 'Whether control remapping was applied.' },
        remappingUICreated: { type: 'boolean', description: 'Whether remapping UI was created.' },
        autoAimApplied: { type: 'boolean', description: 'Whether auto-aim was applied.' },
        currentAutoAimStrength: { type: 'number', description: 'Current auto-aim strength.' },

        // === Cognitive Outputs ===
        difficultyApplied: { type: 'boolean', description: 'Whether difficulty was applied.' },
        currentDifficulty: { type: 'string', description: 'Current difficulty preset.' },
        navigationAssistanceApplied: { type: 'boolean', description: 'Whether navigation assistance was applied.' },
        motionSicknessOptionsApplied: { type: 'boolean', description: 'Whether motion sickness options were applied.' },

        // === Preset Outputs ===
        presetCreated: { type: 'boolean', description: 'Whether preset was created.' },
        presetApplied: { type: 'boolean', description: 'Whether preset was applied.' },
        presetPath: { type: 'string', description: 'Preset asset path.' },
        settingsExported: { type: 'boolean', description: 'Whether settings were exported.' },
        settingsImported: { type: 'boolean', description: 'Whether settings were imported.' },
        exportPath: { type: 'string', description: 'Export file path.' },

        // === Info Output ===
        accessibilityInfo: {
          type: 'object',
          properties: {
            visualSettings: {
              type: 'object',
              properties: {
                colorblindMode: { type: 'string' },
                colorblindSeverity: { type: 'number' },
                highContrastEnabled: { type: 'boolean' },
                uiScale: { type: 'number' },
                textToSpeechEnabled: { type: 'boolean' }
              }
            },
            subtitleSettings: {
              type: 'object',
              properties: {
                enabled: { type: 'boolean' },
                fontSize: { type: 'number' },
                speakerIdentification: { type: 'boolean' },
                directionalIndicators: { type: 'boolean' }
              }
            },
            audioSettings: {
              type: 'object',
              properties: {
                monoAudio: { type: 'boolean' },
                audioVisualization: { type: 'boolean' },
                audioBalance: { type: 'number' }
              }
            },
            motorSettings: {
              type: 'object',
              properties: {
                holdToToggle: { type: 'boolean' },
                autoAimEnabled: { type: 'boolean' },
                autoAimStrength: { type: 'number' },
                oneHandedMode: { type: 'boolean' }
              }
            },
            cognitiveSettings: {
              type: 'object',
              properties: {
                difficultyPreset: { type: 'string' },
                objectiveReminders: { type: 'boolean' },
                navigationAssistance: { type: 'boolean' },
                motionSicknessReduction: { type: 'boolean' }
              }
            }
          },
          description: 'Current accessibility settings.'
        },

        error: commonSchemas.stringProp
      }
    }
  },
  {
    name: 'manage_ui',
    category: 'utility',
    description: 'Runtime UI management: spawn widgets, hierarchy, viewport control.',
    annotations: {
      audience: ['developer', 'designer'],
      priority: 6,
      tags: ['ui', 'widget', 'viewport', 'hud', 'runtime']
    },
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'create_widget', 'remove_widget_from_viewport', 'get_all_widgets', 'get_widget_hierarchy',
            'set_input_mode', 'show_mouse_cursor', 'set_widget_visibility'
          ],
          description: 'UI action'
        },
        widgetPath: commonSchemas.widgetPath,
        addToViewport: commonSchemas.booleanProp,
        zOrder: commonSchemas.integerProp,
        key: { type: 'string', description: 'Widget key or name' },
        visible: commonSchemas.booleanProp,
        showCursor: commonSchemas.booleanProp,
        inputMode: { type: 'string', enum: ['GameOnly', 'UIOnly', 'GameAndUI'] }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        ...commonSchemas.outputBase,
        widgets: commonSchemas.arrayOfObjects,
        hierarchy: commonSchemas.objectProp,
        widgetName: commonSchemas.stringProp
      }
    }
  },
  {
    name: 'manage_gameplay_abilities',
    category: 'gameplay',
    description: 'Create and configure Gameplay Abilities, Effects, and Ability Tasks.',
    annotations: {
      audience: ['developer', 'designer'],
      priority: 7,
      tags: ['gas', 'gameplay-ability', 'gameplay-effect', 'ability-task']
    },
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            // Abilities
            'create_gameplay_ability', 'set_ability_tags', 'set_ability_costs', 'set_ability_cooldown',
            'set_ability_targeting', 'add_ability_task', 'set_activation_policy', 'set_instancing_policy',
            // Effects
            'create_gameplay_effect', 'set_effect_duration', 'add_effect_modifier', 'set_modifier_magnitude',
            'add_effect_execution_calculation', 'add_effect_cue', 'set_effect_stacking', 'set_effect_tags',
            // Common
            'add_tag_to_asset', 'get_gas_info'
          ],
          description: 'Action to perform'
        },
        name: commonSchemas.name,
        path: commonSchemas.directoryPathForCreation,
        blueprintPath: commonSchemas.blueprintPath,
        assetPath: commonSchemas.assetPath,
        // Ability params
        abilityPath: commonSchemas.assetPath,
        abilityTags: commonSchemas.arrayOfStrings,
        cancelAbilitiesWithTags: commonSchemas.arrayOfStrings,
        blockAbilitiesWithTags: commonSchemas.arrayOfStrings,
        costEffectPath: commonSchemas.assetPath,
        cooldownEffectPath: commonSchemas.assetPath,
        targetingType: { type: 'string', enum: ['self', 'actor', 'location'] },
        targetingRange: commonSchemas.numberProp,
        requiresLineOfSight: commonSchemas.booleanProp,
        targetingAngle: commonSchemas.numberProp,
        taskType: { type: 'string' },
        taskClassName: { type: 'string' },
        policy: { type: 'string' },
        // Effect params
        effectPath: commonSchemas.assetPath,
        durationType: { type: 'string', enum: ['instant', 'infinite', 'has_duration'] },
        duration: commonSchemas.numberProp,
        operation: { type: 'string', enum: ['add', 'multiply', 'divide', 'override'] },
        magnitude: commonSchemas.numberProp,
        attributeName: { type: 'string' },
        modifierIndex: commonSchemas.numberProp,
        magnitudeType: { type: 'string' },
        value: commonSchemas.value,
        calculationClass: commonSchemas.assetPath,
        cueTag: { type: 'string' },
        stackingType: { type: 'string', enum: ['none', 'aggregate_by_source', 'aggregate_by_target'] },
        stackLimit: commonSchemas.numberProp,
        grantedTags: commonSchemas.arrayOfStrings,
        // Common
        tag: { type: 'string' },
        tagName: { type: 'string' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        ...commonSchemas.outputBase,
        assetPath: commonSchemas.assetPath,
        variablesAdded: commonSchemas.arrayOfStrings
      }
    }
  },
  {
    name: 'manage_attribute_sets',
    category: 'gameplay',
    description: 'Create Blueprint AttributeSets and add Ability System Components.',
    annotations: {
      audience: ['developer'],
      priority: 6,
      tags: ['gas', 'attribute-set', 'ability-system-component', 'attributes']
    },
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'add_ability_system_component', 'configure_asc',
            'create_attribute_set', 'add_attribute',
            'set_attribute_base_value', 'set_attribute_clamping'
          ],
          description: 'Action to perform'
        },
        blueprintPath: commonSchemas.blueprintPath,
        name: commonSchemas.name,
        path: commonSchemas.directoryPathForCreation,
        componentName: commonSchemas.componentName,
        replicationMode: { type: 'string', enum: ['full', 'mixed', 'minimal'] },
        attributeSetPath: commonSchemas.assetPath,
        attributeName: { type: 'string' },
        defaultValue: commonSchemas.numberProp,
        baseValue: commonSchemas.numberProp,
        minValue: commonSchemas.numberProp,
        maxValue: commonSchemas.numberProp
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        ...commonSchemas.outputBase,
        assetPath: commonSchemas.assetPath
      }
    }
  },
  {
    name: 'manage_gameplay_cues',
    category: 'gameplay',
    description: 'Create and configure Gameplay Cue Notifies (Static/Actor).',
    annotations: {
      audience: ['developer', 'artist'],
      priority: 5,
      tags: ['gas', 'gameplay-cue', 'vfx', 'audio', 'feedback']
    },
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'create_gameplay_cue_notify', 'configure_cue_trigger', 'set_cue_effects'
          ],
          description: 'Action to perform'
        },
        name: commonSchemas.name,
        path: commonSchemas.directoryPathForCreation,
        cueType: { type: 'string', enum: ['static', 'actor'] },
        cueTag: { type: 'string' },
        cuePath: commonSchemas.assetPath,
        triggerType: { type: 'string', enum: ['on_execute', 'while_active', 'on_remove'] },
        particleSystem: commonSchemas.assetPath,
        sound: commonSchemas.assetPath,
        cameraShake: commonSchemas.assetPath,
        blueprintPath: commonSchemas.blueprintPath
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        ...commonSchemas.outputBase,
        assetPath: commonSchemas.assetPath
      }
    }
  },
  {
    name: 'test_gameplay_abilities',
    category: 'gameplay',
    description: 'Runtime testing of GAS: Activate abilities, apply effects, query attributes.',
    annotations: {
      audience: ['developer'],
      priority: 4,
      tags: ['gas', 'testing', 'debug', 'runtime', 'ability', 'effect']
    },
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'test_activate_ability', 'test_apply_effect',
            'test_get_attribute', 'test_get_gameplay_tags'
          ],
          description: 'Action to perform'
        },
        actorName: commonSchemas.actorName,
        actorLabel: { type: 'string', description: 'Actor label in the level' },
        abilityClass: commonSchemas.assetPath,
        effectClass: commonSchemas.assetPath,
        attributeName: { type: 'string', description: 'Attribute name (e.g. Health)' },
        attributeSetClass: { type: 'string', description: 'AttributeSet class name' },
        payload: commonSchemas.objectProp
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        ...commonSchemas.outputBase,
        value: commonSchemas.value,
        tags: commonSchemas.arrayOfStrings
      }
    }
  }
];
