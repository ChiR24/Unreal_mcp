/**
 * Timeline playback records: play, pause, stop, set_playback_speed,
 * get_properties, set_properties.
 *
 * Grounded in sequence-playback-actions.ts. Playback operates on the
 * currently open sequence in the Sequencer editor.
 */
import type { CapabilityRecordSource } from '../../index.js';
import { buildRecord, P, SEQ_PLUGINS } from './helpers.js';

const F = 'timeline';
const D = 'sequence';
const NR = 'Distinct Sequencer playback operation with unique timeline semantics.';

export const TIMELINE_PLAYBACK_RECORDS: readonly CapabilityRecordSource[] = [
  buildRecord({
    id: 'sequence.play', action: 'play', family: F, domain: D,
    summary: 'Start playing the currently open Level Sequence.',
    whenToUse: ['Sequence playback must be started for preview or PIE.'],
    whenNotToUse: ['The sequence is already playing.'],
    inputProps: { action: P.action, path: P.path, startTime: P.start, loopMode: P.property },
    required: ['action', 'path'],
    effect: 'write', latency: 'instant', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'play', path: '/Game/Cinematics/SEQ_Master' },
    exampleOutput: { success: true, message: 'Playback started' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildRecord({
    id: 'sequence.pause', action: 'pause', family: F, domain: D,
    summary: 'Pause playback of the currently open Level Sequence.',
    whenToUse: ['Sequence playback must be paused.'],
    whenNotToUse: ['The sequence is not playing.'],
    inputProps: { action: P.action, path: P.path },
    required: ['action', 'path'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'instant', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'pause', path: '/Game/Cinematics/SEQ_Master' },
    exampleOutput: { success: true, message: 'Playback paused' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildRecord({
    id: 'sequence.stop', action: 'stop', family: F, domain: D,
    summary: 'Stop playback and reset the playhead of the open Level Sequence.',
    whenToUse: ['Sequence playback must be stopped and the playhead reset.'],
    whenNotToUse: ['The sequence should only be paused.'],
    inputProps: { action: P.action, path: P.path },
    required: ['action', 'path'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'instant', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'stop', path: '/Game/Cinematics/SEQ_Master' },
    exampleOutput: { success: true, message: 'Playback stopped' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildRecord({
    id: 'sequence.set_playback_speed', action: 'set_playback_speed', family: F, domain: D,
    summary: 'Set the playback speed multiplier of the open Level Sequence.',
    whenToUse: ['Playback speed must be changed for preview.'],
    whenNotToUse: ['The speed value is not a positive number.'],
    inputProps: { action: P.action, path: P.path, speed: P.speed },
    required: ['action', 'path', 'speed'],
    effect: 'write', latency: 'instant', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'set_playback_speed', path: '/Game/Cinematics/SEQ_Master', speed: 0.5 },
    exampleOutput: { success: true, message: 'Playback speed set' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildRecord({
    id: 'sequence.get_properties', action: 'get_properties', family: F, domain: D,
    summary: 'Read playback properties (frame rate, length, range) of a Level Sequence.',
    whenToUse: ['Sequence timing properties must be inspected.'],
    whenNotToUse: ['Properties are being set rather than read.'],
    inputProps: { action: P.action, path: P.path },
    required: ['action', 'path'],
    outputProps: {
      frameRate: P.frameRate, lengthInFrames: { type: 'integer', description: 'Sequence length in frames.' },
      playbackStart: { type: 'integer', description: 'Playback start frame.' },
      playbackEnd: { type: 'integer', description: 'Playback end frame.' },
    },
    outputRequired: ['frameRate'],
    effect: 'read', latency: 'instant', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'get_properties', path: '/Game/Cinematics/SEQ_Master' },
    exampleOutput: { success: true, frameRate: 24, lengthInFrames: 120 },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildRecord({
    id: 'sequence.set_properties', action: 'set_properties', family: F, domain: D,
    summary: 'Set playback properties (frame rate, length, range) of a Level Sequence.',
    whenToUse: ['Sequence timing properties must be configured.'],
    whenNotToUse: ['Properties are being read rather than set.'],
    inputProps: { action: P.action, path: P.path, frameRate: P.frameRate,
      lengthInFrames: { type: 'integer', description: 'Sequence length in frames.' },
      playbackStart: { type: 'integer', description: 'Playback start frame.' },
      playbackEnd: { type: 'integer', description: 'Playback end frame.' } },
    required: ['action', 'path'],
    effect: 'write', latency: 'interactive', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'set_properties', path: '/Game/Cinematics/SEQ_Master', frameRate: 30 },
    exampleOutput: { success: true, message: 'Properties set' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
];
