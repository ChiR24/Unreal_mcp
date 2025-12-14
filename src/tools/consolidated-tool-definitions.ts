import { commonSchemas } from './tool-definition-utils.js';
// Force rebuild timestamp update

export const consolidatedToolDefinitions = [
  // 1. ASSET MANAGER
  {
    name: 'manage_asset',
    description: `Comprehensive asset management suite. Handles import/export, basic file operations, dependency analysis, source control, and specialized graph editing (Materials, Behavior Trees).

Use it when you need to:
- manage asset lifecycle (import, duplicate, rename, delete).
- organize project structure (folders, redirects).
- analyze dependencies or fix reference issues.
- edit Material graphs or Behavior Trees.
- generate basic assets like Materials, Textures, or Blueprints (via asset creation).

Supported actions:
- Core: list, import, duplicate, rename, move, delete, delete_assets, create_folder, search_assets.
- Utils: get_dependencies, get_source_control_state, analyze_graph, create_thumbnail, set_tags, get_metadata, set_metadata, validate, fixup_redirectors, find_by_tag, generate_report.
- Creation: create_material, create_material_instance, create_render_target.
- Rendering: nanite_rebuild_mesh.
- Material Graph: add_material_node, connect_material_pins, remove_material_node, break_material_connections, get_material_node_details.
- Behavior Tree: add_bt_node, connect_bt_nodes, remove_bt_node, break_bt_connections, set_bt_node_properties.`,
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
        fromNodeId: { type: 'string' },
        fromPin: { type: 'string' },
        toNodeId: { type: 'string' },
        toPin: { type: 'string' },
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
    description: `Blueprint authoring and editing tool.

Use it when you need to:
- create new Blueprints.
- inspect existing Blueprints (get_blueprint).
- modify the component hierarchy (SCS).
- edit the Blueprint graph (nodes, pins, properties).

Supported actions:
- Lifecycle: create, get_blueprint.
- Components (SCS): add_component, set_default, modify_scs, get_scs, add_scs_component, remove_scs_component, reparent_scs_component, set_scs_transform, set_scs_property.
- Helpers: ensure_exists, probe_handle, add_variable, add_function, add_event, add_construction_script, set_variable_metadata, set_metadata.
- Graph: create_node, delete_node, connect_pins, break_pin_links, set_node_property, create_reroute_node, get_node_details, get_graph_details, get_pin_details.`,
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
    description: `Viewport actor manipulation.

Use it when you need to:
- spawn, destroy, or duplicate actors.
- move/rotate/scale actors.
- modify actor components or tags.
- apply physics forces.

Supported actions: spawn, spawn_blueprint, delete, delete_by_tag, duplicate, apply_force, set_transform, get_transform, set_visibility, add_component, set_component_properties, get_components, add_tag, remove_tag, find_by_tag, find_by_name, list, set_blueprint_variables, create_snapshot, attach, detach.`,
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
    description: `Editor session control.

Use it when you need to:
- control PIE (Play In Editor).
- move the viewport camera.
- execute console commands (legacy).
- take screenshots/bookmarks.
- simulate input (UI).

Supported actions: play, stop, stop_pie, pause, resume, set_game_speed, eject, possess, set_camera, set_camera_position, set_camera_fov, set_view_mode, set_viewport_resolution, console_command, screenshot, step_frame, start_recording, stop_recording, create_bookmark, jump_to_bookmark, set_preferences, set_viewport_realtime, open_asset, simulate_input.`,
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
    description: `Level and World Partition management.

Use it when you need to:
- load/save levels.
- manage streaming levels.
- build lighting.
- load world partition cells or toggle data layers.

Supported actions: load, save, stream, create_level, create_light, build_lighting, set_metadata, load_cells, set_datalayer, export_level, import_level, list_levels, get_summary, delete, validate_level.`,
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'load', 'save', 'save_as', 'save_level_as', 'stream', 'create_level', 'create_light', 'build_lighting',
            'set_metadata', 'load_cells', 'set_datalayer',
            'export_level', 'import_level', 'list_levels', 'get_summary', 'delete', 'validate_level',
            'cleanup_invalid_datalayers'
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
        levelPaths: { type: 'array', items: { type: 'string' } }
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
    description: `Animation and Physics tools.

Use it when you need to:
- create Animation Blueprints, Montages, or Blend Spaces.
- setup Ragdolls or Physics Assets.
- configure vehicles.

Supported actions: create_animation_bp, play_montage, setup_ragdoll, configure_vehicle, create_blend_space, create_state_machine, setup_ik, create_procedural_anim, create_blend_tree, setup_retargeting, setup_physics_simulation, cleanup.`,
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
    description: `Effects management (Niagara, Particles, Debug Shapes).

Use it when you need to:
- spawn effects or debug shapes.
- create/edit Niagara systems and emitters.
- edit Niagara graphs (modules, pins).

Supported actions: 
- Spawning: particle, niagara, debug_shape, spawn_niagara, create_dynamic_light.
- Asset Creation: create_niagara_system, create_niagara_emitter.
- Graph: add_niagara_module, connect_niagara_pins, remove_niagara_node, set_niagara_parameter.
- Utils: clear_debug_shapes, cleanup.`,
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
            'clear_debug_shapes', 'cleanup'
          ],
          description: 'Action'
        },
        name: { type: 'string' },
        systemName: { type: 'string' },
        systemPath: { type: 'string', description: 'Required for spawning Niagara effects (spawn_niagara, create_volumetric_fog, etc) and most graph operations.' },
        location: commonSchemas.location,
        scale: { type: 'number' },
        shape: { type: 'string' },
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
        value: { description: 'Value.' }
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
    description: `Environment creation (Landscape, Foliage).

Use it when you need to:
- create/sculpt landscapes.
- paint foliage.
- procedural generation.

Supported actions: create_landscape, sculpt, add_foliage, paint_foliage, create_procedural_terrain, create_procedural_foliage, add_foliage_instances, get_foliage_instances, remove_foliage.`,
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
    description: `System-level control.

Use it when you need to:
- profiling & performance (stat commands).
- quality settings.
- run external tools (UBT, Tests).
- manage logs/insights.
- execute arbitrary console commands.
- set/get CVars and project settings.
- validate assets.

Supported actions: 
- Core: profile, show_fps, set_quality, screenshot, set_resolution, set_fullscreen, execute_command, console_command.
- CVars: set_cvar.
- Settings: get_project_settings, validate_assets.
- Pipeline: run_ubt, run_tests.
- Debug/Logs: subscribe, spawn_category, start_session.
- Render: lumen_update_scene.
- UI: play_sound, create_widget, show_widget.`,
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
    description: `Sequencer (Cinematics) management.

Use it when you need to:
- create/edit level sequences.
- add tracks (camera, actors, audio, events).
- keyframe properties.
- manage sequence playback and settings.

Supported actions:
- Lifecycle: create, open, duplicate, rename, delete, list.
- Bindings: add_camera, add_actor, add_actors, remove_actors, get_bindings, add_spawnable_from_class.
- Playback: play, pause, stop, set_playback_speed.
- Keyframes: add_keyframe.
- Properties: get_properties, set_properties, get_metadata, set_metadata.
- Tracks: add_track, add_section, list_tracks, remove_track, set_track_muted, set_track_solo, set_track_locked.
- Settings: set_display_rate, set_tick_resolution, set_work_range, set_view_range.`,
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
            'list_tracks', 'remove_track'
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
    description: `Enhanced Input management.

Use it when you need to:
- create Input Actions (IA_*)
- create Input Mapping Contexts (IMC_*)
- bind keys to actions in a mapping context.

Supported actions:
- create_input_action: Create a UInputAction asset.
- create_input_mapping_context: Create a UInputMappingContext asset.
- add_mapping: Add a key mapping to a context.
- remove_mapping: Remove a mapping from a context.`,
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
    description: `Low-level object inspection and property manipulation.

Use it when you need to:
- read/write properties of any UObject or component.
- list objects/components.
- manage actor tags and snapshots.
- find objects by class or tag.

Supported actions:
- Object: inspect_object, inspect_class, list_objects, find_by_class, delete_object, export.
- Properties: get_property, set_property, get_component_property, set_component_property.
- Components: get_components, get_bounding_box.
- Tags: add_tag, find_by_tag, get_metadata.
- Snapshots: create_snapshot, restore_snapshot.`,
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
        actorName: { type: 'string', description: 'Actor name for inspection.' },
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
    description: `Audio asset and component management.

Use it when you need to:
- create sound cues/mixes.
- play sounds (3D/2D).
- control audio components.

Supported actions: create_sound_cue, play_sound_at_location, play_sound_2d, create_audio_component, create_sound_mix, push_sound_mix, pop_sound_mix, set_sound_mix_class_override, clear_sound_mix_class_override, set_base_sound_mix, prime_sound, play_sound_attached, spawn_sound_at_location, fade_sound_in, fade_sound_out.`,
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
            'fade_sound_in', 'fade_sound_out', 'create_ambient_sound'
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
        componentName: { type: 'string' }
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
    description: `Behavior Tree editing tool.

Use it when you need to:
- create new Behavior Tree assets.
- add nodes (Sequence, Selector, Tasks).
- connect nodes.
- set properties.

Supported actions: create, add_node, connect_nodes, remove_node, break_connections, set_node_properties.`,
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
    description: `Direct Blueprint Graph manipulation.

Use it when you need to:
- add nodes (Function, Variable, Event, Macro).
- connect/disconnect pins.
- create reroute nodes.
- set node properties.
- get node/graph details.

Supported actions: create_node, delete_node, connect_pins, break_pin_links, set_node_property, create_reroute_node, get_node_details, get_graph_details, get_pin_details.`,
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'create_node', 'delete_node', 'connect_pins', 'break_pin_links', 'set_node_property',
            'create_reroute_node', 'get_node_details', 'get_graph_details', 'get_pin_details'
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
    description: `Lighting and Rendering configuration.

Use it when you need to:
- spawn and configure lights (Directional, Point, Spot, Rect, Sky).
- configure Global Illumination (Lumen, Lightmass) and Shadows.
- setup Volumetric Fog and Atmosphere.
- build lighting (bake).
- set exposure and post-process settings.

Supported actions: 
- Spawning: spawn_light, create_light, spawn_sky_light, create_sky_light, ensure_single_sky_light, create_lightmass_volume, create_lighting_enabled_level, create_dynamic_light.
- Configuration: setup_global_illumination, configure_shadows, set_exposure, set_ambient_occlusion, setup_volumetric_fog.
- Building: build_lighting.`,
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'spawn_light', 'create_light', 'spawn_sky_light', 'create_sky_light', 'ensure_single_sky_light',
            'create_lightmass_volume', 'create_lighting_enabled_level', 'create_dynamic_light',
            'setup_global_illumination', 'configure_shadows', 'set_exposure', 'set_ambient_occlusion', 'setup_volumetric_fog',
            'build_lighting'
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
    description: `Performance profiling and optimization.

Use it when you need to:
- profiling (CPU, GPU, Memory, RenderThread).
- optimize content (LODs, texture streaming, draw calls).
- benchmark the scene.
- configure rendering features (Nanite, Lumen, World Partition).

Supported actions: 
- Profiling: start_profiling, stop_profiling, run_benchmark, show_fps, show_stats, generate_memory_report.
- Settings: set_scalability, set_resolution_scale, set_vsync, set_frame_rate_limit, enable_gpu_timing.
- Optimization: configure_texture_streaming, configure_lod, apply_baseline_settings, optimize_draw_calls, merge_actors, configure_occlusion_culling, optimize_shaders.
- Features: configure_nanite, configure_world_partition.`,
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
  }
];
