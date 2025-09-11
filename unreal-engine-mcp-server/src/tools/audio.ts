// Audio tools for Unreal Engine
import { UnrealBridge } from '../unreal-bridge.js';

export class AudioTools {
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
    const commands = [];
    const path = params.savePath || '/Game/Audio/Cues';
    
    commands.push(`CreateSoundCue ${params.name} ${path}`);
    
    if (params.wavePath) {
      commands.push(`AddSoundWave ${params.name} ${params.wavePath}`);
    }
    
    if (params.settings) {
      if (params.settings.volume !== undefined) {
        commands.push(`SetSoundCueVolume ${params.name} ${params.settings.volume}`);
      }
      if (params.settings.pitch !== undefined) {
        commands.push(`SetSoundCuePitch ${params.name} ${params.settings.pitch}`);
      }
      if (params.settings.looping !== undefined) {
        commands.push(`SetSoundCueLooping ${params.name} ${params.settings.looping}`);
      }
      if (params.settings.attenuationSettings) {
        commands.push(`SetSoundCueAttenuation ${params.name} ${params.settings.attenuationSettings}`);
      }
    }
    
    for (const cmd of commands) {
      await this.bridge.executeConsoleCommand(cmd);
    }
    
    return { success: true, message: `Sound cue ${params.name} created` };
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
    
    const command = `PlaySoundAtLocation ${params.soundPath} ${params.location.join(' ')} ${volume} ${pitch} ${startTime}`;
    return this.bridge.executeConsoleCommand(command);
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
    
    const command = `PlaySound2D ${params.soundPath} ${volume} ${pitch} ${startTime}`;
    return this.bridge.executeConsoleCommand(command);
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
    const command = `SetMasterVolume ${params.volume}`;
    return this.bridge.executeConsoleCommand(command);
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
