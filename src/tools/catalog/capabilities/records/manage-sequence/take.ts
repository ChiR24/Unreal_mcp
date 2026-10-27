/**
 * Take Recorder records: create_take_recorder_panel, configure_take_sources,
 * start_recording, stop_recording, configure_recorded_tracks.
 *
 * Gated by MCP_SEQUENCE_HAS_TAKE_RECORDER_API (Takes plugin).
 *
 * ASYNC/ARTIFACT CONTRACT:
 * - start_recording begins recording into a ULevelSequence via
 *   UTakeRecorderBlueprintLibrary::StartRecording. It does NOT return a
 *   completed recording; the recording runs for duration seconds when given,
 *   otherwise until stop_recording is called.
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
    summary: 'Open the Take Recorder panel (or bring it forward) and read its state: panelOpen, mode (NewRecording, RecordingInto, EditingPreset or ReviewingRecording), the sequencePath it is set up with, frameRate (with frameRateNumerator and frameRateDenominator), frameRateFromTimecode and sourceCount. Needs the Takes plugin (NOT_AVAILABLE). Every other Take Recorder action except stop_recording opens the panel itself.',
    whenToUse: ['The Take Recorder state (mode, sequence, frame rate, sources) must be read before setting up a take.'],
    whenNotToUse: ['Sources or tracks are being set (configure_take_sources and configure_recorded_tracks open the panel themselves).'],
    inputProps: {},
    effect: 'write', latency: 'instant', resources: 'low', plugins: TAKE_PLUGINS,
    exampleInput: { action: 'create_take_recorder_panel' },
  }),
  buildRecord({
    id: 'sequence.take.configure_take_sources', action: 'configure_take_sources', family: F, domain: D,
    summary: 'Set what the next take records: actors by name (actorName, actorNames, actors or sourceActors; in the editor world or the running game) are added as enabled actor sources, and sourceClasses adds allowlisted source classes; clearSources true removes every source first. Before the sources it applies the take setup the call names: a Level Sequence (sequencePath, recorded as a new take based on it, or into it with recordInto true) or a takePresetPath, and frameRate. recordType, recordParentHierarchy and reduceKeys set the options of the actor sources. The reply gives addedSources, requestedSources and sourceCount. Refused: no actors, classes or clearSources (INVALID_ARGUMENT), an actor not found (ACTOR_NOT_FOUND), a class not allowlisted (INVALID_SOURCE_CLASS), any change while recording (RECORDING_ACTIVE); a refused call leaves the sources and the panel as they were.',
    whenToUse: ['Actors (characters, cameras, props) must be chosen for recording before a take starts.'],
    whenNotToUse: ['Only which properties an actor records changes (configure_recorded_tracks).'],
    // ConfigurePanel (TakeRecorderRuntime.cpp) applies the sequence or take
    // preset, frameRate and recordInto before the sources are configured.
    inputProps: { sourceActors: P.actorNames, actorNames: P.actorNames, actorName: P.actorName, sourceClasses: P.sourceClasses, clearSources: P.clearSources, takePresetPath: P.takePresetPath, recordType: P.recordType, actors: A.actors, recordParentHierarchy: A.recordParentHierarchy, reduceKeys: REDUCE_KEYS, recordingSequencePath: P.recordingSequencePath, takeSequencePath: P.takeSequencePath, sequencePath: P.sequencePath, frameRate: P.frameRate, recordInto: P.recordInto },
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low', plugins: TAKE_PLUGINS,
    exampleInput: { action: 'configure_take_sources', sourceActors: ['Actor1', 'Actor2'] },
  }),
  buildRecord({
    id: 'sequence.take.start_recording', action: 'start_recording', family: F, domain: D,
    summary: 'Start a Take Recorder take, set up as configure_take_sources sets it when any source or sequence field is sent. The panel needs a Level Sequence and at least one enabled, valid source; otherwise CANNOT_START_RECORDING with hasSequence, sourceCount and validSourceCount. The reply comes once recording has begun after the countdown: sequencePath, sequencePackagePath, subsceneFolderPath, state, recording and countdownSeconds. A recorder that stops first is RECORDING_START_FAILED and one not started within the countdown plus 10 s RECORDING_START_TIMEOUT; both put the setup back (rolledBack). duration (seconds after the countdown) stops the take by itself, otherwise stop_recording ends it. Refused while a take records (RECORDING_ACTIVE). Sequencer is not opened.',
    whenToUse: ['A take recording must be started.'],
    whenNotToUse: ['A take is already recording (RECORDING_ACTIVE).'],
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
    summary: 'Stop the take that is recording and finalize its Level Sequence: sequencePath, sequencePackagePath, subsceneFolderPath (where the per-source sequences go), state, recording false, isRecorded, bindingCount, trackCount and hasRecordedData. A take that recorded nothing answers RECORDING_OUTPUT_EMPTY instead of success; nothing recording is NOT_RECORDING with the recorder state.',
    whenToUse: ['An in-progress take recording must be stopped and finalized.'],
    whenNotToUse: ['No recording is in progress (NOT_RECORDING).'],
    inputProps: {},
    outputProps: { hasRecordedData: { type: 'boolean', description: 'Whether the take is marked recorded and has bindings or tracks.' }, sequencePath: P.sequencePath, sequencePackagePath: { type: 'string', description: 'Package of the recorded sequence.' }, subsceneFolderPath: { type: 'string', description: 'Folder of the per-source subsequences.' }, state: { type: 'string', description: 'Recorder state after the stop.' }, recording: { type: 'boolean', description: 'False once stopped.' }, isRecorded: { type: 'boolean', description: 'Whether the sequence is marked as a recorded take.' }, bindingCount: { type: 'number', description: 'Object bindings in the recorded sequence.' }, trackCount: { type: 'number', description: 'Tracks in the recorded sequence, master and bound.' } },
    outputRequired: ['hasRecordedData'],
    effect: 'write', latency: 'interactive', resources: 'medium', plugins: TAKE_PLUGINS,
    exampleInput: { action: 'stop_recording' },
    exampleOutput: { success: true, hasRecordedData: true, sequencePath: '/Game/Takes/SEQ_Take01' },
  }),
  buildRecord({
    id: 'sequence.take.configure_recorded_tracks', action: 'configure_recorded_tracks', family: F, domain: D,
    summary: 'Choose what actor sources record: the recorded properties named in tracks, properties or trackNames (as the source lists them, such as Transform; matched without case) turn on, or off with enabled false, and disableOthers turns every other one off. It acts on the sources of the actors named (an actor not yet a source is added first) or on every actor source; recordType, recordParentHierarchy and reduceKeys set their options. The reply gives actorSourceCount, matchedProperties, changedProperties and appliedSourceOptions. Refused: nothing to change (INVALID_ARGUMENT), no actor sources (NO_ACTOR_SOURCES), no named property found (RECORDED_PROPERTY_NOT_FOUND), any change while recording (RECORDING_ACTIVE); a refused call removes the sources it added.',
    whenToUse: ['A take should record only some properties of an actor, such as its transform but not its lights.'],
    whenNotToUse: ['Which actors record changes (configure_take_sources).'],
    inputProps: { sourceActors: P.actorNames, actorNames: P.actorNames, actorName: P.actorName, tracks: P.recordedTracks, reduceKeys: REDUCE_KEYS, sequencePath: P.sequencePath, frameRate: P.frameRate, recordInto: P.recordInto, properties: A.properties, trackNames: A.trackNames, actors: A.actors, enabled: A.enabled, disableOthers: A.disableOthers, recordParentHierarchy: A.recordParentHierarchy, recordType: P.recordType },
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low', plugins: TAKE_PLUGINS,
    exampleInput: { action: 'configure_recorded_tracks', sourceActors: ['Actor1'], tracks: ['transform', 'animation'] },
  }),
];
