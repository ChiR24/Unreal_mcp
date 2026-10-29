/**
 * Loudness records: Sound Waves measured as broadcast measures them (EBU R128) and set to play at one loudness.
 * Native Domains/AudioAuthoring (McpLoudness, McpAudioLoudness).
 */
import type { CapabilityRecordSource } from '../../model.js';
import { buildCoreRecord } from '../core/builder.js';

const SOUND_PATHS = {
  type: 'array', minItems: 1, maxItems: 64, items: { type: 'string' },
  description: 'The Sound Waves, by asset path (1-64). A Sound Cue or MetaSound is refused with ASSET_NOT_FOUND naming it: measure the waves it plays.',
};
const SOUNDS = {
  type: 'array', description: 'Each Sound Wave, in the order given.',
  items: { type: 'object', additionalProperties: false, properties: {
    soundPath: { type: 'string' },
    lufs: { type: 'number', description: 'Its integrated loudness in LUFS (EBU R128: K-weighted, gated at -70 LUFS and 10 LU under the mean), from its own samples.' },
    playsAtLufs: { type: 'number', description: 'The loudness it plays at through its Volume (lufs plus 20 log10 of the Volume).' },
    volume: { type: 'number', description: 'Its Volume (the wave\'s own gain, 1 as recorded).' },
    peakDb: { type: 'number', description: 'Its sample peak in dBFS.' },
    peakAtVolumeDb: { type: 'number', description: 'The sample peak through its Volume; above 0 it is louder than full scale before the mix.' },
    note: { type: 'string', description: 'Silent, or under 400 ms of sound: it has no loudness (its Volume is left alone).' },
    saved: { type: 'boolean', description: 'normalize_loudness: the wave was saved.' },
  } },
};
const MONO = 'Mono counts one channel, as EBU R128 does, so a mono wave reads 3 LU under the same sound in stereo.';

export const LOUDNESS_RECORDS: readonly CapabilityRecordSource[] = [
  buildCoreRecord({
    parentTool: 'manage_audio', action: 'measure_loudness', dispatchAction: 'manage_audio', domain: 'audio', family: 'authoring',
    editorStates: ['edit', 'pie', 'simulate'],
    summary: `Measure how loud Sound Waves are the way broadcast and streaming measure it: EBU R128 integrated loudness in LUFS from each wave's own samples (K-weighted, so a deep rumble counts less than speech at the same level; gated, so silences and quiet tails do not pull it down), the loudness each plays at through its Volume, and its sample peak; spreadLu says how far apart they play. A peak meter says how high a sound reaches, not how loud it sounds: two waves with the same peak can play 10 LU apart. ${MONO} Read only; normalize_loudness sets the Volumes.`,
    topics: ['loudness', 'lufs', 'how loud is a sound', 'compare sound levels', 'ebu r128', 'loudness of sound effects', 'which sound is louder'],
    whenToUse: [
      'Checking whether sound effects, dialogue or music play at consistent loudness before mixing.',
      'Finding the sound that is far louder or quieter than its neighbours.',
    ],
    whenNotToUse: [
      'Making them play at one loudness: use normalize_loudness.',
      'Hearing whether a sound plays at all in the level: use play_sound with measure.',
    ],
    inputProps: { soundPaths: SOUND_PATHS },
    required: ['soundPaths'],
    effect: 'read',
    costLatency: 'interactive',
    exampleInput: { action: 'measure_loudness', soundPaths: ['/Game/Audio/SW_Jump', '/Game/Audio/SW_Coin'] },
    exampleOutput: {
      success: true, message: 'Measured 2 Sound Wave(s); they play 7.9 LU apart.', spreadLu: 7.9,
      sounds: [{ soundPath: '/Game/Audio/SW_Jump.SW_Jump', lufs: -14.2, playsAtLufs: -14.2, volume: 1, peakDb: -0.3, peakAtVolumeDb: -0.3 },
        { soundPath: '/Game/Audio/SW_Coin.SW_Coin', lufs: -22.1, playsAtLufs: -22.1, volume: 1, peakDb: -6.4, peakAtVolumeDb: -6.4 }],
    },
    outputProps: { sounds: SOUNDS, spreadLu: { type: 'number', description: 'How many LU the loudest wave plays above the quietest (through their Volumes).' } },
    outputRequired: ['sounds'],
  }),
  buildCoreRecord({
    parentTool: 'manage_audio', action: 'normalize_loudness', dispatchAction: 'manage_audio', domain: 'audio', family: 'authoring',
    summary: `Make Sound Waves play at one loudness: each wave's EBU R128 integrated loudness is measured from its own samples (as measure_loudness does) and its Volume set so it plays at targetLufs, all in one undo step, each wave saved unless save is false. The samples stay as they are; peakAtVolumeDb says when a raised wave now peaks above full scale before the mix. A silent wave, or one under 400 ms of sound, keeps its Volume (note). ${MONO} Refused, changing nothing: ASSET_NOT_FOUND (a path that is not a Sound Wave), ENGINE_CONTENT (an engine wave: duplicate it into /Game first).`,
    topics: ['normalize loudness', 'match sound levels', 'make sounds equally loud', 'balance sound effects', 'loudness normalization', 'set sounds to lufs'],
    whenToUse: [
      'Sound effects, dialogue lines or music tracks must play at one loudness (a mixing pass, imported packs at different levels).',
    ],
    whenNotToUse: [
      'Only reading how loud they are: use measure_loudness.',
      'Changing the loudness of a whole category at play time: use a Sound Class volume or a Sound Mix.',
    ],
    inputProps: {
      soundPaths: SOUND_PATHS,
      targetLufs: { type: 'number', minimum: -70, maximum: 0, description: 'The loudness each wave should play at, in LUFS (-23 is broadcast; games and streaming commonly sit around -14 to -18).' },
      save: { type: 'boolean', description: 'Save each changed wave (default true); false leaves the change in memory only.' },
    },
    required: ['soundPaths', 'targetLufs'],
    effect: 'write',
    costLatency: 'interactive',
    exampleInput: { action: 'normalize_loudness', soundPaths: ['/Game/Audio/SW_Jump', '/Game/Audio/SW_Coin'], targetLufs: -18 },
    exampleOutput: {
      success: true, message: 'Set the Volume of 2 of 2 Sound Wave(s) to play at -18.0 LUFS.', spreadLu: 0,
      sounds: [{ soundPath: '/Game/Audio/SW_Jump.SW_Jump', lufs: -14.2, playsAtLufs: -18, volume: 0.646, peakDb: -0.3, peakAtVolumeDb: -4.1, saved: true },
        { soundPath: '/Game/Audio/SW_Coin.SW_Coin', lufs: -22.1, playsAtLufs: -18, volume: 1.603, peakDb: -6.4, peakAtVolumeDb: -2.3, saved: true }],
    },
    outputProps: {
      sounds: SOUNDS,
      spreadLu: { type: 'number', description: 'How many LU apart the waves play now (0 when every one reached the target).' },
      undo: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'What editor undo takes back: {undoable, transactionScope, steps}; every Volume set is one step.' },
    },
    outputRequired: ['sounds'],
  }),
  buildCoreRecord({
    parentTool: 'manage_audio', action: 'set_sound_volume', dispatchAction: 'manage_audio', domain: 'audio', family: 'authoring',
    summary: 'Make sounds louder or quieter: each Sound Wave\'s Volume (a MetaSound\'s too) or Sound Cue\'s Volume Multiplier set to volume (1 as recorded, 0.5 half, 0 silent) or changed by changeDb (6 is about twice as loud in level, -6 half; negative is quieter), every sound checked first, in one undo step, each saved unless save is false. The reply gives each one\'s volume before and after. To make several play at one loudness by ear, use normalize_loudness. Refused, changing nothing: ASSET_NOT_FOUND (not a Sound Wave, MetaSound or Sound Cue), ENGINE_CONTENT (an engine sound: duplicate it into /Game first), INVALID_ARGUMENT (both volume and changeDb; neither is MISSING_REQUIRED_ONEOF).',
    topics: ['make a sound louder or quieter', 'turn the music or a sound up or down', 'set sound volume'],
    whenToUse: ['A sound, a music track or a Sound Cue plays too loud or too quiet.'],
    whenNotToUse: ['Many sounds should play at one loudness: use normalize_loudness.', 'A whole category (music, effects) should change at play time: use a Sound Class or a Sound Mix.'],
    inputProps: {
      soundPaths: { type: 'array', minItems: 1, maxItems: 64, items: { type: 'string' }, description: 'The Sound Waves, MetaSounds or Sound Cues, by asset path (1-64).' },
      volume: { type: 'number', minimum: 0, maximum: 4, description: 'The volume to set: 1 as recorded, 0.5 half, 0 silent.' },
      changeDb: { type: 'number', minimum: -40, maximum: 24, description: 'How many dB louder each gets from where it is (negative is quieter); in place of volume.' },
      save: { type: 'boolean', description: 'Save each changed sound (default true); false leaves the change in memory only.' },
    },
    required: ['soundPaths'], requiredOneOf: ['volume', 'changeDb'],
    effect: 'write',
    costLatency: 'interactive',
    exampleInput: { action: 'set_sound_volume', soundPaths: ['/Game/Audio/SW_Music_Main'], changeDb: -6 },
    exampleOutput: { success: true, message: 'Set the volume of 1 sound(s).', sounds: [{ soundPath: '/Game/Audio/SW_Music_Main.SW_Music_Main', previousVolume: 1, volume: 0.501, saved: true }] },
    outputProps: {
      sounds: { type: 'array', description: 'Each sound changed.', items: { type: 'object', additionalProperties: false, properties: {
        soundPath: { type: 'string' }, previousVolume: { type: 'number' }, volume: { type: 'number' }, saved: { type: 'boolean' } } } },
      undo: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'What editor undo takes back: {undoable, transactionScope, steps}.' },
    },
    outputRequired: ['sounds'],
  }),
];
