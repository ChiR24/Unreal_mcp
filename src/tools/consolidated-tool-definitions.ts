import { commonSchemas } from './tool-definition-utils.js';
/** MCP Tool Definition type for explicit annotation to avoid TS7056 */
export interface ToolDefinition {
  category?: 'core' | 'world' | 'authoring' | 'gameplay' | 'utility';
  name: string;
  description: string;
  inputSchema: Record<string, unknown>;
  outputSchema?: Record<string, unknown>;
  [key: string]: unknown;
}
export const consolidatedToolDefinitions: ToolDefinition[] = [
  {
    name: 'manage_pipeline',
    description: 'Filter visible tools by category. Actions: list_categories (show available), set_categories (enable specific), get_status (current state). Categories: core, world, authoring, gameplay, utility, all.',
    category: 'core',
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
    description: 'Create, import, duplicate, rename, delete assets. Edit Material graphs and instances. Analyze dependencies.',
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
        maxDepth: commonSchemas.numberProp
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
    name: 'manage_blueprint',
    category: 'authoring',
    description: 'Create Blueprints, add SCS components (mesh, collision, camera), and manipulate graph nodes.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'create', 'get_blueprint', 'get', 'compile',
            'add_component', 'set_default', 'modify_scs', 'get_scs', 'add_scs_component', 'remove_scs_component', 'reparent_scs_component', 'set_scs_transform', 'set_scs_property',
            'ensure_exists', 'probe_handle', 'add_variable', 'remove_variable', 'rename_variable', 'add_function', 'add_event', 'remove_event', 'add_construction_script', 'set_variable_metadata', 'set_metadata',
            'create_node', 'add_node', 'delete_node', 'connect_pins', 'break_pin_links', 'set_node_property', 'create_reroute_node', 'get_node_details', 'get_graph_details', 'get_pin_details',
            'list_node_types', 'set_pin_default_value'
          ],
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
        compile: commonSchemas.compile,
        save: commonSchemas.save,
        eventType: commonSchemas.stringProp,
        customEventName: commonSchemas.eventName,
        parameters: commonSchemas.arrayOfObjects
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        ...commonSchemas.outputBase,
        blueprintPath: commonSchemas.blueprintPath,
        blueprint: commonSchemas.objectProp
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
            'spawn', 'spawn_blueprint', 'delete', 'delete_by_tag', 'duplicate',
            'apply_force', 'set_transform', 'get_transform', 'set_visibility',
            'add_component', 'set_component_properties', 'get_components',
            'add_tag', 'remove_tag', 'find_by_tag', 'find_by_name', 'list', 'set_blueprint_variables',
            'create_snapshot', 'attach', 'detach'
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
        data: commonSchemas.objectProp
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
            'play', 'stop', 'stop_pie', 'pause', 'resume', 'set_game_speed', 'eject', 'possess',
            'set_camera', 'set_camera_position', 'set_camera_fov', 'set_view_mode',
            'set_viewport_resolution', 'console_command', 'execute_command', 'screenshot', 'step_frame',
            'start_recording', 'stop_recording', 'create_bookmark', 'jump_to_bookmark',
            'set_preferences', 'set_viewport_realtime', 'open_asset', 'simulate_input'
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
        eventType: { type: 'string', enum: ['KeyDown', 'KeyUp', 'Both'] }
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
    description: 'Load/save levels, streaming, World Partition, data layers, HLOD, sublevels, level instances.',
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
            'connect_level_blueprint_nodes', 'create_level_instance', 'create_packed_level_actor', 'get_level_structure_info'
          ],
          description: 'Level action'
        },
        // Core properties
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
    name: 'animation_physics',
    category: 'utility',
    description: 'Create Animation BPs, Montages, Blend Spaces, IK rigs, ragdolls, vehicles, and author animation sequences/curves.',
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
            'create_ik_rig', 'add_ik_chain', 'create_ik_retargeter', 'set_retarget_chain_mapping',
            'get_animation_info'
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
    description: 'Spawn Niagara/Cascade particles, draw debug shapes, edit VFX node graphs, and author Niagara systems/emitters.',
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
            'get_niagara_info', 'validate_niagara_system'
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
    description: 'Create/sculpt landscapes, paint foliage, generate procedural terrain/biomes, configure sky/fog/clouds, water bodies, and weather systems.',
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
            'configure_rain_particles', 'configure_snow_particles', 'configure_lightning'
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
        bottomRadius: { type: 'number', description: 'Bottom radius of the atmosphere in km' },
        atmosphereHeight: { type: 'number', description: 'Height of the atmosphere in km' },
        mieAnisotropy: { type: 'number', description: 'Mie scattering anisotropy factor (-1 to 1)' },
        mieScatteringScale: { type: 'number', description: 'Scale for Mie scattering' },
        rayleighScatteringScale: { type: 'number', description: 'Scale for Rayleigh scattering' },
        multiScatteringFactor: { type: 'number', description: 'Multi-scattering approximation factor' },
        rayleighExponentialDistribution: { type: 'number', description: 'Rayleigh exponential distribution' },
        mieExponentialDistribution: { type: 'number', description: 'Mie exponential distribution' },
        mieAbsorptionScale: { type: 'number', description: 'Mie absorption scale' },
        otherAbsorptionScale: { type: 'number', description: 'Other absorption scale (ozone)' },
        heightFogContribution: { type: 'number', description: 'Height fog contribution to atmosphere' },
        aerialPerspectiveViewDistanceScale: { type: 'number', description: 'Aerial perspective view distance scale' },
        transmittanceMinLightElevationAngle: { type: 'number', description: 'Minimum light elevation angle for transmittance' },
        aerialPerspectiveStartDepth: { type: 'number', description: 'Aerial perspective start depth' },
        rayleighScattering: { 
          type: 'object', 
          description: 'Rayleigh scattering color as {r, g, b}',
          properties: { r: { type: 'number' }, g: { type: 'number' }, b: { type: 'number' } }
        },
        mieScattering: { 
          type: 'object', 
          description: 'Mie scattering color as {r, g, b}',
          properties: { r: { type: 'number' }, g: { type: 'number' }, b: { type: 'number' } }
        },
        mieAbsorption: { 
          type: 'object', 
          description: 'Mie absorption color as {r, g, b}',
          properties: { r: { type: 'number' }, g: { type: 'number' }, b: { type: 'number' } }
        },
        skyLuminanceFactor: { 
          type: 'object', 
          description: 'Sky luminance factor as {r, g, b}',
          properties: { r: { type: 'number' }, g: { type: 'number' }, b: { type: 'number' } }
        },
        // Phase 28: Exponential Height Fog properties
        fogDensity: { type: 'number', description: 'Global fog density (0.0 - 1.0)' },
        fogHeightFalloff: { type: 'number', description: 'Rate at which fog density decreases with height' },
        fogMaxOpacity: { type: 'number', description: 'Maximum fog opacity (0.0 - 1.0)' },
        startDistance: { type: 'number', description: 'Distance from the camera at which fog starts' },
        endDistance: { type: 'number', description: 'End distance for fog' },
        fogCutoffDistance: { type: 'number', description: 'Distance at which fog is completely cut off' },
        volumetricFog: { type: 'boolean', description: 'Enable volumetric fog' },
        volumetricFogScatteringDistribution: { type: 'number', description: 'Phase function for volumetric fog' },
        volumetricFogExtinctionScale: { type: 'number', description: 'Scale for volumetric fog extinction' },
        volumetricFogDistance: { type: 'number', description: 'Volumetric fog distance' },
        volumetricFogStartDistance: { type: 'number', description: 'Volumetric fog start distance' },
        volumetricFogNearFadeInDistance: { type: 'number', description: 'Volumetric fog near fade-in distance' },
        fogInscatteringColor: { 
          type: 'object', 
          description: 'Fog inscattering color as {r, g, b}',
          properties: { r: { type: 'number' }, g: { type: 'number' }, b: { type: 'number' } }
        },
        directionalInscatteringColor: { 
          type: 'object', 
          description: 'Directional inscattering color as {r, g, b}',
          properties: { r: { type: 'number' }, g: { type: 'number' }, b: { type: 'number' } }
        },
        volumetricFogAlbedo: { 
          type: 'object', 
          description: 'Volumetric fog albedo as {r, g, b} (0.0 - 1.0)',
          properties: { r: { type: 'number' }, g: { type: 'number' }, b: { type: 'number' } }
        },
        volumetricFogEmissive: { 
          type: 'object', 
          description: 'Volumetric fog emissive color as {r, g, b}',
          properties: { r: { type: 'number' }, g: { type: 'number' }, b: { type: 'number' } }
        },
        directionalInscatteringExponent: { type: 'number', description: 'Directional inscattering exponent' },
        directionalInscatteringStartDistance: { type: 'number', description: 'Directional inscattering start distance' },
        secondFogDensity: { type: 'number', description: 'Second fog layer density' },
        secondFogHeightFalloff: { type: 'number', description: 'Second fog layer height falloff' },
        secondFogHeightOffset: { type: 'number', description: 'Second fog layer height offset' },
        inscatteringColorCubemapAngle: { type: 'number', description: 'Inscattering color cubemap angle' },
        fullyDirectionalInscatteringColorDistance: { type: 'number', description: 'Fully directional inscattering color distance' },
        nonDirectionalInscatteringColorDistance: { type: 'number', description: 'Non-directional inscattering color distance' },
        inscatteringTextureTint: { 
          type: 'object', 
          description: 'Inscattering texture tint as {r, g, b}',
          properties: { r: { type: 'number' }, g: { type: 'number' }, b: { type: 'number' } }
        },
        // Phase 28: Volumetric Cloud properties
        layerBottomAltitude: { type: 'number', description: 'Bottom altitude of cloud layer in km' },
        layerHeight: { type: 'number', description: 'Height of cloud layer in km' },
        tracingStartMaxDistance: { type: 'number', description: 'Max distance for cloud tracing start' },
        tracingStartDistanceFromCamera: { type: 'number', description: 'Distance from camera to start tracing' },
        tracingMaxDistance: { type: 'number', description: 'Maximum ray marching distance for clouds' },
        planetRadius: { type: 'number', description: 'Planet radius in km (used when no SkyAtmosphere)' },
        groundAlbedo: { 
          type: 'object', 
          description: 'Ground albedo color as {r, g, b} (0.0 - 1.0)',
          properties: { r: { type: 'number' }, g: { type: 'number' }, b: { type: 'number' } }
        },
        usePerSampleAtmosphericLightTransmittance: { type: 'boolean', description: 'Per-sample atmospheric light transmittance' },
        skyLightCloudBottomOcclusion: { type: 'number', description: 'Sky light occlusion at cloud bottom (0.0 - 1.0)' },
        viewSampleCountScale: { type: 'number', description: 'Sample count scale for primary view' },
        reflectionViewSampleCountScale: { type: 'number', description: 'Sample count scale for reflections' },
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
    name: 'manage_sequence',
    category: 'utility',
    description: 'Sequencer, cinematics, and Movie Render Queue. Create/edit Level Sequences, bind actors, keyframes, camera cuts, render jobs.',
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
            'add_console_variable', 'remove_console_variable', 'configure_high_res_settings', 'set_tile_count'
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
        save: commonSchemas.save
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
  {
    name: 'manage_input',
    category: 'utility',
    description: 'Create Input Actions and Mapping Contexts. Add key/gamepad bindings with modifiers and triggers.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'create_input_action',
            'create_input_mapping_context',
            'add_mapping',
            'remove_mapping'
          ],
          description: 'Action to perform'
        },
        name: commonSchemas.name,
        path: commonSchemas.directoryPath,
        contextPath: commonSchemas.assetPath,
        actionPath: commonSchemas.assetPath,
        key: commonSchemas.stringProp
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
    name: 'inspect',
    category: 'core',
    description: 'Inspect any UObject: read/write properties, list components, export snapshots, and query class info.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'inspect_object', 'set_property', 'get_property', 'get_components', 'inspect_class', 'list_objects',
            'get_component_property', 'set_component_property', 'get_metadata', 'add_tag', 'find_by_tag',
            'create_snapshot', 'restore_snapshot', 'export', 'delete_object', 'find_by_class', 'get_bounding_box'
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
  {
    name: 'manage_behavior_tree',
    category: 'utility',
    description: 'Create Behavior Trees, add task/decorator/service nodes, and configure node properties.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: ['create', 'add_node', 'connect_nodes', 'remove_node', 'break_connections', 'set_node_properties'],
          description: 'Action'
        },
        name: commonSchemas.name,
        savePath: commonSchemas.savePath,
        assetPath: commonSchemas.assetPath,
        nodeType: commonSchemas.stringProp,
        nodeId: commonSchemas.nodeId,
        parentNodeId: commonSchemas.nodeId,
        childNodeId: commonSchemas.nodeId,
        x: commonSchemas.numberProp,
        y: commonSchemas.numberProp,
        comment: commonSchemas.stringProp,
        properties: commonSchemas.objectProp
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        ...commonSchemas.outputWithNodeId
      }
    }
  },
  // [MERGED] manage_blueprint_graph actions now in manage_blueprint (Phase 53: Strategic Tool Merging)
  {
    name: 'manage_lighting',
    category: 'world',
    description: 'Spawn lights (point, spot, rect, sky), configure GI, shadows, volumetric fog, and build lighting.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'spawn_light', 'create_light', 'spawn_sky_light', 'create_sky_light', 'ensure_single_sky_light',
            'create_lightmass_volume', 'create_lighting_enabled_level', 'create_dynamic_light',
            'setup_global_illumination', 'configure_shadows', 'set_exposure', 'set_ambient_occlusion', 'setup_volumetric_fog',
            'build_lighting', 'list_light_types'
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
        size: commonSchemas.scale
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        ...commonSchemas.outputBase,
        actorName: commonSchemas.actorName
      }
    }
  },
  {
    name: 'manage_performance',
    category: 'utility',
    description: 'Run profiling/benchmarks, configure scalability, LOD, Nanite, and optimization settings.',
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
            'get_mesh_info'
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
        enableNanite: { type: 'boolean', description: 'Enable Nanite for the output mesh.' }
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
    description: 'Edit skeletal meshes: add sockets, configure physics assets, set skin weights, and create morph targets.',
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
            'get_skeleton_info', 'list_bones', 'list_sockets', 'list_physics_bodies'
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
        overwrite: commonSchemas.overwrite
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
        boneInfo: {
          type: 'object',
          properties: {
            name: commonSchemas.stringProp,
            index: commonSchemas.numberProp,
            parentName: commonSchemas.stringProp,
            parentIndex: commonSchemas.numberProp
          }
        },
        bones: commonSchemas.arrayOfObjects,
        sockets: commonSchemas.arrayOfObjects,
        physicsBodies: commonSchemas.arrayOfObjects,
        error: commonSchemas.stringProp
      }
    }
  },
  {
    name: 'manage_material_authoring',
    category: 'authoring',
    description: 'Create materials with expressions, parameters, functions, instances, and landscape blend layers.',
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
            'compile_material', 'get_material_info'
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
        save: commonSchemas.save
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        ...commonSchemas.outputBase,
        assetPath: commonSchemas.assetPath,
        nodeId: commonSchemas.nodeId,
        materialInfo: {
          type: 'object',
          properties: {
            domain: commonSchemas.stringProp,
            blendMode: commonSchemas.stringProp,
            shadingModel: commonSchemas.stringProp,
            twoSided: commonSchemas.booleanProp,
            nodeCount: commonSchemas.numberProp,
            parameters: commonSchemas.arrayOfObjects
          }
        },
        error: commonSchemas.stringProp
      }
    }
  },
  {
    name: 'manage_texture',
    category: 'authoring',
    description: 'Create procedural textures, process images, bake normal/AO maps, and set compression settings.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'create_noise_texture', 'create_gradient_texture', 'create_pattern_texture',
            'create_normal_from_height', 'create_ao_from_mesh',
            'resize_texture', 'adjust_levels', 'adjust_curves', 'blur', 'sharpen',
            'invert', 'desaturate', 'channel_pack', 'channel_extract', 'combine_textures',
            'set_compression_settings', 'set_texture_group', 'set_lod_bias',
            'configure_virtual_texture', 'set_streaming_priority',
            'get_texture_info'
          ],
          description: 'Texture action to perform'
        },
        assetPath: commonSchemas.assetPath,
        name: commonSchemas.name,
        path: commonSchemas.directoryPathForCreation,
        width: commonSchemas.width,
        height: commonSchemas.height,
        newWidth: { type: 'number', description: 'New width for resize operation.' },
        newHeight: { type: 'number', description: 'New height for resize operation.' },
        noiseType: { type: 'string', enum: ['Perlin', 'Simplex', 'Worley', 'Voronoi'], description: 'Type of noise to generate.' },
        scale: { type: 'number', description: 'Noise scale/frequency.' },
        octaves: { type: 'number', description: 'Number of noise octaves for FBM.' },
        persistence: { type: 'number', description: 'Amplitude falloff per octave.' },
        lacunarity: { type: 'number', description: 'Frequency multiplier per octave.' },
        seed: { type: 'number', description: 'Random seed for procedural generation.' },
        seamless: { type: 'boolean', description: 'Generate seamless/tileable texture.' },
        gradientType: { type: 'string', enum: ['Linear', 'Radial', 'Angular'], description: 'Type of gradient.' },
        startColor: { type: 'object', description: 'Start color {r, g, b, a}.' },
        endColor: { type: 'object', description: 'End color {r, g, b, a}.' },
        angle: commonSchemas.angle,
        centerX: { type: 'number', description: 'Center X position (0-1) for radial gradient.' },
        centerY: { type: 'number', description: 'Center Y position (0-1) for radial gradient.' },
        radius: commonSchemas.radius,
        colorStops: { type: 'array', items: commonSchemas.objectProp, description: 'Array of {position, color} for multi-color gradients.' },
        patternType: { type: 'string', enum: ['Checker', 'Grid', 'Brick', 'Tile', 'Dots', 'Stripes'], description: 'Type of pattern.' },
        primaryColor: { type: 'object', description: 'Primary pattern color {r, g, b, a}.' },
        secondaryColor: { type: 'object', description: 'Secondary pattern color {r, g, b, a}.' },
        tilesX: { type: 'number', description: 'Number of pattern tiles horizontally.' },
        tilesY: { type: 'number', description: 'Number of pattern tiles vertically.' },
        lineWidth: { type: 'number', description: 'Line width for grid/stripes (0-1).' },
        brickRatio: { type: 'number', description: 'Width/height ratio for brick pattern.' },
        offset: { type: 'number', description: 'Brick offset ratio (0-1).' },
        sourceTexture: { type: 'string', description: 'Source height map texture path.' },
        strength: commonSchemas.strength,
        algorithm: { type: 'string', enum: ['Sobel', 'Prewitt', 'Scharr'], description: 'Normal calculation algorithm.' },
        flipY: { type: 'boolean', description: 'Flip green channel for DirectX/OpenGL compatibility.' },
        meshPath: commonSchemas.meshPath,
        samples: { type: 'number', description: 'Number of AO samples.' },
        rayDistance: { type: 'number', description: 'Maximum ray distance for AO.' },
        bias: { type: 'number', description: 'AO bias to prevent self-occlusion.' },
        uvChannel: { type: 'number', description: 'UV channel to use for baking.' },
        filterMethod: { type: 'string', enum: ['Nearest', 'Bilinear', 'Bicubic', 'Lanczos'], description: 'Resize filter method.' },
        preserveAspect: { type: 'boolean', description: 'Preserve aspect ratio when resizing.' },
        outputPath: commonSchemas.outputPath,
        inputBlackPoint: { type: 'number', description: 'Input black point (0-1).' },
        inputWhitePoint: { type: 'number', description: 'Input white point (0-1).' },
        gamma: { type: 'number', description: 'Gamma correction value.' },
        outputBlackPoint: { type: 'number', description: 'Output black point (0-1).' },
        outputWhitePoint: { type: 'number', description: 'Output white point (0-1).' },
        curvePoints: { type: 'array', items: commonSchemas.objectProp, description: 'Array of {x, y} curve control points.' },
        blurType: { type: 'string', enum: ['Gaussian', 'Box', 'Radial'], description: 'Type of blur.' },
        sharpenType: { type: 'string', enum: ['UnsharpMask', 'Laplacian'], description: 'Type of sharpening.' },
        channel: { type: 'string', enum: ['All', 'Red', 'Green', 'Blue', 'Alpha'], description: 'Target channel.' },
        invertAlpha: { type: 'boolean', description: 'Whether to invert alpha channel.' },
        amount: { type: 'number', description: 'Effect amount (0-1 for desaturate).' },
        method: { type: 'string', enum: ['Luminance', 'Average', 'Lightness'], description: 'Desaturation method.' },
        outputAsGrayscale: { type: 'boolean', description: 'Output extracted channel as grayscale.' },
        redChannel: { type: 'string', description: 'Source texture for red channel.' },
        greenChannel: { type: 'string', description: 'Source texture for green channel.' },
        blueChannel: { type: 'string', description: 'Source texture for blue channel.' },
        alphaChannel: { type: 'string', description: 'Source texture for alpha channel.' },
        redSourceChannel: { type: 'string', enum: ['Red', 'Green', 'Blue', 'Alpha'], description: 'Which channel to use from red source.' },
        greenSourceChannel: { type: 'string', enum: ['Red', 'Green', 'Blue', 'Alpha'], description: 'Which channel to use from green source.' },
        blueSourceChannel: { type: 'string', enum: ['Red', 'Green', 'Blue', 'Alpha'], description: 'Which channel to use from blue source.' },
        alphaSourceChannel: { type: 'string', enum: ['Red', 'Green', 'Blue', 'Alpha'], description: 'Which channel to use from alpha source.' },
        baseTexture: { type: 'string', description: 'Base texture path for combining.' },
        blendTexture: { type: 'string', description: 'Blend texture path for combining.' },
        blendMode: { type: 'string', enum: ['Multiply', 'Add', 'Subtract', 'Screen', 'Overlay', 'SoftLight', 'HardLight', 'Difference', 'Normal'], description: 'Blend mode for combining textures.' },
        opacity: { type: 'number', description: 'Blend opacity (0-1).' },
        maskTexture: { type: 'string', description: 'Optional mask texture for blending.' },
        compressionSettings: {
          type: 'string',
          enum: ['TC_Default', 'TC_Normalmap', 'TC_Masks', 'TC_Grayscale', 'TC_Displacementmap',
                 'TC_VectorDisplacementmap', 'TC_HDR', 'TC_EditorIcon', 'TC_Alpha',
                 'TC_DistanceFieldFont', 'TC_HDR_Compressed', 'TC_BC7'],
          description: 'Texture compression setting.'
        },
        textureGroup: {
          type: 'string',
          description: 'Texture group (TEXTUREGROUP_World, TEXTUREGROUP_Character, TEXTUREGROUP_UI, etc.).'
        },
        lodBias: { type: 'number', description: 'LOD bias (-2 to 4, lower = higher quality).' },
        virtualTextureStreaming: { type: 'boolean', description: 'Enable virtual texture streaming.' },
        tileSize: { type: 'number', description: 'Virtual texture tile size (32, 64, 128, 256, 512, 1024).' },
        tileBorderSize: { type: 'number', description: 'Virtual texture tile border size.' },
        neverStream: { type: 'boolean', description: 'Disable texture streaming.' },
        streamingPriority: { type: 'number', description: 'Streaming priority (-1 to 1, lower = higher priority).' },
        hdr: { type: 'boolean', description: 'Create HDR texture (16-bit float).' },
        save: commonSchemas.save
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        ...commonSchemas.outputBase,
        assetPath: commonSchemas.assetPath,
        textureInfo: {
          type: 'object',
          properties: {
            width: commonSchemas.numberProp,
            height: commonSchemas.numberProp,
            format: commonSchemas.stringProp,
            compression: commonSchemas.stringProp,
            textureGroup: commonSchemas.stringProp,
            mipCount: commonSchemas.numberProp,
            sRGB: commonSchemas.booleanProp,
            hasAlpha: commonSchemas.booleanProp,
            virtualTextureStreaming: commonSchemas.booleanProp,
            neverStream: commonSchemas.booleanProp
          }
        },
        error: commonSchemas.stringProp
      }
    }
  },
  // [MERGED] manage_animation_authoring actions now in animation_physics (Phase 53: Strategic Tool Merging)
  // [MERGED] manage_audio_authoring actions now in manage_audio (Phase 53: Strategic Tool Merging)
  // [MERGED] manage_niagara_authoring actions now in manage_effect (Phase 53: Strategic Tool Merging)
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
        save: commonSchemas.save,
        replicationMode: {
          type: 'string',
          enum: ['Full', 'Minimal', 'Mixed'],
          description: 'ASC replication mode.'
        },
        ownerActor: { type: 'string', description: 'Owner actor class for ASC.' },
        avatarActor: { type: 'string', description: 'Avatar actor class for ASC.' },
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
        parentClass: commonSchemas.parentClass,
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
        costAttribute: { type: 'string', description: 'Attribute used for cost (e.g., Mana).' },
        costMagnitude: { type: 'number', description: 'Cost magnitude.' },
        cooldownEffectPath: { type: 'string', description: 'Path to cooldown Gameplay Effect.' },
        cooldownDuration: { type: 'number', description: 'Cooldown duration in seconds.' },
        cooldownTags: {
          type: 'array',
          items: commonSchemas.stringProp,
          description: 'Tags applied during cooldown.'
        },
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
        taskSettings: {
          type: 'object',
          description: 'Task-specific settings.'
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
        coefficient: { type: 'number', description: 'Coefficient for attribute-based calculation.' },
        preMultiplyAdditiveValue: { type: 'number', description: 'Value added before multiplication.' },
        postMultiplyAdditiveValue: { type: 'number', description: 'Value added after multiplication.' },
        sourceAttribute: { type: 'string', description: 'Source attribute for attribute-based calculation.' },
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
            'get_character_info'
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
        footstepDecalPath: { type: 'string', description: 'Path to footstep decal.' }
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
            'get_combat_info'
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
    description: 'Create AI Controllers, configure Behavior Trees, Blackboards, EQS queries, and perception systems.',
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
            'get_ai_info'
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
        }
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
            'get_inventory_info'
          ],
          description: 'Inventory action to perform.'
        },
        name: commonSchemas.assetNameForCreation,
        path: commonSchemas.directoryPathForCreation,
        folder: commonSchemas.directoryPath,
        save: commonSchemas.save,
        blueprintPath: commonSchemas.blueprintPath,
        itemPath: commonSchemas.itemDataPath,
        parentClass: commonSchemas.parentClass,
        displayName: commonSchemas.stringProp,
        description: commonSchemas.stringProp,
        icon: commonSchemas.iconPath,
        mesh: commonSchemas.meshAssetPath,
        stackSize: commonSchemas.numberProp,
        weight: commonSchemas.numberProp,
        rarity: {
          type: 'string',
          enum: ['Common', 'Uncommon', 'Rare', 'Epic', 'Legendary', 'Custom'],
          description: 'Item rarity tier.'
        },
        value: commonSchemas.numberProp,
        tags: {
          type: 'array',
          items: commonSchemas.stringProp,
          description: 'Gameplay tags for item categorization.'
        },
        customProperties: {
          type: 'object',
          additionalProperties: true,
          description: 'Custom key-value properties for item.'
        },
        categoryPath: { type: 'string', description: 'Path to item category asset.' },
        parentCategory: { type: 'string', description: 'Parent category path.' },
        categoryIcon: { type: 'string', description: 'Icon texture for category.' },
        componentName: commonSchemas.componentName,
        slotCount: commonSchemas.numberProp,
        slotSize: {
          type: 'object',
          properties: { width: commonSchemas.numberProp, height: commonSchemas.numberProp },
          description: 'Size of each slot (for grid inventory).'
        },
        maxWeight: commonSchemas.numberProp,
        allowStacking: { type: 'boolean', description: 'Allow items to stack.' },
        slotCategories: {
          type: 'array',
          items: commonSchemas.stringProp,
          description: 'Allowed item categories per slot.'
        },
        slotRestrictions: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              slotIndex: commonSchemas.numberProp,
              allowedCategories: commonSchemas.arrayOfStrings,
              restrictedCategories: commonSchemas.arrayOfStrings
            }
          },
          description: 'Per-slot category restrictions.'
        },
        replicated: commonSchemas.replicated,
        replicationCondition: {
          type: 'string',
          enum: ['None', 'OwnerOnly', 'SkipOwner', 'SimulatedOnly', 'AutonomousOnly', 'Custom'],
          description: 'Replication condition for inventory.'
        },
        pickupPath: { type: 'string', description: 'Path to pickup actor Blueprint.' },
        meshPath: commonSchemas.meshPath,
        itemDataPath: commonSchemas.itemDataPath,
        interactionRadius: { type: 'number', description: 'Radius for pickup interaction.' },
        interactionType: {
          type: 'string',
          enum: ['Overlap', 'Interact', 'Key', 'Hold'],
          description: 'How player picks up item.'
        },
        interactionKey: { type: 'string', description: 'Input action for pickup (if type is Key/Hold).' },
        prompt: commonSchemas.prompt,
        highlightMaterial: { type: 'string', description: 'Material for highlight effect.' },
        respawnable: commonSchemas.booleanProp,
        respawnTime: commonSchemas.respawnTime,
        respawnEffect: { type: 'string', description: 'Niagara effect for respawn.' },
        pickupSound: { type: 'string', description: 'Sound cue for pickup.' },
        pickupParticle: { type: 'string', description: 'Particle effect on pickup.' },
        bobbing: { type: 'boolean', description: 'Enable bobbing animation.' },
        rotation: { type: 'boolean', description: 'Enable rotation animation.' },
        glowEffect: { type: 'boolean', description: 'Enable glow effect.' },
        slots: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              name: commonSchemas.stringProp,
              socketName: commonSchemas.stringProp,
              allowedCategories: commonSchemas.arrayOfStrings,
              restrictedCategories: commonSchemas.arrayOfStrings
            }
          },
          description: 'Equipment slot definitions.'
        },
        statModifiers: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              attribute: commonSchemas.stringProp,
              operation: { type: 'string', enum: ['Add', 'Multiply', 'Override'] },
              value: commonSchemas.numberProp
            }
          },
          description: 'Stat modifiers when equipped.'
        },
        abilityGrants: {
          type: 'array',
          items: commonSchemas.stringProp,
          description: 'Gameplay abilities granted when equipped.'
        },
        passiveEffects: {
          type: 'array',
          items: commonSchemas.stringProp,
          description: 'Passive gameplay effects when equipped.'
        },
        attachToSocket: { type: 'boolean', description: 'Attach mesh to socket when equipped.' },
        meshComponent: { type: 'string', description: 'Component name for equipment mesh.' },
        animationOverrides: {
          type: 'object',
          additionalProperties: commonSchemas.stringProp,
          description: 'Animation overrides (slot -> anim asset).'
        },
        lootTablePath: commonSchemas.lootTablePath,
        lootWeight: { type: 'number', description: 'Weight for drop chance calculation.' },
        minQuantity: { type: 'number', description: 'Minimum drop quantity.' },
        maxQuantity: { type: 'number', description: 'Maximum drop quantity.' },
        conditions: {
          type: 'array',
          items: commonSchemas.stringProp,
          description: 'Conditions for loot entry (gameplay tag expressions).'
        },
        actorPath: { type: 'string', description: 'Path to actor Blueprint for loot drop.' },
        dropCount: { type: 'number', description: 'Number of drops to roll.' },
        guaranteedDrops: {
          type: 'array',
          items: commonSchemas.stringProp,
          description: 'Item paths that always drop.'
        },
        dropRadius: { type: 'number', description: 'Radius for scattered drops.' },
        dropForce: { type: 'number', description: 'Physics force applied to drops.' },
        tiers: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              name: commonSchemas.stringProp,
              color: {
                type: 'object',
                properties: commonSchemas.colorObject.properties
              },
              dropWeight: commonSchemas.numberProp,
              statMultiplier: commonSchemas.numberProp
            }
          },
          description: 'Quality tier definitions.'
        },
        recipePath: commonSchemas.recipePath,
        outputItemPath: { type: 'string', description: 'Path to item produced by recipe.' },
        outputQuantity: { type: 'number', description: 'Quantity produced.' },
        ingredients: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              itemPath: commonSchemas.stringProp,
              quantity: commonSchemas.numberProp
            }
          },
          description: 'Required ingredients with quantities.'
        },
        craftTime: { type: 'number', description: 'Time in seconds to craft.' },
        requiredLevel: { type: 'number', description: 'Required player level.' },
        requiredSkills: {
          type: 'array',
          items: commonSchemas.stringProp,
          description: 'Required skill tags.'
        },
        requiredStation: { type: 'string', description: 'Required crafting station type.' },
        unlockConditions: {
          type: 'array',
          items: commonSchemas.stringProp,
          description: 'Conditions to unlock recipe.'
        },
        recipes: {
          type: 'array',
          items: commonSchemas.stringProp,
          description: 'Recipe paths for crafting station.'
        },
        stationType: { type: 'string', description: 'Type of crafting station.' }
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
        traceChannel: commonSchemas.traceChannel,
        traceDistance: commonSchemas.traceDistance,
        traceRadius: commonSchemas.traceRadius,
        traceFrequency: commonSchemas.traceFrequency,
        widgetClass: commonSchemas.widgetClass,
        widgetOffset: {
          type: 'object',
          properties: commonSchemas.vector3.properties,
          description: 'Widget offset from actor.'
        },
        showOnHover: { type: 'boolean', description: 'Show widget when hovering.' },
        showPromptText: { type: 'boolean', description: 'Show interaction prompt text.' },
        promptTextFormat: { type: 'string', description: 'Format string for prompt (e.g., "Press {Key} to {Action}").' },
        doorPath: { type: 'string', description: 'Path to door actor blueprint.' },
        meshPath: commonSchemas.meshPath,
        openAngle: { type: 'number', description: 'Door open rotation angle in degrees.' },
        openTime: { type: 'number', description: 'Time to open/close door in seconds.' },
        openDirection: { type: 'string', enum: ['push', 'pull', 'auto'], description: 'Door open direction.' },
        pivotOffset: {
          type: 'object',
          properties: commonSchemas.vector3.properties,
          description: 'Offset for door pivot point.'
        },
        locked: commonSchemas.locked,
        keyItemPath: { type: 'string', description: 'Item required to unlock.' },
        openSound: { type: 'string', description: 'Sound to play on open.' },
        closeSound: { type: 'string', description: 'Sound to play on close.' },
        autoClose: { type: 'boolean', description: 'Automatically close after opening.' },
        autoCloseDelay: { type: 'number', description: 'Delay before auto-close in seconds.' },
        requiresKey: { type: 'boolean', description: 'Whether interaction requires a key item.' },
        switchPath: { type: 'string', description: 'Path to switch actor blueprint.' },
        switchType: { type: 'string', enum: ['button', 'lever', 'pressure_plate', 'toggle'], description: 'Type of switch.' },
        toggleable: { type: 'boolean', description: 'Whether switch can be toggled.' },
        oneShot: { type: 'boolean', description: 'Whether switch can only be used once.' },
        resetTime: { type: 'number', description: 'Time to reset switch in seconds.' },
        activateSound: { type: 'string', description: 'Sound on activation.' },
        deactivateSound: { type: 'string', description: 'Sound on deactivation.' },
        targetActors: {
          type: 'array',
          items: commonSchemas.stringProp,
          description: 'Actors affected by this switch.'
        },
        chestPath: { type: 'string', description: 'Path to chest actor blueprint.' },
        lidMeshPath: { type: 'string', description: 'Path to lid mesh.' },
        lootTablePath: commonSchemas.lootTablePath,
        respawnable: commonSchemas.booleanProp,
        respawnTime: commonSchemas.respawnTime,
        leverType: { type: 'string', enum: ['rotate', 'translate'], description: 'Lever movement type.' },
        moveDistance: { type: 'number', description: 'Distance for translation lever.' },
        moveTime: { type: 'number', description: 'Time for lever movement.' },
        fractureMode: { type: 'string', enum: ['voronoi', 'uniform', 'radial', 'custom'], description: 'Fracture pattern type.' },
        fracturePieces: { type: 'number', description: 'Number of fracture pieces.' },
        enablePhysics: { type: 'boolean', description: 'Enable physics on destruction.' },
        levels: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              damageThreshold: commonSchemas.numberProp,
              meshIndex: commonSchemas.numberProp,
              enablePhysics: commonSchemas.booleanProp,
              removeAfterTime: commonSchemas.numberProp
            }
          },
          description: 'Destruction level definitions.'
        },
        destroySound: { type: 'string', description: 'Sound on destruction.' },
        destroyParticle: { type: 'string', description: 'Particle effect on destruction.' },
        debrisPhysicsMaterial: { type: 'string', description: 'Physics material for debris.' },
        debrisLifetime: { type: 'number', description: 'Debris lifetime in seconds.' },
        maxHealth: { type: 'number', description: 'Maximum health before destruction.' },
        damageThresholds: {
          type: 'array',
          items: commonSchemas.numberProp,
          description: 'Damage thresholds for destruction levels.'
        },
        impactDamageMultiplier: { type: 'number', description: 'Multiplier for impact damage.' },
        radialDamageMultiplier: { type: 'number', description: 'Multiplier for radial damage.' },
        autoDestroy: { type: 'boolean', description: 'Automatically destroy at zero health.' },
        triggerPath: { type: 'string', description: 'Path to trigger actor blueprint.' },
        triggerShape: { type: 'string', enum: ['box', 'sphere', 'capsule'], description: 'Shape of trigger volume.' },
        size: {
          type: 'object',
          properties: commonSchemas.vector3.properties,
          description: 'Size of trigger volume.'
        },
        filterByTag: { type: 'string', description: 'Actor tag filter for trigger.' },
        filterByClass: { type: 'string', description: 'Actor class filter for trigger.' },
        filterByInterface: { type: 'string', description: 'Interface filter for trigger.' },
        ignoreClasses: {
          type: 'array',
          items: commonSchemas.stringProp,
          description: 'Classes to ignore in trigger.'
        },
        ignoreTags: {
          type: 'array',
          items: commonSchemas.stringProp,
          description: 'Tags to ignore in trigger.'
        },
        onEnterEvent: { type: 'string', description: 'Event dispatcher name for enter.' },
        onExitEvent: { type: 'string', description: 'Event dispatcher name for exit.' },
        onStayEvent: { type: 'string', description: 'Event dispatcher name for stay.' },
        stayInterval: { type: 'number', description: 'Interval for stay events in seconds.' },
        responseType: { type: 'string', enum: ['once', 'repeatable', 'toggle'], description: 'How trigger responds.' },
        cooldown: commonSchemas.cooldown,
        maxActivations: { type: 'number', description: 'Maximum number of activations (0 = unlimited).' }
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
    name: 'manage_widget_authoring',
    category: 'utility',
    description: 'Create UMG widgets: buttons, text, images, sliders. Configure layouts, bindings, animations. Build HUDs and menus.',
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
            'preview_widget'
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
        customHeight: { type: 'number', description: 'Custom preview height.' }
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
    description: 'Configure multiplayer: property replication, RPCs (Server/Client/Multicast), authority, relevancy, and network prediction.',
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
        save: commonSchemas.save
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
    name: 'manage_game_framework',
    category: 'utility',
    description: 'Create GameMode, GameState, PlayerController, PlayerState Blueprints. Configure match flow, teams, scoring, and spawning.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'create_game_mode', 'create_game_state', 'create_player_controller',
            'create_player_state', 'create_game_instance', 'create_hud_class',
            'set_default_pawn_class', 'set_player_controller_class',
            'set_game_state_class', 'set_player_state_class', 'configure_game_rules',
            'setup_match_states', 'configure_round_system', 'configure_team_system',
            'configure_scoring_system', 'configure_spawn_system',
            'configure_player_start', 'set_respawn_rules', 'configure_spectating',
            'get_game_framework_info'
          ],
          description: 'Game framework action to perform.'
        },
        name: commonSchemas.name,
        path: commonSchemas.directoryPathForCreation,
        gameModeBlueprint: { type: 'string', description: 'Path to GameMode blueprint to configure.' },
        blueprintPath: commonSchemas.blueprintPath,
        levelPath: commonSchemas.levelPath,
        parentClass: commonSchemas.parentClass,
        pawnClass: { type: 'string', description: 'Pawn class to use.' },
        defaultPawnClass: { type: 'string', description: 'Default pawn class for GameMode.' },
        playerControllerClass: { type: 'string', description: 'PlayerController class path.' },
        gameStateClass: { type: 'string', description: 'GameState class path.' },
        playerStateClass: { type: 'string', description: 'PlayerState class path.' },
        spectatorClass: { type: 'string', description: 'Spectator pawn class.' },
        hudClass: { type: 'string', description: 'HUD class path.' },
        timeLimit: commonSchemas.numberProp,
        scoreLimit: commonSchemas.numberProp,
        bDelayedStart: { type: 'boolean', description: 'Whether to delay match start.' },
        startPlayersNeeded: commonSchemas.numberProp,
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
        respawnConditions: {
          type: 'array',
          items: commonSchemas.stringProp,
          description: 'Conditions for respawn (e.g., "RoundEnd", "Manual").'
        },
        usePlayerStarts: { type: 'boolean', description: 'Use PlayerStart actors.' },
        location: {
          type: 'object',
          properties: commonSchemas.vector3.properties,
          description: 'Spawn location.'
        },
        rotation: {
          type: 'object',
          properties: commonSchemas.rotation.properties,
          description: 'Spawn rotation.'
        },
        bPlayerOnly: { type: 'boolean', description: 'Restrict to players only.' },
        allowSpectating: { type: 'boolean', description: 'Allow spectator mode.' },
        spectatorViewMode: {
          type: 'string',
          enum: ['FreeCam', 'ThirdPerson', 'FirstPerson', 'DeathCam'],
          description: 'Spectator view mode.'
        },
        save: commonSchemas.save
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: commonSchemas.booleanProp,
        message: commonSchemas.stringProp,
        blueprintPath: commonSchemas.blueprintPath,
        gameFrameworkInfo: {
          type: 'object',
          properties: {
            gameModeClass: commonSchemas.stringProp,
            gameStateClass: commonSchemas.stringProp,
            playerControllerClass: commonSchemas.stringProp,
            playerStateClass: commonSchemas.stringProp,
            defaultPawnClass: commonSchemas.stringProp,
            hudClass: commonSchemas.stringProp,
            spectatorClass: commonSchemas.stringProp,
            matchState: commonSchemas.stringProp,
            numTeams: commonSchemas.numberProp,
            timeLimit: commonSchemas.numberProp,
            scoreLimit: commonSchemas.numberProp
          },
          description: 'Game framework information (for get_game_framework_info).'
        },
        error: commonSchemas.stringProp
      }
    }
  },
  {
    name: 'manage_sessions',
    category: 'utility',
    description: 'Configure local multiplayer: split-screen layouts, LAN hosting/joining, voice chat channels, and push-to-talk.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'configure_local_session_settings', 'configure_session_interface',
            'configure_split_screen', 'set_split_screen_type', 'add_local_player', 'remove_local_player',
            'configure_lan_play', 'host_lan_server', 'join_lan_server',
            'enable_voice_chat', 'configure_voice_settings', 'set_voice_channel',
            'mute_player', 'set_voice_attenuation', 'configure_push_to_talk',
            'get_sessions_info'
          ],
          description: 'Sessions action to perform.'
        },
        sessionName: commonSchemas.sessionName,
        sessionId: commonSchemas.sessionId,
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
          description: 'Type of session interface to use.'
        },
        enabled: commonSchemas.enabled,
        splitScreenType: {
          type: 'string',
          enum: ['None', 'TwoPlayer_Horizontal', 'TwoPlayer_Vertical', 'ThreePlayer_FavorTop', 'ThreePlayer_FavorBottom', 'FourPlayer_Grid'],
          description: 'Split-screen layout type.'
        },
        playerIndex: commonSchemas.numberProp,
        controllerId: commonSchemas.numberProp,
        serverAddress: commonSchemas.serverAddress,
        serverPort: commonSchemas.numberProp,
        serverPassword: { type: 'string', description: 'Server password for protected games.' },
        serverName: { type: 'string', description: 'Display name for the server.' },
        mapName: { type: 'string', description: 'Map to load for hosting.' },
        travelOptions: { type: 'string', description: 'Travel URL options string.' },
        voiceEnabled: { type: 'boolean', description: 'Enable/disable voice chat.' },
        voiceSettings: {
          type: 'object',
          properties: {
            volume: { type: 'number', description: 'Voice volume (0.0 - 1.0).' },
            noiseGateThreshold: { type: 'number', description: 'Noise gate threshold.' },
            noiseSuppression: { type: 'boolean', description: 'Enable noise suppression.' },
            echoCancellation: { type: 'boolean', description: 'Enable echo cancellation.' },
            sampleRate: { type: 'number', description: 'Audio sample rate in Hz.' }
          },
          description: 'Voice processing settings.'
        },
        channelName: commonSchemas.channelName,
        channelType: {
          type: 'string',
          enum: ['Team', 'Global', 'Proximity', 'Party'],
          description: 'Voice channel type.'
        },
        playerName: { type: 'string', description: 'Player name for voice operations.' },
        targetPlayerId: { type: 'string', description: 'Target player ID.' },
        muted: commonSchemas.muted,
        attenuationRadius: { type: 'number', description: 'Radius for voice attenuation (Proximity chat).' },
        attenuationFalloff: { type: 'number', description: 'Falloff rate for voice attenuation.' },
        pushToTalkEnabled: { type: 'boolean', description: 'Enable push-to-talk mode.' },
        pushToTalkKey: { type: 'string', description: 'Key binding for push-to-talk.' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: commonSchemas.booleanProp,
        message: commonSchemas.stringProp,
        sessionId: commonSchemas.sessionId,
        sessionName: { type: 'string', description: 'Name of created session.' },
        playerIndex: { type: 'number', description: 'Index of added local player.' },
        serverAddress: commonSchemas.serverAddress,
        channelName: { type: 'string', description: 'Voice channel name.' },
        sessionsInfo: {
          type: 'object',
          properties: {
            currentSessionName: commonSchemas.stringProp,
            isLANMatch: commonSchemas.booleanProp,
            maxPlayers: commonSchemas.numberProp,
            currentPlayers: commonSchemas.numberProp,
            localPlayerCount: commonSchemas.numberProp,
            splitScreenEnabled: commonSchemas.booleanProp,
            splitScreenType: commonSchemas.stringProp,
            voiceChatEnabled: commonSchemas.booleanProp,
            activeVoiceChannels: {
              type: 'array',
              items: commonSchemas.stringProp
            },
            isHosting: commonSchemas.booleanProp,
            connectedServerAddress: commonSchemas.stringProp
          },
          description: 'Sessions information (for get_sessions_info).'
        },
        error: commonSchemas.stringProp
      }
    }
  },
  {
    name: 'manage_level_structure',
    category: 'world',
    description: 'Create levels and sublevels. Configure World Partition, streaming, data layers, HLOD, and level instances.',
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
          ],
          description: 'Level structure action to perform.'
        },
        levelName: commonSchemas.stringProp,
        levelPath: commonSchemas.levelPath,
        parentLevel: commonSchemas.parentLevel,
        templateLevel: commonSchemas.templateLevel,
        bCreateWorldPartition: { type: 'boolean', description: 'Create with World Partition enabled.' },
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
        dataLayerLabel: { type: 'string', description: 'Display label for the data layer.' },
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
  {
    name: 'manage_volumes',
    category: 'world',
    description: 'Create trigger volumes, blocking volumes, physics volumes, audio volumes, and navigation bounds.',
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
            'get_volumes_info'
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
        save: commonSchemas.save
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
        volumesInfo: {
          type: 'object',
          properties: {
            totalCount: commonSchemas.numberProp,
            volumes: {
              type: 'array',
              items: {
                type: 'object',
                properties: {
                  name: commonSchemas.stringProp,
                  class: commonSchemas.stringProp,
                  location: {
                    type: 'object',
                    properties: {
                      x: commonSchemas.numberProp, y: commonSchemas.numberProp, z: commonSchemas.numberProp
                    }
                  },
                  extent: {
                    type: 'object',
                    properties: {
                      x: commonSchemas.numberProp, y: commonSchemas.numberProp, z: commonSchemas.numberProp
                    }
                  }
                }
              }
            }
          },
          description: 'Volume information (for get_volumes_info).'
        },
        error: commonSchemas.stringProp
      }
    }
  },
  {
    name: 'manage_navigation',
    category: 'gameplay',
    description: 'Configure NavMesh settings, add nav modifiers, create nav links and smart links for pathfinding.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'configure_nav_mesh_settings', 'set_nav_agent_properties', 'rebuild_navigation',
            'create_nav_modifier_component', 'set_nav_area_class', 'configure_nav_area_cost',
            'create_nav_link_proxy', 'configure_nav_link', 'set_nav_link_type',
            'create_smart_link', 'configure_smart_link_behavior',
            'get_navigation_info'
          ],
          description: 'Navigation action to perform'
        },
        navMeshPath: { type: 'string', description: 'Path to NavMesh data asset.' },
        actorName: commonSchemas.actorName,
        actorPath: commonSchemas.actorPath,
        blueprintPath: commonSchemas.blueprintPath,
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
        componentName: commonSchemas.componentName,
        areaClass: commonSchemas.areaClass,
        areaClassToReplace: { type: 'string', description: 'Area class to replace (optional modifier behavior).' },
        failsafeExtent: {
          type: 'object',
          properties: {
            x: commonSchemas.numberProp, y: commonSchemas.numberProp, z: commonSchemas.numberProp
          },
          description: 'Failsafe extent for nav modifier when actor has no collision.'
        },
        bIncludeAgentHeight: { type: 'boolean', description: 'Expand lower bounds by agent height.' },
        areaCost: { type: 'number', description: 'Pathfinding cost multiplier for area (1.0 = normal).' },
        fixedAreaEnteringCost: { type: 'number', description: 'Fixed cost added when entering the area.' },
        linkName: commonSchemas.linkName,
        startPoint: {
          type: 'object',
          properties: {
            x: commonSchemas.numberProp, y: commonSchemas.numberProp, z: commonSchemas.numberProp
          },
          description: 'Start point of navigation link (relative to actor).'
        },
        endPoint: {
          type: 'object',
          properties: {
            x: commonSchemas.numberProp, y: commonSchemas.numberProp, z: commonSchemas.numberProp
          },
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
        bSmartLinkIsRelevant: { type: 'boolean', description: 'Toggle smart link relevancy.' },
        enabledAreaClass: { type: 'string', description: 'Area class when smart link is enabled.' },
        disabledAreaClass: { type: 'string', description: 'Area class when smart link is disabled.' },
        broadcastRadius: { type: 'number', description: 'Radius for state change broadcast.' },
        broadcastInterval: { type: 'number', description: 'Interval for state change broadcast (0 = single).' },
        bCreateBoxObstacle: { type: 'boolean', description: 'Add box obstacle during nav generation.' },
        obstacleOffset: {
          type: 'object',
          properties: {
            x: commonSchemas.numberProp, y: commonSchemas.numberProp, z: commonSchemas.numberProp
          },
          description: 'Offset of simple box obstacle.'
        },
        obstacleExtent: {
          type: 'object',
          properties: {
            x: commonSchemas.numberProp, y: commonSchemas.numberProp, z: commonSchemas.numberProp
          },
          description: 'Extent of simple box obstacle.'
        },
        obstacleAreaClass: { type: 'string', description: 'Area class for box obstacle.' },
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
        filter: commonSchemas.filter,
        save: commonSchemas.save
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: commonSchemas.booleanProp,
        message: commonSchemas.stringProp,
        actorName: commonSchemas.actorName,
        componentName: commonSchemas.componentName,
        navMeshInfo: {
          type: 'object',
          properties: {
            agentRadius: commonSchemas.numberProp,
            agentHeight: commonSchemas.numberProp,
            agentStepHeight: commonSchemas.numberProp,
            agentMaxSlope: commonSchemas.numberProp,
            cellSize: commonSchemas.numberProp,
            cellHeight: commonSchemas.numberProp,
            tileSizeUU: commonSchemas.numberProp,
            tileCount: commonSchemas.numberProp,
            boundsVolumes: commonSchemas.numberProp,
            navLinkCount: commonSchemas.numberProp
          },
          description: 'Navigation system information.'
        },
        error: commonSchemas.stringProp
      }
    }
  },
  {
    name: 'manage_splines',
    category: 'world',
    description: 'Create spline actors, add/modify points, attach meshes along splines, and query spline data.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
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
          description: 'Spline action to perform'
        },
        actorName: commonSchemas.actorName,
        actorPath: commonSchemas.actorPath,
        splineName: { type: 'string', description: 'Name of spline component.' },
        componentName: commonSchemas.componentName,
        blueprintPath: commonSchemas.blueprintPath,
        location: {
          type: 'object',
          properties: {
            x: commonSchemas.numberProp, y: commonSchemas.numberProp, z: commonSchemas.numberProp
          },
          description: 'Location for spline actor.'
        },
        rotation: {
          type: 'object',
          properties: {
            pitch: commonSchemas.numberProp, yaw: commonSchemas.numberProp, roll: commonSchemas.numberProp
          },
          description: 'Rotation for spline actor.'
        },
        scale: {
          type: 'object',
          properties: {
            x: commonSchemas.numberProp, y: commonSchemas.numberProp, z: commonSchemas.numberProp
          },
          description: 'Scale for spline actor.'
        },
        pointIndex: { type: 'number', description: 'Index of spline point to modify.' },
        position: {
          type: 'object',
          properties: {
            x: commonSchemas.numberProp, y: commonSchemas.numberProp, z: commonSchemas.numberProp
          },
          description: 'Position for spline point.'
        },
        arriveTangent: {
          type: 'object',
          properties: {
            x: commonSchemas.numberProp, y: commonSchemas.numberProp, z: commonSchemas.numberProp
          },
          description: 'Arrive tangent for spline point (incoming direction).'
        },
        leaveTangent: {
          type: 'object',
          properties: {
            x: commonSchemas.numberProp, y: commonSchemas.numberProp, z: commonSchemas.numberProp
          },
          description: 'Leave tangent for spline point (outgoing direction).'
        },
        tangent: {
          type: 'object',
          properties: {
            x: commonSchemas.numberProp, y: commonSchemas.numberProp, z: commonSchemas.numberProp
          },
          description: 'Unified tangent (sets both arrive and leave).'
        },
        pointRotation: {
          type: 'object',
          properties: {
            pitch: commonSchemas.numberProp, yaw: commonSchemas.numberProp, roll: commonSchemas.numberProp
          },
          description: 'Rotation at spline point.'
        },
        pointScale: {
          type: 'object',
          properties: {
            x: commonSchemas.numberProp, y: commonSchemas.numberProp, z: commonSchemas.numberProp
          },
          description: 'Scale at spline point.'
        },
        coordinateSpace: {
          type: 'string',
          enum: ['Local', 'World'],
          description: 'Coordinate space for position/tangent values (default: Local).'
        },
        splineType: {
          type: 'string',
          enum: ['Linear', 'Curve', 'Constant', 'CurveClamped', 'CurveCustomTangent'],
          description: 'Type of spline interpolation.'
        },
        bClosedLoop: { type: 'boolean', description: 'Whether spline forms a closed loop.' },
        bUpdateSpline: { type: 'boolean', description: 'Update spline after modification (default: true).' },
        meshPath: commonSchemas.meshPath,
        materialPath: commonSchemas.materialPath,
        forwardAxis: {
          type: 'string',
          enum: ['X', 'Y', 'Z'],
          description: 'Forward axis for spline mesh deformation.'
        },
        startPos: {
          type: 'object',
          properties: {
            x: commonSchemas.numberProp, y: commonSchemas.numberProp, z: commonSchemas.numberProp
          },
          description: 'Start position for spline mesh segment.'
        },
        startTangent: {
          type: 'object',
          properties: {
            x: commonSchemas.numberProp, y: commonSchemas.numberProp, z: commonSchemas.numberProp
          },
          description: 'Start tangent for spline mesh segment.'
        },
        endPos: {
          type: 'object',
          properties: {
            x: commonSchemas.numberProp, y: commonSchemas.numberProp, z: commonSchemas.numberProp
          },
          description: 'End position for spline mesh segment.'
        },
        endTangent: {
          type: 'object',
          properties: {
            x: commonSchemas.numberProp, y: commonSchemas.numberProp, z: commonSchemas.numberProp
          },
          description: 'End tangent for spline mesh segment.'
        },
        startScale: {
          type: 'object',
          properties: commonSchemas.vector2.properties,
          description: 'X/Y scale at spline mesh start.'
        },
        endScale: {
          type: 'object',
          properties: commonSchemas.vector2.properties,
          description: 'X/Y scale at spline mesh end.'
        },
        startRoll: { type: 'number', description: 'Roll angle at spline mesh start (radians).' },
        endRoll: { type: 'number', description: 'Roll angle at spline mesh end (radians).' },
        bSmoothInterpRollScale: { type: 'boolean', description: 'Use smooth interpolation for roll/scale.' },
        spacing: { type: 'number', description: 'Distance between scattered meshes.' },
        startOffset: { type: 'number', description: 'Offset from spline start for first mesh.' },
        endOffset: { type: 'number', description: 'Offset from spline end for last mesh.' },
        bAlignToSpline: { type: 'boolean', description: 'Align scattered meshes to spline direction.' },
        bRandomizeRotation: { type: 'boolean', description: 'Apply random rotation to scattered meshes.' },
        rotationRandomRange: {
          type: 'object',
          properties: {
            pitch: commonSchemas.numberProp, yaw: commonSchemas.numberProp, roll: commonSchemas.numberProp
          },
          description: 'Random rotation range (degrees).'
        },
        bRandomizeScale: { type: 'boolean', description: 'Apply random scale to scattered meshes.' },
        scaleMin: { type: 'number', description: 'Minimum random scale multiplier.' },
        scaleMax: { type: 'number', description: 'Maximum random scale multiplier.' },
        randomSeed: { type: 'number', description: 'Seed for randomization (for reproducible results).' },
        templateType: {
          type: 'string',
          enum: ['road', 'river', 'fence', 'wall', 'cable', 'pipe'],
          description: 'Type of spline template to create.'
        },
        width: commonSchemas.width,
        segmentLength: { type: 'number', description: 'Length of mesh segments for deformation.' },
        postSpacing: { type: 'number', description: 'Spacing between fence posts.' },
        railHeight: { type: 'number', description: 'Height of fence rails.' },
        pipeRadius: { type: 'number', description: 'Radius for pipe template.' },
        cableSlack: { type: 'number', description: 'Slack/sag amount for cable template.' },
        points: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              position: {
                type: 'object',
                properties: commonSchemas.vector3.properties
              },
              arriveTangent: {
                type: 'object',
                properties: commonSchemas.vector3.properties
              },
              leaveTangent: {
                type: 'object',
                properties: commonSchemas.vector3.properties
              },
              rotation: {
                type: 'object',
                properties: commonSchemas.rotation.properties
              },
              scale: {
                type: 'object',
                properties: commonSchemas.vector3.properties
              },
              type: {
                type: 'string',
                enum: ['Linear', 'Curve', 'Constant', 'CurveClamped', 'CurveCustomTangent']
              }
            },
            required: ['position']
          },
          description: 'Array of spline points for batch creation.'
        },
        filter: commonSchemas.filter,
        save: commonSchemas.save
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: commonSchemas.booleanProp,
        message: commonSchemas.stringProp,
        actorName: commonSchemas.actorName,
        componentName: commonSchemas.componentName,
        pointCount: { type: 'number', description: 'Number of points in spline.' },
        splineLength: { type: 'number', description: 'Total length of spline in units.' },
        bClosedLoop: { type: 'boolean', description: 'Whether spline is a closed loop.' },
        splineInfo: {
          type: 'object',
          properties: {
            pointCount: commonSchemas.numberProp,
            splineLength: commonSchemas.numberProp,
            bClosedLoop: commonSchemas.booleanProp,
            points: {
              type: 'array',
              items: {
                type: 'object',
                properties: {
                  index: commonSchemas.numberProp,
                  position: commonSchemas.objectProp,
                  arriveTangent: commonSchemas.objectProp,
                  leaveTangent: commonSchemas.objectProp,
                  rotation: commonSchemas.objectProp,
                  scale: commonSchemas.objectProp,
                  type: commonSchemas.stringProp
                }
              }
            }
          },
          description: 'Detailed spline information.'
        },
        meshComponents: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              name: commonSchemas.stringProp,
              meshPath: commonSchemas.stringProp,
              forwardAxis: commonSchemas.stringProp
            }
          },
          description: 'List of spline mesh components.'
        },
        scatteredMeshes: { type: 'number', description: 'Number of meshes scattered along spline.' },
        error: commonSchemas.stringProp
      }
    }
  },
  {
    name: 'manage_pcg',
    category: 'world',
    description: 'Create and manage PCG (Procedural Content Generation) graphs, nodes, samplers, filters, and spawners. Execute graphs and configure partition settings.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'create_pcg_graph', 'create_pcg_subgraph', 'add_pcg_node',
            'connect_pcg_pins', 'set_pcg_node_settings',
            'add_landscape_data_node', 'add_spline_data_node', 'add_volume_data_node',
            'add_actor_data_node', 'add_texture_data_node',
            'add_surface_sampler', 'add_mesh_sampler', 'add_spline_sampler', 'add_volume_sampler',
            'add_bounds_modifier', 'add_density_filter', 'add_height_filter', 'add_slope_filter',
            'add_distance_filter', 'add_bounds_filter', 'add_self_pruning',
            'add_transform_points', 'add_project_to_surface', 'add_copy_points', 'add_merge_points',
            'add_static_mesh_spawner', 'add_actor_spawner', 'add_spline_spawner',
            'execute_pcg_graph', 'set_pcg_partition_grid_size',
            'get_pcg_info'
          ],
          description: 'PCG action to perform'
        },
        graphName: { type: 'string', description: 'Name for new PCG graph asset.' },
        graphPath: commonSchemas.assetPath,
        subgraphPath: commonSchemas.assetPath,
        nodeName: { type: 'string', description: 'Name or ID of PCG node.' },
        nodeId: { type: 'string', description: 'Unique ID of PCG node.' },
        nodeClass: { type: 'string', description: 'Class of PCG node to create (e.g., UPCGSurfaceSamplerSettings).' },
        settingsClass: { type: 'string', description: 'Settings class for node.' },
        nodePosition: {
          type: 'object',
          properties: {
            x: commonSchemas.numberProp, y: commonSchemas.numberProp
          },
          description: 'Position of node in graph editor.'
        },
        sourceNodeId: { type: 'string', description: 'Source node ID for pin connection.' },
        sourcePinName: { type: 'string', description: 'Source pin name for connection.' },
        targetNodeId: { type: 'string', description: 'Target node ID for pin connection.' },
        targetPinName: { type: 'string', description: 'Target pin name for connection.' },
        settings: {
          type: 'object',
          description: 'Node settings as key-value pairs.'
        },
        samplerType: {
          type: 'string',
          enum: ['Surface', 'Mesh', 'Spline', 'Volume'],
          description: 'Type of sampler to create.'
        },
        pointsPerSquaredMeter: { type: 'number', description: 'Point density for surface sampler.' },
        looseness: { type: 'number', description: 'Looseness value for sampler (0-1).' },
        pointExtents: {
          type: 'object',
          properties: {
            x: commonSchemas.numberProp, y: commonSchemas.numberProp, z: commonSchemas.numberProp
          },
          description: 'Point extents for sampler.'
        },
        unbounded: { type: 'boolean', description: 'Whether sampler is unbounded.' },
        filterType: {
          type: 'string',
          enum: ['Density', 'Height', 'Slope', 'Distance', 'Bounds', 'SelfPruning'],
          description: 'Type of filter to create.'
        },
        minValue: { type: 'number', description: 'Minimum value for filter.' },
        maxValue: { type: 'number', description: 'Maximum value for filter.' },
        invert: { type: 'boolean', description: 'Invert filter results.' },
        minHeight: { type: 'number', description: 'Minimum height for height filter.' },
        maxHeight: { type: 'number', description: 'Maximum height for height filter.' },
        minSlope: { type: 'number', description: 'Minimum slope angle (degrees) for slope filter.' },
        maxSlope: { type: 'number', description: 'Maximum slope angle (degrees) for slope filter.' },
        distanceMin: { type: 'number', description: 'Minimum distance for distance filter.' },
        distanceMax: { type: 'number', description: 'Maximum distance for distance filter.' },
        boundsMin: {
          type: 'object',
          properties: {
            x: commonSchemas.numberProp, y: commonSchemas.numberProp, z: commonSchemas.numberProp
          },
          description: 'Minimum bounds for bounds filter/modifier.'
        },
        boundsMax: {
          type: 'object',
          properties: {
            x: commonSchemas.numberProp, y: commonSchemas.numberProp, z: commonSchemas.numberProp
          },
          description: 'Maximum bounds for bounds filter/modifier.'
        },
        pruningRadius: { type: 'number', description: 'Radius for self-pruning.' },
        pruningType: {
          type: 'string',
          enum: ['Random', 'LargestRemoved', 'SmallestRemoved'],
          description: 'Pruning type for self-pruning.'
        },
        offsetMin: {
          type: 'object',
          properties: {
            x: commonSchemas.numberProp, y: commonSchemas.numberProp, z: commonSchemas.numberProp
          },
          description: 'Minimum offset for transform.'
        },
        offsetMax: {
          type: 'object',
          properties: {
            x: commonSchemas.numberProp, y: commonSchemas.numberProp, z: commonSchemas.numberProp
          },
          description: 'Maximum offset for transform.'
        },
        rotationMin: {
          type: 'object',
          properties: {
            pitch: commonSchemas.numberProp, yaw: commonSchemas.numberProp, roll: commonSchemas.numberProp
          },
          description: 'Minimum rotation for transform.'
        },
        rotationMax: {
          type: 'object',
          properties: {
            pitch: commonSchemas.numberProp, yaw: commonSchemas.numberProp, roll: commonSchemas.numberProp
          },
          description: 'Maximum rotation for transform.'
        },
        scaleMin: {
          type: 'object',
          properties: {
            x: commonSchemas.numberProp, y: commonSchemas.numberProp, z: commonSchemas.numberProp
          },
          description: 'Minimum scale for transform.'
        },
        scaleMax: {
          type: 'object',
          properties: {
            x: commonSchemas.numberProp, y: commonSchemas.numberProp, z: commonSchemas.numberProp
          },
          description: 'Maximum scale for transform.'
        },
        bAbsoluteOffset: { type: 'boolean', description: 'Use absolute offset values.' },
        bAbsoluteRotation: { type: 'boolean', description: 'Use absolute rotation values.' },
        bAbsoluteScale: { type: 'boolean', description: 'Use absolute scale values.' },
        spawnerType: {
          type: 'string',
          enum: ['StaticMesh', 'Actor', 'Spline'],
          description: 'Type of spawner to create.'
        },
        meshPath: commonSchemas.meshPath,
        meshPaths: {
          type: 'array',
          items: commonSchemas.stringProp,
          description: 'Array of mesh paths for spawner.'
        },
        actorClass: { type: 'string', description: 'Actor class path for actor spawner.' },
        splineActorClass: { type: 'string', description: 'Spline actor class for spline spawner.' },
        meshWeights: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              meshPath: commonSchemas.stringProp,
              weight: commonSchemas.numberProp
            }
          },
          description: 'Weighted mesh selection for spawner.'
        },
        spawnOption: {
          type: 'string',
          enum: ['CollapseActors', 'MergePCGOnly', 'NoMerging'],
          description: 'Actor spawn option.'
        },
        actorName: commonSchemas.actorName,
        componentName: commonSchemas.componentName,
        bForce: { type: 'boolean', description: 'Force graph execution.' },
        partitionGridSize: { type: 'number', description: 'Partition grid size for World Partition.' },
        inputTag: { type: 'string', description: 'Input tag for data node.' },
        landscapeActor: { type: 'string', description: 'Landscape actor name for landscape data node.' },
        splineActor: { type: 'string', description: 'Spline actor name for spline data node.' },
        volumeActor: { type: 'string', description: 'Volume actor name for volume data node.' },
        actorPath: commonSchemas.actorPath,
        texturePath: commonSchemas.texturePath,
        filter: commonSchemas.filter,
        includeNodes: { type: 'boolean', description: 'Include node details in get_pcg_info.' },
        includeConnections: { type: 'boolean', description: 'Include connection details in get_pcg_info.' },
        save: commonSchemas.save
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: commonSchemas.booleanProp,
        message: commonSchemas.stringProp,
        graphPath: commonSchemas.assetPath,
        nodeId: { type: 'string', description: 'Created/modified node ID.' },
        nodeName: { type: 'string', description: 'Created/modified node name.' },
        connectionId: { type: 'string', description: 'Created connection ID.' },
        pcgInfo: {
          type: 'object',
          properties: {
            graphPath: commonSchemas.stringProp,
            nodeCount: commonSchemas.numberProp,
            inputNodes: commonSchemas.arrayOfStrings,
            outputNodes: commonSchemas.arrayOfStrings,
            nodes: {
              type: 'array',
              items: {
                type: 'object',
                properties: {
                  id: commonSchemas.stringProp,
                  name: commonSchemas.stringProp,
                  class: commonSchemas.stringProp,
                  position: {
                    type: 'object',
                    properties: {
                      x: commonSchemas.numberProp,
                      y: commonSchemas.numberProp
                    }
                  }
                }
              }
            },
            connections: {
              type: 'array',
              items: {
                type: 'object',
                properties: {
                  sourceNode: commonSchemas.stringProp,
                  sourcePin: commonSchemas.stringProp,
                  targetNode: commonSchemas.stringProp,
                  targetPin: commonSchemas.stringProp
                }
              }
            }
          },
          description: 'PCG graph information (for get_pcg_info).'
        },
        error: commonSchemas.stringProp
      }
    }
  },
  // Phase 29: Advanced Lighting & Rendering
  {
    name: 'manage_post_process',
    category: 'world',
    description: 'Post-process volumes, bloom, DOF, motion blur, color grading, reflection captures, ray tracing, scene captures, and light channels.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            // Post-Process Core
            'create_post_process_volume', 'configure_pp_blend', 'configure_pp_priority', 'get_post_process_settings',
            // Visual Effects
            'configure_bloom', 'configure_dof', 'configure_motion_blur',
            // Color & Lens
            'configure_color_grading', 'configure_white_balance', 'configure_vignette',
            'configure_chromatic_aberration', 'configure_film_grain', 'configure_lens_flares',
            // Reflection Captures
            'create_sphere_reflection_capture', 'create_box_reflection_capture', 'create_planar_reflection', 'recapture_scene',
            // Ray Tracing
            'configure_ray_traced_shadows', 'configure_ray_traced_gi', 'configure_ray_traced_reflections',
            'configure_ray_traced_ao', 'configure_path_tracing',
            // Scene Captures
            'create_scene_capture_2d', 'create_scene_capture_cube', 'capture_scene',
            // Light Channels
            'set_light_channel', 'set_actor_light_channel',
            // Lightmass
            'configure_lightmass_settings', 'build_lighting_quality', 'configure_indirect_lighting_cache', 'configure_volumetric_lightmap'
          ],
          description: 'Post-process/rendering action'
        },
        // Actor/Volume identification
        actorName: commonSchemas.actorName,
        volumeName: commonSchemas.volumeName,
        name: commonSchemas.name,
        location: commonSchemas.location,
        rotation: commonSchemas.rotation,
        
        // Post-Process Volume Core
        infinite: { type: 'boolean', description: 'Whether volume is infinite (unbound) or uses extent.' },
        blendRadius: { type: 'number', description: 'Blend radius for volume transitions.' },
        blendWeight: { type: 'number', description: 'Blend weight (0.0 - 1.0).' },
        priority: { type: 'number', description: 'Volume priority (higher = more important).' },
        enabled: commonSchemas.enabled,
        extent: commonSchemas.extent,
        
        // Bloom Settings
        bloomIntensity: { type: 'number', description: 'Bloom intensity (0.0 - 8.0).' },
        bloomThreshold: { type: 'number', description: 'Bloom threshold (-1 to 8, -1 = default).' },
        bloomSizeScale: { type: 'number', description: 'Bloom size scale multiplier.' },
        bloomConvolutionSize: { type: 'number', description: 'Bloom convolution kernel size.' },
        lensFlareIntensity: { type: 'number', description: 'Lens flare intensity (0.0 - 8.0).' },
        lensFlareTint: commonSchemas.colorObject,
        lensFlareBokehSize: { type: 'number', description: 'Lens flare bokeh size.' },
        lensFlareThreshold: { type: 'number', description: 'Lens flare threshold.' },
        
        // Depth of Field Settings
        focalDistance: { type: 'number', description: 'Distance to focus plane in cm.' },
        focalRegion: { type: 'number', description: 'Size of focal region in cm.' },
        nearTransitionRegion: { type: 'number', description: 'Near blur transition region in cm.' },
        farTransitionRegion: { type: 'number', description: 'Far blur transition region in cm.' },
        depthBlurKmForMaxMobileDof: { type: 'number', description: 'Depth blur distance for mobile DOF.' },
        depthBlurRadius: { type: 'number', description: 'Depth blur radius.' },
        depthOfFieldMethod: { type: 'string', enum: ['BokehDOF', 'Gaussian', 'CircleDOF'], description: 'DOF rendering method.' },
        nearBlurSize: { type: 'number', description: 'Near blur size (0.0 - 4.0).' },
        farBlurSize: { type: 'number', description: 'Far blur size (0.0 - 4.0).' },
        
        // Motion Blur Settings
        motionBlurAmount: { type: 'number', description: 'Motion blur amount (0.0 - 4.0).' },
        motionBlurMax: { type: 'number', description: 'Max motion blur (% of screen).' },
        motionBlurPerObjectSize: { type: 'number', description: 'Per-object motion blur size threshold.' },
        motionBlurTargetFPS: { type: 'number', description: 'Target FPS for motion blur calculations.' },
        
        // Color Grading Settings
        globalSaturation: commonSchemas.colorObject,
        globalContrast: commonSchemas.colorObject,
        globalGamma: commonSchemas.colorObject,
        globalGain: commonSchemas.colorObject,
        globalOffset: commonSchemas.colorObject,
        colorOffset: commonSchemas.colorObject,
        colorSaturation: commonSchemas.colorObject,
        colorContrast: commonSchemas.colorObject,
        colorGamma: commonSchemas.colorObject,
        colorGain: commonSchemas.colorObject,
        sceneColorTint: commonSchemas.colorObject,
        
        // White Balance
        whiteTemp: { type: 'number', description: 'White balance temperature (1500-15000).' },
        whiteTint: { type: 'number', description: 'White balance tint (-1.0 to 1.0).' },
        
        // Vignette
        vignetteIntensity: { type: 'number', description: 'Vignette intensity (0.0 - 1.0).' },
        
        // Chromatic Aberration
        chromaticAberrationIntensity: { type: 'number', description: 'Chromatic aberration intensity (0.0 - 1.0).' },
        chromaticAberrationStartOffset: { type: 'number', description: 'Chromatic aberration start offset (0.0 - 1.0).' },
        
        // Film Grain
        filmGrainIntensity: { type: 'number', description: 'Film grain intensity (0.0 - 1.0).' },
        filmGrainIntensityShadows: { type: 'number', description: 'Film grain intensity in shadows.' },
        filmGrainIntensityMidtones: { type: 'number', description: 'Film grain intensity in midtones.' },
        filmGrainIntensityHighlights: { type: 'number', description: 'Film grain intensity in highlights.' },
        filmGrainShadowsMax: { type: 'number', description: 'Film grain shadows max.' },
        filmGrainHighlightsMin: { type: 'number', description: 'Film grain highlights min.' },
        filmGrainHighlightsMax: { type: 'number', description: 'Film grain highlights max.' },
        filmGrainTexelSize: { type: 'number', description: 'Film grain texel size.' },
        
        // Reflection Capture
        influenceRadius: { type: 'number', description: 'Influence radius for reflection capture.' },
        boxExtent: commonSchemas.extent,
        boxTransitionDistance: { type: 'number', description: 'Box transition distance.' },
        captureOffset: commonSchemas.location,
        brightness: { type: 'number', description: 'Capture brightness.' },
        screenPercentage: { type: 'number', description: 'Screen percentage for planar reflections (25-100).' },
        
        // Ray Tracing
        rayTracedShadowsEnabled: { type: 'boolean', description: 'Enable ray traced shadows.' },
        rayTracedShadowsSamplesPerPixel: { type: 'number', description: 'Samples per pixel for RT shadows.' },
        rayTracedGIEnabled: { type: 'boolean', description: 'Enable ray traced global illumination.' },
        rayTracedGIType: { type: 'string', enum: ['BruteForce', 'FinalGather'], description: 'Ray traced GI type.' },
        rayTracedGIMaxBounces: { type: 'number', description: 'Max bounces for RT GI.' },
        rayTracedGISamplesPerPixel: { type: 'number', description: 'Samples per pixel for RT GI.' },
        rayTracedReflectionsEnabled: { type: 'boolean', description: 'Enable ray traced reflections.' },
        rayTracedReflectionsMaxBounces: { type: 'number', description: 'Max bounces for RT reflections.' },
        rayTracedReflectionsSamplesPerPixel: { type: 'number', description: 'Samples per pixel for RT reflections.' },
        rayTracedReflectionsMaxRoughness: { type: 'number', description: 'Max roughness for RT reflections.' },
        rayTracedAOEnabled: { type: 'boolean', description: 'Enable ray traced ambient occlusion.' },
        rayTracedAOIntensity: { type: 'number', description: 'RT AO intensity.' },
        rayTracedAORadius: { type: 'number', description: 'RT AO radius.' },
        rayTracedAOSamplesPerPixel: { type: 'number', description: 'Samples per pixel for RT AO.' },
        pathTracingEnabled: { type: 'boolean', description: 'Enable path tracing.' },
        pathTracingSamplesPerPixel: { type: 'number', description: 'Samples per pixel for path tracing.' },
        pathTracingMaxBounces: { type: 'number', description: 'Max bounces for path tracing.' },
        pathTracingFilterWidth: { type: 'number', description: 'Filter width for path tracing.' },
        
        // Scene Capture
        fov: { type: 'number', description: 'Field of view for scene capture.' },
        captureResolution: { type: 'number', description: 'Capture resolution (width=height).' },
        captureWidth: { type: 'number', description: 'Capture width in pixels.' },
        captureHeight: { type: 'number', description: 'Capture height in pixels.' },
        captureSource: {
          type: 'string',
          enum: ['SceneColorHDR', 'SceneColorHDRNoAlpha', 'FinalColorLDR', 'SceneColorSceneDepth', 'SceneDepth', 'DeviceDepth', 'Normal', 'BaseColor'],
          description: 'What to capture.'
        },
        textureTargetPath: { type: 'string', description: 'Path to render target texture.' },
        savePath: commonSchemas.savePath,
        
        // Light Channels
        lightActorName: { type: 'string', description: 'Name of light actor to configure.' },
        channel0: { type: 'boolean', description: 'Light channel 0 state.' },
        channel1: { type: 'boolean', description: 'Light channel 1 state.' },
        channel2: { type: 'boolean', description: 'Light channel 2 state.' },
        
        // Lightmass Settings
        numIndirectBounces: { type: 'number', description: 'Number of indirect lighting bounces.' },
        indirectLightingQuality: { type: 'number', description: 'Indirect lighting quality (0.0-1.0).' },
        indirectLightingSmoothness: { type: 'number', description: 'Indirect lighting smoothness.' },
        environmentColor: commonSchemas.colorObject,
        environmentIntensity: { type: 'number', description: 'Environment lighting intensity.' },
        staticLightingScaleX: { type: 'number', description: 'Static lighting scale X.' },
        staticLightingScaleY: { type: 'number', description: 'Static lighting scale Y.' },
        staticLightingScaleZ: { type: 'number', description: 'Static lighting scale Z.' },
        
        // Lighting Quality
        quality: {
          type: 'string',
          enum: ['Preview', 'Medium', 'High', 'Production'],
          description: 'Lighting build quality.'
        },
        
        // Indirect Lighting Cache
        indirectLightingCacheEnabled: { type: 'boolean', description: 'Enable indirect lighting cache.' },
        indirectLightingCacheQuality: {
          type: 'string',
          enum: ['Point', 'Volume'],
          description: 'Indirect lighting cache quality.'
        },
        
        // Volumetric Lightmap
        volumetricLightmapEnabled: { type: 'boolean', description: 'Enable volumetric lightmap.' },
        volumetricLightmapDetailCellSize: { type: 'number', description: 'Volumetric lightmap detail cell size.' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        ...commonSchemas.outputBase,
        actorName: commonSchemas.actorName,
        volumeName: commonSchemas.stringProp,
        capturePath: commonSchemas.stringProp,
        textureTargetPath: commonSchemas.stringProp,
        // Post-process settings output
        postProcessSettings: {
          type: 'object',
          description: 'Current post-process settings.'
        },
        // Reflection capture info
        reflectionCaptureInfo: {
          type: 'object',
          properties: {
            type: commonSchemas.stringProp,
            influenceRadius: commonSchemas.numberProp,
            brightness: commonSchemas.numberProp
          }
        },
        // Scene capture info
        sceneCaptureInfo: {
          type: 'object',
          properties: {
            captureSource: commonSchemas.stringProp,
            resolution: commonSchemas.numberProp,
            fov: commonSchemas.numberProp
          }
        },
        // Light channel info
        lightChannels: {
          type: 'object',
          properties: {
            channel0: commonSchemas.booleanProp,
            channel1: commonSchemas.booleanProp,
            channel2: commonSchemas.booleanProp
          }
        },
        // Lightmass info
        lightmassInfo: {
          type: 'object',
          properties: {
            numIndirectBounces: commonSchemas.numberProp,
            indirectLightingQuality: commonSchemas.numberProp,
            buildQuality: commonSchemas.stringProp
          }
        },
        error: commonSchemas.stringProp
      }
    }
  },
  // ============================================================================
  // Phase 30: Cinematics & Media
  // ============================================================================
  // NOTE: manage_sequencer and manage_movie_render have been merged into manage_sequence
  {
    name: 'manage_media',
    category: 'authoring',
    description: 'Manage Media Framework. Create media players, sources, textures, and control playback.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            // Asset creation
            'create_media_player', 'create_file_media_source', 'create_stream_media_source', 'create_media_texture',
            'create_media_playlist', 'create_media_sound_wave',
            // Asset management
            'delete_media_asset', 'get_media_info',
            // Playlist management
            'add_to_playlist', 'remove_from_playlist', 'get_playlist',
            // Playback control
            'open_source', 'open_url', 'close', 'play', 'pause', 'stop', 'seek', 'set_rate',
            // Properties
            'set_looping', 'get_duration', 'get_time', 'get_state',
            // Texture binding
            'bind_to_texture', 'unbind_from_texture'
          ],
          description: 'Media action to perform.'
        },
        // Asset paths
        mediaPlayerPath: { type: 'string', description: 'Path to media player asset.' },
        mediaSourcePath: { type: 'string', description: 'Path to media source asset.' },
        mediaTexturePath: { type: 'string', description: 'Path to media texture asset.' },
        playlistPath: { type: 'string', description: 'Path to media playlist asset.' },
        soundWavePath: { type: 'string', description: 'Path to media sound wave asset.' },
        savePath: commonSchemas.savePath,
        assetName: commonSchemas.name,
        
        // File/stream sources
        filePath: { type: 'string', description: 'Path to media file on disk.' },
        url: { type: 'string', description: 'URL for streaming media.' },
        
        // Texture settings
        textureWidth: { type: 'number', description: 'Media texture width.' },
        textureHeight: { type: 'number', description: 'Media texture height.' },
        srgb: { type: 'boolean', description: 'Enable sRGB for texture.' },
        autoPlay: { type: 'boolean', description: 'Auto play when opened.' },
        
        // Playback control
        time: { type: 'number', description: 'Time in seconds for seek.' },
        rate: { type: 'number', description: 'Playback rate (1.0 = normal speed).' },
        looping: { type: 'boolean', description: 'Enable looping.' },
        
        // Playlist
        playlistIndex: { type: 'number', description: 'Index in playlist.' },
        
        // Common
        save: commonSchemas.save
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        ...commonSchemas.outputBase,
        mediaPlayerPath: commonSchemas.stringProp,
        mediaSourcePath: commonSchemas.stringProp,
        mediaTexturePath: commonSchemas.stringProp,
        playlistPath: commonSchemas.stringProp,
        mediaInfo: {
          type: 'object',
          properties: {
            duration: commonSchemas.numberProp,
            videoWidth: commonSchemas.numberProp,
            videoHeight: commonSchemas.numberProp,
            frameRate: commonSchemas.numberProp,
            audioChannels: commonSchemas.numberProp,
            audioSampleRate: commonSchemas.numberProp,
            hasVideo: commonSchemas.booleanProp,
            hasAudio: commonSchemas.booleanProp,
            codecInfo: commonSchemas.stringProp
          }
        },
        playbackState: {
          type: 'object',
          properties: {
            state: { type: 'string', enum: ['Closed', 'Error', 'Opening', 'Playing', 'Paused', 'Stopped', 'Preparing'] },
            currentTime: commonSchemas.numberProp,
            duration: commonSchemas.numberProp,
            rate: commonSchemas.numberProp,
            isLooping: commonSchemas.booleanProp,
            isBuffering: commonSchemas.booleanProp
          }
        },
        playlist: commonSchemas.arrayOfStrings,
        error: commonSchemas.stringProp
      }
    }
  },
  // Phase 31: Data & Persistence
  {
    name: 'manage_data',
    category: 'utility',
    description: 'Manage data assets, data tables, save games, gameplay tags, and config files. Create, edit, import/export data.',
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
            'read_config_value', 'write_config_value', 'get_config_section', 'flush_config', 'reload_config'
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
    description: 'Run UBT, generate projects, cook/package, validate assets, manage plugins and DDC.',
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
            'clear_ddc', 'get_ddc_stats', 'configure_ddc'
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
  // Phase 33: Testing & Quality
  {
    name: 'manage_testing',
    category: 'utility',
    description: 'Run automation tests, create functional tests, enable profiling tools, validate assets and blueprints.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            // Automation Tests
            'list_tests', 'run_tests', 'run_test', 'get_test_results', 'get_test_info',
            // Functional Tests
            'list_functional_tests', 'run_functional_test', 'get_functional_test_results',
            // Profiling
            'start_trace', 'stop_trace', 'get_trace_status',
            'enable_visual_logger', 'disable_visual_logger', 'get_visual_logger_status',
            'start_stats_capture', 'stop_stats_capture',
            'get_memory_report', 'get_performance_stats',
            // Validation
            'validate_asset', 'validate_assets_in_path', 'validate_blueprint',
            'check_map_errors', 'fix_redirectors', 'get_redirectors'
          ],
          description: 'Testing action to perform.'
        },
        // Test parameters
        testName: { type: 'string', description: 'Full test name or pattern to run.' },
        testFilter: { type: 'string', description: 'Filter tests by name pattern.' },
        testFlags: {
          type: 'string',
          enum: ['Smoke', 'Engine', 'Product', 'Perf', 'Stress', 'All'],
          description: 'Test category filter.'
        },
        testPriority: {
          type: 'string',
          enum: ['Critical', 'High', 'Medium', 'Low', 'All'],
          description: 'Test priority filter.'
        },
        runInPIE: { type: 'boolean', description: 'Run test in Play-In-Editor mode.' },
        
        // Functional test parameters
        functionalTestPath: { type: 'string', description: 'Path to functional test level or actor.' },
        testTimeout: { type: 'number', description: 'Test timeout in seconds.' },
        
        // Trace parameters
        traceName: { type: 'string', description: 'Name for trace file.' },
        traceChannels: {
          type: 'array',
          items: { type: 'string' },
          description: 'Trace channels to enable (CPU, GPU, Frame, Memory, etc.).'
        },
        traceOutputPath: { type: 'string', description: 'Output path for trace file.' },
        
        // Visual Logger parameters
        logCategories: {
          type: 'array',
          items: { type: 'string' },
          description: 'Visual logger categories to enable.'
        },
        
        // Validation parameters
        assetPath: commonSchemas.assetPath,
        directoryPath: commonSchemas.directoryPath,
        blueprintPath: commonSchemas.blueprintPath,
        levelPath: { type: 'string', description: 'Level path for map error checking.' },
        recursive: { type: 'boolean', description: 'Recursively validate assets in subdirectories.' },
        fixIssues: { type: 'boolean', description: 'Attempt to fix validation issues.' },
        
        // Common
        save: commonSchemas.save
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        ...commonSchemas.outputBase,
        // Test results
        tests: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              name: commonSchemas.stringProp,
              displayName: commonSchemas.stringProp,
              testFlags: commonSchemas.stringProp,
              numParticipants: commonSchemas.numberProp
            }
          }
        },
        testResults: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              testName: commonSchemas.stringProp,
              passed: commonSchemas.booleanProp,
              duration: commonSchemas.numberProp,
              errors: commonSchemas.arrayOfStrings,
              warnings: commonSchemas.arrayOfStrings,
              logs: commonSchemas.arrayOfStrings
            }
          }
        },
        totalTests: commonSchemas.numberProp,
        passedTests: commonSchemas.numberProp,
        failedTests: commonSchemas.numberProp,
        skippedTests: commonSchemas.numberProp,
        
        // Trace results
        traceFilePath: commonSchemas.stringProp,
        traceStatus: { type: 'string', enum: ['idle', 'recording', 'stopped'] },
        traceDuration: commonSchemas.numberProp,
        
        // Visual Logger status
        visualLoggerEnabled: commonSchemas.booleanProp,
        activeCategories: commonSchemas.arrayOfStrings,
        
        // Performance stats
        performanceStats: {
          type: 'object',
          properties: {
            frameTime: commonSchemas.numberProp,
            gameThreadTime: commonSchemas.numberProp,
            renderThreadTime: commonSchemas.numberProp,
            gpuTime: commonSchemas.numberProp,
            fps: commonSchemas.numberProp,
            memoryUsedMB: commonSchemas.numberProp,
            memoryAvailableMB: commonSchemas.numberProp
          }
        },
        memoryReport: {
          type: 'object',
          properties: {
            totalPhysical: commonSchemas.numberProp,
            usedPhysical: commonSchemas.numberProp,
            totalVirtual: commonSchemas.numberProp,
            usedVirtual: commonSchemas.numberProp,
            peakUsed: commonSchemas.numberProp
          }
        },
        
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
        mapErrors: commonSchemas.arrayOfStrings,
        redirectors: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              redirectorPath: commonSchemas.stringProp,
              targetPath: commonSchemas.stringProp,
              fixed: commonSchemas.booleanProp
            }
          }
        },
        redirectorsFixed: commonSchemas.numberProp
      }
    }
  },
  
  // ===== PHASE 34: EDITOR UTILITIES =====
  {
    name: 'manage_editor_utilities',
    description: 'Editor automation: modes, content browser, selection, collision, physics materials, subsystems, timers, delegates, transactions.',
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
    description: 'Common gameplay patterns: targeting, checkpoints, objectives, markers, photo mode, dialogue, instancing, HLOD, localization, scalability.',
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
            'get_gameplay_systems_info'
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

  // ===== PHASE 36: CHARACTER & AVATAR PLUGINS =====
  {
    name: 'manage_character_avatar',
    category: 'authoring',
    description: 'MetaHuman, Groom/Hair, Mutable (Customizable), and Ready Player Me avatar systems. Import, spawn, customize, and configure character avatars.',
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
    description: 'Import/export via Interchange, USD, Alembic, glTF, Datasmith, SpeedTree, Quixel/Fab, Houdini Engine, and Substance plugins.',
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
            'get_asset_plugins_info'
          ],
          description: 'Asset & content plugin action.'
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

  // ===== PHASE 38: AUDIO MIDDLEWARE PLUGINS =====
  {
    name: 'manage_audio_middleware',
    category: 'utility',
    description: 'Audio middleware integration: Wwise (Audiokinetic), FMOD (Firelight), and Bink Video (built-in). Post events, manage banks, set parameters, play videos.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            // WWISE (30 actions)
            'connect_wwise_project', 'post_wwise_event', 'post_wwise_event_at_location',
            'stop_wwise_event', 'set_rtpc_value', 'set_rtpc_value_on_actor', 'get_rtpc_value',
            'set_wwise_switch', 'set_wwise_switch_on_actor', 'set_wwise_state',
            'load_wwise_bank', 'unload_wwise_bank', 'get_loaded_banks',
            'create_wwise_component', 'configure_wwise_component', 'configure_spatial_audio',
            'configure_room', 'configure_portal', 'set_listener_position',
            'get_wwise_event_duration', 'create_wwise_trigger', 'set_wwise_game_object',
            'unset_wwise_game_object', 'post_wwise_trigger', 'set_aux_send',
            'configure_occlusion', 'set_wwise_project_path', 'get_wwise_status',
            'configure_wwise_init', 'restart_wwise_engine',
            // FMOD (30 actions)
            'connect_fmod_project', 'play_fmod_event', 'play_fmod_event_at_location',
            'stop_fmod_event', 'set_fmod_parameter', 'set_fmod_global_parameter',
            'get_fmod_parameter', 'load_fmod_bank', 'unload_fmod_bank', 'get_fmod_loaded_banks',
            'create_fmod_component', 'configure_fmod_component', 'set_fmod_bus_volume',
            'set_fmod_bus_paused', 'set_fmod_bus_mute', 'set_fmod_vca_volume',
            'apply_fmod_snapshot', 'release_fmod_snapshot', 'set_fmod_listener_attributes',
            'get_fmod_event_info', 'configure_fmod_occlusion', 'configure_fmod_attenuation',
            'set_fmod_studio_path', 'get_fmod_status', 'configure_fmod_init',
            'restart_fmod_engine', 'set_fmod_3d_attributes', 'get_fmod_memory_usage',
            'pause_all_fmod_events', 'resume_all_fmod_events',
            // BINK VIDEO (20 actions)
            'create_bink_media_player', 'open_bink_video', 'play_bink', 'pause_bink',
            'stop_bink', 'seek_bink', 'set_bink_looping', 'set_bink_rate',
            'set_bink_volume', 'get_bink_duration', 'get_bink_time', 'get_bink_status',
            'create_bink_texture', 'configure_bink_texture', 'set_bink_texture_player',
            'draw_bink_to_texture', 'configure_bink_buffer_mode', 'configure_bink_sound_track',
            'configure_bink_draw_style', 'get_bink_dimensions',
            // Utility
            'get_audio_middleware_info'
          ],
          description: 'Audio middleware action.'
        },
        
        // Common parameters
        actorName: commonSchemas.actorName,
        assetPath: commonSchemas.assetPath,
        componentName: { type: 'string', description: 'Audio component name.' },
        
        // WWISE parameters
        eventName: { type: 'string', description: 'Wwise event name or path.' },
        eventId: { type: 'number', description: 'Wwise event ID (optional alternative to name).' },
        playingId: { type: 'number', description: 'Wwise playing ID for stopping specific instances.' },
        rtpcName: { type: 'string', description: 'RTPC (Real-Time Parameter Control) name.' },
        rtpcValue: { type: 'number', description: 'RTPC value.' },
        rtpcInterpolation: { type: 'number', description: 'RTPC interpolation time in ms.' },
        switchGroup: { type: 'string', description: 'Wwise switch group name.' },
        switchValue: { type: 'string', description: 'Wwise switch value.' },
        stateGroup: { type: 'string', description: 'Wwise state group name.' },
        stateValue: { type: 'string', description: 'Wwise state value.' },
        bankName: { type: 'string', description: 'SoundBank name.' },
        bankPath: { type: 'string', description: 'SoundBank file path.' },
        triggerName: { type: 'string', description: 'Wwise trigger name.' },
        auxBusName: { type: 'string', description: 'Auxiliary bus name.' },
        auxSendLevel: { type: 'number', description: 'Aux send level (0.0 - 1.0).' },
        occlusionValue: { type: 'number', description: 'Occlusion value (0.0 - 1.0).' },
        obstructionValue: { type: 'number', description: 'Obstruction value (0.0 - 1.0).' },
        roomId: { type: 'number', description: 'Wwise room ID.' },
        roomSettings: { 
          type: 'object', 
          properties: {
            reverbLevel: { type: 'number' },
            transmissionLoss: { type: 'number' },
            roomGameObjectId: { type: 'number' }
          },
          description: 'Room acoustic settings.'
        },
        portalSettings: {
          type: 'object',
          properties: {
            enabled: { type: 'boolean' },
            openness: { type: 'number' }
          },
          description: 'Portal acoustic settings.'
        },
        wwiseProjectPath: { type: 'string', description: 'Path to Wwise project folder.' },
        wwiseInitSettings: {
          type: 'object',
          properties: {
            maxSoundPropagationDepth: { type: 'number' },
            diffractionShadowAttenFactor: { type: 'number' },
            diffractionShadowDegrees: { type: 'number' }
          },
          description: 'Wwise initialization settings.'
        },
        
        // FMOD parameters
        fmodEventPath: { type: 'string', description: 'FMOD event path (e.g., event:/Music/Theme).' },
        fmodEventId: { type: 'string', description: 'FMOD event GUID.' },
        fmodInstanceId: { type: 'number', description: 'FMOD event instance ID.' },
        parameterName: { type: 'string', description: 'FMOD parameter name.' },
        parameterValue: { type: 'number', description: 'FMOD parameter value.' },
        fmodBankPath: { type: 'string', description: 'FMOD bank file path.' },
        busPath: { type: 'string', description: 'FMOD bus path (e.g., bus:/Music).' },
        busVolume: { type: 'number', description: 'Bus volume (0.0 - 1.0).' },
        busPaused: { type: 'boolean', description: 'Bus paused state.' },
        busMuted: { type: 'boolean', description: 'Bus muted state.' },
        vcaPath: { type: 'string', description: 'FMOD VCA path.' },
        vcaVolume: { type: 'number', description: 'VCA volume (0.0 - 1.0).' },
        snapshotPath: { type: 'string', description: 'FMOD snapshot path.' },
        snapshotIntensity: { type: 'number', description: 'Snapshot intensity (0.0 - 1.0).' },
        listenerIndex: { type: 'number', description: 'Listener index.' },
        fmod3DAttributes: {
          type: 'object',
          properties: {
            position: { type: 'array', items: { type: 'number' } },
            velocity: { type: 'array', items: { type: 'number' } },
            forward: { type: 'array', items: { type: 'number' } },
            up: { type: 'array', items: { type: 'number' } }
          },
          description: 'FMOD 3D attributes.'
        },
        fmodStudioPath: { type: 'string', description: 'Path to FMOD Studio project folder.' },
        fmodInitSettings: {
          type: 'object',
          properties: {
            maxChannels: { type: 'number' },
            studioFlags: { type: 'number' },
            liveupdatePort: { type: 'number' }
          },
          description: 'FMOD initialization settings.'
        },
        fmodOcclusionSettings: {
          type: 'object',
          properties: {
            directOcclusion: { type: 'number' },
            reverbOcclusion: { type: 'number' }
          },
          description: 'FMOD occlusion settings.'
        },
        fmodAttenuationSettings: {
          type: 'object',
          properties: {
            minDistance: { type: 'number' },
            maxDistance: { type: 'number' },
            rolloff: { type: 'string', enum: ['Linear', 'Logarithmic', 'Custom'] }
          },
          description: 'FMOD attenuation settings.'
        },
        
        // BINK VIDEO parameters
        mediaPlayerName: { type: 'string', description: 'Bink media player asset name.' },
        mediaPlayerPath: { type: 'string', description: 'Bink media player asset path.' },
        videoUrl: { type: 'string', description: 'Video URL or file path.' },
        seekTime: { type: 'number', description: 'Seek time in seconds.' },
        looping: { type: 'boolean', description: 'Enable looping playback.' },
        playbackRate: { type: 'number', description: 'Playback rate (1.0 = normal).' },
        volume: { type: 'number', description: 'Volume (0.0 - 1.0).' },
        textureName: { type: 'string', description: 'Bink texture asset name.' },
        texturePath: { type: 'string', description: 'Bink texture asset path.' },
        binkTextureSettings: {
          type: 'object',
          properties: {
            addressX: { type: 'string', enum: ['Wrap', 'Clamp', 'Mirror'] },
            addressY: { type: 'string', enum: ['Wrap', 'Clamp', 'Mirror'] },
            pixelFormat: { type: 'string' },
            tonemap: { type: 'boolean' },
            outputNits: { type: 'number' },
            alpha: { type: 'number' },
            decodeSRGB: { type: 'boolean' }
          },
          description: 'Bink texture settings.'
        },
        bufferMode: { 
          type: 'string', 
          enum: ['Stream', 'PreloadAll', 'StreamUntilResident'], 
          description: 'Bink buffer mode.' 
        },
        soundTrack: { 
          type: 'string', 
          enum: ['None', 'Simple', 'LanguageOverride', '51Surround', '51SurroundLanguageOverride', '71Surround', '71SurroundLanguageOverride'], 
          description: 'Bink sound track mode.' 
        },
        soundTrackStart: { type: 'number', description: 'Sound track start index.' },
        drawStyle: { 
          type: 'string', 
          enum: ['RenderToTexture', 'OverlayFillScreenWithAspectRatio', 'OverlayOriginalMovieSize', 'OverlayFillScreen', 'OverlaySpecificDestinationRectangle'], 
          description: 'Bink draw style.' 
        },
        layerDepth: { type: 'number', description: 'Bink layer depth for overlay rendering.' },
        drawToTexture: { type: 'string', description: 'Target texture for Draw operation.' },
        drawTonemap: { type: 'boolean', description: 'Enable tonemapping when drawing.' },
        drawAlpha: { type: 'number', description: 'Alpha value for drawing (0.0 - 1.0).' },
        drawHDR: { type: 'boolean', description: 'Enable HDR when drawing.' },
        
        // Common transform for spatial audio
        location: { type: 'array', items: { type: 'number' }, description: 'World location [X, Y, Z].' },
        rotation: { type: 'array', items: { type: 'number' }, description: 'Rotation [Pitch, Yaw, Roll].' },
        
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
        
        // Plugin availability
        middlewareInfo: {
          type: 'object',
          properties: {
            wwiseAvailable: commonSchemas.booleanProp,
            wwiseVersion: commonSchemas.stringProp,
            fmodAvailable: commonSchemas.booleanProp,
            fmodVersion: commonSchemas.stringProp,
            binkAvailable: commonSchemas.booleanProp
          },
          description: 'Audio middleware availability (for get_audio_middleware_info).'
        },
        
        // Wwise outputs
        playingId: { type: 'number', description: 'Wwise playing ID.' },
        rtpcValue: { type: 'number', description: 'Retrieved RTPC value.' },
        eventDuration: { type: 'number', description: 'Event duration in ms.' },
        loadedBanks: { type: 'array', items: { type: 'string' }, description: 'List of loaded bank names.' },
        wwiseStatus: {
          type: 'object',
          properties: {
            isInitialized: commonSchemas.booleanProp,
            projectPath: commonSchemas.stringProp,
            activeSounds: { type: 'number' }
          },
          description: 'Wwise status info.'
        },
        
        // FMOD outputs
        fmodInstanceId: { type: 'number', description: 'FMOD event instance ID.' },
        fmodParameterValue: { type: 'number', description: 'Retrieved parameter value.' },
        fmodLoadedBanks: { type: 'array', items: { type: 'string' }, description: 'List of loaded FMOD banks.' },
        fmodEventInfo: {
          type: 'object',
          properties: {
            path: commonSchemas.stringProp,
            length: { type: 'number' },
            is3D: commonSchemas.booleanProp,
            isOneshot: commonSchemas.booleanProp,
            minDistance: { type: 'number' },
            maxDistance: { type: 'number' }
          },
          description: 'FMOD event information.'
        },
        fmodMemoryUsage: {
          type: 'object',
          properties: {
            currentAllocated: { type: 'number' },
            maxAllocated: { type: 'number' },
            sampleDataAllocated: { type: 'number' }
          },
          description: 'FMOD memory usage stats.'
        },
        fmodStatus: {
          type: 'object',
          properties: {
            isInitialized: commonSchemas.booleanProp,
            studioPath: commonSchemas.stringProp,
            activeEvents: { type: 'number' },
            cpuDsp: { type: 'number' },
            cpuUpdate: { type: 'number' }
          },
          description: 'FMOD status info.'
        },
        
        // Bink outputs
        mediaPlayerPath: commonSchemas.stringProp,
        texturePath: commonSchemas.stringProp,
        duration: { type: 'number', description: 'Video duration in seconds.' },
        currentTime: { type: 'number', description: 'Current playback time in seconds.' },
        binkStatus: {
          type: 'object',
          properties: {
            isPlaying: commonSchemas.booleanProp,
            isPaused: commonSchemas.booleanProp,
            isStopped: commonSchemas.booleanProp,
            isLooping: commonSchemas.booleanProp,
            currentRate: { type: 'number' }
          },
          description: 'Bink playback status.'
        },
        binkDimensions: {
          type: 'object',
          properties: {
            width: { type: 'number' },
            height: { type: 'number' },
            frameRate: { type: 'number' }
          },
          description: 'Bink video dimensions.'
        }
      }
    }
  },

  // ===== PHASE 39: MOTION CAPTURE & LIVE LINK =====
  {
    name: 'manage_livelink',
    category: 'utility',
    description: 'Live Link motion capture: sources, subjects, presets, face tracking, skeleton mapping. Manage live data streaming from mocap systems.',
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

  // ===== PHASE 40: VIRTUAL PRODUCTION PLUGINS =====
  {
    name: 'manage_virtual_production',
    category: 'utility',
    description: 'Virtual production plugins: nDisplay clusters, Composure compositing, OCIO color management, Remote Control, DMX lighting, OSC, MIDI, and Timecode/Genlock.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            // nDISPLAY - Cluster (10)
            'create_ndisplay_config', 'add_cluster_node', 'remove_cluster_node',
            'add_viewport', 'remove_viewport', 'set_viewport_camera',
            'configure_viewport_region', 'set_projection_policy', 'configure_warp_blend', 'list_cluster_nodes',
            // nDISPLAY - LED/ICVFX (10)
            'create_led_wall', 'configure_led_wall_size', 'configure_icvfx_camera',
            'add_icvfx_camera', 'remove_icvfx_camera', 'configure_inner_frustum',
            'configure_outer_viewport', 'set_chromakey_settings', 'configure_light_cards', 'set_stage_settings',
            // nDISPLAY - Sync (5)
            'set_sync_policy', 'configure_genlock', 'set_primary_node',
            'configure_network_settings', 'get_ndisplay_info',
            // COMPOSURE (12)
            'create_composure_element', 'delete_composure_element', 'add_composure_layer',
            'remove_composure_layer', 'attach_child_layer', 'detach_child_layer',
            'add_input_pass', 'add_transform_pass', 'add_output_pass',
            'configure_chroma_keyer', 'bind_render_target', 'get_composure_info',
            // OCIO (10)
            'create_ocio_config', 'load_ocio_config', 'get_ocio_colorspaces',
            'get_ocio_displays', 'set_display_view', 'add_colorspace_transform',
            'apply_ocio_look', 'configure_viewport_ocio', 'set_ocio_working_colorspace', 'get_ocio_info',
            // REMOTE CONTROL (15)
            'create_remote_control_preset', 'load_remote_control_preset', 'expose_property',
            'unexpose_property', 'expose_function', 'create_controller',
            'bind_controller', 'get_exposed_properties', 'set_exposed_property_value',
            'get_exposed_property_value', 'start_web_server', 'stop_web_server',
            'get_web_server_status', 'create_layout_group', 'get_remote_control_info',
            // DMX (20)
            'create_dmx_library', 'import_gdtf', 'create_fixture_type',
            'add_fixture_mode', 'add_fixture_function', 'create_fixture_patch',
            'assign_fixture_to_universe', 'configure_dmx_port', 'create_artnet_port',
            'create_sacn_port', 'send_dmx', 'receive_dmx',
            'set_fixture_channel_value', 'get_fixture_channel_value', 'add_dmx_component',
            'configure_dmx_component', 'list_dmx_universes', 'list_dmx_fixtures',
            'create_dmx_sequencer_track', 'get_dmx_info',
            // OSC (12)
            'create_osc_server', 'stop_osc_server', 'create_osc_client',
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

  // ============================================
  // Phase 41: XR Plugins (VR/AR/MR) - manage_xr
  // ============================================
  {
    name: 'manage_xr',
    description: `XR Plugins (VR/AR/MR) management - OpenXR, Meta Quest, SteamVR, ARKit, ARCore, Varjo, HoloLens.
Supports ~140 actions across 7 XR platform subsystems.

OPENXR (20 actions): Core runtime, tracking, actions, haptics, hand/eye tracking
META QUEST (22 actions): Passthrough, scene capture, hand/face/body tracking, spatial anchors
STEAMVR (18 actions): Chaperone, overlays, lighthouse, skeletal input
ARKIT (22 actions): World tracking, planes, images, face/body tracking, scene reconstruction
ARCORE (18 actions): Planes, anchors, depth, geospatial, cloud anchors
VARJO (16 actions): Passthrough, eye tracking, foveated rendering, mixed reality
HOLOLENS (20 actions): Spatial mapping, scene understanding, QR tracking, voice commands
UTILITIES (6 actions): System info, device management`,
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
            'reset_xr_orientation', 'configure_xr_spectator', 'get_xr_runtime_name'
          ],
          description: 'XR action to perform.'
        },

        // === Common Parameters ===
        device: { type: 'string', description: 'XR device name or ID.' },
        hand: { type: 'string', enum: ['left', 'right', 'both'], description: 'Hand specifier.' },
        controller: { type: 'string', enum: ['left', 'right'], description: 'Controller side.' },

        // === OpenXR Parameters ===
        trackingOrigin: { type: 'string', enum: ['floor', 'eye', 'stage'], description: 'Tracking origin mode.' },
        actionSetName: { type: 'string', description: 'Name for action set.' },
        actionName: { type: 'string', description: 'Name for XR action.' },
        actionType: { type: 'string', enum: ['boolean', 'float', 'vector2', 'pose', 'vibration'], description: 'Action type.' },
        bindingPath: { type: 'string', description: 'OpenXR binding path (e.g., /user/hand/left/input/trigger/value).' },
        hapticDuration: { type: 'number', description: 'Haptic duration in milliseconds.' },
        hapticFrequency: { type: 'number', description: 'Haptic frequency in Hz.' },
        hapticAmplitude: { type: 'number', description: 'Haptic amplitude (0.0-1.0).' },
        renderScale: { type: 'number', description: 'Render scale multiplier.' },

        // === Quest Parameters ===
        passthroughEnabled: { type: 'boolean', description: 'Enable passthrough.' },
        passthroughOpacity: { type: 'number', description: 'Passthrough opacity (0.0-1.0).' },
        passthroughContrast: { type: 'number', description: 'Passthrough contrast adjustment.' },
        passthroughBrightness: { type: 'number', description: 'Passthrough brightness adjustment.' },
        passthroughSaturation: { type: 'number', description: 'Passthrough saturation adjustment.' },
        anchorId: { type: 'string', description: 'Spatial anchor ID.' },
        anchorLocation: {
          type: 'object',
          properties: {
            x: { type: 'number' },
            y: { type: 'number' },
            z: { type: 'number' }
          },
          description: 'Anchor world location.'
        },
        saveToCloud: { type: 'boolean', description: 'Save anchor to cloud storage.' },

        // === SteamVR Parameters ===
        overlayName: { type: 'string', description: 'Overlay identifier.' },
        overlayKey: { type: 'string', description: 'Unique overlay key.' },
        overlayTexture: { type: 'string', description: 'Texture path for overlay.' },
        overlayWidth: { type: 'number', description: 'Overlay width in meters.' },
        overlayTransform: {
          type: 'object',
          properties: {
            x: { type: 'number' },
            y: { type: 'number' },
            z: { type: 'number' },
            pitch: { type: 'number' },
            yaw: { type: 'number' },
            roll: { type: 'number' }
          },
          description: 'Overlay transform.'
        },
        deviceIndex: { type: 'number', description: 'Tracked device index.' },
        actionManifestPath: { type: 'string', description: 'Path to action manifest JSON.' },

        // === ARKit Parameters ===
        sessionType: { type: 'string', enum: ['world', 'face', 'body', 'geo'], description: 'ARKit session type.' },
        planeDetection: { type: 'string', enum: ['horizontal', 'vertical', 'both', 'none'], description: 'Plane detection mode.' },
        referenceImageName: { type: 'string', description: 'Reference image name for tracking.' },
        referenceImagePath: { type: 'string', description: 'Path to reference image.' },
        physicalWidth: { type: 'number', description: 'Physical width of reference image in meters.' },
        peopleOcclusionEnabled: { type: 'boolean', description: 'Enable people occlusion.' },
        sceneReconstructionEnabled: { type: 'boolean', description: 'Enable scene reconstruction.' },
        raycastOrigin: {
          type: 'object',
          properties: { x: { type: 'number' }, y: { type: 'number' } },
          description: 'Screen-space raycast origin.'
        },
        raycastTypes: {
          type: 'array',
          items: { type: 'string', enum: ['plane', 'featurePoint', 'mesh'] },
          description: 'Raycast target types.'
        },

        // === ARCore Parameters ===
        depthEnabled: { type: 'boolean', description: 'Enable depth API.' },
        geospatialEnabled: { type: 'boolean', description: 'Enable geospatial API.' },
        latitude: { type: 'number', description: 'Latitude for geospatial anchor.' },
        longitude: { type: 'number', description: 'Longitude for geospatial anchor.' },
        altitude: { type: 'number', description: 'Altitude for geospatial anchor.' },
        heading: { type: 'number', description: 'Heading for geospatial anchor.' },
        cloudAnchorId: { type: 'string', description: 'Cloud anchor ID.' },
        cloudAnchorTtlDays: { type: 'number', description: 'Cloud anchor time-to-live in days.' },

        // === Varjo Parameters ===
        depthTestEnabled: { type: 'boolean', description: 'Enable video pass-through depth testing.' },
        depthTestRange: { type: 'number', description: 'Depth test range in meters.' },
        foveatedRenderingEnabled: { type: 'boolean', description: 'Enable foveated rendering.' },
        foveatedInnerRadius: { type: 'number', description: 'Inner foveated region radius.' },
        foveatedOuterRadius: { type: 'number', description: 'Outer foveated region radius.' },
        foveatedInnerQuality: { type: 'number', description: 'Inner region render quality (0.0-1.0).' },
        foveatedOuterQuality: { type: 'number', description: 'Outer region render quality (0.0-1.0).' },
        chromaKeyEnabled: { type: 'boolean', description: 'Enable chroma key.' },
        chromaKeyColor: {
          type: 'object',
          properties: { r: { type: 'number' }, g: { type: 'number' }, b: { type: 'number' } },
          description: 'Chroma key color.'
        },
        chromaKeyTolerance: { type: 'number', description: 'Chroma key color tolerance.' },
        markerEnabled: { type: 'boolean', description: 'Enable marker tracking.' },
        markerIds: { type: 'array', items: { type: 'number' }, description: 'Marker IDs to track.' },

        // === HoloLens Parameters ===
        spatialMappingEnabled: { type: 'boolean', description: 'Enable spatial mapping.' },
        spatialMappingQuality: { type: 'string', enum: ['low', 'medium', 'high'], description: 'Spatial mapping quality.' },
        trianglesPerCubicMeter: { type: 'number', description: 'Target triangle density.' },
        sceneUnderstandingEnabled: { type: 'boolean', description: 'Enable scene understanding.' },
        sceneQueryTypes: {
          type: 'array',
          items: { type: 'string', enum: ['wall', 'floor', 'ceiling', 'platform', 'background', 'world'] },
          description: 'Scene object types to query.'
        },
        qrTrackingEnabled: { type: 'boolean', description: 'Enable QR code tracking.' },
        worldAnchorName: { type: 'string', description: 'World anchor name.' },
        worldAnchorStore: { type: 'string', description: 'World anchor store name.' },
        voiceCommand: { type: 'string', description: 'Voice command phrase.' },
        voiceCommandId: { type: 'string', description: 'Voice command identifier.' },

        // === Device Priority ===
        devicePriority: {
          type: 'array',
          items: { type: 'string' },
          description: 'XR device priority order.'
        },

        // === Spectator ===
        spectatorEnabled: { type: 'boolean', description: 'Enable spectator screen.' },
        spectatorMode: { type: 'string', enum: ['disabled', 'singleEye', 'texture', 'mirror'], description: 'Spectator mode.' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean', description: 'Whether the operation succeeded.' },
        message: { type: 'string', description: 'Status message.' },

        // OpenXR outputs
        openxrInfo: {
          type: 'object',
          properties: {
            available: { type: 'boolean' },
            runtimeName: { type: 'string' },
            runtimeVersion: { type: 'string' },
            apiVersion: { type: 'string' },
            extensions: { type: 'array', items: { type: 'string' } }
          },
          description: 'OpenXR runtime info.'
        },
        trackingOrigin: { type: 'string', description: 'Current tracking origin.' },
        actionSetId: { type: 'string', description: 'Created action set ID.' },
        actionId: { type: 'string', description: 'Created action ID.' },
        actionState: {
          type: 'object',
          properties: {
            isActive: { type: 'boolean' },
            currentState: { type: 'number' },
            changedSinceLastSync: { type: 'boolean' }
          },
          description: 'XR action state.'
        },
        hmdPose: {
          type: 'object',
          properties: {
            position: { type: 'object', properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } } },
            rotation: { type: 'object', properties: { pitch: { type: 'number' }, yaw: { type: 'number' }, roll: { type: 'number' } } },
            isTracking: { type: 'boolean' }
          },
          description: 'HMD pose.'
        },
        controllerPose: {
          type: 'object',
          properties: {
            position: { type: 'object', properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } } },
            rotation: { type: 'object', properties: { pitch: { type: 'number' }, yaw: { type: 'number' }, roll: { type: 'number' } } },
            isTracking: { type: 'boolean' }
          },
          description: 'Controller pose.'
        },
        handTrackingData: {
          type: 'object',
          properties: {
            isTracking: { type: 'boolean' },
            jointCount: { type: 'number' },
            confidence: { type: 'number' }
          },
          description: 'Hand tracking data.'
        },
        eyeTrackingData: {
          type: 'object',
          properties: {
            isTracking: { type: 'boolean' },
            gazeDirection: { type: 'object', properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } } },
            fixationPoint: { type: 'object', properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } } }
          },
          description: 'Eye tracking data.'
        },
        viewConfiguration: {
          type: 'object',
          properties: {
            viewCount: { type: 'number' },
            recommendedWidth: { type: 'number' },
            recommendedHeight: { type: 'number' }
          },
          description: 'View configuration.'
        },
        supportedExtensions: { type: 'array', items: { type: 'string' }, description: 'Supported OpenXR extensions.' },

        // Quest outputs
        questInfo: {
          type: 'object',
          properties: {
            available: { type: 'boolean' },
            deviceType: { type: 'string' },
            handTrackingSupported: { type: 'boolean' },
            faceTrackingSupported: { type: 'boolean' },
            bodyTrackingSupported: { type: 'boolean' },
            passthroughSupported: { type: 'boolean' }
          },
          description: 'Quest device info.'
        },
        sceneAnchors: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              id: { type: 'string' },
              label: { type: 'string' },
              position: { type: 'object' }
            }
          },
          description: 'Scene anchors.'
        },
        roomLayout: {
          type: 'object',
          properties: {
            floorUuid: { type: 'string' },
            ceilingUuid: { type: 'string' },
            wallUuids: { type: 'array', items: { type: 'string' } }
          },
          description: 'Room layout.'
        },
        handPose: {
          type: 'object',
          properties: {
            isTracking: { type: 'boolean' },
            pinchStrength: { type: 'number' },
            pointerPose: { type: 'object' }
          },
          description: 'Quest hand pose.'
        },
        faceState: {
          type: 'object',
          properties: {
            isTracking: { type: 'boolean' },
            expressionWeights: { type: 'object' }
          },
          description: 'Quest face state.'
        },
        bodyState: {
          type: 'object',
          properties: {
            isTracking: { type: 'boolean' },
            jointCount: { type: 'number' },
            confidence: { type: 'number' }
          },
          description: 'Quest body state.'
        },
        spatialAnchorId: { type: 'string', description: 'Created spatial anchor ID.' },
        loadedAnchors: { type: 'array', items: { type: 'string' }, description: 'Loaded anchor IDs.' },
        guardianGeometry: {
          type: 'object',
          properties: {
            pointCount: { type: 'number' },
            dimensions: { type: 'object' }
          },
          description: 'Guardian geometry.'
        },

        // SteamVR outputs
        steamvrInfo: {
          type: 'object',
          properties: {
            available: { type: 'boolean' },
            runtimeVersion: { type: 'string' },
            hmdPresent: { type: 'boolean' },
            trackedDeviceCount: { type: 'number' }
          },
          description: 'SteamVR info.'
        },
        chaperoneGeometry: {
          type: 'object',
          properties: {
            playAreaSize: { type: 'object' },
            boundaryPoints: { type: 'array' }
          },
          description: 'Chaperone geometry.'
        },
        overlayHandle: { type: 'string', description: 'Created overlay handle.' },
        trackedDeviceCount: { type: 'number', description: 'Number of tracked devices.' },
        trackedDeviceInfo: {
          type: 'object',
          properties: {
            index: { type: 'number' },
            class: { type: 'string' },
            serialNumber: { type: 'string' },
            isConnected: { type: 'boolean' }
          },
          description: 'Tracked device info.'
        },
        lighthouseInfo: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              serialNumber: { type: 'string' },
              mode: { type: 'string' },
              position: { type: 'object' }
            }
          },
          description: 'Lighthouse base station info.'
        },
        skeletalBoneData: {
          type: 'object',
          properties: {
            boneCount: { type: 'number' },
            isTracking: { type: 'boolean' }
          },
          description: 'Skeletal bone data.'
        },

        // ARKit outputs
        arkitInfo: {
          type: 'object',
          properties: {
            available: { type: 'boolean' },
            worldTrackingSupported: { type: 'boolean' },
            faceTrackingSupported: { type: 'boolean' },
            bodyTrackingSupported: { type: 'boolean' },
            sceneReconstructionSupported: { type: 'boolean' }
          },
          description: 'ARKit info.'
        },
        trackedPlanes: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              id: { type: 'string' },
              classification: { type: 'string' },
              center: { type: 'object' },
              extent: { type: 'object' }
            }
          },
          description: 'Tracked planes.'
        },
        trackedImages: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              name: { type: 'string' },
              isTracking: { type: 'boolean' },
              transform: { type: 'object' }
            }
          },
          description: 'Tracked images.'
        },
        faceBlendshapes: {
          type: 'object',
          description: 'Face blendshape weights.'
        },
        faceGeometry: {
          type: 'object',
          properties: {
            vertexCount: { type: 'number' },
            triangleCount: { type: 'number' }
          },
          description: 'Face geometry.'
        },
        bodySkeleton: {
          type: 'object',
          properties: {
            isTracking: { type: 'boolean' },
            jointCount: { type: 'number' }
          },
          description: 'Body skeleton.'
        },
        arkitAnchorId: { type: 'string', description: 'Created ARKit anchor ID.' },
        lightEstimation: {
          type: 'object',
          properties: {
            ambientIntensity: { type: 'number' },
            ambientColorTemperature: { type: 'number' }
          },
          description: 'Light estimation.'
        },
        sceneMesh: {
          type: 'object',
          properties: {
            vertexCount: { type: 'number' },
            faceCount: { type: 'number' }
          },
          description: 'Scene mesh.'
        },
        raycastResults: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              type: { type: 'string' },
              distance: { type: 'number' },
              worldPosition: { type: 'object' }
            }
          },
          description: 'Raycast results.'
        },
        cameraIntrinsics: {
          type: 'object',
          properties: {
            focalLength: { type: 'object' },
            principalPoint: { type: 'object' },
            imageResolution: { type: 'object' }
          },
          description: 'Camera intrinsics.'
        },

        // ARCore outputs
        arcoreInfo: {
          type: 'object',
          properties: {
            available: { type: 'boolean' },
            depthSupported: { type: 'boolean' },
            geospatialSupported: { type: 'boolean' }
          },
          description: 'ARCore info.'
        },
        arcorePlanes: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              id: { type: 'string' },
              type: { type: 'string' },
              centerPose: { type: 'object' },
              extentX: { type: 'number' },
              extentZ: { type: 'number' }
            }
          },
          description: 'ARCore planes.'
        },
        arcorePoints: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              id: { type: 'string' },
              position: { type: 'object' },
              confidence: { type: 'number' }
            }
          },
          description: 'ARCore feature points.'
        },
        arcoreAnchorId: { type: 'string', description: 'Created ARCore anchor ID.' },
        depthImage: {
          type: 'object',
          properties: {
            width: { type: 'number' },
            height: { type: 'number' },
            format: { type: 'string' }
          },
          description: 'Depth image info.'
        },
        geospatialPose: {
          type: 'object',
          properties: {
            latitude: { type: 'number' },
            longitude: { type: 'number' },
            altitude: { type: 'number' },
            heading: { type: 'number' },
            horizontalAccuracy: { type: 'number' },
            verticalAccuracy: { type: 'number' }
          },
          description: 'Geospatial pose.'
        },
        geospatialAnchorId: { type: 'string', description: 'Created geospatial anchor ID.' },
        cloudAnchorId: { type: 'string', description: 'Hosted cloud anchor ID.' },

        // Varjo outputs
        varjoInfo: {
          type: 'object',
          properties: {
            available: { type: 'boolean' },
            deviceType: { type: 'string' },
            eyeTrackingSupported: { type: 'boolean' },
            passthroughSupported: { type: 'boolean' },
            mixedRealitySupported: { type: 'boolean' }
          },
          description: 'Varjo info.'
        },
        varjoGazeData: {
          type: 'object',
          properties: {
            isTracking: { type: 'boolean' },
            leftEye: { type: 'object' },
            rightEye: { type: 'object' },
            combinedGaze: { type: 'object' },
            focusDistance: { type: 'number' }
          },
          description: 'Varjo gaze data.'
        },
        varjoCameraIntrinsics: {
          type: 'object',
          properties: {
            focalLength: { type: 'object' },
            principalPoint: { type: 'object' }
          },
          description: 'Varjo camera intrinsics.'
        },
        varjoEnvironmentCubemap: {
          type: 'object',
          properties: {
            available: { type: 'boolean' },
            resolution: { type: 'number' }
          },
          description: 'Environment cubemap info.'
        },

        // HoloLens outputs
        hololensInfo: {
          type: 'object',
          properties: {
            available: { type: 'boolean' },
            spatialMappingSupported: { type: 'boolean' },
            sceneUnderstandingSupported: { type: 'boolean' },
            handTrackingSupported: { type: 'boolean' },
            eyeTrackingSupported: { type: 'boolean' }
          },
          description: 'HoloLens info.'
        },
        spatialMesh: {
          type: 'object',
          properties: {
            surfaceCount: { type: 'number' },
            totalVertices: { type: 'number' },
            totalTriangles: { type: 'number' }
          },
          description: 'Spatial mesh info.'
        },
        sceneObjects: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              kind: { type: 'string' },
              position: { type: 'object' },
              extents: { type: 'object' }
            }
          },
          description: 'Scene understanding objects.'
        },
        trackedQRCodes: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              id: { type: 'string' },
              data: { type: 'string' },
              size: { type: 'number' },
              position: { type: 'object' }
            }
          },
          description: 'Tracked QR codes.'
        },
        worldAnchorId: { type: 'string', description: 'Created world anchor ID.' },
        loadedWorldAnchors: { type: 'array', items: { type: 'string' }, description: 'Loaded world anchor names.' },
        hololensHandMesh: {
          type: 'object',
          properties: {
            isTracking: { type: 'boolean' },
            vertexCount: { type: 'number' },
            indexCount: { type: 'number' }
          },
          description: 'HoloLens hand mesh.'
        },
        hololensGazeRay: {
          type: 'object',
          properties: {
            origin: { type: 'object' },
            direction: { type: 'object' },
            isTracking: { type: 'boolean' }
          },
          description: 'HoloLens gaze ray.'
        },
        registeredVoiceCommands: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              id: { type: 'string' },
              phrase: { type: 'string' }
            }
          },
          description: 'Registered voice commands.'
        },

        // Utility outputs
        xrSystemInfo: {
          type: 'object',
          properties: {
            hmdConnected: { type: 'boolean' },
            hmdName: { type: 'string' },
            trackingSystemName: { type: 'string' },
            stereoRenderingMode: { type: 'string' }
          },
          description: 'XR system info.'
        },
        xrDevices: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              name: { type: 'string' },
              type: { type: 'string' },
              isConnected: { type: 'boolean' },
              priority: { type: 'number' }
            }
          },
          description: 'Available XR devices.'
        },
        xrRuntimeName: { type: 'string', description: 'Active XR runtime name.' }
      }
    }
  },

  // ============================================
  // Phase 42: AI & NPC Plugins - manage_ai_npc
  // ============================================
  {
    name: 'manage_ai_npc',
    category: 'gameplay',
    description: `AI & NPC Plugins management - Convai, Inworld AI, NVIDIA ACE (Audio2Face).
Supports ~30 actions across 3 AI NPC subsystems for conversational AI characters.

CONVAI (10 actions): Character creation, backstory configuration, voice settings, lipsync, sessions
INWORLD AI (10 actions): Character creation, scene configuration, emotions, goals, sessions
NVIDIA ACE (8 actions): Audio2Face configuration, blendshape processing, streaming, emotion control
UTILITIES (2 actions): System info, backend listing`,
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            // Convai (10 actions)
            'create_convai_character', 'configure_character_backstory', 'configure_character_voice',
            'configure_convai_lipsync', 'start_convai_session', 'stop_convai_session',
            'send_text_to_character', 'get_character_response', 'configure_convai_actions', 'get_convai_info',
            // Inworld AI (10 actions)
            'create_inworld_character', 'configure_inworld_settings', 'configure_inworld_scene',
            'start_inworld_session', 'stop_inworld_session', 'send_message_to_character',
            'get_character_emotion', 'get_character_goals', 'trigger_inworld_event', 'get_inworld_info',
            // NVIDIA ACE (8 actions)
            'configure_audio2face', 'process_audio_to_blendshapes', 'configure_blendshape_mapping',
            'start_audio2face_stream', 'stop_audio2face_stream', 'get_audio2face_status',
            'configure_ace_emotions', 'get_ace_info',
            // Utilities (2 actions)
            'get_ai_npc_info', 'list_available_ai_backends'
          ],
          description: 'AI NPC action to perform.'
        },

        // === Common Parameters ===
        actorName: commonSchemas.actorName,
        componentName: commonSchemas.componentName,
        blueprintPath: commonSchemas.blueprintPath,
        save: commonSchemas.save,

        // === Character Identity ===
        characterId: { type: 'string', description: 'Character ID from Convai or Inworld.' },
        characterName: { type: 'string', description: 'Display name for the character.' },
        backstory: { type: 'string', description: 'Character backstory/personality prompt.' },
        role: { type: 'string', description: 'Character role (e.g., "Guard", "Merchant").' },
        description: { type: 'string', description: 'Character description.' },

        // === Convai Voice Settings ===
        voiceType: { type: 'string', description: 'Voice type (e.g., "Male", "Female", or specific voice ID).' },
        voiceId: { type: 'string', description: 'Specific voice ID for the character.' },
        language: { type: 'string', description: 'Language code (e.g., "en-US", "es-ES").' },
        speechRate: { type: 'number', description: 'Speech rate multiplier (0.5-2.0).' },
        pitch: { type: 'number', description: 'Voice pitch adjustment (-1.0 to 1.0).' },

        // === Convai Lipsync ===
        lipsyncEnabled: { type: 'boolean', description: 'Enable lipsync processing.' },
        lipsyncMode: { type: 'string', enum: ['viseme', 'blendshape', 'arkit'], description: 'Lipsync output mode.' },
        visemeMultiplier: { type: 'number', description: 'Viseme intensity multiplier.' },
        blendshapeParams: {
          type: 'object',
          properties: {
            jawOpenMultiplier: { type: 'number' },
            lipFunnelMultiplier: { type: 'number' },
            lipPuckerMultiplier: { type: 'number' },
            mouthSmileMultiplier: { type: 'number' }
          },
          description: 'Blendshape parameter overrides.'
        },

        // === Convai Actions ===
        availableActions: {
          type: 'array',
          items: { type: 'string' },
          description: 'Actions the character can perform (e.g., "wave", "point", "attack").'
        },
        actionContext: { type: 'string', description: 'Context description for action selection.' },

        // === Session Management ===
        sessionId: { type: 'string', description: 'Session ID for conversation tracking.' },
        autoStartSession: { type: 'boolean', description: 'Automatically start session on component begin play.' },
        sessionTimeout: { type: 'number', description: 'Session timeout in seconds.' },

        // === Text/Message Input ===
        message: { type: 'string', description: 'Text message to send to the character.' },
        textInput: { type: 'string', description: 'Text input for the character.' },
        speakerName: { type: 'string', description: 'Name of the speaker sending the message.' },

        // === Inworld Settings ===
        sceneId: { type: 'string', description: 'Inworld scene/workspace ID.' },
        apiKey: { type: 'string', description: 'Inworld API key.' },
        apiSecret: { type: 'string', description: 'Inworld API secret.' },

        // === Inworld Character Profile ===
        characterProfile: {
          type: 'object',
          properties: {
            name: { type: 'string' },
            role: { type: 'string' },
            pronouns: { type: 'string' },
            age: { type: 'string' },
            hobbies: { type: 'string' },
            motivation: { type: 'string' },
            flaws: { type: 'string' },
            facts: { type: 'array', items: { type: 'string' } }
          },
          description: 'Inworld character profile configuration.'
        },

        // === Inworld Emotions ===
        emotionLabel: {
          type: 'string',
          enum: [
            'NEUTRAL', 'JOY', 'SADNESS', 'ANGER', 'FEAR', 'SURPRISE', 'DISGUST', 'CONTEMPT',
            'BELLIGERENCE', 'DOMINEERING', 'CRITICISM', 'TENSION', 'TENSE_HUMOR', 'DEFENSIVENESS',
            'WHINING', 'STONEWALLING', 'INTEREST', 'VALIDATION', 'HUMOR', 'AFFECTION'
          ],
          description: 'Emotion label for the character.'
        },
        emotionStrength: { type: 'number', description: 'Emotion intensity (0.0-1.0).' },

        // === Inworld Goals ===
        goalName: { type: 'string', description: 'Character goal name.' },
        goalPriority: { type: 'number', description: 'Goal priority (1-10).' },
        goalDescription: { type: 'string', description: 'Goal description.' },

        // === Inworld Events ===
        eventName: commonSchemas.eventName,
        eventPayload: {
          type: 'object',
          additionalProperties: true,
          description: 'Event payload data.'
        },

        // === Inworld Relationship ===
        relationship: {
          type: 'object',
          properties: {
            trust: { type: 'number', description: 'Trust level (-100 to 100).' },
            respect: { type: 'number', description: 'Respect level (-100 to 100).' },
            familiar: { type: 'number', description: 'Familiarity level (-100 to 100).' },
            flirtatious: { type: 'number', description: 'Flirtatiousness (-100 to 100).' },
            attraction: { type: 'number', description: 'Attraction level (-100 to 100).' }
          },
          description: 'Character relationship parameters.'
        },

        // === NVIDIA ACE / Audio2Face ===
        aceProviderName: { type: 'string', description: 'ACE provider name (e.g., "LocalA2F-Claire", "RemoteA2F").' },
        aceDestUrl: { type: 'string', description: 'ACE destination URL (e.g., "https://grpc.nvcf.nvidia.com:443").' },
        aceApiKey: { type: 'string', description: 'NVIDIA API key for cloud ACE.' },
        nvcfFunctionId: { type: 'string', description: 'NVCF function ID for cloud ACE.' },
        nvcfFunctionVersion: { type: 'string', description: 'NVCF function version.' },

        // === Audio2Face Audio Input ===
        soundWavePath: { type: 'string', description: 'Path to SoundWave asset for A2F processing.' },
        audioSampleRate: { type: 'number', description: 'Audio sample rate in Hz.' },
        audioChannels: { type: 'number', description: 'Number of audio channels.' },
        isLastAudioChunk: { type: 'boolean', description: 'Whether this is the last audio chunk in stream.' },

        // === Audio2Face Blendshape Mapping ===
        blendshapeMapping: {
          type: 'object',
          additionalProperties: { type: 'string' },
          description: 'Custom blendshape name mapping (ARKit name -> mesh blendshape name).'
        },
        blendshapeMultipliers: {
          type: 'object',
          additionalProperties: { type: 'number' },
          description: 'Per-blendshape intensity multipliers.'
        },

        // === Audio2Face Emotion Control ===
        a2fEmotion: {
          type: 'object',
          properties: {
            joy: { type: 'number', description: 'Joy weight (0.0-1.0).' },
            sadness: { type: 'number', description: 'Sadness weight (0.0-1.0).' },
            anger: { type: 'number', description: 'Anger weight (0.0-1.0).' },
            fear: { type: 'number', description: 'Fear weight (0.0-1.0).' },
            surprise: { type: 'number', description: 'Surprise weight (0.0-1.0).' },
            disgust: { type: 'number', description: 'Disgust weight (0.0-1.0).' }
          },
          description: 'Audio2Face emotion override weights.'
        },
        a2fParams: {
          type: 'object',
          properties: {
            skinStrength: { type: 'number', description: 'Skin deformation strength.' },
            blinkStrength: { type: 'number', description: 'Blink animation strength.' },
            lipSyncStrength: { type: 'number', description: 'Lip sync strength.' },
            browStrength: { type: 'number', description: 'Eyebrow movement strength.' },
            eyelidOpenOffset: { type: 'number', description: 'Eyelid open offset.' }
          },
          description: 'Audio2Face model parameters.'
        },

        // === MetaHuman Integration ===
        isMetaHuman: { type: 'boolean', description: 'Whether the target is a MetaHuman character.' },
        faceAnimBPPath: { type: 'string', description: 'Path to Face_AnimBP for MetaHuman.' },
        useA2FPoseAsset: { type: 'boolean', description: 'Use mh_arkit_mapping_pose_A2F for MetaHuman.' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean', description: 'Whether the operation succeeded.' },
        message: { type: 'string', description: 'Status message.' },

        // === Character Creation Outputs ===
        characterId: { type: 'string', description: 'Created/referenced character ID.' },
        componentAdded: { type: 'boolean', description: 'Whether component was added to actor.' },

        // === Session Outputs ===
        sessionId: { type: 'string', description: 'Active session ID.' },
        sessionActive: { type: 'boolean', description: 'Whether session is active.' },

        // === Character Response ===
        responseText: { type: 'string', description: 'Character response text.' },
        responseAudioPath: { type: 'string', description: 'Path to generated response audio.' },
        selectedAction: { type: 'string', description: 'Action selected by the character.' },

        // === Emotion Outputs ===
        currentEmotion: { type: 'string', description: 'Current emotion label.' },
        emotionStrength: { type: 'number', description: 'Current emotion strength.' },
        emotionWeights: {
          type: 'object',
          additionalProperties: { type: 'number' },
          description: 'All emotion weights.'
        },

        // === Goals Outputs ===
        activeGoals: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              name: { type: 'string' },
              priority: { type: 'number' },
              progress: { type: 'number' }
            }
          },
          description: 'Active character goals.'
        },

        // === Audio2Face Outputs ===
        a2fProcessing: { type: 'boolean', description: 'Whether A2F is processing.' },
        blendshapeCount: { type: 'number', description: 'Number of blendshapes being driven.' },
        currentBlendshapes: {
          type: 'object',
          additionalProperties: { type: 'number' },
          description: 'Current blendshape values.'
        },

        // === Convai Info ===
        convaiInfo: {
          type: 'object',
          properties: {
            available: { type: 'boolean' },
            moduleVersion: { type: 'string' },
            connectedCharacters: { type: 'number' },
            activeSessions: { type: 'number' },
            lipsyncEnabled: { type: 'boolean' }
          },
          description: 'Convai plugin info.'
        },

        // === Inworld Info ===
        inworldInfo: {
          type: 'object',
          properties: {
            available: { type: 'boolean' },
            connected: { type: 'boolean' },
            activeSceneId: { type: 'string' },
            registeredCharacters: { type: 'number' },
            activeConversations: { type: 'number' }
          },
          description: 'Inworld AI plugin info.'
        },

        // === ACE Info ===
        aceInfo: {
          type: 'object',
          properties: {
            available: { type: 'boolean' },
            runtimeLoaded: { type: 'boolean' },
            providers: { type: 'array', items: { type: 'string' } },
            activeStreams: { type: 'number' },
            gpuAccelerated: { type: 'boolean' }
          },
          description: 'NVIDIA ACE plugin info.'
        },

        // === Backend List ===
        availableBackends: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              name: { type: 'string' },
              type: { type: 'string', enum: ['convai', 'inworld', 'ace'] },
              available: { type: 'boolean' },
              version: { type: 'string' }
            }
          },
          description: 'Available AI NPC backends.'
        },

        // === AI NPC Info (combined) ===
        aiNpcInfo: {
          type: 'object',
          properties: {
            actorName: { type: 'string' },
            hasConvaiComponent: { type: 'boolean' },
            hasInworldComponent: { type: 'boolean' },
            hasACEComponent: { type: 'boolean' },
            activeBackend: { type: 'string' },
            characterId: { type: 'string' },
            sessionActive: { type: 'boolean' },
            currentEmotion: { type: 'string' }
          },
          description: 'AI NPC configuration info for a specific actor.'
        },

        error: commonSchemas.stringProp
      }
    }
  },

  // ============================================
  // Phase 43: Utility Plugins - manage_utility_plugins
  // ============================================
  {
    name: 'manage_utility_plugins',
    category: 'utility',
    description: `Utility Plugins management - Python Scripting, Editor Scripting, Modeling Tools, Common UI, Paper2D, Procedural Mesh, Variant Manager.
Supports ~100 actions across 7 utility plugin subsystems.

PYTHON SCRIPTING (15 actions): Execute scripts, configure paths, create editor utilities
EDITOR SCRIPTING (12 actions): Editor utility widgets, menu entries, toolbar buttons
MODELING TOOLS (18 actions): Activate tools, mesh selection, sculpt brushes, geometry ops
COMMON UI (10 actions): UI input config, activatable widgets, navigation rules
PAPER2D (12 actions): Sprites, flipbooks, tile maps, sprite actors
PROCEDURAL MESH (15 actions): Procedural mesh components, sections, vertices, conversion
VARIANT MANAGER (15 actions): Variant sets, variants, activation, property captures
UTILITIES (3 actions): System info, plugin listing`,
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            // Python Scripting (15 actions)
            'execute_python_script', 'execute_python_file', 'execute_python_command',
            'configure_python_paths', 'add_python_path', 'remove_python_path',
            'get_python_paths', 'create_python_editor_utility', 'run_startup_scripts',
            'get_python_output', 'clear_python_output', 'is_python_available',
            'get_python_version', 'reload_python_module', 'get_python_info',
            // Editor Scripting (12 actions)
            'create_editor_utility_widget', 'create_editor_utility_blueprint',
            'add_menu_entry', 'remove_menu_entry', 'add_toolbar_button', 'remove_toolbar_button',
            'register_editor_command', 'unregister_editor_command', 'execute_editor_command',
            'create_blutility_action', 'run_editor_utility', 'get_editor_scripting_info',
            // Modeling Tools (18 actions)
            'activate_modeling_tool', 'deactivate_modeling_tool', 'get_active_tool',
            'select_mesh_elements', 'clear_mesh_selection', 'get_mesh_selection',
            'set_sculpt_brush', 'configure_sculpt_brush', 'execute_sculpt_stroke',
            'apply_mesh_operation', 'undo_mesh_operation', 'accept_tool_result',
            'cancel_tool', 'set_tool_property', 'get_tool_properties',
            'list_available_tools', 'enter_modeling_mode', 'get_modeling_tools_info',
            // Common UI (10 actions)
            'configure_ui_input_config', 'create_common_activatable_widget',
            'configure_navigation_rules', 'set_input_action_data', 'get_ui_input_config',
            'register_common_input_metadata', 'configure_gamepad_navigation',
            'set_default_focus_widget', 'configure_analog_cursor', 'get_common_ui_info',
            // Paper2D (12 actions)
            'create_sprite', 'create_flipbook', 'add_flipbook_keyframe',
            'create_tile_map', 'create_tile_set', 'set_tile_map_layer',
            'spawn_paper_sprite_actor', 'spawn_paper_flipbook_actor',
            'configure_sprite_collision', 'configure_sprite_material',
            'get_sprite_info', 'get_paper2d_info',
            // Procedural Mesh (15 actions)
            'create_procedural_mesh_component', 'create_mesh_section',
            'update_mesh_section', 'clear_mesh_section', 'clear_all_mesh_sections',
            'set_mesh_section_visible', 'set_mesh_collision',
            'set_mesh_vertices', 'set_mesh_triangles', 'set_mesh_normals',
            'set_mesh_uvs', 'set_mesh_colors', 'set_mesh_tangents',
            'convert_procedural_to_static_mesh', 'get_procedural_mesh_info',
            // Variant Manager (15 actions)
            'create_level_variant_sets', 'create_variant_set', 'delete_variant_set',
            'add_variant', 'remove_variant', 'duplicate_variant',
            'activate_variant', 'deactivate_variant', 'get_active_variant',
            'add_actor_binding', 'remove_actor_binding', 'capture_property',
            'configure_variant_dependency', 'export_variant_configuration',
            'get_variant_manager_info',
            // Utilities (3 actions)
            'get_utility_plugins_info', 'list_utility_plugins', 'get_plugin_status'
          ],
          description: 'Utility plugin action to perform.'
        },

        // === Common Parameters ===
        actorName: commonSchemas.actorName,
        componentName: commonSchemas.componentName,
        blueprintPath: commonSchemas.blueprintPath,
        assetPath: commonSchemas.assetPath,
        save: commonSchemas.save,

        // === Python Scripting ===
        pythonScript: { type: 'string', description: 'Python script content to execute.' },
        pythonFilePath: { type: 'string', description: 'Path to Python file to execute.' },
        pythonCommand: { type: 'string', description: 'Python command/statement to execute.' },
        pythonPaths: { type: 'array', items: { type: 'string' }, description: 'Python paths to add to sys.path.' },
        pythonPath: { type: 'string', description: 'Single Python path to add/remove.' },
        moduleName: { type: 'string', description: 'Python module name to reload.' },
        executionMode: {
          type: 'string',
          enum: ['execute_file', 'execute_statement', 'evaluate_statement'],
          description: 'Python execution mode.'
        },
        captureOutput: { type: 'boolean', description: 'Capture Python stdout/stderr.' },

        // === Editor Scripting ===
        widgetName: { type: 'string', description: 'Editor utility widget name.' },
        widgetClass: { type: 'string', description: 'Widget class to create.' },
        menuName: { type: 'string', description: 'Menu name for entry.' },
        menuSection: { type: 'string', description: 'Menu section.' },
        menuLabel: { type: 'string', description: 'Menu entry label.' },
        menuTooltip: { type: 'string', description: 'Menu entry tooltip.' },
        menuIcon: { type: 'string', description: 'Menu icon style name.' },
        toolbarName: { type: 'string', description: 'Toolbar name.' },
        buttonLabel: { type: 'string', description: 'Toolbar button label.' },
        buttonTooltip: { type: 'string', description: 'Toolbar button tooltip.' },
        buttonIcon: { type: 'string', description: 'Toolbar button icon.' },
        commandName: { type: 'string', description: 'Editor command name.' },
        commandDescription: { type: 'string', description: 'Editor command description.' },
        blutilityClass: { type: 'string', description: 'Blutility action class.' },

        // === Modeling Tools ===
        toolName: { type: 'string', description: 'Modeling tool name (e.g., "PolyEdit", "Sculpt", "TriEdit").' },
        toolIdentifier: { type: 'string', description: 'Full tool identifier.' },
        selectionMode: {
          type: 'string',
          enum: ['vertex', 'edge', 'face', 'polygroup', 'triangle'],
          description: 'Mesh element selection mode.'
        },
        elementIndices: { type: 'array', items: { type: 'number' }, description: 'Indices of elements to select.' },
        brushType: {
          type: 'string',
          enum: ['standard', 'smooth', 'move', 'pinch', 'inflate', 'flatten', 'plane_cut'],
          description: 'Sculpt brush type.'
        },
        brushRadius: { type: 'number', description: 'Sculpt brush radius.' },
        brushStrength: { type: 'number', description: 'Sculpt brush strength (0.0-1.0).' },
        brushFalloff: { type: 'number', description: 'Sculpt brush falloff.' },
        strokeStart: {
          type: 'object',
          properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } },
          description: 'Sculpt stroke start position.'
        },
        strokeEnd: {
          type: 'object',
          properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } },
          description: 'Sculpt stroke end position.'
        },
        meshOperation: {
          type: 'string',
          enum: ['extrude', 'inset', 'bevel', 'bridge', 'fill_hole', 'weld', 'delete', 'flip_normals'],
          description: 'Mesh operation to apply.'
        },
        operationParams: { type: 'object', additionalProperties: true, description: 'Mesh operation parameters.' },
        toolPropertyName: { type: 'string', description: 'Tool property name to set.' },
        toolPropertyValue: { type: ['string', 'number', 'boolean', 'object'], description: 'Tool property value.' },

        // === Common UI ===
        inputConfigName: { type: 'string', description: 'UI input config name.' },
        inputConfigClass: { type: 'string', description: 'UI input config class.' },
        navigationRules: {
          type: 'object',
          properties: {
            wrapHorizontal: { type: 'boolean' },
            wrapVertical: { type: 'boolean' },
            explicitNavigation: { type: 'boolean' }
          },
          description: 'Navigation rule settings.'
        },
        inputActionData: {
          type: 'object',
          properties: {
            actionName: { type: 'string' },
            keyMappings: { type: 'array', items: { type: 'string' } }
          },
          description: 'Input action data configuration.'
        },
        focusWidgetPath: { type: 'string', description: 'Path to default focus widget.' },
        analogCursorSettings: {
          type: 'object',
          properties: {
            enabled: { type: 'boolean' },
            speed: { type: 'number' },
            deadzone: { type: 'number' }
          },
          description: 'Analog cursor settings.'
        },

        // === Paper2D ===
        spriteName: { type: 'string', description: 'Sprite asset name.' },
        texturePath: { type: 'string', description: 'Source texture path.' },
        sourceRect: {
          type: 'object',
          properties: { x: { type: 'number' }, y: { type: 'number' }, width: { type: 'number' }, height: { type: 'number' } },
          description: 'Source rectangle in texture.'
        },
        pixelsPerUnit: { type: 'number', description: 'Pixels per Unreal unit.' },
        flipbookName: { type: 'string', description: 'Flipbook asset name.' },
        frameRate: { type: 'number', description: 'Flipbook frame rate.' },
        keyframeIndex: { type: 'number', description: 'Keyframe index.' },
        keyframeDuration: { type: 'number', description: 'Keyframe duration.' },
        spriteAsset: { type: 'string', description: 'Sprite asset path for keyframe.' },
        tileMapName: { type: 'string', description: 'Tile map asset name.' },
        tileSetName: { type: 'string', description: 'Tile set asset name.' },
        mapWidth: { type: 'number', description: 'Tile map width in tiles.' },
        mapHeight: { type: 'number', description: 'Tile map height in tiles.' },
        tileWidth: { type: 'number', description: 'Tile width in pixels.' },
        tileHeight: { type: 'number', description: 'Tile height in pixels.' },
        layerIndex: { type: 'number', description: 'Tile map layer index.' },
        tileData: {
          type: 'array',
          items: {
            type: 'object',
            properties: { x: { type: 'number' }, y: { type: 'number' }, tileIndex: { type: 'number' } }
          },
          description: 'Tile placement data.'
        },
        collisionType: {
          type: 'string',
          enum: ['none', 'box', 'circle', 'polygon'],
          description: 'Sprite collision type.'
        },
        location: commonSchemas.location,

        // === Procedural Mesh ===
        sectionIndex: { type: 'number', description: 'Mesh section index.' },
        vertices: {
          type: 'array',
          items: {
            type: 'object',
            properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } }
          },
          description: 'Vertex positions.'
        },
        triangles: {
          type: 'array',
          items: { type: 'number' },
          description: 'Triangle indices (groups of 3).'
        },
        normals: {
          type: 'array',
          items: {
            type: 'object',
            properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } }
          },
          description: 'Vertex normals.'
        },
        uvs: {
          type: 'array',
          items: {
            type: 'object',
            properties: { u: { type: 'number' }, v: { type: 'number' } }
          },
          description: 'UV coordinates.'
        },
        vertexColors: {
          type: 'array',
          items: {
            type: 'object',
            properties: { r: { type: 'number' }, g: { type: 'number' }, b: { type: 'number' }, a: { type: 'number' } }
          },
          description: 'Vertex colors.'
        },
        tangents: {
          type: 'array',
          items: {
            type: 'object',
            properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' }, w: { type: 'number' } }
          },
          description: 'Vertex tangents.'
        },
        createCollision: { type: 'boolean', description: 'Generate collision for mesh.' },
        staticMeshPath: { type: 'string', description: 'Output static mesh asset path.' },

        // === Variant Manager ===
        variantSetsName: { type: 'string', description: 'Level variant sets asset name.' },
        variantSetName: { type: 'string', description: 'Variant set name.' },
        variantName: { type: 'string', description: 'Variant name.' },
        variantDisplayText: { type: 'string', description: 'Variant display text.' },
        targetActorName: { type: 'string', description: 'Target actor for binding.' },
        propertyPath: { type: 'string', description: 'Property path to capture.' },
        propertyValue: { type: ['string', 'number', 'boolean', 'object'], description: 'Property value to set.' },
        dependencyVariant: { type: 'string', description: 'Variant dependency name.' },
        dependencyCondition: {
          type: 'string',
          enum: ['enable', 'disable'],
          description: 'Dependency condition.'
        },
        exportPath: { type: 'string', description: 'Export file path.' },
        exportFormat: {
          type: 'string',
          enum: ['json', 'csv'],
          description: 'Export format.'
        },

        // === Plugin Query ===
        pluginName: { type: 'string', description: 'Plugin name to query.' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        ...commonSchemas.outputBase,
        message: { type: 'string', description: 'Status message.' },

        // === Python Outputs ===
        pythonOutput: { type: 'string', description: 'Python script output.' },
        pythonError: { type: 'string', description: 'Python error message if any.' },
        pythonResult: { type: 'string', description: 'Python evaluation result.' },
        pythonVersion: { type: 'string', description: 'Python version string.' },
        pythonAvailable: { type: 'boolean', description: 'Whether Python is available.' },
        pythonPaths: { type: 'array', items: { type: 'string' }, description: 'Current Python paths.' },
        pythonInfo: {
          type: 'object',
          properties: {
            available: { type: 'boolean' },
            version: { type: 'string' },
            paths: { type: 'array', items: { type: 'string' } },
            startupScripts: { type: 'array', items: { type: 'string' } }
          },
          description: 'Python scripting info.'
        },

        // === Editor Scripting Outputs ===
        widgetCreated: { type: 'boolean', description: 'Whether widget was created.' },
        widgetPath: { type: 'string', description: 'Created widget asset path.' },
        menuEntryAdded: { type: 'boolean', description: 'Whether menu entry was added.' },
        toolbarButtonAdded: { type: 'boolean', description: 'Whether toolbar button was added.' },
        commandRegistered: { type: 'boolean', description: 'Whether command was registered.' },
        editorScriptingInfo: {
          type: 'object',
          properties: {
            registeredCommands: { type: 'array', items: { type: 'string' } },
            activeWidgets: { type: 'array', items: { type: 'string' } }
          },
          description: 'Editor scripting info.'
        },

        // === Modeling Tools Outputs ===
        toolActivated: { type: 'boolean', description: 'Whether tool was activated.' },
        activeTool: { type: 'string', description: 'Currently active tool name.' },
        selectedElements: {
          type: 'object',
          properties: {
            mode: { type: 'string' },
            count: { type: 'number' },
            indices: { type: 'array', items: { type: 'number' } }
          },
          description: 'Current mesh selection.'
        },
        operationApplied: { type: 'boolean', description: 'Whether operation was applied.' },
        availableTools: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              name: { type: 'string' },
              identifier: { type: 'string' },
              category: { type: 'string' }
            }
          },
          description: 'Available modeling tools.'
        },
        modelingToolsInfo: {
          type: 'object',
          properties: {
            available: { type: 'boolean' },
            inModelingMode: { type: 'boolean' },
            activeTool: { type: 'string' },
            toolCount: { type: 'number' }
          },
          description: 'Modeling tools info.'
        },

        // === Common UI Outputs ===
        configApplied: { type: 'boolean', description: 'Whether config was applied.' },
        commonUIInfo: {
          type: 'object',
          properties: {
            available: { type: 'boolean' },
            inputConfigs: { type: 'array', items: { type: 'string' } },
            activatableWidgets: { type: 'number' }
          },
          description: 'Common UI info.'
        },

        // === Paper2D Outputs ===
        spriteCreated: { type: 'boolean', description: 'Whether sprite was created.' },
        spritePath: { type: 'string', description: 'Created sprite asset path.' },
        flipbookCreated: { type: 'boolean', description: 'Whether flipbook was created.' },
        flipbookPath: { type: 'string', description: 'Created flipbook asset path.' },
        tileMapCreated: { type: 'boolean', description: 'Whether tile map was created.' },
        tileMapPath: { type: 'string', description: 'Created tile map asset path.' },
        actorSpawned: { type: 'boolean', description: 'Whether actor was spawned.' },
        actorName: { type: 'string', description: 'Spawned actor name.' },
        paper2DInfo: {
          type: 'object',
          properties: {
            available: { type: 'boolean' },
            spriteCount: { type: 'number' },
            flipbookCount: { type: 'number' },
            tileMapCount: { type: 'number' }
          },
          description: 'Paper2D info.'
        },

        // === Procedural Mesh Outputs ===
        componentCreated: { type: 'boolean', description: 'Whether component was created.' },
        sectionCreated: { type: 'boolean', description: 'Whether section was created.' },
        sectionUpdated: { type: 'boolean', description: 'Whether section was updated.' },
        sectionCleared: { type: 'boolean', description: 'Whether section was cleared.' },
        meshConverted: { type: 'boolean', description: 'Whether mesh was converted.' },
        convertedMeshPath: { type: 'string', description: 'Converted static mesh path.' },
        proceduralMeshInfo: {
          type: 'object',
          properties: {
            componentName: { type: 'string' },
            sectionCount: { type: 'number' },
            vertexCount: { type: 'number' },
            triangleCount: { type: 'number' },
            hasCollision: { type: 'boolean' }
          },
          description: 'Procedural mesh component info.'
        },

        // === Variant Manager Outputs ===
        variantSetsCreated: { type: 'boolean', description: 'Whether variant sets were created.' },
        variantSetsPath: { type: 'string', description: 'Variant sets asset path.' },
        variantSetCreated: { type: 'boolean', description: 'Whether variant set was created.' },
        variantCreated: { type: 'boolean', description: 'Whether variant was created.' },
        variantActivated: { type: 'boolean', description: 'Whether variant was activated.' },
        activeVariant: { type: 'string', description: 'Currently active variant name.' },
        bindingAdded: { type: 'boolean', description: 'Whether binding was added.' },
        propertyCaptured: { type: 'boolean', description: 'Whether property was captured.' },
        exported: { type: 'boolean', description: 'Whether export succeeded.' },
        variantManagerInfo: {
          type: 'object',
          properties: {
            available: { type: 'boolean' },
            variantSetsCount: { type: 'number' },
            totalVariants: { type: 'number' }
          },
          description: 'Variant manager info.'
        },

        // === Utility Plugin List ===
        utilityPlugins: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              name: { type: 'string' },
              available: { type: 'boolean' },
              enabled: { type: 'boolean' },
              version: { type: 'string' }
            }
          },
          description: 'Available utility plugins.'
        },

        // === Plugin Status ===
        pluginStatus: {
          type: 'object',
          properties: {
            name: { type: 'string' },
            available: { type: 'boolean' },
            enabled: { type: 'boolean' },
            loaded: { type: 'boolean' },
            version: { type: 'string' }
          },
          description: 'Specific plugin status.'
        },

         error: commonSchemas.stringProp
      }
    }
  },

  // ===== PHASE 44: PHYSICS & DESTRUCTION PLUGINS =====
  {
    name: 'manage_physics_destruction',
    category: 'gameplay',
    description: 'Chaos Physics systems: Destruction (Geometry Collections, fracturing, field systems), Vehicles (wheeled physics), Cloth (simulation), and Flesh (deformable physics).',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            // CHAOS DESTRUCTION (29 actions)
            'create_geometry_collection', 'fracture_uniform', 'fracture_clustered', 'fracture_radial',
            'fracture_slice', 'fracture_brick', 'flatten_fracture', 'set_geometry_collection_materials',
            'set_damage_thresholds', 'set_cluster_connection_type', 'set_collision_particles_fraction',
            'set_remove_on_break', 'create_field_system_actor', 'add_transient_field', 'add_persistent_field',
            'add_construction_field', 'add_field_radial_falloff', 'add_field_radial_vector',
            'add_field_uniform_vector', 'add_field_noise', 'add_field_strain', 'create_anchor_field',
            'set_dynamic_state', 'enable_clustering', 'get_geometry_collection_stats',
            'create_geometry_collection_cache', 'record_geometry_collection_cache',
            'apply_cache_to_collection', 'remove_geometry_collection_cache',
            // CHAOS VEHICLES (19 actions)
            'create_wheeled_vehicle_bp', 'add_vehicle_wheel', 'remove_wheel_from_vehicle',
            'configure_engine_setup', 'configure_transmission_setup', 'configure_steering_setup',
            'configure_differential_setup', 'configure_suspension_setup', 'configure_brake_setup',
            'set_vehicle_mesh', 'set_wheel_class', 'set_wheel_offset', 'set_wheel_radius',
            'set_vehicle_mass', 'set_drag_coefficient', 'set_center_of_mass',
            'create_vehicle_animation_instance', 'set_vehicle_animation_bp', 'get_vehicle_config',
            // CHAOS CLOTH (15 actions)
            'create_chaos_cloth_config', 'create_chaos_cloth_shared_sim_config',
            'apply_cloth_to_skeletal_mesh', 'remove_cloth_from_skeletal_mesh',
            'set_cloth_mass_properties', 'set_cloth_gravity', 'set_cloth_damping',
            'set_cloth_collision_properties', 'set_cloth_stiffness', 'set_cloth_tether_stiffness',
            'set_cloth_aerodynamics', 'set_cloth_anim_drive', 'set_cloth_long_range_attachment',
            'get_cloth_config', 'get_cloth_stats',
            // CHAOS FLESH (13 actions)
            'create_flesh_asset', 'create_flesh_component', 'set_flesh_simulation_properties',
            'set_flesh_stiffness', 'set_flesh_damping', 'set_flesh_incompressibility',
            'set_flesh_inflation', 'set_flesh_solver_iterations', 'bind_flesh_to_skeleton',
            'set_flesh_rest_state', 'create_flesh_cache', 'record_flesh_simulation',
            'get_flesh_asset_info',
            // Utility (4 actions)
            'get_physics_destruction_info', 'list_geometry_collections', 'list_chaos_vehicles', 'get_chaos_plugin_status'
          ],
          description: 'Physics/Destruction action.'
        },

        // === Common Parameters ===
        actorName: commonSchemas.actorName,
        assetPath: commonSchemas.assetPath,
        assetName: { type: 'string', description: 'Name for the new asset.' },
        componentName: { type: 'string', description: 'Component name.' },
        save: commonSchemas.save,

        // === Geometry Collection Parameters ===
        sourceMeshPath: { type: 'string', description: 'Source static mesh path for geometry collection.' },
        geometryCollectionPath: { type: 'string', description: 'Geometry collection asset path.' },
        fractureLevel: { type: 'number', description: 'Fracture level (0 = root level).' },
        seedCount: { type: 'number', description: 'Number of Voronoi seeds for fracturing.' },
        clusterCount: { type: 'number', description: 'Number of clusters for clustered fracture.' },
        radialCenter: {
          type: 'object',
          properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } },
          description: 'Center point for radial fracture.'
        },
        radialNormal: {
          type: 'object',
          properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } },
          description: 'Normal direction for radial fracture.'
        },
        radialRadius: { type: 'number', description: 'Radius for radial fracture.' },
        slicePlane: {
          type: 'object',
          properties: {
            origin: { type: 'object', properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } } },
            normal: { type: 'object', properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } } }
          },
          description: 'Plane definition for slice fracture.'
        },
        brickLength: { type: 'number', description: 'Brick length for brick fracture.' },
        brickWidth: { type: 'number', description: 'Brick width for brick fracture.' },
        brickHeight: { type: 'number', description: 'Brick height for brick fracture.' },
        materialPaths: { type: 'array', items: { type: 'string' }, description: 'Material paths for geometry collection.' },
        damageThreshold: { type: 'number', description: 'Damage threshold value.' },
        damageModel: { type: 'string', enum: ['Material', 'UserDefined'], description: 'Damage model type.' },
        clusterConnectionType: {
          type: 'string',
          enum: ['PointImplicit', 'PointImplicitAugmentedGrid', 'DelaunayTriangulation', 'MinimalSpanningSubsetDelaunayTriangulation', 'PointImplicitConvex', 'None'],
          description: 'Cluster connection type.'
        },
        collisionParticlesFraction: { type: 'number', description: 'Fraction of particles for collision (0.0-1.0).' },
        removeOnBreak: { type: 'boolean', description: 'Remove pieces on break.' },
        removeOnSleep: { type: 'boolean', description: 'Remove pieces on sleep.' },
        maxBreakTime: { type: 'number', description: 'Max time before removal after break.' },
        dynamicState: { type: 'string', enum: ['Static', 'Kinematic', 'Dynamic', 'Sleeping'], description: 'Dynamic state for geometry.' },
        clusteringEnabled: { type: 'boolean', description: 'Enable/disable clustering.' },
        maxClusterLevel: { type: 'number', description: 'Maximum cluster level.' },

        // === Field System Parameters ===
        fieldSystemName: { type: 'string', description: 'Field system actor name.' },
        fieldType: {
          type: 'string',
          enum: ['RadialFalloff', 'RadialVector', 'UniformVector', 'Noise', 'Strain', 'AnchorField'],
          description: 'Type of field to add.'
        },
        fieldMagnitude: { type: 'number', description: 'Field magnitude/strength.' },
        fieldPosition: {
          type: 'object',
          properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } },
          description: 'Field center position.'
        },
        fieldDirection: {
          type: 'object',
          properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } },
          description: 'Field direction vector.'
        },
        fieldRadius: { type: 'number', description: 'Field effect radius.' },
        fieldFalloff: { type: 'string', enum: ['None', 'Linear', 'Squared', 'Inverse', 'Logarithmic'], description: 'Field falloff type.' },

        // === Geometry Collection Cache Parameters ===
        cacheName: { type: 'string', description: 'Cache asset name.' },
        cachePath: { type: 'string', description: 'Cache asset path.' },
        recordDuration: { type: 'number', description: 'Recording duration in seconds.' },
        startFrame: { type: 'number', description: 'Start frame for cache.' },
        endFrame: { type: 'number', description: 'End frame for cache.' },

        // === Vehicle Parameters ===
        vehicleBlueprintPath: { type: 'string', description: 'Vehicle blueprint asset path.' },
        vehicleName: { type: 'string', description: 'Vehicle blueprint name.' },
        skeletalMeshPath: { type: 'string', description: 'Vehicle skeletal mesh path.' },
        wheelIndex: { type: 'number', description: 'Wheel index (0-based).' },
        wheelBoneName: { type: 'string', description: 'Wheel bone name in skeleton.' },
        wheelClass: { type: 'string', description: 'Wheel class name or path.' },
        wheelOffset: {
          type: 'object',
          properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } },
          description: 'Wheel attachment offset.'
        },
        wheelRadius: { type: 'number', description: 'Wheel radius in cm.' },
        wheelWidth: { type: 'number', description: 'Wheel width in cm.' },
        wheelMass: { type: 'number', description: 'Wheel mass in kg.' },
        suspensionMaxRaise: { type: 'number', description: 'Max suspension raise in cm.' },
        suspensionMaxDrop: { type: 'number', description: 'Max suspension drop in cm.' },
        suspensionNaturalFrequency: { type: 'number', description: 'Suspension frequency (Hz).' },
        suspensionDampingRatio: { type: 'number', description: 'Suspension damping ratio.' },
        brakeForce: { type: 'number', description: 'Brake force in Newtons.' },
        handbrakeForce: { type: 'number', description: 'Handbrake force in Newtons.' },
        engineSetup: {
          type: 'object',
          properties: {
            maxRPM: { type: 'number' },
            idleRPM: { type: 'number' },
            maxTorque: { type: 'number' },
            torqueCurve: { type: 'array', items: { type: 'object', properties: { inVal: { type: 'number' }, outVal: { type: 'number' } } } }
          },
          description: 'Engine configuration.'
        },
        transmissionSetup: {
          type: 'object',
          properties: {
            gearRatios: { type: 'array', items: { type: 'number' } },
            reverseGearRatio: { type: 'number' },
            finalDriveRatio: { type: 'number' },
            gearChangeTime: { type: 'number' },
            gearAutoBox: { type: 'boolean' }
          },
          description: 'Transmission configuration.'
        },
        steeringSetup: {
          type: 'object',
          properties: {
            steeringCurve: { type: 'array', items: { type: 'object', properties: { inVal: { type: 'number' }, outVal: { type: 'number' } } } },
            steeringType: { type: 'string', enum: ['SingleAngle', 'AngleRatio', 'Ackermann'] }
          },
          description: 'Steering configuration.'
        },
        differentialSetup: {
          type: 'object',
          properties: {
            differentialType: { type: 'string', enum: ['LimitedSlip_4W', 'LimitedSlip_FrontDrive', 'LimitedSlip_RearDrive', 'Open_4W', 'Open_FrontDrive', 'Open_RearDrive'] },
            frontRearSplit: { type: 'number' }
          },
          description: 'Differential configuration.'
        },
        vehicleMass: { type: 'number', description: 'Vehicle mass in kg.' },
        dragCoefficient: { type: 'number', description: 'Aerodynamic drag coefficient.' },
        centerOfMass: {
          type: 'object',
          properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } },
          description: 'Center of mass offset.'
        },
        animationBPPath: { type: 'string', description: 'Vehicle animation blueprint path.' },

        // === Cloth Parameters ===
        clothConfigName: { type: 'string', description: 'Cloth config asset name.' },
        clothConfigPath: { type: 'string', description: 'Cloth config asset path.' },
        skeletalMeshAssetPath: { type: 'string', description: 'Skeletal mesh to apply cloth to.' },
        clothLODIndex: { type: 'number', description: 'LOD index for cloth binding.' },
        clothSectionIndex: { type: 'number', description: 'Mesh section index for cloth.' },
        clothMass: { type: 'number', description: 'Cloth mass (kg/m^2).' },
        clothGravityScale: { type: 'number', description: 'Cloth gravity scale (1.0 = normal gravity).' },
        clothLinearDamping: { type: 'number', description: 'Linear velocity damping.' },
        clothAngularDamping: { type: 'number', description: 'Angular velocity damping.' },
        clothCollisionThickness: { type: 'number', description: 'Collision thickness in cm.' },
        clothFriction: { type: 'number', description: 'Cloth friction coefficient.' },
        clothSelfCollision: { type: 'boolean', description: 'Enable self-collision.' },
        clothSelfCollisionRadius: { type: 'number', description: 'Self-collision radius.' },
        clothEdgeStiffness: { type: 'number', description: 'Edge constraint stiffness.' },
        clothBendingStiffness: { type: 'number', description: 'Bending constraint stiffness.' },
        clothAreaStiffness: { type: 'number', description: 'Area constraint stiffness.' },
        clothTetherStiffness: { type: 'number', description: 'Tether constraint stiffness.' },
        clothTetherLimit: { type: 'number', description: 'Tether limit scale.' },
        clothDragCoefficient: { type: 'number', description: 'Aerodynamic drag coefficient.' },
        clothLiftCoefficient: { type: 'number', description: 'Aerodynamic lift coefficient.' },
        clothAnimDriveStiffness: { type: 'number', description: 'Animation drive stiffness.' },
        clothAnimDriveDamping: { type: 'number', description: 'Animation drive damping.' },
        clothLongRangeAttachment: { type: 'boolean', description: 'Enable long-range attachment.' },
        clothLongRangeStiffness: { type: 'number', description: 'Long-range attachment stiffness.' },

        // === Flesh Parameters ===
        fleshAssetName: { type: 'string', description: 'Flesh asset name.' },
        fleshAssetPath: { type: 'string', description: 'Flesh asset path.' },
        fleshMass: { type: 'number', description: 'Flesh mass in kg.' },
        fleshStiffness: { type: 'number', description: 'Flesh stiffness value.' },
        fleshDamping: { type: 'number', description: 'Flesh damping value.' },
        fleshIncompressibility: { type: 'number', description: 'Incompressibility stiffness.' },
        fleshInflation: { type: 'number', description: 'Inflation pressure.' },
        fleshSolverIterations: { type: 'number', description: 'Number of solver iterations.' },
        fleshSubstepCount: { type: 'number', description: 'Number of simulation substeps.' },
        skeletonMeshPath: { type: 'string', description: 'Skeleton mesh path for binding.' },
        fleshCacheName: { type: 'string', description: 'Flesh cache asset name.' },
        fleshCachePath: { type: 'string', description: 'Flesh cache asset path.' },

        // === Query Parameters ===
        filter: commonSchemas.filter,
        pluginName: { type: 'string', description: 'Plugin name to check status.' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        ...commonSchemas.outputBase,
        message: { type: 'string', description: 'Status message.' },

        // === Geometry Collection Outputs ===
        geometryCollectionCreated: { type: 'boolean', description: 'Whether geometry collection was created.' },
        geometryCollectionPath: { type: 'string', description: 'Created geometry collection path.' },
        fractureApplied: { type: 'boolean', description: 'Whether fracture was applied.' },
        fragmentCount: { type: 'number', description: 'Number of fragments after fracture.' },
        clusterCount: { type: 'number', description: 'Number of clusters.' },
        geometryCollectionStats: {
          type: 'object',
          properties: {
            numTransforms: { type: 'number' },
            numGeometries: { type: 'number' },
            numClusters: { type: 'number' },
            numRootBones: { type: 'number' },
            boundingBox: {
              type: 'object',
              properties: {
                min: { type: 'object', properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } } },
                max: { type: 'object', properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } } }
              }
            }
          },
          description: 'Geometry collection statistics.'
        },

        // === Field System Outputs ===
        fieldSystemCreated: { type: 'boolean', description: 'Whether field system was created.' },
        fieldAdded: { type: 'boolean', description: 'Whether field was added.' },
        fieldSystemName: { type: 'string', description: 'Field system actor name.' },

        // === Cache Outputs ===
        cacheCreated: { type: 'boolean', description: 'Whether cache was created.' },
        cachePath: { type: 'string', description: 'Cache asset path.' },
        recordingStarted: { type: 'boolean', description: 'Whether recording started.' },
        cacheApplied: { type: 'boolean', description: 'Whether cache was applied.' },

        // === Vehicle Outputs ===
        vehicleCreated: { type: 'boolean', description: 'Whether vehicle was created.' },
        vehicleBlueprintPath: { type: 'string', description: 'Created vehicle blueprint path.' },
        wheelAdded: { type: 'boolean', description: 'Whether wheel was added.' },
        wheelRemoved: { type: 'boolean', description: 'Whether wheel was removed.' },
        configApplied: { type: 'boolean', description: 'Whether config was applied.' },
        vehicleConfig: {
          type: 'object',
          properties: {
            wheelCount: { type: 'number' },
            vehicleMass: { type: 'number' },
            maxSpeed: { type: 'number' },
            engineMaxRPM: { type: 'number' },
            gearCount: { type: 'number' },
            differentialType: { type: 'string' }
          },
          description: 'Current vehicle configuration.'
        },

        // === Cloth Outputs ===
        clothConfigCreated: { type: 'boolean', description: 'Whether cloth config was created.' },
        clothConfigPath: { type: 'string', description: 'Cloth config asset path.' },
        clothApplied: { type: 'boolean', description: 'Whether cloth was applied.' },
        clothRemoved: { type: 'boolean', description: 'Whether cloth was removed.' },
        clothConfig: {
          type: 'object',
          properties: {
            mass: { type: 'number' },
            gravityScale: { type: 'number' },
            edgeStiffness: { type: 'number' },
            bendingStiffness: { type: 'number' },
            selfCollision: { type: 'boolean' }
          },
          description: 'Current cloth configuration.'
        },
        clothStats: {
          type: 'object',
          properties: {
            vertexCount: { type: 'number' },
            triangleCount: { type: 'number' },
            constraintCount: { type: 'number' },
            simulationTime: { type: 'number' }
          },
          description: 'Cloth simulation statistics.'
        },

        // === Flesh Outputs ===
        fleshAssetCreated: { type: 'boolean', description: 'Whether flesh asset was created.' },
        fleshAssetPath: { type: 'string', description: 'Flesh asset path.' },
        fleshComponentCreated: { type: 'boolean', description: 'Whether flesh component was created.' },
        fleshBound: { type: 'boolean', description: 'Whether flesh was bound to skeleton.' },
        fleshAssetInfo: {
          type: 'object',
          properties: {
            nodeCount: { type: 'number' },
            tetCount: { type: 'number' },
            vertexCount: { type: 'number' },
            mass: { type: 'number' },
            stiffness: { type: 'number' }
          },
          description: 'Flesh asset information.'
        },

        // === Lists ===
        geometryCollections: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              name: { type: 'string' },
              path: { type: 'string' },
              fragmentCount: { type: 'number' }
            }
          },
          description: 'List of geometry collections.'
        },
        chaosVehicles: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              name: { type: 'string' },
              path: { type: 'string' },
              wheelCount: { type: 'number' }
            }
          },
          description: 'List of Chaos vehicles.'
        },

        // === Plugin Info ===
        physicsDestructionInfo: {
          type: 'object',
          properties: {
            chaosDestructionAvailable: { type: 'boolean' },
            chaosVehiclesAvailable: { type: 'boolean' },
            chaosClothAvailable: { type: 'boolean' },
            chaosFleshAvailable: { type: 'boolean' },
            geometryCollectionCount: { type: 'number' },
            fieldSystemCount: { type: 'number' },
            vehicleCount: { type: 'number' }
          },
          description: 'Physics destruction plugin info.'
        },
        pluginStatus: {
          type: 'object',
          properties: {
            name: { type: 'string' },
            available: { type: 'boolean' },
            enabled: { type: 'boolean' },
            version: { type: 'string' }
          },
          description: 'Plugin status.'
        },

        error: commonSchemas.stringProp
      }
    }
  },

  // ===== PHASE 45: ACCESSIBILITY SYSTEM =====
  {
    name: 'manage_accessibility',
    category: 'utility',
    description: 'Accessibility features: Visual (colorblind, high contrast, UI scale), Subtitles, Audio (mono, visualization), Motor (remapping, auto-aim), Cognitive (difficulty, navigation).',
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
  // Phase 46: Modding & UGC System
  {
    name: 'manage_modding',
    category: 'utility',
    description: 'Mod support and user-generated content. PAK loading, mod discovery, asset overrides, SDK generation, security.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            // PAK Loading (6 actions)
            'configure_mod_loading_paths', 'scan_for_mod_paks', 'load_mod_pak', 'unload_mod_pak',
            'validate_mod_pak', 'configure_mod_load_order',
            // Discovery (5 actions)
            'list_installed_mods', 'enable_mod', 'disable_mod',
            'check_mod_compatibility', 'get_mod_info',
            // Asset Override (4 actions)
            'configure_asset_override_paths', 'register_mod_asset_redirect',
            'restore_original_asset', 'list_asset_overrides',
            // SDK Generation (4 actions)
            'export_moddable_headers', 'create_mod_template_project',
            'configure_exposed_classes', 'get_sdk_config',
            // Security (4 actions)
            'configure_mod_sandbox', 'set_allowed_mod_operations',
            'validate_mod_content', 'get_security_config',
            // Utility (2 actions)
            'get_modding_info', 'reset_mod_system'
          ],
          description: 'Modding action.'
        },

        // === PAK Loading Parameters ===
        modPaths: { type: 'array', items: { type: 'string' }, description: 'List of mod directory paths to scan.' },
        pakPath: { type: 'string', description: 'Path to PAK file to load/unload.' },
        modId: { type: 'string', description: 'Unique mod identifier.' },
        modName: { type: 'string', description: 'Display name for mod.' },
        mountPoint: { type: 'string', description: 'Mount point for PAK (e.g., /Game/Mods/MyMod/).' },
        loadPriority: { type: 'number', description: 'Load order priority (lower = loads first).' },
        autoLoad: { type: 'boolean', description: 'Automatically load on startup.' },
        loadOrder: { type: 'array', items: { type: 'string' }, description: 'Ordered list of mod IDs for load sequence.' },

        // === Discovery Parameters ===
        enabled: { type: 'boolean', description: 'Enable or disable the mod.' },
        targetVersion: { type: 'string', description: 'Target game version for compatibility check.' },
        dependencies: { type: 'array', items: { type: 'string' }, description: 'Required mod dependencies.' },

        // === Asset Override Parameters ===
        overridePaths: { type: 'array', items: { type: 'string' }, description: 'Paths that can be overridden by mods.' },
        originalAssetPath: { type: 'string', description: 'Original asset path to redirect from.' },
        modAssetPath: { type: 'string', description: 'Mod asset path to redirect to.' },
        redirectType: { type: 'string', enum: ['Replace', 'Merge', 'Append'], description: 'Type of asset redirect.' },

        // === SDK Generation Parameters ===
        outputPath: { type: 'string', description: 'Output path for exported headers/templates.' },
        templateName: { type: 'string', description: 'Name for mod template project.' },
        exposedClasses: { type: 'array', items: { type: 'string' }, description: 'Classes to expose for modding.' },
        exposedProperties: { type: 'array', items: { type: 'string' }, description: 'Properties to expose for modding.' },
        includeSourceCode: { type: 'boolean', description: 'Include source code stubs in template.' },
        sdkVersion: { type: 'string', description: 'SDK version string.' },

        // === Security Parameters ===
        sandboxEnabled: { type: 'boolean', description: 'Enable mod sandbox.' },
        sandboxLevel: { type: 'string', enum: ['Minimal', 'Standard', 'Strict'], description: 'Sandbox restriction level.' },
        allowedOperations: {
          type: 'object',
          properties: {
            fileRead: { type: 'boolean' },
            fileWrite: { type: 'boolean' },
            networkAccess: { type: 'boolean' },
            nativeCode: { type: 'boolean' },
            blueprintExecution: { type: 'boolean' },
            assetLoading: { type: 'boolean' }
          },
          description: 'Allowed mod operations.'
        },
        contentValidation: {
          type: 'object',
          properties: {
            validateAssets: { type: 'boolean' },
            validateBlueprints: { type: 'boolean' },
            checkSignatures: { type: 'boolean' },
            maxAssetSize: { type: 'number' },
            blockedAssetTypes: { type: 'array', items: { type: 'string' } }
          },
          description: 'Content validation settings.'
        },

        // === Filter ===
        filter: { type: 'string', description: 'Filter string for listings.' },
        includeDisabled: { type: 'boolean', description: 'Include disabled mods in listings.' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        ...commonSchemas.outputBase,
        message: { type: 'string', description: 'Status message.' },

        // === PAK Loading Outputs ===
        pakLoaded: { type: 'boolean', description: 'Whether PAK was loaded.' },
        pakUnloaded: { type: 'boolean', description: 'Whether PAK was unloaded.' },
        pakValid: { type: 'boolean', description: 'Whether PAK passed validation.' },
        pakInfo: {
          type: 'object',
          properties: {
            pakPath: { type: 'string' },
            mountPoint: { type: 'string' },
            size: { type: 'number' },
            assetCount: { type: 'number' },
            isEncrypted: { type: 'boolean' },
            isCompressed: { type: 'boolean' }
          },
          description: 'PAK file information.'
        },
        discoveredPaks: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              path: { type: 'string' },
              name: { type: 'string' },
              size: { type: 'number' }
            }
          },
          description: 'List of discovered PAK files.'
        },

        // === Mod Discovery Outputs ===
        installedMods: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              modId: { type: 'string' },
              name: { type: 'string' },
              version: { type: 'string' },
              author: { type: 'string' },
              description: { type: 'string' },
              enabled: { type: 'boolean' },
              loadOrder: { type: 'number' },
              dependencies: { type: 'array', items: { type: 'string' } }
            }
          },
          description: 'List of installed mods.'
        },
        modEnabled: { type: 'boolean', description: 'Whether mod was enabled.' },
        modDisabled: { type: 'boolean', description: 'Whether mod was disabled.' },
        compatibilityResult: {
          type: 'object',
          properties: {
            compatible: { type: 'boolean' },
            issues: { type: 'array', items: { type: 'string' } },
            missingDependencies: { type: 'array', items: { type: 'string' } },
            versionConflicts: { type: 'array', items: { type: 'string' } }
          },
          description: 'Mod compatibility check result.'
        },
        modInfo: {
          type: 'object',
          properties: {
            modId: { type: 'string' },
            name: { type: 'string' },
            version: { type: 'string' },
            author: { type: 'string' },
            description: { type: 'string' },
            enabled: { type: 'boolean' },
            pakPath: { type: 'string' },
            assetCount: { type: 'number' },
            dependencies: { type: 'array', items: { type: 'string' } }
          },
          description: 'Mod information.'
        },

        // === Asset Override Outputs ===
        redirectRegistered: { type: 'boolean', description: 'Whether redirect was registered.' },
        assetRestored: { type: 'boolean', description: 'Whether original asset was restored.' },
        activeOverrides: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              originalPath: { type: 'string' },
              modPath: { type: 'string' },
              modId: { type: 'string' },
              redirectType: { type: 'string' }
            }
          },
          description: 'List of active asset overrides.'
        },

        // === SDK Generation Outputs ===
        headersExported: { type: 'boolean', description: 'Whether headers were exported.' },
        templateCreated: { type: 'boolean', description: 'Whether template was created.' },
        exportPath: { type: 'string', description: 'Path to exported content.' },
        exposedClassCount: { type: 'number', description: 'Number of exposed classes.' },
        sdkConfig: {
          type: 'object',
          properties: {
            version: { type: 'string' },
            exposedClasses: { type: 'array', items: { type: 'string' } },
            exposedProperties: { type: 'array', items: { type: 'string' } },
            outputPath: { type: 'string' }
          },
          description: 'SDK configuration.'
        },

        // === Security Outputs ===
        sandboxConfigured: { type: 'boolean', description: 'Whether sandbox was configured.' },
        operationsSet: { type: 'boolean', description: 'Whether operations were set.' },
        contentValidationResult: {
          type: 'object',
          properties: {
            valid: { type: 'boolean' },
            errors: { type: 'array', items: { type: 'string' } },
            warnings: { type: 'array', items: { type: 'string' } },
            blockedAssets: { type: 'array', items: { type: 'string' } }
          },
          description: 'Content validation result.'
        },
        securityConfig: {
          type: 'object',
          properties: {
            sandboxEnabled: { type: 'boolean' },
            sandboxLevel: { type: 'string' },
            allowedOperations: { type: 'object' },
            contentValidation: { type: 'object' }
          },
          description: 'Security configuration.'
        },

        // === Modding Info Output ===
        moddingInfo: {
          type: 'object',
          properties: {
            modSystemEnabled: { type: 'boolean' },
            installedModCount: { type: 'number' },
            enabledModCount: { type: 'number' },
            loadedPakCount: { type: 'number' },
            modPaths: { type: 'array', items: { type: 'string' } },
            sandboxEnabled: { type: 'boolean' },
            sdkVersion: { type: 'string' }
          },
          description: 'Modding system info.'
        },

        error: commonSchemas.stringProp
      }
    }
  }
];
