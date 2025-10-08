// Level management tools for Unreal Engine
import { UnrealBridge } from '../unreal-bridge.js';
import {
  coerceBoolean,
  coerceNumber,
  coerceString,
  interpretStandardResult
} from '../utils/result-helpers.js';

export class LevelTools {
  constructor(private bridge: UnrealBridge) {}

  // Load level (using LevelEditorSubsystem to avoid crashes)
  async loadLevel(params: {
    levelPath: string;
    streaming?: boolean;
    position?: [number, number, number];
  }) {
    if (params.streaming) {
      const python = `
import unreal
import json

result = {
  "success": False,
  "message": "",
  "error": "",
  "details": [],
  "warnings": []
}

try:
  ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
  world = ues.get_editor_world() if ues else None
  if world:
    try:
      unreal.EditorLevelUtils.add_level_to_world(world, r"${params.levelPath}", unreal.LevelStreamingKismet)
      result["success"] = True
      result["message"] = "Streaming level added"
      result["details"].append("Streaming level added via EditorLevelUtils")
    except Exception as add_error:
      result["error"] = f"Failed to add streaming level: {add_error}"
  else:
    result["error"] = "No editor world available"
except Exception as outer_error:
  result["error"] = f"Streaming level operation failed: {outer_error}"

if result["success"]:
  if not result["message"]:
    result["message"] = "Streaming level added"
else:
  if not result["error"]:
    result["error"] = result["message"] or "Failed to add streaming level"
  if not result["message"]:
    result["message"] = result["error"]

if not result["warnings"]:
  result.pop("warnings")
if not result["details"]:
  result.pop("details")
if result.get("error") is None:
  result.pop("error")

print("RESULT:" + json.dumps(result))
`.trim();

      try {
        const response = await this.bridge.executePython(python);
        const interpreted = interpretStandardResult(response, {
          successMessage: 'Streaming level added',
          failureMessage: 'Failed to add streaming level'
        });

        if (interpreted.success) {
          const result: Record<string, unknown> = {
            success: true,
            message: interpreted.message
          };
          if (interpreted.warnings?.length) {
            result.warnings = interpreted.warnings;
          }
          if (interpreted.details?.length) {
            result.details = interpreted.details;
          }
          return result;
        }
      } catch {}

      return this.bridge.executeConsoleCommand(`LoadStreamLevel ${params.levelPath}`);
    } else {
      const python = `
import unreal
import json

result = {
  "success": False,
  "message": "",
  "error": "",
  "warnings": [],
  "details": [],
  "level": r"${params.levelPath}"
}

try:
  level_path = r"${params.levelPath}"
  asset_path = level_path
  try:
    tail = asset_path.rsplit('/', 1)[-1]
    if '.' not in tail:
      asset_path = f"{asset_path}.{tail}"
  except Exception:
    pass

  asset_exists = False
  try:
    asset_exists = unreal.EditorAssetLibrary.does_asset_exist(asset_path)
  except Exception:
    asset_exists = False

  if not asset_exists:
    result["error"] = f"Level not found: {asset_path}"
  else:
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if les:
      success = les.load_level(level_path)
      if success:
        result["success"] = True
        result["message"] = "Level loaded successfully"
        result["details"].append("Level loaded via LevelEditorSubsystem")
      else:
        result["error"] = "Failed to load level"
    else:
      result["error"] = "LevelEditorSubsystem not available"
except Exception as err:
  result["error"] = f"Failed to load level: {err}"

if result["success"]:
  if not result["message"]:
    result["message"] = "Level loaded successfully"
else:
  if not result["error"]:
    result["error"] = "Failed to load level"
  if not result["message"]:
    result["message"] = result["error"]

if not result["warnings"]:
  result.pop("warnings")
if not result["details"]:
  result.pop("details")
if result.get("error") is None:
  result.pop("error")

print("RESULT:" + json.dumps(result))
`.trim();

      try {
        const response = await this.bridge.executePython(python);
        const interpreted = interpretStandardResult(response, {
          successMessage: `Level ${params.levelPath} loaded`,
          failureMessage: `Failed to load level ${params.levelPath}`
        });
        const payloadLevel = coerceString(interpreted.payload.level) ?? params.levelPath;

        if (interpreted.success) {
          const result: Record<string, unknown> = {
            success: true,
            message: interpreted.message,
            level: payloadLevel
          };
          if (interpreted.warnings?.length) {
            result.warnings = interpreted.warnings;
          }
          if (interpreted.details?.length) {
            result.details = interpreted.details;
          }
          return result;
        }

        const failure: Record<string, unknown> = {
          success: false,
          error: interpreted.error || interpreted.message,
          level: payloadLevel
        };
        if (interpreted.warnings?.length) {
          failure.warnings = interpreted.warnings;
        }
        if (interpreted.details?.length) {
          failure.details = interpreted.details;
        }
        return failure;
      } catch (e) {
        return { success: false, error: `Failed to load level: ${e}` };
      }
    }
  }

  // Save current level
  async saveLevel(_params: {
    levelName?: string;
    savePath?: string;
  }) {
    const python = `
import unreal
import json

result = {
  "success": False,
  "message": "",
  "error": "",
  "warnings": [],
  "details": [],
  "skipped": False,
  "reason": ""
}

def print_result(payload):
  data = dict(payload)
  if data.get("skipped") and not data.get("message"):
    data["message"] = data.get("reason") or "Level save skipped"
  if data.get("success") and not data.get("message"):
    data["message"] = "Level saved"
  if not data.get("success"):
    if not data.get("error"):
      data["error"] = data.get("message") or "Failed to save level"
    if not data.get("message"):
      data["message"] = data.get("error") or "Failed to save level"
  if data.get("success"):
    data.pop("error", None)
  if not data.get("warnings"):
    data.pop("warnings", None)
  if not data.get("details"):
    data.pop("details", None)
  if not data.get("skipped"):
    data.pop("skipped", None)
    data.pop("reason", None)
  else:
    if not data.get("reason"):
      data.pop("reason", None)
  print("RESULT:" + json.dumps(data))

try:
  # Attempt to reduce source control prompts (best-effort, may be a no-op depending on UE version)
  try:
    prefs = unreal.SourceControlPreferences()
    muted = False
    try:
      prefs.set_enable_source_control(False)
      muted = True
    except Exception:
      try:
        prefs.enable_source_control = False
        muted = True
      except Exception:
        muted = False
    if muted:
      result["details"].append("Source control prompts disabled")
  except Exception:
    pass

  # Determine if level is dirty and save via LevelEditorSubsystem when possible
  world = None
  try:
    world = unreal.EditorSubsystemLibrary.get_editor_world()
  except Exception:
    try:
      ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
      world = ues.get_editor_world() if ues else None
    except Exception:
      world = None

  pkg_path = None
  try:
    if world is not None:
      full = world.get_path_name()
      pkg_path = full.split('.')[0] if '.' in full else full
      if pkg_path:
        result["details"].append(f"Detected level package: {pkg_path}")
  except Exception:
    pkg_path = None

  skip_save = False
  try:
    is_dirty = None
    if pkg_path:
      editor_asset_lib = getattr(unreal, 'EditorAssetLibrary', None)
      if editor_asset_lib and hasattr(editor_asset_lib, 'is_asset_dirty'):
        try:
          is_dirty = editor_asset_lib.is_asset_dirty(pkg_path)
        except Exception as check_error:
          result["warnings"].append(f"EditorAssetLibrary.is_asset_dirty failed: {check_error}")
          is_dirty = None
      if is_dirty is None and world is not None:
        # Fallback: inspect the current level via the active world (avoids deprecated EditorLevelLibrary)
        try:
          level = world.get_current_level() if hasattr(world, 'get_current_level') else None
          package = level.get_outermost() if level and hasattr(level, 'get_outermost') else None
          if package and hasattr(package, 'is_dirty'):
            is_dirty = package.is_dirty()
        except Exception as fallback_error:
          result["warnings"].append(f"Fallback dirty check failed: {fallback_error}")
    if is_dirty is False:
      result["success"] = True
      result["skipped"] = True
      result["reason"] = "Level not dirty"
      result["message"] = "Level save skipped"
      skip_save = True
    elif is_dirty is None and pkg_path:
      result["warnings"].append("Unable to determine level dirty state; attempting save anyway")
  except Exception as dirty_error:
    result["warnings"].append(f"Failed to check level dirty state: {dirty_error}")

  if not skip_save:
    saved = False
    try:
      les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
      if les:
        les.save_current_level()
        saved = True
        result["details"].append("Level saved via LevelEditorSubsystem")
    except Exception as save_error:
      result["error"] = f"Level save failed: {save_error}"
      saved = False

    if not saved:
      raise Exception('LevelEditorSubsystem not available')

    result["success"] = True
    if not result["message"]:
      result["message"] = "Level saved"
except Exception as err:
  result["error"] = str(err)

print_result(result)
`.trim();

    try {
      const response = await this.bridge.executePython(python);
      const interpreted = interpretStandardResult(response, {
        successMessage: 'Level saved',
        failureMessage: 'Failed to save level'
      });

      if (interpreted.success) {
        const result: Record<string, unknown> = {
          success: true,
          message: interpreted.message
        };
        const skipped = coerceBoolean(interpreted.payload.skipped);
        if (typeof skipped === 'boolean') {
          result.skipped = skipped;
        }
        const reason = coerceString(interpreted.payload.reason);
        if (reason) {
          result.reason = reason;
        }
        if (interpreted.warnings?.length) {
          result.warnings = interpreted.warnings;
        }
        if (interpreted.details?.length) {
          result.details = interpreted.details;
        }
        return result;
      }

      const failure: Record<string, unknown> = {
        success: false,
        error: interpreted.error || interpreted.message
      };
      if (interpreted.message && interpreted.message !== failure.error) {
        failure.message = interpreted.message;
      }
      const skippedFailure = coerceBoolean(interpreted.payload.skipped);
      if (typeof skippedFailure === 'boolean') {
        failure.skipped = skippedFailure;
      }
      const failureReason = coerceString(interpreted.payload.reason);
      if (failureReason) {
        failure.reason = failureReason;
      }
      if (interpreted.warnings?.length) {
        failure.warnings = interpreted.warnings;
      }
      if (interpreted.details?.length) {
        failure.details = interpreted.details;
      }

      return failure;
    } catch (e) {
      return { success: false, error: `Failed to save level: ${e}` };
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
    const python = `
import unreal
import json

result = {
  "success": False,
  "message": "",
  "error": "",
  "warnings": [],
  "details": [],
  "path": r"${fullPath}",
  "partitioned": ${isPartitioned ? 'True' : 'False'}
}

try:
  les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
  if les:
    les.new_level(r"${fullPath}", ${isPartitioned ? 'True' : 'False'})
    result["success"] = True
    result["message"] = "Level created"
    result["details"].append("Level created via LevelEditorSubsystem.new_level")
  else:
    result["error"] = "LevelEditorSubsystem not available"
except Exception as err:
  result["error"] = f"Level creation failed: {err}"

if result["success"]:
  if not result["message"]:
    result["message"] = "Level created"
else:
  if not result["error"]:
    result["error"] = "Failed to create level"
  if not result["message"]:
    result["message"] = result["error"]

if not result["warnings"]:
  result.pop("warnings")
if not result["details"]:
  result.pop("details")
if result.get("error") is None:
  result.pop("error")

print("RESULT:" + json.dumps(result))
`.trim();

    try {
      const response = await this.bridge.executePython(python);
      const interpreted = interpretStandardResult(response, {
        successMessage: 'Level created',
        failureMessage: 'Failed to create level'
      });

      const path = coerceString(interpreted.payload.path) ?? fullPath;
      const partitioned = coerceBoolean(interpreted.payload.partitioned, isPartitioned) ?? isPartitioned;

      if (interpreted.success) {
        const result: Record<string, unknown> = {
          success: true,
          message: interpreted.message,
          path,
          partitioned
        };
        if (interpreted.warnings?.length) {
          result.warnings = interpreted.warnings;
        }
        if (interpreted.details?.length) {
          result.details = interpreted.details;
        }
        return result;
      }

      const failure: Record<string, unknown> = {
        success: false,
        error: interpreted.error || interpreted.message,
        path,
        partitioned
      };
      if (interpreted.warnings?.length) {
        failure.warnings = interpreted.warnings;
      }
      if (interpreted.details?.length) {
        failure.details = interpreted.details;
      }

      return failure;
    } catch (e) {
      return { success: false, error: `Failed to create level: ${e}` };
    }
  }

  // Stream level (Python attempt with fallback)
  async streamLevel(params: {
    levelPath?: string;
    levelName?: string;
    shouldBeLoaded: boolean;
    shouldBeVisible?: boolean;
    position?: [number, number, number];
  }) {
    const rawPath = typeof params.levelPath === 'string' ? params.levelPath.trim() : '';
    const levelPath = rawPath.length > 0 ? rawPath : undefined;
    const providedName = typeof params.levelName === 'string' ? params.levelName.trim() : '';
    const derivedName = providedName.length > 0
      ? providedName
      : (levelPath ? levelPath.split('/').filter(Boolean).pop() ?? '' : '');
    const levelName = derivedName.length > 0 ? derivedName : undefined;
    const qualifiedName = levelPath && levelName && !levelName.includes('.')
      ? `${levelPath}.${levelName}`
      : undefined;
    const shouldBeVisible = params.shouldBeVisible ?? params.shouldBeLoaded;

    const levelLiteral = JSON.stringify(levelName ?? '');
    const levelPathLiteral = JSON.stringify(levelPath ?? '');
    const qualifiedLiteral = JSON.stringify(qualifiedName ?? '');
    const loadLiteral = params.shouldBeLoaded ? 'True' : 'False';
    const visibleLiteral = shouldBeVisible ? 'True' : 'False';

    const python = `
import unreal
import json

result = {
  "success": False,
  "message": "",
  "error": "",
  "warnings": [],
  "details": [],
  "level": ${levelLiteral},
  "level_path": ${levelPathLiteral},
  "qualified_name": ${qualifiedLiteral},
  "loaded": ${loadLiteral},
  "visible": ${visibleLiteral}
}

try:
  ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
  world = ues.get_editor_world() if ues else None
  if world:
    updated = False
    streaming_levels = []
    try:
      if hasattr(world, 'get_streaming_levels'):
        streaming_levels = list(world.get_streaming_levels() or [])
    except Exception as primary_error:
      result["warnings"].append(f"get_streaming_levels unavailable: {primary_error}")

    if not streaming_levels:
      try:
        if hasattr(world, 'get_level_streaming_levels'):
          streaming_levels = list(world.get_level_streaming_levels() or [])
      except Exception as alt_error:
        result["warnings"].append(f"get_level_streaming_levels unavailable: {alt_error}")

    if not streaming_levels:
      try:
        fallback_levels = getattr(world, 'streaming_levels', None)
        if fallback_levels is not None:
          streaming_levels = list(fallback_levels)
      except Exception as attr_error:
        result["warnings"].append(f"streaming_levels attribute unavailable: {attr_error}")

    if not streaming_levels:
      result["error"] = "Streaming levels unavailable"
    else:
      target_candidates = []
      try:
        level_path = (result.get("level_path") or "").replace('\\', '/')
        level_name = (result.get("level") or "").replace('\\', '/')
        qualified_name = (result.get("qualified_name") or "").replace('\\', '/')

        candidates_raw = [level_path, level_name, qualified_name]

        if level_path:
          tail = level_path.split('/')[-1]
          if tail:
            candidates_raw.append(tail)
            if '.' not in tail:
              candidates_raw.append(f"{level_path}.{tail}")

        if level_name and '/' in level_name:
          tail = level_name.split('/')[-1]
          if tail:
            candidates_raw.append(tail)
            if '.' not in tail and level_path:
              candidates_raw.append(f"{level_path}.{tail}")

        target_candidates = [cand for cand in candidates_raw if cand]
      except Exception as candidate_error:
        target_candidates = []
        result["warnings"].append(f"Failed to build target candidates: {candidate_error}")

      for streaming_level in streaming_levels:
        try:
          name = None
          if hasattr(streaming_level, 'get_world_asset_package_name'):
            name = streaming_level.get_world_asset_package_name()
          if not name:
            try:
              name = str(streaming_level.get_editor_property('world_asset'))
            except Exception:
              name = None

          normalized_name = str(name).replace('\\', '/') if name else None
          matches = False
          if normalized_name and target_candidates:
            for candidate in target_candidates:
              if normalized_name == candidate or normalized_name.endswith('/' + candidate) or normalized_name.endswith(candidate):
                matches = True
                break

          if matches:
            try:
              streaming_level.set_should_be_loaded(${loadLiteral})
            except Exception as load_error:
              result["warnings"].append(f"Failed to set loaded flag: {load_error}")
            try:
              streaming_level.set_should_be_visible(${visibleLiteral})
            except Exception as visible_error:
              result["warnings"].append(f"Failed to set visibility: {visible_error}")
            updated = True
            break
        except Exception as iteration_error:
          result["warnings"].append(f"Streaming level iteration error: {iteration_error}")

      if updated:
        result["success"] = True
        result["message"] = "Streaming level updated"
        result["details"].append("Streaming level flags updated for editor world")
      else:
        result["error"] = "Streaming level not found"
  else:
    result["error"] = "No editor world available"
except Exception as err:
  result["error"] = f"Streaming level update failed: {err}"

if result["success"]:
  if not result["message"]:
    result["message"] = "Streaming level updated"
else:
  if not result["error"]:
    result["error"] = "Streaming level update failed"
  if not result["message"]:
    result["message"] = result["error"]

if not result["warnings"]:
  result.pop("warnings")
if not result["details"]:
  result.pop("details")
result.pop("qualified_name", None)
if result.get("error") is None:
  result.pop("error")

print("RESULT:" + json.dumps(result))
`.trim();

    try {
      const response = await this.bridge.executePython(python);
      const interpreted = interpretStandardResult(response, {
        successMessage: 'Streaming level updated',
        failureMessage: 'Streaming level update failed'
      });

      const payloadLevelPath = coerceString((interpreted.payload as Record<string, unknown>)['level_path'])
        ?? levelPath
        ?? undefined;
      const payloadLevel = coerceString(interpreted.payload.level)
        ?? levelName
        ?? (payloadLevelPath ? payloadLevelPath.split('/').filter(Boolean).pop() : undefined)
        ?? '';
      const loaded = coerceBoolean(interpreted.payload.loaded, params.shouldBeLoaded) ?? params.shouldBeLoaded;
      const visible = coerceBoolean(interpreted.payload.visible, shouldBeVisible) ?? shouldBeVisible;

      if (interpreted.success) {
        const result: Record<string, unknown> = {
          success: true,
          message: interpreted.message,
          level: payloadLevel,
          levelPath: payloadLevelPath,
          loaded,
          visible
        };
        if (interpreted.warnings?.length) {
          result.warnings = interpreted.warnings;
        }
        if (interpreted.details?.length) {
          result.details = interpreted.details;
        }
        return result;
      }

      const failure: Record<string, unknown> = {
        success: false,
        error: interpreted.error || interpreted.message || 'Streaming level update failed',
        level: payloadLevel,
        levelPath: payloadLevelPath,
        loaded,
        visible
      };
      if (interpreted.message && interpreted.message !== failure.error) {
        failure.message = interpreted.message;
      }
      if (interpreted.warnings?.length) {
        failure.warnings = interpreted.warnings;
      }
      if (interpreted.details?.length) {
        failure.details = interpreted.details;
      }
      return failure;
    } catch {
      const levelIdentifier = levelName ?? levelPath ?? '';
      const simpleName = levelIdentifier.split('/').filter(Boolean).pop() || levelIdentifier;
      const loadCmd = params.shouldBeLoaded ? 'Load' : 'Unload';
      const visCmd = shouldBeVisible ? 'Show' : 'Hide';
      const command = `StreamLevel ${simpleName} ${loadCmd} ${visCmd}`;
      return this.bridge.executeConsoleCommand(command);
    }
  }

  // World composition
  async setupWorldComposition(params: {
    enableComposition: boolean;
    tileSize?: number;
    distanceStreaming?: boolean;
    streamingDistance?: number;
  }) {
  const commands: string[] = [];
    
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
    
    await this.bridge.executeConsoleCommands(commands);
    
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
  const commands: string[] = [];
    
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
    
    await this.bridge.executeConsoleCommands(commands);
    
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
    const python = `
import unreal
import json

result = {
  "success": False,
  "message": "",
  "error": "",
  "warnings": [],
  "details": [],
  "rebuildAll": ${params.rebuildAll ? 'True' : 'False'},
  "selectedOnly": ${params.selectedOnly ? 'True' : 'False'},
  "selectionCount": 0
}

try:
  nav_system = unreal.EditorSubsystemLibrary.get_editor_subsystem(unreal.NavigationSystemV1)
  if not nav_system:
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = ues.get_editor_world() if ues else None
    nav_system = unreal.NavigationSystemV1.get_navigation_system(world) if world else None

  if nav_system:
    if ${params.rebuildAll ? 'True' : 'False'}:
      nav_system.navigation_build_async()
      result["success"] = True
      result["message"] = "Navigation rebuild started"
      result["details"].append("Triggered full navigation rebuild")
    else:
      actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
      selected_actors = actor_subsystem.get_selected_level_actors() if actor_subsystem else []
      result["selectionCount"] = len(selected_actors) if selected_actors else 0

      if ${params.selectedOnly ? 'True' : 'False'} and selected_actors:
        for actor in selected_actors:
          nav_system.update_nav_octree(actor)
        result["success"] = True
        result["message"] = f"Navigation updated for {len(selected_actors)} actors"
        result["details"].append("Updated nav octree for selected actors")
      elif selected_actors:
        for actor in selected_actors:
          nav_system.update_nav_octree(actor)
        nav_system.update(0.0)
        result["success"] = True
        result["message"] = f"Navigation updated for {len(selected_actors)} actors"
        result["details"].append("Updated nav octree and performed incremental update")
      else:
        nav_system.update(0.0)
        result["success"] = True
        result["message"] = "Navigation incremental update performed"
        result["details"].append("No selected actors; performed incremental update")
  else:
    result["error"] = "Navigation system not available. Add a NavMeshBoundsVolume to the level first."
except AttributeError as attr_error:
  result["error"] = f"Navigation API not available: {attr_error}"
except Exception as err:
  result["error"] = f"Navigation build failed: {err}"

if result["success"]:
  if not result["message"]:
    result["message"] = "Navigation build started"
else:
  if not result["error"]:
    result["error"] = result["message"] or "Navigation build failed"
  if not result["message"]:
    result["message"] = result["error"]

if not result["warnings"]:
  result.pop("warnings")
if not result["details"]:
  result.pop("details")
if result.get("error") is None:
  result.pop("error")

if not result.get("selectionCount"):
  result.pop("selectionCount", None)

print("RESULT:" + json.dumps(result))
`.trim();

    try {
      const response = await this.bridge.executePython(python);
      const interpreted = interpretStandardResult(response, {
        successMessage: params.rebuildAll ? 'Navigation rebuild started' : 'Navigation update started',
        failureMessage: 'Navigation build failed'
      });

      const result: Record<string, unknown> = interpreted.success
        ? { success: true, message: interpreted.message }
        : { success: false, error: interpreted.error || interpreted.message };

      const rebuildAll = coerceBoolean(interpreted.payload.rebuildAll, params.rebuildAll);
      const selectedOnly = coerceBoolean(interpreted.payload.selectedOnly, params.selectedOnly);
      if (typeof rebuildAll === 'boolean') {
        result.rebuildAll = rebuildAll;
      } else if (typeof params.rebuildAll === 'boolean') {
        result.rebuildAll = params.rebuildAll;
      }
      if (typeof selectedOnly === 'boolean') {
        result.selectedOnly = selectedOnly;
      } else if (typeof params.selectedOnly === 'boolean') {
        result.selectedOnly = params.selectedOnly;
      }

      const selectionCount = coerceNumber(interpreted.payload.selectionCount);
      if (typeof selectionCount === 'number') {
        result.selectionCount = selectionCount;
      }

      if (interpreted.warnings?.length) {
        result.warnings = interpreted.warnings;
      }
      if (interpreted.details?.length) {
        result.details = interpreted.details;
      }

      return result;
    } catch (e) {
      return {
        success: false,
        error: `Navigation build not available: ${e}. Please ensure a NavMeshBoundsVolume exists in the level.`
      };
    }
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

