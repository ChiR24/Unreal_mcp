import type { ITools } from '../../../../types/tools/tool-interfaces.js';
import type { AudioArgs, HandlerArgs } from '../../../../types/handlers/handler-types.js';
import { TOOL_ACTIONS } from '../../../../utils/commands/action-constants.js';
import { createUnknownActionResponse } from '../../foundation/dispatch/handler-error-context.js';
import { cleanObject } from '../../../../utils/serialization/safe-json.js';
import { executeAutomationRequest } from '../../foundation/dispatch/common-handlers.js';
import {
  createReverbZone,
  createSoundClass,
  createSoundCue,
  createSoundMix,
  setSoundAttenuation
} from './audio-asset-actions.js';
import {
  addSourceEffect,
  enableAudioAnalysis,
  setAudioOcclusion,
  setDopplerEffect
} from './audio-effects-actions.js';
import {
  clearSoundMixClassOverride,
  popSoundMix,
  pushSoundMix,
  setBaseSoundMix,
  setSoundMixClassOverride
} from './audio-mix-actions.js';
import {
  createAmbientSound,
  createAudioComponent,
  fadeSound,
  playSound2D,
  playSoundAtLocation
} from './audio-playback-actions.js';

export async function handleAudioTools(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<Record<string, unknown>> {
  const audioArgs: AudioArgs = args;

  switch (action) {
    case 'create_sound_cue':
      return cleanObject(await createSoundCue(tools, audioArgs)) as Record<string, unknown>;
    case 'play_sound_at_location':
      return cleanObject(await playSoundAtLocation(tools, audioArgs)) as Record<string, unknown>;
    case 'play_sound_2d':
      return cleanObject(await playSound2D(tools, audioArgs)) as Record<string, unknown>;
    case 'create_audio_component':
      return cleanObject(await createAudioComponent(tools, audioArgs)) as Record<string, unknown>;
    case 'set_sound_attenuation':
      return cleanObject(await setSoundAttenuation(tools, audioArgs)) as Record<string, unknown>;
    case 'create_sound_class':
      return cleanObject(await createSoundClass(tools, audioArgs)) as Record<string, unknown>;
    case 'create_sound_mix':
      return cleanObject(await createSoundMix(tools, audioArgs)) as Record<string, unknown>;
    case 'push_sound_mix':
      return cleanObject(await pushSoundMix(tools, audioArgs)) as Record<string, unknown>;
    case 'pop_sound_mix':
      return cleanObject(await popSoundMix(tools, audioArgs)) as Record<string, unknown>;
    case 'create_ambient_sound':
      return cleanObject(await createAmbientSound(tools, audioArgs)) as Record<string, unknown>;
    case 'create_reverb_zone':
      return cleanObject(await createReverbZone(tools, audioArgs)) as Record<string, unknown>;
    case 'enable_audio_analysis':
      return cleanObject(await enableAudioAnalysis(tools, audioArgs)) as Record<string, unknown>;
    case 'fade_sound':
      return cleanObject(await fadeSound(tools, audioArgs)) as Record<string, unknown>;
    case 'set_doppler_effect':
      return cleanObject(await setDopplerEffect(tools, audioArgs)) as Record<string, unknown>;
    case 'set_audio_occlusion':
      return cleanObject(await setAudioOcclusion(tools, audioArgs)) as Record<string, unknown>;
    case 'spawn_sound_at_location':
      return cleanObject(await executeAutomationRequest(tools, TOOL_ACTIONS.SPAWN_SOUND_AT_LOCATION, args)) as Record<string, unknown>;
    case 'play_sound_attached':
      return cleanObject(await executeAutomationRequest(tools, TOOL_ACTIONS.PLAY_SOUND_ATTACHED, args)) as Record<string, unknown>;
    case 'set_sound_mix_class_override':
      return cleanObject(await setSoundMixClassOverride(tools, audioArgs)) as Record<string, unknown>;
    case 'clear_sound_mix_class_override':
      return cleanObject(await clearSoundMixClassOverride(tools, audioArgs)) as Record<string, unknown>;
    case 'set_base_sound_mix':
      return cleanObject(await setBaseSoundMix(tools, audioArgs)) as Record<string, unknown>;
    case 'prime_sound':
      return cleanObject(await executeAutomationRequest(tools, TOOL_ACTIONS.PRIME_SOUND, args)) as Record<string, unknown>;
    case 'fade_sound_in':
    case 'fade_sound_out':
      return cleanObject(await executeAutomationRequest(tools, action, args)) as Record<string, unknown>;
    case 'add_source_effect':
      return cleanObject(await addSourceEffect(tools, args)) as Record<string, unknown>;
    default:
      return createUnknownActionResponse(`Unknown audio action: ${action}`, { isError: true });
  }
}
