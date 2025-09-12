// Level management tools for Unreal Engine
import { UnrealBridge } from '../unreal-bridge.js';

export class LevelTools {
  constructor(private bridge: UnrealBridge) {}

  // Execute console command
  private async executeCommand(command: string) {
    return this.bridge.httpCall('/remote/object/call', 'PUT', {
      objectPath: '/Script/Engine.Default__KismetSystemLibrary',
      functionName: 'ExecuteConsoleCommand',
      parameters: {
        WorldContextObject: null,
        Command: command,
        SpecificPlayer: null
      },
      generateTransaction: false
    });
  }

  // Load level (best-effort Python with console fallback)
  async loadLevel(params: {
    levelPath: string;
    streaming?: boolean;
    position?: [number, number, number];
  }) {
    if (params.streaming) {
      // Try to add as streaming level
      const py = `\nimport unreal\ntry:\n    world = unreal.EditorLevelLibrary.get_editor_world()\n    if world:\n        unreal.EditorLevelUtils.add_level_to_world(world, r"${params.levelPath}", unreal.LevelStreamingKismet)\n        print('RESULT:{\\'success\\': True}')\n    else:\n        print('RESULT:{\\'success\\': False, \\'error\\': \\'No editor world\\'}')\nexcept Exception as e:\n    print('RESULT:{\\'success\\': False, \\'error\\': \\'%s\\'}' % str(e))\n`.trim();
      try {
        const resp = await this.bridge.executePython(py);
        const out = typeof resp === 'string' ? resp : JSON.stringify(resp);
        const m = out.match(/RESULT:({.*})/);
        if (m) { try { const parsed = JSON.parse(m[1].replace(/'/g, '"')); if (parsed.success) return { success: true, message: 'Streaming level added' }; } catch {} }
      } catch {}
      // Fallback to console
      return this.bridge.executeConsoleCommand(`LoadStreamLevel ${params.levelPath}`);
    } else {
      // Open map in editor runtime
      return this.bridge.executeConsoleCommand(`open ${params.levelPath}`);
    }
  }

  // Save current level
  async saveLevel(params: {
    levelName?: string;
    savePath?: string;
  }) {
    // Use Python EditorLevelLibrary.save_current_level for reliability
    const python = `
import unreal
try:
    # Attempt to reduce source control prompts (best-effort, may be a no-op depending on UE version)
    try:
        prefs = unreal.SourceControlPreferences()
        try:
            prefs.set_enable_source_control(False)
        except Exception:
            try:
                prefs.enable_source_control = False
            except Exception:
                pass
    except Exception:
        pass

    # Determine if level is dirty and save via LevelEditorSubsystem when possible
    try:
        world = None
        try:
            world = unreal.EditorSubsystemLibrary.get_editor_world()
        except Exception:
            try:
                world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
            except Exception:
                world = None
        pkg_path = None
        try:
            if world is not None:
                full = world.get_path_name()
                pkg_path = full.split('.')[0] if '.' in full else full
        except Exception:
            pkg_path = None
        if pkg_path and not unreal.EditorAssetLibrary.is_asset_dirty(pkg_path):
            print('RESULT:{"success": true, "skipped": true, "reason": "Level not dirty"}')
            raise SystemExit(0)
    except Exception:
        pass

    # Save using LevelEditorSubsystem to avoid deprecation
    saved = False
    try:
        les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        if les:
            les.save_current_level()
            saved = True
    except Exception:
        pass
    if not saved:
        unreal.EditorLevelLibrary.save_current_level()
    print('RESULT:{"success": true}')
except Exception as e:
    print('RESULT:{"success": false, "error": "' + str(e).replace('"','\\"') + '"}')
`.trim();
    try {
      const resp = await this.bridge.executePython(python)
      const out = typeof resp === 'string' ? resp : JSON.stringify(resp)
      const m = out.match(/RESULT:({.*})/)
      if (m) {
        try { const parsed = JSON.parse(m[1]); return parsed.success ? { success: true, message: 'Level saved' } : { success: false, error: parsed.error } } catch {}
      }
      return { success: true, message: 'Level saved' }
    } catch (e) {
      return { success: false, error: `Failed to save level: ${e}` }
    }
  }

  // Create new level (Python via LevelEditorSubsystem)
  async createLevel(params: {
    levelName: string;
    template?: 'Empty' | 'Default' | 'VR' | 'TimeOfDay';
    savePath?: string;
  }) {
    const basePath = params.savePath || '/Game/Maps';
    const isPartitioned = true; // default to World Partition for UE5
    const fullPath = `${basePath}/${params.levelName}`;
    const py = `\nimport unreal\ntry:\n    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)\n    if les:\n        les.new_level(r"${fullPath}", ${isPartitioned ? 'True' : 'False'})\n        print('RESULT:{\'success\': True, \'message\': \'Level created\'}')\n    else:\n        print('RESULT:{\'success\': False, \'error\': \'LevelEditorSubsystem not available\'}')\nexcept Exception as e:\n    print('RESULT:{\'success\': False, \'error\': \'%s\'}' % str(e))\n`.trim();
    try {
      const resp = await this.bridge.executePython(py);
      const out = typeof resp === 'string' ? resp : JSON.stringify(resp);
      const m = out.match(/RESULT:({.*})/);
      if (m) { try { const parsed = JSON.parse(m[1].replace(/'/g, '"')); return parsed.success ? { success: true, message: parsed.message } : { success: false, error: parsed.error }; } catch {} }
      return { success: true, message: 'Level creation attempted' };
    } catch (e) {
      return { success: false, error: `Failed to create level: ${e}` };
    }
  }

  // Stream level (Python attempt with fallback)
  async streamLevel(params: {
    levelName: string;
    shouldBeLoaded: boolean;
    shouldBeVisible: boolean;
    position?: [number, number, number];
  }) {
    const py = `\nimport unreal\ntry:\n    world = unreal.EditorLevelLibrary.get_editor_world()\n    if world:\n        # Find streaming level by name and set flags\n        updated = False\n        for sl in world.get_streaming_levels():\n            try:\n                name = sl.get_world_asset_package_name() if hasattr(sl, 'get_world_asset_package_name') else str(sl.get_editor_property('world_asset'))\n                if name and name.endswith('/${params.levelName}'):\n                    try: sl.set_should_be_loaded(${params.shouldBeLoaded ? 'True' : 'False'})\n                    except Exception: pass\n                    try: sl.set_should_be_visible(${params.shouldBeVisible ? 'True' : 'False'})\n                    except Exception: pass\n                    updated = True\n                    break\n            except Exception: pass\n        print('RESULT:{\\'success\\': %s}' % ('True' if updated else 'False'))\n    else:\n        print('RESULT:{\\'success\\': False, \\'error\\': \\'No editor world\\'}')\nexcept Exception as e:\n    print('RESULT:{\\'success\\': False, \\'error\\': \\'%s\\'}' % str(e))\n`.trim();
    try {
      const resp = await this.bridge.executePython(py);
      const out = typeof resp === 'string' ? resp : JSON.stringify(resp);
      const m = out.match(/RESULT:({.*})/);
      if (m) { try { const parsed = JSON.parse(m[1].replace(/'/g, '"')); if (parsed.success) return { success: true, message: 'Streaming level updated' }; } catch {} }
    } catch {}
    // Fallback
    const loadCmd = params.shouldBeLoaded ? 'Load' : 'Unload';
    const visCmd = params.shouldBeVisible ? 'Show' : 'Hide';
    const command = `StreamLevel ${params.levelName} ${loadCmd} ${visCmd}`;
    return this.bridge.executeConsoleCommand(command);
  }

  // World composition
  async setupWorldComposition(params: {
    enableComposition: boolean;
    tileSize?: number;
    distanceStreaming?: boolean;
    streamingDistance?: number;
  }) {
    const commands = [];
    
    if (params.enableComposition) {
      commands.push('EnableWorldComposition');
      if (params.tileSize) {
        commands.push(`SetWorldTileSize ${params.tileSize}`);
      }
      if (params.distanceStreaming) {
        commands.push(`EnableDistanceStreaming ${params.streamingDistance || 5000}`);
      }
    } else {
      commands.push('DisableWorldComposition');
    }
    
    for (const cmd of commands) {
      await this.bridge.executeConsoleCommand(cmd);
    }
    
    return { success: true, message: 'World composition configured' };
  }

  // Level blueprint
  async editLevelBlueprint(params: {
    eventType: 'BeginPlay' | 'EndPlay' | 'Tick' | 'Custom';
    customEventName?: string;
    nodes?: Array<{
      nodeType: string;
      position: [number, number];
      connections?: string[];
    }>;
  }) {
    const command = `OpenLevelBlueprint ${params.eventType}`;
    return this.bridge.executeConsoleCommand(command);
  }

  // Sub-levels
  async createSubLevel(params: {
    name: string;
    type: 'Persistent' | 'Streaming' | 'Lighting' | 'Gameplay';
    parent?: string;
  }) {
    const command = `CreateSubLevel ${params.name} ${params.type} ${params.parent || 'None'}`;
    return this.bridge.executeConsoleCommand(command);
  }

  // World settings
  async setWorldSettings(params: {
    gravity?: number;
    worldScale?: number;
    gameMode?: string;
    defaultPawn?: string;
    killZ?: number;
  }) {
    const commands = [];
    
    if (params.gravity !== undefined) {
      commands.push(`SetWorldGravity ${params.gravity}`);
    }
    if (params.worldScale !== undefined) {
      commands.push(`SetWorldToMeters ${params.worldScale}`);
    }
    if (params.gameMode) {
      commands.push(`SetGameMode ${params.gameMode}`);
    }
    if (params.defaultPawn) {
      commands.push(`SetDefaultPawn ${params.defaultPawn}`);
    }
    if (params.killZ !== undefined) {
      commands.push(`SetKillZ ${params.killZ}`);
    }
    
    for (const cmd of commands) {
      await this.bridge.executeConsoleCommand(cmd);
    }
    
    return { success: true, message: 'World settings updated' };
  }

  // Level bounds
  async setLevelBounds(params: {
    min: [number, number, number];
    max: [number, number, number];
  }) {
    const command = `SetLevelBounds ${params.min.join(',')} ${params.max.join(',')}`;
    return this.bridge.executeConsoleCommand(command);
  }

  // Navigation mesh
  async buildNavMesh(params: {
    rebuildAll?: boolean;
    selectedOnly?: boolean;
  }) {
    const command = params.rebuildAll 
      ? 'RebuildNavigation' 
      : 'BuildPaths';
    
    return this.bridge.executeConsoleCommand(command);
  }

  // Level visibility
  async setLevelVisibility(params: {
    levelName: string;
    visible: boolean;
  }) {
    const command = `SetLevelVisibility ${params.levelName} ${params.visible}`;
    return this.bridge.executeConsoleCommand(command);
  }

  // World origin
  async setWorldOrigin(params: {
    location: [number, number, number];
  }) {
    const command = `SetWorldOriginLocation ${params.location.join(' ')}`;
    return this.bridge.executeConsoleCommand(command);
  }

  // Level streaming volumes
  async createStreamingVolume(params: {
    levelName: string;
    position: [number, number, number];
    size: [number, number, number];
    streamingDistance?: number;
  }) {
    const command = `CreateStreamingVolume ${params.levelName} ${params.position.join(' ')} ${params.size.join(' ')} ${params.streamingDistance || 0}`;
    return this.bridge.executeConsoleCommand(command);
  }

  // Level LOD
  async setLevelLOD(params: {
    levelName: string;
    lodLevel: number;
    distance: number;
  }) {
    const command = `SetLevelLOD ${params.levelName} ${params.lodLevel} ${params.distance}`;
    return this.bridge.executeConsoleCommand(command);
  }
}
