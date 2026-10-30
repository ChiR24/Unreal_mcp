// Fold specs for manage_audio. Data only; see ../shared/fold.ts.
import type { FoldSpec } from '../shared/fold-types.js';

export const MANAGE_AUDIO_FOLDS: readonly FoldSpec[] = [
  {
    primary: 'play_sound', selector: 'playback',
    summary: 'Play a sound: 2D, at a location (played or spawned), attached to an actor, or prime it for playback. A 2D sound (looping music too) plays until stop_sound. measure plays it 2D and reports how loud it played (peak and average dB, silent). The sounds already playing are listed by console_command au.DumpActiveSounds.',
    topics: ['play sound', 'play sound 2d', 'play sound at location', 'attached sound', 'prime sound', 'preview sound', 'preload sound', 'measure sound', 'is sound audible', 'looping music', 'background music'],
    members: { '2d': 'play_sound_2d', at_location: 'play_sound_at_location', spawn_at_location: 'spawn_sound_at_location', attached: 'play_sound_attached', prime: 'prime_sound', measure: 'play_sound_measure' },
  },
  {
    primary: 'fade_sound', selector: 'fade',
    summary: 'Fade a sound an actor or audio component plays. soundName is an actor label or name, or an audio component name; componentName picks one of that actor\'s audio components (refused, listing the ones it has, when it names none), else the first. in (re)starts the sound from its beginning, rising from silence to targetVolume (default 1) over fadeInTime; out lowers it to targetVolume (default 0) over fadeOutTime and then stops it, whatever the level, and changes nothing when it was not playing; to_volume changes the volume of the sound playing to targetVolume over fadeTime (default 1 s), stopping it at 0 (the default), and is refused with SOUND_NOT_PLAYING when nothing plays. The engine ignores a fade out to a level at or above the current one. While a game runs in the editor, the name resolves in the game first. The reply names the actor and component, the fade time and volume used, and playing (whether the sound plays after the call).',
    topics: ['music volume', 'turn down the music', 'lower the volume', 'make the music quieter'],
    members: { to_volume: 'fade_sound', in: 'fade_sound_in', out: 'fade_sound_out' },
  },
  {
    primary: 'stop_sound', selector: 'soundOp',
    summary: 'Stop, pause or resume playing sounds: the 2D sounds play_sound started (only that sound with soundPath), or with all every sound the editor and a running game play, its music included. Pause holds a sound where it is and resume continues it; the reply counts the sounds changed (0 when none was playing).',
    topics: ['pause music', 'pause sound', 'resume music', 'turn off sound'],
    whenNotToUse: ['The game or the Play In Editor session must be paused (use control_editor pause).'],
    members: { stop: 'stop_sound', pause: 'pause_sound', resume: 'resume_sound' },
  },
  {
    primary: 'control_sound_mix', selector: 'control',
    summary: 'Change the mix of the sound being heard (the running game while one plays, else the editor; inGame says which), chosen by control: push and pop put a Sound Mix on the mix stack and take it off, set_base sets the base mix beneath them, and set_class_override and clear_class_override change how a mix adjusts the volume and pitch of one Sound Class, which applies while that mix is pushed or base. Mixes and classes are named by asset path or bare name (ASSET_NOT_FOUND when one does not load). Runtime state only: nothing is saved.',
    topics: ['sound mix', 'push sound mix', 'base sound mix', 'sound class override', 'duck music'],
    members: { push: 'push_sound_mix', pop: 'pop_sound_mix', set_base: 'set_base_sound_mix', set_class_override: 'set_sound_mix_class_override', clear_class_override: 'clear_sound_mix_class_override' },
  },
  {
    primary: 'create_audio_actor', selector: 'kind',
    summary: 'Place sound in the level being edited, chosen by kind. ambient_sound spawns an Ambient Sound actor playing soundPath at location (default 0,0,0), labelled name, at volume and pitch, with attenuationPath and concurrencyPath when given; it plays in the editor now and when the game starts. audio_component adds an audio component with soundPath to actorName, or to a new actor at location when actorName is left out (the reply names the actor), playing at once unless autoPlay is false. reverb_zone spawns an Audio Volume named name (DUPLICATE_NAME when taken) as a box of size (default 500 on each axis) around location, applying reverbEffect at volume (default 1) with fadeTime (default 2 s). A sound, attenuation, concurrency or reverb path that does not load is ASSET_NOT_FOUND before anything spawns; the ambient sound and the reverb zone are one undo step each.',
    topics: ['ambient sound', 'audio component', 'reverb zone', 'add background music', 'add a sound or music to the level or a room', 'add echo to a cave or a hall', 'add rain, wind or bird sounds', 'play music when the game starts'],
    members: { ambient_sound: 'create_ambient_sound', audio_component: 'create_audio_component', reverb_zone: 'create_reverb_zone' },
  },
  {
    primary: 'create_audio_asset', selector: 'kind',
    summary: 'Create an audio asset named name in path, chosen by kind: sound_cue (playing wavePath when given), sound_class, sound_mix, attenuation_settings, reverb_effect, dialogue_voice, dialogue_wave, source_effect_chain or submix_effect. An asset already at that path is refused (ASSET_ALREADY_EXISTS) rather than rebuilt over, and a reference that does not load is refused before anything is made. Saved unless save is false.',
    topics: ['sound cue', 'sound class', 'sound mix', 'attenuation settings', 'reverb effect', 'dialogue wave', 'submix effect', 'make sound'],
    members: {
      sound_cue: 'create_sound_cue', sound_class: 'create_sound_class', sound_mix: 'create_sound_mix', attenuation_settings: 'create_attenuation_settings',
      reverb_effect: 'create_reverb_effect', dialogue_voice: 'create_dialogue_voice', dialogue_wave: 'create_dialogue_wave',
      source_effect_chain: 'create_source_effect_chain', submix_effect: 'create_submix_effect',
    },
  },
  {
    primary: 'edit_sound_cue', selector: 'edit',
    summary: 'Edit a sound cue graph (assetPath): add or connect nodes, set attenuation or concurrency, add a Doppler pitch shift at the root. add_source_effect works on a Source Effect Chain at assetPath instead: it appends effectPresetPath, or a new preset of effectType saved as its own asset beside the chain (<chain>_<effectType>, numbered when taken).',
    topics: ['sound cue node', 'cue attenuation', 'cue concurrency', 'doppler effect', 'source effect', 'add wave player to cue'],
    members: { add_node: 'add_cue_node', connect_nodes: 'connect_cue_nodes', set_attenuation: 'set_cue_attenuation', set_concurrency: 'set_cue_concurrency', doppler: 'set_doppler_effect', add_source_effect: 'add_source_effect' },
  },
  {
    primary: 'configure_sound_attenuation', selector: 'setting',
    summary: 'Configure sound attenuation: distance falloff and air absorption, spatialization, occlusion, reverb send, or a named attenuation preset.',
    topics: ['attenuation', 'spatialization', 'occlusion', 'reverb send', 'falloff distance', 'make sound 3d', 'sound blocked by walls', 'air absorption', 'muffled with distance', 'make a sound quieter far away'],
    members: {
      distance: 'configure_distance_attenuation', spatialization: 'configure_spatialization', occlusion: 'configure_occlusion', reverb_send: 'configure_reverb_send',
      audio_occlusion: 'set_audio_occlusion', preset: 'set_sound_attenuation',
    },
  },
  {
    primary: 'configure_sound_class', selector: 'setting',
    summary: 'Change a Sound Class or Sound Mix, chosen by setting, each saved: parent sets or clears a class\'s parent (a loop is refused), properties sets its volume, pitch, low-pass, LFE bleed and center-channel volume, mix_modifier adds or edits the adjuster a mix applies to a class (and the mix fades), mix_eq sets the mix\'s four EQ bands. A call that sets nothing is refused.',
    topics: ['sound class properties', 'sound class parent', 'mix modifier', 'mix eq', 'mix equalizer', 'master volume', 'change the volume in the menu or the game', 'the music or the sounds louder or quieter', 'change the volume in the settings or the menu'],
    members: { parent: 'set_class_parent', properties: 'set_class_properties', mix_modifier: 'add_mix_modifier', mix_eq: 'configure_mix_eq' },
  },
  {
    primary: 'edit_metasound', selector: 'edit',
    summary: 'Create a MetaSound or edit it: add inputs, outputs and nodes, connect or disconnect nodes, remove nodes, set input and node literals, or run many edits in one batch.',
    topics: ['metasound', 'metasound node', 'metasound input', 'metasound output', 'synth sound', 'build metasound graph', 'make synth sound', 'beep sound'],
    members: { create: 'create_metasound', add_input: 'add_metasound_input', add_output: 'add_metasound_output', add_node: 'add_metasound_node', connect_nodes: 'connect_metasound_nodes', disconnect: 'disconnect_metasound_nodes', remove_node: 'remove_metasound_node', set_default: 'set_metasound_default', batch: 'build_metasound' },
  },
];
