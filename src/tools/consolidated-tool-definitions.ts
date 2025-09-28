// Consolidated tool definitions - reduced from 36 to 13 multi-purpose tools

export const consolidatedToolDefinitions = [
  // 1. ASSET MANAGER - Combines asset operations
  {
    name: 'manage_asset',
  description: `Asset library utility for browsing, importing, and bootstrapping simple materials.

Use it when you need to:
- explore project content (\u002fContent automatically maps to \u002fGame).
- import FBX/PNG/WAV/EXR files into the project.
- spin up a minimal Material asset at a specific path.

Supported actions: list, import, create_material.`,
    inputSchema: {
      type: 'object',
      properties: {
        action: { 
          type: 'string', 
          enum: ['list', 'import', 'create_material'],
          description: 'Action to perform'
        },
        // For list
        directory: { type: 'string', description: 'Directory path to list (shows immediate children only)' },
        // For import
        sourcePath: { type: 'string', description: 'Source file path' },
        destinationPath: { type: 'string', description: 'Destination path' },
        // For create_material
        name: { type: 'string', description: 'Asset name' },
        path: { type: 'string', description: 'Save path' }
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
        message: { type: 'string', description: 'Status message' },
        error: { type: 'string', description: 'Error message if failed' }
      }
    }
  },

  // 2. ACTOR CONTROL - Combines actor operations
  {
    name: 'control_actor',
  description: `Viewport actor toolkit for spawning, removing, or nudging actors with physics forces.

Use it when you need to:
- drop a class or mesh into the level (classPath accepts names or asset paths).
- delete actors by label, case-insensitively.
- push a physics-enabled actor with a world-space force vector.

Supported actions: spawn, delete, apply_force.`,
    inputSchema: {
      type: 'object',
      properties: {
        action: { 
          type: 'string', 
          enum: ['spawn', 'delete', 'apply_force'],
          description: 'Action to perform'
        },
        // Common
        actorName: { type: 'string', description: 'Actor name (optional for spawn, auto-generated if not provided)' },
        classPath: { 
          type: 'string', 
          description: 'Actor class (e.g., "StaticMeshActor", "CameraActor") OR asset path (e.g., "/Engine/BasicShapes/Cube", "/Game/MyMesh"). Asset paths will automatically spawn as StaticMeshActor with the mesh applied'
        },
        // Transform
        location: {
          type: 'object',
          properties: {
            x: { type: 'number' },
            y: { type: 'number' },
            z: { type: 'number' }
          }
        },
        rotation: {
          type: 'object',
          properties: {
            pitch: { type: 'number' },
            yaw: { type: 'number' },
            roll: { type: 'number' }
          }
        },
        // Physics
        force: {
          type: 'object',
          properties: {
            x: { type: 'number' },
            y: { type: 'number' },
            z: { type: 'number' }
          }
        }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean', description: 'Whether the operation succeeded' },
        actor: { type: 'string', description: 'Spawned actor name (for spawn)' },
        deleted: { type: 'string', description: 'Deleted actor name (for delete)' },
        physicsEnabled: { type: 'boolean', description: 'Physics state (for apply_force)' },
        message: { type: 'string', description: 'Status message' },
        error: { type: 'string', description: 'Error message if failed' }
      }
    }
  },

  // 3. EDITOR CONTROL - Combines editor operations
  {
    name: 'control_editor',
  description: `Editor session controls for PIE playback, camera placement, and view modes.

Use it when you need to:
- start or stop Play In Editor.
- reposition the active viewport camera.
- switch between Lit/Unlit/Wireframe and other safe view modes.

Supported actions: play, stop, set_camera, set_view_mode (with validation).`,
    inputSchema: {
      type: 'object',
      properties: {
        action: { 
          type: 'string', 
          enum: ['play', 'stop', 'set_camera', 'set_view_mode'],
          description: 'Editor action'
        },
        // Camera
        location: {
          type: 'object',
          properties: {
            x: { type: 'number' },
            y: { type: 'number' },
            z: { type: 'number' }
          }
        },
        rotation: {
          type: 'object',
          properties: {
            pitch: { type: 'number' },
            yaw: { type: 'number' },
            roll: { type: 'number' }
          }
        },
        // View mode
        viewMode: { 
          type: 'string', 
          description: 'View mode (Lit, Unlit, Wireframe, etc.)'
        }
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
- kick off a lighting build at a chosen quality.

Supported actions: load, save, stream, create_light, build_lighting.`,
    inputSchema: {
      type: 'object',
      properties: {
        action: { 
          type: 'string', 
          enum: ['load', 'save', 'stream', 'create_light', 'build_lighting'],
          description: 'Level action'
        },
        // Level
        levelPath: { type: 'string', description: 'Level path' },
        levelName: { type: 'string', description: 'Level name' },
        streaming: { type: 'boolean', description: 'Use streaming' },
        shouldBeLoaded: { type: 'boolean', description: 'Load or unload' },
        shouldBeVisible: { type: 'boolean', description: 'Visibility' },
        // Lighting
        lightType: { 
          type: 'string', 
          enum: ['Directional', 'Point', 'Spot', 'Rect'],
          description: 'Light type'
        },
        name: { type: 'string', description: 'Object name' },
        location: {
          type: 'object',
          properties: {
            x: { type: 'number' },
            y: { type: 'number' },
            z: { type: 'number' }
          }
        },
        intensity: { type: 'number', description: 'Light intensity' },
        quality: { 
          type: 'string',
          enum: ['Preview', 'Medium', 'High', 'Production'],
          description: 'Build quality'
        }
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
  description: `Animation and physics rigging helper covering Anim BPs, montage playback, and ragdoll setup.

Use it when you need to:
- generate an Animation Blueprint for a skeleton.
- play a montage/animation on an actor at a chosen rate.
- enable a quick ragdoll using an existing physics asset.

Supported actions: create_animation_bp, play_montage, setup_ragdoll.`,
    inputSchema: {
      type: 'object',
      properties: {
        action: { 
          type: 'string', 
          enum: ['create_animation_bp', 'play_montage', 'setup_ragdoll'],
          description: 'Action type'
        },
        // Common
        name: { type: 'string', description: 'Asset name' },
        actorName: { type: 'string', description: 'Actor name' },
        // Animation
        skeletonPath: { type: 'string', description: 'Skeleton path' },
        montagePath: { type: 'string', description: 'Montage path' },
        animationPath: { type: 'string', description: 'Animation path' },
        playRate: { type: 'number', description: 'Play rate' },
        // Physics
        physicsAssetName: { type: 'string', description: 'Physics asset' },
        blendWeight: { type: 'number', description: 'Blend weight' },
        savePath: { type: 'string', description: 'Save location' }
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
          enum: ['particle', 'niagara', 'debug_shape'],
          description: 'Effect type'
        },
        // Common
        name: { type: 'string', description: 'Effect name' },
        location: {
          type: 'object',
          properties: {
            x: { type: 'number' },
            y: { type: 'number' },
            z: { type: 'number' }
          }
        },
        // Particles
        effectType: { 
          type: 'string',
          description: 'Effect type (Fire, Smoke, Water, etc.)'
        },
        systemPath: { type: 'string', description: 'Niagara system path' },
        scale: { type: 'number', description: 'Scale factor' },
        // Debug
        shape: { 
          type: 'string',
          description: 'Debug shape (Line, Box, Sphere, etc.)'
        },
        size: { type: 'number', description: 'Size/radius' },
        color: {
          type: 'array',
          items: { type: 'number' },
          description: 'RGBA color'
        },
        duration: { type: 'number', description: 'Duration' }
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
  description: `Blueprint scaffolding helper for creating assets and attaching components.

Use it when you need to:
- create a new Blueprint of a specific base type (Actor, Pawn, Character, ...).
- add a component to an existing Blueprint asset with a unique name.

Supported actions: create, add_component.`,
    inputSchema: {
      type: 'object',
      properties: {
        action: { 
          type: 'string', 
          enum: ['create', 'add_component'],
          description: 'Blueprint action'
        },
        name: { type: 'string', description: 'Blueprint name' },
        blueprintType: { 
          type: 'string',
          description: 'Type (Actor, Pawn, Character, etc.)'
        },
        componentType: { type: 'string', description: 'Component type' },
        componentName: { type: 'string', description: 'Component name' },
        savePath: { type: 'string', description: 'Save location' }
      },
      required: ['action', 'name']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean', description: 'Whether the operation succeeded' },
        blueprintPath: { type: 'string', description: 'Blueprint asset path' },
        componentAdded: { type: 'string', description: 'Added component name' },
        message: { type: 'string', description: 'Status message' },
        warning: { type: 'string', description: 'Warning if manual steps needed' }
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

Supported actions: create_landscape, sculpt, add_foliage, paint_foliage, create_procedural_terrain, create_procedural_foliage, add_foliage_instances, create_landscape_grass_type.`,
    inputSchema: {
      type: 'object',
      properties: {
        action: { 
          type: 'string', 
          enum: ['create_landscape', 'sculpt', 'add_foliage', 'paint_foliage', 'create_procedural_terrain', 'create_procedural_foliage', 'add_foliage_instances', 'create_landscape_grass_type'],
          description: 'Environment action'
        },
        // Common
        name: { type: 'string', description: 'Object name' },
        // Landscape
        sizeX: { type: 'number', description: 'Landscape size X' },
        sizeY: { type: 'number', description: 'Landscape size Y' },
        tool: { 
          type: 'string',
          description: 'Sculpt tool (Sculpt, Smooth, Flatten, etc.)'
        },
        // Advanced: procedural terrain
        location: {
          type: 'object',
          properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } }
        },
        subdivisions: { type: 'number' },
        heightFunction: { type: 'string' },
        materialPath: { type: 'string' },
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
        meshPath: { type: 'string', description: 'Mesh path' },
        density: { type: 'number', description: 'Density' },
        // Painting
        position: {
          type: 'object',
          properties: {
            x: { type: 'number' },
            y: { type: 'number' },
            z: { type: 'number' }
          }
        },
        brushSize: { type: 'number', description: 'Brush size' },
        strength: { type: 'number', description: 'Tool strength' }
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
  description: `Runtime/system controls for profiling, quality tiers, audio/UI triggers, screenshots, and editor lifecycle.

Use it when you need to:
- toggle stat overlays or targeted profilers.
- adjust scalability categories (sg.*) or enable FPS display.
- play a one-shot sound and optionally position it.
- create/show lightweight widgets.
- capture a screenshot or start/quit the editor process.

Supported actions: profile, show_fps, set_quality, play_sound, create_widget, show_widget, screenshot, engine_start, engine_quit.`,
    inputSchema: {
      type: 'object',
      properties: {
        action: { 
          type: 'string', 
          enum: ['profile', 'show_fps', 'set_quality', 'play_sound', 'create_widget', 'show_widget', 'screenshot', 'engine_start', 'engine_quit'],
          description: 'System action'
        },
        // Performance
        profileType: { 
          type: 'string',
          description: 'Profile type (CPU, GPU, Memory)'
        },
        category: { 
          type: 'string',
          description: 'Quality category (Shadows, Textures, etc.)'
        },
        level: { type: 'number', description: 'Quality level (0-4)' },
        enabled: { type: 'boolean', description: 'Enable/disable' },
        verbose: { type: 'boolean', description: 'Verbose output' },
        // Audio
        soundPath: { type: 'string', description: 'Sound asset path' },
        location: {
          type: 'object',
          properties: {
            x: { type: 'number' },
            y: { type: 'number' },
            z: { type: 'number' }
          }
        },
        volume: { type: 'number', description: 'Volume (0-1)' },
        is3D: { type: 'boolean', description: '3D sound' },
        // UI
        widgetName: { type: 'string', description: 'Widget name' },
        widgetType: { 
          type: 'string',
          description: 'Widget type (HUD, Menu, etc.)'
        },
        visible: { type: 'boolean', description: 'Visibility' },
        // Screenshot
        resolution: { type: 'string', description: 'e.g. 1920x1080' },
        // Engine lifecycle
        projectPath: { type: 'string', description: 'Path to .uproject (for engine_start, optional if UE_PROJECT_PATH env set)' },
        editorExe: { type: 'string', description: 'Path to UE Editor executable (optional if UE_EDITOR_EXE env set)' }
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
        command: { type: 'string', description: 'Console command to execute' }
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

  // 11. REMOTE CONTROL PRESETS
  {
    name: 'manage_rc',
  description: `Remote Control preset helper for building, exposing, and mutating RC assets.

Use it when you need to:
- create a preset asset on disk.
- expose actors or object properties to the preset.
- list the exposed fields for inspection.
- get or set property values through RC with JSON-serializable payloads.

Supported actions: create_preset, expose_actor, expose_property, list_fields, set_property, get_property.`,
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: ['create_preset', 'expose_actor', 'expose_property', 'list_fields', 'set_property', 'get_property'],
          description: 'RC action'
        },
        name: { type: 'string', description: 'Preset or entity name' },
        path: { type: 'string', description: 'Preset save path (e.g. /Game/RCPresets)' },
        presetPath: { type: 'string', description: 'Preset asset path (e.g. /Game/RCPresets/MyPreset)' },
        actorName: { type: 'string', description: 'Actor label/name to expose' },
        objectPath: { type: 'string', description: 'Object path for property get/set' },
        propertyName: { type: 'string', description: 'Property name for remote property set/get' },
        value: { description: 'Value for property set (JSON-serializable)' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        message: { type: 'string' },
        presetPath: { type: 'string' },
        fields: { type: 'array', items: { type: 'object' } },
        value: {},
        error: { type: 'string' }
      }
    }
  },

  // 12. SEQUENCER / CINEMATICS
  {
    name: 'manage_sequence',
  description: `Sequencer automation helper for Level Sequences: asset management, bindings, and playback control.

Use it when you need to:
- create or open a sequence asset.
- add actors, spawnable cameras, or other bindings.
- adjust sequence metadata (frame rate, bounds, playback window).
- drive playback (play/pause/stop), adjust speed, or fetch binding info.

Supported actions: create, open, add_camera, add_actor, add_actors, remove_actors, get_bindings, add_spawnable_from_class, play, pause, stop, set_properties, get_properties, set_playback_speed.`,
    inputSchema: {
      type: 'object',
      properties: {
        action: { 
          type: 'string', 
          enum: [
            'create', 'open', 'add_camera', 'add_actor', 'add_actors', 
            'remove_actors', 'get_bindings', 'add_spawnable_from_class',
            'play', 'pause', 'stop', 'set_properties', 'get_properties', 'set_playback_speed'
          ], 
          description: 'Sequence action' 
        },
        name: { type: 'string', description: 'Sequence name (for create)' },
        path: { type: 'string', description: 'Save path (for create), or asset path (for open/operations)' },
        actorName: { type: 'string', description: 'Actor name to add as possessable' },
        actorNames: { type: 'array', items: { type: 'string' }, description: 'Multiple actor names for batch operations' },
        className: { type: 'string', description: 'Class name for spawnable (e.g. StaticMeshActor, CineCameraActor)' },
        spawnable: { type: 'boolean', description: 'If true, camera is spawnable' },
        frameRate: { type: 'number', description: 'Frame rate for sequence' },
        lengthInFrames: { type: 'number', description: 'Total length in frames' },
        playbackStart: { type: 'number', description: 'Playback start frame' },
        playbackEnd: { type: 'number', description: 'Playback end frame' },
        speed: { type: 'number', description: 'Playback speed multiplier' },
        loopMode: { type: 'string', enum: ['once', 'loop', 'pingpong'], description: 'Playback loop mode' }
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

  // 13. INTROSPECTION
  {
    name: 'inspect',
  description: `Introspection utility for reading or mutating properties on actors, components, or CDOs.

Use it when you need to:
- inspect an object by path and retrieve its serialized properties.
- set a property value with built-in validation.

Supported actions: inspect_object, set_property.`,
    inputSchema: {
      type: 'object',
      properties: {
        action: { type: 'string', enum: ['inspect_object', 'set_property'], description: 'Inspection action' },
        objectPath: { type: 'string', description: 'Object path' },
        propertyName: { type: 'string', description: 'Property to set/get' },
        value: { description: 'Value to set (JSON-serializable)' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        info: { type: 'object' },
        message: { type: 'string' },
        error: { type: 'string' }
      }
    }
  }
];
