/**
 * Take Recorder records: create_take_recorder_panel, configure_take_sources,
 * start_recording, stop_recording, configure_recorded_tracks.
 *
 * Gated by MCP_SEQUENCE_HAS_TAKE_RECORDER_API (Takes plugin).
 *
 * ASYNC/ARTIFACT CONTRACT:
 * - start_recording begins recording into a ULevelSequence via
 *   UTakeRecorderBlueprintLibrary::StartRecording. It does NOT return a
 *   completed recording; the recording runs until stop_recording is called.
 * - stop_recording stops the recording and returns hasRecordedData.
 *   If no data was captured, returns RECORDING_OUTPUT_EMPTY.
 * - Take Recorder has NO interrupt/cancel. An in-progress recording must be
 *   stopped via stop_recording; there is no abort other than stop.
 * - Concurrent guard: starting while already recording returns
 *   ALREADY_RECORDING.
 * - Artifacts: a ULevelSequence asset containing recorded animation data.
 */
import type { CapabilityRecordSource } from '../../index.js';
import { buildRecord, P, TAKE_PLUGINS } from './helpers.js';

const F = 'take';
const D = 'take_recorder';
const NR = 'Distinct Take Recorder operation with unique source or recording lifecycle.';

export const TAKE_RECORDS: readonly CapabilityRecordSource[] = [
  buildRecord({
    id: 'sequence.take.create_take_recorder_panel', action: 'create_take_recorder_panel', family: F, domain: D,
    summary: 'Open or focus the Take Recorder panel in the editor.',
    whenToUse: ['The Take Recorder panel must be opened for recording.'],
    whenNotToUse: ['The panel is already open.'],
    inputProps: { action: P.action },
    required: ['action'],
    effect: 'write', latency: 'instant', resources: 'low', plugins: TAKE_PLUGINS,
    exampleInput: { action: 'create_take_recorder_panel' },
    exampleOutput: { success: true, message: 'Take Recorder panel opened' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildRecord({
    id: 'sequence.take.configure_take_sources', action: 'configure_take_sources', family: F, domain: D,
    summary: 'Configure Take Recorder sources (actors, components) for recording.',
    whenToUse: ['Recording sources must be specified before starting a take.'],
    whenNotToUse: ['Sources are already configured.'],
    inputProps: { action: P.action, sourceActors: P.actorNames, takePresetPath: P.takePresetPath, recordType: P.property },
    required: ['action'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low', plugins: TAKE_PLUGINS,
    exampleInput: { action: 'configure_take_sources', sourceActors: ['Actor1', 'Actor2'] },
    exampleOutput: { success: true, message: 'Take sources configured' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildRecord({
    id: 'sequence.take.start_recording', action: 'start_recording', family: F, domain: D,
    summary: 'Start a Take Recorder recording into a Level Sequence. Recording runs until stop_recording. No interrupt/cancel available.',
    whenToUse: ['A take recording must be started.'],
    whenNotToUse: ['A recording is already in progress (ALREADY_RECORDING).'],
    inputProps: { action: P.action, recordingSequencePath: P.recordingSequencePath, takeSequencePath: P.takeSequencePath, duration: { type: 'number', description: 'Optional max recording duration in seconds.' } },
    required: ['action'],
    effect: 'write',
    behavior: { longRunning: true, safeToRetry: false, supportsUndo: false },
    latency: 'long-running', resources: 'medium', plugins: TAKE_PLUGINS,
    editorStates: ['edit'],
    exampleInput: { action: 'start_recording', recordingSequencePath: '/Game/Takes/SEQ_Take01' },
    exampleOutput: { success: true, message: 'Recording started' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET',
    normalizationRationale: 'start_recording begins an async recording session; completion requires stop_recording. No interrupt/cancel.',
  }),
  buildRecord({
    id: 'sequence.take.stop_recording', action: 'stop_recording', family: F, domain: D,
    summary: 'Stop a Take Recorder recording and finalize the recorded Level Sequence. Returns hasRecordedData.',
    whenToUse: ['An in-progress take recording must be stopped and finalized.'],
    whenNotToUse: ['No recording is in progress (NOT_RECORDING).'],
    inputProps: { action: P.action },
    required: ['action'],
    outputProps: { hasRecordedData: { type: 'boolean', description: 'Whether recorded data was captured.' }, sequencePath: P.sequencePath },
    outputRequired: ['hasRecordedData'],
    effect: 'write', latency: 'interactive', resources: 'medium', plugins: TAKE_PLUGINS,
    exampleInput: { action: 'stop_recording' },
    exampleOutput: { success: true, hasRecordedData: true, sequencePath: '/Game/Takes/SEQ_Take01' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET',
    normalizationRationale: 'stop_recording finalizes the take; RECORDING_OUTPUT_EMPTY if no data. Distinct lifecycle from start.',
  }),
  buildRecord({
    id: 'sequence.take.configure_recorded_tracks', action: 'configure_recorded_tracks', family: F, domain: D,
    summary: 'Configure which tracks are recorded for each Take Recorder source.',
    whenToUse: ['Specific tracks (transform, animation, etc.) must be recorded per source.'],
    whenNotToUse: ['Default track recording is sufficient.'],
    inputProps: { action: P.action, sourceActors: P.actorNames, tracks: P.actorNames, reduceKeys: { type: 'boolean', description: 'Whether to reduce keyframes.' } },
    required: ['action'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low', plugins: TAKE_PLUGINS,
    exampleInput: { action: 'configure_recorded_tracks', sourceActors: ['Actor1'], tracks: ['transform', 'animation'] },
    exampleOutput: { success: true, message: 'Recorded tracks configured' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
];
