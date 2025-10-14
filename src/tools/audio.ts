// Audio tools for Unreal Engine
import JSON5 from 'json5';
import { UnrealBridge } from '../unreal-bridge.js';
import { escapePythonString } from '../utils/python.js';
import { allowPythonFallbackFromEnv } from '../utils/env.js';

export class AudioTools {
  constructor(private bridge: UnrealBridge) {}

  private interpretResult(
    resp: unknown,
    defaults: { successMessage: string; failureMessage: string }
  ): { success: true; message: string; details?: string } | { success: false; error: string; details?: string } {
    const normalizePayload = (
      payload: Record<string, unknown>
    ): { success: true; message: string; details?: string } | { success: false; error: string; details?: string } => {
      const warningsValue = payload?.warnings;
      const warningsText = Array.isArray(warningsValue)
        ? (warningsValue.length > 0 ? warningsValue.join('; ') : undefined)
        : typeof warningsValue === 'string' && warningsValue.trim() !== ''
          ? warningsValue
          : undefined;

      if (payload?.success === true) {
        const message = typeof payload?.message === 'string' && payload.message.trim() !== ''
          ? payload.message
          : defaults.successMessage;
        return {
          success: true as const,
          message,
          details: warningsText
        };
      }

      const error =
        (typeof payload?.error === 'string' && payload.error.trim() !== ''
          ? payload.error
          : undefined)
        ?? (typeof payload?.message === 'string' && payload.message.trim() !== ''
          ? payload.message
          : undefined)
        ?? defaults.failureMessage;

      return {
        success: false as const,
        error,
        details: warningsText
      };
    };

    if (resp && typeof resp === 'object') {
      const payload = resp as Record<string, unknown>;
      if ('success' in payload || 'error' in payload || 'message' in payload || 'warnings' in payload) {
        return normalizePayload(payload);
      }
    }

    const raw = typeof resp === 'string' ? resp : JSON.stringify(resp);

    const extractJson = (input: string): string | undefined => {
      const marker = 'RESULT:';
      const markerIndex = input.lastIndexOf(marker);
      if (markerIndex === -1) {
        return undefined;
      }

      const afterMarker = input.slice(markerIndex + marker.length);
      const firstBraceIndex = afterMarker.indexOf('{');
      if (firstBraceIndex === -1) {
        return undefined;
      }

      let depth = 0;
      let inString = false;
      let escapeNext = false;

      for (let i = firstBraceIndex; i < afterMarker.length; i++) {
        const char = afterMarker[i];

        if (inString) {
          if (escapeNext) {
            escapeNext = false;
            continue;
          }
          if (char === '\\') {
            escapeNext = true;
            continue;
          }
          if (char === '"') {
            inString = false;
          }
          continue;
        }

        if (char === '"') {
          inString = true;
          continue;
        }

        if (char === '{') {
          depth += 1;
        } else if (char === '}') {
          depth -= 1;
          if (depth === 0) {
            return afterMarker.slice(firstBraceIndex, i + 1);
          }
        }
      }

      const fallbackMatch = /\{[\s\S]*\}/.exec(afterMarker);
      return fallbackMatch ? fallbackMatch[0] : undefined;
    };

    const jsonPayload = extractJson(raw);

    if (jsonPayload) {
      const parseAttempts: Array<{ label: string; parser: () => unknown }> = [
        {
          label: 'json',
          parser: () => JSON.parse(jsonPayload)
        },
        {
          label: 'json5',
          parser: () => JSON5.parse(jsonPayload)
        }
      ];

      const sanitizedForJson5 = jsonPayload
        .replace(/\bTrue\b/g, 'true')
        .replace(/\bFalse\b/g, 'false')
        .replace(/\bNone\b/g, 'null');

      if (sanitizedForJson5 !== jsonPayload) {
        parseAttempts.push({ label: 'json5-sanitized', parser: () => JSON5.parse(sanitizedForJson5) });
      }

      const parseErrors: string[] = [];

      for (const attempt of parseAttempts) {
        try {
          const parsed = attempt.parser();
          if (parsed && typeof parsed === 'object') {
            return normalizePayload(parsed as Record<string, unknown>);
          }
        } catch (err) {
          parseErrors.push(`${attempt.label}: ${err instanceof Error ? err.message : String(err)}`);
        }
      }

      const errorMatch = /["']error["']\s*:\s*(?:"([^"\\]*(?:\\.[^"\\]*)*)"|'([^'\\]*(?:\\.[^'\\]*)*)')/i.exec(jsonPayload);
      const messageMatch = /["']message["']\s*:\s*(?:"([^"\\]*(?:\\.[^"\\]*)*)"|'([^'\\]*(?:\\.[^'\\]*)*)')/i.exec(jsonPayload);
      const fallbackText = errorMatch?.[1] ?? errorMatch?.[2] ?? messageMatch?.[1] ?? messageMatch?.[2];
      const errorText = fallbackText && fallbackText.trim().length > 0
        ? fallbackText.trim()
        : `${defaults.failureMessage}: ${parseErrors[0] ?? 'Unable to parse RESULT payload'}`;

      const snippet = jsonPayload.length > 240 ? `${jsonPayload.slice(0, 240)}…` : jsonPayload;
      const detailsParts: string[] = [];
      if (parseErrors.length > 0) {
        detailsParts.push(`Parse attempts failed: ${parseErrors.join('; ')}`);
      }
      detailsParts.push(`Raw payload: ${snippet}`);
      const detailsText = detailsParts.join(' | ');

      return {
        success: false as const,
        error: errorText,
        details: detailsText
      };
    }

    return { success: false as const, error: defaults.failureMessage };
  }

  // Create sound cue
  async createSoundCue(params: {
    name: string;
    wavePath?: string;
    savePath?: string;
    settings?: {
      volume?: number;
      pitch?: number;
      looping?: boolean;
      attenuationSettings?: string;
    };
  }) {
  const escapePyString = (value: string) => value.replace(/\\/g, '\\\\').replace(/"/g, '\\"');
  const toPyNumber = (value?: number) =>
    value === undefined || value === null || !Number.isFinite(value) ? 'None' : String(value);
  const toPyBool = (value?: boolean) =>
    value === undefined || value === null ? 'None' : value ? 'True' : 'False';

  const path = params.savePath || '/Game/Audio/Cues';
  const wavePath = params.wavePath || '';
  const attenuationPath = params.settings?.attenuationSettings || '';
  const volumeLiteral = toPyNumber(params.settings?.volume);
  const pitchLiteral = toPyNumber(params.settings?.pitch);
  const loopingLiteral = toPyBool(params.settings?.looping);

  const _py = `
import unreal
import json

name = r"${escapePyString(params.name)}"
package_path = r"${escapePyString(path)}"
wave_path = r"${escapePyString(wavePath)}"
attenuation_path = r"${escapePyString(attenuationPath)}"
attach_wave = ${params.wavePath ? 'True' : 'False'}
volume_override = ${volumeLiteral}
pitch_override = ${pitchLiteral}
looping_override = ${loopingLiteral}

result = {
  "success": False,
  "message": "",
  "error": "",
  "warnings": []
}

try:
  asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
  if not asset_tools:
    result["error"] = "AssetToolsHelpers unavailable"
    raise SystemExit(0)

  factory = None
  try:
    factory = unreal.SoundCueFactoryNew()
  except Exception:
    factory = None

  if not factory:
    result["error"] = "SoundCueFactoryNew unavailable"
    raise SystemExit(0)

  package_path = package_path.rstrip('/') if package_path else package_path

  asset = asset_tools.create_asset(
    asset_name=name,
    package_path=package_path,
    asset_class=unreal.SoundCue,
    factory=factory
  )

  if not asset:
    result["error"] = "Failed to create SoundCue"
    raise SystemExit(0)

  asset_subsystem = None
  try:
    asset_subsystem = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
  except Exception:
    asset_subsystem = None

  editor_library = unreal.EditorAssetLibrary

  if attach_wave:
    wave_exists = False
    try:
      if asset_subsystem and hasattr(asset_subsystem, "does_asset_exist"):
        wave_exists = asset_subsystem.does_asset_exist(wave_path)
      else:
        wave_exists = editor_library.does_asset_exist(wave_path)
    except Exception as existence_error:
      result["warnings"].append(f"Wave lookup failed: {existence_error}")

    if not wave_exists:
      result["warnings"].append(f"Wave asset not found: {wave_path}")
    else:
      try:
        if asset_subsystem and hasattr(asset_subsystem, "load_asset"):
          wave_asset = asset_subsystem.load_asset(wave_path)
        else:
          wave_asset = editor_library.load_asset(wave_path)
        if wave_asset:
          # Hooking up cue nodes via Python is non-trivial; surface warning for manual setup
          result["warnings"].append("Sound cue created without automatic wave node hookup")
      except Exception as wave_error:
        result["warnings"].append(f"Failed to load wave asset: {wave_error}")

  if volume_override is not None and hasattr(asset, "volume_multiplier"):
    asset.volume_multiplier = volume_override
  if pitch_override is not None and hasattr(asset, "pitch_multiplier"):
    asset.pitch_multiplier = pitch_override
  if looping_override is not None and hasattr(asset, "b_looping"):
    asset.b_looping = looping_override

  if attenuation_path:
    try:
      attenuation_asset = editor_library.load_asset(attenuation_path)
      if attenuation_asset:
        applied = False
        if hasattr(asset, "set_attenuation_settings"):
          try:
            asset.set_attenuation_settings(attenuation_asset)
            applied = True
          except Exception:
            applied = False
        if not applied and hasattr(asset, "attenuation_settings"):
          asset.attenuation_settings = attenuation_asset
          applied = True
        if not applied:
          result["warnings"].append("Attenuation asset loaded but could not be applied automatically")
    except Exception as attenuation_error:
      result["warnings"].append(f"Failed to apply attenuation: {attenuation_error}")

  try:
    save_target = f"{package_path}/{name}" if package_path else name
    if asset_subsystem and hasattr(asset_subsystem, "save_asset"):
      asset_subsystem.save_asset(save_target)
    else:
      editor_library.save_asset(save_target)
  except Exception as save_error:
    result["warnings"].append(f"Save failed: {save_error}")

  result["success"] = True
  result["message"] = "Sound cue created"

except SystemExit:
  pass
except Exception as error:
  result["error"] = str(error)

finally:
  payload = dict(result)
  if payload.get("success"):
    if not payload.get("message"):
      payload["message"] = "Sound cue created"
    payload.pop("error", None)
  else:
    if not payload.get("error"):
      payload["error"] = payload.get("message") or "Failed to create SoundCue"
    if not payload.get("message"):
      payload["message"] = payload["error"]
  if not payload.get("warnings"):
    payload.pop("warnings", None)
  print('RESULT:' + json.dumps(payload))
`.trim();
    try {
  const allowPythonFallback = allowPythonFallbackFromEnv();
  const payload = {
        name: params.name,
        package_path: path,
        wave_path: wavePath,
        attenuation_path: attenuationPath,
        attach_wave: !!params.wavePath,
        volume: params.settings?.volume ?? null,
        pitch: params.settings?.pitch ?? null,
        looping: params.settings?.looping ?? null
      };
      const resp = await this.bridge.executeEditorFunction('CREATE_SOUND_CUE', payload as any, { allowPythonFallback });
      return this.interpretResult(resp, {
        successMessage: 'Sound cue created',
        failureMessage: 'Failed to create SoundCue'
      });
    } catch (e) {
      return { success: false, error: `Failed to create sound cue: ${e}` };
    }
  }

  // Play sound at location
  async playSoundAtLocation(params: {
    soundPath: string;
    location: [number, number, number];
    volume?: number;
    pitch?: number;
    startTime?: number;
  }) {
    const volume = params.volume ?? 1.0;
    const pitch = params.pitch ?? 1.0;
    const startTime = params.startTime ?? 0.0;
    const soundPath = params.soundPath ?? '';

  const _py = `
import unreal
import json

result = {
  "success": False,
  "message": "",
  "error": "",
  "warnings": []
}

try:
  path = "${escapePythonString(soundPath)}"
  if not unreal.EditorAssetLibrary.does_asset_exist(path):
    result["error"] = "Sound asset not found"
    raise SystemExit(0)

  snd = unreal.EditorAssetLibrary.load_asset(path)
  if not snd:
    result["error"] = f"Failed to load sound asset: {path}"
    raise SystemExit(0)

  world = None
  try:
    world = unreal.EditorUtilityLibrary.get_editor_world()
  except Exception:
    world = None

  if not world:
      editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
      if editor_subsystem and hasattr(editor_subsystem, 'get_editor_world'):
        world = editor_subsystem.get_editor_world()

  if not world:
    try:
      world = unreal.EditorSubsystemLibrary.get_editor_world()
    except Exception:
      world = None

  if not world:
    result["error"] = "Unable to resolve editor world. Start PIE and ensure Editor Scripting Utilities is enabled."
    raise SystemExit(0)

  loc = unreal.Vector(${params.location[0]}, ${params.location[1]}, ${params.location[2]})
  rot = unreal.Rotator(0.0, 0.0, 0.0)
  unreal.GameplayStatics.spawn_sound_at_location(world, snd, loc, rot, ${volume}, ${pitch}, ${startTime})

  result["success"] = True
  result["message"] = "Sound played"

except SystemExit:
  pass
except Exception as e:
  result["error"] = str(e)
finally:
  payload = dict(result)
  if payload.get("success"):
    if not payload.get("message"):
      payload["message"] = "Sound played"
    payload.pop("error", None)
  else:
    if not payload.get("error"):
      payload["error"] = payload.get("message") or "Failed to play sound"
    if not payload.get("message"):
      payload["message"] = payload["error"]
  if not payload.get("warnings"):
    payload.pop("warnings", None)
  print('RESULT:' + json.dumps(payload))
`.trim();
    try {
  const allowPythonFallback = allowPythonFallbackFromEnv();
  const payload = {
        path: soundPath,
        x: params.location?.[0] ?? 0,
        y: params.location?.[1] ?? 0,
        z: params.location?.[2] ?? 0,
        volume,
        pitch,
        startTime
      };
      const resp = await this.bridge.executeEditorFunction('PLAY_SOUND_AT_LOCATION', payload as any, { allowPythonFallback });
      return this.interpretResult(resp, {
        successMessage: 'Sound played',
        failureMessage: 'Failed to play sound'
      });
  } catch (e) {
    return { success: false, error: `Failed to play sound: ${e}` };
  }
  }

  // Play sound 2D
  async playSound2D(params: {
    soundPath: string;
    volume?: number;
    pitch?: number;
    startTime?: number;
  }) {
    const volume = params.volume ?? 1.0;
    const pitch = params.pitch ?? 1.0;
    const startTime = params.startTime ?? 0.0;
    const soundPath = params.soundPath ?? '';

  const _py = `
import unreal
import json

result = {
  "success": False,
  "message": "",
  "error": "",
  "warnings": []
}

try:
  path = "${escapePythonString(soundPath)}"
  if not unreal.EditorAssetLibrary.does_asset_exist(path):
    result["error"] = "Sound asset not found"
    raise SystemExit(0)

  snd = unreal.EditorAssetLibrary.load_asset(path)
  if not snd:
    result["error"] = f"Failed to load sound asset: {path}"
    raise SystemExit(0)

  world = None
  try:
    world = unreal.EditorUtilityLibrary.get_editor_world()
  except Exception:
    world = None

  if not world:
    editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if editor_subsystem and hasattr(editor_subsystem, 'get_editor_world'):
      world = editor_subsystem.get_editor_world()

  if not world:
    try:
      world = unreal.EditorSubsystemLibrary.get_editor_world()
    except Exception:
      world = None

  if not world:
    result["error"] = "Unable to resolve editor world. Start PIE and ensure Editor Scripting Utilities is enabled."
    raise SystemExit(0)

  ok = False
  try:
    unreal.GameplayStatics.spawn_sound_2d(world, snd, ${volume}, ${pitch}, ${startTime})
    ok = True
  except AttributeError:
    try:
      unreal.GameplayStatics.play_sound_2d(world, snd, ${volume}, ${pitch}, ${startTime})
      ok = True
    except AttributeError:
      pass

  if not ok:
    cam_loc = unreal.Vector(0.0, 0.0, 0.0)
    try:
      editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
      if editor_subsystem and hasattr(editor_subsystem, 'get_level_viewport_camera_info'):
        info = editor_subsystem.get_level_viewport_camera_info()
        if isinstance(info, (list, tuple)) and len(info) > 0:
          cam_loc = info[0]
    except Exception:
      try:
        controller = world.get_first_player_controller()
        if controller:
          pawn = controller.get_pawn()
          if pawn:
            cam_loc = pawn.get_actor_location()
      except Exception:
        pass

    try:
      rot = unreal.Rotator(0.0, 0.0, 0.0)
      unreal.GameplayStatics.spawn_sound_at_location(world, snd, cam_loc, rot, ${volume}, ${pitch}, ${startTime})
      ok = True
      result["warnings"].append("Fell back to 3D playback at camera location")
    except Exception as location_error:
      result["warnings"].append(f"Failed fallback playback: {location_error}")

  if not ok:
    result["error"] = "Failed to play sound in 2D or fallback configuration"
    raise SystemExit(0)

  result["success"] = True
  result["message"] = "Sound2D played"

except SystemExit:
  pass
except Exception as e:
  result["error"] = str(e)
finally:
  payload = dict(result)
  if payload.get("success"):
    if not payload.get("message"):
      payload["message"] = "Sound2D played"
    payload.pop("error", None)
  else:
    if not payload.get("error"):
      payload["error"] = payload.get("message") or "Failed to play sound2D"
    if not payload.get("message"):
      payload["message"] = payload["error"]
  if not payload.get("warnings"):
    payload.pop("warnings", None)
  print('RESULT:' + json.dumps(payload))
`.trim();
    try {
  const allowPythonFallback = allowPythonFallbackFromEnv();
  const payload = {
        path: soundPath,
        volume,
        pitch,
        startTime
      };
      const resp = await this.bridge.executeEditorFunction('PLAY_SOUND_2D', payload as any, { allowPythonFallback });
      return this.interpretResult(resp, {
        successMessage: 'Sound2D played',
        failureMessage: 'Failed to play sound2D'
      });
  } catch (e) {
    return { success: false, error: `Failed to play sound2D: ${e}` };
  }
  }

  // Create audio component
  async createAudioComponent(params: {
    actorName: string;
    componentName: string;
    soundPath: string;
    autoPlay?: boolean;
    is3D?: boolean;
  }) {
    const commands = [];
    
    commands.push(`AddAudioComponent ${params.actorName} ${params.componentName} ${params.soundPath}`);
    
    if (params.autoPlay !== undefined) {
      commands.push(`SetAudioComponentAutoPlay ${params.actorName}.${params.componentName} ${params.autoPlay}`);
    }
    
    if (params.is3D !== undefined) {
      commands.push(`SetAudioComponent3D ${params.actorName}.${params.componentName} ${params.is3D}`);
    }
    
    for (const cmd of commands) {
      await this.bridge.executeConsoleCommand(cmd);
    }
    
    return { success: true, message: `Audio component ${params.componentName} added to ${params.actorName}` };
  }

  // Set sound attenuation
  async setSoundAttenuation(params: {
    name: string;
    innerRadius?: number;
    falloffDistance?: number;
    attenuationShape?: 'Sphere' | 'Capsule' | 'Box' | 'Cone';
    falloffMode?: 'Linear' | 'Logarithmic' | 'Inverse' | 'LogReverse' | 'Natural';
  }) {
    const commands = [];
    
    commands.push(`CreateAttenuationSettings ${params.name}`);
    
    if (params.innerRadius !== undefined) {
      commands.push(`SetAttenuationInnerRadius ${params.name} ${params.innerRadius}`);
    }
    
    if (params.falloffDistance !== undefined) {
      commands.push(`SetAttenuationFalloffDistance ${params.name} ${params.falloffDistance}`);
    }
    
    if (params.attenuationShape) {
      commands.push(`SetAttenuationShape ${params.name} ${params.attenuationShape}`);
    }
    
    if (params.falloffMode) {
      commands.push(`SetAttenuationFalloffMode ${params.name} ${params.falloffMode}`);
    }
    
    for (const cmd of commands) {
      await this.bridge.executeConsoleCommand(cmd);
    }
    
    return { success: true, message: `Attenuation settings ${params.name} configured` };
  }

  // Create sound class
  async createSoundClass(params: {
    name: string;
    parentClass?: string;
    properties?: {
      volume?: number;
      pitch?: number;
      lowPassFilterFrequency?: number;
      attenuationDistanceScale?: number;
    };
  }) {
    const commands = [];
    const parent = params.parentClass || 'Master';
    
    commands.push(`CreateSoundClass ${params.name} ${parent}`);
    
    if (params.properties) {
      if (params.properties.volume !== undefined) {
        commands.push(`SetSoundClassVolume ${params.name} ${params.properties.volume}`);
      }
      if (params.properties.pitch !== undefined) {
        commands.push(`SetSoundClassPitch ${params.name} ${params.properties.pitch}`);
      }
      if (params.properties.lowPassFilterFrequency !== undefined) {
        commands.push(`SetSoundClassLowPassFilter ${params.name} ${params.properties.lowPassFilterFrequency}`);
      }
      if (params.properties.attenuationDistanceScale !== undefined) {
        commands.push(`SetSoundClassAttenuationScale ${params.name} ${params.properties.attenuationDistanceScale}`);
      }
    }
    
    for (const cmd of commands) {
      await this.bridge.executeConsoleCommand(cmd);
    }
    
    return { success: true, message: `Sound class ${params.name} created` };
  }

  // Create sound mix
  async createSoundMix(params: {
    name: string;
    classAdjusters?: Array<{
      soundClass: string;
      volumeAdjuster?: number;
      pitchAdjuster?: number;
      fadeInTime?: number;
      fadeOutTime?: number;
    }>;
  }) {
    const commands = [];
    
    commands.push(`CreateSoundMix ${params.name}`);
    
    if (params.classAdjusters) {
      for (const adjuster of params.classAdjusters) {
        commands.push(`AddSoundMixClassAdjuster ${params.name} ${adjuster.soundClass}`);
        
        if (adjuster.volumeAdjuster !== undefined) {
          commands.push(`SetSoundMixVolume ${params.name} ${adjuster.soundClass} ${adjuster.volumeAdjuster}`);
        }
        if (adjuster.pitchAdjuster !== undefined) {
          commands.push(`SetSoundMixPitch ${params.name} ${adjuster.soundClass} ${adjuster.pitchAdjuster}`);
        }
        if (adjuster.fadeInTime !== undefined) {
          commands.push(`SetSoundMixFadeIn ${params.name} ${adjuster.soundClass} ${adjuster.fadeInTime}`);
        }
        if (adjuster.fadeOutTime !== undefined) {
          commands.push(`SetSoundMixFadeOut ${params.name} ${adjuster.soundClass} ${adjuster.fadeOutTime}`);
        }
      }
    }
    
    for (const cmd of commands) {
      await this.bridge.executeConsoleCommand(cmd);
    }
    
    return { success: true, message: `Sound mix ${params.name} created` };
  }

  // Push/Pop sound mix
  async pushSoundMix(params: {
    mixName: string;
  }) {
    const command = `PushSoundMix ${params.mixName}`;
    return this.bridge.executeConsoleCommand(command);
  }

  async popSoundMix(params: {
    mixName: string;
  }) {
    const command = `PopSoundMix ${params.mixName}`;
    return this.bridge.executeConsoleCommand(command);
  }

  // Set master volume
  async setMasterVolume(params: {
    volume: number; // 0.0 to 1.0
  }) {
    // Clamp volume between 0 and 1
    const vol = Math.max(0.0, Math.min(1.0, params.volume));
    
    // Use the proper Unreal Engine audio command
    // Note: au.Master.Volume is the correct console variable for master volume
    const command = `au.Master.Volume ${vol}`;
    
    try {
      await this.bridge.executeConsoleCommand(command);
      return { success: true, message: `Master volume set to ${vol}` };
    } catch (e) {
      // Fallback to Python method if console command fails
      const py = `
  import unreal
  import json
      try:
        # Try using AudioMixerBlueprintLibrary if available
        try:
          unreal.AudioMixerBlueprintLibrary.set_overall_volume_multiplier(${vol})
          print('RESULT:' + json.dumps({'success': True}))
        except AttributeError:
          # Fallback to GameplayStatics method using modern subsystems (no deprecated EditorLevelLibrary)
          try:
            world = None
            try:
              editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
              if editor_subsystem and hasattr(editor_subsystem, 'get_editor_world'):
                world = editor_subsystem.get_editor_world()
            except Exception:
              world = None

            if world is None:
              try:
                world = unreal.EditorSubsystemLibrary.get_editor_world()
              except Exception:
                world = None

            if world:
              unreal.GameplayStatics.set_global_pitch_modulation(world, 1.0, 0.0)  # Reset pitch
              unreal.GameplayStatics.set_global_time_dilation(world, 1.0)  # Reset time
              # Note: There's no direct master volume in GameplayStatics, use sound class
              print('RESULT:' + json.dumps({'success': False, 'error': 'Master volume control not available, use sound classes instead'}))
            else:
              print('RESULT:' + json.dumps({'success': False, 'error': 'Unable to resolve editor world'}))
          except Exception as e2:
            print('RESULT:' + json.dumps({'success': False, 'error': str(e2)}))
  except Exception as e:
      print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))
  `.trim();
      
      try {
  const resp = await (this.bridge as any).executeEditorPython(py, { allowPythonFallback: allowPythonFallbackFromEnv() });
        const out = typeof resp === 'string' ? resp : JSON.stringify(resp);
        const m = out.match(/RESULT:({.*})/);
        if (m) {
          try {
            const parsed = JSON.parse(m[1]);
            return parsed.success 
              ? { success: true, message: `Master volume set to ${vol}` }
              : { success: false, error: parsed.error };
          } catch {}
        }
        return { success: true, message: 'Master volume set command executed' };
      } catch {
        return { success: false, error: `Failed to set master volume: ${e}` };
      }
    }
  }

  // Create ambient sound
  async createAmbientSound(params: {
    name: string;
    location: [number, number, number];
    soundPath: string;
    volume?: number;
    radius?: number;
    autoPlay?: boolean;
  }) {
    const commands = [];
    
    commands.push(`SpawnAmbientSound ${params.name} ${params.location.join(' ')} ${params.soundPath}`);
    
    if (params.volume !== undefined) {
      commands.push(`SetAmbientVolume ${params.name} ${params.volume}`);
    }
    
    if (params.radius !== undefined) {
      commands.push(`SetAmbientRadius ${params.name} ${params.radius}`);
    }
    
    if (params.autoPlay !== undefined) {
      commands.push(`SetAmbientAutoPlay ${params.name} ${params.autoPlay}`);
    }
    
    for (const cmd of commands) {
      await this.bridge.executeConsoleCommand(cmd);
    }
    
    return { success: true, message: `Ambient sound ${params.name} created` };
  }

  // Create reverb zone
  async createReverbZone(params: {
    name: string;
    location: [number, number, number];
    size: [number, number, number];
    reverbEffect?: string;
    volume?: number;
    fadeTime?: number;
  }) {
    const commands = [];
    
    commands.push(`CreateReverbVolume ${params.name} ${params.location.join(' ')} ${params.size.join(' ')}`);
    
    if (params.reverbEffect) {
      commands.push(`SetReverbEffect ${params.name} ${params.reverbEffect}`);
    }
    
    if (params.volume !== undefined) {
      commands.push(`SetReverbVolume ${params.name} ${params.volume}`);
    }
    
    if (params.fadeTime !== undefined) {
      commands.push(`SetReverbFadeTime ${params.name} ${params.fadeTime}`);
    }
    
    for (const cmd of commands) {
      await this.bridge.executeConsoleCommand(cmd);
    }
    
    return { success: true, message: `Reverb zone ${params.name} created` };
  }

  // Audio analysis
  async enableAudioAnalysis(params: {
    enabled: boolean;
    fftSize?: number;
    outputType?: 'Magnitude' | 'Decibel' | 'Normalized';
  }) {
    const commands = [];
    
    commands.push(`EnableAudioAnalysis ${params.enabled}`);
    
    if (params.enabled && params.fftSize) {
      commands.push(`SetFFTSize ${params.fftSize}`);
    }
    
    if (params.enabled && params.outputType) {
      commands.push(`SetAudioAnalysisOutput ${params.outputType}`);
    }
    
    for (const cmd of commands) {
      await this.bridge.executeConsoleCommand(cmd);
    }
    
    return { success: true, message: `Audio analysis ${params.enabled ? 'enabled' : 'disabled'}` };
  }

  // Stop all sounds
  async stopAllSounds() {
    return this.bridge.executeConsoleCommand('StopAllSounds');
  }

  // Fade sound
  async fadeSound(params: {
    soundName: string;
    targetVolume: number;
    fadeTime: number;
    fadeType?: 'FadeIn' | 'FadeOut' | 'FadeTo';
  }) {
    const type = params.fadeType || 'FadeTo';
    const command = `${type}Sound ${params.soundName} ${params.targetVolume} ${params.fadeTime}`;
    return this.bridge.executeConsoleCommand(command);
  }

  // Set doppler effect
  async setDopplerEffect(params: {
    enabled: boolean;
    scale?: number;
  }) {
    const commands = [];
    
    commands.push(`EnableDoppler ${params.enabled}`);
    
    if (params.scale !== undefined) {
      commands.push(`SetDopplerScale ${params.scale}`);
    }
    
    for (const cmd of commands) {
      await this.bridge.executeConsoleCommand(cmd);
    }
    
    return { success: true, message: `Doppler effect ${params.enabled ? 'enabled' : 'disabled'}` };
  }

  // Audio occlusion
  async setAudioOcclusion(params: {
    enabled: boolean;
    lowPassFilterFrequency?: number;
    volumeAttenuation?: number;
  }) {
    const commands = [];
    
    commands.push(`EnableAudioOcclusion ${params.enabled}`);
    
    if (params.lowPassFilterFrequency !== undefined) {
      commands.push(`SetOcclusionLowPassFilter ${params.lowPassFilterFrequency}`);
    }
    
    if (params.volumeAttenuation !== undefined) {
      commands.push(`SetOcclusionVolumeAttenuation ${params.volumeAttenuation}`);
    }
    
    for (const cmd of commands) {
      await this.bridge.executeConsoleCommand(cmd);
    }
    
    return { success: true, message: `Audio occlusion ${params.enabled ? 'enabled' : 'disabled'}` };
  }
}
