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
import type { CapabilityRecordSource } from '../../model.js';
import { buildRecord, P } from './helpers.js';

const F = 'replay';
const D = 'replay';
const REPLAY_PLUGINS = ['LevelSequenceEditor', 'OnlineSubsystem'];

export const REPLAY_RECORDS: readonly CapabilityRecordSource[] = [
  buildRecord({
    id: 'sequence.replay.start_demo_recording', action: 'start_demo_recording', family: F, domain: D,
    summary: 'Start recording the running game as a replay named replayName (or demoName; else the configure_demo_settings default, else McpReplay_<unix time>), labelled friendlyName, until stop_demo_recording. Refused while a replay plays (ALREADY_PLAYING) or records (ALREADY_RECORDING), and outside Play (NOT_IN_PIE). Replies give the replay state (activeReplayName, recording, playing, currentTimeSeconds, totalTimeSeconds, paused, playbackSpeed).',
    whenToUse: ['A demo replay must be recorded.'],
    whenNotToUse: ['A recording is already in progress (ALREADY_RECORDING).'],
    inputProps: { replayName: P.replayName, demoName: P.demoName, friendlyName: P.friendlyName, additionalOptions: P.additionalOptions },
    effect: 'write',
    behavior: { longRunning: true, safeToRetry: false },
    latency: 'long-running', resources: 'medium', plugins: REPLAY_PLUGINS,
    editorStates: ['edit', 'pie'],
    exampleInput: { action: 'start_demo_recording', replayName: 'Demo_01' },
  }),
  buildRecord({
    id: 'sequence.replay.stop_demo_recording', action: 'stop_demo_recording', family: F, domain: D,
    summary: 'Stop the recording start_demo_recording began and answer its replayName; NOT_RECORDING when none runs, NOT_IN_PIE outside Play.',
    whenToUse: ['An in-progress demo recording must be stopped.'],
    whenNotToUse: ['No recording is in progress (NOT_RECORDING).'],
    inputProps: {},
    outputProps: { replayName: P.replayName },
    outputRequired: ['replayName'],
    effect: 'write', latency: 'interactive', resources: 'medium', plugins: REPLAY_PLUGINS,
    editorStates: ['edit', 'pie'],
    exampleInput: { action: 'stop_demo_recording' },
    exampleOutput: { success: true, replayName: 'Demo_01' },
  }),
  buildRecord({
    id: 'sequence.replay.configure_demo_settings', action: 'configure_demo_settings', family: F, domain: D,
    summary: 'Configure replay settings (max record time, checkpoint interval, speed, actor priority).',
    whenToUse: ['Replay recording settings must be configured before recording.'],
    whenNotToUse: ['Default replay settings are acceptable.'],
    inputProps: { replayName: { type: 'string', description: 'Default replay name for recordings that do not name one.' }, maxRecordTimeSeconds: { type: 'number', description: 'Max record time (<=86400).' }, checkpointSaveMaxMSPerFrame: { type: 'number', description: 'Max checkpoint save MS per frame (<=1000).' }, playbackSpeed: P.speed, prioritizeActors: P.prioritizeActors, loadDefaultMapOnStop: { type: 'boolean', description: 'Whether to load the default map on stop.' }, friendlyName: P.friendlyName, additionalOptions: P.additionalOptions },
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'instant', resources: 'low', plugins: REPLAY_PLUGINS,
    exampleInput: { action: 'configure_demo_settings', maxRecordTimeSeconds: 300, checkpointSaveMaxMSPerFrame: 100 },
  }),
  buildRecord({
    id: 'sequence.replay.play_demo', action: 'play_demo', family: F, domain: D,
    summary: 'Play the replay replayName (or demoName) back in the running game at the configured playback speed. Refused while recording (ALREADY_RECORDING) and outside Play (NOT_IN_PIE); PLAYBACK_FAILED when the replay does not start. Replies give the replay state (activeReplayName, recording, playing, currentTimeSeconds, totalTimeSeconds, paused, playbackSpeed).',
    whenToUse: ['A recorded demo replay must be played back.'],
    whenNotToUse: ['A recording is in progress (ALREADY_PLAYING).'],
    inputProps: { demoName: P.demoName, replayName: P.replayName, additionalOptions: P.additionalOptions },
    effect: 'write', latency: 'instant', resources: 'medium', plugins: REPLAY_PLUGINS,
    editorStates: ['edit', 'pie'],
    exampleInput: { action: 'play_demo', demoName: 'Demo_01' },
  }),
  buildRecord({
    id: 'sequence.replay.pause_demo', action: 'pause_demo', family: F, domain: D,
    summary: 'Pause the playing replay (paused true) or resume it (paused false); without paused it toggles. NOT_PLAYING when no replay plays. Replies give the replay state (activeReplayName, recording, playing, currentTimeSeconds, totalTimeSeconds, paused, playbackSpeed).',
    whenToUse: ['Demo playback must be paused.'],
    whenNotToUse: ['Demo is not playing.'],
    inputProps: { paused: P.paused },
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'instant', resources: 'low', plugins: REPLAY_PLUGINS,
    editorStates: ['edit', 'pie'],
    exampleInput: { action: 'pause_demo' },
  }),
  buildRecord({
    id: 'sequence.replay.seek_demo', action: 'seek_demo', family: F, domain: D,
    summary: 'Jump the playing replay to timeSeconds, clamped to its length, and answer once playback is within 0.25 s of it (actualTimeSeconds); REPLAY_SEEK_TIMEOUT after 10 s, NOT_PLAYING when no replay plays.',
    whenToUse: ['Demo playback must be seeked to a specific time.'],
    whenNotToUse: ['Demo is not playing.'],
    inputProps: { timeSeconds: { type: 'number', description: 'Seek time in seconds (<=86400).' }, seconds: { type: 'number', description: 'Seek time in seconds (alias of timeSeconds).' } },
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'instant', resources: 'low', plugins: REPLAY_PLUGINS,
    editorStates: ['edit', 'pie'],
    exampleInput: { action: 'seek_demo', timeSeconds: 10.0 },
  }),
  buildRecord({
    id: 'sequence.replay.set_demo_playback_speed', action: 'set_demo_playback_speed', family: F, domain: D,
    summary: 'Set the playing replay\'s speed (speed or playbackSpeed, 1 is real time, at most 16); later playback keeps it. NOT_PLAYING when no replay plays.',
    whenToUse: ['Demo playback speed must be changed.'],
    whenNotToUse: ['Demo is not playing.'],
    inputProps: { speed: P.speed, playbackSpeed: P.speed },
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'instant', resources: 'low', plugins: REPLAY_PLUGINS,
    editorStates: ['edit', 'pie'],
    exampleInput: { action: 'set_demo_playback_speed', speed: 2.0 },
  }),
  buildRecord({
    id: 'sequence.replay.configure_killcam_duration', action: 'configure_killcam_duration', family: F, domain: D,
    summary: 'Configure killcam replay duration (max 600 seconds).',
    whenToUse: ['Killcam duration must be set for replay-based kill cameras.'],
    whenNotToUse: ['No killcam is needed.'],
    inputProps: { durationSeconds: { type: 'number', description: 'Killcam duration in seconds (<=600).' } },
    required: ['durationSeconds'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'instant', resources: 'low', plugins: REPLAY_PLUGINS,
    exampleInput: { action: 'configure_killcam_duration', durationSeconds: 5.0 },
  }),
  buildRecord({
    id: 'sequence.replay.start_killcam', action: 'start_killcam', family: F, domain: D,
    summary: 'Play back the last durationSeconds (default the configure_killcam_duration value) of the playing replay, starting replayName first when none plays, and answer once playback reaches that point; NOT_AVAILABLE without a replay to play.',
    whenToUse: ['A killcam replay must be triggered.'],
    whenNotToUse: ['No killcam is needed.'],
    inputProps: { demoName: P.demoName, replayName: P.replayName, durationSeconds: { type: 'number', description: 'Killcam duration in seconds (<=600).' }, additionalOptions: P.additionalOptions },
    effect: 'write',
    behavior: { longRunning: true, safeToRetry: false },
    latency: 'long-running', resources: 'medium', plugins: REPLAY_PLUGINS,
    editorStates: ['edit', 'pie'],
    exampleInput: { action: 'start_killcam', demoName: 'Demo_01', durationSeconds: 3.0 },
  }),
];
