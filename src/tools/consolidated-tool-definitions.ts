import { commonSchemas } from './tool-definition-utils.js';
// Force rebuild timestamp update

/** MCP Tool Definition type for explicit annotation to avoid TS7056 */
export interface ToolDefinition {
  name: string;
  description: string;
  inputSchema: Record<string, unknown>;
  outputSchema?: Record<string, unknown>;
  [key: string]: unknown;
}

export const consolidatedToolDefinitions: ToolDefinition[] = [
  // 1. ASSET MANAGER
  {
    name: 'manage_asset',
    description: 'Create, import, duplicate, rename, delete assets. Edit Material graphs and instances. Analyze dependencies.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            // Core
            'list', 'import', 'duplicate', 'rename', 'move', 'delete', 'delete_asset', 'delete_assets', 'create_folder', 'search_assets',
            // Utils
            'get_dependencies', 'get_source_control_state', 'analyze_graph', 'get_asset_graph', 'create_thumbnail', 'set_tags', 'get_metadata', 'set_metadata', 'validate', 'fixup_redirectors', 'find_by_tag', 'generate_report',
            // Creation
            'create_material', 'create_material_instance', 'create_render_target', 'generate_lods', 'add_material_parameter', 'list_instances', 'reset_instance_parameters', 'exists', 'get_material_stats',
            // Rendering
            'nanite_rebuild_mesh',
            // Material Graph
            'add_material_node', 'connect_material_pins', 'remove_material_node', 'break_material_connections', 'get_material_node_details', 'rebuild_material'
          ],
          description: 'Action to perform'
        },
        // -- Common --
        assetPath: { type: 'string', description: 'Target asset path (e.g., "/Game/MyAsset").' },

        // -- List/Search --
        directory: { type: 'string', description: 'Directory path to list.' },
        classNames: { type: 'array', items: { type: 'string' }, description: 'Class names filter.' },
        packagePaths: { type: 'array', items: { type: 'string' }, description: 'Package paths to search.' },
        recursivePaths: { type: 'boolean' },
        recursiveClasses: { type: 'boolean' },
        limit: { type: 'number' },

        // -- Import --
        sourcePath: { type: 'string', description: 'Source file path on disk.' },
        destinationPath: { type: 'string', description: 'Destination content path.' },

        // -- Operations --
        assetPaths: { type: 'array', items: { type: 'string' }, description: 'Batch asset paths.' },
        lodCount: { type: 'number', description: 'Number of LODs to generate.' },
        reductionSettings: { type: 'object', description: 'LOD reduction settings.' },
        nodeName: { type: 'string', description: 'Variable name or Function name, depending on node type' },
        eventName: { type: 'string', description: 'For Event nodes (e.g. ReceiveBeginPlay) or CustomEvent nodes' },
        memberClass: { type: 'string', description: 'For Event nodes, the class defining the event (optional)' },
        posX: { type: 'number' },
        newName: { type: 'string', description: 'New name for rename/duplicate.' },
        overwrite: { type: 'boolean' },
        save: { type: 'boolean' },
        fixupRedirectors: { type: 'boolean' },
        directoryPath: { type: 'string' },

        // -- Material/Instance Creation --
        name: { type: 'string', description: 'Name of new asset.' },
        path: { type: 'string', description: 'Directory to create asset in.' },
        parentMaterial: { type: 'string', description: 'Parent material for instances.' },
        parameters: { type: 'object', description: 'Material instance parameters.' },

        // -- Render Target --
        width: { type: 'number' },
        height: { type: 'number' },
        format: { type: 'string' },

        // -- Nanite --
        meshPath: { type: 'string' },

        // -- Metadata/Tags --
        tag: { type: 'string' },
        metadata: { type: 'object' },

        // -- Graph Editing (Material/BT) --
        graphName: { type: 'string' },
        nodeType: { type: 'string' },
        nodeId: { type: 'string' },
        sourceNodeId: { type: 'string', description: 'Source node ID for material connections' },
        targetNodeId: { type: 'string', description: 'Target node ID, or "Main" for material root' },
        inputName: { type: 'string', description: 'Input pin name (e.g., "BaseColor", "Roughness")' },
        fromNodeId: { type: 'string', description: '[Deprecated] Use sourceNodeId. Source node ID' },
        fromPin: { type: 'string', description: 'Source pin name' },
        toNodeId: { type: 'string', description: '[Deprecated] Use targetNodeId. Target node ID' },
        toPin: { type: 'string', description: '[Deprecated] Use inputName. Target pin name' },
        parameterName: { type: 'string' },
        value: { description: 'Property value (number, string, etc).' },
        x: { type: 'number' },
        y: { type: 'number' },
        comment: { type: 'string' },
        parentNodeId: { type: 'string' },
        childNodeId: { type: 'string' },

        // -- Analyze --
        maxDepth: { type: 'number' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        message: { type: 'string' },
        assets: { type: 'array', items: { type: 'object' } },
        paths: { type: 'array', items: { type: 'string' } },
        path: { type: 'string' },
        error: { type: 'string' },
        // Graph results
        nodeId: { type: 'string' },
        details: { type: 'object' }
      }
    }
  },

  // 2. BLUEPRINT MANAGER
  {
    name: 'manage_blueprint',
    description: 'Create Blueprints, add SCS components (mesh, collision, camera), and manipulate graph nodes.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            // Lifecycle
            'create', 'get_blueprint', 'get', 'compile',
            // SCS
            'add_component', 'set_default', 'modify_scs', 'get_scs', 'add_scs_component', 'remove_scs_component', 'reparent_scs_component', 'set_scs_transform', 'set_scs_property',
            // Helpers
            'ensure_exists', 'probe_handle', 'add_variable', 'remove_variable', 'rename_variable', 'add_function', 'add_event', 'remove_event', 'add_construction_script', 'set_variable_metadata', 'set_metadata',
            // Graph
            'create_node', 'add_node', 'delete_node', 'connect_pins', 'break_pin_links', 'set_node_property', 'create_reroute_node', 'get_node_details', 'get_graph_details', 'get_pin_details'
          ],
          description: 'Blueprint action'
        },
        // -- Identifiers --
        name: { type: 'string', description: 'Blueprint name.' },
        blueprintPath: { type: 'string', description: 'Blueprint asset path.' },

        // -- Create --
        blueprintType: { type: 'string', description: 'Parent class (e.g., Actor).' },
        savePath: { type: 'string' },

        // -- Components --
        componentType: { type: 'string' },
        componentName: { type: 'string' },
        componentClass: { type: 'string' },
        attachTo: { type: 'string' },
        newParent: { type: 'string' },

        // -- Properties/Defaults --
        propertyName: { type: 'string' },
        variableName: { type: 'string', description: 'Name of the variable.' },
        oldName: { type: 'string' },
        newName: { type: 'string' },
        value: { description: 'Value to set.' },
        metadata: { type: 'object' },
        properties: { type: 'object', description: 'Initial CDO properties for new blueprints.' },

        // -- Graph Editing --
        graphName: { type: 'string' },
        nodeType: { type: 'string' },
        nodeId: { type: 'string' },
        pinName: { type: 'string' },
        linkedTo: { type: 'string' },
        memberName: { type: 'string' },
        x: { type: 'number' },
        y: { type: 'number' },

        // -- SCS Transform --
        location: { type: 'array', items: { type: 'number' } },
        rotation: { type: 'array', items: { type: 'number' } },
        scale: { type: 'array', items: { type: 'number' } },

        // -- Batch Operations --
        operations: { type: 'array', items: { type: 'object' } },
        compile: { type: 'boolean' },
        save: { type: 'boolean' },

        // -- Events --
        eventType: { type: 'string', description: 'Event type (e.g. "BeginPlay", "Tick", "custom").' },
        customEventName: { type: 'string', description: 'Name for custom event.' },
        parameters: { type: 'array', items: { type: 'object' }, description: 'Parameters for the event.' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        blueprintPath: { type: 'string' },
        message: { type: 'string' },
        error: { type: 'string' },
        // Snapshot data
        blueprint: { type: 'object' }
      }
    }
  },

  // 3. ACTOR CONTROL
  {
    name: 'control_actor',
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
          description: 'Action to perform'
        },
        actorName: { type: 'string' },
        childActor: { type: 'string', description: 'Name of the child actor (alias for actorName in detach, or specific child for attach).' },
        parentActor: { type: 'string' },
        classPath: { type: 'string' },
        meshPath: { type: 'string' },
        blueprintPath: { type: 'string' },
        location: commonSchemas.location,
        rotation: commonSchemas.rotation,
        scale: commonSchemas.scale,
        force: commonSchemas.vector3,
        componentType: { type: 'string' },
        componentName: { type: 'string' },
        properties: { type: 'object' },
        visible: { type: 'boolean' },
        newName: { type: 'string' },
        tag: { type: 'string' },
        variables: { type: 'object' },
        snapshotName: { type: 'string' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        actor: { type: 'string' },
        actorPath: { type: 'string' },
        message: { type: 'string' },
        components: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              name: { type: 'string' },
              class: { type: 'string' },
              relativeLocation: { type: 'object', properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } } },
              relativeRotation: { type: 'object', properties: { pitch: { type: 'number' }, yaw: { type: 'number' }, roll: { type: 'number' } } },
              relativeScale: { type: 'object', properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } } }
            }
          }
        },
        data: { type: 'object' }
      }
    }
  },

  // 4. EDITOR CONTROL
  {
    name: 'control_editor',
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
        viewMode: { type: 'string' },
        enabled: { type: 'boolean', description: 'For set_viewport_realtime.' },
        speed: { type: 'number' },
        filename: { type: 'string' },
        fov: { type: 'number' },
        width: { type: 'number' },
        height: { type: 'number' },
        command: { type: 'string' },
        steps: { type: 'integer' },
        bookmarkName: { type: 'string' },
        assetPath: { type: 'string' },
        // Simulate Input
        keyName: { type: 'string' },
        eventType: { type: 'string', enum: ['KeyDown', 'KeyUp', 'Both'] }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        message: { type: 'string' }
      }
    }
  },

  // 5. LEVEL MANAGER
  {
    name: 'manage_level',
    description: 'Load/save levels, configure streaming, manage World Partition cells, and build lighting.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'load', 'save', 'save_as', 'save_level_as', 'stream', 'create_level', 'create_light', 'build_lighting',
            'set_metadata', 'load_cells', 'set_datalayer',
            'export_level', 'import_level', 'list_levels', 'get_summary', 'delete', 'validate_level',
            'cleanup_invalid_datalayers', 'add_sublevel'
          ],
          description: 'Action'
        },
        levelPath: { type: 'string', description: 'Required for load/save actions and get_summary.' },
        levelName: { type: 'string' },
        streaming: { type: 'boolean' },
        shouldBeLoaded: { type: 'boolean' },
        shouldBeVisible: { type: 'boolean' },
        // Lighting
        lightType: { type: 'string', enum: ['Directional', 'Point', 'Spot', 'Rect'] },
        location: commonSchemas.location,
        intensity: { type: 'number' },
        quality: { type: 'string' },
        // World Partition
        min: { type: 'array', items: { type: 'number' } },
        max: { type: 'array', items: { type: 'number' } },
        dataLayerLabel: { type: 'string' },
        dataLayerState: { type: 'string' },
        recursive: { type: 'boolean' },
        // Export/Import
        exportPath: { type: 'string' },
        packagePath: { type: 'string' },
        destinationPath: { type: 'string' },
        note: { type: 'string' },
        // Delete
        levelPaths: { type: 'array', items: { type: 'string' } },
        // Sublevel
        subLevelPath: { type: 'string' },
        parentLevel: { type: 'string' },
        streamingMethod: { type: 'string', enum: ['Blueprint', 'AlwaysLoaded'] }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        message: { type: 'string' },
        data: { type: 'object' }
      }
    }
  },

  // 6. ANIMATION & PHYSICS
  {
    name: 'animation_physics',
    description: 'Create Animation BPs, Montages, Blend Spaces, IK rigs, ragdolls, and vehicles (wheeled/hover/flying).',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'create_animation_bp', 'play_montage', 'setup_ragdoll', 'activate_ragdoll', 'configure_vehicle',
            'create_blend_space', 'create_state_machine', 'setup_ik', 'create_procedural_anim',
            'create_blend_tree', 'setup_retargeting', 'setup_physics_simulation', 'cleanup',
            'create_animation_asset', 'add_notify'
          ],
          description: 'Action'
        },
        name: { type: 'string' },
        actorName: { type: 'string' },
        skeletonPath: { type: 'string' },
        montagePath: { type: 'string' },
        animationPath: { type: 'string' },
        playRate: { type: 'number' },
        physicsAssetName: { type: 'string' },
        meshPath: { type: 'string' },
        vehicleName: { type: 'string' },
        vehicleType: { type: 'string' },
        // ... (omitting detailed vehicle/IK schema for brevity, keeping core structure)
        savePath: { type: 'string' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        message: { type: 'string' }
      }
    }
  },

  // 7. EFFECTS MANAGER (Niagara & Particles)
  {
    name: 'manage_effect',
    description: 'Spawn Niagara/Cascade particles, draw debug shapes, and edit VFX node graphs.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'particle', 'niagara', 'debug_shape', 'spawn_niagara', 'create_dynamic_light',
            'create_niagara_system', 'create_niagara_emitter',
            'create_volumetric_fog', 'create_particle_trail', 'create_environment_effect', 'create_impact_effect', 'create_niagara_ribbon',
            'activate', 'activate_effect', 'deactivate', 'reset', 'advance_simulation',
            'add_niagara_module', 'connect_niagara_pins', 'remove_niagara_node', 'set_niagara_parameter',
            'clear_debug_shapes', 'cleanup', 'list_debug_shapes'
          ],
          description: 'Action'
        },
        name: { type: 'string' },
        systemName: { type: 'string' },
        systemPath: { type: 'string', description: 'Required for spawning Niagara effects (spawn_niagara, create_volumetric_fog, etc) and most graph operations.' },
        preset: { type: 'string', description: 'Required for particle action. Path to particle system asset.' },
        location: commonSchemas.location,
        scale: { type: 'number' },
        shape: { type: 'string', description: 'Supported: sphere, box, cylinder, line, cone, capsule, arrow, plane' },
        size: { type: 'number' },
        color: { type: 'array', items: { type: 'number' } },
        // Graph
        modulePath: { type: 'string' },
        emitterName: { type: 'string' },
        pinName: { type: 'string' },
        linkedTo: { type: 'string' },
        parameterName: { type: 'string' },
        parameterType: { type: 'string', description: 'Float, Vector, Color, Bool, etc.' },
        type: { type: 'string', description: 'Alias for parameterType' },
        value: { description: 'Value.' },
        // Cleanup
        filter: { type: 'string', description: 'Filter for cleanup action. Required.' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        message: { type: 'string' }
      }
    }
  },

  // 8. ENVIRONMENT BUILDER
  {
    name: 'build_environment',
    description: 'Create/sculpt landscapes, paint foliage, and generate procedural terrain/biomes.',
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
            'generate_lods', 'bake_lightmap', 'export_snapshot', 'import_snapshot', 'delete'
          ],
          description: 'Action'
        },
        // Common
        name: { type: 'string', description: 'Name of landscape, foliage type, or procedural volume.' },
        landscapeName: { type: 'string' },
        heightData: { type: 'array', items: { type: 'number' } },
        minX: { type: 'number' },
        minY: { type: 'number' },
        maxX: { type: 'number' },
        maxY: { type: 'number' },
        updateNormals: { type: 'boolean' },
        location: commonSchemas.location,
        rotation: commonSchemas.rotation,
        scale: commonSchemas.scale,

        // Landscape
        sizeX: { type: 'number' },
        sizeY: { type: 'number' },
        sectionSize: { type: 'number' },
        sectionsPerComponent: { type: 'number' },
        componentCount: { type: 'object', properties: { x: { type: 'number' }, y: { type: 'number' } } },
        materialPath: { type: 'string' },

        // Sculpt/Paint
        tool: { type: 'string' },
        radius: { type: 'number' },
        strength: { type: 'number' },
        falloff: { type: 'number' },
        brushSize: { type: 'number' },
        layerName: { type: 'string', description: 'Required for paint_landscape.' },
        eraseMode: { type: 'boolean' },

        // Foliage
        foliageType: { type: 'string', description: 'Required for add_foliage_instances, paint_foliage.' },
        foliageTypePath: { type: 'string' },
        meshPath: { type: 'string' },
        density: { type: 'number' },
        minScale: { type: 'number' },
        maxScale: { type: 'number' },
        cullDistance: { type: 'number' },
        alignToNormal: { type: 'boolean' },
        randomYaw: { type: 'boolean' },
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

        // Procedural
        bounds: { type: 'object' },
        volumeName: { type: 'string' },
        seed: { type: 'number' },
        foliageTypes: { type: 'array', items: { type: 'object' } },

        // General
        path: { type: 'string' },
        filename: { type: 'string' },
        assetPaths: { type: 'array', items: { type: 'string' } }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        message: { type: 'string' }
      }
    }
  },

  // 9. SYSTEM CONTROL
  {
    name: 'system_control',
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
            // Added missing actions
            'set_cvar', 'get_project_settings', 'validate_assets',
            'set_project_setting'
          ],
          description: 'Action'
        },
        // Profile/Quality
        profileType: { type: 'string' },
        category: { type: 'string' },
        level: { type: 'number' },
        enabled: { type: 'boolean' },
        resolution: { type: 'string', description: 'Resolution string (e.g. "1920x1080").' },

        // Commands
        command: { type: 'string' },

        // UBT
        target: { type: 'string' },
        platform: { type: 'string' },
        configuration: { type: 'string' },
        arguments: { type: 'string' },

        // Tests
        filter: { type: 'string' },

        // Insights
        channels: { type: 'string' },

        // UI Widget Management
        widgetPath: { type: 'string', description: 'Path to the widget blueprint (for add_widget_child).' },
        childClass: { type: 'string', description: 'Class of the child widget to add (e.g. /Script/UMG.Button).' },
        parentName: { type: 'string', description: 'Name of the parent widget to add to (optional).' },

        // Project Settings
        section: { type: 'string', description: 'Config section (e.g. /Script/EngineSettings.GeneralProjectSettings).' },
        key: { type: 'string', description: 'Config key (e.g. ProjectID).' },
        value: { type: 'string', description: 'Config value.' },
        configName: { type: 'string', description: 'Config file name (Game, Engine, Input, etc.). Defaults to Game.' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        message: { type: 'string' },
        output: { type: 'string' }
      }
    }
  },

  // 10. SEQUENCER
  {
    name: 'manage_sequence',
    description: 'Edit Level Sequences: add tracks, bind actors, set keyframes, control playback, and record camera.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'create', 'open', 'add_camera', 'add_actor', 'add_actors', 'remove_actors',
            'get_bindings', 'play', 'pause', 'stop', 'set_playback_speed', 'add_keyframe',
            // Added missing sequence actions
            'get_properties', 'set_properties', 'duplicate', 'rename', 'delete', 'list', 'get_metadata', 'set_metadata',
            'add_spawnable_from_class', 'add_track', 'add_section', 'set_display_rate', 'set_tick_resolution',
            'set_work_range', 'set_view_range', 'set_track_muted', 'set_track_solo', 'set_track_locked',
            'list_tracks', 'remove_track', 'list_track_types'
          ],
          description: 'Action'
        },
        name: { type: 'string', description: 'Sequence name for creation.' },
        path: { type: 'string', description: 'Sequence asset path.' },
        actorName: { type: 'string', description: 'Actor name for binding.' },
        actorNames: { type: 'array', items: { type: 'string' }, description: 'Multiple actor names.' },
        frame: { type: 'number', description: 'Frame number for keyframes.' },
        value: { type: 'object', description: 'Value for keyframes.' },
        property: { type: 'string', description: 'Property name for keyframes.' },
        // Duplicate/Rename
        destinationPath: { type: 'string', description: 'Destination path for duplicate.' },
        newName: { type: 'string', description: 'New name for rename/duplicate.' },
        overwrite: { type: 'boolean', description: 'Overwrite existing on duplicate.' },
        // Playback
        speed: { type: 'number', description: 'Playback speed multiplier.' },
        startTime: { type: 'number', description: 'Start time for playback.' },
        loopMode: { type: 'string', description: 'Loop mode for playback.' },
        // Spawnable
        className: { type: 'string', description: 'Class name for spawnables.' },
        spawnable: { type: 'boolean', description: 'Create as spawnable.' },
        // Track management
        trackType: { type: 'string', description: 'Track type (Animation, Transform, Audio, Event).' },
        trackName: { type: 'string', description: 'Track name for track operations.' },
        muted: { type: 'boolean', description: 'Mute state for set_track_muted.' },
        solo: { type: 'boolean', description: 'Solo state for set_track_solo.' },
        locked: { type: 'boolean', description: 'Lock state for set_track_locked.' },
        // Section
        assetPath: { type: 'string', description: 'Asset path for section content.' },
        startFrame: { type: 'number', description: 'Section start frame.' },
        endFrame: { type: 'number', description: 'Section end frame.' },
        // Display settings
        frameRate: { type: 'string', description: 'Display frame rate (e.g., "30fps").' },
        resolution: { type: 'string', description: 'Tick resolution (e.g., "24000fps").' },
        // Work/View range
        start: { type: 'number', description: 'Range start.' },
        end: { type: 'number', description: 'Range end.' },
        // Properties
        lengthInFrames: { type: 'number', description: 'Sequence length in frames.' },
        playbackStart: { type: 'number', description: 'Playback start frame.' },
        playbackEnd: { type: 'number', description: 'Playback end frame.' },
        // Metadata
        metadata: { type: 'object', description: 'Metadata object.' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        message: { type: 'string' }
      }
    }
  },

  // 11. INPUT MANAGEMENT
  {
    name: 'manage_input',
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
        name: { type: 'string', description: 'Name of the asset (for creation).' },
        path: { type: 'string', description: 'Path to save the asset (e.g. /Game/Input).' },
        contextPath: { type: 'string', description: 'Path to the Input Mapping Context.' },
        actionPath: { type: 'string', description: 'Path to the Input Action.' },
        key: { type: 'string', description: 'Key name (e.g. "SpaceBar", "W", "Gamepad_FaceButton_Bottom").' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        message: { type: 'string' },
        assetPath: { type: 'string' }
      }
    }
  },

  // 12. INTROSPECTION (INSPECT)
  {
    name: 'inspect',
    description: 'Inspect any UObject: read/write properties, list components, export snapshots, and query class info.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'inspect_object', 'set_property', 'get_property', 'get_components', 'inspect_class', 'list_objects',
            // Added missing inspect actions
            'get_component_property', 'set_component_property', 'get_metadata', 'add_tag', 'find_by_tag',
            'create_snapshot', 'restore_snapshot', 'export', 'delete_object', 'find_by_class', 'get_bounding_box'
          ],
          description: 'Action'
        },
        objectPath: { type: 'string', description: 'UObject path to inspect/modify.' },
        propertyName: { type: 'string', description: 'Property name to get/set.' },
        propertyPath: { type: 'string', description: 'Alternate property path parameter.' },
        value: { description: 'Value to set.' },
        // Actor/Component identifiers
        actorName: { type: 'string', description: 'Actor name (required for snapshots, export, and component resolution).' },
        name: { type: 'string', description: 'Object name (alternative to objectPath).' },
        componentName: { type: 'string', description: 'Component name for component property access.' },
        // Search/Filter
        className: { type: 'string', description: 'Class name for find_by_class.' },
        classPath: { type: 'string', description: 'Class path (alternative to className).' },
        tag: { type: 'string', description: 'Tag for add_tag/find_by_tag.' },
        filter: { type: 'string', description: 'Filter for list_objects.' },
        // Snapshots
        snapshotName: { type: 'string', description: 'Snapshot name for create/restore.' },
        // Export
        destinationPath: { type: 'string', description: 'Export destination path.' },
        outputPath: { type: 'string', description: 'Alternative export path.' },
        format: { type: 'string', description: 'Export format (e.g., JSON).' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        value: { description: 'Property value.' }
      }
    }
  },

  // 12. AUDIO MANAGER
  {
    name: 'manage_audio',
    description: 'Play/stop sounds, add audio components, configure mixes, attenuation, and spatial audio.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'create_sound_cue', 'play_sound_at_location', 'play_sound_2d', 'create_audio_component',
            'create_sound_mix', 'push_sound_mix', 'pop_sound_mix',
            'set_sound_mix_class_override', 'clear_sound_mix_class_override', 'set_base_sound_mix',
            'prime_sound', 'play_sound_attached', 'spawn_sound_at_location',
            'fade_sound_in', 'fade_sound_out', 'create_ambient_sound',
            // Added missing actions
            'create_sound_class', 'set_sound_attenuation', 'create_reverb_zone',
            'enable_audio_analysis', 'fade_sound', 'set_doppler_effect', 'set_audio_occlusion'
          ],
          description: 'Action'
        },
        name: { type: 'string' },
        soundPath: { type: 'string', description: 'Required for create_sound_cue, play_sound_*, create_audio_component, create_ambient_sound.' },
        location: commonSchemas.location,
        rotation: commonSchemas.rotation,
        volume: { type: 'number' },
        pitch: { type: 'number' },
        startTime: { type: 'number' },
        attenuationPath: { type: 'string' },
        concurrencyPath: { type: 'string' },
        mixName: { type: 'string' },
        soundClassName: { type: 'string' },
        fadeInTime: { type: 'number' },
        fadeOutTime: { type: 'number' },
        fadeTime: { type: 'number' },
        targetVolume: { type: 'number' },
        attachPointName: { type: 'string' },
        actorName: { type: 'string' },
        componentName: { type: 'string' },
        // Added missing parameters
        parentClass: { type: 'string' },
        properties: { type: 'object' },
        innerRadius: { type: 'number' },
        falloffDistance: { type: 'number' },
        attenuationShape: { type: 'string' },
        falloffMode: { type: 'string' },
        reverbEffect: { type: 'string' },
        size: commonSchemas.scale,
        fftSize: { type: 'number' },
        outputType: { type: 'string' },
        soundName: { type: 'string' },
        fadeType: { type: 'string' },
        scale: { type: 'number' },
        lowPassFilterFrequency: { type: 'number' },
        volumeAttenuation: { type: 'number' },
        enabled: { type: 'boolean' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        message: { type: 'string' }
      }
    }
  },

  // 13. BEHAVIOR TREE
  {
    name: 'manage_behavior_tree',
    description: 'Create Behavior Trees, add task/decorator/service nodes, and configure node properties.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: ['create', 'add_node', 'connect_nodes', 'remove_node', 'break_connections', 'set_node_properties'],
          description: 'Action'
        },
        // For create action
        name: { type: 'string', description: 'Name of the new Behavior Tree asset' },
        savePath: { type: 'string', description: 'Path to save the new Behavior Tree (e.g., /Game/AI)' },
        // Existing params
        assetPath: { type: 'string' },
        nodeType: { type: 'string' },
        nodeId: { type: 'string' },
        parentNodeId: { type: 'string' },
        childNodeId: { type: 'string' },
        x: { type: 'number' },
        y: { type: 'number' },
        comment: { type: 'string' },
        properties: { type: 'object' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        message: { type: 'string' },
        nodeId: { type: 'string' }
      }
    }
  },

  // 14. BLUEPRINT GRAPH DIRECT
  {
    name: 'manage_blueprint_graph',
    description: 'Add Blueprint graph nodes (functions, events, variables), connect pins, and set property values.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'create_node', 'delete_node', 'connect_pins', 'break_pin_links', 'set_node_property',
            'create_reroute_node', 'get_node_details', 'get_graph_details', 'get_pin_details',
            'list_node_types', 'set_pin_default_value'
          ],
          description: 'Action'
        },
        blueprintPath: { type: 'string' },
        graphName: { type: 'string' },
        nodeType: { type: 'string' },
        nodeId: { type: 'string' },
        pinName: { type: 'string' },
        linkedTo: { type: 'string' },
        memberName: { type: 'string' },
        x: { type: 'number' },
        y: { type: 'number' },
        propertyName: { type: 'string' },
        value: { description: 'Value.' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        nodeId: { type: 'string' },
        details: { type: 'object' }
      }
    }
  },

  // 15. LIGHTING MANAGER
  {
    name: 'manage_lighting',
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
        // Common
        name: { type: 'string' },
        location: commonSchemas.location,
        rotation: commonSchemas.rotation,

        // Light Properties
        lightType: { type: 'string', enum: ['Directional', 'Point', 'Spot', 'Rect'] },
        intensity: { type: 'number' },
        color: { type: 'array', items: { type: 'number' } },
        castShadows: { type: 'boolean' },
        useAsAtmosphereSunLight: { type: 'boolean', description: 'For Directional Lights, use as Atmosphere Sun Light.' },
        temperature: { type: 'number' },
        radius: { type: 'number' },
        falloffExponent: { type: 'number' },
        innerCone: { type: 'number' },
        outerCone: { type: 'number' },
        width: { type: 'number' },
        height: { type: 'number' },

        // Sky Light
        sourceType: { type: 'string', enum: ['CapturedScene', 'SpecifiedCubemap'] },
        cubemapPath: { type: 'string' },
        recapture: { type: 'boolean' },

        // Global Illumination
        method: { type: 'string', enum: ['Lightmass', 'LumenGI', 'ScreenSpace', 'None'] },
        quality: { type: 'string' }, // 'Low' | 'Medium' | 'High' | 'Epic' | 'Preview' | 'Production'
        indirectLightingIntensity: { type: 'number' },
        bounces: { type: 'number' },

        // Shadows
        shadowQuality: { type: 'string' },
        cascadedShadows: { type: 'boolean' },
        shadowDistance: { type: 'number' },
        contactShadows: { type: 'boolean' },
        rayTracedShadows: { type: 'boolean' },

        // Exposure / Post Process
        compensationValue: { type: 'number' },
        minBrightness: { type: 'number' },
        maxBrightness: { type: 'number' },
        enabled: { type: 'boolean' },

        // Volumetric Fog
        density: { type: 'number' },
        scatteringIntensity: { type: 'number' },
        fogHeight: { type: 'number' },

        // Building
        buildOnlySelected: { type: 'boolean' },
        buildReflectionCaptures: { type: 'boolean' },

        // Level
        levelName: { type: 'string' },
        copyActors: { type: 'boolean' },
        useTemplate: { type: 'boolean' },

        // Volume
        size: commonSchemas.scale
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        message: { type: 'string' },
        actorName: { type: 'string' }
      }
    }
  },

  // 16. PERFORMANCE MANAGER
  {
    name: 'manage_performance',
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
        // Profiling
        type: { type: 'string', enum: ['CPU', 'GPU', 'Memory', 'RenderThread', 'GameThread', 'All'] },
        duration: { type: 'number' },
        outputPath: { type: 'string' },
        detailed: { type: 'boolean' },
        category: { type: 'string' },

        // Settings
        level: { type: 'number' },
        scale: { type: 'number' },
        enabled: { type: 'boolean' },
        maxFPS: { type: 'number' },
        verbose: { type: 'boolean' },

        // Optimization
        poolSize: { type: 'number' },
        boostPlayerLocation: { type: 'boolean' },
        forceLOD: { type: 'number' },
        lodBias: { type: 'number' },
        distanceScale: { type: 'number' },
        skeletalBias: { type: 'number' },
        hzb: { type: 'boolean' },
        enableInstancing: { type: 'boolean' },
        enableBatching: { type: 'boolean' },
        mergeActors: { type: 'boolean' },
        actors: { type: 'array', items: { type: 'string' } },
        freezeRendering: { type: 'boolean' },
        compileOnDemand: { type: 'boolean' },
        cacheShaders: { type: 'boolean' },
        reducePermutations: { type: 'boolean' },

        // Features
        maxPixelsPerEdge: { type: 'number' },
        streamingPoolSize: { type: 'number' },
        streamingDistance: { type: 'number' },
        cellSize: { type: 'number' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        message: { type: 'string' },
        params: { type: 'object' }
      }
    }
  },

  // 17. GEOMETRY MANAGER (Phase 6 - Geometry Script)
  {
    name: 'manage_geometry',
    description: 'Create procedural meshes using Geometry Script: booleans, deformers, UVs, collision, and LOD generation.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            // Primitives (basic)
            'create_box', 'create_sphere', 'create_cylinder', 'create_cone', 'create_capsule',
            'create_torus', 'create_plane', 'create_disc',
            // Primitives (additional)
            'create_stairs', 'create_spiral_stairs', 'create_ring',
            'create_arch', 'create_pipe', 'create_ramp',
            // Booleans
            'boolean_union', 'boolean_subtract', 'boolean_intersection',
            'boolean_trim', 'self_union',
            // Modeling operations
            'extrude', 'inset', 'outset', 'bevel', 'offset_faces', 'shell', 'revolve', 'chamfer',
            'extrude_along_spline', 'bridge', 'loft', 'sweep',
            'duplicate_along_spline', 'loop_cut', 'edge_split', 'quadrangulate',
            // Deformers
            'bend', 'twist', 'taper', 'noise_deform', 'smooth', 'relax',
            'stretch', 'spherify', 'cylindrify',
            // Topology operations
            'triangulate', 'poke',
            // Transform operations
            'mirror', 'array_linear', 'array_radial',
            // Mesh processing
            'simplify_mesh', 'subdivide', 'remesh_uniform', 'merge_vertices', 'remesh_voxel',
            // Mesh repair
            'weld_vertices', 'fill_holes', 'remove_degenerates',
            // UVs
            'auto_uv', 'project_uv', 'transform_uvs', 'unwrap_uv', 'pack_uv_islands',
            // Normals/Tangents
            'recalculate_normals', 'flip_normals', 'recompute_tangents',
            // Collision
            'generate_collision', 'generate_complex_collision', 'simplify_collision',
            // LOD operations
            'generate_lods', 'set_lod_settings', 'set_lod_screen_sizes', 'convert_to_nanite',
            // Export/Convert
            'convert_to_static_mesh',
            // Utils
            'get_mesh_info'
          ],
          description: 'Geometry action to perform'
        },

        // -- Target Identification --
        meshPath: { type: 'string', description: 'Path to target static mesh asset (e.g., /Game/Meshes/MyMesh).' },
        targetMeshPath: { type: 'string', description: 'Path to second mesh for boolean operations.' },
        outputPath: { type: 'string', description: 'Path for output mesh asset.' },
        actorName: { type: 'string', description: 'Name of actor in level (for dynamic mesh operations).' },

        // -- Primitive Dimensions --
        width: { type: 'number', description: 'Width dimension (X axis).' },
        height: { type: 'number', description: 'Height dimension (Z axis).' },
        depth: { type: 'number', description: 'Depth dimension (Y axis).' },
        radius: { type: 'number', description: 'Radius for sphere, cylinder, capsule, torus, disc.' },
        innerRadius: { type: 'number', description: 'Inner radius for torus.' },
        numSides: { type: 'number', description: 'Number of sides for cylinder, cone, etc.' },
        numRings: { type: 'number', description: 'Number of rings for sphere, torus.' },
        numSteps: { type: 'number', description: 'Number of steps for stairs.' },
        stepWidth: { type: 'number', description: 'Width of each stair step.' },
        stepHeight: { type: 'number', description: 'Height of each stair step.' },
        stepDepth: { type: 'number', description: 'Depth of each stair step.' },
        numTurns: { type: 'number', description: 'Number of turns for spiral.' },

        // -- Segments/Subdivisions --
        widthSegments: { type: 'number', description: 'Segments along width.' },
        heightSegments: { type: 'number', description: 'Segments along height.' },
        depthSegments: { type: 'number', description: 'Segments along depth.' },
        radialSegments: { type: 'number', description: 'Radial segments for circular shapes.' },

        // -- Transform --
        location: { type: 'object', properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } }, description: 'World location.' },
        rotation: { type: 'object', properties: { pitch: { type: 'number' }, yaw: { type: 'number' }, roll: { type: 'number' } }, description: 'Rotation.' },
        scale: { type: 'object', properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } }, description: 'Scale.' },

        // -- Modeling Operations --
        distance: { type: 'number', description: 'Extrude distance or offset amount.' },
        amount: { type: 'number', description: 'Generic amount for operations (bevel size, inset distance, etc.).' },
        segments: { type: 'number', description: 'Number of segments for bevel, subdivide.' },
        angle: { type: 'number', description: 'Angle in degrees for bend, twist operations.' },
        axis: { type: 'string', enum: ['X', 'Y', 'Z'], description: 'Axis for deformation operations.' },
        strength: { type: 'number', description: 'Strength/intensity for deformers.' },
        iterations: { type: 'number', description: 'Number of iterations for smooth, remesh.' },
        targetTriangleCount: { type: 'number', description: 'Target triangle count for simplification.' },
        targetEdgeLength: { type: 'number', description: 'Target edge length for remeshing.' },
        weldDistance: { type: 'number', description: 'Distance threshold for vertex welding.' },

        // -- Selection (for face/edge/vertex operations) --
        faceIndices: { type: 'array', items: { type: 'number' }, description: 'Array of face indices.' },
        edgeIndices: { type: 'array', items: { type: 'number' }, description: 'Array of edge indices.' },
        vertexIndices: { type: 'array', items: { type: 'number' }, description: 'Array of vertex indices.' },
        selectionBox: { type: 'object', properties: { min: { type: 'object' }, max: { type: 'object' } }, description: 'Bounding box for selection.' },

        // -- UV Parameters --
        uvChannel: { type: 'number', description: 'UV channel index (0-7).' },
        uvScale: { type: 'object', properties: { u: { type: 'number' }, v: { type: 'number' } }, description: 'UV scale.' },
        uvOffset: { type: 'object', properties: { u: { type: 'number' }, v: { type: 'number' } }, description: 'UV offset.' },
        projectionDirection: { type: 'string', enum: ['X', 'Y', 'Z', 'Auto'], description: 'Projection direction for UV.' },

        // -- Normals --
        hardEdgeAngle: { type: 'number', description: 'Angle threshold for hard edges (degrees).' },
        computeWeightedNormals: { type: 'boolean', description: 'Use area-weighted normals.' },
        smoothingGroupId: { type: 'number', description: 'Smoothing group ID.' },

        // -- Collision --
        collisionType: { type: 'string', enum: ['Default', 'Simple', 'Complex', 'UseComplexAsSimple', 'UseSimpleAsComplex'], description: 'Collision complexity type.' },
        hullCount: { type: 'number', description: 'Number of convex hulls for decomposition.' },
        hullPrecision: { type: 'number', description: 'Precision for convex hull generation (0-1).' },
        maxVerticesPerHull: { type: 'number', description: 'Maximum vertices per convex hull.' },

        // -- LOD Parameters --
        lodCount: { type: 'number', description: 'Number of LOD levels to generate.' },
        lodIndex: { type: 'number', description: 'Specific LOD index to configure.' },
        reductionPercent: { type: 'number', description: 'Percent of triangles to reduce per LOD.' },
        screenSize: { type: 'number', description: 'Screen size threshold for LOD switching.' },
        screenSizes: { type: 'array', items: { type: 'number' }, description: 'Array of screen sizes for each LOD.' },
        preserveBorders: { type: 'boolean', description: 'Preserve mesh borders during LOD generation.' },
        preserveUVs: { type: 'boolean', description: 'Preserve UV seams during LOD generation.' },

        // -- Export --
        exportFormat: { type: 'string', enum: ['FBX', 'OBJ', 'glTF', 'USD'], description: 'Export file format.' },
        exportPath: { type: 'string', description: 'File system path for export.' },
        includeNormals: { type: 'boolean', description: 'Include normals in export.' },
        includeUVs: { type: 'boolean', description: 'Include UVs in export.' },
        includeTangents: { type: 'boolean', description: 'Include tangents in export.' },

        // -- Options --
        createAsset: { type: 'boolean', description: 'Create as persistent asset.' },
        overwrite: { type: 'boolean', description: 'Overwrite existing asset.' },
        save: { type: 'boolean', description: 'Save asset after operation.' },
        enableNanite: { type: 'boolean', description: 'Enable Nanite for the output mesh.' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        message: { type: 'string' },
        meshPath: { type: 'string', description: 'Path to created/modified mesh.' },
        actorName: { type: 'string', description: 'Name of spawned actor (if applicable).' },
        meshInfo: {
          type: 'object',
          properties: {
            vertexCount: { type: 'number' },
            triangleCount: { type: 'number' },
            uvChannels: { type: 'number' },
            hasNormals: { type: 'boolean' },
            hasTangents: { type: 'boolean' },
            boundingBox: { type: 'object' },
            lodCount: { type: 'number' }
          },
          description: 'Mesh statistics (for get_mesh_info).'
        },
        error: { type: 'string' }
      }
    }
  },

  // 18. SKELETON MANAGER (Phase 7 - Skeletal Mesh & Rigging)
  {
    name: 'manage_skeleton',
    description: 'Edit skeletal meshes: add sockets, configure physics assets, set skin weights, and create morph targets.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            // 7.1 Skeleton Creation
            'create_skeleton', 'add_bone', 'remove_bone', 'rename_bone',
            'set_bone_transform', 'set_bone_parent',
            'create_virtual_bone',
            'create_socket', 'configure_socket',
            // 7.2 Skin Weights
            'auto_skin_weights', 'set_vertex_weights',
            'normalize_weights', 'prune_weights',
            'copy_weights', 'mirror_weights',
            // 7.3 Physics Asset
            'create_physics_asset',
            'add_physics_body', 'configure_physics_body',
            'add_physics_constraint', 'configure_constraint_limits',
            // 7.4 Cloth Setup
            'bind_cloth_to_skeletal_mesh', 'assign_cloth_asset_to_mesh',
            // 7.5 Morph Targets
            'create_morph_target', 'set_morph_target_deltas', 'import_morph_targets',
            // Utils
            'get_skeleton_info', 'list_bones', 'list_sockets', 'list_physics_bodies'
          ],
          description: 'Skeleton action to perform'
        },

        // -- Asset Identification --
        skeletonPath: { type: 'string', description: 'Path to skeleton asset (e.g., /Game/Characters/MySkeleton).' },
        skeletalMeshPath: { type: 'string', description: 'Path to skeletal mesh asset.' },
        physicsAssetPath: { type: 'string', description: 'Path to physics asset.' },
        morphTargetPath: { type: 'string', description: 'Path to morph target or FBX file for import.' },
        clothAssetPath: { type: 'string', description: 'Path to cloth asset.' },
        outputPath: { type: 'string', description: 'Path for output asset creation.' },

        // -- Bone Parameters --
        boneName: { type: 'string', description: 'Name of the bone to create/modify.' },
        newBoneName: { type: 'string', description: 'New name for rename_bone action.' },
        parentBoneName: { type: 'string', description: 'Parent bone name for hierarchy.' },
        sourceBoneName: { type: 'string', description: 'Source bone for virtual bone.' },
        targetBoneName: { type: 'string', description: 'Target bone for virtual bone.' },
        boneIndex: { type: 'number', description: 'Bone index for operations.' },

        // -- Transform --
        location: { type: 'object', properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } }, description: 'Position in world or local space.' },
        rotation: { type: 'object', properties: { pitch: { type: 'number' }, yaw: { type: 'number' }, roll: { type: 'number' } }, description: 'Rotation in degrees.' },
        scale: { type: 'object', properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } }, description: 'Scale factor.' },

        // -- Socket Parameters --
        socketName: { type: 'string', description: 'Name of the socket.' },
        attachBoneName: { type: 'string', description: 'Bone to attach socket to.' },
        relativeLocation: { type: 'object', properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } }, description: 'Socket offset from bone.' },
        relativeRotation: { type: 'object', properties: { pitch: { type: 'number' }, yaw: { type: 'number' }, roll: { type: 'number' } }, description: 'Socket rotation offset.' },
        relativeScale: { type: 'object', properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } }, description: 'Socket scale.' },

        // -- Skin Weight Parameters --
        vertexIndex: { type: 'number', description: 'Vertex index for weight operations.' },
        vertexIndices: { type: 'array', items: { type: 'number' }, description: 'Array of vertex indices.' },
        weights: { type: 'array', items: { type: 'object' }, description: 'Array of {boneIndex, weight} pairs.' },
        threshold: { type: 'number', description: 'Weight threshold for pruning (0-1).' },
        mirrorAxis: { type: 'string', enum: ['X', 'Y', 'Z'], description: 'Axis for weight mirroring.' },
        mirrorTable: { type: 'object', description: 'Bone name mapping for mirroring.' },

        // -- Physics Asset Parameters --
        bodyType: { type: 'string', enum: ['Capsule', 'Sphere', 'Box', 'Convex', 'Sphyl'], description: 'Physics body shape type.' },
        bodyName: { type: 'string', description: 'Name of the physics body.' },
        mass: { type: 'number', description: 'Body mass in kg.' },
        linearDamping: { type: 'number', description: 'Linear damping factor.' },
        angularDamping: { type: 'number', description: 'Angular damping factor.' },
        collisionEnabled: { type: 'boolean', description: 'Enable collision for this body.' },
        simulatePhysics: { type: 'boolean', description: 'Enable physics simulation.' },

        // -- Constraint Parameters --
        constraintName: { type: 'string', description: 'Name of the physics constraint.' },
        bodyA: { type: 'string', description: 'First body for constraint.' },
        bodyB: { type: 'string', description: 'Second body for constraint.' },
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

        // -- Morph Target Parameters --
        morphTargetName: { type: 'string', description: 'Name of the morph target.' },
        deltas: { type: 'array', items: { type: 'object' }, description: 'Array of {vertexIndex, delta} for morph target.' },

        // -- Cloth Parameters --
        paintValue: { type: 'number', description: 'Cloth weight paint value (0-1).' },

        // -- Options --
        save: { type: 'boolean', description: 'Save asset after operation.' },
        overwrite: { type: 'boolean', description: 'Overwrite existing asset.' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        message: { type: 'string' },
        skeletonPath: { type: 'string', description: 'Path to created/modified skeleton.' },
        physicsAssetPath: { type: 'string', description: 'Path to created physics asset.' },
        socketName: { type: 'string', description: 'Name of created socket.' },
        boneInfo: {
          type: 'object',
          properties: {
            name: { type: 'string' },
            index: { type: 'number' },
            parentName: { type: 'string' },
            parentIndex: { type: 'number' }
          },
          description: 'Bone information.'
        },
        bones: { type: 'array', items: { type: 'object' }, description: 'List of bones (for list_bones).' },
        sockets: { type: 'array', items: { type: 'object' }, description: 'List of sockets (for list_sockets).' },
        physicsBodies: { type: 'array', items: { type: 'object' }, description: 'List of physics bodies.' },
        error: { type: 'string' }
      }
    }
  },

  // 19. MATERIAL AUTHORING (Phase 8 - Advanced Material Creation)
  {
    name: 'manage_material_authoring',
    description: 'Create materials with expressions, parameters, functions, instances, and landscape blend layers.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            // Material Creation
            'create_material', 'set_blend_mode', 'set_shading_model', 'set_material_domain',
            // Expressions
            'add_texture_sample', 'add_texture_coordinate', 'add_scalar_parameter', 'add_vector_parameter',
            'add_static_switch_parameter', 'add_math_node', 'add_world_position', 'add_vertex_normal',
            'add_pixel_depth', 'add_fresnel', 'add_reflection_vector', 'add_panner', 'add_rotator',
            'add_noise', 'add_voronoi', 'add_if', 'add_switch', 'add_custom_expression',
            // Connections
            'connect_nodes', 'disconnect_nodes',
            // Functions
            'create_material_function', 'add_function_input', 'add_function_output', 'use_material_function',
            // Instances
            'create_material_instance', 'set_scalar_parameter_value', 'set_vector_parameter_value', 'set_texture_parameter_value',
            // Specialized
            'create_landscape_material', 'create_decal_material', 'create_post_process_material',
            'add_landscape_layer', 'configure_layer_blend',
            // Utils
            'compile_material', 'get_material_info'
          ],
          description: 'Material authoring action to perform'
        },

        // -- Asset Identification --
        assetPath: { type: 'string', description: 'Path to material asset (e.g., /Game/Materials/MyMaterial).' },
        name: { type: 'string', description: 'Name of new material or function.' },
        path: { type: 'string', description: 'Directory to create asset in (e.g., /Game/Materials).' },

        // -- Material Properties --
        materialDomain: { type: 'string', enum: ['Surface', 'DeferredDecal', 'LightFunction', 'Volume', 'PostProcess', 'UI'], description: 'Material domain type.' },
        blendMode: { type: 'string', enum: ['Opaque', 'Masked', 'Translucent', 'Additive', 'Modulate', 'AlphaComposite', 'AlphaHoldout'], description: 'Blend mode.' },
        shadingModel: { type: 'string', enum: ['DefaultLit', 'Unlit', 'Subsurface', 'SubsurfaceProfile', 'PreintegratedSkin', 'ClearCoat', 'Hair', 'Cloth', 'Eye', 'TwoSidedFoliage', 'ThinTranslucent'], description: 'Shading model.' },
        twoSided: { type: 'boolean', description: 'Enable two-sided rendering.' },

        // -- Node Positioning --
        x: { type: 'number', description: 'Node X position in material graph.' },
        y: { type: 'number', description: 'Node Y position in material graph.' },

        // -- Texture Sample --
        texturePath: { type: 'string', description: 'Path to texture asset for sampling.' },
        samplerType: { type: 'string', enum: ['Color', 'LinearColor', 'Normal', 'Masks', 'Alpha', 'VirtualColor', 'VirtualNormal'], description: 'Texture sampler type.' },

        // -- Texture Coordinate --
        coordinateIndex: { type: 'number', description: 'UV channel index (0-7).' },
        uTiling: { type: 'number', description: 'U tiling factor.' },
        vTiling: { type: 'number', description: 'V tiling factor.' },

        // -- Parameters --
        parameterName: { type: 'string', description: 'Name of the parameter.' },
        defaultValue: { description: 'Default value for parameter (number for scalar, object for vector, bool for switch).' },
        group: { type: 'string', description: 'Parameter group name.' },
        value: { description: 'Value to set (number, vector object, or texture path).' },

        // -- Math Node --
        operation: { type: 'string', enum: ['Add', 'Subtract', 'Multiply', 'Divide', 'Lerp', 'Clamp', 'Power', 'SquareRoot', 'Abs', 'Floor', 'Ceil', 'Frac', 'Sine', 'Cosine', 'Saturate', 'OneMinus', 'Min', 'Max', 'Dot', 'Cross', 'Normalize', 'Append'], description: 'Math operation type.' },
        constA: { type: 'number', description: 'Constant A input value.' },
        constB: { type: 'number', description: 'Constant B input value.' },

        // -- Custom Expression --
        code: { type: 'string', description: 'HLSL code for custom expression.' },
        outputType: { type: 'string', enum: ['Float1', 'Float2', 'Float3', 'Float4', 'MaterialAttributes'], description: 'Output type of custom expression.' },
        description: { type: 'string', description: 'Description for custom expression or function.' },

        // -- Node Connections --
        sourceNodeId: { type: 'string', description: 'Source node ID for connection.' },
        sourcePin: { type: 'string', description: 'Source pin name (output).' },
        targetNodeId: { type: 'string', description: 'Target node ID for connection.' },
        targetPin: { type: 'string', description: 'Target pin name (input).' },
        nodeId: { type: 'string', description: 'Node ID for disconnect operations.' },
        pinName: { type: 'string', description: 'Pin name for disconnect operations.' },

        // -- Material Function --
        functionPath: { type: 'string', description: 'Path to material function asset.' },
        exposeToLibrary: { type: 'boolean', description: 'Expose function to material library.' },
        inputName: { type: 'string', description: 'Name of function input/output.' },
        inputType: { type: 'string', enum: ['Float1', 'Float2', 'Float3', 'Float4', 'Texture2D', 'TextureCube', 'Bool', 'MaterialAttributes'], description: 'Type of function input/output.' },

        // -- Material Instance --
        parentMaterial: { type: 'string', description: 'Path to parent material for instances.' },

        // -- Landscape Material --
        layerName: { type: 'string', description: 'Landscape layer name.' },
        blendType: { type: 'string', enum: ['LB_WeightBlend', 'LB_AlphaBlend', 'LB_HeightBlend'], description: 'Landscape layer blend type.' },
        layers: { type: 'array', items: { type: 'object' }, description: 'Array of layer configurations for layer blend.' },

        // -- Options --
        save: { type: 'boolean', description: 'Save asset after operation.' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        message: { type: 'string' },
        assetPath: { type: 'string', description: 'Path to created/modified material.' },
        nodeId: { type: 'string', description: 'ID of created node.' },
        materialInfo: {
          type: 'object',
          properties: {
            domain: { type: 'string' },
            blendMode: { type: 'string' },
            shadingModel: { type: 'string' },
            twoSided: { type: 'boolean' },
            nodeCount: { type: 'number' },
            parameters: { type: 'array', items: { type: 'object' } }
          },
          description: 'Material information (for get_material_info).'
        },
        error: { type: 'string' }
      }
    }
  },

  // 20. TEXTURE MANAGEMENT (Phase 9 - Texture Generation & Processing)
  {
    name: 'manage_texture',
    description: 'Create procedural textures, process images, bake normal/AO maps, and set compression settings.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            // Procedural Generation
            'create_noise_texture', 'create_gradient_texture', 'create_pattern_texture',
            'create_normal_from_height', 'create_ao_from_mesh',
            // Processing
            'resize_texture', 'adjust_levels', 'adjust_curves', 'blur', 'sharpen',
            'invert', 'desaturate', 'channel_pack', 'channel_extract', 'combine_textures',
            // Settings
            'set_compression_settings', 'set_texture_group', 'set_lod_bias',
            'configure_virtual_texture', 'set_streaming_priority',
            // Utility
            'get_texture_info'
          ],
          description: 'Texture action to perform'
        },

        // -- Asset Identification --
        assetPath: { type: 'string', description: 'Path to texture asset (e.g., /Game/Textures/MyTexture).' },
        name: { type: 'string', description: 'Name of new texture to create.' },
        path: { type: 'string', description: 'Directory to create texture in (e.g., /Game/Textures).' },

        // -- Dimensions --
        width: { type: 'number', description: 'Texture width in pixels.' },
        height: { type: 'number', description: 'Texture height in pixels.' },
        newWidth: { type: 'number', description: 'New width for resize operation.' },
        newHeight: { type: 'number', description: 'New height for resize operation.' },

        // -- Noise Parameters --
        noiseType: { type: 'string', enum: ['Perlin', 'Simplex', 'Worley', 'Voronoi'], description: 'Type of noise to generate.' },
        scale: { type: 'number', description: 'Noise scale/frequency.' },
        octaves: { type: 'number', description: 'Number of noise octaves for FBM.' },
        persistence: { type: 'number', description: 'Amplitude falloff per octave.' },
        lacunarity: { type: 'number', description: 'Frequency multiplier per octave.' },
        seed: { type: 'number', description: 'Random seed for procedural generation.' },
        seamless: { type: 'boolean', description: 'Generate seamless/tileable texture.' },

        // -- Gradient Parameters --
        gradientType: { type: 'string', enum: ['Linear', 'Radial', 'Angular'], description: 'Type of gradient.' },
        startColor: { type: 'object', description: 'Start color {r, g, b, a}.' },
        endColor: { type: 'object', description: 'End color {r, g, b, a}.' },
        angle: { type: 'number', description: 'Rotation angle in degrees.' },
        centerX: { type: 'number', description: 'Center X position (0-1) for radial gradient.' },
        centerY: { type: 'number', description: 'Center Y position (0-1) for radial gradient.' },
        radius: { type: 'number', description: 'Radius for radial gradient (0-1).' },
        colorStops: { type: 'array', items: { type: 'object' }, description: 'Array of {position, color} for multi-color gradients.' },

        // -- Pattern Parameters --
        patternType: { type: 'string', enum: ['Checker', 'Grid', 'Brick', 'Tile', 'Dots', 'Stripes'], description: 'Type of pattern.' },
        primaryColor: { type: 'object', description: 'Primary pattern color {r, g, b, a}.' },
        secondaryColor: { type: 'object', description: 'Secondary pattern color {r, g, b, a}.' },
        tilesX: { type: 'number', description: 'Number of pattern tiles horizontally.' },
        tilesY: { type: 'number', description: 'Number of pattern tiles vertically.' },
        lineWidth: { type: 'number', description: 'Line width for grid/stripes (0-1).' },
        brickRatio: { type: 'number', description: 'Width/height ratio for brick pattern.' },
        offset: { type: 'number', description: 'Brick offset ratio (0-1).' },

        // -- Normal Map Generation --
        sourceTexture: { type: 'string', description: 'Source height map texture path.' },
        strength: { type: 'number', description: 'Normal map strength/intensity.' },
        algorithm: { type: 'string', enum: ['Sobel', 'Prewitt', 'Scharr'], description: 'Normal calculation algorithm.' },
        flipY: { type: 'boolean', description: 'Flip green channel for DirectX/OpenGL compatibility.' },

        // -- AO Baking --
        meshPath: { type: 'string', description: 'Path to mesh for AO baking.' },
        samples: { type: 'number', description: 'Number of AO samples.' },
        rayDistance: { type: 'number', description: 'Maximum ray distance for AO.' },
        bias: { type: 'number', description: 'AO bias to prevent self-occlusion.' },
        uvChannel: { type: 'number', description: 'UV channel to use for baking.' },

        // -- Resize Parameters --
        filterMethod: { type: 'string', enum: ['Nearest', 'Bilinear', 'Bicubic', 'Lanczos'], description: 'Resize filter method.' },
        preserveAspect: { type: 'boolean', description: 'Preserve aspect ratio when resizing.' },
        outputPath: { type: 'string', description: 'Output path (defaults to overwrite source).' },

        // -- Levels/Curves --
        inputBlackPoint: { type: 'number', description: 'Input black point (0-1).' },
        inputWhitePoint: { type: 'number', description: 'Input white point (0-1).' },
        gamma: { type: 'number', description: 'Gamma correction value.' },
        outputBlackPoint: { type: 'number', description: 'Output black point (0-1).' },
        outputWhitePoint: { type: 'number', description: 'Output white point (0-1).' },
        curvePoints: { type: 'array', items: { type: 'object' }, description: 'Array of {x, y} curve control points.' },

        // -- Blur/Sharpen --
        blurType: { type: 'string', enum: ['Gaussian', 'Box', 'Radial'], description: 'Type of blur.' },
        sharpenType: { type: 'string', enum: ['UnsharpMask', 'Laplacian'], description: 'Type of sharpening.' },

        // -- Channel Operations --
        channel: { type: 'string', enum: ['All', 'Red', 'Green', 'Blue', 'Alpha'], description: 'Target channel.' },
        invertAlpha: { type: 'boolean', description: 'Whether to invert alpha channel.' },
        amount: { type: 'number', description: 'Effect amount (0-1 for desaturate).' },
        method: { type: 'string', enum: ['Luminance', 'Average', 'Lightness'], description: 'Desaturation method.' },
        outputAsGrayscale: { type: 'boolean', description: 'Output extracted channel as grayscale.' },

        // -- Channel Pack Sources --
        redChannel: { type: 'string', description: 'Source texture for red channel.' },
        greenChannel: { type: 'string', description: 'Source texture for green channel.' },
        blueChannel: { type: 'string', description: 'Source texture for blue channel.' },
        alphaChannel: { type: 'string', description: 'Source texture for alpha channel.' },
        redSourceChannel: { type: 'string', enum: ['Red', 'Green', 'Blue', 'Alpha'], description: 'Which channel to use from red source.' },
        greenSourceChannel: { type: 'string', enum: ['Red', 'Green', 'Blue', 'Alpha'], description: 'Which channel to use from green source.' },
        blueSourceChannel: { type: 'string', enum: ['Red', 'Green', 'Blue', 'Alpha'], description: 'Which channel to use from blue source.' },
        alphaSourceChannel: { type: 'string', enum: ['Red', 'Green', 'Blue', 'Alpha'], description: 'Which channel to use from alpha source.' },

        // -- Combine Textures --
        baseTexture: { type: 'string', description: 'Base texture path for combining.' },
        blendTexture: { type: 'string', description: 'Blend texture path for combining.' },
        blendMode: { type: 'string', enum: ['Multiply', 'Add', 'Subtract', 'Screen', 'Overlay', 'SoftLight', 'HardLight', 'Difference', 'Normal'], description: 'Blend mode for combining textures.' },
        opacity: { type: 'number', description: 'Blend opacity (0-1).' },
        maskTexture: { type: 'string', description: 'Optional mask texture for blending.' },

        // -- Compression Settings --
        compressionSettings: {
          type: 'string',
          enum: ['TC_Default', 'TC_Normalmap', 'TC_Masks', 'TC_Grayscale', 'TC_Displacementmap',
                 'TC_VectorDisplacementmap', 'TC_HDR', 'TC_EditorIcon', 'TC_Alpha',
                 'TC_DistanceFieldFont', 'TC_HDR_Compressed', 'TC_BC7'],
          description: 'Texture compression setting.'
        },

        // -- Texture Group --
        textureGroup: {
          type: 'string',
          description: 'Texture group (TEXTUREGROUP_World, TEXTUREGROUP_Character, TEXTUREGROUP_UI, etc.).'
        },

        // -- LOD and Streaming --
        lodBias: { type: 'number', description: 'LOD bias (-2 to 4, lower = higher quality).' },
        virtualTextureStreaming: { type: 'boolean', description: 'Enable virtual texture streaming.' },
        tileSize: { type: 'number', description: 'Virtual texture tile size (32, 64, 128, 256, 512, 1024).' },
        tileBorderSize: { type: 'number', description: 'Virtual texture tile border size.' },
        neverStream: { type: 'boolean', description: 'Disable texture streaming.' },
        streamingPriority: { type: 'number', description: 'Streaming priority (-1 to 1, lower = higher priority).' },

        // -- HDR --
        hdr: { type: 'boolean', description: 'Create HDR texture (16-bit float).' },

        // -- Options --
        save: { type: 'boolean', description: 'Save asset after operation.' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        message: { type: 'string' },
        assetPath: { type: 'string', description: 'Path to created/modified texture.' },
        textureInfo: {
          type: 'object',
          properties: {
            width: { type: 'number' },
            height: { type: 'number' },
            format: { type: 'string' },
            compression: { type: 'string' },
            textureGroup: { type: 'string' },
            mipCount: { type: 'number' },
            sRGB: { type: 'boolean' },
            hasAlpha: { type: 'boolean' },
            virtualTextureStreaming: { type: 'boolean' },
            neverStream: { type: 'boolean' }
          },
          description: 'Texture information (for get_texture_info).'
        },
        error: { type: 'string' }
      }
    }
  },

  // ============================================================================
  // 21. Animation Authoring (Phase 10)
  // ============================================================================
  {
    name: 'manage_animation_authoring',
    description: 'Create Animation Sequences, Montages, Blend Spaces, Animation BPs, Control Rigs, and IK Retargeters.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            // Animation Sequences (10)
            'create_animation_sequence',
            'set_sequence_length',
            'add_bone_track',
            'set_bone_key',
            'set_curve_key',
            'add_notify',
            'add_notify_state',
            'add_sync_marker',
            'set_root_motion_settings',
            'set_additive_settings',
            // Animation Montages (8)
            'create_montage',
            'add_montage_section',
            'add_montage_slot',
            'set_section_timing',
            'add_montage_notify',
            'set_blend_in',
            'set_blend_out',
            'link_sections',
            // Blend Spaces (7)
            'create_blend_space_1d',
            'create_blend_space_2d',
            'add_blend_sample',
            'set_axis_settings',
            'set_interpolation_settings',
            'create_aim_offset',
            'add_aim_offset_sample',
            // Animation Blueprints (10)
            'create_anim_blueprint',
            'add_state_machine',
            'add_state',
            'add_transition',
            'set_transition_rules',
            'add_blend_node',
            'add_cached_pose',
            'add_slot_node',
            'add_layered_blend_per_bone',
            'set_anim_graph_node_value',
            // Control Rig (5)
            'create_control_rig',
            'add_control',
            'add_rig_unit',
            'connect_rig_elements',
            'create_pose_library',
            // Retargeting (4)
            'create_ik_rig',
            'add_ik_chain',
            'create_ik_retargeter',
            'set_retarget_chain_mapping',
            // Utility (1)
            'get_animation_info'
          ],
          description: 'Animation authoring action to perform.'
        },
        // Common parameters
        name: { type: 'string', description: 'Asset name for creation.' },
        path: { type: 'string', description: 'Directory path for asset creation.' },
        assetPath: { type: 'string', description: 'Path to existing animation asset.' },
        skeletonPath: { type: 'string', description: 'Path to skeleton asset.' },
        skeletalMeshPath: { type: 'string', description: 'Path to skeletal mesh asset.' },
        blueprintPath: { type: 'string', description: 'Path to animation blueprint.' },
        save: { type: 'boolean', description: 'Save asset after modification.' },

        // Animation Sequence parameters
        numFrames: { type: 'number', description: 'Number of frames in sequence.' },
        frameRate: { type: 'number', description: 'Frames per second.' },
        boneName: { type: 'string', description: 'Name of bone for track/key.' },
        frame: { type: 'number', description: 'Frame number for keyframe.' },
        location: { type: 'object', description: 'Location {x, y, z}.' },
        rotation: { type: 'object', description: 'Rotation {pitch, yaw, roll} or quaternion {x, y, z, w}.' },
        scale: { type: 'object', description: 'Scale {x, y, z}.' },
        curveName: { type: 'string', description: 'Name of animation curve.' },
        value: { type: 'number', description: 'Curve/property value.' },
        createIfMissing: { type: 'boolean', description: 'Create curve if it does not exist.' },
        notifyClass: { type: 'string', description: 'Notify class name (e.g., AnimNotify_PlaySound).' },
        notifyName: { type: 'string', description: 'Optional custom notify name.' },
        trackIndex: { type: 'number', description: 'Notify track index.' },
        startFrame: { type: 'number', description: 'Start frame for notify state.' },
        endFrame: { type: 'number', description: 'End frame for notify state.' },
        markerName: { type: 'string', description: 'Sync marker name.' },
        enableRootMotion: { type: 'boolean', description: 'Enable root motion.' },
        rootMotionRootLock: { type: 'string', enum: ['RefPose', 'AnimFirstFrame', 'Zero'], description: 'Root motion lock mode.' },
        forceRootLock: { type: 'boolean', description: 'Force root lock.' },
        additiveAnimType: { type: 'string', enum: ['NoAdditive', 'LocalSpaceAdditive', 'MeshSpaceAdditive'], description: 'Additive animation type.' },
        basePoseType: { type: 'string', enum: ['RefPose', 'AnimationFrame', 'AnimationScaled'], description: 'Base pose type for additive.' },
        basePoseAnimation: { type: 'string', description: 'Base pose animation path.' },
        basePoseFrame: { type: 'number', description: 'Base pose frame number.' },

        // Montage parameters
        slotName: { type: 'string', description: 'Slot name (e.g., DefaultSlot).' },
        sectionName: { type: 'string', description: 'Montage section name.' },
        startTime: { type: 'number', description: 'Start time in seconds.' },
        animationPath: { type: 'string', description: 'Path to animation asset.' },
        length: { type: 'number', description: 'Section length in seconds.' },
        time: { type: 'number', description: 'Time position in seconds.' },
        blendTime: { type: 'number', description: 'Blend duration in seconds.' },
        blendOption: { type: 'string', enum: ['Linear', 'Cubic', 'HermiteCubic', 'Sinusoidal', 'QuadraticInOut', 'CubicInOut', 'QuarticInOut', 'QuinticInOut', 'CircularIn', 'CircularOut', 'CircularInOut', 'ExpIn', 'ExpOut', 'ExpInOut'], description: 'Blend curve option.' },
        fromSection: { type: 'string', description: 'Source section name for linking.' },
        toSection: { type: 'string', description: 'Target section name for linking.' },

        // Blend Space parameters
        axisName: { type: 'string', description: 'Axis name (e.g., Speed, Direction).' },
        axisMin: { type: 'number', description: 'Axis minimum value.' },
        axisMax: { type: 'number', description: 'Axis maximum value.' },
        horizontalAxisName: { type: 'string', description: 'Horizontal axis name for 2D blend space.' },
        horizontalMin: { type: 'number', description: 'Horizontal axis minimum.' },
        horizontalMax: { type: 'number', description: 'Horizontal axis maximum.' },
        verticalAxisName: { type: 'string', description: 'Vertical axis name for 2D blend space.' },
        verticalMin: { type: 'number', description: 'Vertical axis minimum.' },
        verticalMax: { type: 'number', description: 'Vertical axis maximum.' },
        sampleValue: { type: ['number', 'object'], description: 'Sample value (number for 1D, {x, y} for 2D).' },
        axis: { type: 'string', enum: ['Horizontal', 'Vertical', 'X'], description: 'Axis to configure.' },
        minValue: { type: 'number', description: 'Axis minimum value.' },
        maxValue: { type: 'number', description: 'Axis maximum value.' },
        gridDivisions: { type: 'number', description: 'Number of grid divisions.' },
        interpolationType: { type: 'string', enum: ['Lerp', 'Cubic'], description: 'Blend interpolation type.' },
        targetWeightInterpolationSpeed: { type: 'number', description: 'Weight interpolation speed.' },
        yaw: { type: 'number', description: 'Yaw angle for aim offset.' },
        pitch: { type: 'number', description: 'Pitch angle for aim offset.' },

        // Animation Blueprint parameters
        parentClass: { type: 'string', description: 'Parent class (e.g., AnimInstance).' },
        stateMachineName: { type: 'string', description: 'State machine name.' },
        stateName: { type: 'string', description: 'State name.' },
        isEntryState: { type: 'boolean', description: 'Mark as entry state.' },
        fromState: { type: 'string', description: 'Source state for transition.' },
        toState: { type: 'string', description: 'Target state for transition.' },
        blendLogicType: { type: 'string', enum: ['StandardBlend', 'Inertialization', 'Custom'], description: 'Transition blend logic type.' },
        automaticTriggerRule: { type: 'string', enum: ['TimeRemaining', 'FractionRemaining', 'None'], description: 'Automatic transition trigger rule.' },
        automaticTriggerTime: { type: 'number', description: 'Time/fraction for automatic trigger.' },
        blendType: { type: 'string', enum: ['TwoWayBlend', 'BlendByBool', 'BlendPosesByBool', 'BlendByInt', 'LayeredBlendPerBone', 'MakeDynamicAdditive', 'ApplyAdditive'], description: 'Blend node type.' },
        nodeName: { type: 'string', description: 'Node name.' },
        x: { type: 'number', description: 'Node X position.' },
        y: { type: 'number', description: 'Node Y position.' },
        cacheName: { type: 'string', description: 'Cached pose name.' },
        layerSetup: { type: 'array', items: { type: 'object' }, description: 'Layer setup for layered blend per bone.' },
        propertyName: { type: 'string', description: 'Property name to set.' },

        // Control Rig parameters
        controlName: { type: 'string', description: 'Control name.' },
        controlType: { type: 'string', enum: ['Transform', 'Bool', 'Float', 'Integer', 'Vector2D', 'EulerTransform'], description: 'Control type.' },
        parentBone: { type: 'string', description: 'Parent bone for control.' },
        parentControl: { type: 'string', description: 'Parent control for hierarchy.' },
        unitType: { type: 'string', enum: ['FKIK', 'Aim', 'BasicIK', 'TwoBoneIK', 'FABRIK', 'SplineIK', 'LimbIK'], description: 'Rig unit type.' },
        unitName: { type: 'string', description: 'Rig unit name.' },
        settings: { type: 'object', description: 'Unit-specific settings.' },
        sourceElement: { type: 'string', description: 'Source element for connection.' },
        sourcePin: { type: 'string', description: 'Source pin name.' },
        targetElement: { type: 'string', description: 'Target element for connection.' },
        targetPin: { type: 'string', description: 'Target pin name.' },

        // IK Retargeting parameters
        chainName: { type: 'string', description: 'IK chain name.' },
        startBone: { type: 'string', description: 'Start bone of IK chain.' },
        endBone: { type: 'string', description: 'End bone of IK chain.' },
        goal: { type: 'string', description: 'IK goal name.' },
        sourceIKRigPath: { type: 'string', description: 'Source IK rig path for retargeter.' },
        targetIKRigPath: { type: 'string', description: 'Target IK rig path for retargeter.' },
        sourceChain: { type: 'string', description: 'Source chain name for mapping.' },
        targetChain: { type: 'string', description: 'Target chain name for mapping.' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        message: { type: 'string' },
        assetPath: { type: 'string', description: 'Path to created/modified asset.' },
        animationInfo: {
          type: 'object',
          properties: {
            assetType: { type: 'string' },
            skeletonPath: { type: 'string' },
            duration: { type: 'number' },
            numFrames: { type: 'number' },
            frameRate: { type: 'number' },
            numBoneTracks: { type: 'number' },
            numCurves: { type: 'number' },
            numNotifies: { type: 'number' },
            isAdditive: { type: 'boolean' },
            hasRootMotion: { type: 'boolean' }
          },
          description: 'Animation asset information (for get_animation_info).'
        },
        error: { type: 'string' }
      }
    }
  },

  // ============================================================================
  // 22. Audio Authoring (Phase 11)
  // ============================================================================
  {
    name: 'manage_audio_authoring',
    description: 'Create Sound Cues, MetaSounds, sound classes/mixes, attenuation settings, and dialogue systems.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            // Sound Cues (5)
            'create_sound_cue',
            'add_cue_node',
            'connect_cue_nodes',
            'set_cue_attenuation',
            'set_cue_concurrency',
            // MetaSounds (6)
            'create_metasound',
            'add_metasound_node',
            'connect_metasound_nodes',
            'add_metasound_input',
            'add_metasound_output',
            'set_metasound_default',
            // Sound Classes & Mixes (6)
            'create_sound_class',
            'set_class_properties',
            'set_class_parent',
            'create_sound_mix',
            'add_mix_modifier',
            'configure_mix_eq',
            // Attenuation & Spatialization (5)
            'create_attenuation_settings',
            'configure_distance_attenuation',
            'configure_spatialization',
            'configure_occlusion',
            'configure_reverb_send',
            // Dialogue System (3)
            'create_dialogue_voice',
            'create_dialogue_wave',
            'set_dialogue_context',
            // Effects (4)
            'create_reverb_effect',
            'create_source_effect_chain',
            'add_source_effect',
            'create_submix_effect',
            // Utility (1)
            'get_audio_info'
          ],
          description: 'Audio authoring action to perform.'
        },
        // Common parameters
        name: { type: 'string', description: 'Asset name for creation.' },
        path: { type: 'string', description: 'Directory path for asset creation.' },
        assetPath: { type: 'string', description: 'Path to existing audio asset.' },
        save: { type: 'boolean', description: 'Save asset after modification.' },

        // Sound Cue parameters
        wavePath: { type: 'string', description: 'Path to SoundWave asset.' },
        nodeType: {
          type: 'string',
          enum: [
            'WavePlayer', 'Mixer', 'Random', 'Modulator', 'Attenuation', 'Looping',
            'Concatenator', 'Delay', 'Doppler', 'Enveloper', 'Crossfade', 'Switch',
            'MatineeControlled', 'GroupControl', 'OscillatorSound', 'QualityLevel'
          ],
          description: 'Sound Cue node type to add.'
        },
        nodeId: { type: 'string', description: 'Node ID for reference.' },
        sourceNodeId: { type: 'string', description: 'Source node ID for connection.' },
        targetNodeId: { type: 'string', description: 'Target node ID for connection.' },
        outputPin: { type: 'number', description: 'Output pin index.' },
        inputPin: { type: 'number', description: 'Input pin index.' },
        attenuationPath: { type: 'string', description: 'Path to SoundAttenuation asset.' },
        concurrencyPath: { type: 'string', description: 'Path to SoundConcurrency asset.' },
        looping: { type: 'boolean', description: 'Enable looping.' },
        volume: { type: 'number', description: 'Volume multiplier (0.0 - 1.0).' },
        pitch: { type: 'number', description: 'Pitch multiplier.' },
        x: { type: 'number', description: 'Node X position in graph.' },
        y: { type: 'number', description: 'Node Y position in graph.' },

        // MetaSound parameters
        metasoundType: {
          type: 'string',
          enum: ['Source', 'Patch'],
          description: 'MetaSound type (Source for playable, Patch for reusable).'
        },
        inputName: { type: 'string', description: 'MetaSound input name.' },
        inputType: {
          type: 'string',
          enum: ['Audio', 'Float', 'Int', 'Bool', 'String', 'Trigger', 'Time'],
          description: 'MetaSound input data type.'
        },
        outputName: { type: 'string', description: 'MetaSound output name.' },
        outputType: {
          type: 'string',
          enum: ['Audio', 'Float', 'Int', 'Bool', 'String', 'Trigger'],
          description: 'MetaSound output data type.'
        },
        sourceNode: { type: 'string', description: 'Source node name for MetaSound connection.' },
        sourcePin: { type: 'string', description: 'Source pin name for MetaSound connection.' },
        targetNode: { type: 'string', description: 'Target node name for MetaSound connection.' },
        targetPin: { type: 'string', description: 'Target pin name for MetaSound connection.' },
        defaultValue: { description: 'Default value for input (type depends on inputType).' },
        metasoundNodeType: {
          type: 'string',
          description: 'MetaSound node type (e.g., Audio:Multiply, Generators:Sine, Filters:BiQuad).'
        },

        // Sound Class parameters
        soundClassPath: { type: 'string', description: 'Path to SoundClass asset.' },
        parentClassPath: { type: 'string', description: 'Parent SoundClass path.' },
        properties: {
          type: 'object',
          properties: {
            volume: { type: 'number' },
            pitch: { type: 'number' },
            lowPassFilterFrequency: { type: 'number' },
            attenuationDistanceScale: { type: 'number' },
            sendLevel: { type: 'number' },
            occlusionFilterFrequency: { type: 'number' },
            voiceCenterChannelVolume: { type: 'number' },
            radioFilterVolume: { type: 'number' },
            radioFilterVolumeThreshold: { type: 'number' },
            defaultVolume: { type: 'number' },
            default2DReverbSendAmount: { type: 'number' }
          },
          description: 'Sound class property values.'
        },

        // Sound Mix parameters
        classAdjusters: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              soundClass: { type: 'string' },
              volumeAdjuster: { type: 'number' },
              pitchAdjuster: { type: 'number' },
              applyToChildren: { type: 'boolean' },
              fadeInTime: { type: 'number' },
              fadeOutTime: { type: 'number' }
            }
          },
          description: 'Array of sound class adjusters for the mix.'
        },
        eq: {
          type: 'object',
          properties: {
            lowFrequency: { type: 'number' },
            lowFrequencyGain: { type: 'number' },
            midFrequency1: { type: 'number' },
            midFrequency1Gain: { type: 'number' },
            midFrequency2: { type: 'number' },
            midFrequency2Gain: { type: 'number' },
            midFrequency3: { type: 'number' },
            midFrequency3Gain: { type: 'number' },
            highFrequency: { type: 'number' },
            highFrequencyGain: { type: 'number' }
          },
          description: 'EQ settings for sound mix.'
        },

        // Attenuation parameters
        attenuationFunction: {
          type: 'string',
          enum: ['Linear', 'Logarithmic', 'Inverse', 'LogReverse', 'NaturalSound', 'Custom'],
          description: 'Distance attenuation function.'
        },
        innerRadius: { type: 'number', description: 'Inner radius (full volume).' },
        falloffDistance: { type: 'number', description: 'Distance over which attenuation occurs.' },
        dbAttenuationAtMax: { type: 'number', description: 'Attenuation in dB at max distance.' },
        attenuationShape: {
          type: 'string',
          enum: ['Sphere', 'Capsule', 'Box', 'Cone'],
          description: 'Shape of attenuation zone.'
        },
        spatializationAlgorithm: {
          type: 'string',
          enum: ['Panning', 'Binaural', 'Plugin'],
          description: 'Spatialization algorithm.'
        },
        enableOcclusion: { type: 'boolean', description: 'Enable occlusion.' },
        occlusionLowPassFilterFrequency: { type: 'number', description: 'Low pass filter frequency when occluded.' },
        occlusionVolumeAttenuation: { type: 'number', description: 'Volume attenuation when occluded.' },
        occlusionInterpolationTime: { type: 'number', description: 'Occlusion interpolation time.' },
        reverbSendMethod: {
          type: 'string',
          enum: ['Linear', 'CustomCurve', 'Manual'],
          description: 'Method for reverb send.'
        },
        reverbSendLevel: { type: 'number', description: 'Reverb send level (0.0 - 1.0).' },
        reverbWetLevel: { type: 'number', description: 'Reverb wet level.' },

        // Dialogue parameters
        dialogueVoice: {
          type: 'object',
          properties: {
            gender: { type: 'string', enum: ['Neutral', 'Male', 'Female'] },
            plurality: { type: 'string', enum: ['Singular', 'Plural'] }
          },
          description: 'Dialogue voice characteristics.'
        },
        speakerPath: { type: 'string', description: 'Path to DialogueVoice asset for speaker.' },
        targetPaths: {
          type: 'array',
          items: { type: 'string' },
          description: 'Array of DialogueVoice paths for targets.'
        },
        dialogueContext: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              speaker: { type: 'string' },
              targets: { type: 'array', items: { type: 'string' } },
              wave: { type: 'string' }
            }
          },
          description: 'Dialogue context mappings.'
        },

        // Reverb Effect parameters
        reverbSettings: {
          type: 'object',
          properties: {
            density: { type: 'number' },
            diffusion: { type: 'number' },
            gain: { type: 'number' },
            gainHF: { type: 'number' },
            decayTime: { type: 'number' },
            decayHFRatio: { type: 'number' },
            reflectionsGain: { type: 'number' },
            reflectionsDelay: { type: 'number' },
            lateReverbGain: { type: 'number' },
            lateReverbDelay: { type: 'number' },
            airAbsorptionGainHF: { type: 'number' },
            roomRolloffFactor: { type: 'number' }
          },
          description: 'Reverb effect settings.'
        },

        // Source Effect parameters
        effectType: {
          type: 'string',
          enum: [
            'Filter', 'EQ', 'Chorus', 'Dynamics', 'Envelope', 'Flanger', 'Folding',
            'LPF', 'HPF', 'Panner', 'Phaser', 'RingModulation', 'Stereo', 'Delay',
            'BitCrusher', 'Convolution', 'Distortion', 'Limiter', 'Parametric'
          ],
          description: 'Source effect type to add.'
        },
        effectPreset: { type: 'string', description: 'Path to effect preset asset.' },
        bypassWhenSilent: { type: 'boolean', description: 'Bypass effect when silent.' },

        // Submix Effect parameters
        submixEffectType: {
          type: 'string',
          enum: [
            'Reverb', 'EQ', 'Dynamics', 'SubmixDelay', 'SubmixFilter', 'StereoDelay',
            'Flanger', 'Phaser', 'Limiter', 'Distortion', 'Chorus', 'Convolution'
          ],
          description: 'Submix effect type.'
        }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        message: { type: 'string' },
        assetPath: { type: 'string', description: 'Path to created/modified audio asset.' },
        nodeId: { type: 'string', description: 'ID of created node.' },
        audioInfo: {
          type: 'object',
          properties: {
            assetType: { type: 'string' },
            duration: { type: 'number' },
            sampleRate: { type: 'number' },
            numChannels: { type: 'number' },
            nodeCount: { type: 'number' },
            inputCount: { type: 'number' },
            outputCount: { type: 'number' },
            soundClass: { type: 'string' },
            attenuation: { type: 'string' },
            concurrency: { type: 'string' }
          },
          description: 'Audio asset information (for get_audio_info).'
        },
        error: { type: 'string' }
      }
    }
  },

  // ============================================================================
  // 23. Niagara Authoring (Phase 12)
  // ============================================================================
  {
    name: 'manage_niagara_authoring',
    description: 'Create and edit Niagara VFX systems, emitters, modules, spawn/force/render settings, and GPU particle simulation.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            // Systems & Emitters (4)
            'create_niagara_system',
            'create_niagara_emitter',
            'add_emitter_to_system',
            'set_emitter_properties',
            // Spawn Modules (3)
            'add_spawn_rate_module',
            'add_spawn_burst_module',
            'add_spawn_per_unit_module',
            // Particle Modules (2)
            'add_initialize_particle_module',
            'add_particle_state_module',
            // Force Module (1)
            'add_force_module',
            // Motion Modules (2)
            'add_velocity_module',
            'add_acceleration_module',
            // Appearance Modules (2)
            'add_size_module',
            'add_color_module',
            // Renderer Modules (4)
            'add_sprite_renderer_module',
            'add_mesh_renderer_module',
            'add_ribbon_renderer_module',
            'add_light_renderer_module',
            // Behavior Modules (3)
            'add_collision_module',
            'add_kill_particles_module',
            'add_camera_offset_module',
            // Parameters (3)
            'add_user_parameter',
            'set_parameter_value',
            'bind_parameter_to_source',
            // Data Interfaces (5)
            'add_skeletal_mesh_data_interface',
            'add_static_mesh_data_interface',
            'add_spline_data_interface',
            'add_audio_spectrum_data_interface',
            'add_collision_query_data_interface',
            // Events (3)
            'add_event_generator',
            'add_event_receiver',
            'configure_event_payload',
            // GPU (2)
            'enable_gpu_simulation',
            'add_simulation_stage',
            // Utility (2)
            'get_niagara_info',
            'validate_niagara_system'
          ],
          description: 'Niagara authoring action to perform.'
        },
        // Common parameters
        name: { type: 'string', description: 'Asset name for creation.' },
        path: { type: 'string', description: 'Directory path for asset creation.' },
        assetPath: { type: 'string', description: 'Path to existing Niagara asset.' },
        systemPath: { type: 'string', description: 'Path to Niagara System asset.' },
        emitterPath: { type: 'string', description: 'Path to Niagara Emitter asset.' },
        emitterName: { type: 'string', description: 'Name of emitter within the system.' },
        save: { type: 'boolean', description: 'Save asset after modification.' },

        // Emitter properties
        emitterProperties: {
          type: 'object',
          properties: {
            enabled: { type: 'boolean', description: 'Enable/disable emitter.' },
            localSpace: { type: 'boolean', description: 'Simulate in local space.' },
            deterministic: { type: 'boolean', description: 'Use deterministic random.' },
            randomSeed: { type: 'number', description: 'Random seed for deterministic simulation.' },
            simulationTarget: { type: 'string', enum: ['CPUSim', 'GPUComputeSim'], description: 'Simulation target.' },
            scalabilityMode: { type: 'string', enum: ['Self', 'System'], description: 'Scalability mode.' },
            fixedBounds: { type: 'boolean', description: 'Use fixed bounds.' },
            bounds: {
              type: 'object',
              properties: {
                min: { type: 'object', properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } } },
                max: { type: 'object', properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } } }
              }
            }
          },
          description: 'Emitter property values.'
        },

        // Spawn parameters
        spawnRate: { type: 'number', description: 'Particles per second for spawn rate module.' },
        burstCount: { type: 'number', description: 'Number of particles for burst spawn.' },
        burstTime: { type: 'number', description: 'Time at which burst occurs.' },
        burstInterval: { type: 'number', description: 'Interval between bursts (0 for single burst).' },
        spawnPerUnit: { type: 'number', description: 'Particles spawned per unit distance.' },

        // Initialize particle parameters
        lifetime: { type: 'number', description: 'Particle lifetime in seconds.' },
        lifetimeMin: { type: 'number', description: 'Minimum lifetime for random range.' },
        lifetimeMax: { type: 'number', description: 'Maximum lifetime for random range.' },
        mass: { type: 'number', description: 'Particle mass.' },
        spriteSize: {
          type: 'object',
          properties: { x: { type: 'number' }, y: { type: 'number' } },
          description: 'Sprite size {x, y}.'
        },
        spriteSizeMin: { type: 'object', description: 'Minimum sprite size for random range.' },
        spriteSizeMax: { type: 'object', description: 'Maximum sprite size for random range.' },
        meshScale: {
          type: 'object',
          properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } },
          description: 'Mesh scale {x, y, z}.'
        },

        // Force module parameters
        forceType: {
          type: 'string',
          enum: ['Gravity', 'Drag', 'Vortex', 'PointAttraction', 'CurlNoise', 'Wind', 'LinearForce'],
          description: 'Type of force to apply.'
        },
        forceStrength: { type: 'number', description: 'Force strength/magnitude.' },
        forceVector: {
          type: 'object',
          properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } },
          description: 'Force direction vector.'
        },
        dragCoefficient: { type: 'number', description: 'Drag coefficient.' },
        vortexAxis: {
          type: 'object',
          properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } },
          description: 'Vortex rotation axis.'
        },
        attractorPosition: {
          type: 'object',
          properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } },
          description: 'Position of point attractor.'
        },
        attractorRadius: { type: 'number', description: 'Attractor influence radius.' },
        killRadius: { type: 'number', description: 'Radius at which particles are killed.' },
        noiseFrequency: { type: 'number', description: 'Curl noise frequency.' },
        noiseStrength: { type: 'number', description: 'Curl noise strength.' },

        // Velocity/Acceleration parameters
        velocity: {
          type: 'object',
          properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } },
          description: 'Initial velocity vector.'
        },
        velocityMin: { type: 'object', description: 'Minimum velocity for random range.' },
        velocityMax: { type: 'object', description: 'Maximum velocity for random range.' },
        acceleration: {
          type: 'object',
          properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } },
          description: 'Acceleration vector.'
        },
        velocityMode: {
          type: 'string',
          enum: ['Linear', 'FromPoint', 'InCone'],
          description: 'Velocity initialization mode.'
        },
        coneAngle: { type: 'number', description: 'Cone angle for InCone velocity mode.' },

        // Size module parameters
        sizeMode: {
          type: 'string',
          enum: ['Uniform', 'NonUniform', 'ByLifetime'],
          description: 'Size calculation mode.'
        },
        uniformSize: { type: 'number', description: 'Uniform size value.' },
        sizeScale: {
          type: 'object',
          properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } },
          description: 'Size scale multiplier.'
        },
        sizeCurve: {
          type: 'array',
          items: {
            type: 'object',
            properties: { time: { type: 'number' }, value: { type: 'number' } }
          },
          description: 'Size over lifetime curve points.'
        },

        // Color module parameters
        color: {
          type: 'object',
          properties: { r: { type: 'number' }, g: { type: 'number' }, b: { type: 'number' }, a: { type: 'number' } },
          description: 'Particle color {r, g, b, a} (0-1 range).'
        },
        colorMin: { type: 'object', description: 'Minimum color for random range.' },
        colorMax: { type: 'object', description: 'Maximum color for random range.' },
        colorMode: {
          type: 'string',
          enum: ['Direct', 'ByLifetime', 'BySpeed', 'ByAttribute'],
          description: 'Color calculation mode.'
        },
        colorCurve: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              time: { type: 'number' },
              color: { type: 'object', properties: { r: { type: 'number' }, g: { type: 'number' }, b: { type: 'number' }, a: { type: 'number' } } }
            }
          },
          description: 'Color over lifetime curve points.'
        },

        // Renderer parameters
        materialPath: { type: 'string', description: 'Path to material for renderer.' },
        meshPath: { type: 'string', description: 'Path to static mesh for mesh renderer.' },
        sortMode: {
          type: 'string',
          enum: ['None', 'ViewDepth', 'ViewDistance', 'CustomAscending', 'CustomDescending'],
          description: 'Particle sort mode.'
        },
        subImageIndex: { type: 'number', description: 'Sub-image index for flipbook materials.' },
        alignment: {
          type: 'string',
          enum: ['Unaligned', 'VelocityAligned', 'CustomAlignment'],
          description: 'Sprite alignment mode.'
        },
        facingMode: {
          type: 'string',
          enum: ['FaceCamera', 'FaceCameraPlane', 'CustomFacing', 'FaceCameraPosition', 'FaceCameraDistanceBlend'],
          description: 'Sprite facing mode.'
        },

        // Ribbon parameters
        ribbonWidth: { type: 'number', description: 'Ribbon width.' },
        ribbonTwist: { type: 'number', description: 'Ribbon twist amount.' },
        ribbonFacingMode: {
          type: 'string',
          enum: ['Screen', 'Custom', 'CustomSideVector'],
          description: 'Ribbon facing mode.'
        },
        tessellationFactor: { type: 'number', description: 'Ribbon tessellation factor.' },

        // Light renderer parameters
        lightRadius: { type: 'number', description: 'Light radius.' },
        lightIntensity: { type: 'number', description: 'Light intensity.' },
        lightColor: {
          type: 'object',
          properties: { r: { type: 'number' }, g: { type: 'number' }, b: { type: 'number' } },
          description: 'Light color.'
        },
        volumetricScattering: { type: 'number', description: 'Volumetric scattering intensity.' },
        lightExponent: { type: 'number', description: 'Light falloff exponent.' },
        affectsTranslucency: { type: 'boolean', description: 'Light affects translucent materials.' },

        // Collision parameters
        collisionMode: {
          type: 'string',
          enum: ['None', 'SceneDepth', 'DistanceField', 'AnalyticPlane'],
          description: 'Collision detection mode.'
        },
        restitution: { type: 'number', description: 'Collision restitution (bounciness).' },
        friction: { type: 'number', description: 'Collision friction.' },
        radiusScale: { type: 'number', description: 'Collision radius scale.' },
        dieOnCollision: { type: 'boolean', description: 'Kill particle on collision.' },

        // Kill particles parameters
        killCondition: {
          type: 'string',
          enum: ['Age', 'Box', 'Sphere', 'Plane', 'Custom'],
          description: 'Kill condition type.'
        },
        killBox: {
          type: 'object',
          properties: {
            min: { type: 'object', properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } } },
            max: { type: 'object', properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } } }
          },
          description: 'Kill volume box bounds.'
        },
        invertKillZone: { type: 'boolean', description: 'Kill particles outside the zone instead of inside.' },

        // Camera offset parameters
        cameraOffset: { type: 'number', description: 'Camera offset distance.' },
        cameraOffsetMode: {
          type: 'string',
          enum: ['Absolute', 'Relative'],
          description: 'Camera offset mode.'
        },

        // Parameter settings
        parameterName: { type: 'string', description: 'Name of user parameter.' },
        parameterType: {
          type: 'string',
          enum: ['Float', 'Int', 'Bool', 'Vector', 'LinearColor', 'Texture', 'StaticMesh', 'SkeletalMesh', 'Object'],
          description: 'Parameter data type.'
        },
        parameterValue: { description: 'Parameter value (type depends on parameterType).' },
        sourceBinding: { type: 'string', description: 'Source binding path for parameter binding.' },

        // Data interface parameters
        skeletalMeshPath: { type: 'string', description: 'Path to skeletal mesh asset.' },
        staticMeshPath: { type: 'string', description: 'Path to static mesh asset.' },
        useWholeSkeletonOrBones: {
          type: 'string',
          enum: ['WholeSkeleton', 'SpecificBones'],
          description: 'Skeletal mesh sampling mode.'
        },
        specificBones: {
          type: 'array',
          items: { type: 'string' },
          description: 'List of specific bone names to sample.'
        },
        samplingMode: {
          type: 'string',
          enum: ['Vertices', 'Triangles', 'Bones', 'Sockets'],
          description: 'Mesh sampling mode.'
        },

        // Event parameters
        eventName: { type: 'string', description: 'Event name for generator/receiver.' },
        eventPayload: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              name: { type: 'string' },
              type: { type: 'string', enum: ['Float', 'Int', 'Bool', 'Vector', 'LinearColor'] }
            }
          },
          description: 'Event payload attribute definitions.'
        },
        spawnOnEvent: { type: 'boolean', description: 'Spawn particles when event received.' },
        eventSpawnCount: { type: 'number', description: 'Number of particles to spawn per event.' },

        // GPU simulation parameters
        gpuEnabled: { type: 'boolean', description: 'Enable GPU simulation.' },
        fixedBoundsEnabled: { type: 'boolean', description: 'Use fixed bounds for GPU sim.' },
        deterministicEnabled: { type: 'boolean', description: 'Enable deterministic simulation.' },
        stageName: { type: 'string', description: 'Simulation stage name.' },
        stageIterationSource: {
          type: 'string',
          enum: ['Particles', 'DataInterface', 'None'],
          description: 'Simulation stage iteration source.'
        }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        message: { type: 'string' },
        assetPath: { type: 'string', description: 'Path to created/modified Niagara asset.' },
        emitterName: { type: 'string', description: 'Name of added/modified emitter.' },
        moduleName: { type: 'string', description: 'Name of added module.' },
        parameterName: { type: 'string', description: 'Name of added/modified parameter.' },
        niagaraInfo: {
          type: 'object',
          properties: {
            assetType: { type: 'string', enum: ['System', 'Emitter'] },
            emitterCount: { type: 'number' },
            emitters: {
              type: 'array',
              items: {
                type: 'object',
                properties: {
                  name: { type: 'string' },
                  enabled: { type: 'boolean' },
                  simulationTarget: { type: 'string' },
                  moduleCount: { type: 'number' }
                }
              }
            },
            userParameterCount: { type: 'number' },
            userParameters: {
              type: 'array',
              items: {
                type: 'object',
                properties: { name: { type: 'string' }, type: { type: 'string' } }
              }
            },
            hasGPUEmitters: { type: 'boolean' },
            isValid: { type: 'boolean' },
            validationErrors: { type: 'array', items: { type: 'string' } }
          },
          description: 'Niagara asset information (for get_niagara_info).'
        },
        validationResult: {
          type: 'object',
          properties: {
            isValid: { type: 'boolean' },
            errors: { type: 'array', items: { type: 'string' } },
            warnings: { type: 'array', items: { type: 'string' } }
          },
          description: 'Validation result (for validate_niagara_system).'
        },
        error: { type: 'string' }
      }
    }
  },

  // ============================================================================
  // 24. Gameplay Ability System (Phase 13)
  // ============================================================================
  {
    name: 'manage_gas',
    description: 'Configure Gameplay Ability System: create abilities, effects, attribute sets, gameplay cues, costs, and cooldowns.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            // Components & Attributes (6)
            'add_ability_system_component',
            'configure_asc',
            'create_attribute_set',
            'add_attribute',
            'set_attribute_base_value',
            'set_attribute_clamping',
            // Gameplay Abilities (8)
            'create_gameplay_ability',
            'set_ability_tags',
            'set_ability_costs',
            'set_ability_cooldown',
            'set_ability_targeting',
            'add_ability_task',
            'set_activation_policy',
            'set_instancing_policy',
            // Gameplay Effects (8)
            'create_gameplay_effect',
            'set_effect_duration',
            'add_effect_modifier',
            'set_modifier_magnitude',
            'add_effect_execution_calculation',
            'add_effect_cue',
            'set_effect_stacking',
            'set_effect_tags',
            // Gameplay Cues (4)
            'create_gameplay_cue_notify',
            'configure_cue_trigger',
            'set_cue_effects',
            'add_tag_to_asset',
            // Utility (1)
            'get_gas_info'
          ],
          description: 'GAS action to perform.'
        },
        // Common parameters
        name: { type: 'string', description: 'Asset name for creation.' },
        path: { type: 'string', description: 'Directory path for asset creation.' },
        assetPath: { type: 'string', description: 'Path to existing asset.' },
        blueprintPath: { type: 'string', description: 'Path to Blueprint asset.' },
        save: { type: 'boolean', description: 'Save asset after modification.' },

        // Ability System Component parameters
        replicationMode: {
          type: 'string',
          enum: ['Full', 'Minimal', 'Mixed'],
          description: 'ASC replication mode.'
        },
        ownerActor: { type: 'string', description: 'Owner actor class for ASC.' },
        avatarActor: { type: 'string', description: 'Avatar actor class for ASC.' },

        // Attribute Set parameters
        attributeSetPath: { type: 'string', description: 'Path to Attribute Set asset.' },
        attributeName: { type: 'string', description: 'Name of the attribute.' },
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

        // Gameplay Ability parameters
        abilityPath: { type: 'string', description: 'Path to Gameplay Ability asset.' },
        parentClass: { type: 'string', description: 'Parent class for ability.' },
        abilityTags: {
          type: 'array',
          items: { type: 'string' },
          description: 'Gameplay tags for this ability.'
        },
        cancelAbilitiesWithTag: {
          type: 'array',
          items: { type: 'string' },
          description: 'Tags of abilities to cancel when this activates.'
        },
        blockAbilitiesWithTag: {
          type: 'array',
          items: { type: 'string' },
          description: 'Tags of abilities blocked while this is active.'
        },
        activationRequiredTags: {
          type: 'array',
          items: { type: 'string' },
          description: 'Tags required to activate this ability.'
        },
        activationBlockedTags: {
          type: 'array',
          items: { type: 'string' },
          description: 'Tags that block activation of this ability.'
        },

        // Ability Costs
        costEffectPath: { type: 'string', description: 'Path to cost Gameplay Effect.' },
        costAttribute: { type: 'string', description: 'Attribute used for cost (e.g., Mana).' },
        costMagnitude: { type: 'number', description: 'Cost magnitude.' },

        // Ability Cooldown
        cooldownEffectPath: { type: 'string', description: 'Path to cooldown Gameplay Effect.' },
        cooldownDuration: { type: 'number', description: 'Cooldown duration in seconds.' },
        cooldownTags: {
          type: 'array',
          items: { type: 'string' },
          description: 'Tags applied during cooldown.'
        },

        // Ability Targeting
        targetingMode: {
          type: 'string',
          enum: ['None', 'SingleTarget', 'AOE', 'Directional', 'Ground', 'ActorPlacement'],
          description: 'Targeting mode for ability.'
        },
        targetRange: { type: 'number', description: 'Maximum targeting range.' },
        aoeRadius: { type: 'number', description: 'Area of effect radius.' },

        // Ability Tasks
        taskType: {
          type: 'string',
          enum: ['WaitDelay', 'WaitInputPress', 'WaitInputRelease', 'WaitGameplayEvent', 'WaitTargetData', 'WaitConfirmCancel', 'PlayMontageAndWait', 'ApplyRootMotionConstantForce', 'WaitMovementModeChange'],
          description: 'Type of ability task to add.'
        },
        taskSettings: {
          type: 'object',
          description: 'Task-specific settings.'
        },

        // Activation/Instancing Policies
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

        // Gameplay Effect parameters
        effectPath: { type: 'string', description: 'Path to Gameplay Effect asset.' },
        durationType: {
          type: 'string',
          enum: ['Instant', 'Infinite', 'HasDuration'],
          description: 'Effect duration type.'
        },
        duration: { type: 'number', description: 'Duration in seconds (for HasDuration).' },
        period: { type: 'number', description: 'Period for periodic effects.' },

        // Effect Modifiers
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

        // Execution Calculation
        calculationClass: { type: 'string', description: 'UGameplayEffectExecutionCalculation class path.' },

        // Effect Cues
        cueTag: { type: 'string', description: 'Gameplay Cue tag (e.g., GameplayCue.Damage.Fire).' },
        cuePath: { type: 'string', description: 'Path to Gameplay Cue asset.' },

        // Effect Stacking
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

        // Effect Tags
        grantedTags: {
          type: 'array',
          items: { type: 'string' },
          description: 'Tags granted while effect is active.'
        },
        applicationRequiredTags: {
          type: 'array',
          items: { type: 'string' },
          description: 'Tags required to apply this effect.'
        },
        removalTags: {
          type: 'array',
          items: { type: 'string' },
          description: 'Tags that cause effect removal.'
        },
        immunityTags: {
          type: 'array',
          items: { type: 'string' },
          description: 'Tags that block this effect.'
        },

        // Gameplay Cue parameters
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

        // Cue Effects
        particleSystemPath: { type: 'string', description: 'Path to Niagara/Cascade particle system.' },
        soundPath: { type: 'string', description: 'Path to sound cue/wave.' },
        cameraShakePath: { type: 'string', description: 'Path to camera shake asset.' },
        decalPath: { type: 'string', description: 'Path to decal material.' },

        // Tag parameters
        tagName: { type: 'string', description: 'Gameplay tag name to add.' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        message: { type: 'string' },
        assetPath: { type: 'string', description: 'Path to created/modified asset.' },
        componentName: { type: 'string', description: 'Name of added component.' },
        attributeName: { type: 'string', description: 'Name of added/modified attribute.' },
        modifierIndex: { type: 'number', description: 'Index of added modifier.' },
        gasInfo: {
          type: 'object',
          properties: {
            assetType: { type: 'string', enum: ['AttributeSet', 'GameplayAbility', 'GameplayEffect', 'GameplayCue'] },
            attributes: {
              type: 'array',
              items: {
                type: 'object',
                properties: {
                  name: { type: 'string' },
                  baseValue: { type: 'number' },
                  currentValue: { type: 'number' }
                }
              }
            },
            abilityTags: { type: 'array', items: { type: 'string' } },
            effectDuration: { type: 'string' },
            modifierCount: { type: 'number' },
            cueType: { type: 'string' }
          },
          description: 'GAS asset information (for get_gas_info).'
        },
        error: { type: 'string' }
      }
    }
  },

  // ============================================================================
  // 25. Character & Movement System (Phase 14)
  // ============================================================================
  {
    name: 'manage_character',
    description: 'Create character Blueprints with capsule, mesh, camera. Configure movement speeds, jumping, advanced locomotion (mantle, climb, slide).',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            // Character Creation (4)
            'create_character_blueprint',
            'configure_capsule_component',
            'configure_mesh_component',
            'configure_camera_component',
            // Movement Component (5)
            'configure_movement_speeds',
            'configure_jump',
            'configure_rotation',
            'add_custom_movement_mode',
            'configure_nav_movement',
            // Advanced Movement (6)
            'setup_mantling',
            'setup_vaulting',
            'setup_climbing',
            'setup_sliding',
            'setup_wall_running',
            'setup_grappling',
            // Footsteps System (3)
            'setup_footstep_system',
            'map_surface_to_sound',
            'configure_footstep_fx',
            // Utility (1)
            'get_character_info'
          ],
          description: 'Character action to perform.'
        },
        // Common parameters
        name: { type: 'string', description: 'Asset name for creation.' },
        path: { type: 'string', description: 'Directory path for asset creation.' },
        blueprintPath: { type: 'string', description: 'Path to Character Blueprint.' },
        save: { type: 'boolean', description: 'Save asset after modification.' },

        // Character Creation parameters
        parentClass: {
          type: 'string',
          enum: ['Character', 'ACharacter', 'PlayerCharacter', 'AICharacter'],
          description: 'Parent class for character blueprint.'
        },
        skeletalMeshPath: { type: 'string', description: 'Path to skeletal mesh for character.' },
        animBlueprintPath: { type: 'string', description: 'Path to animation blueprint.' },

        // Capsule Component
        capsuleRadius: { type: 'number', description: 'Capsule collision radius.' },
        capsuleHalfHeight: { type: 'number', description: 'Capsule collision half height.' },

        // Mesh Component
        meshOffset: {
          type: 'object',
          properties: {
            x: { type: 'number' },
            y: { type: 'number' },
            z: { type: 'number' }
          },
          description: 'Mesh location offset.'
        },
        meshRotation: {
          type: 'object',
          properties: {
            pitch: { type: 'number' },
            yaw: { type: 'number' },
            roll: { type: 'number' }
          },
          description: 'Mesh rotation offset.'
        },

        // Camera Component
        cameraSocketName: { type: 'string', description: 'Socket to attach camera to.' },
        cameraOffset: {
          type: 'object',
          properties: {
            x: { type: 'number' },
            y: { type: 'number' },
            z: { type: 'number' }
          },
          description: 'Camera location offset.'
        },
        cameraUsePawnControlRotation: { type: 'boolean', description: 'Camera follows controller rotation.' },
        springArmLength: { type: 'number', description: 'Spring arm length for third-person.' },
        springArmLagEnabled: { type: 'boolean', description: 'Enable camera lag.' },
        springArmLagSpeed: { type: 'number', description: 'Camera lag speed.' },

        // Movement Speeds
        walkSpeed: { type: 'number', description: 'Walking speed.' },
        runSpeed: { type: 'number', description: 'Running/jogging speed.' },
        sprintSpeed: { type: 'number', description: 'Sprinting speed.' },
        crouchSpeed: { type: 'number', description: 'Crouching speed.' },
        swimSpeed: { type: 'number', description: 'Swimming speed.' },
        flySpeed: { type: 'number', description: 'Flying speed.' },
        acceleration: { type: 'number', description: 'Ground acceleration.' },
        deceleration: { type: 'number', description: 'Braking deceleration.' },
        groundFriction: { type: 'number', description: 'Ground friction coefficient.' },

        // Jump Configuration
        jumpHeight: { type: 'number', description: 'Jump Z velocity.' },
        airControl: { type: 'number', description: 'Air control amount (0-1).' },
        doubleJumpEnabled: { type: 'boolean', description: 'Enable double jump.' },
        maxJumpCount: { type: 'number', description: 'Maximum jumps allowed.' },
        jumpHoldTime: { type: 'number', description: 'Max hold time for variable jump.' },
        gravityScale: { type: 'number', description: 'Gravity multiplier.' },
        fallingLateralFriction: { type: 'number', description: 'Air friction.' },

        // Rotation Configuration
        orientToMovement: { type: 'boolean', description: 'Orient rotation to movement direction.' },
        useControllerRotationYaw: { type: 'boolean', description: 'Use controller yaw rotation.' },
        useControllerRotationPitch: { type: 'boolean', description: 'Use controller pitch rotation.' },
        useControllerRotationRoll: { type: 'boolean', description: 'Use controller roll rotation.' },
        rotationRate: { type: 'number', description: 'Character rotation rate (degrees/sec).' },

        // Custom Movement Mode
        modeName: { type: 'string', description: 'Name for custom movement mode.' },
        modeId: { type: 'number', description: 'Custom movement mode ID.' },

        // Nav Movement
        navAgentRadius: { type: 'number', description: 'Navigation agent radius.' },
        navAgentHeight: { type: 'number', description: 'Navigation agent height.' },
        avoidanceEnabled: { type: 'boolean', description: 'Enable AI avoidance.' },
        pathFollowingEnabled: { type: 'boolean', description: 'Enable path following.' },

        // Advanced Movement - Mantling
        mantleHeight: { type: 'number', description: 'Maximum mantle height.' },
        mantleReachDistance: { type: 'number', description: 'Forward reach for mantle check.' },
        mantleAnimationPath: { type: 'string', description: 'Path to mantle animation montage.' },

        // Advanced Movement - Vaulting
        vaultHeight: { type: 'number', description: 'Maximum vault obstacle height.' },
        vaultDepth: { type: 'number', description: 'Obstacle depth to check.' },
        vaultAnimationPath: { type: 'string', description: 'Path to vault animation montage.' },

        // Advanced Movement - Climbing
        climbSpeed: { type: 'number', description: 'Climbing movement speed.' },
        climbableTag: { type: 'string', description: 'Tag for climbable surfaces.' },
        climbAnimationPath: { type: 'string', description: 'Path to climb animation.' },

        // Advanced Movement - Sliding
        slideSpeed: { type: 'number', description: 'Sliding speed.' },
        slideDuration: { type: 'number', description: 'Slide duration.' },
        slideCooldown: { type: 'number', description: 'Cooldown between slides.' },
        slideAnimationPath: { type: 'string', description: 'Path to slide animation.' },

        // Advanced Movement - Wall Running
        wallRunSpeed: { type: 'number', description: 'Wall running speed.' },
        wallRunDuration: { type: 'number', description: 'Maximum wall run duration.' },
        wallRunGravityScale: { type: 'number', description: 'Gravity during wall run.' },
        wallRunAnimationPath: { type: 'string', description: 'Path to wall run animation.' },

        // Advanced Movement - Grappling
        grappleRange: { type: 'number', description: 'Maximum grapple distance.' },
        grappleSpeed: { type: 'number', description: 'Grapple pull speed.' },
        grappleTargetTag: { type: 'string', description: 'Tag for grapple targets.' },
        grappleCablePath: { type: 'string', description: 'Path to cable mesh/material.' },

        // Footstep System
        footstepEnabled: { type: 'boolean', description: 'Enable footstep system.' },
        footstepSocketLeft: { type: 'string', description: 'Left foot socket name.' },
        footstepSocketRight: { type: 'string', description: 'Right foot socket name.' },
        footstepTraceDistance: { type: 'number', description: 'Ground trace distance.' },

        // Surface Mapping
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
        success: { type: 'boolean' },
        message: { type: 'string' },
        blueprintPath: { type: 'string', description: 'Path to created/modified blueprint.' },
        componentName: { type: 'string', description: 'Name of configured component.' },
        modeName: { type: 'string', description: 'Name of added movement mode.' },
        characterInfo: {
          type: 'object',
          properties: {
            capsuleRadius: { type: 'number' },
            capsuleHalfHeight: { type: 'number' },
            walkSpeed: { type: 'number' },
            jumpZVelocity: { type: 'number' },
            airControl: { type: 'number' },
            orientToMovement: { type: 'boolean' },
            hasSpringArm: { type: 'boolean' },
            hasCamera: { type: 'boolean' },
            customMovementModes: {
              type: 'array',
              items: { type: 'string' }
            }
          },
          description: 'Character configuration info (for get_character_info).'
        },
        error: { type: 'string' }
      }
    }
  },

  // 26. COMBAT & WEAPONS SYSTEM (Phase 15)
  {
    name: 'manage_combat',
    description: 'Create weapons with hitscan/projectile firing, configure damage types, hitboxes, reload, and melee combat (combos, parry, block).',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            // Weapon Base
            'create_weapon_blueprint', 'configure_weapon_mesh', 'configure_weapon_sockets', 'set_weapon_stats',
            // Firing Modes
            'configure_hitscan', 'configure_projectile', 'configure_spread_pattern', 'configure_recoil_pattern', 'configure_aim_down_sights',
            // Projectiles
            'create_projectile_blueprint', 'configure_projectile_movement', 'configure_projectile_collision', 'configure_projectile_homing',
            // Damage System
            'create_damage_type', 'configure_damage_execution', 'setup_hitbox_component',
            // Weapon Features
            'setup_reload_system', 'setup_ammo_system', 'setup_attachment_system', 'setup_weapon_switching',
            // Effects
            'configure_muzzle_flash', 'configure_tracer', 'configure_impact_effects', 'configure_shell_ejection',
            // Melee Combat
            'create_melee_trace', 'configure_combo_system', 'create_hit_pause', 'configure_hit_reaction', 'setup_parry_block_system', 'configure_weapon_trails',
            // Utility
            'get_combat_info'
          ],
          description: 'Combat action to perform'
        },
        // Common parameters
        blueprintPath: { type: 'string', description: 'Path to weapon/projectile blueprint.' },
        name: { type: 'string', description: 'Name for new weapon/projectile/damage type.' },
        path: { type: 'string', description: 'Directory path for asset creation.' },

        // Weapon Base
        weaponMeshPath: { type: 'string', description: 'Path to weapon static/skeletal mesh.' },
        muzzleSocketName: { type: 'string', description: 'Socket name for muzzle (default: Muzzle).' },
        ejectionSocketName: { type: 'string', description: 'Socket name for shell ejection.' },
        attachmentSocketNames: {
          type: 'array',
          items: { type: 'string' },
          description: 'List of attachment socket names.'
        },
        baseDamage: { type: 'number', description: 'Base damage per hit.' },
        fireRate: { type: 'number', description: 'Rounds per minute.' },
        range: { type: 'number', description: 'Maximum effective range.' },
        spread: { type: 'number', description: 'Bullet spread in degrees.' },

        // Firing Modes
        hitscanEnabled: { type: 'boolean', description: 'Enable hitscan firing.' },
        traceChannel: {
          type: 'string',
          enum: ['Visibility', 'Camera', 'Weapon', 'Custom'],
          description: 'Trace channel for hitscan.'
        },
        projectileClass: { type: 'string', description: 'Projectile class to spawn.' },
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

        // Projectile
        projectileSpeed: { type: 'number', description: 'Initial projectile speed.' },
        projectileGravityScale: { type: 'number', description: 'Gravity scale (0 = no gravity).' },
        projectileLifespan: { type: 'number', description: 'Projectile lifetime in seconds.' },
        projectileMeshPath: { type: 'string', description: 'Path to projectile mesh.' },
        collisionRadius: { type: 'number', description: 'Projectile collision sphere radius.' },
        bounceEnabled: { type: 'boolean', description: 'Enable projectile bouncing.' },
        bounceVelocityRatio: { type: 'number', description: 'Velocity retained on bounce (0-1).' },
        homingEnabled: { type: 'boolean', description: 'Enable homing behavior.' },
        homingAcceleration: { type: 'number', description: 'Homing turn rate.' },
        homingTargetTag: { type: 'string', description: 'Tag for homing targets.' },

        // Damage System
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
            radius: { type: 'number' },
            halfHeight: { type: 'number' },
            extent: { type: 'object', properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } } }
          },
          description: 'Hitbox dimensions.'
        },
        isDamageZoneHead: { type: 'boolean', description: 'Mark as headshot zone.' },
        damageMultiplier: { type: 'number', description: 'Damage multiplier for this hitbox.' },

        // Weapon Features
        magazineSize: { type: 'number', description: 'Magazine capacity.' },
        reloadTime: { type: 'number', description: 'Reload duration in seconds.' },
        reloadAnimationPath: { type: 'string', description: 'Path to reload animation.' },
        ammoType: { type: 'string', description: 'Ammo type identifier.' },
        maxAmmo: { type: 'number', description: 'Maximum reserve ammo.' },
        startingAmmo: { type: 'number', description: 'Starting reserve ammo.' },
        attachmentSlots: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              slotName: { type: 'string' },
              socketName: { type: 'string' },
              allowedTypes: { type: 'array', items: { type: 'string' } }
            }
          },
          description: 'Attachment slot definitions.'
        },
        switchInTime: { type: 'number', description: 'Time to equip weapon.' },
        switchOutTime: { type: 'number', description: 'Time to unequip weapon.' },
        switchInAnimationPath: { type: 'string', description: 'Path to equip animation.' },
        switchOutAnimationPath: { type: 'string', description: 'Path to unequip animation.' },

        // Effects
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

        // Melee Combat
        meleeTraceStartSocket: { type: 'string', description: 'Socket for trace start.' },
        meleeTraceEndSocket: { type: 'string', description: 'Socket for trace end.' },
        meleeTraceRadius: { type: 'number', description: 'Sphere trace radius.' },
        meleeTraceChannel: { type: 'string', description: 'Trace channel for melee.' },
        comboWindowTime: { type: 'number', description: 'Time window for combo input.' },
        maxComboCount: { type: 'number', description: 'Maximum combo length.' },
        comboAnimations: {
          type: 'array',
          items: { type: 'string' },
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
        success: { type: 'boolean' },
        message: { type: 'string' },
        blueprintPath: { type: 'string', description: 'Path to created/modified blueprint.' },
        damageTypePath: { type: 'string', description: 'Path to created damage type.' },
        combatInfo: {
          type: 'object',
          properties: {
            weaponType: { type: 'string' },
            firingMode: { type: 'string' },
            baseDamage: { type: 'number' },
            fireRate: { type: 'number' },
            magazineSize: { type: 'number' },
            hasADS: { type: 'boolean' },
            hasReload: { type: 'boolean' },
            isMelee: { type: 'boolean' },
            comboCount: { type: 'number' },
            attachmentSlots: { type: 'array', items: { type: 'string' } }
          },
          description: 'Combat configuration info (for get_combat_info).'
        },
        error: { type: 'string' }
      }
    }
  },

  // 27. AI SYSTEM (Phase 16)
  {
    name: 'manage_ai',
    description: 'Create AI controllers, blackboards, behavior trees. Configure EQS queries, perception (sight/hearing), State Trees, and Smart Objects.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            // AI Controller
            'create_ai_controller', 'assign_behavior_tree', 'assign_blackboard',
            // Blackboard
            'create_blackboard_asset', 'add_blackboard_key', 'set_key_instance_synced',
            // Behavior Tree
            'create_behavior_tree', 'add_composite_node', 'add_task_node', 'add_decorator', 'add_service', 'configure_bt_node',
            // EQS
            'create_eqs_query', 'add_eqs_generator', 'add_eqs_context', 'add_eqs_test', 'configure_test_scoring',
            // Perception
            'add_ai_perception_component', 'configure_sight_config', 'configure_hearing_config', 'configure_damage_sense_config', 'set_perception_team',
            // State Trees
            'create_state_tree', 'add_state_tree_state', 'add_state_tree_transition', 'configure_state_tree_task',
            // Smart Objects
            'create_smart_object_definition', 'add_smart_object_slot', 'configure_slot_behavior', 'add_smart_object_component',
            // Mass AI
            'create_mass_entity_config', 'configure_mass_entity', 'add_mass_spawner',
            // Utility
            'get_ai_info'
          ],
          description: 'AI action to perform'
        },

        // Common parameters
        name: { type: 'string', description: 'Name for new AI asset.' },
        path: { type: 'string', description: 'Directory path for asset creation.' },
        blueprintPath: { type: 'string', description: 'Path to blueprint for component additions.' },

        // AI Controller
        controllerPath: { type: 'string', description: 'Path to AI controller blueprint.' },
        behaviorTreePath: { type: 'string', description: 'Path to behavior tree asset.' },
        blackboardPath: { type: 'string', description: 'Path to blackboard asset.' },
        parentClass: {
          type: 'string',
          enum: ['AAIController', 'APlayerController'],
          description: 'Parent class for AI controller (default: AAIController).'
        },
        autoRunBehaviorTree: { type: 'boolean', description: 'Start behavior tree automatically on possess.' },

        // Blackboard Keys
        keyName: { type: 'string', description: 'Blackboard key name.' },
        keyType: {
          type: 'string',
          enum: ['Bool', 'Int', 'Float', 'Vector', 'Rotator', 'Object', 'Class', 'Enum', 'Name', 'String'],
          description: 'Blackboard key data type.'
        },
        isInstanceSynced: { type: 'boolean', description: 'Sync key across instances.' },
        baseObjectClass: { type: 'string', description: 'Base class for Object/Class keys.' },
        enumClass: { type: 'string', description: 'Enum class for Enum keys.' },

        // Behavior Tree Nodes
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
        parentNodeId: { type: 'string', description: 'Parent node ID to attach new node to.' },
        nodeId: { type: 'string', description: 'Node ID for configuration.' },
        nodeProperties: {
          type: 'object',
          additionalProperties: true,
          description: 'Properties to set on the node.'
        },
        customTaskClass: { type: 'string', description: 'Custom task class path for Custom task type.' },
        customDecoratorClass: { type: 'string', description: 'Custom decorator class path.' },
        customServiceClass: { type: 'string', description: 'Custom service class path.' },

        // EQS
        queryPath: { type: 'string', description: 'Path to EQS query asset.' },
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
            searchRadius: { type: 'number' },
            searchCenter: { type: 'string' },
            actorClass: { type: 'string' },
            gridSize: { type: 'number' },
            spacesBetween: { type: 'number' },
            innerRadius: { type: 'number' },
            outerRadius: { type: 'number' }
          },
          description: 'Generator-specific settings.'
        },
        testSettings: {
          type: 'object',
          properties: {
            scoringEquation: { type: 'string', enum: ['Linear', 'Square', 'InverseLinear', 'Constant'] },
            clampMin: { type: 'number' },
            clampMax: { type: 'number' },
            filterType: { type: 'string', enum: ['Minimum', 'Maximum', 'Range'] },
            floatMin: { type: 'number' },
            floatMax: { type: 'number' }
          },
          description: 'Test scoring and filter settings.'
        },
        testIndex: { type: 'number', description: 'Index of test to configure.' },

        // Perception
        sightConfig: {
          type: 'object',
          properties: {
            sightRadius: { type: 'number' },
            loseSightRadius: { type: 'number' },
            peripheralVisionAngle: { type: 'number' },
            pointOfViewBackwardOffset: { type: 'number' },
            nearClippingRadius: { type: 'number' },
            autoSuccessRange: { type: 'number' },
            maxAge: { type: 'number' },
            detectionByAffiliation: {
              type: 'object',
              properties: {
                enemies: { type: 'boolean' },
                neutrals: { type: 'boolean' },
                friendlies: { type: 'boolean' }
              }
            }
          },
          description: 'AI sight sense configuration.'
        },
        hearingConfig: {
          type: 'object',
          properties: {
            hearingRange: { type: 'number' },
            loSHearingRange: { type: 'number' },
            detectFriendly: { type: 'boolean' },
            maxAge: { type: 'number' }
          },
          description: 'AI hearing sense configuration.'
        },
        damageConfig: {
          type: 'object',
          properties: {
            maxAge: { type: 'number' }
          },
          description: 'AI damage sense configuration.'
        },
        teamId: { type: 'number', description: 'Team ID for perception affiliation (0=Neutral, 1=Player, 2=Enemy, etc.).' },
        dominantSense: {
          type: 'string',
          enum: ['Sight', 'Hearing', 'Damage', 'Touch', 'None'],
          description: 'Dominant sense for perception prioritization.'
        },

        // State Trees (UE5.3+)
        stateTreePath: { type: 'string', description: 'Path to State Tree asset.' },
        stateName: { type: 'string', description: 'Name of state to add/configure.' },
        fromState: { type: 'string', description: 'Source state for transition.' },
        toState: { type: 'string', description: 'Target state for transition.' },
        transitionCondition: { type: 'string', description: 'Condition expression for transition.' },
        stateTaskClass: { type: 'string', description: 'Task class for state.' },
        stateEvaluatorClass: { type: 'string', description: 'Evaluator class for state.' },

        // Smart Objects
        definitionPath: { type: 'string', description: 'Path to Smart Object Definition asset.' },
        slotIndex: { type: 'number', description: 'Index of slot to configure.' },
        slotOffset: {
          type: 'object',
          properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } },
          description: 'Local offset for slot.'
        },
        slotRotation: {
          type: 'object',
          properties: { pitch: { type: 'number' }, yaw: { type: 'number' }, roll: { type: 'number' } },
          description: 'Local rotation for slot.'
        },
        slotBehaviorDefinition: { type: 'string', description: 'Gameplay behavior definition for slot.' },
        slotActivityTags: {
          type: 'array',
          items: { type: 'string' },
          description: 'Activity tags for the slot.'
        },
        slotUserTags: {
          type: 'array',
          items: { type: 'string' },
          description: 'Required user tags for slot.'
        },
        slotEnabled: { type: 'boolean', description: 'Whether slot is enabled.' },

        // Mass AI
        configPath: { type: 'string', description: 'Path to Mass Entity Config asset.' },
        massTraits: {
          type: 'array',
          items: { type: 'string' },
          description: 'List of Mass traits to add.'
        },
        massProcessors: {
          type: 'array',
          items: { type: 'string' },
          description: 'List of Mass processors to configure.'
        },
        spawnerSettings: {
          type: 'object',
          properties: {
            entityCount: { type: 'number' },
            spawnRadius: { type: 'number' },
            entityConfig: { type: 'string' },
            spawnOnBeginPlay: { type: 'boolean' }
          },
          description: 'Mass spawner configuration.'
        }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        message: { type: 'string' },
        assetPath: { type: 'string', description: 'Path to created/modified asset.' },
        controllerPath: { type: 'string', description: 'Path to created AI controller.' },
        behaviorTreePath: { type: 'string', description: 'Path to created behavior tree.' },
        blackboardPath: { type: 'string', description: 'Path to created blackboard.' },
        queryPath: { type: 'string', description: 'Path to created EQS query.' },
        stateTreePath: { type: 'string', description: 'Path to created state tree.' },
        definitionPath: { type: 'string', description: 'Path to created smart object definition.' },
        configPath: { type: 'string', description: 'Path to created mass entity config.' },
        nodeId: { type: 'string', description: 'ID of created BT/StateTree node.' },
        keyIndex: { type: 'number', description: 'Index of added blackboard key.' },
        testIndex: { type: 'number', description: 'Index of added EQS test.' },
        slotIndex: { type: 'number', description: 'Index of added smart object slot.' },
        aiInfo: {
          type: 'object',
          properties: {
            controllerClass: { type: 'string' },
            assignedBehaviorTree: { type: 'string' },
            assignedBlackboard: { type: 'string' },
            blackboardKeys: { type: 'array', items: { type: 'object' } },
            btNodeCount: { type: 'number' },
            perceptionSenses: { type: 'array', items: { type: 'string' } },
            teamId: { type: 'number' },
            stateTreeStates: { type: 'array', items: { type: 'string' } },
            smartObjectSlots: { type: 'number' },
            massTraits: { type: 'array', items: { type: 'string' } }
          },
          description: 'AI configuration info (for get_ai_info).'
        },
        error: { type: 'string' }
      }
    }
  },

  // ============================================================================
  // 28. Inventory & Items System (Phase 17)
  // ============================================================================
  {
    name: 'manage_inventory',
    description: 'Create item data assets, inventory components, pickup actors, equipment slots, loot tables, and crafting recipes.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            // Data Assets (4)
            'create_item_data_asset',
            'set_item_properties',
            'create_item_category',
            'assign_item_category',
            // Inventory Component (5)
            'create_inventory_component',
            'configure_inventory_slots',
            'add_inventory_functions',
            'configure_inventory_events',
            'set_inventory_replication',
            // Pickups (4)
            'create_pickup_actor',
            'configure_pickup_interaction',
            'configure_pickup_respawn',
            'configure_pickup_effects',
            // Equipment (5)
            'create_equipment_component',
            'define_equipment_slots',
            'configure_equipment_effects',
            'add_equipment_functions',
            'configure_equipment_visuals',
            // Loot System (4)
            'create_loot_table',
            'add_loot_entry',
            'configure_loot_drop',
            'set_loot_quality_tiers',
            // Crafting (4)
            'create_crafting_recipe',
            'configure_recipe_requirements',
            'create_crafting_station',
            'add_crafting_component',
            // Utility (1)
            'get_inventory_info'
          ],
          description: 'Inventory action to perform.'
        },
        // Common parameters
        name: { type: 'string', description: 'Asset name for creation.' },
        path: { type: 'string', description: 'Directory path for asset creation.' },
        folder: { type: 'string', description: 'Content folder for asset.' },
        save: { type: 'boolean', description: 'Save asset after modification.' },
        blueprintPath: { type: 'string', description: 'Path to Blueprint asset.' },

        // Item Data Asset parameters
        itemPath: { type: 'string', description: 'Path to item data asset.' },
        parentClass: { type: 'string', description: 'Parent class for data asset.' },
        displayName: { type: 'string', description: 'Display name for item.' },
        description: { type: 'string', description: 'Item description.' },
        icon: { type: 'string', description: 'Path to icon texture.' },
        mesh: { type: 'string', description: 'Path to static/skeletal mesh.' },
        stackSize: { type: 'number', description: 'Maximum stack size (1 for non-stackable).' },
        weight: { type: 'number', description: 'Item weight for inventory capacity.' },
        rarity: {
          type: 'string',
          enum: ['Common', 'Uncommon', 'Rare', 'Epic', 'Legendary', 'Custom'],
          description: 'Item rarity tier.'
        },
        value: { type: 'number', description: 'Base item value/price.' },
        tags: {
          type: 'array',
          items: { type: 'string' },
          description: 'Gameplay tags for item categorization.'
        },
        customProperties: {
          type: 'object',
          additionalProperties: true,
          description: 'Custom key-value properties for item.'
        },

        // Item Category parameters
        categoryPath: { type: 'string', description: 'Path to item category asset.' },
        parentCategory: { type: 'string', description: 'Parent category path.' },
        categoryIcon: { type: 'string', description: 'Icon texture for category.' },

        // Inventory Component parameters
        componentName: { type: 'string', description: 'Name for the component.' },
        slotCount: { type: 'number', description: 'Number of inventory slots.' },
        slotSize: {
          type: 'object',
          properties: { width: { type: 'number' }, height: { type: 'number' } },
          description: 'Size of each slot (for grid inventory).'
        },
        maxWeight: { type: 'number', description: 'Maximum weight capacity.' },
        allowStacking: { type: 'boolean', description: 'Allow items to stack.' },
        slotCategories: {
          type: 'array',
          items: { type: 'string' },
          description: 'Allowed item categories per slot.'
        },
        slotRestrictions: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              slotIndex: { type: 'number' },
              allowedCategories: { type: 'array', items: { type: 'string' } },
              restrictedCategories: { type: 'array', items: { type: 'string' } }
            }
          },
          description: 'Per-slot category restrictions.'
        },
        replicated: { type: 'boolean', description: 'Enable network replication.' },
        replicationCondition: {
          type: 'string',
          enum: ['None', 'OwnerOnly', 'SkipOwner', 'SimulatedOnly', 'AutonomousOnly', 'Custom'],
          description: 'Replication condition for inventory.'
        },

        // Pickup parameters
        pickupPath: { type: 'string', description: 'Path to pickup actor Blueprint.' },
        meshPath: { type: 'string', description: 'Path to mesh for pickup.' },
        itemDataPath: { type: 'string', description: 'Path to item data asset.' },
        interactionRadius: { type: 'number', description: 'Radius for pickup interaction.' },
        interactionType: {
          type: 'string',
          enum: ['Overlap', 'Interact', 'Key', 'Hold'],
          description: 'How player picks up item.'
        },
        interactionKey: { type: 'string', description: 'Input action for pickup (if type is Key/Hold).' },
        prompt: { type: 'string', description: 'Interaction prompt text.' },
        highlightMaterial: { type: 'string', description: 'Material for highlight effect.' },
        respawnable: { type: 'boolean', description: 'Whether pickup respawns.' },
        respawnTime: { type: 'number', description: 'Time in seconds to respawn.' },
        respawnEffect: { type: 'string', description: 'Niagara effect for respawn.' },
        pickupSound: { type: 'string', description: 'Sound cue for pickup.' },
        pickupParticle: { type: 'string', description: 'Particle effect on pickup.' },
        bobbing: { type: 'boolean', description: 'Enable bobbing animation.' },
        rotation: { type: 'boolean', description: 'Enable rotation animation.' },
        glowEffect: { type: 'boolean', description: 'Enable glow effect.' },

        // Equipment parameters
        slots: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              name: { type: 'string' },
              socketName: { type: 'string' },
              allowedCategories: { type: 'array', items: { type: 'string' } },
              restrictedCategories: { type: 'array', items: { type: 'string' } }
            }
          },
          description: 'Equipment slot definitions.'
        },
        statModifiers: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              attribute: { type: 'string' },
              operation: { type: 'string', enum: ['Add', 'Multiply', 'Override'] },
              value: { type: 'number' }
            }
          },
          description: 'Stat modifiers when equipped.'
        },
        abilityGrants: {
          type: 'array',
          items: { type: 'string' },
          description: 'Gameplay abilities granted when equipped.'
        },
        passiveEffects: {
          type: 'array',
          items: { type: 'string' },
          description: 'Passive gameplay effects when equipped.'
        },
        attachToSocket: { type: 'boolean', description: 'Attach mesh to socket when equipped.' },
        meshComponent: { type: 'string', description: 'Component name for equipment mesh.' },
        animationOverrides: {
          type: 'object',
          additionalProperties: { type: 'string' },
          description: 'Animation overrides (slot -> anim asset).'
        },

        // Loot Table parameters
        lootTablePath: { type: 'string', description: 'Path to loot table asset.' },
        lootWeight: { type: 'number', description: 'Weight for drop chance calculation.' },
        minQuantity: { type: 'number', description: 'Minimum drop quantity.' },
        maxQuantity: { type: 'number', description: 'Maximum drop quantity.' },
        conditions: {
          type: 'array',
          items: { type: 'string' },
          description: 'Conditions for loot entry (gameplay tag expressions).'
        },
        actorPath: { type: 'string', description: 'Path to actor Blueprint for loot drop.' },
        dropCount: { type: 'number', description: 'Number of drops to roll.' },
        guaranteedDrops: {
          type: 'array',
          items: { type: 'string' },
          description: 'Item paths that always drop.'
        },
        dropRadius: { type: 'number', description: 'Radius for scattered drops.' },
        dropForce: { type: 'number', description: 'Physics force applied to drops.' },
        tiers: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              name: { type: 'string' },
              color: {
                type: 'object',
                properties: { r: { type: 'number' }, g: { type: 'number' }, b: { type: 'number' }, a: { type: 'number' } }
              },
              dropWeight: { type: 'number' },
              statMultiplier: { type: 'number' }
            }
          },
          description: 'Quality tier definitions.'
        },

        // Crafting parameters
        recipePath: { type: 'string', description: 'Path to crafting recipe asset.' },
        outputItemPath: { type: 'string', description: 'Path to item produced by recipe.' },
        outputQuantity: { type: 'number', description: 'Quantity produced.' },
        ingredients: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              itemPath: { type: 'string' },
              quantity: { type: 'number' }
            }
          },
          description: 'Required ingredients with quantities.'
        },
        craftTime: { type: 'number', description: 'Time in seconds to craft.' },
        requiredLevel: { type: 'number', description: 'Required player level.' },
        requiredSkills: {
          type: 'array',
          items: { type: 'string' },
          description: 'Required skill tags.'
        },
        requiredStation: { type: 'string', description: 'Required crafting station type.' },
        unlockConditions: {
          type: 'array',
          items: { type: 'string' },
          description: 'Conditions to unlock recipe.'
        },
        recipes: {
          type: 'array',
          items: { type: 'string' },
          description: 'Recipe paths for crafting station.'
        },
        stationType: { type: 'string', description: 'Type of crafting station.' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        message: { type: 'string' },
        assetPath: { type: 'string', description: 'Path to created/modified asset.' },
        itemPath: { type: 'string', description: 'Path to created item data asset.' },
        categoryPath: { type: 'string', description: 'Path to created category.' },
        pickupPath: { type: 'string', description: 'Path to created pickup actor.' },
        lootTablePath: { type: 'string', description: 'Path to created loot table.' },
        recipePath: { type: 'string', description: 'Path to created recipe.' },
        stationPath: { type: 'string', description: 'Path to created crafting station.' },
        componentAdded: { type: 'boolean', description: 'Whether component was added.' },
        slotCount: { type: 'number', description: 'Number of slots configured.' },
        entryIndex: { type: 'number', description: 'Index of added loot entry.' },
        inventoryInfo: {
          type: 'object',
          properties: {
            assetType: { type: 'string', enum: ['Item', 'Inventory', 'Pickup', 'LootTable', 'Recipe', 'Station'] },
            itemProperties: {
              type: 'object',
              properties: {
                displayName: { type: 'string' },
                stackSize: { type: 'number' },
                weight: { type: 'number' },
                rarity: { type: 'string' },
                value: { type: 'number' }
              }
            },
            inventorySlots: { type: 'number' },
            maxWeight: { type: 'number' },
            equipmentSlots: { type: 'array', items: { type: 'string' } },
            lootEntries: { type: 'number' },
            recipeIngredients: { type: 'array', items: { type: 'object' } },
            craftTime: { type: 'number' }
          },
          description: 'Inventory system info (for get_inventory_info).'
        },
        error: { type: 'string' }
      }
    }
  },

  // ============================================================================
  // PHASE 18: INTERACTION SYSTEM
  // ============================================================================
  {
    name: 'manage_interaction',
    description: 'Create interactive objects: doors, switches, chests, levers. Set up destructible meshes and trigger volumes.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            // Interaction Component
            'create_interaction_component',
            'configure_interaction_trace',
            'configure_interaction_widget',
            'add_interaction_events',
            // Interactables
            'create_interactable_interface',
            'create_door_actor',
            'configure_door_properties',
            'create_switch_actor',
            'configure_switch_properties',
            'create_chest_actor',
            'configure_chest_properties',
            'create_lever_actor',
            // Destructibles
            'setup_destructible_mesh',
            'configure_destruction_levels',
            'configure_destruction_effects',
            'configure_destruction_damage',
            'add_destruction_component',
            // Triggers
            'create_trigger_actor',
            'configure_trigger_events',
            'configure_trigger_filter',
            'configure_trigger_response',
            // Utility
            'get_interaction_info'
          ],
          description: 'The interaction action to perform.'
        },
        // Common parameters
        name: { type: 'string', description: 'Name for created asset/actor.' },
        folder: { type: 'string', description: 'Folder path for created asset.' },
        blueprintPath: { type: 'string', description: 'Path to target blueprint.' },
        actorName: { type: 'string', description: 'Name of target actor in level.' },
        componentName: { type: 'string', description: 'Name for added component.' },

        // Interaction Component parameters
        traceType: { type: 'string', enum: ['line', 'sphere', 'box'], description: 'Type of interaction trace.' },
        traceChannel: { type: 'string', description: 'Collision channel for trace.' },
        traceDistance: { type: 'number', description: 'Maximum trace distance.' },
        traceRadius: { type: 'number', description: 'Radius for sphere/box trace.' },
        traceFrequency: { type: 'number', description: 'Trace update frequency (per second).' },
        widgetClass: { type: 'string', description: 'Widget class for interaction prompt.' },
        widgetOffset: {
          type: 'object',
          properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } },
          description: 'Widget offset from actor.'
        },
        showOnHover: { type: 'boolean', description: 'Show widget when hovering.' },
        showPromptText: { type: 'boolean', description: 'Show interaction prompt text.' },
        promptTextFormat: { type: 'string', description: 'Format string for prompt (e.g., "Press {Key} to {Action}").' },

        // Door parameters
        doorPath: { type: 'string', description: 'Path to door actor blueprint.' },
        meshPath: { type: 'string', description: 'Path to static/skeletal mesh.' },
        openAngle: { type: 'number', description: 'Door open rotation angle in degrees.' },
        openTime: { type: 'number', description: 'Time to open/close door in seconds.' },
        openDirection: { type: 'string', enum: ['push', 'pull', 'auto'], description: 'Door open direction.' },
        pivotOffset: {
          type: 'object',
          properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } },
          description: 'Offset for door pivot point.'
        },
        locked: { type: 'boolean', description: 'Whether door/chest is locked.' },
        keyItemPath: { type: 'string', description: 'Item required to unlock.' },
        openSound: { type: 'string', description: 'Sound to play on open.' },
        closeSound: { type: 'string', description: 'Sound to play on close.' },
        autoClose: { type: 'boolean', description: 'Automatically close after opening.' },
        autoCloseDelay: { type: 'number', description: 'Delay before auto-close in seconds.' },
        requiresKey: { type: 'boolean', description: 'Whether interaction requires a key item.' },

        // Switch parameters
        switchPath: { type: 'string', description: 'Path to switch actor blueprint.' },
        switchType: { type: 'string', enum: ['button', 'lever', 'pressure_plate', 'toggle'], description: 'Type of switch.' },
        toggleable: { type: 'boolean', description: 'Whether switch can be toggled.' },
        oneShot: { type: 'boolean', description: 'Whether switch can only be used once.' },
        resetTime: { type: 'number', description: 'Time to reset switch in seconds.' },
        activateSound: { type: 'string', description: 'Sound on activation.' },
        deactivateSound: { type: 'string', description: 'Sound on deactivation.' },
        targetActors: {
          type: 'array',
          items: { type: 'string' },
          description: 'Actors affected by this switch.'
        },

        // Chest parameters
        chestPath: { type: 'string', description: 'Path to chest actor blueprint.' },
        lidMeshPath: { type: 'string', description: 'Path to lid mesh.' },
        lootTablePath: { type: 'string', description: 'Path to loot table for contents.' },
        respawnable: { type: 'boolean', description: 'Whether chest respawns contents.' },
        respawnTime: { type: 'number', description: 'Time to respawn contents in seconds.' },

        // Lever parameters
        leverType: { type: 'string', enum: ['rotate', 'translate'], description: 'Lever movement type.' },
        moveDistance: { type: 'number', description: 'Distance for translation lever.' },
        moveTime: { type: 'number', description: 'Time for lever movement.' },

        // Destructible parameters
        fractureMode: { type: 'string', enum: ['voronoi', 'uniform', 'radial', 'custom'], description: 'Fracture pattern type.' },
        fracturePieces: { type: 'number', description: 'Number of fracture pieces.' },
        enablePhysics: { type: 'boolean', description: 'Enable physics on destruction.' },
        levels: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              damageThreshold: { type: 'number' },
              meshIndex: { type: 'number' },
              enablePhysics: { type: 'boolean' },
              removeAfterTime: { type: 'number' }
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
          items: { type: 'number' },
          description: 'Damage thresholds for destruction levels.'
        },
        impactDamageMultiplier: { type: 'number', description: 'Multiplier for impact damage.' },
        radialDamageMultiplier: { type: 'number', description: 'Multiplier for radial damage.' },
        autoDestroy: { type: 'boolean', description: 'Automatically destroy at zero health.' },

        // Trigger parameters
        triggerPath: { type: 'string', description: 'Path to trigger actor blueprint.' },
        triggerShape: { type: 'string', enum: ['box', 'sphere', 'capsule'], description: 'Shape of trigger volume.' },
        size: {
          type: 'object',
          properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } },
          description: 'Size of trigger volume.'
        },
        filterByTag: { type: 'string', description: 'Actor tag filter for trigger.' },
        filterByClass: { type: 'string', description: 'Actor class filter for trigger.' },
        filterByInterface: { type: 'string', description: 'Interface filter for trigger.' },
        ignoreClasses: {
          type: 'array',
          items: { type: 'string' },
          description: 'Classes to ignore in trigger.'
        },
        ignoreTags: {
          type: 'array',
          items: { type: 'string' },
          description: 'Tags to ignore in trigger.'
        },
        onEnterEvent: { type: 'string', description: 'Event dispatcher name for enter.' },
        onExitEvent: { type: 'string', description: 'Event dispatcher name for exit.' },
        onStayEvent: { type: 'string', description: 'Event dispatcher name for stay.' },
        stayInterval: { type: 'number', description: 'Interval for stay events in seconds.' },
        responseType: { type: 'string', enum: ['once', 'repeatable', 'toggle'], description: 'How trigger responds.' },
        cooldown: { type: 'number', description: 'Cooldown between activations in seconds.' },
        maxActivations: { type: 'number', description: 'Maximum number of activations (0 = unlimited).' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        message: { type: 'string' },
        assetPath: { type: 'string', description: 'Path to created/modified asset.' },
        blueprintPath: { type: 'string', description: 'Path to created blueprint.' },
        interfacePath: { type: 'string', description: 'Path to created interface.' },
        componentAdded: { type: 'boolean', description: 'Whether component was added.' },
        interactionInfo: {
          type: 'object',
          properties: {
            assetType: { type: 'string', enum: ['Door', 'Switch', 'Chest', 'Lever', 'Trigger', 'Destructible', 'Component'] },
            isLocked: { type: 'boolean' },
            isOpen: { type: 'boolean' },
            health: { type: 'number' },
            maxHealth: { type: 'number' },
            interactionEnabled: { type: 'boolean' },
            triggerShape: { type: 'string' },
            destructionLevel: { type: 'number' }
          },
          description: 'Interaction system info (for get_interaction_info).'
        },
        error: { type: 'string' }
      }
    }
  },

  // ============================================================================
  // PHASE 19: COMPLETE UI/UX SYSTEM
  // ============================================================================
  {
    name: 'manage_widget_authoring',
    description: 'Create UMG widgets: buttons, text, images, sliders. Configure layouts, bindings, animations. Build HUDs and menus.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            // Widget Creation
            'create_widget_blueprint',
            'set_widget_parent_class',
            // Layout Panels
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
            // Common Widgets
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
            // Layout & Styling
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
            // Bindings & Events
            'create_property_binding',
            'bind_text',
            'bind_visibility',
            'bind_color',
            'bind_enabled',
            'bind_on_clicked',
            'bind_on_hovered',
            'bind_on_value_changed',
            // Widget Animations
            'create_widget_animation',
            'add_animation_track',
            'add_animation_keyframe',
            'set_animation_loop',
            // UI Templates
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
            // Utility
            'get_widget_info',
            'preview_widget'
          ],
          description: 'The widget authoring action to perform.'
        },
        // Common parameters
        name: { type: 'string', description: 'Name for created widget/asset.' },
        folder: { type: 'string', description: 'Folder path for created asset.' },
        widgetPath: { type: 'string', description: 'Path to target widget blueprint.' },
        slotName: { type: 'string', description: 'Name of widget slot to target.' },
        parentSlot: { type: 'string', description: 'Parent slot to add widget to.' },
        parentClass: { type: 'string', description: 'Parent class for widget.' },

        // Layout properties
        anchorMin: {
          type: 'object',
          properties: { x: { type: 'number' }, y: { type: 'number' } },
          description: 'Minimum anchor point (0-1).'
        },
        anchorMax: {
          type: 'object',
          properties: { x: { type: 'number' }, y: { type: 'number' } },
          description: 'Maximum anchor point (0-1).'
        },
        alignment: {
          type: 'object',
          properties: { x: { type: 'number' }, y: { type: 'number' } },
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

        // Transform properties
        translation: {
          type: 'object',
          properties: { x: { type: 'number' }, y: { type: 'number' } },
          description: 'Render translation.'
        },
        scale: {
          type: 'object',
          properties: { x: { type: 'number' }, y: { type: 'number' } },
          description: 'Render scale.'
        },
        shear: {
          type: 'object',
          properties: { x: { type: 'number' }, y: { type: 'number' } },
          description: 'Render shear.'
        },
        angle: { type: 'number', description: 'Rotation angle in degrees.' },
        pivot: {
          type: 'object',
          properties: { x: { type: 'number' }, y: { type: 'number' } },
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

        // Widget properties
        text: { type: 'string', description: 'Text content.' },
        font: { type: 'string', description: 'Font asset path.' },
        fontSize: { type: 'number', description: 'Font size.' },
        colorAndOpacity: {
          type: 'object',
          properties: { r: { type: 'number' }, g: { type: 'number' }, b: { type: 'number' }, a: { type: 'number' } },
          description: 'Color and opacity (0-1 values).'
        },
        justification: {
          type: 'string',
          enum: ['Left', 'Center', 'Right'],
          description: 'Text justification.'
        },
        autoWrap: { type: 'boolean', description: 'Enable text auto-wrap.' },
        texturePath: { type: 'string', description: 'Texture/image asset path.' },
        brushSize: {
          type: 'object',
          properties: { x: { type: 'number' }, y: { type: 'number' } },
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
        minValue: { type: 'number', description: 'Minimum value.' },
        maxValue: { type: 'number', description: 'Maximum value.' },
        stepSize: { type: 'number', description: 'Value step size.' },
        delta: { type: 'number', description: 'Spinbox increment.' },
        percent: { type: 'number', description: 'Progress bar percentage (0-1).' },
        fillColorAndOpacity: {
          type: 'object',
          properties: { r: { type: 'number' }, g: { type: 'number' }, b: { type: 'number' }, a: { type: 'number' } },
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
          items: { type: 'string' },
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

        // Grid/layout panel properties
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
          properties: { r: { type: 'number' }, g: { type: 'number' }, b: { type: 'number' }, a: { type: 'number' } },
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

        // Styling properties
        color: {
          type: 'object',
          properties: { r: { type: 'number' }, g: { type: 'number' }, b: { type: 'number' }, a: { type: 'number' } },
          description: 'Widget color.'
        },
        opacity: { type: 'number', description: 'Widget opacity (0-1).' },
        brush: { type: 'string', description: 'Brush asset path.' },
        backgroundImage: { type: 'string', description: 'Background image path.' },
        style: { type: 'string', description: 'Style preset name.' },

        // Binding properties
        propertyName: { type: 'string', description: 'Property to bind.' },
        bindingType: { type: 'string', enum: ['function', 'variable'], description: 'Binding type.' },
        bindingSource: { type: 'string', description: 'Variable or function name to bind to.' },
        functionName: { type: 'string', description: 'Function to call on event.' },
        onHoveredFunction: { type: 'string', description: 'Function to call on hover.' },
        onUnhoveredFunction: { type: 'string', description: 'Function to call on unhover.' },

        // Animation properties
        animationName: { type: 'string', description: 'Name of widget animation.' },
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

        // Template properties
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
          items: { type: 'string' },
          description: 'HUD elements to include.'
        },

        // HUD element properties
        barStyle: {
          type: 'string',
          enum: ['simple', 'segmented', 'radial'],
          description: 'Health bar style.'
        },
        showNumbers: { type: 'boolean', description: 'Show numeric values.' },
        barColor: {
          type: 'object',
          properties: { r: { type: 'number' }, g: { type: 'number' }, b: { type: 'number' }, a: { type: 'number' } },
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
        fadeTime: { type: 'number', description: 'Fade time in seconds.' },
        gridSize: {
          type: 'object',
          properties: { columns: { type: 'number' }, rows: { type: 'number' } },
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

        // Preview properties
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
        success: { type: 'boolean' },
        message: { type: 'string' },
        widgetPath: { type: 'string', description: 'Path to created/modified widget.' },
        slotName: { type: 'string', description: 'Name of created slot.' },
        animationName: { type: 'string', description: 'Name of created animation.' },
        trackIndex: { type: 'number', description: 'Index of created track.' },
        keyframeIndex: { type: 'number', description: 'Index of created keyframe.' },
        bindingCreated: { type: 'boolean', description: 'Whether binding was created.' },
        widgetInfo: {
          type: 'object',
          properties: {
            widgetClass: { type: 'string' },
            parentClass: { type: 'string' },
            slots: { type: 'array', items: { type: 'string' } },
            animations: { type: 'array', items: { type: 'string' } },
            variables: { type: 'array', items: { type: 'string' } },
            functions: { type: 'array', items: { type: 'string' } },
            eventDispatchers: { type: 'array', items: { type: 'string' } }
          },
          description: 'Widget info (for get_widget_info).'
        },
        error: { type: 'string' }
      }
    }
  },

  // ============================================================================
  // 34. Networking & Multiplayer (Phase 20)
  // ============================================================================
  {
    name: 'manage_networking',
    description: 'Configure multiplayer: property replication, RPCs (Server/Client/Multicast), authority, relevancy, and network prediction.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            // Replication (6)
            'set_property_replicated',
            'set_replication_condition',
            'configure_net_update_frequency',
            'configure_net_priority',
            'set_net_dormancy',
            'configure_replication_graph',
            // RPCs (3)
            'create_rpc_function',
            'configure_rpc_validation',
            'set_rpc_reliability',
            // Authority & Ownership (4)
            'set_owner',
            'set_autonomous_proxy',
            'check_has_authority',
            'check_is_locally_controlled',
            // Network Relevancy (3)
            'configure_net_cull_distance',
            'set_always_relevant',
            'set_only_relevant_to_owner',
            // Net Serialization (3)
            'configure_net_serialization',
            'set_replicated_using',
            'configure_push_model',
            // Network Prediction (4)
            'configure_client_prediction',
            'configure_server_correction',
            'add_network_prediction_data',
            'configure_movement_prediction',
            // Connection & Session (3)
            'configure_net_driver',
            'set_net_role',
            'configure_replicated_movement',
            // Utility (1)
            'get_networking_info'
          ],
          description: 'Networking action to perform'
        },

        // -- Asset/Actor Identification --
        blueprintPath: { type: 'string', description: 'Path to blueprint asset (e.g., /Game/Blueprints/BP_MyActor).' },
        actorName: { type: 'string', description: 'Name of runtime actor in level.' },

        // -- Property Replication --
        propertyName: { type: 'string', description: 'Name of property to configure.' },
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
        repNotifyFunc: { type: 'string', description: 'RepNotify function name for ReplicatedUsing.' },

        // -- Net Update Frequency --
        netUpdateFrequency: { type: 'number', description: 'How often actor replicates (Hz, default 100).' },
        minNetUpdateFrequency: { type: 'number', description: 'Minimum update frequency when idle (Hz, default 2).' },

        // -- Net Priority --
        netPriority: { type: 'number', description: 'Network priority for bandwidth (default 1.0).' },

        // -- Net Dormancy --
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

        // -- Replication Graph --
        nodeClass: { type: 'string', description: 'Custom replication graph node class.' },
        spatialBias: { type: 'number', description: 'Spatial bias for replication graph.' },
        defaultSettingsClass: { type: 'string', description: 'Default replication settings class.' },

        // -- RPC Creation --
        functionName: { type: 'string', description: 'Name of RPC function.' },
        rpcType: {
          type: 'string',
          enum: ['Server', 'Client', 'NetMulticast'],
          description: 'Type of RPC.'
        },
        reliable: { type: 'boolean', description: 'Whether RPC is reliable.' },
        parameters: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              name: { type: 'string' },
              type: { type: 'string' }
            }
          },
          description: 'RPC function parameters.'
        },
        returnType: { type: 'string', description: 'RPC return type (usually void).' },

        // -- RPC Validation --
        validationFunctionName: { type: 'string', description: 'Name of validation function.' },
        withValidation: { type: 'boolean', description: 'Enable RPC validation.' },

        // -- Authority & Ownership --
        ownerActorName: { type: 'string', description: 'Name of owner actor (null to clear).' },
        isAutonomousProxy: { type: 'boolean', description: 'Configure as autonomous proxy.' },

        // -- Network Relevancy --
        netCullDistanceSquared: { type: 'number', description: 'Network cull distance squared.' },
        useOwnerNetRelevancy: { type: 'boolean', description: 'Use owner relevancy.' },
        alwaysRelevant: { type: 'boolean', description: 'Always relevant to all clients.' },
        onlyRelevantToOwner: { type: 'boolean', description: 'Only relevant to owner.' },

        // -- Net Serialization --
        structName: { type: 'string', description: 'Name of struct for custom serialization.' },
        useNetSerialize: { type: 'boolean', description: 'Use custom NetSerialize.' },
        usePushModel: { type: 'boolean', description: 'Use push-model replication.' },
        propertyNames: {
          type: 'array',
          items: { type: 'string' },
          description: 'Properties for push model.'
        },

        // -- Network Prediction --
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
              name: { type: 'string' },
              type: { type: 'string' }
            }
          },
          description: 'Predicted properties.'
        },

        // -- Movement Prediction --
        networkSmoothingMode: {
          type: 'string',
          enum: ['Disabled', 'Linear', 'Exponential'],
          description: 'Movement smoothing mode.'
        },
        networkMaxSmoothUpdateDistance: { type: 'number', description: 'Max smooth update distance.' },
        networkNoSmoothUpdateDistance: { type: 'number', description: 'No smooth update distance.' },

        // -- Net Driver --
        maxClientRate: { type: 'number', description: 'Max client rate.' },
        maxInternetClientRate: { type: 'number', description: 'Max internet client rate.' },
        netServerMaxTickRate: { type: 'number', description: 'Server max tick rate.' },

        // -- Net Role --
        role: {
          type: 'string',
          enum: ['ROLE_None', 'ROLE_SimulatedProxy', 'ROLE_AutonomousProxy', 'ROLE_Authority'],
          description: 'Net role.'
        },

        // -- Replicated Movement --
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

        // -- Options --
        save: { type: 'boolean', description: 'Save asset after operation.' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        message: { type: 'string' },
        blueprintPath: { type: 'string', description: 'Path to modified blueprint.' },
        functionName: { type: 'string', description: 'Created RPC function name.' },
        hasAuthority: { type: 'boolean', description: 'Authority check result.' },
        isLocallyControlled: { type: 'boolean', description: 'Local control check result.' },
        role: { type: 'string', description: 'Current net role.' },
        remoteRole: { type: 'string', description: 'Current remote role.' },
        networkingInfo: {
          type: 'object',
          properties: {
            bReplicates: { type: 'boolean' },
            bAlwaysRelevant: { type: 'boolean' },
            bOnlyRelevantToOwner: { type: 'boolean' },
            netUpdateFrequency: { type: 'number' },
            minNetUpdateFrequency: { type: 'number' },
            netPriority: { type: 'number' },
            netDormancy: { type: 'string' },
            netCullDistanceSquared: { type: 'number' },
            replicatedProperties: {
              type: 'array',
              items: {
                type: 'object',
                properties: {
                  name: { type: 'string' },
                  condition: { type: 'string' },
                  repNotifyFunc: { type: 'string' }
                }
              }
            },
            rpcFunctions: {
              type: 'array',
              items: {
                type: 'object',
                properties: {
                  name: { type: 'string' },
                  type: { type: 'string' },
                  reliable: { type: 'boolean' }
                }
              }
            }
          },
          description: 'Networking info (for get_networking_info).'
        },
        error: { type: 'string' }
      }
    }
  },

  // ============================================================================
  // 33. GAME FRAMEWORK MANAGER (Phase 21)
  // ============================================================================
  {
    name: 'manage_game_framework',
    description: 'Create GameMode, GameState, PlayerController, PlayerState Blueprints. Configure match flow, teams, scoring, and spawning.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            // Core Classes (6)
            'create_game_mode', 'create_game_state', 'create_player_controller',
            'create_player_state', 'create_game_instance', 'create_hud_class',
            // Game Mode Configuration (5)
            'set_default_pawn_class', 'set_player_controller_class',
            'set_game_state_class', 'set_player_state_class', 'configure_game_rules',
            // Match Flow (5)
            'setup_match_states', 'configure_round_system', 'configure_team_system',
            'configure_scoring_system', 'configure_spawn_system',
            // Player Management (3)
            'configure_player_start', 'set_respawn_rules', 'configure_spectating',
            // Utility (1)
            'get_game_framework_info'
          ],
          description: 'Game framework action to perform.'
        },

        // Asset identification
        name: { type: 'string', description: 'Name of new blueprint.' },
        path: { type: 'string', description: 'Directory to create blueprint in (e.g., /Game/Blueprints).' },
        gameModeBlueprint: { type: 'string', description: 'Path to GameMode blueprint to configure.' },
        blueprintPath: { type: 'string', description: 'Path to blueprint (alternative to gameModeBlueprint).' },
        levelPath: { type: 'string', description: 'Path to level for info queries.' },

        // Class references
        parentClass: { type: 'string', description: 'Parent class for blueprint creation.' },
        pawnClass: { type: 'string', description: 'Pawn class to use.' },
        defaultPawnClass: { type: 'string', description: 'Default pawn class for GameMode.' },
        playerControllerClass: { type: 'string', description: 'PlayerController class path.' },
        gameStateClass: { type: 'string', description: 'GameState class path.' },
        playerStateClass: { type: 'string', description: 'PlayerState class path.' },
        spectatorClass: { type: 'string', description: 'Spectator pawn class.' },
        hudClass: { type: 'string', description: 'HUD class path.' },

        // Game rules
        timeLimit: { type: 'number', description: 'Match time limit in seconds (0 = unlimited).' },
        scoreLimit: { type: 'number', description: 'Score limit to win (0 = unlimited).' },
        bDelayedStart: { type: 'boolean', description: 'Whether to delay match start.' },
        startPlayersNeeded: { type: 'number', description: 'Minimum players needed to start.' },

        // Match states
        states: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              name: { type: 'string', enum: ['waiting', 'warmup', 'in_progress', 'post_match', 'custom'] },
              duration: { type: 'number', description: 'Duration in seconds (0 = manual transition).' },
              customName: { type: 'string', description: 'Custom state name if name is "custom".' }
            }
          },
          description: 'Match state definitions.'
        },

        // Round system
        numRounds: { type: 'number', description: 'Number of rounds (0 = unlimited).' },
        roundTime: { type: 'number', description: 'Time per round in seconds.' },
        intermissionTime: { type: 'number', description: 'Time between rounds in seconds.' },

        // Team system
        numTeams: { type: 'number', description: 'Number of teams (0 = FFA).' },
        teamSize: { type: 'number', description: 'Maximum players per team.' },
        autoBalance: { type: 'boolean', description: 'Enable automatic team balancing.' },
        friendlyFire: { type: 'boolean', description: 'Enable friendly fire damage.' },
        teamIndex: { type: 'number', description: 'Team index for PlayerStart.' },

        // Scoring
        scorePerKill: { type: 'number', description: 'Points awarded per kill.' },
        scorePerObjective: { type: 'number', description: 'Points awarded per objective.' },
        scorePerAssist: { type: 'number', description: 'Points awarded per assist.' },

        // Spawn system
        spawnSelectionMethod: {
          type: 'string',
          enum: ['Random', 'RoundRobin', 'FarthestFromEnemies'],
          description: 'How to select spawn points.'
        },
        respawnDelay: { type: 'number', description: 'Delay before respawn in seconds.' },
        respawnLocation: {
          type: 'string',
          enum: ['PlayerStart', 'LastDeath', 'TeamBase'],
          description: 'Where players respawn.'
        },
        respawnConditions: {
          type: 'array',
          items: { type: 'string' },
          description: 'Conditions for respawn (e.g., "RoundEnd", "Manual").'
        },
        usePlayerStarts: { type: 'boolean', description: 'Use PlayerStart actors.' },

        // PlayerStart configuration
        location: {
          type: 'object',
          properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } },
          description: 'Spawn location.'
        },
        rotation: {
          type: 'object',
          properties: { pitch: { type: 'number' }, yaw: { type: 'number' }, roll: { type: 'number' } },
          description: 'Spawn rotation.'
        },
        bPlayerOnly: { type: 'boolean', description: 'Restrict to players only.' },

        // Spectating
        allowSpectating: { type: 'boolean', description: 'Allow spectator mode.' },
        spectatorViewMode: {
          type: 'string',
          enum: ['FreeCam', 'ThirdPerson', 'FirstPerson', 'DeathCam'],
          description: 'Spectator view mode.'
        },

        // Options
        save: { type: 'boolean', description: 'Save asset after operation.' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        message: { type: 'string' },
        blueprintPath: { type: 'string', description: 'Path to created/modified blueprint.' },
        gameFrameworkInfo: {
          type: 'object',
          properties: {
            gameModeClass: { type: 'string' },
            gameStateClass: { type: 'string' },
            playerControllerClass: { type: 'string' },
            playerStateClass: { type: 'string' },
            defaultPawnClass: { type: 'string' },
            hudClass: { type: 'string' },
            spectatorClass: { type: 'string' },
            matchState: { type: 'string' },
            numTeams: { type: 'number' },
            timeLimit: { type: 'number' },
            scoreLimit: { type: 'number' }
          },
          description: 'Game framework information (for get_game_framework_info).'
        },
        error: { type: 'string' }
      }
    }
  },

  // ============================================================================
  // 34. SESSIONS & LOCAL MULTIPLAYER (Phase 22)
  // ============================================================================
  {
    name: 'manage_sessions',
    description: 'Configure local multiplayer: split-screen layouts, LAN hosting/joining, voice chat channels, and push-to-talk.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            // Session Management (2)
            'configure_local_session_settings', 'configure_session_interface',
            // Local Multiplayer (4)
            'configure_split_screen', 'set_split_screen_type', 'add_local_player', 'remove_local_player',
            // LAN (3)
            'configure_lan_play', 'host_lan_server', 'join_lan_server',
            // Voice Chat (6)
            'enable_voice_chat', 'configure_voice_settings', 'set_voice_channel',
            'mute_player', 'set_voice_attenuation', 'configure_push_to_talk',
            // Utility (1)
            'get_sessions_info'
          ],
          description: 'Sessions action to perform.'
        },

        // Session identification
        sessionName: { type: 'string', description: 'Name of the session.' },
        sessionId: { type: 'string', description: 'Session ID for existing sessions.' },

        // Local session settings
        maxPlayers: { type: 'number', description: 'Maximum players allowed in session.' },
        bIsLANMatch: { type: 'boolean', description: 'Whether this is a LAN match.' },
        bAllowJoinInProgress: { type: 'boolean', description: 'Allow joining games in progress.' },
        bAllowInvites: { type: 'boolean', description: 'Allow player invites.' },
        bUsesPresence: { type: 'boolean', description: 'Use presence for session discovery.' },
        bUseLobbiesIfAvailable: { type: 'boolean', description: 'Use lobby system if available.' },
        bShouldAdvertise: { type: 'boolean', description: 'Advertise session publicly.' },

        // Session interface
        interfaceType: {
          type: 'string',
          enum: ['Default', 'LAN', 'Null'],
          description: 'Type of session interface to use.'
        },

        // Split-screen configuration
        enabled: { type: 'boolean', description: 'Enable/disable feature.' },
        splitScreenType: {
          type: 'string',
          enum: ['None', 'TwoPlayer_Horizontal', 'TwoPlayer_Vertical', 'ThreePlayer_FavorTop', 'ThreePlayer_FavorBottom', 'FourPlayer_Grid'],
          description: 'Split-screen layout type.'
        },

        // Local player management
        playerIndex: { type: 'number', description: 'Index of local player.' },
        controllerId: { type: 'number', description: 'Controller ID to assign.' },

        // LAN settings
        serverAddress: { type: 'string', description: 'Server IP address to connect to.' },
        serverPort: { type: 'number', description: 'Server port number.' },
        serverPassword: { type: 'string', description: 'Server password for protected games.' },
        serverName: { type: 'string', description: 'Display name for the server.' },
        mapName: { type: 'string', description: 'Map to load for hosting.' },
        travelOptions: { type: 'string', description: 'Travel URL options string.' },

        // Voice chat settings
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
        channelName: { type: 'string', description: 'Voice channel name.' },
        channelType: {
          type: 'string',
          enum: ['Team', 'Global', 'Proximity', 'Party'],
          description: 'Voice channel type.'
        },

        // Player targeting for voice operations
        playerName: { type: 'string', description: 'Player name for voice operations.' },
        targetPlayerId: { type: 'string', description: 'Target player ID.' },
        muted: { type: 'boolean', description: 'Mute state for player.' },

        // Voice attenuation
        attenuationRadius: { type: 'number', description: 'Radius for voice attenuation (Proximity chat).' },
        attenuationFalloff: { type: 'number', description: 'Falloff rate for voice attenuation.' },

        // Push-to-talk
        pushToTalkEnabled: { type: 'boolean', description: 'Enable push-to-talk mode.' },
        pushToTalkKey: { type: 'string', description: 'Key binding for push-to-talk.' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        message: { type: 'string' },
        sessionId: { type: 'string', description: 'ID of created/modified session.' },
        sessionName: { type: 'string', description: 'Name of created session.' },
        playerIndex: { type: 'number', description: 'Index of added local player.' },
        serverAddress: { type: 'string', description: 'Address of hosted/joined server.' },
        channelName: { type: 'string', description: 'Voice channel name.' },
        sessionsInfo: {
          type: 'object',
          properties: {
            currentSessionName: { type: 'string' },
            isLANMatch: { type: 'boolean' },
            maxPlayers: { type: 'number' },
            currentPlayers: { type: 'number' },
            localPlayerCount: { type: 'number' },
            splitScreenEnabled: { type: 'boolean' },
            splitScreenType: { type: 'string' },
            voiceChatEnabled: { type: 'boolean' },
            activeVoiceChannels: {
              type: 'array',
              items: { type: 'string' }
            },
            isHosting: { type: 'boolean' },
            connectedServerAddress: { type: 'string' }
          },
          description: 'Sessions information (for get_sessions_info).'
        },
        error: { type: 'string' }
      }
    }
  },

  // ============================================================================
  // 35. LEVEL STRUCTURE (Phase 23)
  // ============================================================================
  {
    name: 'manage_level_structure',
    description: 'Create levels and sublevels. Configure World Partition, streaming, data layers, HLOD, and level instances.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            // Levels (5)
            'create_level', 'create_sublevel', 'configure_level_streaming',
            'set_streaming_distance', 'configure_level_bounds',
            // World Partition (6)
            'enable_world_partition', 'configure_grid_size', 'create_data_layer',
            'assign_actor_to_data_layer', 'configure_hlod_layer', 'create_minimap_volume',
            // Level Blueprint (3)
            'open_level_blueprint', 'add_level_blueprint_node', 'connect_level_blueprint_nodes',
            // Level Instances (2)
            'create_level_instance', 'create_packed_level_actor',
            // Utility (1)
            'get_level_structure_info'
          ],
          description: 'Level structure action to perform.'
        },

        // Level identification
        levelName: { type: 'string', description: 'Name of the level.' },
        levelPath: { type: 'string', description: 'Path to the level asset.' },
        parentLevel: { type: 'string', description: 'Parent level for sublevel creation.' },

        // Level creation
        templateLevel: { type: 'string', description: 'Template level to copy from.' },
        bCreateWorldPartition: { type: 'boolean', description: 'Create with World Partition enabled.' },

        // Sublevel configuration
        sublevelName: { type: 'string', description: 'Name of the sublevel.' },
        sublevelPath: { type: 'string', description: 'Path to the sublevel asset.' },

        // Level streaming
        streamingMethod: {
          type: 'string',
          enum: ['Blueprint', 'AlwaysLoaded', 'Disabled'],
          description: 'Level streaming method.'
        },
        bShouldBeVisible: { type: 'boolean', description: 'Level should be visible when loaded.' },
        bShouldBlockOnLoad: { type: 'boolean', description: 'Block game until level is loaded.' },
        bDisableDistanceStreaming: { type: 'boolean', description: 'Disable distance-based streaming.' },

        // Streaming distance (creates ALevelStreamingVolume)
        streamingDistance: { type: 'number', description: 'Distance/radius for streaming volume (creates ALevelStreamingVolume).' },
        streamingUsage: {
          type: 'string',
          enum: ['Loading', 'LoadingAndVisibility', 'VisibilityBlockingOnLoad', 'BlockingOnLoad', 'LoadingNotVisible'],
          description: 'Streaming volume usage mode (default: LoadingAndVisibility).'
        },
        createVolume: { type: 'boolean', description: 'Create a streaming volume (true) or just report existing volumes (false). Default: true.' },

        // Level bounds
        boundsOrigin: {
          type: 'object',
          properties: {
            x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' }
          },
          description: 'Origin of level bounds.'
        },
        boundsExtent: {
          type: 'object',
          properties: {
            x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' }
          },
          description: 'Extent of level bounds.'
        },
        bAutoCalculateBounds: { type: 'boolean', description: 'Auto-calculate bounds from content.' },

        // World Partition
        bEnableWorldPartition: { type: 'boolean', description: 'Enable World Partition for level.' },
        gridCellSize: { type: 'number', description: 'World Partition grid cell size.' },
        loadingRange: { type: 'number', description: 'Loading range for grid cells.' },

        // Data layers
        dataLayerName: { type: 'string', description: 'Name of the data layer.' },
        dataLayerLabel: { type: 'string', description: 'Display label for the data layer.' },
        bIsInitiallyVisible: { type: 'boolean', description: 'Data layer initially visible.' },
        bIsInitiallyLoaded: { type: 'boolean', description: 'Data layer initially loaded.' },
        dataLayerType: {
          type: 'string',
          enum: ['Runtime', 'Editor'],
          description: 'Type of data layer.'
        },

        // Actor assignment to data layer
        actorName: { type: 'string', description: 'Name of actor to assign to data layer.' },
        actorPath: { type: 'string', description: 'Path to the actor.' },

        // HLOD configuration
        hlodLayerName: { type: 'string', description: 'Name of the HLOD layer.' },
        hlodLayerPath: { type: 'string', description: 'Path to the HLOD layer asset.' },
        bIsSpatiallyLoaded: { type: 'boolean', description: 'HLOD is spatially loaded.' },
        cellSize: { type: 'number', description: 'HLOD cell size.' },
        loadingDistance: { type: 'number', description: 'HLOD loading distance.' },

        // Minimap volume
        volumeName: { type: 'string', description: 'Name of the minimap volume.' },
        volumeLocation: {
          type: 'object',
          properties: {
            x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' }
          },
          description: 'Location of the volume.'
        },
        volumeExtent: {
          type: 'object',
          properties: {
            x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' }
          },
          description: 'Extent of the volume.'
        },

        // Level Blueprint
        nodeClass: { type: 'string', description: 'Class of node to add to Level Blueprint.' },
        nodePosition: {
          type: 'object',
          properties: {
            x: { type: 'number' }, y: { type: 'number' }
          },
          description: 'Position of node in graph.'
        },
        nodeName: { type: 'string', description: 'Name of the node.' },

        // Node connections
        sourceNodeName: { type: 'string', description: 'Source node for connection.' },
        sourcePinName: { type: 'string', description: 'Source pin name.' },
        targetNodeName: { type: 'string', description: 'Target node for connection.' },
        targetPinName: { type: 'string', description: 'Target pin name.' },

        // Level instances
        levelInstanceName: { type: 'string', description: 'Name of the level instance.' },
        levelAssetPath: { type: 'string', description: 'Path to the level asset for instancing.' },
        instanceLocation: {
          type: 'object',
          properties: {
            x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' }
          },
          description: 'Location of the level instance.'
        },
        instanceRotation: {
          type: 'object',
          properties: {
            pitch: { type: 'number' }, yaw: { type: 'number' }, roll: { type: 'number' }
          },
          description: 'Rotation of the level instance.'
        },
        instanceScale: {
          type: 'object',
          properties: {
            x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' }
          },
          description: 'Scale of the level instance.'
        },

        // Packed level actor
        packedLevelName: { type: 'string', description: 'Name for the packed level actor.' },
        bPackBlueprints: { type: 'boolean', description: 'Include blueprints in packed level.' },
        bPackStaticMeshes: { type: 'boolean', description: 'Include static meshes in packed level.' },

        // Save option
        save: { type: 'boolean', description: 'Save after operation.' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        message: { type: 'string' },
        levelPath: { type: 'string', description: 'Path to created/modified level.' },
        sublevelPath: { type: 'string', description: 'Path to created sublevel.' },
        dataLayerName: { type: 'string', description: 'Name of created data layer.' },
        hlodLayerPath: { type: 'string', description: 'Path to created HLOD layer.' },
        nodeName: { type: 'string', description: 'Name of created blueprint node.' },
        levelInstanceName: { type: 'string', description: 'Name of created level instance.' },
        levelStructureInfo: {
          type: 'object',
          properties: {
            currentLevel: { type: 'string' },
            sublevelCount: { type: 'number' },
            sublevels: {
              type: 'array',
              items: { type: 'string' }
            },
            worldPartitionEnabled: { type: 'boolean' },
            gridCellSize: { type: 'number' },
            dataLayers: {
              type: 'array',
              items: { type: 'string' }
            },
            hlodLayers: {
              type: 'array',
              items: { type: 'string' }
            },
            levelInstances: {
              type: 'array',
              items: { type: 'string' }
            }
          },
          description: 'Level structure information (for get_level_structure_info).'
        },
        error: { type: 'string' }
      }
    }
  },

  // 38. VOLUMES & ZONES (Phase 24)
  {
    name: 'manage_volumes',
    description: 'Create volumes: trigger, blocking, kill zone, physics, audio/reverb, nav mesh bounds, and rendering (cull distance, lightmass).',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            // Trigger Volumes
            'create_trigger_volume', 'create_trigger_box', 'create_trigger_sphere', 'create_trigger_capsule',
            // Gameplay Volumes
            'create_blocking_volume', 'create_kill_z_volume', 'create_pain_causing_volume', 'create_physics_volume',
            // Audio Volumes
            'create_audio_volume', 'create_reverb_volume',
            // Rendering Volumes
            'create_cull_distance_volume', 'create_precomputed_visibility_volume', 'create_lightmass_importance_volume',
            // Navigation Volumes
            'create_nav_mesh_bounds_volume', 'create_nav_modifier_volume', 'create_camera_blocking_volume',
            // Configuration
            'set_volume_extent', 'set_volume_properties',
            // Utility
            'get_volumes_info'
          ],
          description: 'Volume action to perform'
        },

        // Volume identification
        volumeName: { type: 'string', description: 'Name for the volume actor.' },
        volumePath: { type: 'string', description: 'Path to existing volume actor.' },

        // Location and transform
        location: {
          type: 'object',
          properties: {
            x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' }
          },
          description: 'World location for the volume.'
        },
        rotation: {
          type: 'object',
          properties: {
            pitch: { type: 'number' }, yaw: { type: 'number' }, roll: { type: 'number' }
          },
          description: 'Rotation of the volume.'
        },

        // Volume extent/size
        extent: {
          type: 'object',
          properties: {
            x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' }
          },
          description: 'Extent (half-size) of the volume in each axis.'
        },

        // Trigger shape parameters
        sphereRadius: { type: 'number', description: 'Radius for sphere trigger volumes.' },
        capsuleRadius: { type: 'number', description: 'Radius for capsule trigger volumes.' },
        capsuleHalfHeight: { type: 'number', description: 'Half-height for capsule trigger volumes.' },
        boxExtent: {
          type: 'object',
          properties: {
            x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' }
          },
          description: 'Extent for box trigger volumes.'
        },

        // Pain Causing Volume specific
        bPainCausing: { type: 'boolean', description: 'Whether the volume causes pain/damage.' },
        damagePerSec: { type: 'number', description: 'Damage per second for pain volumes.' },
        damageType: { type: 'string', description: 'Damage type class path for pain volumes.' },

        // Physics Volume specific
        bWaterVolume: { type: 'boolean', description: 'Whether this is a water volume.' },
        fluidFriction: { type: 'number', description: 'Fluid friction for physics volumes.' },
        terminalVelocity: { type: 'number', description: 'Terminal velocity in the volume.' },
        priority: { type: 'number', description: 'Priority when volumes overlap.' },

        // Audio Volume specific
        bEnabled: { type: 'boolean', description: 'Whether the audio volume is enabled.' },

        // Reverb Volume specific
        reverbEffect: { type: 'string', description: 'Reverb effect asset path.' },
        reverbVolume: { type: 'number', description: 'Volume level for reverb (0.0-1.0).' },
        fadeTime: { type: 'number', description: 'Fade time for reverb transitions.' },

        // Cull Distance Volume specific
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

        // Nav Modifier Volume specific
        areaClass: { type: 'string', description: 'Navigation area class path.' },
        bDynamicModifier: { type: 'boolean', description: 'Whether nav modifier updates dynamically.' },

        // Post Process Volume (basic)
        bUnbound: { type: 'boolean', description: 'Whether post process volume affects entire world.' },
        blendRadius: { type: 'number', description: 'Blend radius for post process volume.' },
        blendWeight: { type: 'number', description: 'Blend weight (0.0-1.0) for post process.' },

        // General volume properties
        properties: {
          type: 'object',
          description: 'Additional volume-specific properties as key-value pairs.'
        },

        // Query parameters
        filter: { type: 'string', description: 'Filter string for get_volumes_info.' },
        volumeType: { type: 'string', description: 'Type filter for get_volumes_info (e.g., "Trigger", "Physics").' },

        // Save option
        save: { type: 'boolean', description: 'Save the level after modification.' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        message: { type: 'string' },
        volumeName: { type: 'string', description: 'Name of created/modified volume.' },
        volumePath: { type: 'string', description: 'Path to created/modified volume.' },
        volumeClass: { type: 'string', description: 'Class of the volume.' },
        volumesInfo: {
          type: 'object',
          properties: {
            totalCount: { type: 'number' },
            volumes: {
              type: 'array',
              items: {
                type: 'object',
                properties: {
                  name: { type: 'string' },
                  class: { type: 'string' },
                  location: {
                    type: 'object',
                    properties: {
                      x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' }
                    }
                  },
                  extent: {
                    type: 'object',
                    properties: {
                      x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' }
                    }
                  }
                }
              }
            }
          },
          description: 'Volume information (for get_volumes_info).'
        },
        error: { type: 'string' }
      }
    }
  },

  // Phase 25: Navigation System
  {
    name: 'manage_navigation',
    description: 'Configure NavMesh: agent radius/height, cell size, rebuild. Create nav modifiers, nav links, and smart links for AI pathfinding.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            // NavMesh
            'configure_nav_mesh_settings', 'set_nav_agent_properties', 'rebuild_navigation',
            // Nav Modifiers
            'create_nav_modifier_component', 'set_nav_area_class', 'configure_nav_area_cost',
            // Nav Links
            'create_nav_link_proxy', 'configure_nav_link', 'set_nav_link_type',
            'create_smart_link', 'configure_smart_link_behavior',
            // Utility
            'get_navigation_info'
          ],
          description: 'Navigation action to perform'
        },

        // NavMesh identification
        navMeshPath: { type: 'string', description: 'Path to NavMesh data asset.' },
        actorName: { type: 'string', description: 'Name of nav link proxy or actor.' },
        actorPath: { type: 'string', description: 'Path to existing actor.' },
        blueprintPath: { type: 'string', description: 'Path to Blueprint for component addition.' },

        // Nav agent properties (ARecastNavMesh)
        agentRadius: { type: 'number', description: 'Navigation agent radius (default: 35).' },
        agentHeight: { type: 'number', description: 'Navigation agent height (default: 144).' },
        agentStepHeight: { type: 'number', description: 'Maximum step height agent can climb (default: 35).' },
        agentMaxSlope: { type: 'number', description: 'Maximum slope angle in degrees (default: 44).' },

        // NavMesh generation settings (FNavMeshResolutionParam)
        cellSize: { type: 'number', description: 'NavMesh cell size (default: 19).' },
        cellHeight: { type: 'number', description: 'NavMesh cell height (default: 10).' },
        tileSizeUU: { type: 'number', description: 'NavMesh tile size in UU (default: 1000).' },
        minRegionArea: { type: 'number', description: 'Minimum region area to keep.' },
        mergeRegionSize: { type: 'number', description: 'Region merge threshold.' },
        maxSimplificationError: { type: 'number', description: 'Edge simplification error.' },

        // Nav modifier component (UNavModifierComponent)
        componentName: { type: 'string', description: 'Name for nav modifier component.' },
        areaClass: { type: 'string', description: 'Nav area class path (e.g., /Script/NavigationSystem.NavArea_Obstacle).' },
        areaClassToReplace: { type: 'string', description: 'Area class to replace (optional modifier behavior).' },
        failsafeExtent: {
          type: 'object',
          properties: {
            x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' }
          },
          description: 'Failsafe extent for nav modifier when actor has no collision.'
        },
        bIncludeAgentHeight: { type: 'boolean', description: 'Expand lower bounds by agent height.' },

        // Nav area cost configuration
        areaCost: { type: 'number', description: 'Pathfinding cost multiplier for area (1.0 = normal).' },
        fixedAreaEnteringCost: { type: 'number', description: 'Fixed cost added when entering the area.' },

        // Nav link configuration (ANavLinkProxy, FNavigationLink)
        linkName: { type: 'string', description: 'Name for navigation link.' },
        startPoint: {
          type: 'object',
          properties: {
            x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' }
          },
          description: 'Start point of navigation link (relative to actor).'
        },
        endPoint: {
          type: 'object',
          properties: {
            x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' }
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

        // Smart link configuration (UNavLinkCustomComponent)
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

        // Obstacle configuration
        bCreateBoxObstacle: { type: 'boolean', description: 'Add box obstacle during nav generation.' },
        obstacleOffset: {
          type: 'object',
          properties: {
            x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' }
          },
          description: 'Offset of simple box obstacle.'
        },
        obstacleExtent: {
          type: 'object',
          properties: {
            x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' }
          },
          description: 'Extent of simple box obstacle.'
        },
        obstacleAreaClass: { type: 'string', description: 'Area class for box obstacle.' },

        // Location and transform
        location: {
          type: 'object',
          properties: {
            x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' }
          },
          description: 'World location for nav link proxy.'
        },
        rotation: {
          type: 'object',
          properties: {
            pitch: { type: 'number' }, yaw: { type: 'number' }, roll: { type: 'number' }
          },
          description: 'Rotation for nav link proxy.'
        },

        // Query parameters
        filter: { type: 'string', description: 'Filter for get_navigation_info query.' },

        // Save option
        save: { type: 'boolean', description: 'Save the level/asset after modification.' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        message: { type: 'string' },
        actorName: { type: 'string', description: 'Name of created/modified actor.' },
        componentName: { type: 'string', description: 'Name of created component.' },
        navMeshInfo: {
          type: 'object',
          properties: {
            agentRadius: { type: 'number' },
            agentHeight: { type: 'number' },
            agentStepHeight: { type: 'number' },
            agentMaxSlope: { type: 'number' },
            cellSize: { type: 'number' },
            cellHeight: { type: 'number' },
            tileSizeUU: { type: 'number' },
            tileCount: { type: 'number' },
            boundsVolumes: { type: 'number' },
            navLinkCount: { type: 'number' }
          },
          description: 'Navigation system information.'
        },
        error: { type: 'string' }
      }
    }
  },

  // Phase 26: Spline System
  {
    name: 'manage_splines',
    description: 'Create spline actors, add/modify points, attach meshes along splines. Use templates for roads, rivers, fences, cables.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            // Spline Creation
            'create_spline_actor', 'add_spline_point', 'remove_spline_point',
            'set_spline_point_position', 'set_spline_point_tangents',
            'set_spline_point_rotation', 'set_spline_point_scale', 'set_spline_type',
            // Spline Mesh
            'create_spline_mesh_component', 'set_spline_mesh_asset',
            'configure_spline_mesh_axis', 'set_spline_mesh_material',
            // Spline Mesh Array
            'scatter_meshes_along_spline', 'configure_mesh_spacing', 'configure_mesh_randomization',
            // Quick Templates
            'create_road_spline', 'create_river_spline', 'create_fence_spline',
            'create_wall_spline', 'create_cable_spline', 'create_pipe_spline',
            // Utility
            'get_splines_info'
          ],
          description: 'Spline action to perform'
        },

        // Actor/Spline identification
        actorName: { type: 'string', description: 'Name for spline actor.' },
        actorPath: { type: 'string', description: 'Path to existing spline actor.' },
        splineName: { type: 'string', description: 'Name of spline component.' },
        componentName: { type: 'string', description: 'Name for created component.' },
        blueprintPath: { type: 'string', description: 'Path to Blueprint for component addition.' },

        // Location and transform
        location: {
          type: 'object',
          properties: {
            x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' }
          },
          description: 'Location for spline actor.'
        },
        rotation: {
          type: 'object',
          properties: {
            pitch: { type: 'number' }, yaw: { type: 'number' }, roll: { type: 'number' }
          },
          description: 'Rotation for spline actor.'
        },
        scale: {
          type: 'object',
          properties: {
            x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' }
          },
          description: 'Scale for spline actor.'
        },

        // Spline point manipulation
        pointIndex: { type: 'number', description: 'Index of spline point to modify.' },
        position: {
          type: 'object',
          properties: {
            x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' }
          },
          description: 'Position for spline point.'
        },
        arriveTangent: {
          type: 'object',
          properties: {
            x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' }
          },
          description: 'Arrive tangent for spline point (incoming direction).'
        },
        leaveTangent: {
          type: 'object',
          properties: {
            x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' }
          },
          description: 'Leave tangent for spline point (outgoing direction).'
        },
        tangent: {
          type: 'object',
          properties: {
            x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' }
          },
          description: 'Unified tangent (sets both arrive and leave).'
        },
        pointRotation: {
          type: 'object',
          properties: {
            pitch: { type: 'number' }, yaw: { type: 'number' }, roll: { type: 'number' }
          },
          description: 'Rotation at spline point.'
        },
        pointScale: {
          type: 'object',
          properties: {
            x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' }
          },
          description: 'Scale at spline point.'
        },
        coordinateSpace: {
          type: 'string',
          enum: ['Local', 'World'],
          description: 'Coordinate space for position/tangent values (default: Local).'
        },

        // Spline type configuration
        splineType: {
          type: 'string',
          enum: ['Linear', 'Curve', 'Constant', 'CurveClamped', 'CurveCustomTangent'],
          description: 'Type of spline interpolation.'
        },
        bClosedLoop: { type: 'boolean', description: 'Whether spline forms a closed loop.' },
        bUpdateSpline: { type: 'boolean', description: 'Update spline after modification (default: true).' },

        // Spline mesh configuration
        meshPath: { type: 'string', description: 'Path to static mesh asset for spline mesh.' },
        materialPath: { type: 'string', description: 'Path to material asset.' },
        forwardAxis: {
          type: 'string',
          enum: ['X', 'Y', 'Z'],
          description: 'Forward axis for spline mesh deformation.'
        },
        startPos: {
          type: 'object',
          properties: {
            x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' }
          },
          description: 'Start position for spline mesh segment.'
        },
        startTangent: {
          type: 'object',
          properties: {
            x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' }
          },
          description: 'Start tangent for spline mesh segment.'
        },
        endPos: {
          type: 'object',
          properties: {
            x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' }
          },
          description: 'End position for spline mesh segment.'
        },
        endTangent: {
          type: 'object',
          properties: {
            x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' }
          },
          description: 'End tangent for spline mesh segment.'
        },
        startScale: {
          type: 'object',
          properties: { x: { type: 'number' }, y: { type: 'number' } },
          description: 'X/Y scale at spline mesh start.'
        },
        endScale: {
          type: 'object',
          properties: { x: { type: 'number' }, y: { type: 'number' } },
          description: 'X/Y scale at spline mesh end.'
        },
        startRoll: { type: 'number', description: 'Roll angle at spline mesh start (radians).' },
        endRoll: { type: 'number', description: 'Roll angle at spline mesh end (radians).' },
        bSmoothInterpRollScale: { type: 'boolean', description: 'Use smooth interpolation for roll/scale.' },

        // Mesh scattering configuration
        spacing: { type: 'number', description: 'Distance between scattered meshes.' },
        startOffset: { type: 'number', description: 'Offset from spline start for first mesh.' },
        endOffset: { type: 'number', description: 'Offset from spline end for last mesh.' },
        bAlignToSpline: { type: 'boolean', description: 'Align scattered meshes to spline direction.' },
        bRandomizeRotation: { type: 'boolean', description: 'Apply random rotation to scattered meshes.' },
        rotationRandomRange: {
          type: 'object',
          properties: {
            pitch: { type: 'number' }, yaw: { type: 'number' }, roll: { type: 'number' }
          },
          description: 'Random rotation range (degrees).'
        },
        bRandomizeScale: { type: 'boolean', description: 'Apply random scale to scattered meshes.' },
        scaleMin: { type: 'number', description: 'Minimum random scale multiplier.' },
        scaleMax: { type: 'number', description: 'Maximum random scale multiplier.' },
        randomSeed: { type: 'number', description: 'Seed for randomization (for reproducible results).' },

        // Template-specific options
        templateType: {
          type: 'string',
          enum: ['road', 'river', 'fence', 'wall', 'cable', 'pipe'],
          description: 'Type of spline template to create.'
        },
        width: { type: 'number', description: 'Width for road/river templates.' },
        segmentLength: { type: 'number', description: 'Length of mesh segments for deformation.' },
        postSpacing: { type: 'number', description: 'Spacing between fence posts.' },
        railHeight: { type: 'number', description: 'Height of fence rails.' },
        pipeRadius: { type: 'number', description: 'Radius for pipe template.' },
        cableSlack: { type: 'number', description: 'Slack/sag amount for cable template.' },

        // Points array for batch operations
        points: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              position: {
                type: 'object',
                properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } }
              },
              arriveTangent: {
                type: 'object',
                properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } }
              },
              leaveTangent: {
                type: 'object',
                properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } }
              },
              rotation: {
                type: 'object',
                properties: { pitch: { type: 'number' }, yaw: { type: 'number' }, roll: { type: 'number' } }
              },
              scale: {
                type: 'object',
                properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } }
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

        // Query parameters
        filter: { type: 'string', description: 'Filter for get_splines_info query.' },

        // Save option
        save: { type: 'boolean', description: 'Save the level/asset after modification.' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        message: { type: 'string' },
        actorName: { type: 'string', description: 'Name of created/modified spline actor.' },
        componentName: { type: 'string', description: 'Name of created spline component.' },
        pointCount: { type: 'number', description: 'Number of points in spline.' },
        splineLength: { type: 'number', description: 'Total length of spline in units.' },
        bClosedLoop: { type: 'boolean', description: 'Whether spline is a closed loop.' },
        splineInfo: {
          type: 'object',
          properties: {
            pointCount: { type: 'number' },
            splineLength: { type: 'number' },
            bClosedLoop: { type: 'boolean' },
            points: {
              type: 'array',
              items: {
                type: 'object',
                properties: {
                  index: { type: 'number' },
                  position: { type: 'object' },
                  arriveTangent: { type: 'object' },
                  leaveTangent: { type: 'object' },
                  rotation: { type: 'object' },
                  scale: { type: 'object' },
                  type: { type: 'string' }
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
              name: { type: 'string' },
              meshPath: { type: 'string' },
              forwardAxis: { type: 'string' }
            }
          },
          description: 'List of spline mesh components.'
        },
        scatteredMeshes: { type: 'number', description: 'Number of meshes scattered along spline.' },
        error: { type: 'string' }
      }
    }
  }
];

