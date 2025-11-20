// Audio tools for Unreal Engine
import { UnrealBridge } from '../unreal-bridge.js';
import { AutomationBridge } from '../automation-bridge.js';

export class AudioTools {
  constructor(private bridge: UnrealBridge, private automationBridge?: AutomationBridge) {}

  setAutomationBridge(automationBridge?: AutomationBridge) { this.automationBridge = automationBridge; }

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
    if (!this.automationBridge) {
      throw new Error('Automation Bridge not available. Audio operations require plugin support.');
    }

    const path = params.savePath || '/Game/Audio/Cues';

    try {
      const response = await this.automationBridge.sendAutomationRequest('create_sound_cue', {
        name: params.name,
        packagePath: path,
        wavePath: params.wavePath,
        attenuationPath: params.settings?.attenuationSettings,
        volume: params.settings?.volume,
        pitch: params.settings?.pitch,
        looping: params.settings?.looping
      }, {
        timeoutMs: 60000
      });

      if (response.success === false) {
        return { success: false, error: response.error || response.message || 'Failed to create SoundCue' };
      }

      return { success: true, message: response.message || 'Sound cue created' };
    } catch (error) {
      return { success: false, error: `Failed to create sound cue: ${error instanceof Error ? error.message : String(error)}` };
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
    if (!this.automationBridge) {
      throw new Error('Automation Bridge not available. Audio operations require plugin support.');
    }

    try {
      const response = await this.automationBridge.sendAutomationRequest('play_sound_at_location', {
        soundPath: params.soundPath,
        location: params.location,
        volume: params.volume ?? 1.0,
        pitch: params.pitch ?? 1.0,
        startTime: params.startTime ?? 0.0
      }, {
        timeoutMs: 30000
      });

      if (response.success === false) {
        return { success: false, error: response.error || response.message || 'Failed to play sound' };
      }

      return { success: true, message: response.message || 'Sound played' };
    } catch (error) {
      return { success: false, error: `Failed to play sound: ${error instanceof Error ? error.message : String(error)}` };
    }
  }

  // Play sound 2D
  async playSound2D(params: {
    soundPath: string;
    volume?: number;
    pitch?: number;
    startTime?: number;
  }) {
    if (!this.automationBridge) {
      throw new Error('Automation Bridge not available. Audio operations require plugin support.');
    }

    try {
      const response = await this.automationBridge.sendAutomationRequest('play_sound_2d', {
        soundPath: params.soundPath,
        volume: params.volume ?? 1.0,
        pitch: params.pitch ?? 1.0,
        startTime: params.startTime ?? 0.0
      }, {
        timeoutMs: 30000
      });

      if (response.success === false) {
        return { success: false, error: response.error || response.message || 'Failed to play 2D sound' };
      }

      return { success: true, message: response.message || '2D sound played' };
    } catch (error) {
      return { success: false, error: `Failed to play 2D sound: ${error instanceof Error ? error.message : String(error)}` };
    }
  }

  // Create audio component (requires C++ plugin)
  async createAudioComponent(_params: {
    actorName: string;
    componentName: string;
    soundPath: string;
    autoPlay?: boolean;
    is3D?: boolean;
  }) {
    if (!this.automationBridge) {
      return { success: false, error: 'NOT_IMPLEMENTED', message: 'Creating audio components requires C++ plugin support' };
    }

    try {
      const response = await this.automationBridge.sendAutomationRequest('create_audio_component', {
        actorName: _params.actorName,
        componentName: _params.componentName,
        soundPath: _params.soundPath,
        autoPlay: _params.autoPlay ?? false,
        is3D: _params.is3D ?? true
      });

      return response.success
        ? { success: true, message: response.message || 'Audio component created' }
        : { success: false, error: response.error || response.message || 'Failed to create audio component' };
    } catch (error) {
      return { success: false, error: `Failed to create audio component: ${error instanceof Error ? error.message : String(error)}` };
    }
  }

  // Set sound attenuation (requires C++ plugin)
  async setSoundAttenuation(_params: {
    name: string;
    innerRadius?: number;
    falloffDistance?: number;
    attenuationShape?: 'Sphere' | 'Capsule' | 'Box' | 'Cone';
    falloffMode?: 'Linear' | 'Logarithmic' | 'Inverse' | 'LogReverse' | 'Natural';
  }) {
    if (!this.automationBridge) {
      return { success: false, error: 'NOT_IMPLEMENTED', message: 'Setting sound attenuation requires C++ plugin support' };
    }

    try {
      const response = await this.automationBridge.sendAutomationRequest('set_sound_attenuation', {
        name: _params.name,
        innerRadius: _params.innerRadius,
        falloffDistance: _params.falloffDistance,
        attenuationShape: _params.attenuationShape,
        falloffMode: _params.falloffMode
      });

      return response.success
        ? { success: true, message: response.message || 'Sound attenuation set' }
        : { success: false, error: response.error || response.message || 'Failed to set sound attenuation' };
    } catch (error) {
      return { success: false, error: `Failed to set sound attenuation: ${error instanceof Error ? error.message : String(error)}` };
    }
  }

  // Create sound class (requires C++ plugin)
  async createSoundClass(_params: {
    name: string;
    parentClass?: string;
    properties?: {
      volume?: number;
      pitch?: number;
      lowPassFilterFrequency?: number;
      attenuationDistanceScale?: number;
    };
  }) {
    if (!this.automationBridge) {
      return { success: false, error: 'NOT_IMPLEMENTED', message: 'Creating sound classes requires C++ plugin support' };
    }

    try {
      const response = await this.automationBridge.sendAutomationRequest('create_sound_class', {
        name: _params.name,
        parentClass: _params.parentClass,
        properties: _params.properties
      });

      return response.success
        ? { success: true, message: response.message || 'Sound class created' }
        : { success: false, error: response.error || response.message || 'Failed to create sound class' };
    } catch (error) {
      return { success: false, error: `Failed to create sound class: ${error instanceof Error ? error.message : String(error)}` };
    }
  }

  // Create sound mix (requires C++ plugin)
  async createSoundMix(_params: {
    name: string;
    classAdjusters?: Array<{
      soundClass: string;
      volumeAdjuster?: number;
      pitchAdjuster?: number;
      fadeInTime?: number;
      fadeOutTime?: number;
    }>;
  }) {
    if (!this.automationBridge) {
      return { success: false, error: 'NOT_IMPLEMENTED', message: 'Creating sound mixes requires C++ plugin support' };
    }

    try {
      const response = await this.automationBridge.sendAutomationRequest('create_sound_mix', {
        name: _params.name,
        classAdjusters: _params.classAdjusters
      });

      return response.success
        ? { success: true, message: response.message || 'Sound mix created' }
        : { success: false, error: response.error || response.message || 'Failed to create sound mix' };
    } catch (error) {
      return { success: false, error: `Failed to create sound mix: ${error instanceof Error ? error.message : String(error)}` };
    }
  }

  // Push/Pop sound mix (requires C++ plugin)
  async pushSoundMix(_params: {
    mixName: string;
  }) {
    if (!this.automationBridge) {
      return { success: false, error: 'NOT_IMPLEMENTED', message: 'Pushing sound mixes requires C++ plugin support' };
    }

    try {
      const response = await this.automationBridge.sendAutomationRequest('push_sound_mix', {
        mixName: _params.mixName
      });

      return response.success
        ? { success: true, message: response.message || 'Sound mix pushed' }
        : { success: false, error: response.error || response.message || 'Failed to push sound mix' };
    } catch (error) {
      return { success: false, error: `Failed to push sound mix: ${error instanceof Error ? error.message : String(error)}` };
    }
  }

  async popSoundMix(_params: {
    mixName: string;
  }) {
    if (!this.automationBridge) {
      return { success: false, error: 'NOT_IMPLEMENTED', message: 'Popping sound mixes requires C++ plugin support' };
    }

    try {
      const response = await this.automationBridge.sendAutomationRequest('pop_sound_mix', {
        mixName: _params.mixName
      });

      return response.success
        ? { success: true, message: response.message || 'Sound mix popped' }
        : { success: false, error: response.error || response.message || 'Failed to pop sound mix' };
    } catch (error) {
      return { success: false, error: `Failed to pop sound mix: ${error instanceof Error ? error.message : String(error)}` };
    }
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
      return { success: false, error: `Failed to set master volume: ${e instanceof Error ? e.message : String(e)}` };
    }
  }

  // Create ambient sound (requires C++ plugin)
  async createAmbientSound(_params: {
    name: string;
    location: [number, number, number];
    soundPath: string;
    volume?: number;
    radius?: number;
    autoPlay?: boolean;
  }) {
    if (!this.automationBridge) {
      return { success: false, error: 'NOT_IMPLEMENTED', message: 'Creating ambient sounds requires C++ plugin support' };
    }

    try {
      const response = await this.automationBridge.sendAutomationRequest('create_ambient_sound', {
        name: _params.name,
        location: _params.location,
        soundPath: _params.soundPath,
        volume: _params.volume,
        radius: _params.radius,
        autoPlay: _params.autoPlay ?? true
      });

      return response.success
        ? { success: true, message: response.message || 'Ambient sound created' }
        : { success: false, error: response.error || response.message || 'Failed to create ambient sound' };
    } catch (error) {
      return { success: false, error: `Failed to create ambient sound: ${error instanceof Error ? error.message : String(error)}` };
    }
  }

  // Create reverb zone (requires C++ plugin)
  async createReverbZone(_params: {
    name: string;
    location: [number, number, number];
    size: [number, number, number];
    reverbEffect?: string;
    volume?: number;
    fadeTime?: number;
  }) {
    if (!this.automationBridge) {
      return { success: false, error: 'NOT_IMPLEMENTED', message: 'Creating reverb zones requires C++ plugin support' };
    }

    try {
      const response = await this.automationBridge.sendAutomationRequest('create_reverb_zone', {
        name: _params.name,
        location: _params.location,
        size: _params.size,
        reverbEffect: _params.reverbEffect,
        volume: _params.volume,
        fadeTime: _params.fadeTime
      });

      return response.success
        ? { success: true, message: response.message || 'Reverb zone created' }
        : { success: false, error: response.error || response.message || 'Failed to create reverb zone' };
    } catch (error) {
      return { success: false, error: `Failed to create reverb zone: ${error instanceof Error ? error.message : String(error)}` };
    }
  }

  // Audio analysis (requires C++ plugin)
  async enableAudioAnalysis(_params: {
    enabled: boolean;
    fftSize?: number;
    outputType?: 'Magnitude' | 'Decibel' | 'Normalized';
  }) {
    if (!this.automationBridge) {
      return { success: false, error: 'NOT_IMPLEMENTED', message: 'Audio analysis controls require C++ plugin support' };
    }

    try {
      const response = await this.automationBridge.sendAutomationRequest('enable_audio_analysis', {
        enabled: _params.enabled,
        fftSize: _params.fftSize,
        outputType: _params.outputType
      });

      return response.success
        ? { success: true, message: response.message || `Audio analysis ${_params.enabled ? 'enabled' : 'disabled'}` }
        : { success: false, error: response.error || response.message || 'Failed to enable audio analysis' };
    } catch (error) {
      return { success: false, error: `Failed to enable audio analysis: ${error instanceof Error ? error.message : String(error)}` };
    }
  }

  // Stop all sounds
  async stopAllSounds() {
    return this.bridge.executeConsoleCommand('StopAllSounds');
  }

  // Fade sound (requires C++ plugin)
  async fadeSound(_params: {
    soundName: string;
    targetVolume: number;
    fadeTime: number;
    fadeType?: 'FadeIn' | 'FadeOut' | 'FadeTo';
  }) {
    if (!this.automationBridge) {
      return { success: false, error: 'NOT_IMPLEMENTED', message: 'Fading sound requires C++ plugin support' };
    }

    try {
      const response = await this.automationBridge.sendAutomationRequest('fade_sound', {
        soundName: _params.soundName,
        targetVolume: _params.targetVolume,
        fadeTime: _params.fadeTime,
        fadeType: _params.fadeType || 'FadeTo'
      });

      return response.success
        ? { success: true, message: response.message || 'Sound faded' }
        : { success: false, error: response.error || response.message || 'Failed to fade sound' };
    } catch (error) {
      return { success: false, error: `Failed to fade sound: ${error instanceof Error ? error.message : String(error)}` };
    }
  }

  // Set doppler effect (requires C++ plugin)
  async setDopplerEffect(_params: {
    enabled: boolean;
    scale?: number;
  }) {
    if (!this.automationBridge) {
      return { success: false, error: 'NOT_IMPLEMENTED', message: 'Doppler effect controls require C++ plugin support' };
    }

    try {
      const response = await this.automationBridge.sendAutomationRequest('set_doppler_effect', {
        enabled: _params.enabled,
        scale: _params.scale ?? 1.0
      });

      return response.success
        ? { success: true, message: response.message || `Doppler effect ${_params.enabled ? 'enabled' : 'disabled'}` }
        : { success: false, error: response.error || response.message || 'Failed to set doppler effect' };
    } catch (error) {
      return { success: false, error: `Failed to set doppler effect: ${error instanceof Error ? error.message : String(error)}` };
    }
  }

  // Audio occlusion (requires C++ plugin)
  async setAudioOcclusion(_params: {
    enabled: boolean;
    lowPassFilterFrequency?: number;
    volumeAttenuation?: number;
  }) {
    if (!this.automationBridge) {
      return { success: false, error: 'NOT_IMPLEMENTED', message: 'Audio occlusion controls require C++ plugin support' };
    }

    try {
      const response = await this.automationBridge.sendAutomationRequest('set_audio_occlusion', {
        enabled: _params.enabled,
        lowPassFilterFrequency: _params.lowPassFilterFrequency,
        volumeAttenuation: _params.volumeAttenuation
      });

      return response.success
        ? { success: true, message: response.message || `Audio occlusion ${_params.enabled ? 'enabled' : 'disabled'}` }
        : { success: false, error: response.error || response.message || 'Failed to set audio occlusion' };
    } catch (error) {
      return { success: false, error: `Failed to set audio occlusion: ${error instanceof Error ? error.message : String(error)}` };
    }
  }
}
