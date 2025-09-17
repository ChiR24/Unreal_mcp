// Tool definitions for all 16 MCP tools

export const toolDefinitions = [
  // Asset Tools
  {
    name: 'list_assets',
    description: `List assets in a folder of the project.

When to use:
- Browse project content (use /Game; /Content is auto-mapped by the server).
- Get a quick inventory of assets in a subfolder to refine subsequent actions.

Notes:
- For /Game, the server may limit results for performance; prefer subfolders (e.g., /Game/ThirdPerson).
- Returns a structured list with Name/Path/Class/PackagePath when available.

Example:
- {"directory":"/Game/ThirdPerson","recursive":false}`,
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
    description: `Import a file from disk into the project (e.g., FBX, PNG, WAV, EXR).

When to use:
- Bring external content into /Game at a specific destination path.

Notes:
- destinationPath is a package path like /Game/Environment/Trees.
- Keep file names simple (avoid spaces and special characters).

Example:
- {"sourcePath":"C:/Temp/Tree.fbx","destinationPath":"/Game/Environment/Trees"}`,
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
    description: `Spawn a new actor in the current level.

When to use:
- Place a class (e.g., StaticMeshActor, CameraActor) or spawn from an asset path (e.g., /Engine/BasicShapes/Cube).

Notes:
- If an asset path is provided, a StaticMeshActor is auto-spawned with the mesh set.
- location/rotation are optional; defaults are used if omitted.

Example:
- {"classPath":"/Engine/BasicShapes/Cube","location":{"x":0,"y":0,"z":100}}`,
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
    description: `Delete one or more actors by name/label.

When to use:
- Remove actors matching a label/name (case-insensitive).

Example:
- {"actorName":"Cube_1"}`,
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
    description: `Create a simple Material asset at a path.

When to use:
- Quickly scaffold a basic material you can edit later.

Example:
- {"name":"M_Mask","path":"/Game/Materials"}`,
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
    description: `Assign a material to an actor's mesh component.

When to use:
- Swap an actor's material by path; slotIndex defaults to 0.

Example:
- {"actorPath":"/Game/LevelActors/Cube_1","materialPath":"/Game/Materials/M_Mask","slotIndex":0}`,
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
    description: `Start a Play-In-Editor (PIE) session.

When to use:
- Begin simulating the level in the editor.`,
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
    description: `Stop the active PIE session.

When to use:
- End simulation and return to the editor.`,
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
    description: `Reposition the editor viewport camera.

When to use:
- Move/aim the camera in the editor for framing.

Notes:
- Accepts object or array formats; values are normalized.

Example:
- {"location":{"x":0,"y":-600,"z":250},"rotation":{"pitch":0,"yaw":0,"roll":0}}`,
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
    description: `Create an Animation Blueprint for a skeleton.

When to use:
- Generate a starter Anim BP for a given skeleton.

Example:
- {"name":"ABP_Hero","skeletonPath":"/Game/Characters/Hero/SK_Hero_Skeleton","savePath":"/Game/Characters/Hero"}`,
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
    description: `Play a Montage/Animation on an actor.

When to use:
- Trigger a montage on a possessed or editor actor.

Example:
- {"actorName":"Hero","montagePath":"/Game/Anim/MT_Attack","playRate":1.0}`,
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
    description: `Enable simple ragdoll using a physics asset.

When to use:
- Toggle ragdoll behavior on a character skeleton.

Example:
- {"skeletonPath":"/Game/Characters/Hero/SK_Hero_Skeleton","physicsAssetName":"PHYS_Hero","blendWeight":1.0}`,
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
    description: `Apply a world-space force vector to an actor with physics enabled.

Example:
- {"actorName":"PhysicsBox","force":{"x":0,"y":0,"z":5000}}`,
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
    description: `Create a simple particle/FX by tag.

When to use:
- Quickly drop a generic Fire/Smoke/Water effect for previews.

Example:
- {"effectType":"Smoke","name":"SMK1","location":{"x":100,"y":0,"z":50}}`,
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
    description: `Spawn a Niagara system at a location.

Example:
- {"systemPath":"/Game/FX/NS_Explosion","location":{"x":0,"y":0,"z":200},"scale":1.0}`,
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
    description: `Create a new Blueprint asset at a path.

Example:
- {"name":"BP_Switch","blueprintType":"Actor","savePath":"/Game/Blueprints"}`,
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
    description: `Add a component to an existing Blueprint.

Example:
- {"blueprintName":"BP_Switch","componentType":"PointLightComponent","componentName":"KeyLight"}`,
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
    description: `Load a level by path (e.g., /Game/Maps/Lobby).

Example:
- {"levelPath":"/Game/Maps/Lobby","streaming":false}`,
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
    description: `Save the current level to a path or by name.

Example:
- {"levelName":"Lobby","savePath":"/Game/Maps"}`,
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
    description: `Stream in/out a sublevel and set visibility.

Example:
- {"levelName":"Sublevel_A","shouldBeLoaded":true,"shouldBeVisible":true}`,
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
    description: `Create a light (Directional/Point/Spot/Rect/Sky) with optional transform/intensity.

Examples:
- {"lightType":"Directional","name":"KeyLight","intensity":5.0}
- {"lightType":"Point","name":"Fill","location":{"x":0,"y":100,"z":200},"intensity":2000}`,
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
    description: `Start a lighting build.

When to use:
- Bake lights for preview or final output (choose quality).

Example:
- {"quality":"High"}`,
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
    description: `Attempt to create a landscape.

Notes:
- Native Python APIs are limited; you may be guided to use Landscape Mode in the editor.

Example:
- {"name":"Landscape_Basic","sizeX":1024,"sizeY":1024}`,
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
    description: `Sculpt a landscape using editor tools (best-effort; some operations may require manual Landscape Mode).

Example:
- {"landscapeName":"Landscape_Basic","tool":"Smooth","brushSize":300,"strength":0.5}`,
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
    description: `Create or load a FoliageType asset for instanced foliage workflows.

Example:
- {"name":"FT_Grass","meshPath":"/Game/Foliage/SM_Grass","density":300}`,
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
    description: `Paint foliage onto the world.

When to use:
- Scatter instances using an existing FoliageType.

Example:
- {"foliageType":"/Game/Foliage/Types/FT_Grass","position":{"x":0,"y":0,"z":0},"brushSize":300}`,
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
    description: `Draw a debug shape.

Example:
- {"shape":"Sphere","position":{"x":0,"y":0,"z":0},"size":100,"color":[255,0,0,255],"duration":3}`,
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
    description: `Set the viewport view mode.

Example:
- {"mode":"Wireframe"}`,
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
    description: `Start performance profiling.

Example:
- {"type":"GPU","duration":10}`,
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
    description: `Show/hide the FPS counter.

Example:
- {"enabled":true,"verbose":false}`,
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
    description: `Set scalability/quality levels.

Example:
- {"category":"Shadows","level":2}`,
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
    description: `Play a sound.

Example:
- {"soundPath":"/Game/Audio/SFX/Click","volume":0.5,"is3D":true}`,
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
    description: `Create an ambient sound actor.

Example:
- {"name":"Amb_Wind","soundPath":"/Game/Audio/Amb/AMB_Wind","location":{"x":0,"y":0,"z":0},"radius":1000}`,
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
    description: `Create a UI widget.

Example:
- {"name":"HUDMain","type":"HUD","savePath":"/Game/UI"}`,
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
    description: `Show or hide a widget.

Example:
- {"widgetName":"HUDMain","visible":true}`,
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
    description: `Create a HUD description/layout.

Example:
- {"name":"GameHUD","elements":[{"type":"Text","position":[10,10]}]}`,
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
    description: `Execute a console command.

When to use:
- Quick toggles like "stat fps", "viewmode wireframe", or r.* cvars.

Examples:
- {"command":"stat fps"}
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
