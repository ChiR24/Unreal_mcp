// Tool definitions for all 16 MCP tools

export const toolDefinitions = [
  // Asset Tools
  {
    name: 'list_assets',
    description: 'List all assets in a directory',
    inputSchema: {
      type: 'object',
      properties: {
        directory: { type: 'string', description: 'Directory path (e.g. /Game/Assets)' },
        recursive: { type: 'boolean', description: 'List recursively' }
      },
      required: ['directory']
    },
    outputSchema: {
      type: 'object',
      properties: {
        assets: {
          type: 'array',
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
        error: { type: 'string' },
        note: { type: 'string' }
      }
    }
  },
  {
    name: 'import_asset',
    description: 'Import an asset from file system',
    inputSchema: {
      type: 'object',
      properties: {
        sourcePath: { type: 'string', description: 'File system path to import from' },
        destinationPath: { type: 'string', description: 'Project path to import to' }
      },
      required: ['sourcePath', 'destinationPath']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        paths: { type: 'array', items: { type: 'string' } },
        message: { type: 'string' },
        error: { type: 'string' }
      }
    }
  },

  // Actor Tools
  {
    name: 'spawn_actor',
    description: 'Spawn a new actor in the level',
    inputSchema: {
      type: 'object',
      properties: {
        classPath: { type: 'string', description: 'Blueprint/class path' },
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
        }
      },
      required: ['classPath']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        actor: { type: 'string' },
        error: { type: 'string' }
      }
    }
  },
  {
    name: 'delete_actor',
    description: 'Delete an actor from the level',
    inputSchema: {
      type: 'object',
      properties: {
        actorName: { type: 'string', description: 'Name of the actor to delete' }
      },
      required: ['actorName']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        deleted: { type: 'string' },
        error: { type: 'string' }
      }
    }
  },

  // Material Tools
  {
    name: 'create_material',
    description: 'Create a new material asset',
    inputSchema: {
      type: 'object',
      properties: {
        name: { type: 'string', description: 'Material name' },
        path: { type: 'string', description: 'Path to create material' }
      },
      required: ['name', 'path']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        materialPath: { type: 'string' },
        message: { type: 'string' },
        error: { type: 'string' }
      }
    }
  },
  {
    name: 'apply_material_to_actor',
    description: 'Apply a material to an actor in the level',
    inputSchema: {
      type: 'object',
      properties: {
        actorPath: { type: 'string', description: 'Path to the actor' },
        materialPath: { type: 'string', description: 'Path to the material asset' },
        slotIndex: { type: 'number', description: 'Material slot index (default: 0)' }
      },
      required: ['actorPath', 'materialPath']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        message: { type: 'string' }
      }
    }
  },

  // Editor Tools
  {
    name: 'play_in_editor',
    description: 'Start Play In Editor (PIE) mode',
    inputSchema: {
      type: 'object',
      properties: {}
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        playing: { type: 'boolean' },
        message: { type: 'string' }
      }
    }
  },
  {
    name: 'stop_play_in_editor',
    description: 'Stop Play In Editor (PIE) mode',
    inputSchema: {
      type: 'object',
      properties: {}
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        playing: { type: 'boolean' },
        message: { type: 'string' }
      }
    }
  },
  {
    name: 'set_camera',
    description: 'Set viewport camera position and rotation',
    inputSchema: {
      type: 'object',
      properties: {
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
        }
      },
      required: ['location']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        location: { type: 'array', items: { type: 'number' } },
        rotation: { type: 'array', items: { type: 'number' } }
      }
    }
  },

  // Animation Tools
  {
    name: 'create_animation_blueprint',
    description: 'Create an animation blueprint',
    inputSchema: {
      type: 'object',
      properties: {
        name: { type: 'string', description: 'Animation blueprint name' },
        skeletonPath: { type: 'string', description: 'Path to skeleton' },
        savePath: { type: 'string', description: 'Save location' }
      },
      required: ['name', 'skeletonPath']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        blueprintPath: { type: 'string' },
        message: { type: 'string' }
      }
    }
  },
  {
    name: 'play_animation_montage',
    description: 'Play an animation montage on an actor',
    inputSchema: {
      type: 'object',
      properties: {
        actorName: { type: 'string', description: 'Actor name' },
        montagePath: { type: 'string', description: 'Path to montage' },
        playRate: { type: 'number', description: 'Playback rate' }
      },
      required: ['actorName', 'montagePath']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        playing: { type: 'boolean' },
        message: { type: 'string' }
      }
    }
  },

  // Physics Tools
  {
    name: 'setup_ragdoll',
    description: 'Setup ragdoll physics for a skeletal mesh',
    inputSchema: {
      type: 'object',
      properties: {
        skeletonPath: { type: 'string', description: 'Path to skeleton' },
        physicsAssetName: { type: 'string', description: 'Physics asset name' },
        blendWeight: { type: 'number', description: 'Blend weight (0-1)' }
      },
      required: ['skeletonPath', 'physicsAssetName']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        ragdollActive: { type: 'boolean' },
        message: { type: 'string' }
      }
    }
  },
  {
    name: 'apply_force',
    description: 'Apply force to an actor',
    inputSchema: {
      type: 'object',
      properties: {
        actorName: { type: 'string', description: 'Actor name' },
        force: {
          type: 'object',
          properties: {
            x: { type: 'number' },
            y: { type: 'number' },
            z: { type: 'number' }
          }
        }
      },
      required: ['actorName', 'force']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        physicsEnabled: { type: 'boolean' },
        message: { type: 'string' },
        error: { type: 'string' }
      }
    }
  },

  // Niagara Tools
  {
    name: 'create_particle_effect',
    description: 'Create a Niagara particle effect',
    inputSchema: {
      type: 'object',
      properties: {
        effectType: { type: 'string', description: 'Effect type (Fire, Smoke, Water, etc.)' },
        name: { type: 'string', description: 'Effect name' },
        location: {
          type: 'object',
          properties: {
            x: { type: 'number' },
            y: { type: 'number' },
            z: { type: 'number' }
          }
        }
      },
      required: ['effectType', 'name', 'location']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        effectName: { type: 'string' },
        effectPath: { type: 'string' },
        message: { type: 'string' }
      }
    }
  },
  {
    name: 'spawn_niagara_system',
    description: 'Spawn a Niagara system in the level',
    inputSchema: {
      type: 'object',
      properties: {
        systemPath: { type: 'string', description: 'Path to Niagara system' },
        location: {
          type: 'object',
          properties: {
            x: { type: 'number' },
            y: { type: 'number' },
            z: { type: 'number' }
          }
        },
        scale: { type: 'number', description: 'Scale factor' }
      },
      required: ['systemPath', 'location']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        spawned: { type: 'boolean' },
        message: { type: 'string' }
      }
    }
  },

  // Blueprint Tools
  {
    name: 'create_blueprint',
    description: 'Create a new blueprint',
    inputSchema: {
      type: 'object',
      properties: {
        name: { type: 'string', description: 'Blueprint name' },
        blueprintType: { type: 'string', description: 'Type (Actor, Pawn, Character, etc.)' },
        savePath: { type: 'string', description: 'Save location' }
      },
      required: ['name', 'blueprintType']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        blueprintPath: { type: 'string' },
        message: { type: 'string' }
      }
    }
  },
  {
    name: 'add_blueprint_component',
    description: 'Add a component to a blueprint',
    inputSchema: {
      type: 'object',
      properties: {
        blueprintName: { type: 'string', description: 'Blueprint name' },
        componentType: { type: 'string', description: 'Component type' },
        componentName: { type: 'string', description: 'Component name' }
      },
      required: ['blueprintName', 'componentType', 'componentName']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        componentAdded: { type: 'string' },
        message: { type: 'string' },
        warning: { type: 'string' }
      }
    }
  },

  // Level Tools
  {
    name: 'load_level',
    description: 'Load a level',
    inputSchema: {
      type: 'object',
      properties: {
        levelPath: { type: 'string', description: 'Path to level' },
        streaming: { type: 'boolean', description: 'Use streaming' }
      },
      required: ['levelPath']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        levelName: { type: 'string' },
        message: { type: 'string' }
      }
    }
  },
  {
    name: 'save_level',
    description: 'Save the current level',
    inputSchema: {
      type: 'object',
      properties: {
        levelName: { type: 'string', description: 'Level name' },
        savePath: { type: 'string', description: 'Save path' }
      }
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        saved: { type: 'boolean' },
        message: { type: 'string' }
      }
    }
  },
  {
    name: 'stream_level',
    description: 'Stream a level in or out',
    inputSchema: {
      type: 'object',
      properties: {
        levelName: { type: 'string', description: 'Level name' },
        shouldBeLoaded: { type: 'boolean', description: 'Load or unload' },
        shouldBeVisible: { type: 'boolean', description: 'Make visible' }
      },
      required: ['levelName', 'shouldBeLoaded', 'shouldBeVisible']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        loaded: { type: 'boolean' },
        visible: { type: 'boolean' },
        message: { type: 'string' }
      }
    }
  },

  // Lighting Tools
  {
    name: 'create_light',
    description: 'Create a light in the level',
    inputSchema: {
      type: 'object',
      properties: {
        lightType: { type: 'string', description: 'Light type (Directional, Point, Spot, Rect)' },
        name: { type: 'string', description: 'Light name' },
        location: {
          type: 'object',
          properties: {
            x: { type: 'number' },
            y: { type: 'number' },
            z: { type: 'number' }
          }
        },
        intensity: { type: 'number', description: 'Light intensity' }
      },
      required: ['lightType', 'name']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        lightName: { type: 'string' },
        message: { type: 'string' }
      }
    }
  },
  {
    name: 'build_lighting',
    description: 'Build lighting for the current level',
    inputSchema: {
      type: 'object',
      properties: {
        quality: { type: 'string', description: 'Quality (Preview, Medium, High, Production)' }
      }
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        quality: { type: 'string' },
        message: { type: 'string' }
      }
    }
  },

  // Landscape Tools
  {
    name: 'create_landscape',
    description: 'Create a new landscape',
    inputSchema: {
      type: 'object',
      properties: {
        name: { type: 'string', description: 'Landscape name' },
        sizeX: { type: 'number', description: 'Size X' },
        sizeY: { type: 'number', description: 'Size Y' },
        materialPath: { type: 'string', description: 'Material path' }
      },
      required: ['name']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        landscapeName: { type: 'string' },
        message: { type: 'string' }
      }
    }
  },
  {
    name: 'sculpt_landscape',
    description: 'Sculpt the landscape',
    inputSchema: {
      type: 'object',
      properties: {
        landscapeName: { type: 'string', description: 'Landscape name' },
        tool: { type: 'string', description: 'Tool (Sculpt, Smooth, Flatten, etc.)' },
        brushSize: { type: 'number', description: 'Brush size' },
        strength: { type: 'number', description: 'Tool strength' }
      },
      required: ['landscapeName', 'tool']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        message: { type: 'string' }
      }
    }
  },

  // Foliage Tools
  {
    name: 'add_foliage_type',
    description: 'Add a foliage type',
    inputSchema: {
      type: 'object',
      properties: {
        name: { type: 'string', description: 'Foliage type name' },
        meshPath: { type: 'string', description: 'Path to mesh' },
        density: { type: 'number', description: 'Density' }
      },
      required: ['name', 'meshPath']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        foliageTypeName: { type: 'string' },
        message: { type: 'string' }
      }
    }
  },
  {
    name: 'paint_foliage',
    description: 'Paint foliage on landscape',
    inputSchema: {
      type: 'object',
      properties: {
        foliageType: { type: 'string', description: 'Foliage type' },
        position: {
          type: 'object',
          properties: {
            x: { type: 'number' },
            y: { type: 'number' },
            z: { type: 'number' }
          }
        },
        brushSize: { type: 'number', description: 'Brush size' }
      },
      required: ['foliageType', 'position']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        instancesPlaced: { type: 'number' },
        message: { type: 'string' }
      }
    }
  },

  // Debug Visualization Tools
  {
    name: 'draw_debug_shape',
    description: 'Draw a debug shape',
    inputSchema: {
      type: 'object',
      properties: {
        shape: { type: 'string', description: 'Shape type (Line, Box, Sphere, etc.)' },
        position: {
          type: 'object',
          properties: {
            x: { type: 'number' },
            y: { type: 'number' },
            z: { type: 'number' }
          }
        },
        size: { type: 'number', description: 'Size/radius' },
        color: {
          type: 'array',
          items: { type: 'number' },
          description: 'RGBA color'
        },
        duration: { type: 'number', description: 'Duration in seconds' }
      },
      required: ['shape', 'position']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        message: { type: 'string' }
      }
    }
  },
  {
    name: 'set_view_mode',
    description: 'Set the viewport view mode',
    inputSchema: {
      type: 'object',
      properties: {
        mode: { type: 'string', description: 'View mode (Lit, Unlit, Wireframe, etc.)' }
      },
      required: ['mode']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        viewMode: { type: 'string' },
        message: { type: 'string' }
      }
    }
  },

  // Performance Tools
  {
    name: 'start_profiling',
    description: 'Start performance profiling',
    inputSchema: {
      type: 'object',
      properties: {
        type: { type: 'string', description: 'Profiling type (CPU, GPU, Memory, etc.)' },
        duration: { type: 'number', description: 'Duration in seconds' }
      },
      required: ['type']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        profiling: { type: 'boolean' },
        message: { type: 'string' }
      }
    }
  },
  {
    name: 'show_fps',
    description: 'Show FPS counter',
    inputSchema: {
      type: 'object',
      properties: {
        enabled: { type: 'boolean', description: 'Enable FPS display' },
        verbose: { type: 'boolean', description: 'Show verbose stats' }
      },
      required: ['enabled']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        fpsVisible: { type: 'boolean' },
        message: { type: 'string' }
      }
    }
  },
  {
    name: 'set_scalability',
    description: 'Set scalability settings',
    inputSchema: {
      type: 'object',
      properties: {
        category: { type: 'string', description: 'Category (Shadows, Textures, Effects, etc.)' },
        level: { type: 'number', description: 'Quality level (0-4)' }
      },
      required: ['category', 'level']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        qualityLevel: { type: 'number' },
        message: { type: 'string' }
      }
    }
  },

  // Audio Tools
  {
    name: 'play_sound',
    description: 'Play a sound',
    inputSchema: {
      type: 'object',
      properties: {
        soundPath: { type: 'string', description: 'Path to sound asset' },
        location: {
          type: 'object',
          properties: {
            x: { type: 'number' },
            y: { type: 'number' },
            z: { type: 'number' }
          }
        },
        volume: { type: 'number', description: 'Volume (0-1)' },
        is3D: { type: 'boolean', description: '3D or 2D sound' }
      },
      required: ['soundPath']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        soundPlaying: { type: 'boolean' },
        message: { type: 'string' }
      }
    }
  },
  {
    name: 'create_ambient_sound',
    description: 'Create an ambient sound',
    inputSchema: {
      type: 'object',
      properties: {
        name: { type: 'string', description: 'Sound name' },
        soundPath: { type: 'string', description: 'Path to sound' },
        location: {
          type: 'object',
          properties: {
            x: { type: 'number' },
            y: { type: 'number' },
            z: { type: 'number' }
          }
        },
        radius: { type: 'number', description: 'Sound radius' }
      },
      required: ['name', 'soundPath', 'location']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        soundName: { type: 'string' },
        message: { type: 'string' }
      }
    }
  },

  // UI Tools
  {
    name: 'create_widget',
    description: 'Create a UI widget',
    inputSchema: {
      type: 'object',
      properties: {
        name: { type: 'string', description: 'Widget name' },
        type: { type: 'string', description: 'Widget type (HUD, Menu, etc.)' },
        savePath: { type: 'string', description: 'Save location' }
      },
      required: ['name']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        widgetPath: { type: 'string' },
        message: { type: 'string' }
      }
    }
  },
  {
    name: 'show_widget',
    description: 'Show or hide a widget',
    inputSchema: {
      type: 'object',
      properties: {
        widgetName: { type: 'string', description: 'Widget name' },
        visible: { type: 'boolean', description: 'Show or hide' }
      },
      required: ['widgetName', 'visible']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        widgetVisible: { type: 'boolean' },
        message: { type: 'string' }
      }
    }
  },
  {
    name: 'create_hud',
    description: 'Create a HUD',
    inputSchema: {
      type: 'object',
      properties: {
        name: { type: 'string', description: 'HUD name' },
        elements: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              type: { type: 'string', description: 'Element type' },
              position: {
                type: 'array',
                items: { type: 'number' }
              }
            }
          }
        }
      },
      required: ['name']
    },
    outputSchema: {
      type: 'object',
      properties: {
        success: { type: 'boolean' },
        hudPath: { type: 'string' },
        message: { type: 'string' }
      }
    }
  },

  // Console command (universal tool)
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
        success: { type: 'boolean' },
        command: { type: 'string' },
        result: { type: 'object' },
        warning: { type: 'string' },
        info: { type: 'string' },
        error: { type: 'string' },
        message: { type: 'string' }
      }
    }
  }
];
