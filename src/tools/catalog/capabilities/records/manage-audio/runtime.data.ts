import type { CapabilityRecordSource } from '../../model.js';
import { utilityRecord, withTopics } from '../utility/utility-record-builders.js';

const T = 'manage_audio' as const;
const RUNTIME = ['edit', 'pie', 'simulate'] as const;
const r = (action: string, summary: string, params: readonly string[] = [], required: readonly string[] = [], outputs: readonly string[] = [], outputRequired: readonly string[] = []): CapabilityRecordSource => utilityRecord({
  tool: T, action, family: 'runtime', summary, params, required, outputs, outputRequired,
  states: RUNTIME, safeToRetry: false,
});

export const AUDIO_RUNTIME_RECORDS: readonly CapabilityRecordSource[] = [
  r('clear_sound_mix_class_override', 'Clear a Sound Mix class override.', ['mixName', 'soundClassName', 'fadeOutTime'], ['mixName', 'soundClassName']),
  r('create_ambient_sound', 'Create an ambient sound actor.', ['soundPath', 'location', 'name', 'volume', 'pitch', 'attenuationPath', 'concurrencyPath'], ['soundPath'], ['actorName'], ['actorName']),
  r('create_audio_component', 'Create an audio component on an actor, or on a new actor at location when actorName is omitted.', ['actorName', 'componentName', 'soundPath', 'location', 'rotation', 'volume', 'pitch', 'autoPlay'], ['soundPath'], ['componentName'], ['componentName']),
  r('create_reverb_zone', 'Create a runtime reverb zone actor.', ['name', 'location', 'size', 'reverbEffect', 'volume', 'fadeTime'], ['name'], ['actorName'], ['actorName']),
  r('fade_sound', 'Fade a named sound instance to a target volume.', ['soundName', 'componentName', 'targetVolume', 'fadeTime', 'fadeType'], ['soundName']),
  r('fade_sound_in', 'Fade a sound instance in.', ['soundName', 'componentName', 'fadeInTime', 'targetVolume'], ['soundName']),
  r('fade_sound_out', 'Fade a sound instance out.', ['soundName', 'componentName', 'fadeOutTime', 'targetVolume'], ['soundName']),
  withTopics(r('play_sound_2d', 'Play a non-spatial sound.', ['soundPath', 'volume', 'pitch', 'startTime'], ['soundPath']), ['play sound', 'play audio', 'play sfx', 'ui sound', 'play music']),
  withTopics(r('play_sound_at_location', 'Play a sound at a world location.', ['soundPath', 'location', 'rotation', 'volume', 'pitch', 'startTime', 'attenuationPath', 'concurrencyPath'], ['soundPath']), ['play sound at location', '3d sound', 'spatial sound', 'positional audio']),
  r('play_sound_attached', 'Play a sound attached to an actor component.', ['soundPath', 'actorName', 'componentName', 'attachPointName', 'volume', 'pitch'], ['soundPath', 'actorName']),
  withTopics(r('play_sound_measure', 'Play a sound 2D and measure how loud it played, from the mixer\'s own meter: peak and average level in dB and a timeline, with silent: true when it played but produced no audio (an empty Sound Cue, a MetaSound whose audio output is unconnected). Answers when the sound ends or after maxSeconds.', ['soundPath', 'volume', 'maxSeconds'], ['soundPath'], ['peakDb', 'averageDb', 'silent', 'envelopeUpdates', 'endedBecause', 'timeline', 'timelineStepSeconds'], ['peakDb', 'silent']), ['measure sound', 'sound level', 'is sound audible', 'loudness', 'silent sound', 'audio meter', 'verify sound plays']),
  r('pop_sound_mix', 'Pop a Sound Mix from the runtime mix stack.', ['mixName'], ['mixName']),
  r('prime_sound', 'Prime a sound asset for playback.', ['soundPath'], ['soundPath']),
  r('push_sound_mix', 'Push a Sound Mix onto the runtime mix stack.', ['mixName'], ['mixName']),
  r('set_base_sound_mix', 'Set the runtime base Sound Mix.', ['mixName'], ['mixName']),
  r('set_sound_mix_class_override', 'Set a Sound Mix class override.', ['mixName', 'soundClassName', 'volume', 'pitch', 'fadeTime'], ['mixName', 'soundClassName']),
  withTopics(r('stop_sound', 'Stop sounds: the 2D sounds play_sound started (only that sound with soundPath), or with all every sound the editor and a running game play, its music included.', ['soundPath', 'all'], [], ['stopped', 'allStopped']), ['stop sound', 'stop music', 'stop audio', 'silence', 'mute audio']),
  r('pause_sound', 'Pause the sounds stop_sound would stop, each held where it is. Sounds already paused are not counted.', ['soundPath', 'all'], [], ['paused']),
  r('resume_sound', 'Resume the paused sounds stop_sound would stop, each from where it was paused.', ['soundPath', 'all'], [], ['resumed']),
  r('spawn_sound_at_location', 'Spawn a transient sound at a world location.', ['soundPath', 'location', 'rotation', 'volume', 'pitch', 'name'], ['soundPath'], ['componentName']),
];
