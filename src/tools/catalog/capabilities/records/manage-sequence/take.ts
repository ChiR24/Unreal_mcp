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
import type { CapabilityRecordSource } from '../../model.js';
import { A } from './alias-props.js';
import { buildRecord, P, TAKE_PLUGINS } from './helpers.js';

const F = 'take';
const D = 'take_recorder';
const REDUCE_KEYS = { type: 'boolean', description: 'Whether to reduce keyframes.' };

export const TAKE_RECORDS: readonly CapabilityRecordSource[] = [
  buildRecord({
    id: 'sequence.take.create_take_recorder_panel', action: 'create_take_recorder_panel', family: F, domain: D,
    summary: 'Open or focus the Take Recorder panel in the editor.',
    whenToUse: ['The Take Recorder panel must be opened for recording.'],
    whenNotToUse: ['The panel is already open.'],
    inputProps: {},
    effect: 'write', latency: 'instant', resources: 'low', plugins: TAKE_PLUGINS,
    exampleInput: { action: 'create_take_recorder_panel' },
  }),
  buildRecord({
    id: 'sequence.take.configure_take_sources', action: 'configure_take_sources', family: F, domain: D,
    summary: 'Configure Take Recorder sources (actors, components) for recording.',
    whenToUse: ['Recording sources must be specified before starting a take.'],
    whenNotToUse: ['Sources are already configured.'],
    // ConfigurePanel (TakeRecorderRuntime.cpp) applies the sequence or take
    // preset, frameRate and recordInto before the sources are configured.
    inputProps: { sourceActors: P.actorNames, actorNames: P.actorNames, actorName: P.actorName, sourceClasses: P.sourceClasses, clearSources: P.clearSources, takePresetPath: P.takePresetPath, recordType: P.recordType, actors: A.actors, recordParentHierarchy: A.recordParentHierarchy, reduceKeys: REDUCE_KEYS, recordingSequencePath: P.recordingSequencePath, takeSequencePath: P.takeSequencePath, sequencePath: P.sequencePath, frameRate: P.frameRate, recordInto: P.recordInto },
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low', plugins: TAKE_PLUGINS,
    exampleInput: { action: 'configure_take_sources', sourceActors: ['Actor1', 'Actor2'] },
  }),
  buildRecord({
    id: 'sequence.take.start_recording', action: 'start_recording', family: F, domain: D,
    summary: 'Start a Take Recorder recording into a Level Sequence. Recording runs until stop_recording. No interrupt/cancel available.',
    whenToUse: ['A take recording must be started.'],
    whenNotToUse: ['A recording is already in progress (ALREADY_RECORDING).'],
    // HandleStartTakeRecording (TakeRecorderRecording.cpp) configures sources
    // inline when any source field is sent, and stops the take after duration.
    inputProps: { sequencePath: P.sequencePath, recordingSequencePath: P.recordingSequencePath, takeSequencePath: P.takeSequencePath, recordInto: P.recordInto, frameRate: P.frameRate, duration: { type: 'number', minimum: 0, maximum: 86400, description: 'Seconds (above 0) to record after the countdown before the take stops by itself.' }, sourceActors: P.actorNames, actorNames: P.actorNames, actorName: P.actorName, sourceClasses: P.sourceClasses, clearSources: P.clearSources, recordType: P.recordType, recordParentHierarchy: A.recordParentHierarchy, reduceKeys: REDUCE_KEYS },
    effect: 'write',
    behavior: { longRunning: true, safeToRetry: false },
    latency: 'long-running', resources: 'medium', plugins: TAKE_PLUGINS,
    editorStates: ['edit'],
    exampleInput: { action: 'start_recording', recordingSequencePath: '/Game/Takes/SEQ_Take01' },
  }),
  buildRecord({
    id: 'sequence.take.stop_recording', action: 'stop_recording', family: F, domain: D,
    summary: 'Stop a Take Recorder recording and finalize the recorded Level Sequence. Returns hasRecordedData.',
    whenToUse: ['An in-progress take recording must be stopped and finalized.'],
    whenNotToUse: ['No recording is in progress (NOT_RECORDING).'],
    inputProps: {},
    outputProps: { hasRecordedData: { type: 'boolean', description: 'Whether recorded data was captured.' }, sequencePath: P.sequencePath },
    outputRequired: ['hasRecordedData'],
    effect: 'write', latency: 'interactive', resources: 'medium', plugins: TAKE_PLUGINS,
    exampleInput: { action: 'stop_recording' },
    exampleOutput: { success: true, hasRecordedData: true, sequencePath: '/Game/Takes/SEQ_Take01' },
  }),
  buildRecord({
    id: 'sequence.take.configure_recorded_tracks', action: 'configure_recorded_tracks', family: F, domain: D,
    summary: 'Configure which tracks are recorded for each Take Recorder source.',
    whenToUse: ['Specific tracks (transform, animation, etc.) must be recorded per source.'],
    whenNotToUse: ['Default track recording is sufficient.'],
    inputProps: { sourceActors: P.actorNames, actorNames: P.actorNames, actorName: P.actorName, tracks: P.recordedTracks, reduceKeys: REDUCE_KEYS, sequencePath: P.sequencePath, frameRate: P.frameRate, recordInto: P.recordInto, properties: A.properties, trackNames: A.trackNames, actors: A.actors, enabled: A.enabled, disableOthers: A.disableOthers, recordParentHierarchy: A.recordParentHierarchy, recordType: P.recordType },
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low', plugins: TAKE_PLUGINS,
    exampleInput: { action: 'configure_recorded_tracks', sourceActors: ['Actor1'], tracks: ['transform', 'animation'] },
  }),
];
