import { UnrealBridge } from '../unreal-bridge.js';
import { validateAssetParams } from '../utils/validation.js';
import {
  interpretStandardResult,
  coerceBoolean,
  coerceString,
  coerceStringArray
} from '../utils/result-helpers.js';

type CreateAnimationBlueprintSuccess = {
  success: true;
  message: string;
  path: string;
  exists?: boolean;
  skeleton?: string;
  warnings?: string[];
  details?: string[];
};

type CreateAnimationBlueprintFailure = {
  success: false;
  message: string;
  error: string;
  path?: string;
  exists?: boolean;
  skeleton?: string;
  warnings?: string[];
  details?: string[];
};

type PlayAnimationSuccess = {
  success: true;
  message: string;
  warnings?: string[];
  details?: string[];
  actorName?: string;
  animationType?: string;
  assetPath?: string;
};

type PlayAnimationFailure = {
  success: false;
  message: string;
  error: string;
  warnings?: string[];
  details?: string[];
  availableActors?: string[];
  actorName?: string;
  animationType?: string;
  assetPath?: string;
};

export class AnimationTools {
  constructor(private bridge: UnrealBridge) {}

  async createAnimationBlueprint(params: {
    name: string;
    skeletonPath: string;
    savePath?: string;
  }): Promise<CreateAnimationBlueprintSuccess | CreateAnimationBlueprintFailure> {
    try {
      const targetPath = params.savePath ?? '/Game/Animations';
      const validation = validateAssetParams({ name: params.name, savePath: targetPath });
      if (!validation.valid) {
        const message = validation.error ?? 'Invalid asset parameters';
        return { success: false, message, error: message };
      }

      const sanitized = validation.sanitized;
      const assetName = sanitized.name;
      const assetPath = sanitized.savePath ?? targetPath;
      const script = this.buildCreateAnimationBlueprintScript({
        name: assetName,
        path: assetPath,
        skeletonPath: params.skeletonPath
      });

      const response = await this.bridge.executePython(script);
      return this.parseAnimationBlueprintResponse(response, assetName, assetPath);
    } catch (err) {
      const error = `Failed to create Animation Blueprint: ${err}`;
      return { success: false, message: error, error: String(err) };
    }
  }

  async addStateMachine(params: {
    blueprintPath: string;
    machineName: string;
    states: Array<{
      name: string;
      animation?: string;
      isEntry?: boolean;
      isExit?: boolean;
    }>;
    transitions?: Array<{
      sourceState: string;
      targetState: string;
      condition?: string;
    }>;
  }): Promise<{ success: true; message: string } | { success: false; error: string }> {
    try {
      if (!params.blueprintPath || !params.machineName) {
        return { success: false, error: 'blueprintPath and machineName are required' };
      }

      const commands: string[] = [
        `AddAnimStateMachine ${params.blueprintPath} ${params.machineName}`
      ];

      for (const state of params.states) {
        const animationName = state.animation ?? '';
        commands.push(
          `AddAnimState ${params.blueprintPath} ${params.machineName} ${state.name} ${animationName}`
        );
        if (state.isEntry) {
          commands.push(`SetAnimStateEntry ${params.blueprintPath} ${params.machineName} ${state.name}`);
        }
        if (state.isExit) {
          commands.push(`SetAnimStateExit ${params.blueprintPath} ${params.machineName} ${state.name}`);
        }
      }

      if (params.transitions) {
        for (const transition of params.transitions) {
          commands.push(
            `AddAnimTransition ${params.blueprintPath} ${params.machineName} ${transition.sourceState} ${transition.targetState}`
          );
          if (transition.condition) {
            commands.push(
              `SetAnimTransitionRule ${params.blueprintPath} ${params.machineName} ${transition.sourceState} ${transition.targetState} ${transition.condition}`
            );
          }
        }
      }

      await this.bridge.executeConsoleCommands(commands);
      return {
        success: true,
        message: `State machine ${params.machineName} added to ${params.blueprintPath}`
      };
    } catch (err) {
      return { success: false, error: `Failed to add state machine: ${err}` };
    }
  }

  async createBlendSpace(params: {
    name: string;
    savePath?: string;
    dimensions?: 1 | 2;
    skeletonPath?: string;
    horizontalAxis?: { name: string; minValue: number; maxValue: number };
    verticalAxis?: { name: string; minValue: number; maxValue: number };
    samples?: Array<{ animation: string; x: number; y?: number }>;
  }): Promise<{ success: true; message: string; path: string } | { success: false; error: string }> {
    try {
      const targetPath = params.savePath ?? '/Game/Animations';
      const validation = validateAssetParams({ name: params.name, savePath: targetPath });
      if (!validation.valid) {
        return { success: false, error: validation.error ?? 'Invalid asset parameters' };
      }

      const sanitized = validation.sanitized;
      const assetName = sanitized.name;
      const assetPath = sanitized.savePath ?? targetPath;
      const dimensions = params.dimensions === 2 ? 2 : 1;
      const blendSpaceType = dimensions === 2 ? 'BlendSpace' : 'BlendSpace1D';

      const commands: string[] = [
        `CreateAsset ${blendSpaceType} ${assetName} ${assetPath}`,
        `echo Creating ${blendSpaceType} ${assetName} at ${assetPath}`
      ];

      if (params.skeletonPath) {
        commands.push(`SetBlendSpaceSkeleton ${assetName} ${params.skeletonPath}`);
      }

      if (params.horizontalAxis) {
        commands.push(
          `SetBlendSpaceAxis ${assetName} Horizontal ${params.horizontalAxis.name} ${params.horizontalAxis.minValue} ${params.horizontalAxis.maxValue}`
        );
      }

      if (dimensions === 2 && params.verticalAxis) {
        commands.push(
          `SetBlendSpaceAxis ${assetName} Vertical ${params.verticalAxis.name} ${params.verticalAxis.minValue} ${params.verticalAxis.maxValue}`
        );
      }

      if (params.samples) {
        for (const sample of params.samples) {
          const coords = dimensions === 1 ? `${sample.x}` : `${sample.x} ${sample.y ?? 0}`;
          commands.push(`AddBlendSpaceSample ${assetName} ${sample.animation} ${coords}`);
        }
      }

      await this.bridge.executeConsoleCommands(commands);
      return {
        success: true,
        message: `Blend Space ${assetName} created`,
        path: `${assetPath}/${assetName}`
      };
    } catch (err) {
      return { success: false, error: `Failed to create blend space: ${err}` };
    }
  }

  async setupControlRig(params: {
    name: string;
    skeletonPath: string;
    savePath?: string;
    controls?: Array<{
      name: string;
      type: 'Transform' | 'Float' | 'Bool' | 'Vector';
      bone?: string;
      defaultValue?: unknown;
    }>;
  }): Promise<{ success: true; message: string; path: string } | { success: false; error: string }> {
    try {
      const targetPath = params.savePath ?? '/Game/Animations';
      const validation = validateAssetParams({ name: params.name, savePath: targetPath });
      if (!validation.valid) {
        return { success: false, error: validation.error ?? 'Invalid asset parameters' };
      }

      const sanitized = validation.sanitized;
      const assetName = sanitized.name;
      const assetPath = sanitized.savePath ?? targetPath;
      const fullPath = `${assetPath}/${assetName}`;

      const commands: string[] = [
        `CreateAsset ControlRig ${assetName} ${assetPath}`,
        `SetControlRigSkeleton ${assetName} ${params.skeletonPath}`
      ];

      if (params.controls) {
        for (const control of params.controls) {
          commands.push(
            `AddControlRigControl ${assetName} ${control.name} ${control.type} ${control.bone ?? ''}`
          );
          if (control.defaultValue !== undefined) {
            commands.push(
              `SetControlRigDefault ${assetName} ${control.name} ${JSON.stringify(control.defaultValue)}`
            );
          }
        }
      }

      await this.bridge.executeConsoleCommands(commands);
      return {
        success: true,
        message: `Control Rig ${assetName} created`,
        path: fullPath
      };
    } catch (err) {
      return { success: false, error: `Failed to setup control rig: ${err}` };
    }
  }

  async createLevelSequence(params: {
    name: string;
    savePath?: string;
    frameRate?: number;
    duration?: number;
    tracks?: Array<{
      actorName: string;
      trackType: 'Transform' | 'Animation' | 'Camera' | 'Event';
      keyframes?: Array<{ time: number; value: unknown }>;
    }>;
  }): Promise<{ success: true; message: string; path: string } | { success: false; error: string }> {
    try {
      const targetPath = params.savePath ?? '/Game/Cinematics';
      const validation = validateAssetParams({ name: params.name, savePath: targetPath });
      if (!validation.valid) {
        return { success: false, error: validation.error ?? 'Invalid asset parameters' };
      }

      const sanitized = validation.sanitized;
      const assetName = sanitized.name;
      const assetPath = sanitized.savePath ?? targetPath;

      const commands: string[] = [
        `CreateAsset LevelSequence ${assetName} ${assetPath}`,
        `SetSequenceFrameRate ${assetName} ${params.frameRate ?? 30}`,
        `SetSequenceDuration ${assetName} ${params.duration ?? 5}`
      ];

      if (params.tracks) {
        for (const track of params.tracks) {
          commands.push(`AddSequenceTrack ${assetName} ${track.actorName} ${track.trackType}`);
          if (track.keyframes) {
            for (const keyframe of track.keyframes) {
              commands.push(
                `AddSequenceKey ${assetName} ${track.actorName} ${track.trackType} ${keyframe.time} ${JSON.stringify(keyframe.value)}`
              );
            }
          }
        }
      }

      await this.bridge.executeConsoleCommands(commands);
      return {
        success: true,
        message: `Level Sequence ${assetName} created`,
        path: `${assetPath}/${assetName}`
      };
    } catch (err) {
      return { success: false, error: `Failed to create level sequence: ${err}` };
    }
  }

  async playAnimation(params: {
    actorName: string;
    animationType: 'Montage' | 'Sequence' | 'BlendSpace';
    animationPath: string;
    playRate?: number;
    loop?: boolean;
    blendInTime?: number;
    blendOutTime?: number;
  }): Promise<PlayAnimationSuccess | PlayAnimationFailure> {
    try {
      const script = this.buildPlayAnimationScript({
        actorName: params.actorName,
        animationType: params.animationType,
        animationPath: params.animationPath,
        playRate: params.playRate ?? 1.0,
        loop: params.loop ?? false,
        blendInTime: params.blendInTime ?? 0.25,
        blendOutTime: params.blendOutTime ?? 0.25
      });

      const response = await this.bridge.executePython(script);
      const interpreted = interpretStandardResult(response, {
        successMessage: `Animation ${params.animationType} triggered on ${params.actorName}`,
        failureMessage: `Failed to play animation on ${params.actorName}`
      });

      const payload = interpreted.payload ?? {};
      const warnings = interpreted.warnings ?? coerceStringArray((payload as any).warnings) ?? undefined;
      const details = interpreted.details ?? coerceStringArray((payload as any).details) ?? undefined;
      const availableActors = coerceStringArray((payload as any).availableActors);
      const actorName = coerceString((payload as any).actorName) ?? params.actorName;
      const animationType = coerceString((payload as any).animationType) ?? params.animationType;
      const assetPath = coerceString((payload as any).assetPath) ?? params.animationPath;
      const errorMessage = coerceString((payload as any).error) ?? interpreted.error ?? `Animation playback failed for ${params.actorName}`;

      if (interpreted.success) {
        const result: PlayAnimationSuccess = {
          success: true,
          message: interpreted.message
        };

        if (warnings && warnings.length > 0) {
          result.warnings = warnings;
        }
        if (details && details.length > 0) {
          result.details = details;
        }
        if (actorName) {
          result.actorName = actorName;
        }
        if (animationType) {
          result.animationType = animationType;
        }
        if (assetPath) {
          result.assetPath = assetPath;
        }

        return result;
      }

      const failure: PlayAnimationFailure = {
        success: false,
        message: `Failed to play animation: ${errorMessage}`,
        error: errorMessage
      };

      if (warnings && warnings.length > 0) {
        failure.warnings = warnings;
      }
      if (details && details.length > 0) {
        failure.details = details;
      }
      if (availableActors && availableActors.length > 0) {
        failure.availableActors = availableActors;
      }
      if (actorName) {
        failure.actorName = actorName;
      }
      if (animationType) {
        failure.animationType = animationType;
      }
      if (assetPath) {
        failure.assetPath = assetPath;
      }

      return failure;
    } catch (err) {
      const error = `Failed to play animation: ${err}`;
      return { success: false, message: error, error: String(err) };
    }
  }

  private buildCreateAnimationBlueprintScript(args: {
    name: string;
    path: string;
    skeletonPath: string;
  }): string {
    const payload = JSON.stringify(args);
    return `
import unreal
import json
import traceback

params = json.loads(${JSON.stringify(payload)})

result = {
    "success": False,
    "message": "",
    "error": "",
    "warnings": [],
    "details": [],
    "exists": False,
    "skeleton": params.get("skeletonPath") or ""
}

try:
  asset_path = (params.get("path") or "/Game").rstrip('/')
  asset_name = params.get("name") or ""
  full_path = f"{asset_path}/{asset_name}"
  result["path"] = full_path

  editor_lib = unreal.EditorAssetLibrary
  asset_subsystem = None
  try:
    asset_subsystem = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
  except Exception:
    asset_subsystem = None

  skeleton_path = params.get("skeletonPath")
  skeleton_asset = None
  if skeleton_path:
    if editor_lib.does_asset_exist(skeleton_path):
      skeleton_asset = editor_lib.load_asset(skeleton_path)
      if skeleton_asset and isinstance(skeleton_asset, unreal.Skeleton):
        result["details"].append(f"Using skeleton: {skeleton_path}")
        result["skeleton"] = skeleton_path
      else:
        result["error"] = f"Skeleton asset invalid at {skeleton_path}"
        result["warnings"].append(result["error"])
        skeleton_asset = None
    else:
      result["error"] = f"Skeleton not found at {skeleton_path}"
      result["warnings"].append(result["error"])

    if not skeleton_asset:
      raise RuntimeError(result["error"] or f"Skeleton {skeleton_path} unavailable")

  does_exist = False
  try:
    if asset_subsystem and hasattr(asset_subsystem, 'does_asset_exist'):
      does_exist = asset_subsystem.does_asset_exist(full_path)
    else:
      does_exist = editor_lib.does_asset_exist(full_path)
  except Exception:
    does_exist = editor_lib.does_asset_exist(full_path)

  if does_exist:
    result["exists"] = True
    loaded = editor_lib.load_asset(full_path)
    if loaded:
      result["success"] = True
      result["message"] = f"Animation Blueprint already exists at {full_path}"
      result["details"].append(result["message"])
    else:
      result["error"] = f"Asset exists but could not be loaded: {full_path}"
      result["warnings"].append(result["error"])
  else:
    factory = unreal.AnimBlueprintFactory()
    if skeleton_asset:
      try:
        factory.target_skeleton = skeleton_asset
      except Exception as assign_error:
        result["warnings"].append(f"Unable to assign skeleton {skeleton_path}: {assign_error}")

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    created = asset_tools.create_asset(
      asset_name=asset_name,
      package_path=asset_path,
      asset_class=unreal.AnimBlueprint,
      factory=factory
    )

    if created:
      editor_lib.save_asset(full_path, only_if_is_dirty=False)
      result["success"] = True
      result["message"] = f"Animation Blueprint created at {full_path}"
      result["details"].append(result["message"])
    else:
      result["error"] = f"Failed to create Animation Blueprint {asset_name}"

except Exception as exc:
    result["error"] = str(exc)
    result["warnings"].append(result["error"])
    tb = traceback.format_exc()
    if tb:
        result.setdefault("details", []).append(tb)

if result["success"] and not result.get("message"):
    result["message"] = f"Animation Blueprint created at {result.get('path')}"

if not result["success"] and not result.get("error"):
    result["error"] = "Animation Blueprint creation failed"

if not result.get("warnings"):
    result.pop("warnings", None)
if not result.get("details"):
    result.pop("details", None)
if not result.get("error"):
    result.pop("error", None)

print('RESULT:' + json.dumps(result))
`.trim();
  }

  private parseAnimationBlueprintResponse(
    response: unknown,
    assetName: string,
    assetPath: string
  ): CreateAnimationBlueprintSuccess | CreateAnimationBlueprintFailure {
    const interpreted = interpretStandardResult(response, {
      successMessage: `Animation Blueprint ${assetName} created`,
      failureMessage: `Failed to create Animation Blueprint ${assetName}`
    });

    const payload = interpreted.payload ?? {};
    const path = coerceString((payload as any).path) ?? `${assetPath}/${assetName}`;
    const exists = coerceBoolean((payload as any).exists);
    const skeleton = coerceString((payload as any).skeleton);
    const warnings = interpreted.warnings ?? coerceStringArray((payload as any).warnings) ?? undefined;
    const details = interpreted.details ?? coerceStringArray((payload as any).details) ?? undefined;

    if (interpreted.success) {
      const result: CreateAnimationBlueprintSuccess = {
        success: true,
        message: interpreted.message,
        path
      };

      if (typeof exists === 'boolean') {
        result.exists = exists;
      }
      if (skeleton) {
        result.skeleton = skeleton;
      }
      if (warnings && warnings.length > 0) {
        result.warnings = warnings;
      }
      if (details && details.length > 0) {
        result.details = details;
      }

      return result;
    }

    const errorMessage = coerceString((payload as any).error) ?? interpreted.error ?? interpreted.message;

    const failure: CreateAnimationBlueprintFailure = {
      success: false,
      message: `Failed to create Animation Blueprint: ${errorMessage}`,
      error: errorMessage,
      path
    };

    if (typeof exists === 'boolean') {
      failure.exists = exists;
    }
    if (skeleton) {
      failure.skeleton = skeleton;
    }
    if (warnings && warnings.length > 0) {
      failure.warnings = warnings;
    }
    if (details && details.length > 0) {
      failure.details = details;
    }

    return failure;
  }

  private buildPlayAnimationScript(args: {
    actorName: string;
    animationType: string;
    animationPath: string;
    playRate: number;
    loop: boolean;
    blendInTime: number;
    blendOutTime: number;
  }): string {
    const payload = JSON.stringify(args);
    return `
import unreal
import json
import traceback

params = json.loads(${JSON.stringify(payload)})

result = {
    "success": False,
    "message": "",
    "error": "",
    "warnings": [],
    "details": [],
    "actorName": params.get("actorName"),
    "animationType": params.get("animationType"),
    "assetPath": params.get("animationPath"),
    "availableActors": []
}

try:
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = actor_subsystem.get_all_level_actors() if actor_subsystem else []
    target = None
    search = params.get("actorName") or ""
    search_lower = search.lower()

    for actor in actors:
        if not actor:
            continue
        name = (actor.get_name() or "").lower()
        label = (actor.get_actor_label() or "").lower()
        if search_lower and (search_lower == name or search_lower == label or search_lower in label):
            target = actor
            break

    if not target:
        result["error"] = f"Actor not found: {search}"
        result["warnings"].append("Actor search yielded no results")
        suggestions = []
        for actor in actors[:20]:
            try:
                suggestions.append(actor.get_actor_label())
            except Exception:
                continue
        if suggestions:
            result["availableActors"] = suggestions
    else:
        try:
            display_name = target.get_actor_label() or target.get_name()
            if display_name:
                result["actorName"] = display_name
        except Exception:
            pass

        skeletal_component = target.get_component_by_class(unreal.SkeletalMeshComponent)
        if not skeletal_component:
            try:
                skeletal_component = target.get_editor_property('mesh')
            except Exception:
                skeletal_component = None

        if not skeletal_component:
            result["error"] = "No SkeletalMeshComponent found on actor"
            result["warnings"].append("Actor lacks SkeletalMeshComponent")
        else:
            asset_path = params.get("animationPath")
            if not asset_path or not unreal.EditorAssetLibrary.does_asset_exist(asset_path):
                result["error"] = f"Animation asset not found: {asset_path}"
                result["warnings"].append("Animation asset missing")
            else:
                asset = unreal.EditorAssetLibrary.load_asset(asset_path)
                anim_type = params.get("animationType") or ""
                if anim_type == 'Montage':
                    anim_instance = skeletal_component.get_anim_instance()
                    if anim_instance:
                        try:
                            anim_instance.montage_play(asset, params.get("playRate", 1.0))
                            result["success"] = True
                            result["message"] = f"Montage playing on {result.get('actorName') or search}"
                            result["details"].append(result["message"])
                        except Exception as play_error:
                            result["error"] = f"Failed to play montage: {play_error}"
                            result["warnings"].append(result["error"])
                    else:
                        result["error"] = "AnimInstance not found on SkeletalMeshComponent"
                        result["warnings"].append(result["error"])
                elif anim_type == 'Sequence':
                    try:
                        skeletal_component.play_animation(asset, bool(params.get("loop")))
                        try:
                            anim_instance = skeletal_component.get_anim_instance()
                            if anim_instance:
                                anim_instance.set_play_rate(params.get("playRate", 1.0))
                        except Exception:
                            pass
                        result["success"] = True
                        result["message"] = f"Sequence playing on {result.get('actorName') or search}"
                        result["details"].append(result["message"])
                    except Exception as play_error:
                        result["error"] = f"Failed to play sequence: {play_error}"
                        result["warnings"].append(result["error"])
                else:
                    result["error"] = "BlendSpace playback requires Animation Blueprint support"
                    result["warnings"].append("Unsupported animation type for direct play")

except Exception as exc:
    result["error"] = str(exc)
    result["warnings"].append(result["error"])
    tb = traceback.format_exc()
    if tb:
        result["details"].append(tb)

if result["success"] and not result.get("message"):
    result["message"] = f"Animation {result.get('animationType')} triggered on {result.get('actorName') or params.get('actorName')}"

if not result["success"] and not result.get("error"):
    result["error"] = "Animation playback failed"

if not result.get("warnings"):
    result.pop("warnings", None)
if not result.get("details"):
    result.pop("details", None)
if not result.get("availableActors"):
    result.pop("availableActors", None)
if not result.get("error"):
    result.pop("error", None)

print('RESULT:' + json.dumps(result))
`.trim();
  }
}
