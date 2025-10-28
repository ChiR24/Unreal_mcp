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

  // Create audio component (not available via console)
  async createAudioComponent(_params: {
    actorName: string;
    componentName: string;
    soundPath: string;
    autoPlay?: boolean;
    is3D?: boolean;
  }) {
    return { success: false, error: 'NOT_IMPLEMENTED', message: 'Creating audio components is not available via console; requires plugin/editor API' };
  }

  // Set sound attenuation
  async setSoundAttenuation(_params: {
    name: string;
    innerRadius?: number;
    falloffDistance?: number;
    attenuationShape?: 'Sphere' | 'Capsule' | 'Box' | 'Cone';
    falloffMode?: 'Linear' | 'Logarithmic' | 'Inverse' | 'LogReverse' | 'Natural';
  }) {
    return { success: false, error: 'NOT_IMPLEMENTED', message: 'Setting sound attenuation is not available via console; requires plugin/editor API' };
  }

  // Create sound class
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
    return { success: false, error: 'NOT_IMPLEMENTED', message: 'Creating sound classes is not available via console; requires plugin/editor API' };
  }
  // Create sound mix
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
    return { success: false, error: 'NOT_IMPLEMENTED', message: 'Creating sound mixes is not available via console; requires plugin/editor API' };
  }

  // Push/Pop sound mix
  async pushSoundMix(_params: {
    mixName: string;
  }) {
    return { success: false, error: 'NOT_IMPLEMENTED', message: 'Pushing sound mixes is not available via console; requires plugin/editor API' };
  }

  async popSoundMix(_params: {
    mixName: string;
  }) {
    return { success: false, error: 'NOT_IMPLEMENTED', message: 'Popping sound mixes is not available via console; requires plugin/editor API' };
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
  async createAmbientSound(_params: {
    name: string;
    location: [number, number, number];
    soundPath: string;
    volume?: number;
    radius?: number;
    autoPlay?: boolean;
  }) {
    return { success: false, error: 'NOT_IMPLEMENTED', message: 'Creating ambient sounds is not available via console; requires plugin/editor API' };
  }

  // Create reverb zone
  async createReverbZone(_params: {
    name: string;
    location: [number, number, number];
    size: [number, number, number];
    reverbEffect?: string;
    volume?: number;
    fadeTime?: number;
  }) {
    return { success: false, error: 'NOT_IMPLEMENTED', message: 'Creating reverb zones is not available via console; requires plugin/editor API' };
  }

  // Audio analysis
  async enableAudioAnalysis(_params: {
    enabled: boolean;
    fftSize?: number;
    outputType?: 'Magnitude' | 'Decibel' | 'Normalized';
  }) {
    return { success: false, error: 'NOT_IMPLEMENTED', message: 'Audio analysis controls are not available via console; requires plugin/editor API' };
  }

  // Stop all sounds
  async stopAllSounds() {
    return this.bridge.executeConsoleCommand('StopAllSounds');
  }

  // Fade sound
  async fadeSound(_params: {
    soundName: string;
    targetVolume: number;
    fadeTime: number;
    fadeType?: 'FadeIn' | 'FadeOut' | 'FadeTo';
  }) {
    return { success: false, error: 'NOT_IMPLEMENTED', message: 'Fading sound via console is not supported; use plugin/editor API' };
  }

  // Set doppler effect
  async setDopplerEffect(_params: {
    enabled: boolean;
    scale?: number;
  }) {
    return { success: false, error: 'NOT_IMPLEMENTED', message: 'Doppler effect controls are not available via console; requires plugin/editor API' };
  }

  // Audio occlusion
  async setAudioOcclusion(_params: {
    enabled: boolean;
    lowPassFilterFrequency?: number;
    volumeAttenuation?: number;
  }) {
    return { success: false, error: 'NOT_IMPLEMENTED', message: 'Audio occlusion controls are not available via console; requires plugin/editor API' };
  }
}
