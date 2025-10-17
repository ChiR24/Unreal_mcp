// Audio tools for Unreal Engine
import JSON5 from 'json5';
import { UnrealBridge } from '../unreal-bridge.js';
import { AutomationBridge } from '../automation-bridge.js';

export class AudioTools {
  constructor(private bridge: UnrealBridge, private automationBridge?: AutomationBridge) {}

  setAutomationBridge(automationBridge?: AutomationBridge) { this.automationBridge = automationBridge; }

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
      return { success: false, error: `Failed to set master volume: ${e instanceof Error ? e.message : String(e)}` };
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
