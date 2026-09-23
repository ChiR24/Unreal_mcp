// Fold specs for manage_audio. Data only; see ../shared/fold.ts.
import type { FoldSpec } from '../shared/fold-types.js';

export const MANAGE_AUDIO_FOLDS: readonly FoldSpec[] = [
  {
    primary: 'play_sound', selector: 'playback',
    summary: 'Play a sound: 2D, at a location (played or spawned), attached to an actor, or prime it for playback.',
    topics: ['play sound', 'play sound 2d', 'play sound at location', 'attached sound', 'prime sound'],
    members: { '2d': 'play_sound_2d', at_location: 'play_sound_at_location', spawn_at_location: 'spawn_sound_at_location', attached: 'play_sound_attached', prime: 'prime_sound' },
  },
  {
    primary: 'fade_sound', selector: 'fade',
    summary: 'Fade a playing sound: to a target volume, in, or out.',
    members: { to_volume: 'fade_sound', in: 'fade_sound_in', out: 'fade_sound_out' },
  },
  {
    primary: 'control_sound_mix', selector: 'control',
    summary: 'Control sound mixes at runtime: push or pop a mix, set the base mix, set or clear a class override.',
    topics: ['sound mix', 'push sound mix', 'base sound mix', 'sound class override'],
    members: { push: 'push_sound_mix', pop: 'pop_sound_mix', set_base: 'set_base_sound_mix', set_class_override: 'set_sound_mix_class_override', clear_class_override: 'clear_sound_mix_class_override' },
  },
  {
    primary: 'create_audio_actor', selector: 'kind',
    summary: 'Create an ambient sound actor, an audio component on an actor, or a reverb zone.',
    topics: ['ambient sound', 'audio component', 'reverb zone'],
    members: { ambient_sound: 'create_ambient_sound', audio_component: 'create_audio_component', reverb_zone: 'create_reverb_zone' },
  },
  {
    primary: 'create_audio_asset', selector: 'kind',
    summary: 'Create an audio asset: sound cue, sound class, sound mix, attenuation settings, reverb effect, dialogue voice or wave, source effect chain, submix effect.',
    topics: ['sound cue', 'sound class', 'sound mix', 'attenuation settings', 'reverb effect', 'dialogue wave', 'submix effect'],
    members: {
      sound_cue: 'create_sound_cue', sound_class: 'create_sound_class', sound_mix: 'create_sound_mix', attenuation_settings: 'create_attenuation_settings',
      reverb_effect: 'create_reverb_effect', dialogue_voice: 'create_dialogue_voice', dialogue_wave: 'create_dialogue_wave',
      source_effect_chain: 'create_source_effect_chain', submix_effect: 'create_submix_effect',
    },
  },
  {
    primary: 'edit_sound_cue', selector: 'edit',
    summary: 'Edit a sound cue graph: add or connect nodes, set attenuation or concurrency, add a source effect.',
    topics: ['sound cue node', 'cue attenuation', 'cue concurrency', 'source effect'],
    members: { add_node: 'add_cue_node', connect_nodes: 'connect_cue_nodes', set_attenuation: 'set_cue_attenuation', set_concurrency: 'set_cue_concurrency', add_source_effect: 'add_source_effect' },
  },
  {
    primary: 'configure_sound_attenuation', selector: 'setting',
    summary: 'Configure sound attenuation: distance falloff, spatialization, occlusion, reverb send, Doppler, or a named attenuation preset.',
    topics: ['attenuation', 'spatialization', 'occlusion', 'reverb send', 'doppler', 'falloff distance'],
    members: {
      distance: 'configure_distance_attenuation', spatialization: 'configure_spatialization', occlusion: 'configure_occlusion', reverb_send: 'configure_reverb_send',
      audio_occlusion: 'set_audio_occlusion', doppler: 'set_doppler_effect', preset: 'set_sound_attenuation',
    },
  },
  {
    primary: 'configure_sound_class', selector: 'setting',
    summary: 'Configure a sound class or mix: parent class, class properties, mix modifier, mix EQ.',
    topics: ['sound class properties', 'sound class parent', 'mix modifier', 'mix eq'],
    members: { parent: 'set_class_parent', properties: 'set_class_properties', mix_modifier: 'add_mix_modifier', mix_eq: 'configure_mix_eq' },
  },
  {
    primary: 'edit_metasound', selector: 'edit',
    summary: 'Create a MetaSound or edit it: add inputs, outputs and nodes, connect nodes, set input and node literals, or run many edits in one batch.',
    topics: ['metasound', 'metasound node', 'metasound input', 'metasound output', 'synth sound', 'build metasound graph'],
    members: { create: 'create_metasound', add_input: 'add_metasound_input', add_output: 'add_metasound_output', add_node: 'add_metasound_node', connect_nodes: 'connect_metasound_nodes', set_default: 'set_metasound_default', batch: 'build_metasound' },
  },
];
