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
    description: 'Control performance, audio, UI, screenshots, and engine lifecycle',
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
    description: 'Execute any console command in Unreal Engine',
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
    description: 'Manage Remote Control presets: create, expose, list fields, set/get values',
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
    description: 'Create/open sequences, add camera, add actors to sequence',
    inputSchema: {
      type: 'object',
      properties: {
        action: { type: 'string', enum: ['create', 'open', 'add_camera', 'add_actor'], description: 'Sequence action' },
        name: { type: 'string', description: 'Sequence name (for create)' },
        path: { type: 'string', description: 'Save path (for create), or asset path (for open)' },
        actorName: { type: 'string', description: 'Actor name to add as possessable' },
        spawnable: { type: 'boolean', description: 'If true, camera is spawnable' }
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        sequencePath: { type: 'string' },
        cameraBindingId: { type: 'string' },
        message: { type: 'string' },
        error: { type: 'string' }
      }
    }
  },

  // 13. INTROSPECTION
  {
    name: 'inspect',
    description: 'Inspect objects and set properties safely',
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
