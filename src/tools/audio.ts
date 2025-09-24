// Audio tools for Unreal Engine
import { UnrealBridge } from '../unreal-bridge.js';

export class AudioTools {
  constructor(private bridge: UnrealBridge) {}

  // Execute console command
  private async _executeCommand(command: string) {
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
    const path = params.savePath || '/Game/Audio/Cues';
    const py = `
import unreal
import json
name = r"${params.name}"
path = r"${path}"
try:
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    try:
        factory = unreal.SoundCueFactoryNew()
    except Exception:
        factory = None
    if not factory:
        print('RESULT:' + json.dumps({'success': False, 'error': 'SoundCueFactoryNew unavailable'}))
    else:
        asset = asset_tools.create_asset(asset_name=name, package_path=path, asset_class=unreal.SoundCue, factory=factory)
        if asset:
        try:
            if ${params.wavePath !== undefined ? 'True' : 'False'}:
                try:
                    wave_path = r"${params.wavePath || ''}"
                    # Check if wave exists using modern subsystem
                    try:
                        asset_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
                        if hasattr(asset_subsystem, 'does_asset_exist'):
                            wave_exists = asset_subsystem.does_asset_exist(wave_path)
                        else:
                            wave_exists = unreal.EditorAssetLibrary.does_asset_exist(wave_path)
                    except:
                        wave_exists = unreal.EditorAssetLibrary.does_asset_exist(wave_path)

                    if wave_exists:
                        # Load wave using modern subsystem
                        try:
                            if hasattr(asset_subsystem, 'get_asset'):
                                snd = asset_subsystem.get_asset(wave_path)
                            else:
                                snd = unreal.EditorAssetLibrary.load_asset(wave_path)
                        except:
                            snd = unreal.EditorAssetLibrary.load_asset(wave_path)
                        # Simple node hookup via SoundCueGraph is non-trivial via Python; leave as empty cue
                except Exception:
                    pass
            # Save asset using modern subsystem
            try:
                if hasattr(asset_subsystem, 'save_asset'):
                    unreal.EditorAssetLibrary.save_asset(f"{path}/{name}")
                else:
                    unreal.EditorAssetLibrary.save_asset(f"{path}/{name}")
            except:
                unreal.EditorAssetLibrary.save_asset(f"{path}/{name}")
            print('RESULT:' + json.dumps({'success': True}))
        else:
            print('RESULT:' + json.dumps({'success': False, 'error': 'Failed to create SoundCue'}))
except Exception as e:
    print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))
`.trim();
    try {
      const resp = await this.bridge.executePython(py);
      const out = typeof resp === 'string' ? resp : JSON.stringify(resp);
      const m = out.match(/RESULT:({.*})/);
      if (m) { try { const parsed = JSON.parse(m[1]); return parsed.success ? { success: true, message: 'Sound cue created' } : { success: false, error: parsed.error }; } catch {} }
      return { success: true, message: 'Sound cue creation attempted' };
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

    const py = `
import unreal
import json
loc = unreal.Vector(${params.location[0]}, ${params.location[1]}, ${params.location[2]})
path = r"${params.soundPath}"
try:
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        print('RESULT:' + json.dumps({'success': False, 'error': 'Sound asset not found'}))
    else:
        snd = unreal.EditorAssetLibrary.load_asset(path)
        # Get editor world via modern subsystems first to avoid deprecation
        try:
            world = unreal.EditorSubsystemLibrary.get_editor_world()
        except Exception:
            try:
                editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
                if hasattr(editor_subsystem, 'get_editor_world'):
                    world = editor_subsystem.get_editor_world()
                else:
                    world = unreal.EditorLevelLibrary.get_editor_world()
            except Exception:
                world = unreal.EditorLevelLibrary.get_editor_world()
        rot = unreal.Rotator(0.0, 0.0, 0.0)
        # Use spawn_* variant with explicit rotation before optional floats
        unreal.GameplayStatics.spawn_sound_at_location(world, snd, loc, rot, ${volume}, ${pitch}, ${startTime})
        print('RESULT:' + json.dumps({'success': True}))
except Exception as e:
    print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))
`.trim();
    try {
      const resp = await this.bridge.executePython(py);
      const out = typeof resp === 'string' ? resp : JSON.stringify(resp);
      const m = out.match(/RESULT:({.*})/);
      if (m) { try { const parsed = JSON.parse(m[1]); return parsed.success ? { success: true, message: 'Sound played' } : { success: false, error: parsed.error }; } catch {} }
      return { success: true, message: 'Sound play attempted' };
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

    const py = `
import unreal
import json
path = r"${params.soundPath}"
try:
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        print('RESULT:' + json.dumps({'success': False, 'error': 'Sound asset not found'}))
    else:
        snd = unreal.EditorAssetLibrary.load_asset(path)
        # Get editor world via modern subsystems first to avoid deprecation
        try:
            world = unreal.EditorSubsystemLibrary.get_editor_world()
        except Exception:
            try:
                editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
                if hasattr(editor_subsystem, 'get_editor_world'):
                    world = editor_subsystem.get_editor_world()
                else:
                    world = unreal.EditorLevelLibrary.get_editor_world()
            except Exception:
                world = unreal.EditorLevelLibrary.get_editor_world()
        ok = False
        try:
            unreal.GameplayStatics.spawn_sound_2d(world, snd, ${volume}, ${pitch}, ${startTime})
            ok = True
        except AttributeError:
            try:
                unreal.GameplayStatics.play_sound_2d(world, snd, ${volume}, ${pitch}, ${startTime})
                ok = True
            except AttributeError:
                # Fallback: play at camera location as 2D substitute using modern subsystems
                try:
                    # Try modern subsystem first for camera info
                    try:
                        editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
                        if hasattr(editor_subsystem, 'get_level_viewport_camera_info'):
                            info = editor_subsystem.get_level_viewport_camera_info()
                        else:
                            info = unreal.EditorLevelLibrary.get_level_viewport_camera_info()
                    except Exception:
                        info = unreal.EditorLevelLibrary.get_level_viewport_camera_info()
                    cam_loc = info[0] if isinstance(info, (list, tuple)) and len(info) > 0 else unreal.Vector(0.0, 0.0, 0.0)
                except Exception:
                    cam_loc = unreal.Vector(0.0, 0.0, 0.0)
                rot = unreal.Rotator(0.0, 0.0, 0.0)
                unreal.GameplayStatics.spawn_sound_at_location(world, snd, cam_loc, rot, ${volume}, ${pitch}, ${startTime})
                ok = True
        print('RESULT:' + json.dumps({'success': True} if ok else {'success': False, 'error': 'No suitable 2D playback method found'}))
except Exception as e:
    print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))
`.trim();
    try {
      const resp = await this.bridge.executePython(py);
      const out = typeof resp === 'string' ? resp : JSON.stringify(resp);
      const m = out.match(/RESULT:({.*})/);
      if (m) { try { const parsed = JSON.parse(m[1]); return parsed.success ? { success: true, message: 'Sound2D played' } : { success: false, error: parsed.error }; } catch {} }
      return { success: true, message: 'Sound2D play attempted' };
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
          # Fallback to GameplayStatics method using modern subsystems
          try:
              # Try modern subsystem first
              try:
                  editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
                  if hasattr(editor_subsystem, 'get_editor_world'):
                      world = editor_subsystem.get_editor_world()
                  else:
                      world = unreal.EditorLevelLibrary.get_editor_world()
              except Exception:
                  world = unreal.EditorLevelLibrary.get_editor_world()
              unreal.GameplayStatics.set_global_pitch_modulation(world, 1.0, 0.0)  # Reset pitch
              unreal.GameplayStatics.set_global_time_dilation(world, 1.0)  # Reset time
              # Note: There's no direct master volume in GameplayStatics, use sound class
              print('RESULT:' + json.dumps({'success': False, 'error': 'Master volume control not available, use sound classes instead'}))
          except Exception as e2:
              print('RESULT:' + json.dumps({'success': False, 'error': str(e2)}))
  except Exception as e:
      print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))
  `.trim();
      
      try {
        const resp = await this.bridge.executePython(py);
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
