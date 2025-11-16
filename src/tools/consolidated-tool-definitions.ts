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
        directory: { type: 'string', description: 'Directory path to list (shows immediate children only). Automatically maps /Content to /Game. Example: "/Game/MyAssets"' },
        // For import
        sourcePath: { type: 'string', description: 'Source file path on disk to import (FBX, PNG, WAV, EXR supported). Example: "C:/MyAssets/mesh.fbx"' },
        destinationPath: { type: 'string', description: 'Destination path in project content where asset will be imported. Example: "/Game/ImportedAssets"' },
        // For create_material
        name: { type: 'string', description: 'Name for the new material asset. Example: "MyMaterial"' },
        path: { type: 'string', description: 'Content path where material will be saved. Example: "/Game/Materials"' }
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
        actorName: { type: 'string', description: 'Actor label/name (optional for spawn, auto-generated if not provided; required for delete). Case-insensitive for delete action.' },
        classPath: { 
          type: 'string', 
          description: 'Actor class (e.g., "StaticMeshActor", "CameraActor") OR asset path (e.g., "/Engine/BasicShapes/Cube", "/Game/MyMesh"). Asset paths will automatically spawn as StaticMeshActor with the mesh applied. Required for spawn action.'
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
        // Physics
        force: {
          type: 'object',
          description: 'Force vector to apply in Newtons. Required for apply_force action. Actor must have physics simulation enabled.',
          properties: {
            x: { type: 'number', description: 'Force magnitude along X-axis' },
            y: { type: 'number', description: 'Force magnitude along Y-axis' },
            z: { type: 'number', description: 'Force magnitude along Z-axis' }
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
        levelPath: { type: 'string', description: 'Full content path to level asset (e.g., "/Game/Maps/MyLevel"). Required for load action.' },
        levelName: { type: 'string', description: 'Level name for streaming operations. Required for stream action.' },
        streaming: { type: 'boolean', description: 'Whether to use streaming load (true) or direct load (false). Optional for load action.' },
        shouldBeLoaded: { type: 'boolean', description: 'Whether to load (true) or unload (false) the streaming level. Required for stream action.' },
        shouldBeVisible: { type: 'boolean', description: 'Whether the streaming level should be visible after loading. Optional for stream action.' },
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
        savePath: { type: 'string', description: 'Content path where animation blueprint will be saved (e.g., "/Game/Animations"). Required for create_animation_bp action.' }
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
        duration: { type: 'number', description: 'How long debug shape persists in seconds. 0 means one frame, -1 means permanent until cleared. Optional, defaults to 0.' }
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
        name: { type: 'string', description: 'Name for the blueprint asset. Required for create action. For add_component, this is the blueprint asset name or path.' },
        blueprintType: { 
          type: 'string',
          description: 'Base class type for blueprint (Actor, Pawn, Character, Object, ActorComponent, SceneComponent, etc.). Required for create action.'
        },
        componentType: { type: 'string', description: 'Component class to add (StaticMeshComponent, SkeletalMeshComponent, CameraComponent, etc.). Required for add_component action.' },
        componentName: { type: 'string', description: 'Unique name for the component instance within the blueprint. Required for add_component action.' },
        savePath: { type: 'string', description: 'Content path where blueprint will be saved (e.g., "/Game/Blueprints"). Required for create action.' }
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
          enum: ['profile', 'show_fps', 'set_quality', 'play_sound', 'create_widget', 'show_widget', 'screenshot', 'engine_start', 'engine_quit', 'read_log'],
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
        // Engine lifecycle
        projectPath: { type: 'string', description: 'Absolute path to .uproject file (e.g., "C:/Projects/MyGame/MyGame.uproject"). Required for engine_start unless UE_PROJECT_PATH environment variable is set.' },
        editorExe: { type: 'string', description: 'Absolute path to Unreal Editor executable (e.g., "C:/UnrealEngine/Engine/Binaries/Win64/UnrealEditor.exe"). Required for engine_start unless UE_EDITOR_EXE environment variable is set.' }
        ,
        // Log reading
        filter_category: { description: 'Category filter as string or array; comma-separated or array values' },
        filter_level: { type: 'string', enum: ['Error','Warning','Log','Verbose','VeryVerbose','All'], description: 'Log level filter' },
        lines: { type: 'number', description: 'Number of lines to read from tail' },
        log_path: { type: 'string', description: 'Absolute path to a specific .log file to read' },
        include_prefixes: { type: 'array', items: { type: 'string' }, description: 'Only include categories starting with any of these prefixes' },
        exclude_categories: { type: 'array', items: { type: 'string' }, description: 'Categories to exclude' }
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
        error: { type: 'string', description: 'Error message if failed' },
        logPath: { type: 'string', description: 'Log file path used for read_log' },
        entries: { 
          type: 'array',
          items: { type: 'object', properties: { timestamp: { type: 'string' }, category: { type: 'string' }, level: { type: 'string' }, message: { type: 'string' } } },
          description: 'Parsed Output Log entries'
        },
        filteredCount: { type: 'number', description: 'Count of entries after filtering' }
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
        name: { type: 'string', description: 'Name for Remote Control preset asset. Required for create_preset action.' },
        path: { type: 'string', description: 'Content path where preset will be saved (e.g., "/Game/RCPresets"). Required for create_preset action.' },
        presetPath: { type: 'string', description: 'Full content path to existing Remote Control preset asset (e.g., "/Game/RCPresets/MyPreset"). Required for expose_actor, expose_property, list_fields, set_property, and get_property actions.' },
        actorName: { type: 'string', description: 'Actor label/name in level to expose to Remote Control preset. Required for expose_actor action.' },
        objectPath: { type: 'string', description: 'Full object path for property operations (e.g., "/Game/Maps/Level.Level:PersistentLevel.StaticMeshActor_0"). Required for expose_property, set_property, and get_property actions.' },
        propertyName: { type: 'string', description: 'Name of the property to expose, get, or set (e.g., "RelativeLocation", "Intensity", "bHidden"). Required for expose_property, set_property, and get_property actions.' },
        value: { description: 'New value to set for property. Must be JSON-serializable and compatible with property type (e.g., {"X":100,"Y":200,"Z":300} for location, true/false for bool, number for numeric types). Required for set_property action.' }
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
        name: { type: 'string', description: 'Name for new Level Sequence asset. Required for create action.' },
        path: { type: 'string', description: 'Content path - for create action: save location (e.g., "/Game/Cinematics"); for open/operations: full asset path (e.g., "/Game/Cinematics/MySequence"). Required for create and open actions.' },
        actorName: { type: 'string', description: 'Actor label/name in level to add as possessable binding to sequence. Required for add_actor action.' },
        actorNames: { type: 'array', items: { type: 'string' }, description: 'Array of actor labels/names for batch add or remove operations. Required for add_actors and remove_actors actions.' },
        className: { type: 'string', description: 'Unreal class name for spawnable actor (e.g., "StaticMeshActor", "CineCameraActor", "SkeletalMeshActor"). Required for add_spawnable_from_class action.' },
        spawnable: { type: 'boolean', description: 'If true, camera is spawnable (owned by sequence); if false, camera is possessable (references level actor). Optional for add_camera, defaults to true.' },
        frameRate: { type: 'number', description: 'Sequence frame rate in frames per second (e.g., 24, 30, 60). Required for set_properties when changing frame rate.' },
        lengthInFrames: { type: 'number', description: 'Total sequence length measured in frames. Required for set_properties when changing duration.' },
        playbackStart: { type: 'number', description: 'First frame of playback range (inclusive). Optional for set_properties.' },
        playbackEnd: { type: 'number', description: 'Last frame of playback range (inclusive). Optional for set_properties.' },
        speed: { type: 'number', description: 'Playback speed multiplier. 1.0 is normal speed, 2.0 is double speed, 0.5 is half speed. Required for set_playback_speed action.' },
        loopMode: { type: 'string', enum: ['once', 'loop', 'pingpong'], description: 'How sequence loops: "once" plays once and stops, "loop" repeats from start, "pingpong" plays forward then backward. Optional for set_properties.' }
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
        action: { type: 'string', enum: ['inspect_object', 'set_property'], description: 'Introspection action: "inspect_object" retrieves all properties, "set_property" modifies a specific property. Required.' },
        objectPath: { type: 'string', description: 'Full object path in Unreal format (e.g., "/Game/Maps/Level.Level:PersistentLevel.StaticMeshActor_0" or "/Script/Engine.Default__StaticMeshActor" for CDO). Required for both actions.' },
        propertyName: { type: 'string', description: 'Name of the property to modify (e.g., "RelativeLocation", "Mobility", "bHidden"). Required for set_property action.' },
        value: { description: 'New property value. Must be JSON-serializable and compatible with property type (e.g., {"X":100,"Y":0,"Z":0} for vectors, 5.0 for floats, true for bools, "Value" for strings). Required for set_property action.' }
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
