/**
 * Subtitle records: a Sound Wave's subtitle cues, which the engine shows at the bottom of the game's screen while the
 * sound plays. Native Domains/AudioAuthoring (McpAudioSubtitles).
 */
import type { CapabilityRecordSource } from '../../model.js';
import { buildCoreRecord } from '../core/builder.js';

const CUES = {
  type: 'array', maxItems: 200, description: 'The cues as the wave holds them now, in time order.',
  items: { type: 'object', additionalProperties: false, properties: {
    text: { type: 'string', description: 'The words shown.' },
    time: { type: 'number', description: 'Seconds into the sound the cue shows from; it stays until the next cue or the sound ends.' },
  } },
};

export const SUBTITLE_RECORDS: readonly CapabilityRecordSource[] = [
  buildCoreRecord({
    parentTool: 'manage_audio', action: 'set_subtitles', dispatchAction: 'manage_audio', domain: 'audio', family: 'authoring',
    summary: 'Give a Sound Wave subtitles: subtitles [{text, time}] replaces its cues (time is the seconds into the sound a cue shows from; each stays until the next one or the end), [] takes them off. The engine shows them at the bottom of the game\'s screen whenever the wave plays, through a Sound Cue too (not inside a MetaSound, which plays its waves itself), unless the game has subtitles switched off (a warning says so). One undo step, saved unless save is false; the cues come back as the wave holds them. Every cue is checked first, so a bad one changes nothing: ASSET_NOT_FOUND (not a Sound Wave: a Sound Cue plays waves, give those the subtitles), ENGINE_CONTENT (an engine wave: duplicate it into /Game first), INVALID_ARGUMENT (a cue with no text, or a time outside the sound).',
    topics: ['add subtitles', 'subtitles for spoken lines', 'show subtitles in the game while a sound plays', 'closed captions'],
    whenToUse: ['Dialogue lines, a narrator or important sounds must show their words on screen while they play.'],
    whenNotToUse: ['A text should show on screen without a sound: use a widget (manage_blueprint create_widget_template or add_behaviour hint).'],
    inputProps: {
      soundPath: { type: 'string', description: 'The Sound Wave, by asset path.' },
      subtitles: {
        type: 'array', maxItems: 200, description: 'The cues, [{text, time}]: the words, and the seconds into the sound they show from (0 to the sound\'s length). [] takes them all off.',
        items: { type: 'object', additionalProperties: false, properties: { text: { type: 'string' }, time: { type: 'number', minimum: 0 } }, required: ['text'] },
      },
      save: { type: 'boolean', description: 'Save the wave (default true); false leaves the change in memory only.' },
    },
    required: ['soundPath', 'subtitles'],
    effect: 'write',
    costLatency: 'interactive',
    exampleInput: { action: 'set_subtitles', soundPath: '/Game/Audio/SW_Guard_Halt', subtitles: [{ text: 'Halt! Who goes there?', time: 0 }, { text: 'Show yourself.', time: 1.8 }] },
    exampleOutput: {
      success: true, message: '2 subtitle cue(s) on SW_Guard_Halt; they show while it plays in the game.', soundPath: '/Game/Audio/SW_Guard_Halt.SW_Guard_Halt', saved: true,
      subtitles: [{ text: 'Halt! Who goes there?', time: 0 }, { text: 'Show yourself.', time: 1.8 }],
    },
    outputProps: {
      soundPath: { type: 'string', description: 'The wave changed.' },
      subtitles: CUES,
      saved: { type: 'boolean', description: 'The wave was saved.' },
      undo: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'What editor undo takes back: {undoable, transactionScope, steps}.' },
    },
    outputRequired: ['soundPath', 'subtitles'],
  }),
];
