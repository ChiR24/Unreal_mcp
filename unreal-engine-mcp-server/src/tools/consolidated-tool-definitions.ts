// Consolidated tool definitions - reduced from 36 to 10 multi-purpose tools

export const consolidatedToolDefinitions = [
  // 1. ASSET MANAGER - Combines asset operations
  {
    name: 'manage_asset',
    description: 'Manage assets - list, import, create materials',
    inputSchema: {
      type: 'object',
      properties: {
        action: { 
          type: 'string', 
          enum: ['list', 'import', 'create_material'],
          description: 'Action to perform'
        },
        // For list
        directory: { type: 'string', description: 'Directory path for listing' },
        recursive: { type: 'boolean', description: 'List recursively' },
        // For import
        sourcePath: { type: 'string', description: 'Source file path' },
        destinationPath: { type: 'string', description: 'Destination path' },
        // For create_material
        name: { type: 'string', description: 'Asset name' },
        path: { type: 'string', description: 'Save path' }
      },
      required: ['action']
    }
  },

  // 2. ACTOR CONTROL - Combines actor operations
  {
    name: 'control_actor',
    description: 'Control actors - spawn, delete, apply physics',
    inputSchema: {
      type: 'object',
      properties: {
        action: { 
          type: 'string', 
          enum: ['spawn', 'delete', 'apply_force'],
          description: 'Action to perform'
        },
        // Common
        actorName: { type: 'string', description: 'Actor name' },
        classPath: { type: 'string', description: 'Blueprint/class path' },
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
    }
  },

  // 3. EDITOR CONTROL - Combines editor operations
  {
    name: 'control_editor',
    description: 'Control editor - PIE mode, camera, viewport',
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
    }
  },

  // 4. LEVEL MANAGER - Combines level and lighting operations
  {
    name: 'manage_level',
    description: 'Manage levels and lighting',
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
    }
  },

  // 5. ANIMATION SYSTEM - Combines animation and physics setup
  {
    name: 'animation_physics',
    description: 'Animation and physics systems',
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
    }
  },

  // 6. EFFECTS SYSTEM - Combines particles and visual effects
  {
    name: 'create_effect',
    description: 'Create visual effects - particles, Niagara',
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
    }
  },

  // 7. BLUEPRINT MANAGER - Blueprint operations
  {
    name: 'manage_blueprint',
    description: 'Create and modify blueprints',
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
    }
  },

  // 8. ENVIRONMENT BUILDER - Landscape and foliage
  {
    name: 'build_environment',
    description: 'Build environment - landscape, foliage',
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
    }
  },

  // 9. PERFORMANCE & AUDIO - System settings
  {
    name: 'system_control',
    description: 'Control performance, audio, and UI systems',
    inputSchema: {
      type: 'object',
      properties: {
        action: { 
          type: 'string', 
          enum: ['profile', 'show_fps', 'set_quality', 'play_sound', 'create_widget', 'show_widget'],
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
        visible: { type: 'boolean', description: 'Visibility' }
      },
      required: ['action']
    }
  },

  // 10. CONSOLE COMMAND - Universal tool
  {
    name: 'console_command',
    description: 'Execute any console command in Unreal Engine',
    inputSchema: {
      type: 'object',
      properties: {
        command: { type: 'string', description: 'Console command to execute' }
      },
      required: ['command']
    }
  },

  // 11. VERIFICATION - Read-only verification helpers
  {
    name: 'verify_environment',
    description: 'Verify environment changes (foliage, landscape) to detect false successes',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: ['foliage_type_exists', 'foliage_instances_near', 'landscape_exists'],
          description: 'Verification action'
        },
        // Common
        name: { type: 'string', description: 'Name filter (e.g., foliage type name or landscape name)' },
        // For foliage_instances_near
        position: {
          type: 'object',
          properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } }
        },
        radius: { type: 'number', description: 'Radius for instance counting' }
      },
      required: ['action']
    }
  }
];
