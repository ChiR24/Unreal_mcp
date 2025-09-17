// Consolidated tool definitions - reduced from 36 to 13 multi-purpose tools

export const consolidatedToolDefinitions = [
  // 1. ASSET MANAGER - Combines asset operations
  {
    name: 'manage_asset',
    description: `Search, browse, import, and create simple material assets.

When to use this tool:
- You want to list assets in the project Content directory (use /Game; /Content is auto-mapped).
- You want to import files from disk into the project (e.g., FBX, PNG, WAV, EXR).
- You want to generate a very basic Material asset by name at a path.

Supported actions:
- list: Returns assets in a folder (recursive behavior is auto-enabled for /Game).
- import: Imports a file into the project at a destination path (e.g., /Game/Folder).
- create_material: Creates a simple Material asset at a path.

Tips:
- Unreal uses /Game for project content; this server maps /Content → /Game automatically.
- For large projects, listing /Game returns a sample subset for speed; refine to subfolders.

Examples:
- {"action":"list","directory":"/Game/ThirdPerson"}
- {"action":"import","sourcePath":"C:/Temp/Tree.fbx","destinationPath":"/Game/Environment/Trees"}
- {"action":"create_material","name":"M_Mask","path":"/Game/Materials"}`,
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
    description: `Spawn, delete, and apply physics to actors in the level.

When to use this tool:
- Place an actor/mesh, remove an actor, or nudge an actor with a physics force.

Supported actions:
- spawn
- delete
- apply_force

Spawning:
- classPath can be a class name (e.g., StaticMeshActor, CameraActor) OR an asset path (e.g., /Engine/BasicShapes/Cube, /Game/Meshes/SM_Rock).
- Asset paths auto-spawn StaticMeshActor with the mesh assigned.

Deleting:
- Finds actors by label/name (case-insensitive) and deletes matches.

Apply force:
- Applies a world-space force vector to an actor with physics enabled.

Tips:
- classPath accepts classes or asset paths; simple names like Cube auto-resolve to engine assets.
- location/rotation are optional; defaults are used if omitted.
- For delete/apply_force, provide actorName.

Examples:
- {"action":"spawn","classPath":"/Engine/BasicShapes/Cube","location":{"x":0,"y":0,"z":100}}
- {"action":"delete","actorName":"Cube_1"}
- {"action":"apply_force","actorName":"PhysicsBox","force":{"x":0,"y":0,"z":5000}}`,
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
    description: `Play/Stop PIE, position the editor camera, and switch common view modes.

When to use this tool:
- Start/stop a PIE session, move the viewport camera, or change viewmode (Lit/Unlit/Wireframe/etc.).

Supported actions:
- play
- stop
- set_camera
- set_view_mode

Notes:
- View modes are validated; unsafe modes are blocked.
- Camera accepts location/rotation (optional); values normalized.

Examples:
- {"action":"play"}
- {"action":"set_camera","location":{"x":0,"y":-600,"z":250},"rotation":{"pitch":0,"yaw":0,"roll":0}}
- {"action":"set_view_mode","viewMode":"Wireframe"}
- {"action":"stop"}`,
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
    description: `Load/save/stream levels, create lights, and trigger lighting builds.

When to use this tool:
- Switch to a level, save the current level, stream sublevels, add a light, or start a lighting build.

Supported actions:
- load
- save
- stream
- create_light
- build_lighting

Tips:
- Use /Game paths for levels (e.g., /Game/Maps/Level).
- For streaming, set shouldBeLoaded and shouldBeVisible accordingly.
- For lights, provide lightType and optional location/intensity.

Examples:
- {"action":"load","levelPath":"/Game/Maps/Lobby"}
- {"action":"stream","levelName":"Sublevel_A","shouldBeLoaded":true,"shouldBeVisible":true}
- {"action":"create_light","lightType":"Directional","name":"KeyLight","intensity":5.0}
- {"action":"build_lighting","quality":"High"}`,
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
    description: `Create animation blueprints, play montages, and set up simple ragdolls.

When to use this tool:
- Generate an Anim Blueprint for a skeleton, play a Montage/Animation on an actor, or enable ragdoll.

Supported actions:
- create_animation_bp
- play_montage
- setup_ragdoll

Tips:
- Ensure the montage/animation is compatible with the target actor/skeleton.
- setup_ragdoll requires a valid physicsAssetName on the skeleton.
- Use savePath when creating new assets.

Examples:
- {"action":"create_animation_bp","name":"ABP_Hero","skeletonPath":"/Game/Characters/Hero/SK_Hero_Skeleton","savePath":"/Game/Characters/Hero"}
- {"action":"play_montage","actorName":"Hero","montagePath":"/Game/Anim/MT_Attack"}
- {"action":"setup_ragdoll","skeletonPath":"/Game/Characters/Hero/SK_Hero_Skeleton","physicsAssetName":"PHYS_Hero"}`,
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
    description: `Create particles/FX and lightweight debug shapes for rapid iteration.

When to use this tool:
- Spawn a Niagara system at a location, create a particle effect by type tag, or draw debug geometry for planning.

Supported actions:
- particle
- niagara
- debug_shape

Tips:
- Set color as RGBA [r,g,b,a]; scale defaults to 1 if omitted.
- Use debug shapes for quick layout planning and measurements.

Examples:
- {"action":"niagara","systemPath":"/Game/FX/NS_Explosion","location":{"x":0,"y":0,"z":200},"scale":1.0}
- {"action":"particle","effectType":"Smoke","name":"SMK1","location":{"x":100,"y":0,"z":50}}
- {"action":"debug_shape","shape":"Sphere","location":{"x":0,"y":0,"z":0},"size":100,"duration":5}`,
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
    description: `Create new Blueprints and add components programmatically.

When to use this tool:
- Quickly scaffold a Blueprint asset or add a component to an existing Blueprint.

Supported actions:
- create
- add_component

Tips:
- blueprintType can be Actor, Pawn, Character, etc.
- Component names should be unique within the Blueprint.

Examples:
- {"action":"create","name":"BP_Switch","blueprintType":"Actor","savePath":"/Game/Blueprints"}
- {"action":"add_component","name":"BP_Switch","componentType":"PointLightComponent","componentName":"KeyLight"}`,
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
    description: `Environment authoring helpers (landscape, foliage).

When to use this tool:
- Create a procedural terrain alternative, add/paint foliage, or attempt a landscape workflow.

Supported actions:
- create_landscape
- sculpt
- add_foliage
- paint_foliage

Important:
- Native Landscape creation via Python is limited and may return a helpful error suggesting Landscape Mode in the editor.
- Foliage helpers create FoliageType assets and support simple placement.

Tips:
- Adjust brushSize and strength to tune sculpting results.

Examples:
- {"action":"create_landscape","name":"Landscape_Basic","sizeX":1024,"sizeY":1024}
- {"action":"add_foliage","name":"FT_Grass","meshPath":"/Game/Foliage/SM_Grass","density":300}
- {"action":"paint_foliage","foliageType":"/Game/Foliage/Types/FT_Grass","position":{"x":0,"y":0,"z":0},"brushSize":300}`,
    inputSchema: {
      type: 'object',
      properties: {
        action: { 
          type: 'string', 
          enum: ['create_landscape', 'sculpt', 'add_foliage', 'paint_foliage'],
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
        // Foliage
        meshPath: { type: 'string', description: 'Mesh path' },
        foliageType: { type: 'string', description: 'Foliage type' },
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
    description: `Performance toggles, quality settings, audio playback, simple UI helpers, screenshots, and engine lifecycle.

When to use this tool:
- Toggle profiling and FPS stats, adjust quality (sg.*), play a sound, create/show a basic widget, take a screenshot, or launch/quit the editor.

Supported actions:
- profile
- show_fps
- set_quality
- play_sound
- create_widget
- show_widget
- screenshot
- engine_start
- engine_quit

Tips:
- Screenshot resolution format: 1920x1080.
- engine_start can read UE project path from env; provide editorExe/projectPath if needed.

Examples:
- {"action":"show_fps","enabled":true}
- {"action":"set_quality","category":"Shadows","level":2}
- {"action":"play_sound","soundPath":"/Game/Audio/SFX/Click","volume":0.5}
- {"action":"screenshot","resolution":"1920x1080"}`,
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
    description: `Execute an Unreal console command with built-in safety.

When to use this tool:
- Fire a specific command (e.g., stat fps, viewmode wireframe, r.ScreenPercentage 75).

Safety:
- Dangerous commands are blocked (quit/exit, GPU crash triggers, unsafe visualizebuffer modes, etc.).
- Unknown commands will return a warning instead of crashing.

Tips:
- Prefer dedicated tools (system_control, control_editor) when available for structured control.

Examples:
- {"command":"stat fps"}
- {"command":"viewmode wireframe"}
- {"command":"r.ScreenPercentage 75"}`,
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
    description: `Create and manage Remote Control presets; expose actors/properties; set/get values.

When to use this tool:
- Automate Remote Control (RC) preset authoring and interaction from the assistant.

Supported actions:
- create_preset
- expose_actor
- expose_property
- list_fields
- set_property
- get_property

Tips:
- value must be JSON-serializable.
- Use objectPath/presetPath with full asset/object paths.

Examples:
- {"action":"create_preset","name":"LivePreset","path":"/Game/RCPresets"}
- {"action":"expose_actor","presetPath":"/Game/RCPresets/LivePreset","actorName":"CameraActor"}
- {"action":"expose_property","presetPath":"/Game/RCPresets/LivePreset","objectPath":"/Script/Engine.Default__Engine","propertyName":"GameUserSettings"}
- {"action":"list_fields","presetPath":"/Game/RCPresets/LivePreset"}
- {"action":"set_property","objectPath":"/Game/MyActor","propertyName":"CustomFloat","value":0.5}`,
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
    description: `Create/open Level Sequences, bind actors, add cameras, and control playback.

When to use this tool:
- Build quick cinematics: create/open a sequence, add a camera or actors, tweak properties, and play.

Supported actions:
- create
- open
- add_camera
- add_actor
- add_actors
- remove_actors
- get_bindings
- add_spawnable_from_class
- play
- pause
- stop
- set_properties
- get_properties
- set_playback_speed

Tips:
- Set spawnable=true to auto-create a camera actor.
- Use frameRate/lengthInFrames to define timing; use playbackStart/End to trim.

Examples:
- {"action":"create","name":"Intro","path":"/Game/Cinematics"}
- {"action":"add_camera","spawnable":true}
- {"action":"add_actor","actorName":"Hero"}
- {"action":"play","loopMode":"once"}
- {"action":"set_properties","path":"/Game/Cinematics/Intro","frameRate":24,"lengthInFrames":480}`,
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
    description: `Read object info and set properties with validation.

When to use this tool:
- Inspect an object by path (class default object or actor/component) and optionally modify properties.

Supported actions:
- inspect_object
- set_property

Tips:
- propertyName is case-sensitive; ensure it exists on the target object.
- For class default objects (CDOs), use the /Script/...Default__Class path.

Examples:
- {"action":"inspect_object","objectPath":"/Script/Engine.Default__Engine"}
- {"action":"set_property","objectPath":"/Game/MyActor","propertyName":"CustomBool","value":true}`,
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
