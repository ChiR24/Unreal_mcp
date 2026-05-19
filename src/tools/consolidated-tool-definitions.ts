/** Schema for a single MCP tool exposed to the client. */
export interface ToolDefinition {
  category?: 'core' | 'world' | 'gameplay' | 'utility';
  name: string;
  description: string;
  inputSchema: Record<string, unknown>;
  outputSchema?: Record<string, unknown>;
  [key: string]: unknown;
}
import { addActionParamsSchema, commonSchemas } from './tool-definition-utils.js';

/** Canonical list of material authoring actions — single source of truth for schema and handler. */
export const MATERIAL_AUTHORING_ACTIONS = [
  'create_material', 'set_blend_mode', 'set_shading_model', 'set_material_domain',
  'add_texture_sample', 'add_texture_coordinate', 'add_scalar_parameter', 'add_vector_parameter',
  'add_static_switch_parameter', 'add_math_node', 'add_world_position', 'add_vertex_normal',
  'add_pixel_depth', 'add_fresnel', 'add_reflection_vector', 'add_panner', 'add_rotator',
  'add_noise', 'add_voronoi', 'add_if', 'add_switch', 'add_custom_expression',
  'connect_nodes', 'connect_material_pins', 'disconnect_nodes', 'break_material_connections',
  'create_material_function', 'add_function_input', 'add_function_output', 'use_material_function',
  'get_material_function_info',
  'create_material_instance', 'set_scalar_parameter_value', 'set_vector_parameter_value', 'set_texture_parameter_value',
  'create_landscape_material', 'create_decal_material', 'create_post_process_material',
  'add_landscape_layer', 'configure_layer_blend',
  'compile_material', 'get_material_info',
  'find_node', 'get_node_connections', 'get_node_properties', 'set_static_switch_parameter_value',
  'delete_node', 'update_custom_expression',
  'get_node_chain', 'get_connected_subgraph',
  'add_material_node', 'rebuild_material', 'set_material_parameter',
  'get_material_node_details', 'remove_material_node',
  'set_two_sided',
] as const;

export const TEXTURE_ACTIONS = [
  'create_noise_texture', 'create_gradient_texture', 'create_pattern_texture',
  'create_normal_from_height', 'create_ao_from_mesh',
  'resize_texture', 'adjust_levels', 'adjust_curves', 'blur', 'sharpen',
  'invert', 'desaturate', 'channel_pack', 'channel_extract', 'combine_textures',
  'set_compression_settings', 'set_texture_group', 'set_lod_bias',
  'configure_virtual_texture', 'set_streaming_priority', 'get_texture_info'
] as const;

export const SKELETON_ACTIONS = [
  'create_skeleton', 'add_bone', 'remove_bone', 'rename_bone',
  'set_bone_transform', 'set_bone_parent', 'create_virtual_bone',
  'create_socket', 'configure_socket', 'auto_skin_weights', 'set_vertex_weights',
  'normalize_weights', 'prune_weights', 'copy_weights', 'mirror_weights',
  'create_physics_asset', 'add_physics_body', 'configure_physics_body',
  'add_physics_constraint', 'configure_constraint_limits',
  'bind_cloth_to_skeletal_mesh', 'assign_cloth_asset_to_mesh',
  'create_morph_target', 'set_morph_target_deltas', 'import_morph_targets',
  'get_skeleton_info', 'list_bones', 'list_sockets', 'list_physics_bodies'
] as const;

export const LIGHTING_ACTIONS = [
  'spawn_light', 'create_light', 'spawn_sky_light', 'create_sky_light', 'ensure_single_sky_light',
  'create_lightmass_volume', 'create_lighting_enabled_level', 'create_dynamic_light',
  'setup_global_illumination', 'configure_shadows', 'set_exposure', 'set_ambient_occlusion',
  'setup_volumetric_fog', 'build_lighting', 'list_light_types'
] as const;

export const SPLINE_ACTIONS = [
  'create_spline_actor', 'add_spline_point', 'remove_spline_point', 'set_spline_point_position',
  'set_spline_point_tangents', 'set_spline_point_rotation', 'set_spline_point_scale', 'set_spline_type',
  'create_spline_mesh_component', 'set_spline_mesh_asset', 'configure_spline_mesh_axis',
  'set_spline_mesh_material', 'scatter_meshes_along_spline', 'configure_mesh_spacing',
  'configure_mesh_randomization', 'create_road_spline', 'create_river_spline', 'create_fence_spline',
  'create_wall_spline', 'create_cable_spline', 'create_pipe_spline', 'get_splines_info'
] as const;

export const PERFORMANCE_ACTIONS = [
  'start_profiling', 'stop_profiling', 'run_benchmark', 'show_stats', 'generate_memory_report',
  'set_scalability', 'set_resolution_scale', 'set_vsync', 'set_frame_rate_limit', 'enable_gpu_timing',
  'configure_texture_streaming', 'configure_lod', 'apply_baseline_settings', 'optimize_draw_calls',
  'merge_actors', 'configure_occlusion_culling', 'optimize_shaders', 'configure_nanite',
  'configure_world_partition'
] as const;

export const BEHAVIOR_TREE_ACTIONS = [
  'create', 'add_node', 'connect_nodes', 'remove_node', 'break_connections', 'set_node_properties'
] as const;

export const NAVIGATION_ACTIONS = [
  'configure_nav_mesh_settings', 'set_nav_agent_properties', 'rebuild_navigation',
  'create_nav_modifier_component', 'set_nav_area_class', 'configure_nav_area_cost',
  'create_nav_link_proxy', 'configure_nav_link', 'set_nav_link_type', 'create_smart_link',
  'configure_smart_link_behavior', 'get_navigation_info'
] as const;

export const WIDGET_AUTHORING_ACTIONS = [
  'create_widget_blueprint', 'set_widget_parent_class', 'add_canvas_panel', 'add_horizontal_box',
  'add_vertical_box', 'add_overlay', 'add_grid_panel', 'add_uniform_grid', 'add_wrap_box',
  'add_scroll_box', 'add_size_box', 'add_scale_box', 'add_border', 'add_text_block',
  'add_rich_text_block', 'add_image', 'add_button', 'add_check_box', 'add_slider', 'add_progress_bar',
  'add_text_input', 'add_combo_box', 'add_spin_box', 'add_list_view', 'add_tree_view',
  'set_anchor', 'set_alignment', 'set_position', 'set_size', 'set_padding', 'set_z_order',
  'set_render_transform', 'set_visibility', 'set_style', 'set_clipping', 'create_property_binding',
  'bind_text', 'bind_visibility', 'bind_color', 'bind_enabled', 'bind_on_clicked', 'bind_on_hovered',
  'bind_on_value_changed', 'create_widget_animation', 'add_animation_track', 'add_animation_keyframe',
  'set_animation_loop', 'create_main_menu', 'create_pause_menu', 'create_settings_menu',
  'create_loading_screen', 'create_hud_widget', 'add_health_bar', 'add_ammo_counter', 'add_minimap',
  'add_crosshair', 'add_compass', 'add_interaction_prompt', 'add_objective_tracker',
  'add_damage_indicator', 'create_inventory_ui', 'create_dialog_widget', 'create_radial_menu',
  'get_widget_info', 'preview_widget'
] as const;

export const SESSION_ACTIONS = [
  'configure_local_session_settings', 'configure_session_interface', 'configure_split_screen',
  'set_split_screen_type', 'add_local_player', 'remove_local_player', 'configure_lan_play',
  'host_lan_server', 'join_lan_server', 'enable_voice_chat', 'configure_voice_settings',
  'set_voice_channel', 'mute_player', 'set_voice_attenuation', 'configure_push_to_talk',
  'get_sessions_info'
] as const;

export const GAME_FRAMEWORK_ACTIONS = [
  'create_game_mode', 'create_game_state', 'create_player_controller',
  'create_player_state', 'create_game_instance', 'create_hud_class',
  'set_default_pawn_class', 'set_player_controller_class',
  'set_game_state_class', 'set_player_state_class', 'configure_game_rules',
  'setup_match_states', 'configure_round_system', 'configure_team_system',
  'configure_scoring_system', 'configure_spawn_system',
  'configure_player_start', 'set_respawn_rules', 'configure_spectating',
  'get_game_framework_info'
] as const;

export const INPUT_ACTIONS = [
  'create_input_action', 'create_input_mapping_context', 'add_mapping', 'remove_mapping',
  'map_input_action', 'set_input_trigger', 'set_input_modifier', 'enable_input_mapping',
  'disable_input_action', 'get_input_info'
] as const;

export const VOLUME_ACTIONS = [
  'create_trigger_volume', 'add_trigger_volume', 'create_trigger_box', 'create_trigger_sphere',
  'create_trigger_capsule', 'create_blocking_volume', 'add_blocking_volume', 'create_kill_z_volume',
  'add_kill_z_volume', 'create_pain_causing_volume', 'create_physics_volume', 'add_physics_volume',
  'create_audio_volume', 'create_reverb_volume', 'create_cull_distance_volume', 'add_cull_distance_volume',
  'create_precomputed_visibility_volume', 'create_lightmass_importance_volume', 'create_nav_mesh_bounds_volume',
  'create_nav_modifier_volume', 'create_camera_blocking_volume', 'create_post_process_volume',
  'add_post_process_volume', 'set_volume_extent', 'set_volume_bounds', 'set_volume_properties',
  'remove_volume', 'get_volumes_info'
] as const;


/** All MCP tool definitions registered with the server, grouped by category. */
export const consolidatedToolDefinitions: ToolDefinition[] = [
  {
    name: 'manage_tools',
    description: 'Dynamic MCP tool management. List canonical tools, view category counts, and enable/disable tools or categories at runtime.',
    category: 'core',
    inputSchema: {
      type: 'object',
      properties: {
        action: { 
          type: 'string', 
          enum: ['list_tools', 'list_categories', 'enable_tools', 'disable_tools', 'enable_category', 'disable_category', 'get_status', 'reset'],
          description: 'list_tools: show canonical tools with status. list_categories: show category counts. enable/disable_tools: toggle specific tools. enable/disable_category: toggle category. get_status: current state. reset: restore defaults.'
        },
        tools: { type: 'array', items: commonSchemas.stringProp, description: 'Tool names to enable/disable' },
        category: { type: 'string', description: 'Category name to enable/disable (core, world, gameplay, utility, all)' }
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
    description: 'Create/import/manage assets, material graphs, material instances, procedural textures, render targets, and dependency analysis.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'list', 'import', 'duplicate', 'duplicate_asset', 'rename', 'rename_asset', 'move', 'move_asset', 'delete', 'delete_asset', 'delete_assets', 'create_folder', 'search_assets',
            'get_dependencies', 'get_source_control_state', 'analyze_graph', 'get_asset_graph', 'create_thumbnail', 'set_tags', 'get_metadata', 'set_metadata', 'validate', 'fixup_redirectors', 'find_by_tag', 'generate_report',
            'create_render_target', 'generate_lods', 'add_material_parameter', 'list_instances', 'reset_instance_parameters', 'exists', 'get_material_stats',
            'nanite_rebuild_mesh', 'bulk_rename', 'bulk_delete', 'source_control_checkout', 'source_control_submit',
            ...MATERIAL_AUTHORING_ACTIONS, ...TEXTURE_ACTIONS],
          description: 'Action to perform'
        },
        assetPath: commonSchemas.assetPath,
        directory: commonSchemas.directoryPath,
        classNames: commonSchemas.arrayOfStrings,
        packagePaths: commonSchemas.arrayOfStrings,
        recursivePaths: commonSchemas.booleanProp,
        recursiveClasses: commonSchemas.booleanProp,
        limit: commonSchemas.numberProp,
        offset: commonSchemas.numberProp,
        sourcePath: commonSchemas.sourcePath,
        destinationPath: commonSchemas.destinationPath,
        assetPaths: commonSchemas.arrayOfStrings,
        lodCount: commonSchemas.numberProp,
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
        width: commonSchemas.numberProp,
        height: commonSchemas.numberProp,
        depth: commonSchemas.numberProp,
        format: commonSchemas.stringProp,
        meshPath: commonSchemas.meshPath,
        tag: commonSchemas.tagName,
        metadata: commonSchemas.objectProp,
        nodeType: commonSchemas.stringProp,
        nodeId: commonSchemas.nodeId,
        sourceNodeId: commonSchemas.sourceNodeId,
        targetNodeId: commonSchemas.targetNodeId,
        inputName: commonSchemas.pinName,
        parameterName: commonSchemas.parameterName,
        value: commonSchemas.value,
        x: commonSchemas.numberProp,
        y: commonSchemas.numberProp,
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
        materialPath: commonSchemas.materialPath,
        texturePath: commonSchemas.texturePath,
        coordinateIndex: commonSchemas.numberProp,
        parameterType: commonSchemas.stringProp,
        tags: commonSchemas.arrayOfStrings,
        // Handler aliases (alternative parameter names accepted by handler)
        folderPath: commonSchemas.directoryPath,
        type: commonSchemas.stringProp,
        defaultValue: commonSchemas.value,
        force: commonSchemas.booleanProp,
        recursive: commonSchemas.booleanProp,
        reportType: commonSchemas.stringProp,
        pattern: commonSchemas.stringProp,
        replacement: commonSchemas.stringProp,
        materialDomain: commonSchemas.stringProp,
        blendMode: commonSchemas.stringProp,
        shadingModel: commonSchemas.stringProp,
        domain: commonSchemas.stringProp,
        twoSided: commonSchemas.booleanProp,
        uTiling: commonSchemas.numberProp,
        vTiling: commonSchemas.numberProp,
        operation: commonSchemas.stringProp,
        constA: commonSchemas.numberProp,
        constB: commonSchemas.numberProp,
        code: commonSchemas.stringProp,
        outputType: commonSchemas.stringProp,
        inputs: commonSchemas.arrayOfObjects,
        additionalOutputs: commonSchemas.arrayOfObjects,
        group: commonSchemas.stringProp,
        sourcePin: commonSchemas.sourcePin,
        targetPin: commonSchemas.targetPin,
        direction: commonSchemas.stringProp,
        downstream: commonSchemas.booleanProp,
        startNodeId: commonSchemas.nodeId,
        endPin: commonSchemas.pinName,
        orphansOnly: commonSchemas.booleanProp,
        functionPath: commonSchemas.assetPath,
        inputType: commonSchemas.stringProp,
        instancePath: commonSchemas.assetPath,
        layerName: commonSchemas.stringProp,
        blendType: commonSchemas.stringProp,
        layers: commonSchemas.arrayOfObjects,
        nodeIds: commonSchemas.arrayOfStrings,
        sourceTexture: commonSchemas.texturePath,
        strength: commonSchemas.numberProp,
        scale: commonSchemas.numberProp,
        octaves: commonSchemas.numberProp,
        samples: commonSchemas.numberProp,
        newWidth: commonSchemas.numberProp,
        newHeight: commonSchemas.numberProp,
        filterMethod: commonSchemas.stringProp,
        inBlack: commonSchemas.numberProp,
        inWhite: commonSchemas.numberProp,
        gamma: commonSchemas.numberProp,
        curvePoints: commonSchemas.arrayOfObjects,
        amount: commonSchemas.numberProp,
        radius: commonSchemas.numberProp,
        channel: commonSchemas.stringProp,
        redTexture: commonSchemas.texturePath,
        greenTexture: commonSchemas.texturePath,
        blueTexture: commonSchemas.texturePath,
        baseTexture: commonSchemas.texturePath,
        blendTexture: commonSchemas.texturePath,
        opacity: commonSchemas.numberProp,
        compressionSettings: commonSchemas.stringProp,
        textureGroup: commonSchemas.stringProp,
        lodBias: commonSchemas.numberProp,
        virtualTextureStreaming: commonSchemas.booleanProp,
        neverStream: commonSchemas.booleanProp,
        streamingPriority: commonSchemas.numberProp,
        speed: commonSchemas.numberProp,
        speedX: commonSchemas.numberProp,
        speedY: commonSchemas.numberProp,
        levels: commonSchemas.numberProp
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
        details: commonSchemas.objectProp,
        totalCount: commonSchemas.numberProp,
        offset: commonSchemas.numberProp,
        limit: commonSchemas.numberProp
      }
    }
  },
  {
    name: 'manage_blueprint',
    category: 'core',
    description: 'Create Blueprints and UMG widgets, add SCS/UI components, set defaults, and manipulate Blueprint graphs, bindings, and widget layouts.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'create', 'create_blueprint', 'get_blueprint', 'get', 'compile',
            'add_component', 'set_default', 'modify_scs', 'get_scs', 'add_scs_component', 'remove_scs_component', 'reparent_scs_component', 'set_scs_transform', 'set_scs_property',
            'ensure_exists', 'probe_handle', 'add_variable', 'remove_variable', 'rename_variable', 'add_function', 'add_event', 'remove_event', 'add_construction_script', 'set_variable_metadata', 'set_metadata',
            'create_node', 'add_node', 'delete_node', 'connect_pins', 'break_pin_links', 'set_node_property', 'create_reroute_node', 'get_node_details', 'get_graph_details', 'get_pin_details',
            'list_node_types', 'set_pin_default_value'
          ,
            ...WIDGET_AUTHORING_ACTIONS],
          description: 'Blueprint action'
        },
        name: commonSchemas.name,
        blueprintPath: commonSchemas.blueprintPath,
        blueprintType: commonSchemas.parentClass,
        savePath: commonSchemas.savePath,
        componentType: commonSchemas.stringProp,
        componentName: commonSchemas.componentName,
        componentClass: commonSchemas.stringProp,
        attachTo: commonSchemas.stringProp,
        newParent: commonSchemas.stringProp,
        propertyName: commonSchemas.propertyName,
        variableName: commonSchemas.variableName,
        oldName: commonSchemas.stringProp,
        newName: commonSchemas.newName,
        value: commonSchemas.value,
        metadata: commonSchemas.objectProp,
        properties: commonSchemas.objectProp,
        graphName: commonSchemas.graphName,
        nodeType: commonSchemas.stringProp,
        nodeId: commonSchemas.nodeId,
        pinName: commonSchemas.pinName,
        linkedTo: commonSchemas.stringProp,
        memberName: commonSchemas.stringProp,
        x: commonSchemas.numberProp,
        y: commonSchemas.numberProp,
        location: commonSchemas.arrayOfNumbers,
        rotation: commonSchemas.arrayOfNumbers,
        scale: commonSchemas.arrayOfNumbers,
        operations: commonSchemas.arrayOfObjects,
        eventType: commonSchemas.stringProp,
        customEventName: commonSchemas.eventName,
        parameters: commonSchemas.arrayOfObjects,
        // Variable configuration (C++ TryGetStringField/BoolField)
        variableType: { type: 'string', description: 'Variable type (e.g., Boolean, Float, Integer, Vector, String, Object)' },
        defaultValue: commonSchemas.value,
        category: commonSchemas.stringProp,
        isReplicated: commonSchemas.booleanProp,
        isPublic: commonSchemas.booleanProp,
        // Function configuration
        functionName: commonSchemas.functionName,
        inputs: commonSchemas.arrayOfObjects,
        outputs: commonSchemas.arrayOfObjects,
        // Node positioning (C++ TryGetNumberField)
        posX: commonSchemas.numberProp,
        posY: commonSchemas.numberProp,
        // Event configuration
        eventName: commonSchemas.eventName,
        // Component/SCS configuration
        parentComponent: commonSchemas.stringProp,
        meshPath: commonSchemas.meshPath,
        materialPath: commonSchemas.materialPath,
        applyAndSave: commonSchemas.booleanProp,
        // Graph operations
        memberClass: commonSchemas.stringProp,
        targetClass: commonSchemas.stringProp,
        inputAxisName: commonSchemas.stringProp,
        // Compilation options
        saveAfterCompile: commonSchemas.booleanProp,
        // Timing/async options
        timeoutMs: commonSchemas.numberProp,
        // Parent class for blueprint creation
        parentClass: commonSchemas.parentClass,
        // Graph pin connections
        fromNodeId: commonSchemas.sourceNodeId,
        fromPinName: commonSchemas.sourcePin,
        toNodeId: commonSchemas.targetNodeId,
        toPinName: commonSchemas.targetPin
      ,
        path: commonSchemas.directoryPathForCreation,
        folder: commonSchemas.directoryPath,
        widgetPath: commonSchemas.widgetPath,
        slotName: commonSchemas.slotName,
        parentSlot: { type: 'string', description: 'Parent slot to add widget to.' },
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
        zOrder: { type: 'number', description: 'Z-order for canvas slot.' },
        translation: {
                  type: 'object',
                  properties: commonSchemas.vector2.properties,
                  description: 'Render translation.'
                },
        shear: {
                  type: 'object',
                  properties: commonSchemas.vector2.properties,
                  description: 'Render shear.'
                },
        angle: commonSchemas.angle,
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
        fontSize: { type: 'number', description: 'Font size.' },
        colorAndOpacity: {
                  type: 'object',
                  properties: commonSchemas.colorObject.properties,
                  description: 'Color and opacity (0-1 values).'
                },
        autoWrap: { type: 'boolean', description: 'Enable text auto-wrap.' },
        texturePath: commonSchemas.texturePath,
        brushSize: {
                  type: 'object',
                  properties: commonSchemas.vector2.properties,
                  description: 'Brush/image size.'
                },
        isEnabled: { type: 'boolean', description: 'Widget enabled state.' },
        isChecked: { type: 'boolean', description: 'Checkbox checked state.' },
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
        isMarquee: { type: 'boolean', description: 'Progress bar marquee mode.' },
        inputType: { type: 'string', enum: ['single', 'multi'], description: 'Text input type.' },
        hintText: { type: 'string', description: 'Placeholder hint text.' },
        options: {
                  type: 'array',
                  items: commonSchemas.stringProp,
                  description: 'Combo box options.'
                },
        selectedOption: { type: 'string', description: 'Selected combo box option.' },
        orientation: {
                  type: 'string',
                  enum: ['Horizontal', 'Vertical'],
                  description: 'Widget orientation.'
                },
        scrollBarVisibility: {
                  type: 'string',
                  enum: ['Visible', 'Collapsed', 'Auto'],
                  description: 'Scroll bar visibility.'
                },
        alwaysShowScrollbar: { type: 'boolean', description: 'Always show scrollbar.' },
        columnCount: { type: 'number', description: 'Number of columns.' },
        rowCount: { type: 'number', description: 'Number of rows.' },
        slotPadding: {
                  type: 'object',
                  properties: { left: commonSchemas.numberProp, top: commonSchemas.numberProp, right: commonSchemas.numberProp, bottom: commonSchemas.numberProp },
                  description: 'Padding between uniform grid slots.'
                },
        minDesiredSlotWidth: { type: 'number', description: 'Minimum slot width.' },
        minDesiredSlotHeight: { type: 'number', description: 'Minimum slot height.' },
        innerSlotPadding: {
                  type: 'object',
                  properties: { left: commonSchemas.numberProp, top: commonSchemas.numberProp, right: commonSchemas.numberProp, bottom: commonSchemas.numberProp },
                  description: 'Inner wrap box slot padding.'
                },
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
        padding: {
                  type: 'object',
                  properties: { left: commonSchemas.numberProp, top: commonSchemas.numberProp, right: commonSchemas.numberProp, bottom: commonSchemas.numberProp },
                  description: 'Widget slot padding.'
                },
        bindingSource: { type: 'string', description: 'Variable or function name to bind to.' },
        onHoveredFunction: { type: 'string', description: 'Function to call on hover.' },
        onUnhoveredFunction: { type: 'string', description: 'Function to call on unhover.' },
        animationName: commonSchemas.animationName,
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
        settingsType: {
                  type: 'string',
                  enum: ['video', 'audio', 'controls', 'gameplay', 'all'],
                  description: 'Settings menu type.'
                },
        includeProgressBar: { type: 'boolean', description: 'Include progress bar.' },
        promptFormat: { type: 'string', description: 'Interaction prompt format.' },
        maxVisibleObjectives: { type: 'number', description: 'Maximum visible objectives.' },
        fadeTime: commonSchemas.fadeTime,
        gridSize: {
                  type: 'object',
                  properties: { columns: commonSchemas.numberProp, rows: commonSchemas.numberProp },
                  description: 'Inventory grid size.'
                },
        showSpeakerName: { type: 'boolean', description: 'Show speaker name.' },
        segmentCount: { type: 'number', description: 'Number of radial segments.' },
        previewSize: {
                  type: 'string',
                  enum: ['1080p', '720p', 'mobile', 'custom'],
                  description: 'Preview resolution preset.'
                },
        duration: commonSchemas.numberProp,
        height: commonSchemas.numberProp,
        nodeGuid: commonSchemas.stringProp,
        nodeName: commonSchemas.nodeName,
        parentName: commonSchemas.stringProp,
        position: commonSchemas.location,
        preset: commonSchemas.stringProp,
        propertyValue: commonSchemas.value,
        size: commonSchemas.numberProp,
        sourceNode: commonSchemas.stringProp,
        sourcePin: commonSchemas.sourcePin,
        targetNode: commonSchemas.stringProp,
        targetPin: commonSchemas.targetPin,
        title: commonSchemas.stringProp,
        width: commonSchemas.numberProp
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        ...commonSchemas.outputBase,
        blueprintPath: commonSchemas.blueprintPath,
        blueprint: { oneOf: [{ type: 'object' }, { type: 'string' }], description: 'Blueprint data object or path string.' }
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
        value: commonSchemas.value
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
        path: commonSchemas.directoryPath,
        levelPath: commonSchemas.levelPath,
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
        key: commonSchemas.stringProp,
        // simulate_input parameters
        type: commonSchemas.stringProp,
        inputType: commonSchemas.stringProp,
        inputAction: commonSchemas.stringProp,
        x: commonSchemas.numberProp,
        y: commonSchemas.numberProp,
        button: commonSchemas.stringProp,
        id: commonSchemas.stringProp
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
    description: 'Load/save levels, configure streaming, and build lighting.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'load', 'load_level', 'save', 'save_level', 'save_as', 'save_level_as', 'stream', 'unload', 'unload_level', 'create_level', 'create_light', 'build_lighting',
            'set_metadata',
            'export_level', 'import_level', 'list_levels', 'get_summary', 'delete', 'delete_level', 'validate_level',
            'add_sublevel', 'rename_level', 'duplicate_level', 'get_current_level'
          ],
          description: 'Action'
        },
        // Level path parameters
        levelPath: commonSchemas.levelPath,
        assetPath: commonSchemas.assetPath,
        levelPaths: commonSchemas.arrayOfStrings,
        levelName: commonSchemas.stringProp,
        path: commonSchemas.directoryPathForCreation,
        // Save/export/import paths
        savePath: commonSchemas.savePath,
        destinationPath: commonSchemas.destinationPath,
        overwrite: commonSchemas.overwrite,
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
        saveDirtyPackages: commonSchemas.booleanProp,
        // Light creation
        lightType: { type: 'string', enum: ['Directional', 'Point', 'Spot', 'Rect', 'DirectionalLight', 'PointLight', 'SpotLight', 'RectLight', 'directional', 'point', 'spot', 'rect'], description: 'Light type. Accepts short names (Point), class names (PointLight), or lowercase (point).' },
        intensity: commonSchemas.numberProp,
        color: commonSchemas.color,
        location: commonSchemas.location,
        rotation: commonSchemas.rotation,
        // Level creation
        template: commonSchemas.stringProp,
        useWorldPartition: commonSchemas.booleanProp,
        // Metadata & utilities
        metadata: commonSchemas.objectProp,
        newName: commonSchemas.stringProp,
        timeoutMs: commonSchemas.numberProp,
        name: commonSchemas.name,
        quality: commonSchemas.stringProp,
        subLevelPath: commonSchemas.levelPath
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
    description: 'Build environments: landscapes, foliage, procedural terrain/biomes, lighting setups, spline roads/rivers/fences, and world decoration.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'create_landscape', 'sculpt', 'sculpt_landscape', 'add_foliage', 'paint_foliage',
            'create_procedural_terrain', 'create_procedural_foliage', 'add_foliage_instances',
            'get_foliage_instances', 'remove_foliage', 'paint_landscape', 'paint_landscape_layer',
            'modify_heightmap', 'set_landscape_material', 'create_landscape_grass_type',
            'generate_lods', 'bake_lightmap', 'export_snapshot', 'import_snapshot', 'delete',
            'create_sky_sphere', 'set_time_of_day', 'create_fog_volume'
          ,
            ...LIGHTING_ACTIONS, ...SPLINE_ACTIONS],
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
        layerName: commonSchemas.stringProp,
        actorName: commonSchemas.actorName,
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
        // Additional handler-used params
        quadsPerSection: commonSchemas.numberProp,
        count: commonSchemas.numberProp,
        assets: commonSchemas.arrayOfStrings,
        numLODs: commonSchemas.numberProp,
        subdivisions: commonSchemas.numberProp,
        tileSize: commonSchemas.numberProp,
        quality: commonSchemas.stringProp,
        staticMesh: commonSchemas.meshPath,
        timeoutMs: commonSchemas.numberProp,
        path: commonSchemas.directoryPath,
        filename: commonSchemas.stringProp,
        assetPaths: commonSchemas.arrayOfStrings,
        // Additional params for C++ handler alignment (EnvironmentHandlers.cpp)
        names: commonSchemas.arrayOfStrings,
        time: commonSchemas.numberProp,
        spacing: commonSchemas.numberProp,
        heightScale: commonSchemas.numberProp,
        material: commonSchemas.materialPath,
        hour: commonSchemas.numberProp,
        intensity: commonSchemas.numberProp,
        alignToSpline: commonSchemas.booleanProp,
        arriveTangent: commonSchemas.location,
        bClosedLoop: commonSchemas.booleanProp,
        blueprintPath: commonSchemas.blueprintPath,
        compensationValue: commonSchemas.numberProp,
        componentName: commonSchemas.componentName,
        enabled: commonSchemas.enabled,
        forwardAxis: commonSchemas.stringProp,
        initialPoints: commonSchemas.arrayOfObjects,
        leaveTangent: commonSchemas.location,
        lightType: commonSchemas.stringProp,
        materialIndex: commonSchemas.numberProp,
        maxBrightness: commonSchemas.numberProp,
        method: commonSchemas.stringProp,
        minBrightness: commonSchemas.numberProp,
        operation: commonSchemas.stringProp,
        pointIndex: commonSchemas.numberProp,
        pointRotation: commonSchemas.rotation,
        pointScale: commonSchemas.scale,
        pointType: commonSchemas.stringProp,
        propertyName: commonSchemas.propertyName,
        propertyValue: commonSchemas.value,
        randomOffsetRange: commonSchemas.numberProp,
        randomizeRotation: commonSchemas.booleanProp,
        randomizeScale: commonSchemas.booleanProp,
        region: commonSchemas.objectProp,
        rotationRange: commonSchemas.rotation,
        save: commonSchemas.save,
        skipFlush: commonSchemas.booleanProp,
        splineType: commonSchemas.stringProp,
        useRandomOffset: commonSchemas.booleanProp,
        width: commonSchemas.numberProp
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
    name: 'animation_physics',
    category: 'gameplay',
    description: 'Author animation and physics assets: Animation Blueprints, blend spaces, montages, Control Rig/IK, skeletons, sockets, physics assets, cloth, ragdolls, and vehicles.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'create_animation_blueprint', 'create_animation_bp', 'create_anim_blueprint',
            'create_blend_space', 'create_blend_space_1d', 'create_blend_space_2d',
            'create_blend_tree', 'create_procedural_anim',
            'create_aim_offset', 'add_aim_offset_sample',
            'create_state_machine', 'add_state_machine', 'add_state', 'add_transition',
            'set_transition_rules', 'add_blend_node', 'add_cached_pose', 'add_slot_node',
            'create_control_rig',
            'create_ik_rig', 'setup_ik',
            'create_pose_library',
            'create_animation_asset', 'create_animation_sequence',
            'set_sequence_length', 'add_bone_track', 'set_bone_key', 'set_curve_key',
            'add_notify_state', 'add_sync_marker', 'set_root_motion_settings', 'set_additive_settings',
            'create_montage', 'add_montage_section', 'add_montage_slot',
            'set_section_timing', 'add_montage_notify', 'set_blend_in', 'set_blend_out',
            'link_sections', 'add_notify', 'play_montage', 'play_anim_montage',
            'setup_ragdoll', 'activate_ragdoll',
            'configure_vehicle', 'setup_physics_simulation',
            'add_blend_sample', 'force_rebuild_blend_space', 'set_axis_settings',
            'set_interpolation_settings', 'setup_retargeting',
            'add_layered_blend_per_bone', 'set_anim_graph_node_value',
            'set_retarget_chain_mapping', 'get_animation_info',
            'cleanup'
          ,
            ...SKELETON_ACTIONS],
          description: 'Action'
        },
        name: commonSchemas.name,
        path: commonSchemas.directoryPathForCreation,
        assetPath: commonSchemas.assetPath,
        animationPath: commonSchemas.assetPath,
        savePath: commonSchemas.savePath,
        skeletonPath: commonSchemas.assetPath,
        parentClass: commonSchemas.stringProp,
        actorName: commonSchemas.actorName,
        targetSkeleton: commonSchemas.assetPath,
        slotName: commonSchemas.stringProp,
        sectionName: commonSchemas.stringProp,
        notifyName: commonSchemas.stringProp,
        boneName: commonSchemas.boneName,
        curveName: commonSchemas.stringProp,
        stateName: commonSchemas.stringProp,
        machineName: commonSchemas.stringProp,
        numFrames: commonSchemas.numberProp,
        frameRate: commonSchemas.numberProp,
        interpolationType: commonSchemas.stringProp,
        axisName: commonSchemas.stringProp,
        playRate: commonSchemas.numberProp,
        frame: commonSchemas.numberProp,
        time: commonSchemas.numberProp,
        length: commonSchemas.numberProp,
        location: commonSchemas.location,
        rotation: commonSchemas.rotation,
        scale: commonSchemas.scale,
        value: commonSchemas.value,
        vehicleType: commonSchemas.stringProp,
        mass: commonSchemas.numberProp,
        dragCoefficient: commonSchemas.numberProp,
        artifacts: commonSchemas.arrayOfStrings,
        sourceSkeleton: commonSchemas.assetPath,
        save: commonSchemas.save,
        additiveAnimType: commonSchemas.stringProp,
        angularDamping: commonSchemas.numberProp,
        assets: commonSchemas.arrayOfStrings,
        assignToMesh: commonSchemas.booleanProp,
        attachBoneName: commonSchemas.boneName,
        axis: commonSchemas.stringProp,
        basePoseFrame: commonSchemas.numberProp,
        basePoseType: commonSchemas.stringProp,
        blendTime: commonSchemas.numberProp,
        blendType: commonSchemas.stringProp,
        blueprintPath: commonSchemas.blueprintPath,
        bodyA: commonSchemas.stringProp,
        bodyB: commonSchemas.stringProp,
        bodyType: commonSchemas.stringProp,
        boneTracks: commonSchemas.arrayOfObjects,
        cacheName: commonSchemas.stringProp,
        center: commonSchemas.location,
        collisionEnabled: commonSchemas.booleanProp,
        compileReferencers: { ...commonSchemas.booleanProp, description: 'force_rebuild_blend_space: after rebuilding the BS, cascade-compile every AnimBlueprint that references it (via IAssetRegistry referencers). Response returns referencersCompiled count + compiledAnimBlueprints path list. Default true.' },
        constraintName: commonSchemas.stringProp,
        deltas: commonSchemas.arrayOfObjects,
        enableRootMotion: commonSchemas.booleanProp,
        endFrame: commonSchemas.numberProp,
        forceRootLock: commonSchemas.booleanProp,
        fromSection: commonSchemas.stringProp,
        fromState: commonSchemas.stringProp,
        layerSetup: commonSchemas.arrayOfObjects,
        limits: commonSchemas.objectProp,
        linearDamping: commonSchemas.numberProp,
        lodIndex: commonSchemas.numberProp,
        markerName: commonSchemas.stringProp,
        maxValue: commonSchemas.numberProp,
        minValue: commonSchemas.numberProp,
        montagePath: commonSchemas.assetPath,
        morphTargetName: commonSchemas.stringProp,
        morphTargetPath: commonSchemas.assetPath,
        newBoneName: commonSchemas.boneName,
        nodeName: commonSchemas.nodeName,
        outputPath: commonSchemas.outputPath,
        overwrite: commonSchemas.overwrite,
        parentBoneName: commonSchemas.boneName,
        physicsAssetName: commonSchemas.stringProp,
        physicsAssetPath: commonSchemas.physicsAssetPath,
        pitch: commonSchemas.numberProp,
        profileName: commonSchemas.stringProp,
        propertyName: commonSchemas.propertyName,
        radius: commonSchemas.numberProp,
        rebuildBlendParameters: { ...commonSchemas.booleanProp, description: 'force_rebuild_blend_space: also trigger PostEditChangeProperty for the BlendParameters UPROPERTY (axis min/max changed). Default false.' },
        relativeLocation: commonSchemas.location,
        relativeRotation: commonSchemas.rotation,
        relativeScale: commonSchemas.scale,
        removeChildren: commonSchemas.booleanProp,
        rootBoneName: commonSchemas.boneName,
        rootMotionRootLock: commonSchemas.stringProp,
        sampleValue: commonSchemas.numberProp,
        simulatePhysics: commonSchemas.booleanProp,
        skeletalMeshPath: commonSchemas.skeletalMeshPath,
        socketName: commonSchemas.socketName,
        sourceBoneName: commonSchemas.boneName,
        sourceChain: commonSchemas.stringProp,
        sourceMeshPath: commonSchemas.meshPath,
        startFrame: commonSchemas.numberProp,
        startTime: commonSchemas.numberProp,
        stateMachineName: commonSchemas.stringProp,
        suffix: commonSchemas.stringProp,
        targetBoneName: commonSchemas.boneName,
        targetChain: commonSchemas.stringProp,
        targetMeshPath: commonSchemas.meshPath,
        threshold: commonSchemas.numberProp,
        toSection: commonSchemas.stringProp,
        toState: commonSchemas.stringProp,
        trackIndex: commonSchemas.numberProp,
        weights: commonSchemas.arrayOfNumbers,
        yaw: commonSchemas.numberProp
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        ...commonSchemas.outputBase,
        assetPath: commonSchemas.assetPath,
        animPath: commonSchemas.assetPath,
        montagePath: commonSchemas.assetPath,
        blendSpacePath: commonSchemas.assetPath,
        // force_rebuild_blend_space response fields:
        rebuiltBlendParameters: { ...commonSchemas.booleanProp, description: 'force_rebuild_blend_space: echoes whether BlendParameters PostEditChangeProperty was also triggered.' },
        referencersCompiled: { ...commonSchemas.numberProp, description: 'force_rebuild_blend_space (compileReferencers=true): count of referencing AnimBlueprints that compiled successfully after the BS rebuild.' },
        compiledAnimBlueprints: { ...commonSchemas.arrayOfStrings, description: 'force_rebuild_blend_space (compileReferencers=true): asset paths of AnimBlueprints whose compile succeeded.' },
        referencersFailed: { ...commonSchemas.numberProp, description: 'force_rebuild_blend_space (compileReferencers=true): count of referencing AnimBlueprints that failed to compile (or to load).' },
        failedAnimBlueprints: { ...commonSchemas.arrayOfStrings, description: 'force_rebuild_blend_space (compileReferencers=true): asset paths of AnimBlueprints that failed to compile or load.' }
      }
    }
  },
  {
    name: 'system_control',
    category: 'core',
    description: 'Control the project runtime: profiling, benchmarks, scalability/LOD/Nanite settings, CVars, console commands, Python scripts, UBT, tests, logs, and widgets.',
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
            'set_project_setting', 'execute_python'
          ,
            ...PERFORMANCE_ACTIONS],
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
        code: { type: 'string', description: 'Python code to execute inline', maxLength: 1048576 }, // 1MB max — prevents resource exhaustion via oversized payloads
        file: { type: 'string', description: 'Path to .py file to execute', maxLength: 4096 } // Max path length on most OS
      ,
        type: { type: 'string', enum: ['CPU', 'GPU', 'Memory', 'RenderThread', 'GameThread', 'All'] },
        duration: commonSchemas.numberProp,
        outputPath: commonSchemas.outputPath,
        detailed: commonSchemas.booleanProp,
        scale: commonSchemas.numberProp,
        maxFPS: commonSchemas.numberProp,
        poolSize: commonSchemas.numberProp,
        boostPlayerLocation: commonSchemas.booleanProp,
        forceLOD: commonSchemas.numberProp,
        lodBias: commonSchemas.numberProp,
        enableInstancing: commonSchemas.booleanProp,
        enableBatching: commonSchemas.booleanProp,
        mergeActors: commonSchemas.booleanProp,
        actors: commonSchemas.arrayOfStrings,
        streamingDistance: commonSchemas.numberProp,
        cellSize: commonSchemas.numberProp,
        categoryName: commonSchemas.stringProp,
        filename: commonSchemas.stringProp,
        height: commonSchemas.numberProp,
        message: commonSchemas.stringProp,
        name: commonSchemas.name,
        packageName: commonSchemas.stringProp,
        paths: commonSchemas.arrayOfStrings,
        replaceSourceActors: commonSchemas.booleanProp,
        savePath: commonSchemas.savePath,
        text: commonSchemas.stringProp,
        volume: commonSchemas.numberProp,
        widgetId: commonSchemas.stringProp,
        width: commonSchemas.numberProp,
        windowed: commonSchemas.booleanProp
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
    name: 'manage_sequence',
    category: 'utility',
    description: 'Edit Level Sequences: add tracks, bind actors, set keyframes, control playback, and record camera.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'create', 'open', 'add_camera', 'add_actor', 'add_actors', 'remove_actors',
            'get_bindings', 'play', 'pause', 'stop', 'set_playback_speed', 'add_keyframe',
            'get_properties', 'set_properties', 'duplicate', 'rename', 'delete', 'list', 'get_metadata', 'set_metadata',
            'add_spawnable_from_class', 'add_track', 'add_section', 'set_display_rate', 'set_tick_resolution',
            'set_work_range', 'set_view_range', 'set_track_muted', 'set_track_solo', 'set_track_locked',
            'list_tracks', 'remove_track', 'list_track_types'
          ],
          description: 'Action'
        },
        name: commonSchemas.name,
        path: commonSchemas.assetPath,
        actorName: commonSchemas.actorName,
        actorNames: commonSchemas.arrayOfStrings,
        frame: commonSchemas.numberProp,
        value: commonSchemas.objectProp,
        property: commonSchemas.propertyName,
        destinationPath: commonSchemas.destinationPath,
        newName: commonSchemas.newName,
        speed: commonSchemas.numberProp,
        startTime: commonSchemas.numberProp,
        loopMode: commonSchemas.stringProp,
        className: commonSchemas.stringProp,
        spawnable: commonSchemas.booleanProp,
        trackType: commonSchemas.stringProp,
        trackName: commonSchemas.stringProp,
        muted: commonSchemas.booleanProp,
        solo: commonSchemas.booleanProp,
        locked: commonSchemas.booleanProp,
        startFrame: commonSchemas.numberProp,
        endFrame: commonSchemas.numberProp,
        frameRate: commonSchemas.stringProp,
        resolution: commonSchemas.stringProp,
        start: commonSchemas.numberProp,
        end: commonSchemas.numberProp,
        lengthInFrames: commonSchemas.numberProp,
        playbackStart: commonSchemas.numberProp,
        playbackEnd: commonSchemas.numberProp,
        metadata: commonSchemas.objectProp
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
        properties: commonSchemas.objectProp,
        actors: commonSchemas.arrayOfObjects,
        playerController: commonSchemas.objectProp,
        pawn: commonSchemas.objectProp,
        viewTarget: commonSchemas.objectProp,
        playerCameraManager: commonSchemas.objectProp,
        worldType: commonSchemas.stringProp
      }
    }
  },
  {
    name: 'manage_audio',
    category: 'utility',
    description: 'Play/stop sounds, add audio components, configure mixes, attenuation, spatial audio, and author Sound Cues/MetaSounds.',
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
        // Sound Cue authoring
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
            'get_audio_info'
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
        analysisType: commonSchemas.stringProp,
        windowSize: commonSchemas.numberProp,
        outputType: commonSchemas.stringProp,
        soundName: commonSchemas.stringProp,
        fadeType: commonSchemas.stringProp,
        lowPassFilterFrequency: commonSchemas.numberProp,
        enabled: commonSchemas.enabled,
        // Authoring properties
        path: commonSchemas.directoryPathForCreation,
        assetPath: commonSchemas.assetPath,
        save: commonSchemas.save,
        wavePath: commonSchemas.wavePath,
        nodeType: commonSchemas.stringProp,
        sourceNodeId: commonSchemas.sourceNodeId,
        targetNodeId: commonSchemas.targetNodeId,
        looping: commonSchemas.looping,
        inputName: commonSchemas.inputName,
        inputType: commonSchemas.stringProp,
        outputName: commonSchemas.outputName,
        sourceNode: commonSchemas.sourceNode,
        sourcePin: commonSchemas.sourcePin,
        targetNode: commonSchemas.targetNode,
        targetPin: commonSchemas.targetPin,
        defaultValue: commonSchemas.value,
        soundClassPath: commonSchemas.soundClassPath,
        dopplerIntensity: commonSchemas.numberProp,
        effectType: commonSchemas.stringProp,
        enable: commonSchemas.booleanProp,
        enableReverbSend: commonSchemas.booleanProp,
        occlusionFilterScale: commonSchemas.numberProp,
        occlusionInterpolationTime: commonSchemas.numberProp,
        occlusionVolumeScale: commonSchemas.numberProp,
        reverbDistanceMax: commonSchemas.numberProp,
        reverbDistanceMin: commonSchemas.numberProp,
        reverbWetLevelMax: commonSchemas.numberProp,
        reverbWetLevelMin: commonSchemas.numberProp,
        sourceOutputName: commonSchemas.stringProp,
        spatialization: commonSchemas.stringProp,
        speakerPath: commonSchemas.assetPath,
        targetInputName: commonSchemas.stringProp,
        velocityScale: commonSchemas.numberProp,
        volumeAdjuster: commonSchemas.numberProp
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
        // Blueprint graph actions
  {
    name: 'manage_geometry',
    category: 'world',
    description: 'Create procedural meshes using Geometry Script: booleans, deformers, UVs, collision, and LOD generation.',
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
            'stretch', 'spherify', 'cylindrify', 'lattice_deform', 'displace_by_texture',
            'triangulate', 'poke',
            'mirror', 'array_linear', 'array_radial',
            'simplify_mesh', 'subdivide', 'remesh_uniform', 'merge_vertices', 'remesh_voxel',
            'weld_vertices', 'fill_holes', 'remove_degenerates',
            'auto_uv', 'project_uv', 'transform_uvs', 'unwrap_uv', 'pack_uv_islands',
            'recalculate_normals', 'flip_normals', 'recompute_tangents',
            'generate_collision', 'generate_complex_collision', 'simplify_collision',
            'generate_lods', 'set_lod_settings', 'set_lod_screen_sizes', 'convert_to_nanite',
            'convert_to_static_mesh',
            'get_mesh_info'
          ],
          description: 'Geometry action to perform'
        },
        outputPath: commonSchemas.outputPath,
        assetPath: commonSchemas.assetPath,
        name: commonSchemas.name,
        path: commonSchemas.directoryPath,
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
        weight: { type: 'number', description: 'Weight for lattice deformation.' },
        latticeResolution: { type: 'number', description: 'Control lattice resolution for lattice deformation.' },
        heightScale: { type: 'number', description: 'Texture displacement height scale.' },
        midpoint: { type: 'number', description: 'Texture luminance midpoint for displacement.' },
        texturePath: commonSchemas.texturePath,
        position: commonSchemas.location,
        center: commonSchemas.location,
        offset: commonSchemas.location,
        dimensions: commonSchemas.dimensions,
        targetActor: commonSchemas.actorName,
        toolActor: commonSchemas.actorName,
        trimActorName: commonSchemas.actorName,
        splineActorName: commonSchemas.actorName,
        keepTool: commonSchemas.booleanProp,
        keepInside: commonSchemas.booleanProp,
        cap: commonSchemas.booleanProp,
        count: commonSchemas.numberProp,
        steps: commonSchemas.numberProp,
        iterations: { type: 'number', description: 'Number of iterations for smooth, remesh.' },
        targetTriangleCount: { type: 'number', description: 'Target triangle count for simplification.' },
        targetEdgeLength: { type: 'number', description: 'Target edge length for remeshing.' },
        weldDistance: { type: 'number', description: 'Distance threshold for vertex welding.' },
        uvChannel: { type: 'number', description: 'UV channel index (0-7).' },
        uvScale: { type: 'object', properties: { u: commonSchemas.numberProp, v: commonSchemas.numberProp }, description: 'UV scale.' },
        uvOffset: { type: 'object', properties: { u: commonSchemas.numberProp, v: commonSchemas.numberProp }, description: 'UV offset.' },
        hardEdgeAngle: { type: 'number', description: 'Angle threshold for hard edges (degrees).' },
        computeWeightedNormals: { type: 'boolean', description: 'Use area-weighted normals.' },
        collisionType: { type: 'string', enum: ['Default', 'Simple', 'Complex', 'UseComplexAsSimple', 'UseSimpleAsComplex'], description: 'Collision complexity type.' },
        hullCount: { type: 'number', description: 'Number of convex hulls for decomposition.' },
        maxHullCount: { type: 'number', description: 'Maximum hull count for complex collision generation.' },
        maxHullVerts: { type: 'number', description: 'Maximum vertices per hull for complex collision generation.' },
        targetHullCount: { type: 'number', description: 'Target hull count for collision simplification.' },
        simplificationFactor: { type: 'number', description: 'Collision simplification factor.' },
        hullPrecision: { type: 'number', description: 'Precision for convex hull generation (0-1).' },
        maxVerticesPerHull: { type: 'number', description: 'Maximum vertices per convex hull.' },
        lodCount: { type: 'number', description: 'Number of LOD levels to generate.' },
        lodIndex: { type: 'number', description: 'Specific LOD index to configure.' },
        reductionPercent: { type: 'number', description: 'Percent of triangles to reduce per LOD.' },
        trianglePercent: { type: 'number', description: 'Percent of triangles to keep for LOD reduction.' },
        recomputeNormals: commonSchemas.booleanProp,
        recomputeTangents: commonSchemas.booleanProp,
        screenSizes: { type: 'array', items: commonSchemas.numberProp, description: 'Array of screen sizes for each LOD.' }
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
            // Authoring status actions
  {
    name: 'manage_effect',
    category: 'gameplay',
    description: 'Niagara particle systems, VFX, debug shapes, and GPU simulations. Create systems, emitters, modules, and control particle effects.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'particle', 'niagara', 'debug_shape', 'spawn_niagara', 'create_dynamic_light',
            'create_niagara_system', 'create_niagara_emitter', 'create_volumetric_fog',
            'create_particle_trail', 'create_environment_effect', 'create_impact_effect',
            'create_niagara_ribbon', 'activate', 'activate_effect', 'deactivate', 'reset',
            'advance_simulation', 'add_niagara_module', 'connect_niagara_pins',
            'remove_niagara_node', 'set_niagara_parameter', 'clear_debug_shapes', 'cleanup',
            'list_debug_shapes', 'add_emitter_to_system', 'set_emitter_properties',
            'add_spawn_rate_module', 'add_spawn_burst_module', 'add_spawn_per_unit_module',
            'add_initialize_particle_module', 'add_particle_state_module', 'add_force_module',
            'add_velocity_module', 'add_acceleration_module', 'add_size_module', 'add_color_module',
            'add_sprite_renderer_module', 'add_mesh_renderer_module', 'add_ribbon_renderer_module',
            'add_light_renderer_module', 'add_collision_module', 'add_kill_particles_module',
            'add_camera_offset_module', 'add_user_parameter', 'set_parameter_value',
            'bind_parameter_to_source', 'add_skeletal_mesh_data_interface',
            'add_static_mesh_data_interface', 'add_spline_data_interface', 'add_audio_spectrum_data_interface',
            'add_collision_query_data_interface', 'add_event_generator', 'add_event_receiver',
            'configure_event_payload', 'enable_gpu_simulation', 'add_simulation_stage',
            'get_niagara_info', 'validate_niagara_system'
          ],
          description: 'Effect/Niagara action to perform.'
        },
        // Common parameters
        name: commonSchemas.name,
        path: commonSchemas.directoryPathForCreation,
        savePath: commonSchemas.savePath,
        assetPath: commonSchemas.assetPath,
        timeoutMs: commonSchemas.numberProp,
        save: commonSchemas.booleanProp,
        // System/Emitter parameters
        system: commonSchemas.assetPath,
        systemPath: commonSchemas.assetPath,
        systemName: commonSchemas.stringProp,
        emitter: commonSchemas.stringProp,
        emitterName: commonSchemas.stringProp,
        emitterPath: commonSchemas.assetPath,
        emitterProperties: commonSchemas.objectProp,
        // Placement and lifecycle
        location: commonSchemas.location,
        actorName: commonSchemas.actorName,
        attachToActor: commonSchemas.actorName,
        reset: commonSchemas.booleanProp,
        filter: commonSchemas.stringProp,
        deltaTime: commonSchemas.numberProp,
        steps: commonSchemas.integerProp,
        // Debug/particle/fog/light parameters
        preset: commonSchemas.stringProp,
        shape: commonSchemas.stringProp,
        shapeType: commonSchemas.stringProp,
        radius: commonSchemas.numberProp,
        color: {
          oneOf: [
            { type: 'array', items: commonSchemas.numberProp },
            {
              type: 'object',
              properties: {
                r: commonSchemas.numberProp,
                g: commonSchemas.numberProp,
                b: commonSchemas.numberProp,
                a: commonSchemas.numberProp
              }
            }
          ],
          description: 'RGBA color as an array [r,g,b,a] or object {r,g,b,a} depending on action.'
        },
        duration: commonSchemas.numberProp,
        lightType: commonSchemas.stringProp,
        intensity: commonSchemas.numberProp,
        density: commonSchemas.numberProp,
        scattering: commonSchemas.numberProp,
        extinction: commonSchemas.numberProp,
        // Niagara graph operations
        modulePath: commonSchemas.assetPath,
        scriptType: commonSchemas.stringProp,
        autoConnect: commonSchemas.booleanProp,
        nodeId: commonSchemas.nodeId,
        // Niagara parameters
        parameterName: commonSchemas.parameterName,
        parameterType: commonSchemas.stringProp,
        parameterValue: commonSchemas.value,
        value: commonSchemas.value,
        sourceBinding: commonSchemas.stringProp,
        // Emitter/module authoring
        spawnRate: commonSchemas.numberProp,
        burstCount: commonSchemas.numberProp,
        burstTime: commonSchemas.numberProp,
        spawnPerUnit: commonSchemas.numberProp,
        lifetime: commonSchemas.numberProp,
        mass: commonSchemas.numberProp,
        forceType: commonSchemas.stringProp,
        forceStrength: commonSchemas.numberProp,
        forceVector: commonSchemas.location,
        velocity: commonSchemas.location,
        velocityMode: commonSchemas.stringProp,
        acceleration: commonSchemas.location,
        sizeMode: commonSchemas.stringProp,
        uniformSize: commonSchemas.numberProp,
        colorMode: commonSchemas.stringProp,
        materialPath: commonSchemas.materialPath,
        alignment: commonSchemas.stringProp,
        facingMode: commonSchemas.stringProp,
        meshPath: commonSchemas.meshPath,
        lightRadius: commonSchemas.numberProp,
        collisionMode: commonSchemas.stringProp,
        restitution: commonSchemas.numberProp,
        friction: commonSchemas.numberProp,
        dieOnCollision: commonSchemas.booleanProp,
        killCondition: commonSchemas.stringProp,
        cameraOffset: commonSchemas.numberProp,
        // Events, GPU, and simulation stages
        eventName: commonSchemas.eventName,
        eventType: commonSchemas.stringProp,
        spawnOnEvent: commonSchemas.booleanProp,
        eventSpawnCount: commonSchemas.numberProp,
        eventPayload: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              name: commonSchemas.name,
              type: commonSchemas.stringProp
            }
          },
          description: 'Niagara event payload attributes to expose.'
        },
        fixedBoundsEnabled: commonSchemas.booleanProp,
        deterministicEnabled: commonSchemas.booleanProp,
        stageName: commonSchemas.stringProp,
        stageIterationSource: commonSchemas.stringProp
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        ...commonSchemas.outputBase,
        systemPath: commonSchemas.assetPath,
        emitterName: commonSchemas.stringProp,
        shapes: commonSchemas.arrayOfObjects,
        niagaraInfo: commonSchemas.objectProp,
        validationResult: commonSchemas.objectProp
      }
    }
  },
  {
    name: 'manage_gas',
    category: 'gameplay',
    description: 'Create Gameplay Abilities, Effects, Attribute Sets, and Gameplay Cues for ability systems.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'add_ability_system_component',
            'configure_asc',
            'create_attribute_set',
            'add_attribute',
            'set_attribute_base_value',
            'set_attribute_clamping',
            'create_gameplay_ability',
            'set_ability_tags',
            'set_ability_costs',
            'set_ability_cooldown',
            'set_ability_targeting',
            'add_ability_task',
            'set_activation_policy',
            'set_instancing_policy',
            'create_gameplay_effect',
            'set_effect_duration',
            'add_effect_modifier',
            'set_modifier_magnitude',
            'add_effect_execution_calculation',
            'add_effect_cue',
            'set_effect_stacking',
            'set_effect_tags',
            'create_gameplay_cue_notify',
            'configure_cue_trigger',
            'set_cue_effects',
            'add_tag_to_asset',
            'get_gas_info'
          ],
          description: 'GAS action to perform.'
        },
        name: commonSchemas.assetNameForCreation,
        path: commonSchemas.directoryPathForCreation,
        assetPath: commonSchemas.assetPath,
        blueprintPath: commonSchemas.blueprintPath,
        replicationMode: {
          type: 'string',
          enum: ['Full', 'Minimal', 'Mixed'],
          description: 'ASC replication mode.'
        },
        attributeSetPath: { type: 'string', description: 'Path to Attribute Set asset.' },
        attributeName: commonSchemas.attributeName,
        attributeType: {
          type: 'string',
          enum: ['Health', 'MaxHealth', 'Mana', 'MaxMana', 'Stamina', 'MaxStamina', 'Damage', 'Armor', 'AttackPower', 'MoveSpeed', 'Custom'],
          description: 'Predefined attribute type or Custom.'
        },
        baseValue: { type: 'number', description: 'Base value for attribute.' },
        minValue: { type: 'number', description: 'Minimum value for clamping.' },
        maxValue: { type: 'number', description: 'Maximum value for clamping.' },
        clampMode: {
          type: 'string',
          enum: ['None', 'Min', 'Max', 'MinMax'],
          description: 'Attribute clamping mode.'
        },
        abilityPath: commonSchemas.abilityPath,
        abilityTags: {
          type: 'array',
          items: commonSchemas.stringProp,
          description: 'Gameplay tags for this ability.'
        },
        cancelAbilitiesWithTag: {
          type: 'array',
          items: commonSchemas.stringProp,
          description: 'Tags of abilities to cancel when this activates.'
        },
        blockAbilitiesWithTag: {
          type: 'array',
          items: commonSchemas.stringProp,
          description: 'Tags of abilities blocked while this is active.'
        },
        activationRequiredTags: {
          type: 'array',
          items: commonSchemas.stringProp,
          description: 'Tags required to activate this ability.'
        },
        activationBlockedTags: {
          type: 'array',
          items: commonSchemas.stringProp,
          description: 'Tags that block activation of this ability.'
        },
        costEffectPath: { type: 'string', description: 'Path to cost Gameplay Effect.' },
        cooldownEffectPath: { type: 'string', description: 'Path to cooldown Gameplay Effect.' },
        targetingMode: {
          type: 'string',
          enum: ['None', 'SingleTarget', 'AOE', 'Directional', 'Ground', 'ActorPlacement'],
          description: 'Targeting mode for ability.'
        },
        targetRange: { type: 'number', description: 'Maximum targeting range.' },
        aoeRadius: { type: 'number', description: 'Area of effect radius.' },
        taskType: {
          type: 'string',
          enum: ['WaitDelay', 'WaitInputPress', 'WaitInputRelease', 'WaitGameplayEvent', 'WaitTargetData', 'WaitConfirmCancel', 'PlayMontageAndWait', 'ApplyRootMotionConstantForce', 'WaitMovementModeChange'],
          description: 'Type of ability task to add.'
        },
        activationPolicy: {
          type: 'string',
          enum: ['OnInputPressed', 'WhileInputActive', 'OnSpawn', 'OnGiven'],
          description: 'When the ability activates.'
        },
        instancingPolicy: {
          type: 'string',
          enum: ['NonInstanced', 'InstancedPerActor', 'InstancedPerExecution'],
          description: 'How the ability is instanced.'
        },
        effectPath: commonSchemas.effectPath,
        durationType: {
          type: 'string',
          enum: ['Instant', 'Infinite', 'HasDuration'],
          description: 'Effect duration type.'
        },
        duration: commonSchemas.duration,
        period: { type: 'number', description: 'Period for periodic effects.' },
        modifierOperation: {
          type: 'string',
          enum: ['Add', 'Multiply', 'Divide', 'Override'],
          description: 'Modifier operation on attribute.'
        },
        modifierMagnitude: { type: 'number', description: 'Magnitude of the modifier.' },
        magnitudeCalculationType: {
          type: 'string',
          enum: ['ScalableFloat', 'AttributeBased', 'SetByCaller', 'CustomCalculationClass'],
          description: 'How magnitude is calculated.'
        },
        setByCallerTag: { type: 'string', description: 'Tag for SetByCaller magnitude.' },
        targetAttribute: { type: 'string', description: 'Target attribute for modifier.' },
        calculationClass: { type: 'string', description: 'UGameplayEffectExecutionCalculation class path.' },
        cueTag: { type: 'string', description: 'Gameplay Cue tag (e.g., GameplayCue.Damage.Fire).' },
        cuePath: { type: 'string', description: 'Path to Gameplay Cue asset.' },
        stackingType: {
          type: 'string',
          enum: ['None', 'AggregateBySource', 'AggregateByTarget'],
          description: 'Stacking type for effect.'
        },
        stackLimitCount: { type: 'number', description: 'Maximum stack count.' },
        stackDurationRefreshPolicy: {
          type: 'string',
          enum: ['RefreshOnSuccessfulApplication', 'NeverRefresh'],
          description: 'When to refresh stack duration.'
        },
        stackPeriodResetPolicy: {
          type: 'string',
          enum: ['ResetOnSuccessfulApplication', 'NeverReset'],
          description: 'When to reset stack period.'
        },
        stackExpirationPolicy: {
          type: 'string',
          enum: ['ClearEntireStack', 'RemoveSingleStackAndRefreshDuration', 'RefreshDuration'],
          description: 'What happens when stack expires.'
        },
        grantedTags: {
          type: 'array',
          items: commonSchemas.stringProp,
          description: 'Tags granted while effect is active.'
        },
        applicationRequiredTags: {
          type: 'array',
          items: commonSchemas.stringProp,
          description: 'Tags required to apply this effect.'
        },
        removalTags: {
          type: 'array',
          items: commonSchemas.stringProp,
          description: 'Tags that cause effect removal.'
        },
        immunityTags: {
          type: 'array',
          items: commonSchemas.stringProp,
          description: 'Tags that block this effect.'
        },
        cueType: {
          type: 'string',
          enum: ['Static', 'Actor'],
          description: 'Type of gameplay cue notify.'
        },
        triggerType: {
          type: 'string',
          enum: ['OnActive', 'WhileActive', 'Executed', 'OnRemove'],
          description: 'When the cue triggers.'
        },
        particleSystemPath: commonSchemas.particleSystemPath,
        soundPath: commonSchemas.soundPath,
        cameraShakePath: commonSchemas.cameraShakePath,
        decalPath: commonSchemas.decalPath,
        tagName: commonSchemas.tagName,
        componentName: commonSchemas.componentName,
        defaultValue: commonSchemas.value,
        modifierIndex: commonSchemas.numberProp
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        ...commonSchemas.outputBase,
        assetPath: commonSchemas.assetPath,
        componentName: commonSchemas.componentName,
        attributeName: commonSchemas.stringProp,
        modifierIndex: commonSchemas.numberProp,
        gasInfo: {
          type: 'object',
          properties: {
            assetType: { type: 'string', enum: ['AttributeSet', 'GameplayAbility', 'GameplayEffect', 'GameplayCue'] },
            attributes: commonSchemas.arrayOfObjects,
            abilityTags: commonSchemas.arrayOfStrings,
            effectDuration: commonSchemas.stringProp,
            modifierCount: commonSchemas.numberProp,
            cueType: commonSchemas.stringProp
          }
        },
        error: commonSchemas.stringProp
      }
    }
  },
  {
    name: 'manage_character',
    category: 'gameplay',
    description: 'Create Character Blueprints with movement, locomotion, and animation state machines.',
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
            'setup_movement', 'set_walk_speed', 'set_jump_height', 'set_gravity_scale',
            'set_ground_friction', 'set_braking_deceleration', 'configure_crouch', 'configure_sprint'
          ],
          description: 'Character action to perform.'
        },
        name: commonSchemas.assetNameForCreation,
        path: commonSchemas.directoryPathForCreation,
        blueprintPath: commonSchemas.blueprintPath,
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
        mantleHeight: { type: 'number', description: 'Maximum mantle height.' },
        mantleReachDistance: { type: 'number', description: 'Forward reach for mantle check.' },
        vaultHeight: { type: 'number', description: 'Maximum vault obstacle height.' },
        vaultDepth: { type: 'number', description: 'Obstacle depth to check.' },
        climbSpeed: commonSchemas.numberProp,
        climbableTag: { type: 'string', description: 'Tag for climbable surfaces.' },
        slideSpeed: commonSchemas.numberProp,
        slideDuration: commonSchemas.numberProp,
        slideCooldown: commonSchemas.numberProp,
        wallRunSpeed: { type: 'number', description: 'Wall running speed.' },
        wallRunDuration: { type: 'number', description: 'Maximum wall run duration.' },
        wallRunGravityScale: { type: 'number', description: 'Gravity during wall run.' },
        grappleRange: { type: 'number', description: 'Maximum grapple distance.' },
        grappleSpeed: { type: 'number', description: 'Grapple pull speed.' },
        grappleTargetTag: { type: 'string', description: 'Tag for grapple targets.' },
        footstepEnabled: { type: 'boolean', description: 'Enable footstep system.' },
        footstepSocketLeft: { type: 'string', description: 'Left foot socket name.' },
        footstepSocketRight: { type: 'string', description: 'Right foot socket name.' },
        footstepTraceDistance: { type: 'number', description: 'Ground trace distance.' },
        surfaceType: {
          type: 'string',
          enum: ['Default', 'Concrete', 'Grass', 'Dirt', 'Metal', 'Wood', 'Water', 'Snow', 'Sand', 'Gravel', 'Custom'],
          description: 'Physical surface type.'
        },
        brakingDeceleration: commonSchemas.numberProp,
        canCrouch: commonSchemas.booleanProp,
        crouchedHalfHeight: commonSchemas.numberProp,
        customSpeed: commonSchemas.numberProp,
        particleScale: commonSchemas.numberProp,
        volumeMultiplier: commonSchemas.numberProp
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
            springArmTemplates: { ...commonSchemas.arrayOfObjects, description: 'Spring arm component templates on the Character Blueprint.' },
            cameraTemplates: { ...commonSchemas.arrayOfObjects, description: 'Camera component templates on the Character Blueprint.' },
            bFindCameraComponentWhenViewTarget: commonSchemas.booleanProp,
            playerViewState: { ...commonSchemas.objectProp, description: 'Active PIE player controller, pawn, view target, and PlayerCameraManager state.' },
            movementVariables: commonSchemas.arrayOfStrings,
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
    description: 'Create weapons with hitscan/projectile firing, configure damage types, hitboxes, reload, and melee combat (combos, parry, block).',
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
            'setup_damage_type', 'configure_hit_detection', 'get_combat_stats',
            'create_damage_effect', 'apply_damage', 'heal', 'create_shield', 'modify_armor'
          ],
          description: 'Combat action to perform'
        },
        blueprintPath: commonSchemas.blueprintPath,
        name: commonSchemas.name,
        path: commonSchemas.directoryPathForCreation,
        weaponMeshPath: { type: 'string', description: 'Path to weapon static/skeletal mesh.' },
        muzzleSocketName: commonSchemas.muzzleSocketName,
        ejectionSocketName: commonSchemas.ejectionSocketName,
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
        equipAnimationPath: { type: 'string', description: 'Path to equip animation montage.' },
        unequipAnimationPath: { type: 'string', description: 'Path to unequip animation montage.' },
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
        comboWindowTime: { type: 'number', description: 'Time window for combo input.' },
        maxComboCount: { type: 'number', description: 'Maximum combo length.' },
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
        weaponTrailEndSocket: { type: 'string', description: 'Trail end socket.' },
        ammoPerShot: commonSchemas.numberProp,
        armorValue: commonSchemas.numberProp,
        damageAmount: commonSchemas.numberProp,
        damagePerSecond: commonSchemas.numberProp,
        damageReduction: commonSchemas.numberProp,
        damageType: commonSchemas.stringProp,
        duration: commonSchemas.numberProp,
        effectType: commonSchemas.stringProp,
        healAmount: commonSchemas.numberProp,
        infiniteAmmo: commonSchemas.booleanProp,
        maxHealth: commonSchemas.numberProp,
        maxShield: commonSchemas.numberProp,
        shieldAmount: commonSchemas.numberProp,
        shieldRegenDelay: commonSchemas.numberProp,
        shieldRegenRate: commonSchemas.numberProp
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
    description: 'Build AI systems: AI controllers, Behavior Trees, Blackboards, EQS, perception, State Trees, Smart Objects, NavMesh settings, nav modifiers, links, and pathfinding.',
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
            'create_mass_entity_config', 'configure_mass_entity', 'add_mass_spawner',
            'get_ai_info',
            'create_blackboard', 'setup_perception',
            'set_focus', 'clear_focus',
            'set_blackboard_value', 'get_blackboard_value',
            'run_behavior_tree', 'stop_behavior_tree'
          ,
            ...BEHAVIOR_TREE_ACTIONS, ...NAVIGATION_ACTIONS],
          description: 'AI action to perform'
        },
        name: commonSchemas.name,
        path: commonSchemas.directoryPathForCreation,
        blueprintPath: commonSchemas.blueprintPath,
        controllerPath: commonSchemas.controllerPath,
        behaviorTreePath: commonSchemas.behaviorTreePath,
        blackboardPath: commonSchemas.blackboardPath,
        keyName: commonSchemas.keyName,
        keyType: {
          type: 'string',
          enum: ['Bool', 'Int', 'Float', 'Vector', 'Rotator', 'Object', 'Class', 'Enum', 'Name', 'String'],
          description: 'Blackboard key data type.'
        },
        isInstanceSynced: { type: 'boolean', description: 'Sync key across instances.' },
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
        definitionPath: commonSchemas.definitionPath,
        slotIndex: { type: 'number', description: 'Index of slot to configure.' },
        configPath: commonSchemas.configPath,
        actorName: commonSchemas.actorName,
        agentRadius: { type: 'number', description: 'Navigation agent radius (default: 35).' },
        agentHeight: { type: 'number', description: 'Navigation agent height (default: 144).' },
        agentStepHeight: { type: 'number', description: 'Maximum step height agent can climb (default: 35).' },
        agentMaxSlope: { type: 'number', description: 'Maximum slope angle in degrees (default: 44).' },
        cellSize: { type: 'number', description: 'NavMesh cell size (default: 19).' },
        cellHeight: { type: 'number', description: 'NavMesh cell height (default: 10).' },
        tileSizeUU: { type: 'number', description: 'NavMesh tile size in UU (default: 1000).' },
        minRegionArea: { type: 'number', description: 'Minimum region area to keep.' },
        mergeRegionSize: { type: 'number', description: 'Region merge threshold.' },
        maxSimplificationError: { type: 'number', description: 'Edge simplification error.' },
        areaClass: commonSchemas.areaClass,
        failsafeExtent: {
          type: 'object',
          properties: commonSchemas.vector3.properties,
          description: 'Failsafe extent for nav modifier when actor has no collision.'
        },
        areaCost: { type: 'number', description: 'Pathfinding cost multiplier for area (1.0 = normal).' },
        startPoint: {
          type: 'object',
          properties: commonSchemas.vector3.properties,
          description: 'Start point of navigation link (relative to actor).'
        },
        endPoint: {
          type: 'object',
          properties: commonSchemas.vector3.properties,
          description: 'End point of navigation link (relative to actor).'
        },
        direction: {
          type: 'string',
          enum: ['BothWays', 'LeftToRight', 'RightToLeft'],
          description: 'Link traversal direction.'
        },
        snapRadius: { type: 'number', description: 'Snap radius for link endpoints (default: 30).' },
        linkEnabled: { type: 'boolean', description: 'Whether the link is enabled.' },
        linkType: {
          type: 'string',
          enum: ['simple', 'smart'],
          description: 'Type of navigation link.'
        },
        enabledAreaClass: { type: 'string', description: 'Area class when smart link is enabled.' },
        disabledAreaClass: { type: 'string', description: 'Area class when smart link is disabled.' },
        broadcastRadius: { type: 'number', description: 'Radius for state change broadcast.' },
        broadcastInterval: { type: 'number', description: 'Interval for state change broadcast (0 = single).' },
        bCreateBoxObstacle: { type: 'boolean', description: 'Add box obstacle during nav generation.' },
        obstacleOffset: {
          type: 'object',
          properties: commonSchemas.vector3.properties,
          description: 'Offset of simple box obstacle.'
        },
        obstacleExtent: {
          type: 'object',
          properties: commonSchemas.vector3.properties,
          description: 'Extent of simple box obstacle.'
        },
        obstacleAreaClass: { type: 'string', description: 'Area class for box obstacle.' },
        save: commonSchemas.save
      ,
        componentName: commonSchemas.componentName,
        location: {
                  type: 'object',
                  properties: {
                    x: commonSchemas.numberProp, y: commonSchemas.numberProp, z: commonSchemas.numberProp
                  },
                  description: 'World location for nav link proxy.'
                },
        rotation: {
                  type: 'object',
                  properties: {
                    pitch: commonSchemas.numberProp, yaw: commonSchemas.numberProp, roll: commonSchemas.numberProp
                  },
                  description: 'Rotation for nav link proxy.'
                },
        assetPath: commonSchemas.assetPath,
        childNodeId: commonSchemas.nodeId,
        comment: commonSchemas.stringProp,
        enableDamage: commonSchemas.booleanProp,
        enableHearing: commonSchemas.booleanProp,
        enableSight: commonSchemas.booleanProp,
        enabled: commonSchemas.booleanProp,
        focusActorName: commonSchemas.actorName,
        hearingRange: commonSchemas.numberProp,
        loseSightRadius: commonSchemas.numberProp,
        nodeType: commonSchemas.stringProp,
        offset: commonSchemas.vector3,
        parentStateName: commonSchemas.stringProp,
        peripheralVisionAngle: commonSchemas.numberProp,
        properties: commonSchemas.objectProp,
        savePath: commonSchemas.savePath,
        sightRadius: commonSchemas.numberProp,
        spawnCount: commonSchemas.numberProp,
        stateType: commonSchemas.stringProp,
        triggerType: commonSchemas.stringProp,
        value: commonSchemas.value,
        x: commonSchemas.numberProp,
        y: commonSchemas.numberProp
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
  {
    name: 'manage_inventory',
    category: 'gameplay',
    description: 'Create item data assets, inventory components, world pickups, loot tables, and crafting recipes.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'create_item_data_asset',
            'set_item_properties',
            'create_item_category',
            'assign_item_category',
            'create_inventory_component',
            'configure_inventory_slots',
            'add_inventory_functions',
            'configure_inventory_events',
            'set_inventory_replication',
            'create_pickup_actor',
            'configure_pickup_interaction',
            'configure_pickup_respawn',
            'configure_pickup_effects',
            'create_equipment_component',
            'define_equipment_slots',
            'configure_equipment_effects',
            'add_equipment_functions',
            'configure_equipment_visuals',
            'create_loot_table',
            'add_loot_entry',
            'configure_loot_drop',
            'set_loot_quality_tiers',
            'create_crafting_recipe',
            'configure_recipe_requirements',
            'create_crafting_station',
            'add_crafting_component',
            'configure_item_stacking',
            'set_item_icon',
            'add_recipe_ingredient',
            'remove_loot_entry',
            'configure_inventory_weight',
            'configure_station_recipes',
            'get_inventory_info'
          ],
          description: 'Inventory action to perform.'
        },
        name: commonSchemas.assetNameForCreation,
        path: commonSchemas.directoryPathForCreation,
        save: commonSchemas.save,
        blueprintPath: commonSchemas.blueprintPath,
        itemPath: commonSchemas.itemDataPath,
        iconPath: commonSchemas.iconPath,
        maxStackSize: commonSchemas.numberProp,
        stackable: commonSchemas.booleanProp,
        uniqueItems: commonSchemas.booleanProp,
        enableWeight: commonSchemas.booleanProp,
        encumberanceSystem: commonSchemas.booleanProp,
        encumberanceThreshold: commonSchemas.numberProp,
        categoryPath: { type: 'string', description: 'Path to item category asset.' },
        componentName: commonSchemas.componentName,
        slotCount: commonSchemas.numberProp,
        maxWeight: commonSchemas.numberProp,
        replicated: commonSchemas.replicated,
        replicationCondition: {
          type: 'string',
          enum: ['None', 'OwnerOnly', 'SkipOwner', 'SimulatedOnly', 'AutonomousOnly', 'Custom'],
          description: 'Replication condition for inventory.'
        },
        pickupPath: { type: 'string', description: 'Path to pickup actor Blueprint.' },
        interactionType: {
          type: 'string',
          enum: ['Overlap', 'Interact', 'Key', 'Hold'],
          description: 'How player picks up item.'
        },
        prompt: commonSchemas.prompt,
        respawnable: commonSchemas.booleanProp,
        respawnTime: commonSchemas.respawnTime,
        bobbing: { type: 'boolean', description: 'Enable bobbing animation.' },
        rotation: { type: 'boolean', description: 'Enable rotation animation.' },
        glowEffect: { type: 'boolean', description: 'Enable glow effect.' },
        slots: {
          type: 'array',
          items: commonSchemas.stringProp,
          description: 'Equipment slot names to configure.'
        },
        statModifiers: { type: 'boolean', description: 'Enable stat modifier support when equipped.' },
        abilityGrants: { type: 'boolean', description: 'Enable ability grant support when equipped.' },
        passiveEffects: { type: 'boolean', description: 'Enable passive effect support when equipped.' },
        attachToSocket: { type: 'boolean', description: 'Attach mesh to socket when equipped.' },
        lootTablePath: commonSchemas.lootTablePath,
        lootWeight: { type: 'number', description: 'Weight for drop chance calculation.' },
        minQuantity: { type: 'number', description: 'Minimum drop quantity.' },
        maxQuantity: { type: 'number', description: 'Maximum drop quantity.' },
        actorPath: { type: 'string', description: 'Path to actor Blueprint for loot drop.' },
        dropCount: { type: 'number', description: 'Number of drops to roll.' },
        dropRadius: { type: 'number', description: 'Radius for scattered drops.' },
        tiers: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              name: commonSchemas.stringProp,
              dropWeight: commonSchemas.numberProp
            }
          },
          description: 'Quality tier definitions.'
        },
        recipePath: commonSchemas.recipePath,
        outputItemPath: { type: 'string', description: 'Path to item produced by recipe.' },
        outputQuantity: { type: 'number', description: 'Quantity produced.' },
        craftTime: { type: 'number', description: 'Time in seconds to craft.' },
        ingredientItemPath: commonSchemas.itemDataPath,
        quantity: commonSchemas.numberProp,
        requiredLevel: { type: 'number', description: 'Required player level.' },
        requiredStation: { type: 'string', description: 'Required crafting station type.' },
        stationPath: commonSchemas.stringProp,
        recipePaths: commonSchemas.arrayOfStrings,
        craftingSpeedMultiplier: commonSchemas.numberProp,
        stationType: { type: 'string', description: 'Type of crafting station.' },
        defaultSocket: commonSchemas.socketName,
        dropOnDeath: commonSchemas.booleanProp,
        entryIndex: commonSchemas.numberProp,
        properties: commonSchemas.objectProp
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        ...commonSchemas.outputBase,
        assetPath: commonSchemas.assetPath,
        itemPath: commonSchemas.stringProp,
        categoryPath: commonSchemas.stringProp,
        pickupPath: commonSchemas.stringProp,
        lootTablePath: commonSchemas.stringProp,
        recipePath: commonSchemas.stringProp,
        stationPath: commonSchemas.stringProp,
        componentAdded: commonSchemas.booleanProp,
        slotCount: commonSchemas.integerProp,
        entryIndex: commonSchemas.integerProp,
        inventoryInfo: {
          type: 'object',
          properties: {
            assetType: { type: 'string', enum: ['Item', 'Inventory', 'Pickup', 'LootTable', 'Recipe', 'Station'] },
            itemProperties: {
              type: 'object',
              properties: {
                displayName: commonSchemas.stringProp,
                stackSize: commonSchemas.integerProp,
                weight: commonSchemas.numberProp,
                rarity: commonSchemas.stringProp,
                value: commonSchemas.numberProp
              }
            },
            inventorySlots: commonSchemas.numberProp,
            maxWeight: commonSchemas.numberProp,
            equipmentSlots: commonSchemas.arrayOfStrings,
            lootEntries: commonSchemas.numberProp,
            recipeIngredients: commonSchemas.arrayOfObjects,
            craftTime: commonSchemas.numberProp
          },
          description: 'Inventory system info (for get_inventory_info).'
        },
        error: commonSchemas.stringProp
      }
    }
  },
  {
    name: 'manage_interaction',
    category: 'gameplay',
    description: 'Create interactive objects: doors, switches, chests, levers. Set up destructible meshes and trigger volumes.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
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
            'get_interaction_info'
          ],
          description: 'The interaction action to perform.'
        },
        name: commonSchemas.name,
        folder: commonSchemas.directoryPath,
        blueprintPath: commonSchemas.blueprintPath,
        actorName: commonSchemas.actorName,
        componentName: commonSchemas.componentName,
        traceType: { type: 'string', enum: ['line', 'sphere', 'box'], description: 'Type of interaction trace.' },
        traceDistance: commonSchemas.traceDistance,
        traceRadius: commonSchemas.traceRadius,
        widgetClass: commonSchemas.widgetClass,
        showOnHover: { type: 'boolean', description: 'Show widget when hovering.' },
        showPromptText: { type: 'boolean', description: 'Show interaction prompt text.' },
        promptTextFormat: { type: 'string', description: 'Format string for prompt (e.g., "Press {Key} to {Action}").' },
        doorPath: { type: 'string', description: 'Path to door actor blueprint.' },
        openAngle: { type: 'number', description: 'Door open rotation angle in degrees.' },
        openTime: { type: 'number', description: 'Time to open/close door in seconds.' },
        locked: commonSchemas.locked,
        autoClose: { type: 'boolean', description: 'Automatically close after opening.' },
        autoCloseDelay: { type: 'number', description: 'Delay before auto-close in seconds.' },
        requiresKey: { type: 'boolean', description: 'Whether interaction requires a key item.' },
        switchPath: { type: 'string', description: 'Path to switch actor blueprint.' },
        switchType: { type: 'string', enum: ['button', 'lever', 'pressure_plate', 'toggle'], description: 'Type of switch.' },
        resetTime: { type: 'number', description: 'Time to reset switch in seconds.' },
        chestPath: { type: 'string', description: 'Path to chest actor blueprint.' },
        lootTablePath: commonSchemas.lootTablePath,
        triggerPath: { type: 'string', description: 'Path to trigger actor blueprint.' },
        triggerShape: { type: 'string', enum: ['box', 'sphere', 'capsule'], description: 'Shape of trigger volume.' },
        canToggle: commonSchemas.booleanProp
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: commonSchemas.booleanProp,
        message: commonSchemas.stringProp,
        assetPath: commonSchemas.assetPath,
        blueprintPath: commonSchemas.blueprintPath,
        interfacePath: { type: 'string', description: 'Path to created interface.' },
        componentAdded: { type: 'boolean', description: 'Whether component was added.' },
        interactionInfo: {
          type: 'object',
          properties: {
            assetType: { type: 'string', enum: ['Door', 'Switch', 'Chest', 'Lever', 'Trigger', 'Destructible', 'Component'] },
            isLocked: commonSchemas.booleanProp,
            isOpen: commonSchemas.booleanProp,
            health: commonSchemas.numberProp,
            maxHealth: commonSchemas.numberProp,
            interactionEnabled: commonSchemas.booleanProp,
            triggerShape: commonSchemas.stringProp,
            destructionLevel: commonSchemas.numberProp
          },
          description: 'Interaction system info (for get_interaction_info).'
        },
        error: commonSchemas.stringProp
      }
    }
  },
  {
    name: 'manage_networking',
    category: 'utility',
    description: 'Configure multiplayer and player flow: replication, RPCs, authority/relevancy, network prediction, sessions, split-screen, LAN/voice chat, game framework classes, match rules, and input mappings.',
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
            'get_networking_info'
          ,
            ...SESSION_ACTIONS, ...GAME_FRAMEWORK_ACTIONS, ...INPUT_ACTIONS],
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
        functionName: commonSchemas.functionName,
        rpcType: {
          type: 'string',
          enum: ['Server', 'Client', 'NetMulticast'],
          description: 'Type of RPC.'
        },
        reliable: commonSchemas.reliable,
        withValidation: { type: 'boolean', description: 'Enable RPC validation.' },
        ownerActorName: { type: 'string', description: 'Name of owner actor (null to clear).' },
        isAutonomousProxy: { type: 'boolean', description: 'Configure as autonomous proxy.' },
        netCullDistanceSquared: { type: 'number', description: 'Network cull distance squared.' },
        useOwnerNetRelevancy: { type: 'boolean', description: 'Use owner relevancy.' },
        alwaysRelevant: { type: 'boolean', description: 'Always relevant to all clients.' },
        onlyRelevantToOwner: { type: 'boolean', description: 'Only relevant to owner.' },
        structName: { type: 'string', description: 'Name of struct for custom serialization.' },
        usePushModel: { type: 'boolean', description: 'Use push-model replication.' },
        enablePrediction: { type: 'boolean', description: 'Enable client-side prediction.' },
        correctionThreshold: { type: 'number', description: 'Server correction threshold.' },
        smoothingRate: { type: 'number', description: 'Smoothing rate for corrections.' },
        dataType: {
          type: 'string',
          enum: ['Transform', 'Vector', 'Rotator', 'Float'],
          description: 'Network prediction data type.'
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
        // Additional params for C++ handler alignment (NetworkingHandlers.cpp)
        spatiallyLoaded: { type: 'boolean', description: 'Spatially loaded for replication graph.' },
        netLoadOnClient: { type: 'boolean', description: 'Net load on client for replication graph.' },
        replicationPolicy: { type: 'string', description: 'Replication policy for replication graph.' },
        customSerialization: { type: 'boolean', description: 'Use custom serialization.' },
        predictionThreshold: { type: 'number', description: 'Prediction threshold for client prediction.' },
        sessionName: commonSchemas.sessionName,
        maxPlayers: commonSchemas.numberProp,
        bIsLANMatch: { type: 'boolean', description: 'Whether this is a LAN match.' },
        bAllowJoinInProgress: { type: 'boolean', description: 'Allow joining games in progress.' },
        bAllowInvites: { type: 'boolean', description: 'Allow player invites.' },
        bUsesPresence: { type: 'boolean', description: 'Use presence for session discovery.' },
        bUseLobbiesIfAvailable: { type: 'boolean', description: 'Use lobby system if available.' },
        bShouldAdvertise: { type: 'boolean', description: 'Advertise session publicly.' },
        interfaceType: {
                  type: 'string',
                  enum: ['Default', 'LAN', 'Null'],
                  description: 'Type of session interface to use. REQUIRED for configure_session_interface.'
                },
        enabled: commonSchemas.enabled,
        splitScreenType: {
                  type: 'string',
                  enum: ['None', 'TwoPlayer_Horizontal', 'TwoPlayer_Vertical', 'ThreePlayer_FavorTop', 'ThreePlayer_FavorBottom', 'FourPlayer_Grid'],
                  description: 'Split-screen layout type. REQUIRED for set_split_screen_type.'
                },
        playerIndex: { ...commonSchemas.numberProp, description: 'Local player index. REQUIRED for remove_local_player.' },
        controllerId: { ...commonSchemas.numberProp, description: 'Controller ID for player input. REQUIRED for add_local_player.' },
        serverAddress: { ...commonSchemas.serverAddress, description: 'Server IP address. REQUIRED for join_lan_server.' },
        serverPort: commonSchemas.numberProp,
        serverPassword: { type: 'string', description: 'Server password for protected games.' },
        serverName: { type: 'string', description: 'Display name for the server.' },
        mapName: { type: 'string', description: 'Map to load for hosting. REQUIRED for host_lan_server.' },
        travelOptions: { type: 'string', description: 'Travel URL options string.' },
        voiceEnabled: { type: 'boolean', description: 'Enable/disable voice chat. REQUIRED for enable_voice_chat.' },
        voiceSettings: {
                  type: 'object',
                  properties: {
                    volume: { type: 'number', description: 'Voice volume (0.0 - 1.0).' },
                    noiseGateThreshold: { type: 'number', description: 'Noise gate threshold.' },
                    noiseSuppression: { type: 'boolean', description: 'Enable noise suppression.' },
                    echoCancellation: { type: 'boolean', description: 'Enable echo cancellation.' },
                    sampleRate: { type: 'number', description: 'Audio sample rate in Hz.' }
                  },
                  description: 'Voice processing settings. REQUIRED for configure_voice_settings.'
                },
        channelName: { ...commonSchemas.channelName, description: 'Voice channel name. REQUIRED for set_voice_channel.' },
        channelType: {
                  type: 'string',
                  enum: ['Team', 'Global', 'Proximity', 'Party'],
                  description: 'Voice channel type.'
                },
        playerName: { type: 'string', description: 'Player name for voice operations. REQUIRED for mute_player (or targetPlayerId).' },
        targetPlayerId: { type: 'string', description: 'Target player ID. REQUIRED for mute_player (or playerName).' },
        muted: commonSchemas.muted,
        attenuationRadius: { type: 'number', description: 'Radius for voice attenuation (Proximity chat). REQUIRED for set_voice_attenuation.' },
        attenuationFalloff: { type: 'number', description: 'Falloff rate for voice attenuation.' },
        pushToTalkEnabled: { type: 'boolean', description: 'Enable push-to-talk mode. REQUIRED for configure_push_to_talk.' },
        pushToTalkKey: { type: 'string', description: 'Key binding for push-to-talk.' },
        name: commonSchemas.name,
        path: commonSchemas.directoryPathForCreation,
        gameModeBlueprint: { type: 'string', description: 'Path to GameMode blueprint to configure.' },
        parentClass: commonSchemas.parentClass,
        pawnClass: { type: 'string', description: 'Pawn class to use.' },
        defaultPawnClass: { type: 'string', description: 'Default pawn class for GameMode.' },
        playerControllerClass: { type: 'string', description: 'PlayerController class path.' },
        gameStateClass: { type: 'string', description: 'GameState class path.' },
        playerStateClass: { type: 'string', description: 'PlayerState class path.' },
        spectatorClass: { type: 'string', description: 'Spectator pawn class.' },
        hudClass: { type: 'string', description: 'HUD class path.' },
        bDelayedStart: { type: 'boolean', description: 'Whether to delay match start.' },
        states: {
                  type: 'array',
                  items: {
                    type: 'object',
                    properties: {
                      name: { type: 'string', enum: ['waiting', 'warmup', 'in_progress', 'post_match', 'custom'] },
                      duration: commonSchemas.duration,
                      customName: { type: 'string', description: 'Custom state name if name is "custom".' }
                    }
                  },
                  description: 'Match state definitions.'
                },
        numRounds: commonSchemas.numberProp,
        roundTime: commonSchemas.numberProp,
        intermissionTime: commonSchemas.numberProp,
        numTeams: commonSchemas.numberProp,
        teamSize: commonSchemas.numberProp,
        autoBalance: { type: 'boolean', description: 'Enable automatic team balancing.' },
        friendlyFire: { type: 'boolean', description: 'Enable friendly fire damage.' },
        teamIndex: { type: 'number', description: 'Team index for PlayerStart.' },
        scorePerKill: { type: 'number', description: 'Points awarded per kill.' },
        scorePerObjective: { type: 'number', description: 'Points awarded per objective.' },
        scorePerAssist: { type: 'number', description: 'Points awarded per assist.' },
        spawnSelectionMethod: {
                  type: 'string',
                  enum: ['Random', 'RoundRobin', 'FarthestFromEnemies'],
                  description: 'How to select spawn points.'
                },
        respawnDelay: commonSchemas.numberProp,
        respawnLocation: {
                  type: 'string',
                  enum: ['PlayerStart', 'LastDeath', 'TeamBase'],
                  description: 'Where players respawn.'
                },
        usePlayerStarts: { type: 'boolean', description: 'Use PlayerStart actors.' },
        allowSpectating: { type: 'boolean', description: 'Allow spectator mode.' },
        spectatorViewMode: {
                  type: 'string',
                  enum: ['FreeCam', 'ThirdPerson', 'FirstPerson', 'DeathCam'],
                  description: 'Spectator view mode.'
                },
        save: commonSchemas.save,
        contextPath: commonSchemas.assetPath,
        actionPath: commonSchemas.assetPath,
        key: commonSchemas.stringProp,
        triggerType: commonSchemas.stringProp,
        modifierType: commonSchemas.stringProp,
        assetPath: commonSchemas.assetPath,
        priority: { type: 'number', description: 'Priority for input mapping context (default: 0).' },
        timeoutMs: commonSchemas.numberProp,
        canRespawn: commonSchemas.booleanProp,
        executeTravel: commonSchemas.booleanProp,
        forceRespawn: commonSchemas.booleanProp,
        localPlayerNum: commonSchemas.numberProp,
        maxRespawns: commonSchemas.numberProp,
        respawnLives: commonSchemas.numberProp,
        scorePerDeath: commonSchemas.numberProp,
        systemWide: commonSchemas.booleanProp,
        variableName: commonSchemas.variableName,
        winScore: commonSchemas.numberProp
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
  {
    name: 'manage_level_structure',
    category: 'world',
    description: 'Structure worlds: levels, sublevels, World Partition, streaming, data layers, HLOD, level instances, trigger/blocking/physics/audio/post-process volumes, and nav bounds.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'create_level', 'create_sublevel', 'configure_level_streaming',
            'set_streaming_distance', 'configure_level_bounds',
            'enable_world_partition', 'configure_grid_size', 'create_data_layer',
            'assign_actor_to_data_layer', 'configure_hlod_layer', 'create_minimap_volume',
            'open_level_blueprint', 'add_level_blueprint_node', 'connect_level_blueprint_nodes',
            'create_level_instance', 'create_packed_level_actor',
            'get_level_structure_info'
          ,
            ...VOLUME_ACTIONS],
          description: 'Level structure action to perform.'
        },
        levelName: commonSchemas.stringProp,
        levelPath: commonSchemas.levelPath,
        parentLevel: commonSchemas.parentLevel,
        bCreateWorldPartition: { type: 'boolean', description: 'Create with World Partition enabled.' },
        bUseExternalActors: { type: 'boolean', description: 'Enable One File Per Actor (OFPA/External Actors) for Data Layer compatibility. Automatically enabled when bCreateWorldPartition is true.' },
        sublevelName: commonSchemas.sublevelName,
        sublevelPath: commonSchemas.levelPath,
        streamingMethod: {
          type: 'string',
          enum: ['Blueprint', 'AlwaysLoaded', 'Disabled'],
          description: 'Level streaming method.'
        },
        bShouldBeVisible: { type: 'boolean', description: 'Level should be visible when loaded.' },
        bShouldBlockOnLoad: { type: 'boolean', description: 'Block game until level is loaded.' },
        bDisableDistanceStreaming: { type: 'boolean', description: 'Disable distance-based streaming.' },
        streamingDistance: { type: 'number', description: 'Distance/radius for streaming volume (creates ALevelStreamingVolume).' },
        streamingUsage: {
          type: 'string',
          enum: ['Loading', 'LoadingAndVisibility', 'VisibilityBlockingOnLoad', 'BlockingOnLoad', 'LoadingNotVisible'],
          description: 'Streaming volume usage mode (default: LoadingAndVisibility).'
        },
        createVolume: { type: 'boolean', description: 'Create a streaming volume (true) or just report existing volumes (false). Default: true.' },
        boundsOrigin: {
          type: 'object',
          properties: {
            x: commonSchemas.numberProp, y: commonSchemas.numberProp, z: commonSchemas.numberProp
          },
          description: 'Origin of level bounds.'
        },
        boundsExtent: {
          type: 'object',
          properties: {
            x: commonSchemas.numberProp, y: commonSchemas.numberProp, z: commonSchemas.numberProp
          },
          description: 'Extent of level bounds.'
        },
        bAutoCalculateBounds: { type: 'boolean', description: 'Auto-calculate bounds from content.' },
        bEnableWorldPartition: { type: 'boolean', description: 'Enable World Partition for level.' },
        gridCellSize: { type: 'number', description: 'World Partition grid cell size.' },
        loadingRange: { type: 'number', description: 'Loading range for grid cells.' },
        dataLayerName: commonSchemas.dataLayerName,
        bIsInitiallyVisible: { type: 'boolean', description: 'Data layer initially visible.' },
        bIsInitiallyLoaded: { type: 'boolean', description: 'Data layer initially loaded.' },
        dataLayerType: {
          type: 'string',
          enum: ['Runtime', 'Editor'],
          description: 'Type of data layer.'
        },
        actorName: commonSchemas.actorName,
        actorPath: commonSchemas.actorPath,
        hlodLayerName: { type: 'string', description: 'Name of the HLOD layer.' },
        hlodLayerPath: commonSchemas.hlodLayerPath,
        bIsSpatiallyLoaded: { type: 'boolean', description: 'HLOD is spatially loaded.' },
        cellSize: { type: 'number', description: 'HLOD cell size.' },
        loadingDistance: { type: 'number', description: 'HLOD loading distance.' },
        volumeName: commonSchemas.volumeName,
        volumeLocation: {
          type: 'object',
          properties: {
            x: commonSchemas.numberProp, y: commonSchemas.numberProp, z: commonSchemas.numberProp
          },
          description: 'Location of the volume.'
        },
        volumeExtent: {
          type: 'object',
          properties: {
            x: commonSchemas.numberProp, y: commonSchemas.numberProp, z: commonSchemas.numberProp
          },
          description: 'Extent of the volume.'
        },
        nodeClass: commonSchemas.nodeClass,
        nodePosition: {
          type: 'object',
          properties: {
            x: commonSchemas.numberProp, y: commonSchemas.numberProp
          },
          description: 'Position of node in graph.'
        },
        nodeName: commonSchemas.nodeName,
        sourceNodeName: commonSchemas.sourceNode,
        sourcePinName: commonSchemas.sourcePin,
        targetNodeName: commonSchemas.targetNode,
        targetPinName: commonSchemas.targetPin,
        levelInstanceName: commonSchemas.levelInstanceName,
        levelAssetPath: { type: 'string', description: 'Path to the level asset for instancing.' },
        instanceLocation: {
          type: 'object',
          properties: {
            x: commonSchemas.numberProp, y: commonSchemas.numberProp, z: commonSchemas.numberProp
          },
          description: 'Location of the level instance.'
        },
        instanceRotation: {
          type: 'object',
          properties: {
            pitch: commonSchemas.numberProp, yaw: commonSchemas.numberProp, roll: commonSchemas.numberProp
          },
          description: 'Rotation of the level instance.'
        },
        instanceScale: {
          type: 'object',
          properties: {
            x: commonSchemas.numberProp, y: commonSchemas.numberProp, z: commonSchemas.numberProp
          },
          description: 'Scale of the level instance.'
        },
        packedLevelName: { type: 'string', description: 'Name for the packed level actor.' },
        bPackBlueprints: { type: 'boolean', description: 'Include blueprints in packed level.' },
        bPackStaticMeshes: { type: 'boolean', description: 'Include static meshes in packed level.' },
        save: commonSchemas.save
      ,
        // For add_*_volume actions that attach to existing actors
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
        bWaterVolume: { type: 'boolean', description: 'Whether this is a water volume.' },
        fluidFriction: { type: 'number', description: 'Fluid friction for physics volumes.' },
        terminalVelocity: { type: 'number', description: 'Terminal velocity in the volume.' },
        priority: commonSchemas.priority,
        bEnabled: { type: 'boolean', description: 'Whether the audio volume is enabled.' },
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
        bUnbound: { type: 'boolean', description: 'Whether post process volume affects entire world.' },
        blendRadius: { type: 'number', description: 'Blend radius for post process volume.' },
        blendWeight: { type: 'number', description: 'Blend weight (0.0-1.0) for post process.' },
        filter: commonSchemas.filter,
        volumeType: { type: 'string', description: 'Type filter for get_volumes_info (e.g., "Trigger", "Physics").' },
        bBlockOnSlowStreaming: commonSchemas.booleanProp,
        bounds: commonSchemas.objectProp,
        createIfMissing: commonSchemas.booleanProp,
        gridName: commonSchemas.stringProp,
        layerType: commonSchemas.stringProp
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: commonSchemas.booleanProp,
        message: commonSchemas.stringProp,
        levelPath: commonSchemas.levelPath,
        sublevelPath: commonSchemas.levelPath,
        dataLayerName: { type: 'string', description: 'Name of created data layer.' },
        hlodLayerPath: commonSchemas.hlodLayerPath,
        nodeName: commonSchemas.nodeName,
        levelInstanceName: commonSchemas.levelInstanceName,
        levelStructureInfo: {
          type: 'object',
          properties: {
            currentLevel: commonSchemas.stringProp,
            sublevelCount: commonSchemas.numberProp,
            sublevels: {
              type: 'array',
              items: commonSchemas.stringProp
            },
            worldPartitionEnabled: commonSchemas.booleanProp,
            gridCellSize: commonSchemas.numberProp,
            dataLayers: {
              type: 'array',
              items: commonSchemas.stringProp
            },
            hlodLayers: {
              type: 'array',
              items: commonSchemas.stringProp
            },
            levelInstances: {
              type: 'array',
              items: commonSchemas.stringProp
            }
          },
          description: 'Level structure information (for get_level_structure_info).'
        },
        error: commonSchemas.stringProp
      }
    }
  },

];

addActionParamsSchema(consolidatedToolDefinitions);
