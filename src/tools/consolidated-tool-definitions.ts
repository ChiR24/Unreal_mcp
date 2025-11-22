export const consolidatedToolDefinitions = [
  // 1. ASSET MANAGER - Combines asset operations
  {
    name: 'manage_asset',
    description: `Asset library utility for browsing, importing, bootstrapping materials, and managing asset lifecycles.

Use it when you need to:
- explore project content (\u002fContent automatically maps to \u002fGame).
- import FBX/PNG/WAV/EXR files into the project.
- spin up a minimal Material asset at a specific path.
- duplicate, rename, move, or delete assets with optional redirector fixup.
- check source control status of assets.
- analyze asset dependency graphs.

Supported actions: list, import, create_material, create_material_instance, duplicate, rename, move, delete, delete_assets, create_folder, get_dependencies, get_source_control_state, analyze_graph, create_thumbnail, set_tags, generate_report, validate, fixup_redirectors, find_by_tag, get_metadata, set_metadata.`,
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: ['list', 'import', 'create_material', 'create_material_instance', 'duplicate', 'rename', 'move', 'delete', 'delete_assets', 'create_folder', 'get_dependencies', 'get_source_control_state', 'analyze_graph', 'create_thumbnail', 'set_tags', 'generate_report', 'validate', 'fixup_redirectors', 'find_by_tag', 'get_metadata', 'set_metadata'],
          description: 'Action to perform'
        },
        // For list
        directory: { type: 'string', description: 'Directory path to list (shows immediate children only). Automatically maps /Content to /Game. Example: "/Game/MyAssets"' },
        // For import
        sourcePath: { type: 'string', description: 'Source file path on disk to import (FBX, PNG, WAV, EXR supported). Example: "C:/MyAssets/mesh.fbx"' },
        destinationPath: { type: 'string', description: 'Destination path in project content where asset will be imported. Example: "/Game/ImportedAssets"' },
        // Duplicate / move / rename helpers
        assetPath: { type: 'string', description: 'Existing asset path (e.g., "/Game/MyFolder/MyAsset") used by rename/move/delete actions.' },
        assetPaths: {
          type: 'array',
          items: { type: 'string' },
          description: 'Array of asset paths to delete. Overrides assetPath when provided.'
        },
        newName: { type: 'string', description: 'Optional new asset name applied during duplicate, rename, or move actions.' },
        overwrite: { type: 'boolean', description: 'Overwrite any existing asset at the destination path when duplicating.' },
        save: { type: 'boolean', description: 'Save newly created assets after duplication.' },
        fixupRedirectors: { type: 'boolean', description: 'Run redirector fixup for affected folders after move/delete actions.' },
        timeoutMs: { type: 'number', description: 'Optional timeout in milliseconds for automation-bridge-backed asset operations.' },
        // For create_material and create_material_instance
        name: { type: 'string', description: 'Name for the new material asset or instance. Example: "MyMaterial"' },
        path: { type: 'string', description: 'Content path where material will be saved. Example: "/Game/Materials"' },
        parentMaterial: { type: 'string', description: 'Parent material (asset path) for material instances. Example: "/Game/Materials/M_MasterMaterial"' },
        parameters: { type: 'object', additionalProperties: true, description: 'Optional material parameter overrides for instances (name->value map).' },
        // Misc/advanced
        tag: { type: 'string', description: 'Tag name for find_by_tag action' },
        metadata: { type: 'object', additionalProperties: true, description: 'Arbitrary metadata map for set_metadata action' },
        directoryPath: { type: 'string', description: 'Optional directory path for fixup_redirectors' },
        // For create_thumbnail
        width: { type: 'number', description: 'Width for create_thumbnail (default 512).' },
        height: { type: 'number', description: 'Height for create_thumbnail (default 512).' },
        // For analyze_graph
        maxDepth: { type: 'number', description: 'Maximum depth for dependency graph analysis (analyze_graph).' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean', description: 'Whether the operation succeeded' },
        assets: {
          type: 'array',
          description: 'List of assets (for list action)',
          items: {
            type: 'object',
            properties: {
              Name: { type: 'string' },
              Path: { type: 'string' },
              Class: { type: 'string' },
              PackagePath: { type: 'string' }
            }
          }
        },
        paths: { type: 'array', items: { type: 'string' }, description: 'Imported asset paths (for import)' },
        materialPath: { type: 'string', description: 'Created material path (for create_material)' },
        materialInstancePath: { type: 'string', description: 'Created material instance path (for create_material_instance)' },
        path: { type: 'string', description: 'Resulting asset path for duplicate, rename, or move actions' },
        conflictPath: { type: 'string', description: 'Conflicting asset path when the operation could not proceed' },
        overwritten: { type: 'boolean', description: 'Whether an existing asset was overwritten during duplication' },
        deleted: { type: 'array', items: { type: 'string' }, description: 'List of assets successfully deleted (for delete action)' },
        missing: { type: 'array', items: { type: 'string' }, description: 'Assets that were not found during delete action' },
        failed: { type: 'array', items: { type: 'string' }, description: 'Assets that failed to delete' },
        message: { type: 'string', description: 'Status message' },
        error: { type: 'string', description: 'Error message if failed' }
      }
    }
  },
  // 7a. BLUEPRINT SNAPSHOT
  {
    name: 'blueprint_get',
    description: `Retrieve a Blueprint's snapshot from the automation bridge for inspection.

Use it when you need to:
- fetch the current Blueprint definition (variables, functions, events, defaults).
- validate metadata recorded by prior manage_blueprint operations.
- inspect Blueprint structure without issuing a multi-action manage request.

Inputs:
- blueprintPath (string): The Blueprint asset path (e.g. /Game/Blueprints/BP_MyActor).
- name (string): Optional alias when blueprintPath is omitted.
- timeoutMs (number): Optional timeout override.`,
    inputSchema: {
      type: 'object',
      properties: {
        blueprintPath: { type: 'string', description: 'Full Blueprint asset path (preferred).' },
        name: { type: 'string', description: 'Alternative Blueprint identifier when blueprintPath is not provided.' },
        timeoutMs: { type: 'integer', description: 'Optional timeout override in milliseconds.' }
      }
    }
  },

  // 2. ACTOR CONTROL - Combines actor operations
  {
    name: 'control_actor',
    description: `Viewport actor toolkit for spawning, removing, or nudging actors with physics forces, plus advanced component and tagging helpers.

Use it when you need to:
- drop a class or mesh into the level (classPath accepts names or asset paths).
- delete or duplicate actors by label, case-insensitively.
- push a physics-enabled actor with a world-space force vector.
- attach, tag, or edit component properties on actors already in the level.

Supported actions: spawn, spawn_blueprint, delete, delete_by_tag, duplicate, apply_force, set_transform, get_transform, set_visibility, add_component, set_component_properties, get_components, add_tag, find_by_tag, find_by_name, set_blueprint_variables, create_snapshot, attach, detach.`,
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'spawn',
            'spawn_blueprint',
            'delete',
            'delete_by_tag',
            'duplicate',
            'apply_force',
            'set_transform',
            'get_transform',
            'set_visibility',
            'add_component',
            'set_component_properties',
            'get_components',
            'add_tag',
            'find_by_tag',
            'find_by_name',
            'set_blueprint_variables',
            'create_snapshot',
            'attach',
            'detach'
          ],
          description: 'Action to perform'
        },
        // Common
        actorName: { type: 'string', description: 'Actor label/name (optional for spawn, auto-generated if not provided; required for most other operations). Case-insensitive for delete and lookup actions.' },
        classPath: {
          type: 'string',
          description: 'Actor class (e.g., "StaticMeshActor", "CameraActor") OR asset path (e.g., "/Engine/BasicShapes/Cube", "/Game/MyMesh"). Asset paths will automatically spawn as StaticMeshActor with the mesh applied. Required for spawn action.'
        },
        blueprintPath: {
          type: 'string',
          description: 'Blueprint asset path (e.g., "/Game/Blueprints/BP_MyActor") used by spawn_blueprint.'
        },
        // Transform
        location: {
          type: 'object',
          description: 'World space location in centimeters (Unreal units). Optional for spawn, defaults to origin.',
          properties: {
            x: { type: 'number', description: 'X coordinate (forward axis in Unreal)' },
            y: { type: 'number', description: 'Y coordinate (right axis in Unreal)' },
            z: { type: 'number', description: 'Z coordinate (up axis in Unreal)' }
          }
        },
        rotation: {
          type: 'object',
          description: 'World space rotation in degrees. Optional for spawn, defaults to zero rotation.',
          properties: {
            pitch: { type: 'number', description: 'Pitch rotation in degrees (Y-axis rotation)' },
            yaw: { type: 'number', description: 'Yaw rotation in degrees (Z-axis rotation)' },
            roll: { type: 'number', description: 'Roll rotation in degrees (X-axis rotation)' }
          }
        },
        scale: {
          type: 'object',
          description: 'World space scale for set_transform action.',
          properties: {
            x: { type: 'number', description: 'Scale factor along X axis' },
            y: { type: 'number', description: 'Scale factor along Y axis' },
            z: { type: 'number', description: 'Scale factor along Z axis' }
          }
        },
        // Physics
        force: {
          type: 'object',
          description: 'Force vector to apply in Newtons. Required for apply_force action. Actor must have physics simulation enabled.',
          properties: {
            x: { type: 'number', description: 'Force magnitude along X-axis' },
            y: { type: 'number', description: 'Force magnitude along Y-axis' },
            z: { type: 'number', description: 'Force magnitude along Z-axis' }
          }
        },
        componentType: { type: 'string', description: 'Component class to add when action is add_component (e.g., "PointLightComponent").' },
        componentName: { type: 'string', description: 'Component name used by add_component or set_component_properties actions.' },
        properties: { type: 'object', additionalProperties: true, description: 'Property overrides for add_component or set_component_properties actions.' },
        visible: { type: 'boolean', description: 'Visibility toggle used by set_visibility action.' },
        newName: { type: 'string', description: 'Optional new name used by duplicate action.' },
        offset: {
          type: 'object',
          description: 'Optional location offset applied during duplicate action.',
          properties: {
            x: { type: 'number' },
            y: { type: 'number' },
            z: { type: 'number' }
          }
        },
        tag: { type: 'string', description: 'Gameplay tag value used by add_tag, find_by_tag, or delete_by_tag actions.' },
        matchType: { type: 'string', description: 'Optional match type (exact, contains, etc.) for find_by_tag action.' },
        variables: { type: 'object', additionalProperties: true, description: 'Blueprint variable overrides for set_blueprint_variables action.' },
        snapshotName: { type: 'string', description: 'Snapshot identifier for create_snapshot action.' },
        childActor: { type: 'string', description: 'Child actor name used by attach action.' },
        parentActor: { type: 'string', description: 'Parent actor name used by attach action.' },
        timeoutMs: { type: 'integer', description: 'Optional per-call timeout override in milliseconds for automation-backed operations.' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean', description: 'Whether the operation succeeded' },
        actor: { type: 'string', description: 'Spawned actor name (for spawn)' },
        actorPath: { type: 'string', description: 'Resolved actor path in the level (for spawn)' },
        componentPaths: {
          type: 'array',
          description: 'Captured component references for newly spawned actors',
          items: {
            type: 'object',
            properties: {
              name: { type: 'string' },
              path: { type: 'string' },
              class: { type: 'string' }
            }
          }
        },
        deleted: {
          anyOf: [
            { type: 'string' },
            { type: 'array', items: { type: 'string' } }
          ],
          description: 'Deleted actor name or list of names (for delete actions)'
        },
        physicsEnabled: { type: 'boolean', description: 'Physics state (for apply_force)' },
        message: { type: 'string', description: 'Status message' },
        error: { type: 'string', description: 'Error message if failed' }
      }
    }
  },

  // 3. EDITOR CONTROL - Combines editor operations
  {
    name: 'control_editor',
    description: `Editor session controls for PIE playback, camera placement, recording, and viewport operations.

Use it when you need to:
- start, stop, pause, or resume Play In Editor.
- reposition the active viewport camera or adjust FOV/resolution.
- execute safe view mode changes and console commands.
- capture screenshots, bookmarks, or recording sessions for reviews.

Supported actions: play, stop, stop_pie, pause, resume, set_game_speed, eject, possess, set_camera, set_camera_position, set_camera_fov, set_view_mode, set_viewport_resolution, console_command, screenshot, step_frame, start_recording, stop_recording, create_bookmark, jump_to_bookmark, set_preferences, set_viewport_realtime.`,
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'play',
            'stop',
            'stop_pie',
            'pause',
            'resume',
            'set_game_speed',
            'eject',
            'possess',
            'set_camera',
            'set_camera_position',
            'set_camera_fov',
            'set_view_mode',
            'set_viewport_resolution',
            'console_command',
            'screenshot',
            'step_frame',
            'start_recording',
            'stop_recording',
            'create_bookmark',
            'jump_to_bookmark',
            'set_preferences',
            'set_viewport_realtime'
          ],
          description: 'Editor action'
        },
        // Camera
        location: {
          type: 'object',
          description: 'World space camera location for set_camera action. All coordinates required.',
          properties: {
            x: { type: 'number', description: 'X coordinate in centimeters' },
            y: { type: 'number', description: 'Y coordinate in centimeters' },
            z: { type: 'number', description: 'Z coordinate in centimeters' }
          }
        },
        rotation: {
          type: 'object',
          description: 'Camera rotation for set_camera action. All rotation components required.',
          properties: {
            pitch: { type: 'number', description: 'Pitch in degrees' },
            yaw: { type: 'number', description: 'Yaw in degrees' },
            roll: { type: 'number', description: 'Roll in degrees' }
          }
        },
        // View mode
        viewMode: {
          type: 'string',
          description: 'View mode for set_view_mode action. Supported: Lit, Unlit, Wireframe, DetailLighting, LightingOnly, LightComplexity, ShaderComplexity. Required for set_view_mode.'
        },
        speed: { type: 'number', description: 'Playback speed multiplier for set_game_speed action.' },
        filename: { type: 'string', description: 'Optional file name for screenshot/start_recording actions.' },
        fov: { type: 'number', description: 'Field of view value in degrees for set_camera_fov.' },
        width: { type: 'number', description: 'Viewport width in pixels for set_viewport_resolution.' },
        height: { type: 'number', description: 'Viewport height in pixels for set_viewport_resolution.' },
        command: { type: 'string', description: 'Console command to execute when action is console_command.' },
        steps: { type: 'integer', description: 'Frame steps to advance for step_frame action (defaults to 1).' },
        frameRate: { type: 'number', description: 'Capture frame rate for start_recording action.' },
        durationSeconds: { type: 'number', description: 'Optional maximum recording duration for start_recording.' },
        bookmarkName: { type: 'string', description: 'Bookmark identifier for create_bookmark or jump_to_bookmark actions.' },
        category: { type: 'string', description: 'Editor preference category for set_preferences action.' },
        preferences: { type: 'object', additionalProperties: true, description: 'Preference overrides for set_preferences action.' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean', description: 'Whether the operation succeeded' },
        playing: { type: 'boolean', description: 'PIE play state' },
        location: {
          type: 'array',
          items: { type: 'number' },
          description: 'Camera location [x, y, z]'
        },
        rotation: {
          type: 'array',
          items: { type: 'number' },
          description: 'Camera rotation [pitch, yaw, roll]'
        },
        viewMode: { type: 'string', description: 'Current view mode' },
        message: { type: 'string', description: 'Status message' }
      }
    }
  },

  // 4. LEVEL MANAGER - Combines level and lighting operations
  {
    name: 'manage_level',
    description: `Level management helper for loading/saving, streaming, light creation, and lighting builds.

Use it when you need to:
- open or save a level by path.
- toggle streaming sublevels on/off.
- spawn a light actor of a given type.
- generate a new map using a predefined template.
- kick off a lighting build at a chosen quality.

Supported actions: load, save, stream, create_level, create_light, build_lighting.`,
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: ['load', 'save', 'stream', 'create_level', 'create_light', 'build_lighting', 'set_metadata'],
          description: 'Level action'
        },
        // Level
        levelPath: { type: 'string', description: 'Full content path to level asset (e.g., "/Game/Maps/MyLevel"). Required for load action.' },
        levelName: { type: 'string', description: 'Optional short name for streaming operations. If omitted, derived automatically from levelPath.' },
        streaming: { type: 'boolean', description: 'Whether to use streaming load (true) or direct load (false). Optional for load action.' },
        shouldBeLoaded: { type: 'boolean', description: 'Whether to load (true) or unload (false) the streaming level. Required for stream action.' },
        shouldBeVisible: { type: 'boolean', description: 'Whether the streaming level should be visible after loading. Defaults to shouldBeLoaded when omitted.' },
        // Lighting
        lightType: {
          type: 'string',
          enum: ['Directional', 'Point', 'Spot', 'Rect'],
          description: 'Type of light to create. Directional for sun-like lighting, Point for omni-directional, Spot for cone-shaped, Rect for area lighting. Required for create_light.'
        },
        name: { type: 'string', description: 'Name for the spawned light actor. Optional, auto-generated if not provided.' },
        location: {
          type: 'object',
          description: 'World space location for light placement in centimeters. Optional for create_light, defaults to origin.',
          properties: {
            x: { type: 'number', description: 'X coordinate' },
            y: { type: 'number', description: 'Y coordinate' },
            z: { type: 'number', description: 'Z coordinate' }
          }
        },
        intensity: { type: 'number', description: 'Light intensity value in lumens (for Point/Spot) or lux (for Directional). Typical range: 1000-10000. Optional for create_light.' },
        quality: {
          type: 'string',
          enum: ['Preview', 'Medium', 'High', 'Production'],
          description: 'Lighting build quality level. Preview is fastest, Production is highest quality. Required for build_lighting action.'
        },
        metadata: { type: 'object', additionalProperties: true, description: 'Arbitrary metadata map for set_metadata action targeting the level asset.' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean', description: 'Whether the operation succeeded' },
        levelName: { type: 'string', description: 'Level name' },
        loaded: { type: 'boolean', description: 'Level loaded state' },
        visible: { type: 'boolean', description: 'Level visibility' },
        lightName: { type: 'string', description: 'Created light name' },
        buildQuality: { type: 'string', description: 'Lighting build quality used' },
        message: { type: 'string', description: 'Status message' }
      }
    }
  },

  // 5. ANIMATION SYSTEM - Combines animation and physics setup
  {
    name: 'animation_physics',
    description: `Animation and physics rigging helper covering Anim BPs, montage playback, ragdolls, state machines, IK, blend spaces/trees, and vehicle setup.

Use it when you need to:
- generate an Animation Blueprint for a skeleton.
- play a montage/animation on an actor at a chosen rate.
- enable a quick ragdoll using an existing physics asset.
- configure vehicle movement and drivetrain settings via physics helpers.
- generate blend spaces/trees and state machines.
- setup IK chains and retargeting.

Supported actions: create_animation_bp, create_anim_blueprint, create_animation_blueprint, play_montage, play_anim_montage, setup_ragdoll, activate_ragdoll, configure_vehicle, create_blend_space, create_state_machine, setup_ik, create_procedural_anim, create_blend_tree, setup_retargeting, setup_physics_simulation, create_animation_asset, cleanup.`,
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'create_animation_bp',
            'create_anim_blueprint',
            'create_animation_blueprint',
            'play_montage',
            'play_anim_montage',
            'setup_ragdoll',
            'activate_ragdoll',
            'configure_vehicle',
            'create_blend_space',
            'create_state_machine',
            'setup_ik',
            'create_procedural_anim',
            'create_blend_tree',
            'setup_retargeting',
            'setup_physics_simulation',
            'create_animation_asset',
            'cleanup'
          ],
          description: 'Action type'
        },
        // Common
        name: { type: 'string', description: 'Name for the created animation blueprint asset. Required for create_animation_bp action.' },
        actorName: { type: 'string', description: 'Actor label/name in the level to apply animation to. Required for play_montage and setup_ragdoll actions.' },
        // Animation
        skeletonPath: { type: 'string', description: 'Content path to skeleton asset (e.g., "/Game/Characters/MySkeleton"). Required for create_animation_bp action.' },
        montagePath: { type: 'string', description: 'Content path to animation montage asset to play. Required for play_montage if animationPath not provided.' },
        animationPath: { type: 'string', description: 'Content path to animation sequence asset to play. Alternative to montagePath for play_montage action.' },
        playRate: { type: 'number', description: 'Animation playback speed multiplier. 1.0 is normal speed, 2.0 is double speed, 0.5 is half speed. Optional, defaults to 1.0.' },
        // Physics
        physicsAssetName: { type: 'string', description: 'Name or path to physics asset for ragdoll simulation. Required for setup_ragdoll action.' },
        blendWeight: { type: 'number', description: 'Blend weight between animated and ragdoll physics (0.0 to 1.0). 0.0 is fully animated, 1.0 is fully ragdoll. Optional, defaults to 1.0.' },
        meshPath: { type: 'string', description: 'Skeletal mesh to generate a physics asset for. Required when skeletonPath does not resolve to a preview mesh.' },
        assignToMesh: { type: 'boolean', description: 'Assign the newly created physics asset to the skeletal mesh after creation.' },
        previewSkeleton: { type: 'string', description: 'Optional skeleton used to resolve a preview mesh when the main skeleton lacks one.' },
        generateConstraints: { type: 'boolean', description: 'Hint for physics setup on whether to auto-generate joint constraints.' },
        savePath: { type: 'string', description: 'Content path where animation blueprint will be saved (e.g., "/Game/Animations"). Required for create_animation_bp action.' },
        // Vehicle configuration
        vehicleName: { type: 'string', description: 'Vehicle actor or Blueprint identifier to configure. Required for configure_vehicle action.' },
        vehicleType: { type: 'string', enum: ['Car', 'Bike', 'Tank', 'Aircraft'], description: 'Vehicle archetype used for helper defaults (Car, Bike, Tank, Aircraft). Required for configure_vehicle action.' },
        wheels: {
          type: 'array',
          description: 'Wheel configuration entries for configure_vehicle. Each entry should include radius, width, mass, and steering/driving flags.',
          items: {
            type: 'object',
            properties: {
              name: { type: 'string', description: 'Wheel identifier or socket name' },
              radius: { type: 'number', description: 'Wheel radius in centimeters' },
              width: { type: 'number', description: 'Wheel width in centimeters' },
              mass: { type: 'number', description: 'Wheel mass in kilograms' },
              isSteering: { type: 'boolean', description: 'Whether this wheel steers' },
              isDriving: { type: 'boolean', description: 'Whether this wheel applies drive torque' }
            }
          }
        },
        engine: {
          type: 'object',
          description: 'Engine configuration block for configure_vehicle action.',
          properties: {
            maxRPM: { type: 'number', description: 'Maximum engine RPM' },
            torqueCurve: {
              type: 'array',
              description: 'Array of [RPM, Torque] points defining the torque curve.',
              items: {
                type: 'array',
                items: { type: 'number' },
                minItems: 2,
                maxItems: 2
              }
            }
          }
        },
        transmission: {
          type: 'object',
          description: 'Transmission configuration for configure_vehicle action.',
          properties: {
            gears: {
              type: 'array',
              description: 'Gear ratios for the transmission (index 0 = first gear).',
              items: { type: 'number' }
            },
            finalDriveRatio: { type: 'number', description: 'Final drive ratio applied after the gear ratios.' }
          }
        },
        pluginDependencies: {
          type: 'array',
          description: 'Optional list of Unreal plugin names that must be enabled before configure_vehicle runs.',
          items: { type: 'string' }
        },
        // Advanced animation features (optional)
        blueprintPath: { type: 'string', description: 'Animation Blueprint path or target Blueprint path' },
        states: { type: 'array', items: { type: 'object' }, description: 'State definitions for state machines' },
        transitions: { type: 'array', items: { type: 'object' }, description: 'Transition definitions for state machines' },
        chain: { type: 'object', description: 'IK chain configuration' },
        effector: { type: 'object', description: 'IK effector target' },
        settings: { type: 'object', description: 'Generic settings for procedural animation creation' },
        dimensions: { anyOf: [{ type: 'number' }, { type: 'array', items: { type: 'number' }, minItems: 2, maxItems: 2 }], description: 'Blend space dimensions or axis count' },
        horizontalAxis: { type: 'object', properties: { name: { type: 'string' }, minValue: { type: 'number' }, maxValue: { type: 'number' } } },
        verticalAxis: { type: 'object', properties: { name: { type: 'string' }, minValue: { type: 'number' }, maxValue: { type: 'number' } } },
        samples: { type: 'array', items: { type: 'object' }, description: 'Sample animations for blend spaces' },
        assetType: { type: 'string', description: 'Type of animation asset to create (Sequence, Montage, etc.)' },
        sourceSkeleton: { type: 'string', description: 'Source skeleton for retargeting' },
        targetSkeleton: { type: 'string', description: 'Target skeleton for retargeting' },
        assets: { type: 'array', items: { type: 'string' }, description: 'Animation assets to duplicate and retarget' },
        retargetAssets: { type: 'array', items: { type: 'string' }, description: 'Alias for assets; retained for backward compatibility' },
        params: { type: 'object', description: 'Parameters for physics simulation setup' },
        artifacts: { type: 'array', items: { type: 'string' }, description: 'Artifact asset paths to cleanup' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean', description: 'Whether the operation succeeded' },
        blueprintPath: { type: 'string', description: 'Created animation blueprint path' },
        playing: { type: 'boolean', description: 'Montage playing state' },
        playRate: { type: 'number', description: 'Current play rate' },
        ragdollActive: { type: 'boolean', description: 'Ragdoll activation state' },
        vehicleName: { type: 'string', description: 'Vehicle identifier used during configure_vehicle' },
        vehicleType: { type: 'string', description: 'Vehicle archetype applied during configure_vehicle' },
        warnings: { type: 'array', items: { type: 'string' }, description: 'Warnings emitted during the operation' },
        message: { type: 'string', description: 'Status message' }
      }
    }
  },

  // 6. EFFECTS SYSTEM - Combines particles and visual effects
  {
    name: 'create_effect',
    description: `FX sandbox for spawning Niagara systems, particle presets, or disposable debug shapes.

Use it when you need to:
- fire a Niagara system at a specific location/scale.
- trigger a simple particle effect by tag/name.
- draw temporary debug primitives (box/sphere/line) for planning layouts.

Supported actions: niagara, particle, debug_shape.`,
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'particle',
            'niagara',
            'debug_shape',
            'spawn_niagara',
            'set_niagara_parameter',
            'clear_debug_shapes',
            'create_dynamic_light',
            'cleanup'
          ],
          description: 'Effect type'
        },
        // Common
        name: { type: 'string', description: 'Name for the spawned effect actor. Optional, auto-generated if not provided.' },
        location: {
          type: 'object',
          description: 'World space location where effect will be spawned in centimeters. Optional, defaults to origin.',
          properties: {
            x: { type: 'number', description: 'X coordinate' },
            y: { type: 'number', description: 'Y coordinate' },
            z: { type: 'number', description: 'Z coordinate' }
          }
        },
        // Particles
        effectType: {
          type: 'string',
          description: 'Preset particle effect type (Fire, Smoke, Water, Explosion, etc.). Used for particle action to spawn common effects.'
        },
        systemPath: { type: 'string', description: 'Content path to Niagara system asset (e.g., "/Game/Effects/MyNiagaraSystem"). Required for niagara action.' },
        scale: { type: 'number', description: 'Uniform scale multiplier for Niagara effect. 1.0 is normal size. Optional, defaults to 1.0.' },
        // Debug
        shape: {
          type: 'string',
          description: 'Debug shape type to draw (Line, Box, Sphere, Capsule, Cone, Cylinder, Arrow). Required for debug_shape action.'
        },
        size: { type: 'number', description: 'Size/radius of debug shape in centimeters. For Line, this is thickness. For shapes, this is radius/extent. Optional, defaults vary by shape.' },
        color: {
          type: 'array',
          items: { type: 'number' },
          description: 'RGBA color array with values 0-255 (e.g., [255, 0, 0, 255] for red). Optional, defaults to white.'
        },
        duration: { type: 'number', description: 'How long debug shape persists in seconds. 0 means one frame, -1 means permanent until cleared. Optional, defaults to 0.' },
        // Dynamic light parameters (create_dynamic_light)
        lightName: { type: 'string', description: 'Optional name for the dynamic light actor (create_dynamic_light).' },
        lightType: { type: 'string', description: 'Light type for create_dynamic_light: Point, Spot, Directional, Rect.' },
        intensity: { type: 'number', description: 'Light intensity for create_dynamic_light (engine units). Optional.' },
        rotation: { type: 'object', description: 'Rotation for directional/spot/rect lights (pitch,yaw,roll).', properties: { pitch: { type: 'number' }, yaw: { type: 'number' }, roll: { type: 'number' } } },
        pulse: { type: 'object', description: 'Optional pulsing behavior for dynamic lights.', properties: { enabled: { type: 'boolean' }, frequency: { type: 'number' } } },
        // Cleanup (actor removal) filter
        filter: { type: 'string', description: 'Label filter prefix used by cleanup to remove spawned effect actors (create_effect.cleanup).' },
        // Niagara parameter helpers
        systemName: { type: 'string', description: 'Niagara system name (set_niagara_parameter).' },
        parameterName: { type: 'string', description: 'Niagara parameter name (set_niagara_parameter).' },
        parameterType: { type: 'string', description: 'Niagara parameter type (Float, Vector, Color, Bool, Int).' },
        value: { description: 'Value to set for set_niagara_parameter (type dependent).' },
        isUserParameter: { type: 'boolean', description: 'Whether the parameter is a user-exposed parameter (set_niagara_parameter).' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean', description: 'Whether the operation succeeded' },
        effectName: { type: 'string', description: 'Created effect name' },
        effectPath: { type: 'string', description: 'Effect asset path' },
        spawned: { type: 'boolean', description: 'Whether effect was spawned in level' },
        location: {
          type: 'array',
          items: { type: 'number' },
          description: 'Effect location [x, y, z]'
        },
        message: { type: 'string', description: 'Status message' }
      }
    }
  },

  // 7. BLUEPRINT MANAGER - Blueprint operations
  {
    name: 'manage_blueprint',
    description: `Blueprint scaffolding helper for creating assets, attaching components, and mutating defaults.

Use it when you need to:
- create a new Blueprint of a specific base type (Actor, Pawn, Character, ...).
- add a component to an existing Blueprint asset with a unique name.
- tweak Blueprint Class Default Object (CDO) properties when direct editor access cannot.
- orchestrate multi-step Simple Construction Script edits with compile/save toggles.
- retrieve, add, remove, reparent, or modify SCS (Simple Construction Script) components.
- set component transforms and properties within the SCS hierarchy.

Supported actions: create, add_component, set_default, modify_scs, get_scs, add_scs_component, remove_scs_component, reparent_scs_component, set_scs_transform, set_scs_property.`,
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: ['create', 'add_component', 'set_default', 'modify_scs', 'ensure_exists', 'probe_handle', 'add_variable', 'add_function', 'add_event', 'add_construction_script', 'set_variable_metadata', 'set_metadata', 'get', 'get_scs', 'add_scs_component', 'remove_scs_component', 'reparent_scs_component', 'set_scs_transform', 'set_scs_property', 'add_node', 'connect_pins'],
          description: 'Blueprint action'
        },
        componentClass: { type: 'string', description: 'Optional component class name for probe_handle (e.g., StaticMeshComponent)' },
        name: { type: 'string', description: 'Name for the blueprint asset. Required for create action. For add_component, this is the blueprint asset name or path.' },
        blueprintPath: { type: 'string', description: 'Alternative blueprint identifier for modify_scs when different from name.' },
        blueprintType: {
          type: 'string',
          description: 'Base class type for blueprint (Actor, Pawn, Character, Object, ActorComponent, SceneComponent, etc.). Required for create action.'
        },
        componentType: { type: 'string', description: 'Component class to add (StaticMeshComponent, SkeletalMeshComponent, CameraComponent, etc.). Required for add_component action.' },
        componentName: { type: 'string', description: 'Unique name for the component instance within the blueprint. Required for add_component action.' },
        attachTo: { type: 'string', description: 'Optional parent component name when adding components.' },
        transform: {
          type: 'object',
          description: 'Relative transform overrides for add_component or set_component_properties operations.',
          properties: {
            location: {
              type: 'object',
              description: 'Relative location in centimeters (X/Y/Z). Each axis is optional.',
              properties: {
                x: { type: 'number' },
                y: { type: 'number' },
                z: { type: 'number' }
              }
            },
            rotation: {
              type: 'object',
              description: 'Relative rotation in degrees (Pitch/Yaw/Roll). Each axis is optional.',
              properties: {
                pitch: { type: 'number' },
                yaw: { type: 'number' },
                roll: { type: 'number' }
              }
            },
            scale: {
              type: 'object',
              description: 'Relative scale multiplier. Each axis is optional.',
              properties: {
                x: { type: 'number' },
                y: { type: 'number' },
                z: { type: 'number' }
              }
            }
          }
        },
        properties: {
          type: 'object',
          description: 'Component template property overrides applied during add_component or modify_scs.'
        },
        savePath: { type: 'string', description: 'Content path where blueprint will be saved (e.g., "/Game/Blueprints"). Required for create action.' },
        propertyName: { type: 'string', description: 'Blueprint default property to set when action is set_default.' },
        metadata: { type: 'object', additionalProperties: true, description: 'Arbitrary metadata map for set_metadata action targeting the Blueprint asset.' },
        value: { description: 'Value to assign to the Blueprint default property when action is set_default. Accepts JSON-compatible values.' },
        compile: { type: 'boolean', description: 'Compile the Blueprint after modify_scs executes.' },
        save: { type: 'boolean', description: 'Save the Blueprint after modify_scs executes.' },
        timeoutMs: { type: 'number', description: 'Optional timeout (ms) for automation-bridge-backed operations.' },
        operations: {
          type: 'array',
          description: 'List of Simple Construction Script operations for modify_scs.',
          items: {
            type: 'object',
            properties: {
              type: { type: 'string', enum: ['add_component', 'remove_component', 'set_component_properties'], description: 'Operation type.' },
              componentName: { type: 'string', description: 'Target component name.' },
              componentClass: { type: 'string', description: 'Component class path (add_component).' },
              attachTo: { type: 'string', description: 'Parent component when attaching new components.' },
              transform: {
                type: 'object',
                description: 'Relative transform overrides for this operation.',
                properties: {
                  location: {
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
                  }
                }
              },
              properties: { type: 'object', description: 'Component template property overrides.' }
            },
            required: ['type']
          }
        },
        // SCS (Simple Construction Script) specific properties
        newParent: { type: 'string', description: 'New parent component name for reparent_scs_component action.' },
        location: {
          type: 'array',
          items: { type: 'number' },
          minItems: 3,
          maxItems: 3,
          description: 'Location vector [X, Y, Z] in centimeters for set_scs_transform action.'
        },
        rotation: {
          type: 'array',
          items: { type: 'number' },
          minItems: 3,
          maxItems: 3,
          description: 'Rotation vector [Pitch, Yaw, Roll] in degrees for set_scs_transform action.'
        },
        scale: {
          type: 'array',
          items: { type: 'number' },
          minItems: 3,
          maxItems: 3,
          description: 'Scale vector [X, Y, Z] for set_scs_transform action.'
        },
        propertyValue: { description: 'Property value to set for set_scs_property action. Type depends on the property being set.' }
      },
      required: ['action', 'name']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean', description: 'Whether the operation succeeded' },
        blueprintPath: { type: 'string', description: 'Blueprint asset path' },
        componentAdded: { type: 'string', description: 'Added component name (legacy)' },
        componentName: { type: 'string', description: 'Target component name for add_component or modify_scs responses.' },
        componentClass: { type: 'string', description: 'Component class reference reported from add_component.' },
        propertyName: { type: 'string', description: 'Blueprint default property that was updated' },
        value: { description: 'Value applied to the default property' },
        cdoPath: { type: 'string', description: 'Resolved Blueprint CDO path when mutating defaults' },
        compiled: { type: 'boolean', description: 'Whether the Blueprint was compiled after modify_scs.' },
        saved: { type: 'boolean', description: 'Whether the Blueprint asset was saved after modify_scs.' },
        operations: {
          type: 'array',
          description: 'Per-operation outcome returned by modify_scs.',
          items: {
            type: 'object',
            properties: {
              index: { type: 'number' },
              type: { type: 'string' },
              success: { type: 'boolean' },
              componentName: { type: 'string' },
              componentClass: { type: 'string' },
              warning: { type: 'string' }
            }
          }
        },
        message: { type: 'string', description: 'Status message' },
        warning: { type: 'string', description: 'Warning if manual steps needed' },
        warnings: { type: 'array', items: { type: 'string' }, description: 'Detailed warnings when fallbacks/simulation is used' },
        details: { type: 'array', items: { type: 'string' }, description: 'Additional context about Blueprint operations' },
        transport: { type: 'string', description: 'Transport used (e.g., automation_bridge).' },
        bridge: {
          type: 'object',
          description: 'Automation bridge debugging details when available.',
          properties: {
            requestId: { type: 'string' },
            success: { type: 'boolean' },
            error: { type: 'string' }
          }
        },
        // SCS-specific output properties
        scsComponents: {
          type: 'array',
          description: 'List of SCS components for get_scs action.',
          items: {
            type: 'object',
            properties: {
              name: { type: 'string', description: 'Component name' },
              class: { type: 'string', description: 'Component class' },
              parent: { type: 'string', description: 'Parent component name' },
              transform: {
                type: 'object',
                description: 'Component relative transform',
                properties: {
                  location: { type: 'array', items: { type: 'number' }, description: 'Location [X,Y,Z]' },
                  rotation: { type: 'array', items: { type: 'number' }, description: 'Rotation [Pitch,Yaw,Roll]' },
                  scale: { type: 'array', items: { type: 'number' }, description: 'Scale [X,Y,Z]' }
                }
              }
            }
          }
        },
        scsHierarchy: { type: 'string', description: 'Text representation of SCS component hierarchy for get_scs action.' },
        error: { type: 'string', description: 'Error message if operation failed' }
      }
    }
  },

  // 8. ENVIRONMENT BUILDER - Landscape and foliage
  {
    name: 'build_environment',
    description: `Environment authoring toolkit for landscapes and foliage, from sculpting to procedural scatters.

Use it when you need to:
- create or sculpt a landscape actor.
- add foliage via types or paint strokes.
- drive procedural terrain/foliage generation with bounds, seeds, and density settings.
- spawn explicit foliage instances at transforms.

Supported actions: create_landscape, sculpt, add_foliage, paint_foliage, create_procedural_terrain, create_procedural_foliage, add_foliage_instances, get_foliage_instances, remove_foliage, create_landscape_grass_type.`,
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'create_landscape',
            'sculpt',
            'add_foliage',
            'paint_foliage',
            'create_procedural_terrain',
            'create_procedural_foliage',
            'add_foliage_instances',
            'get_foliage_instances',
            'remove_foliage',
            'create_landscape_grass_type',
            'export_snapshot',
            'import_snapshot',
            'delete',
            'add_landscape_spline',
            'set_landscape_material',
            'create_layer',
            'paint_layer',
            'export_heightmap',
            'import_heightmap'
          ],
          description: 'Environment action'
        },
        // Common
        name: { type: 'string', description: 'Name for landscape, foliage type, or grass type actor. Optional for most actions, auto-generated if not provided.' },
        // Landscape
        sizeX: { type: 'number', description: 'Landscape width in components. Each component is typically 63 quads. Required for create_landscape action.' },
        sizeY: { type: 'number', description: 'Landscape height in components. Each component is typically 63 quads. Required for create_landscape action.' },
        tool: {
          type: 'string',
          description: 'Landscape sculpt tool to use (Sculpt, Smooth, Flatten, Ramp, Erosion, Hydro, Noise). Required for sculpt action.'
        },
        // Advanced: procedural terrain
        location: {
          type: 'object',
          description: 'World space location for terrain placement. Required for create_procedural_terrain.',
          properties: { x: { type: 'number', description: 'X coordinate' }, y: { type: 'number', description: 'Y coordinate' }, z: { type: 'number', description: 'Z coordinate' } }
        },
        subdivisions: { type: 'number', description: 'Number of subdivisions for procedural terrain mesh. Higher values create more detailed terrain. Optional for create_procedural_terrain.' },
        heightFunction: { type: 'string', description: 'Mathematical function or algorithm for terrain height generation (e.g., "perlin", "simplex", custom formula). Optional for create_procedural_terrain.' },
        materialPath: { type: 'string', description: 'Content path to material for terrain/landscape (e.g., "/Game/Materials/TerrainMat"). Optional.' },
        // Advanced: procedural foliage
        bounds: {
          type: 'object',
          properties: {
            location: { type: 'object', properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } } },
            size: { type: 'object', properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } } }
          }
        },
        foliageTypes: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              meshPath: { type: 'string' },
              density: { type: 'number' },
              minScale: { type: 'number' },
              maxScale: { type: 'number' },
              alignToNormal: { type: 'boolean' },
              randomYaw: { type: 'boolean' }
            }
          }
        },
        seed: { type: 'number' },
        // Advanced: direct foliage instances
        foliageType: { type: 'string' },
        transforms: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              location: { type: 'object', properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } } },
              rotation: { type: 'object', properties: { pitch: { type: 'number' }, yaw: { type: 'number' }, roll: { type: 'number' } } },
              scale: { type: 'object', properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } } }
            }
          }
        },
        // Foliage (for add_foliage)
        meshPath: { type: 'string', description: 'Content path to static mesh for foliage (e.g., "/Game/Foliage/TreeMesh"). Required for add_foliage action.' },
        density: { type: 'number', description: 'Foliage placement density (instances per unit area). Typical range: 0.1 to 10.0. Required for add_foliage and affects procedural foliage.' },
        // Painting
        position: {
          type: 'object',
          description: 'World space position for foliage paint brush center. Required for paint_foliage action.',
          properties: {
            x: { type: 'number', description: 'X coordinate' },
            y: { type: 'number', description: 'Y coordinate' },
            z: { type: 'number', description: 'Z coordinate' }
          }
        },
        brushSize: { type: 'number', description: 'Radius of foliage paint brush in centimeters. Typical range: 500-5000. Required for paint_foliage action.' },
        strength: { type: 'number', description: 'Paint tool strength/intensity (0.0 to 1.0). Higher values place more instances. Optional for paint_foliage, defaults to 0.5.' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean', description: 'Whether the operation succeeded' },
        landscapeName: { type: 'string', description: 'Landscape actor name' },
        foliageTypeName: { type: 'string', description: 'Foliage type name' },
        instancesPlaced: { type: 'number', description: 'Number of foliage instances placed' },
        message: { type: 'string', description: 'Status message' }
      }
    }
  },

  // 9. PERFORMANCE & AUDIO - System settings
  {
    name: 'system_control',
    description: `Runtime/system controls for profiling, quality tiers, audio/UI triggers, and screenshots.

Use it when you need to:
- toggle stat overlays or targeted profilers.
- adjust scalability categories (sg.*) or enable FPS display.
- play a one-shot sound and optionally position it.
- create/show lightweight widgets.
- capture a screenshot or adjust resolution/fullscreen settings.

Supported actions: profile, show_fps, set_quality, play_sound, create_widget, show_widget, screenshot, set_resolution, set_fullscreen, set_cvar, execute_command.`,
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: ['profile', 'show_fps', 'set_quality', 'play_sound', 'create_widget', 'show_widget', 'screenshot', 'set_resolution', 'set_fullscreen', 'set_cvar', 'execute_command'],
          description: 'System action'
        },
        // Performance
        profileType: {
          type: 'string',
          description: 'Type of profiling to enable: CPU (stat cpu), GPU (stat gpu), Memory (stat memory), FPS (stat fps), Unit (stat unit). Required for profile action.'
        },
        category: {
          type: 'string',
          description: 'Scalability quality category to adjust: ViewDistance, AntiAliasing, Shadow/Shadows, PostProcess/PostProcessing, Texture/Textures, Effects, Foliage, Shading. Required for set_quality action.'
        },
        level: { type: 'number', description: 'Quality level (0=Low, 1=Medium, 2=High, 3=Epic, 4=Cinematic). Required for set_quality action.' },
        enabled: { type: 'boolean', description: 'Enable (true) or disable (false) profiling/FPS display. Required for profile and show_fps actions.' },
        verbose: { type: 'boolean', description: 'Show verbose profiling output with additional details. Optional for profile action.' },
        // Audio
        soundPath: { type: 'string', description: 'Content path to sound asset (SoundCue or SoundWave, e.g., "/Game/Audio/MySound"). Required for play_sound action.' },
        location: {
          type: 'object',
          description: 'World space location for 3D sound playback. Required if is3D is true for play_sound action.',
          properties: {
            x: { type: 'number', description: 'X coordinate' },
            y: { type: 'number', description: 'Y coordinate' },
            z: { type: 'number', description: 'Z coordinate' }
          }
        },
        volume: { type: 'number', description: 'Volume multiplier (0.0=silent, 1.0=full volume). Optional for play_sound, defaults to 1.0.' },
        is3D: { type: 'boolean', description: 'Whether sound should be played as 3D positional audio (true) or 2D (false). Optional for play_sound, defaults to false.' },
        // UI
        widgetName: { type: 'string', description: 'Name for widget asset or instance. Required for create_widget and show_widget actions.' },
        widgetType: {
          type: 'string',
          description: 'Widget blueprint type or category (HUD, Menu, Dialog, Notification, etc.). Optional for create_widget, helps categorize the widget.'
        },
        visible: { type: 'boolean', description: 'Whether widget should be visible (true) or hidden (false). Required for show_widget action.' },
        // Screenshot
        resolution: { type: 'string', description: 'Screenshot resolution in WIDTHxHEIGHT format (e.g., "1920x1080", "3840x2160"). Optional for screenshot action, uses viewport size if not specified.' },
        // Resolution / fullscreen control (system_control)
        width: { type: 'number', description: 'Screen width in pixels (set_resolution / set_fullscreen). Required for set_resolution.' },
        height: { type: 'number', description: 'Screen height in pixels (set_resolution / set_fullscreen). Required for set_resolution.' },
        windowed: { type: 'boolean', description: 'Windowed mode when using set_resolution (true=windowed, false=fullscreen). Optional; defaults to true.' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean', description: 'Whether the operation succeeded' },
        profiling: { type: 'boolean', description: 'Profiling active state' },
        fpsVisible: { type: 'boolean', description: 'FPS display state' },
        qualityLevel: { type: 'number', description: 'Current quality level' },
        soundPlaying: { type: 'boolean', description: 'Sound playback state' },
        widgetPath: { type: 'string', description: 'Created widget path' },
        widgetVisible: { type: 'boolean', description: 'Widget visibility state' },
        imagePath: { type: 'string', description: 'Saved screenshot path' },
        imageBase64: { type: 'string', description: 'Screenshot image base64 (truncated)' },
        pid: { type: 'number', description: 'Process ID for launched editor' },
        message: { type: 'string', description: 'Status message' },
        error: { type: 'string', description: 'Error message if failed' }
      }
    }
  },

  // 10. CONSOLE COMMAND - Universal tool
  {
    name: 'console_command',
    description: `Guarded console command executor for one-off \`stat\`, \`r.*\`, or viewmode commands.

Use it when higher-level tools don't cover the console tweak you need. Hazardous commands (quit/exit, crash triggers, unsafe viewmodes) are blocked, and unknown commands respond with a warning instead of executing blindly.`,
    inputSchema: {
      type: 'object',
      properties: {
        command: { type: 'string', description: 'Console command to execute in Unreal Engine (e.g., "stat fps", "r.SetRes 1920x1080", "viewmode lit"). Dangerous commands like quit/exit and crash triggers are blocked. Required.' }
      },
      required: ['command']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean', description: 'Whether the command executed' },
        command: { type: 'string', description: 'The command that was executed' },
        result: { type: 'object', description: 'Command execution result' },
        warning: { type: 'string', description: 'Warning if command may be unrecognized' },
        info: { type: 'string', description: 'Additional information' },
        error: { type: 'string', description: 'Error message if failed' }
      }
    }
  },


  // 11. SEQUENCER / CINEMATICS
  {
    name: 'manage_sequence',
    description: `Sequencer automation helper for Level Sequences: asset management, bindings, and playback control.

Use it when you need to:
- create or open a sequence asset.
- add actors, spawnable cameras, or other bindings.
- adjust sequence metadata (frame rate, bounds, playback window).
- drive playback (play/pause/stop), adjust speed, or fetch binding info.

Supported actions: create, open, add_camera, add_actor, add_actors, remove_actors, get_bindings, add_spawnable_from_class, play, pause, stop, get_properties, set_playback_speed, list, duplicate, rename, delete, get_metadata.`,
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'create', 'open', 'add_camera', 'add_actor', 'add_actors',
            'remove_actors', 'get_bindings', 'add_spawnable_from_class',
            'play', 'pause', 'stop', 'get_properties', 'set_playback_speed',
            'list', 'duplicate', 'rename', 'delete', 'get_metadata', 'set_metadata'
          ],
          description: 'Sequence action'
        },
        name: { type: 'string', description: 'Name for new Level Sequence asset. Required for create action.' },
        path: { type: 'string', description: 'Content path - for create action: save location (e.g., "/Game/Cinematics"); for open/operations: full asset path (e.g., "/Game/Cinematics/MySequence"). Required for create and open actions.' },
        metadata: { type: 'object', additionalProperties: true, description: 'Arbitrary metadata map for set_metadata action targeting the sequence asset.' },
        actorName: { type: 'string', description: 'Actor label/name in level to add as possessable binding to sequence. Required for add_actor action.' },
        actorNames: { type: 'array', items: { type: 'string' }, description: 'Array of actor labels/names for batch add or remove operations. Required for add_actors and remove_actors actions.' },
        className: { type: 'string', description: 'Unreal class name for spawnable actor (e.g., "StaticMeshActor", "CineCameraActor", "SkeletalMeshActor"). Required for add_spawnable_from_class action.' },
        spawnable: { type: 'boolean', description: 'If true, camera is spawnable (owned by sequence); if false, camera is possessable (references level actor). Optional for add_camera, defaults to true.' },
        frameRate: { type: 'number', description: 'Sequence frame rate in frames per second (e.g., 24, 30, 60). (Not currently implemented by plugin; for future use.)' },
        lengthInFrames: { type: 'number', description: 'Total sequence length measured in frames. (Not currently implemented by plugin; for future use.)' },
        playbackStart: { type: 'number', description: 'First frame of playback range (inclusive). (Not currently implemented by plugin; for future use.)' },
        playbackEnd: { type: 'number', description: 'Last frame of playback range (inclusive). (Not currently implemented by plugin; for future use.)' },
        speed: { type: 'number', description: 'Playback speed multiplier. 1.0 is normal speed, 2.0 is double speed, 0.5 is half speed. Required for set_playback_speed action.' },
        loopMode: { type: 'string', enum: ['once', 'loop', 'pingpong'], description: 'How sequence loops. (Not currently implemented by plugin; for future use.)' },
        destinationPath: { type: 'string', description: 'Destination content folder for duplicate action (e.g., "/Game/Cinematics/Copies"). Required for duplicate.' },
        newName: { type: 'string', description: 'New asset name for duplicate/rename actions.' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        sequencePath: { type: 'string' },
        cameraBindingId: { type: 'string' },
        bindings: { type: 'array', items: { type: 'object' } },
        actorsAdded: { type: 'array', items: { type: 'string' } },
        removedActors: { type: 'array', items: { type: 'string' } },
        notFound: { type: 'array', items: { type: 'string' } },
        spawnableId: { type: 'string' },
        frameRate: { type: 'object' },
        playbackStart: { type: 'number' },
        playbackEnd: { type: 'number' },
        duration: { type: 'number' },
        playbackSpeed: { type: 'number' },
        message: { type: 'string' },
        error: { type: 'string' }
      }
    }
  },

  // 12. INTROSPECTION
  {
    name: 'inspect',
    description: `Introspection utility for reading or mutating properties on actors, components, or CDOs.

Use it when you need to:
- inspect an object by path and retrieve its serialized properties.
- set a property value with built-in validation.
- get or set component properties.
- manage actor tags and snapshots.
- export actors or find objects by class.

Supported actions: inspect_object, set_property, get_property, get_components, get_component_property, set_component_property, get_metadata, add_tag, find_by_tag, create_snapshot, restore_snapshot, export, delete_object, list_objects, find_by_class, get_bounding_box, inspect_class.`,
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'inspect_object',
            'set_property',
            'get_property',
            'get_components',
            'get_component_property',
            'set_component_property',
            'get_metadata',
            'add_tag',
            'find_by_tag',
            'create_snapshot',
            'restore_snapshot',
            'export',
            'delete_object',
            'list_objects',
            'find_by_class',
            'get_bounding_box',
            'inspect_class'
          ],
          description: 'Introspection action.'
        },
        objectPath: { type: 'string', description: 'Full object path in Unreal format. Required for most actions.' },
        propertyName: { type: 'string', description: 'Name of the property to modify or retrieve.' },
        value: { description: 'New property value for set actions.' },
        actorName: { type: 'string', description: 'Name of the actor for actor-specific actions.' },
        tag: { type: 'string', description: 'Tag to add or search for.' },
        snapshotName: { type: 'string', description: 'Name of the snapshot to create or restore.' },
        className: { type: 'string', description: 'Class name for finding objects or inspecting class.' },
        transport: {
          type: 'string',
          enum: ['automation_bridge'],
          description: 'Transport for property mutations. Only automation_bridge is supported; omit to use the default.'
        },
        timeoutMs: {
          type: 'number',
          description: 'Optional timeout in milliseconds.'
        },
        markDirty: {
          type: 'boolean',
          description: 'When false, skips marking the target package dirty (automation bridge transport only).'
        }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        info: { type: 'object' },
        message: { type: 'string' },
        error: { type: 'string' },
        value: {},
        transport: { type: 'string' },
        bridge: { type: 'object' }
      }
    }
  },

  // 13. WORLD PARTITION
  {
    name: 'manage_world_partition',
    description: `World Partition management helper for loading cells and managing data layers.

Use it when you need to:
- load a specific region of the world.
- manage data layers (activate, deactivate, etc.).

Supported actions: load_cells, set_datalayer.`,
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: ['load_cells', 'set_datalayer'],
          description: 'World Partition action'
        },
        // load_cells
        min: { type: 'array', items: { type: 'number' }, description: 'Min bounds [x, y, z]' },
        max: { type: 'array', items: { type: 'number' }, description: 'Max bounds [x, y, z]' },
        origin: { type: 'array', items: { type: 'number' }, description: 'Origin [x, y, z]' },
        extent: { type: 'array', items: { type: 'number' }, description: 'Extent [x, y, z]' },
        // set_datalayer
        dataLayerLabel: { type: 'string', description: 'Data Layer label' },
        dataLayerState: { type: 'string', enum: ['Activated', 'Deactivated', 'Loaded', 'Unloaded'], description: 'Data Layer state' },
        recursive: { type: 'boolean', description: 'Recursive operation' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        message: { type: 'string' },
        error: { type: 'string' }
      }
    }
  },

  // 14. RENDER
  {
    name: 'manage_render',
    description: `Render management helper for render targets, Nanite, and Lumen.

Use it when you need to:
- create or attach render targets.
- rebuild Nanite meshes.
- update Lumen scene.

Supported actions: create_render_target, attach_render_target_to_volume, nanite_rebuild_mesh, lumen_update_scene.`,
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: ['create_render_target', 'attach_render_target_to_volume', 'nanite_rebuild_mesh', 'lumen_update_scene'],
          description: 'Render action'
        },
        // create_render_target
        assetPath: { type: 'string', description: 'Path to create render target' },
        width: { type: 'number', description: 'Width' },
        height: { type: 'number', description: 'Height' },
        format: { type: 'string', description: 'Pixel format' },
        // attach_render_target_to_volume
        volumeName: { type: 'string', description: 'Post Process Volume actor name' },
        materialPath: { type: 'string', description: 'Material path' },
        parameterName: { type: 'string', description: 'Texture parameter name' },
        // nanite_rebuild_mesh
        meshPath: { type: 'string', description: 'Static Mesh path' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        message: { type: 'string' },
        error: { type: 'string' }
      }
    }
  },

  // 15. PIPELINE
  {
    name: 'manage_pipeline',
    description: `Pipeline management helper for running UBT and other external processes.

Use it when you need to:
- run UnrealBuildTool.

Supported actions: run_ubt.`,
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: ['run_ubt'],
          description: 'Pipeline action'
        },
        target: { type: 'string', description: 'Build target' },
        platform: { type: 'string', description: 'Build platform' },
        configuration: { type: 'string', description: 'Build configuration' },
        arguments: { type: 'string', description: 'Additional arguments' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        message: { type: 'string' },
        error: { type: 'string' }
      }
    }
  },

  // 16. TESTS
  {
    name: 'manage_tests',
    description: `Test management helper for running automated tests.

Use it when you need to:
- run automated tests.

Supported actions: run_tests.`,
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: ['run_tests'],
          description: 'Test action'
        },
        filter: { type: 'string', description: 'Test filter' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        message: { type: 'string' },
        error: { type: 'string' }
      }
    }
  },

  // 17. LOGS
  {
    name: 'manage_logs',
    description: `Log management helper for subscribing to logs.

Use it when you need to:
- subscribe to log output.

Supported actions: subscribe.`,
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: ['subscribe'],
          description: 'Log action'
        }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        message: { type: 'string' },
        error: { type: 'string' }
      }
    }
  },

  // 18. DEBUG
  {
    name: 'manage_debug',
    description: `Debug management helper for gameplay debugger.

Use it when you need to:
- spawn gameplay debugger categories.

Supported actions: spawn_category.`,
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: ['spawn_category'],
          description: 'Debug action'
        },
        category: { type: 'string', description: 'Gameplay debugger category' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        message: { type: 'string' },
        error: { type: 'string' }
      }
    }
  },

  // 19. INSIGHTS
  {
    name: 'manage_insights',
    description: `Insights management helper for profiling sessions.

Use it when you need to:
- start a profiling session.

Supported actions: start_session.`,
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: ['start_session'],
          description: 'Insights action'
        },
        channels: { type: 'string', description: 'Trace channels' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        message: { type: 'string' },
        error: { type: 'string' }
      }
    }
  },

  // 20. UI
  {
    name: 'manage_ui',
    description: `UI management helper for simulating input.

Use it when you need to:
- simulate keyboard input.

Supported actions: simulate_input.`,
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: ['simulate_input'],
          description: 'UI action'
        },
        keyName: { type: 'string', description: 'Key name (e.g., "A", "SpaceBar")' },
        eventType: { type: 'string', enum: ['KeyDown', 'KeyUp', 'Both'], description: 'Event type' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        message: { type: 'string' },
        error: { type: 'string' }
      }
    }
  },

  // 21. BLUEPRINT GRAPH
  {
    name: 'manage_blueprint_graph',
    description: `Blueprint graph management helper for editing nodes and pins.

Use it when you need to:
- delete nodes.
- set node properties.
- create reroute nodes.
- get node/graph/pin details.

Supported actions: delete_node, set_node_property, create_reroute_node, get_node_details, get_graph_details, get_pin_details.`,
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: ['create_node', 'delete_node', 'connect_pins', 'break_pin_links', 'set_node_property', 'create_reroute_node', 'get_node_details', 'get_graph_details', 'get_pin_details'],
          description: 'Blueprint graph action'
        },
        blueprintPath: { type: 'string', description: 'Blueprint asset path' },
        graphName: { type: 'string', description: 'Graph name' },
        nodeType: { type: 'string', description: 'Node type for create_node (e.g., CallFunction, VariableGet)' },
        memberName: { type: 'string', description: 'Member name for CallFunction/Variable nodes' },
        nodeId: { type: 'string', description: 'Node ID' },
        propertyName: { type: 'string', description: 'Property name' },
        value: { description: 'Property value' },
        pinName: { type: 'string', description: 'Pin name' },
        linkedTo: { type: 'string', description: 'Pin to connect to (for connect_pins)' },
        x: { type: 'number', description: 'X position' },
        y: { type: 'number', description: 'Y position' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        message: { type: 'string' },
        error: { type: 'string' }
      }
    }
  },

  // 22. NIAGARA GRAPH
  {
    name: 'manage_niagara_graph',
    description: `Niagara graph management helper.

Use it when you need to:
- add modules.
- connect pins.
- remove nodes.
- set parameters.

Supported actions: add_module, connect_pins, remove_node, set_parameter.`,
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: ['add_module', 'connect_pins', 'remove_node', 'set_parameter'],
          description: 'Niagara graph action'
        },
        assetPath: { type: 'string', description: 'Niagara system asset path' },
        modulePath: { type: 'string', description: 'Module path for add_module' },
        emitterName: { type: 'string', description: 'Emitter name' },
        nodeId: { type: 'string', description: 'Node ID' },
        pinName: { type: 'string', description: 'Pin name' },
        linkedTo: { type: 'string', description: 'Linked pin name' },
        parameterName: { type: 'string', description: 'Parameter name' },
        value: { description: 'Parameter value' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        message: { type: 'string' },
        error: { type: 'string' }
      }
    }
  },

  // 23. MATERIAL GRAPH
  {
    name: 'manage_material_graph',
    description: `Material graph management helper.

Use it when you need to:
- remove nodes.
- break connections.
- get node details.

Supported actions: remove_node, break_connections, get_node_details.`,
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: ['add_node', 'connect_pins', 'rebuild', 'remove_node', 'break_connections', 'get_node_details'],
          description: 'Material graph action'
        },
        assetPath: { type: 'string', description: 'Material asset path' },
        nodeType: { type: 'string', description: 'Node type for add_node (e.g. ScalarParameter)' },
        nodeId: { type: 'string', description: 'Node ID' },
        fromNodeId: { type: 'string', description: 'Source node ID' },
        fromPin: { type: 'string', description: 'Source pin name' },
        toNodeId: { type: 'string', description: 'Target node ID' },
        toPin: { type: 'string', description: 'Target pin name' },
        parameterName: { type: 'string', description: 'Parameter name for new node' },
        value: { type: 'number', description: 'Value for new node' },
        x: { type: 'number', description: 'X position' },
        y: { type: 'number', description: 'Y position' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        message: { type: 'string' },
        error: { type: 'string' }
      }
    }
  },

  // 24. BEHAVIOR TREE
  {
    name: 'manage_behavior_tree',
    description: `Behavior Tree management helper.

Use it when you need to:
- remove nodes.
- break connections.
- set node properties.

Supported actions: remove_node, break_connections, set_node_properties.`,
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: ['add_node', 'connect_nodes', 'remove_node', 'break_connections', 'set_node_properties'],
          description: 'Behavior Tree action'
        },
        assetPath: { type: 'string', description: 'Behavior Tree asset path' },
        nodeType: { type: 'string', description: 'Node type for add_node (Sequence, Selector, Wait)' },
        nodeId: { type: 'string', description: 'Node ID' },
        parentNodeId: { type: 'string', description: 'Parent node ID for connect_nodes' },
        childNodeId: { type: 'string', description: 'Child node ID for connect_nodes' },
        comment: { type: 'string', description: 'Comment for set_node_properties' },
        x: { type: 'number', description: 'X position' },
        y: { type: 'number', description: 'Y position' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        message: { type: 'string' },
        error: { type: 'string' }
      }
    }
  }
];
