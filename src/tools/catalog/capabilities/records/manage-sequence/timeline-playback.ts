/**
 * Timeline playback records: play, pause, stop, set_playback_speed,
 * get_properties, set_properties.
 *
 * Grounded in sequence-playback-actions.ts. Playback operates on the
 * currently open sequence in the Sequencer editor.
 */
import type { CapabilityRecordSource } from '../../model.js';
import { buildRecord, P, SEQ_PLUGINS } from './helpers.js';

const F = 'timeline';
const D = 'sequence';

const START_TIME = {
  type: 'number', minimum: 0,
  description: 'Sequence time in seconds. play starts there; pause jumps the playhead there and holds that frame, so the viewport and screenshots show the scene at that moment. A time past the end of the playback range is refused: frame N at 24 fps is N / 24 seconds.',
};

export const TIMELINE_PLAYBACK_RECORDS: readonly CapabilityRecordSource[] = [
  buildRecord({
    id: 'sequence.play', action: 'play', family: F, domain: D,
    topics: ['play sequence', 'play cinematic', 'preview sequence', 'play cutscene', 'preview cutscene'],
    summary: 'Open a Level Sequence (path) in Sequencer and play it as an editor preview in the level viewport. startTime jumps there first; without it play carries on from the playhead (from the start when it sits at the end). loopMode once or loop sets the Sequencer\'s own loop setting, which stays for later previews. The reply reads back startTime (seconds), currentFrame and playbackStart/playbackEnd in display frames. Nothing in the asset or level changes. A time past the playback range or another loopMode is INVALID_ARGUMENT; a path that is not a Level Sequence is SEQUENCE_NOT_FOUND.',
    whenToUse: ['A sequence must be previewed in the editor viewport.', 'A screenshot must show the scene moving through a shot: play, then pause with startTime to hold one moment.'],
    whenNotToUse: ['The sequence must play in game or Play In Editor: that runs through a level sequence actor placed for it, not the Sequencer preview.'],
    inputProps: { path: P.path, startTime: START_TIME, loopMode: { type: 'string', enum: ['once', 'loop'], description: 'Sequencer loop mode for the preview: once or loop.' } },
    required: ['path'],
    effect: 'write', latency: 'instant', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'play', path: '/Game/Cinematics/SEQ_Master' },
  }),
  buildRecord({
    id: 'sequence.pause', action: 'pause', family: F, domain: D,
    topics: ['sequencer playhead', 'scrub sequence', 'set sequence time', 'go to frame', 'hold a frame'],
    summary: 'Pause the Sequencer preview of a Level Sequence, or hold it at one moment to inspect it: startTime (seconds) moves the playhead there and poses the animated meshes at once, so a screenshot taken next shows that moment even with the editor in the background; a sequence not open is opened first. The reply reads back currentFrame in display frames. Without startTime a sequence not open in Sequencer is EDITOR_NOT_OPEN; a time past the playback range is INVALID_ARGUMENT. Nothing in the asset changes.',
    whenToUse: ['Sequence playback must be paused.', 'A pose, camera or effect must be checked at one moment of the sequence: pause with startTime, then take a screenshot.'],
    whenNotToUse: ['The sequence is not open in Sequencer (play opens it).'],
    inputProps: { path: P.path, startTime: START_TIME },
    required: ['path'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'instant', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'pause', path: '/Game/Cinematics/SEQ_Master' },
  }),
  buildRecord({
    id: 'sequence.stop', action: 'stop', family: F, domain: D,
    summary: 'Stop the Sequencer preview of a Level Sequence and put the playhead back on the start of its playback range (where Jump to Start goes, not frame 0), posing the animated meshes there. The reply reads back currentFrame in display frames. A sequence not open in Sequencer is EDITOR_NOT_OPEN; nothing in the asset changes.',
    whenToUse: ['Sequence playback must be stopped and the playhead reset to the start.'],
    whenNotToUse: ['The sequence should only be paused.'],
    inputProps: { path: P.path },
    required: ['path'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'instant', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'stop', path: '/Game/Cinematics/SEQ_Master' },
  }),
  buildRecord({
    id: 'sequence.set_playback_speed', action: 'set_playback_speed', family: F, domain: D,
    summary: 'Set the play rate of a Level Sequence (speed, above 0; 1 is normal, 0.5 half speed) on every level sequence actor in the editor level that plays it, which is the rate it runs at in game and Play In Editor, and on the Sequencer preview when the sequence is open there (a preview setting that is not saved). The actors change in one undo step and the level is left unsaved. The reply lists the actors (levelSequenceActors) and sequencerUpdated. With neither an actor nor an open Sequencer it is PLAYBACK_TARGET_NOT_FOUND; speed 0 or below is INVALID_ARGUMENT.',
    whenToUse: ['A sequence must play faster or slower in game, or in the preview.'],
    whenNotToUse: ['The speed value is not a positive number.'],
    inputProps: { path: P.path, speed: P.speed },
    required: ['path', 'speed'],
    effect: 'write', latency: 'instant', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'set_playback_speed', path: '/Game/Cinematics/SEQ_Master', speed: 0.5 },
  }),
  buildRecord({
    id: 'sequence.get_properties', action: 'get_properties', family: F, domain: D,
    summary: 'Read the playback properties of a Level Sequence: frameRate (the display rate, a number), playbackStart and playbackEnd in display-rate frames, and lengthInFrames. An asset that is not a Level Sequence is INVALID_SEQUENCE.',
    whenToUse: ['Sequence timing properties must be inspected.'],
    whenNotToUse: ['Properties are being set rather than read.'],
    inputProps: { path: P.path },
    required: ['path'],
    outputProps: {
      frameRate: P.frameRate, lengthInFrames: { type: 'integer', description: 'Sequence length in frames.' },
      playbackStart: { type: 'integer', description: 'Playback start frame.' },
      playbackEnd: { type: 'integer', description: 'Playback end frame.' },
    },
    outputRequired: ['frameRate'],
    effect: 'read', latency: 'instant', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'get_properties', path: '/Game/Cinematics/SEQ_Master' },
    exampleOutput: { success: true, frameRate: 24, lengthInFrames: 120 },
  }),
  buildRecord({
    id: 'sequence.set_properties', action: 'set_properties', family: F, domain: D,
    summary: 'Set the display frame rate of a Level Sequence (frameRate, such as 24 or 29.97) and its playback range in display-rate frames: playbackStart, and playbackEnd or lengthInFrames (end = start + length; playbackEnd wins). Send at least one; one undo step, and the sequence is left unsaved. The reply reads the values back; a negative frame or an unreadable rate is INVALID_ARGUMENT, another asset type INVALID_SEQUENCE.',
    whenToUse: ['Sequence timing properties must be configured.'],
    whenNotToUse: ['Properties are being read rather than set.'],
    inputProps: { path: P.path, frameRate: P.frameRate,
      lengthInFrames: { type: 'integer', description: 'Playback length in display-rate frames, counted from playbackStart (or the current start); playbackEnd wins when both are sent.' },
      playbackStart: { type: 'integer', description: 'Playback start, in display-rate frames.' },
      playbackEnd: { type: 'integer', description: 'Playback end, in display-rate frames.' } },
    required: ['path'],
    effect: 'write', latency: 'interactive', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'set_properties', path: '/Game/Cinematics/SEQ_Master', frameRate: 30 },
  }),
];
