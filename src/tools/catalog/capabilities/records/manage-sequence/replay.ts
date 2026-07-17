/**
 * Replay/demo records: start_demo_recording, stop_demo_recording,
 * configure_demo_settings, play_demo, pause_demo, seek_demo,
 * set_demo_playback_speed, configure_killcam_duration, start_killcam.
 *
 * Gated by MCP_HAS_REPLAY_API (ReplaySubsystem.h / OnlineSubsystem).
 *
 * ASYNC/ARTIFACT CONTRACT:
 * - start_demo_recording writes a replay to disk via the engine replay
 *   streamer. It does NOT return a completed replay; recording runs until
 *   stop_demo_recording. No interrupt/cancel - only graceful stop.
 * - Concurrent guards: cannot record while playing (ALREADY_PLAYING),
 *   cannot start if already recording (ALREADY_RECORDING),
 *   cannot stop if not recording (NOT_RECORDING).
 * - Validation limits: replay name <=128 chars ASCII [A-Za-z0-9_-],
 *   maxRecordTimeSeconds <=86400, playbackSpeed <=16,
 *   durationSeconds (killcam) <=600, checkpointSaveMaxMSPerFrame <=1000.
 * - Artifacts: replay files on disk under the configured demo name.
 */
import type { CapabilityRecordSource } from '../../index.js';
import { buildRecord, P } from './helpers.js';

const F = 'replay';
const D = 'replay';
const REPLAY_PLUGINS = ['LevelSequenceEditor', 'OnlineSubsystem'];
const NR = 'Distinct demo/replay operation with unique recording or playback lifecycle.';

export const REPLAY_RECORDS: readonly CapabilityRecordSource[] = [
  buildRecord({
    id: 'sequence.replay.start_demo_recording', action: 'start_demo_recording', family: F, domain: D,
    summary: 'Start recording a demo replay to disk. Recording runs until stop_demo_recording. No interrupt/cancel.',
    whenToUse: ['A demo replay must be recorded.'],
    whenNotToUse: ['A recording is already in progress (ALREADY_RECORDING).'],
    inputProps: { action: P.action, replayName: P.replayName, friendlyName: P.property },
    required: ['action'],
    effect: 'write',
    behavior: { longRunning: true, safeToRetry: false, supportsUndo: false },
    latency: 'long-running', resources: 'medium', plugins: REPLAY_PLUGINS,
    editorStates: ['edit', 'pie'],
    exampleInput: { action: 'start_demo_recording', replayName: 'Demo_01' },
    exampleOutput: { success: true, message: 'Demo recording started' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET',
    normalizationRationale: 'start_demo_recording is async; completion requires stop_demo_recording. No interrupt/cancel.',
  }),
  buildRecord({
    id: 'sequence.replay.stop_demo_recording', action: 'stop_demo_recording', family: F, domain: D,
    summary: 'Stop demo recording and return the replay name.',
    whenToUse: ['An in-progress demo recording must be stopped.'],
    whenNotToUse: ['No recording is in progress (NOT_RECORDING).'],
    inputProps: { action: P.action },
    required: ['action'],
    outputProps: { replayName: P.replayName },
    outputRequired: ['replayName'],
    effect: 'write', latency: 'interactive', resources: 'medium', plugins: REPLAY_PLUGINS,
    editorStates: ['edit', 'pie'],
    exampleInput: { action: 'stop_demo_recording' },
    exampleOutput: { success: true, replayName: 'Demo_01' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET',
    normalizationRationale: 'stop_demo_recording finalizes the replay. Distinct lifecycle from start.',
  }),
  buildRecord({
    id: 'sequence.replay.configure_demo_settings', action: 'configure_demo_settings', family: F, domain: D,
    summary: 'Configure replay settings (max record time, checkpoint interval, speed, actor priority).',
    whenToUse: ['Replay recording settings must be configured before recording.'],
    whenNotToUse: ['Default replay settings are acceptable.'],
    inputProps: { action: P.action, maxRecordTimeSeconds: { type: 'number', description: 'Max record time (<=86400).' }, checkpointSaveMaxMSPerFrame: { type: 'number', description: 'Max checkpoint save MS per frame (<=1000).' }, playbackSpeed: P.speed, prioritizeActors: P.actorNames, loadDefaultMapOnStop: { type: 'boolean', description: 'Whether to load the default map on stop.' } },
    required: ['action'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'instant', resources: 'low', plugins: REPLAY_PLUGINS,
    exampleInput: { action: 'configure_demo_settings', maxRecordTimeSeconds: 300, checkpointSaveMaxMSPerFrame: 100 },
    exampleOutput: { success: true, message: 'Demo settings configured' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildRecord({
    id: 'sequence.replay.play_demo', action: 'play_demo', family: F, domain: D,
    summary: 'Start playing a recorded demo replay.',
    whenToUse: ['A recorded demo replay must be played back.'],
    whenNotToUse: ['A recording is in progress (ALREADY_PLAYING).'],
    inputProps: { action: P.action, demoName: P.demoName },
    required: ['action', 'demoName'],
    effect: 'write', latency: 'instant', resources: 'medium', plugins: REPLAY_PLUGINS,
    editorStates: ['edit', 'pie'],
    exampleInput: { action: 'play_demo', demoName: 'Demo_01' },
    exampleOutput: { success: true, message: 'Demo playback started' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildRecord({
    id: 'sequence.replay.pause_demo', action: 'pause_demo', family: F, domain: D,
    summary: 'Pause demo replay playback.',
    whenToUse: ['Demo playback must be paused.'],
    whenNotToUse: ['Demo is not playing.'],
    inputProps: { action: P.action },
    required: ['action'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'instant', resources: 'low', plugins: REPLAY_PLUGINS,
    editorStates: ['edit', 'pie'],
    exampleInput: { action: 'pause_demo' },
    exampleOutput: { success: true, message: 'Demo paused' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildRecord({
    id: 'sequence.replay.seek_demo', action: 'seek_demo', family: F, domain: D,
    summary: 'Seek to a specific time in demo replay playback.',
    whenToUse: ['Demo playback must be seeked to a specific time.'],
    whenNotToUse: ['Demo is not playing.'],
    inputProps: { action: P.action, timeSeconds: { type: 'number', description: 'Seek time in seconds (<=86400).' } },
    required: ['action', 'timeSeconds'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'instant', resources: 'low', plugins: REPLAY_PLUGINS,
    editorStates: ['edit', 'pie'],
    exampleInput: { action: 'seek_demo', timeSeconds: 10.0 },
    exampleOutput: { success: true, message: 'Demo seeked' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildRecord({
    id: 'sequence.replay.set_demo_playback_speed', action: 'set_demo_playback_speed', family: F, domain: D,
    summary: 'Set demo replay playback speed (max 16x).',
    whenToUse: ['Demo playback speed must be changed.'],
    whenNotToUse: ['Demo is not playing.'],
    inputProps: { action: P.action, speed: P.speed },
    required: ['action', 'speed'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'instant', resources: 'low', plugins: REPLAY_PLUGINS,
    editorStates: ['edit', 'pie'],
    exampleInput: { action: 'set_demo_playback_speed', speed: 2.0 },
    exampleOutput: { success: true, message: 'Demo speed set' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildRecord({
    id: 'sequence.replay.configure_killcam_duration', action: 'configure_killcam_duration', family: F, domain: D,
    summary: 'Configure killcam replay duration (max 600 seconds).',
    whenToUse: ['Killcam duration must be set for replay-based kill cameras.'],
    whenNotToUse: ['No killcam is needed.'],
    inputProps: { action: P.action, durationSeconds: { type: 'number', description: 'Killcam duration in seconds (<=600).' } },
    required: ['action', 'durationSeconds'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'instant', resources: 'low', plugins: REPLAY_PLUGINS,
    exampleInput: { action: 'configure_killcam_duration', durationSeconds: 5.0 },
    exampleOutput: { success: true, message: 'Killcam duration configured' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildRecord({
    id: 'sequence.replay.start_killcam', action: 'start_killcam', family: F, domain: D,
    summary: 'Start a killcam replay playback for a specific duration.',
    whenToUse: ['A killcam replay must be triggered.'],
    whenNotToUse: ['No killcam is needed.'],
    inputProps: { action: P.action, demoName: P.demoName, durationSeconds: { type: 'number', description: 'Killcam duration in seconds (<=600).' } },
    required: ['action', 'demoName'],
    effect: 'write',
    behavior: { longRunning: true, safeToRetry: false },
    latency: 'long-running', resources: 'medium', plugins: REPLAY_PLUGINS,
    editorStates: ['edit', 'pie'],
    exampleInput: { action: 'start_killcam', demoName: 'Demo_01', durationSeconds: 3.0 },
    exampleOutput: { success: true, message: 'Killcam started' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET',
    normalizationRationale: 'start_killcam triggers a bounded-duration replay playback; distinct from continuous play_demo.',
  }),
];
