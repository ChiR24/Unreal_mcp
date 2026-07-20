import type { CapabilityRecordSource } from '../../index.js';
import { utilityRecord } from '../utility/helpers.js';

const T = 'manage_audio' as const;
const RUNTIME = ['edit', 'pie', 'simulate'] as const;
const r = (action: string, summary: string, params: readonly string[] = [], required: readonly string[] = [], outputs: readonly string[] = [], outputRequired: readonly string[] = []): CapabilityRecordSource => utilityRecord({
  tool: T, action, family: 'runtime', summary, params, required, outputs, outputRequired,
  states: RUNTIME, supportsUndo: false, safeToRetry: false,
});

export const AUDIO_RUNTIME_RECORDS: readonly CapabilityRecordSource[] = [
  r('clear_sound_mix_class_override', 'Clear a Sound Mix class override.', ['mixName', 'soundClassName'], ['mixName', 'soundClassName']),
  r('create_ambient_sound', 'Create an ambient sound actor.', ['soundPath', 'location', 'volume', 'pitch', 'attenuationPath'], ['soundPath'], ['actorName'], ['actorName']),
  r('create_audio_component', 'Create an audio component on an actor.', ['actorName', 'componentName', 'soundPath', 'autoPlay'], ['soundPath'], ['componentName'], ['componentName']),
  r('create_reverb_zone', 'Create a runtime reverb zone actor.', ['name', 'location', 'size', 'reverbEffect', 'volume', 'fadeTime'], ['name'], ['actorName'], ['actorName']),
  r('enable_audio_analysis', 'Enable or disable runtime audio analysis.', ['enable', 'enabled', 'analysisType', 'windowSize']),
  r('fade_sound', 'Fade a named sound instance to a target volume.', ['soundName', 'targetVolume', 'fadeTime', 'fadeType'], ['soundName']),
  r('fade_sound_in', 'Fade a sound instance in.', ['soundName', 'fadeInTime', 'targetVolume'], ['soundName']),
  r('fade_sound_out', 'Fade a sound instance out.', ['soundName', 'fadeOutTime', 'targetVolume'], ['soundName']),
  r('play_sound_2d', 'Play a non-spatial sound.', ['soundPath', 'volume', 'pitch', 'startTime'], ['soundPath']),
  r('play_sound_at_location', 'Play a sound at a world location.', ['soundPath', 'location', 'rotation', 'volume', 'pitch', 'startTime', 'attenuationPath', 'concurrencyPath'], ['soundPath']),
  r('play_sound_attached', 'Play a sound attached to an actor component.', ['soundPath', 'actorName', 'componentName', 'attachPointName', 'volume', 'pitch'], ['soundPath', 'actorName']),
  r('pop_sound_mix', 'Pop a Sound Mix from the runtime mix stack.', ['mixName'], ['mixName']),
  r('prime_sound', 'Prime a sound asset for playback.', ['soundPath'], ['soundPath']),
  r('push_sound_mix', 'Push a Sound Mix onto the runtime mix stack.', ['mixName'], ['mixName']),
  r('set_base_sound_mix', 'Set the runtime base Sound Mix.', ['mixName'], ['mixName']),
  r('set_sound_mix_class_override', 'Set a Sound Mix class override.', ['mixName', 'soundClassName', 'volume', 'pitch', 'fadeTime'], ['mixName', 'soundClassName']),
  r('spawn_sound_at_location', 'Spawn a transient sound at a world location.', ['soundPath', 'location', 'rotation', 'volume', 'pitch'], ['soundPath'], ['componentName']),
];
