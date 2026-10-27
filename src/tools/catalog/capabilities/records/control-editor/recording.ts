/**
 * Demo recording records: start_recording, stop_recording.
 */
import type { CapabilityRecordSource } from '../../model.js';
import { buildCoreRecord } from '../core/builder.js';
import { P } from './properties.js';

const F = 'recording';
const D = 'editor';

export const RECORDING_RECORDS: readonly CapabilityRecordSource[] = [
  buildCoreRecord({
    parentTool: 'control_editor', action: 'start_recording', domain: D, family: F,
    summary: 'Start a replay (demo) recording of the running Play In Editor game with DemoRec, named filename (or name; default Recording_<date and time>) and written by the replay streamer, by default under Saved/Demos. frameRate (1 to 120) sets demo.RecordHz, which stays set for later recordings; durationSeconds stops it after that much game time. NO_ACTIVE_SESSION without a running game, RECORDING_NOT_STARTED when no recorder starts.',
    whenToUse: ['The running game must be recorded as a replay.'],
    whenNotToUse: ['A recording is already in progress.', 'The replay must be played back, sought or used as a killcam: the sequence demo actions (start_demo_recording, play_demo) handle the whole replay.'],
    inputProps: { filename: P.filename, name: P.name, frameRate: P.frameRate, durationSeconds: P.durationSeconds },
    required: [],
    effect: 'write',
    costLatency: 'interactive',
    exampleInput: { action: 'start_recording', filename: 'TestRecording' },
    exampleOutput: { success: true, message: 'Started recording to TestRecording', filename: 'TestRecording' },
    outputProps: { filename: P.filename },
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'stop_recording', domain: D, family: F,
    summary: 'Stop the replay recording running in the Play In Editor game (DemoStop), which writes it out. NOT_RECORDING when no game runs or nothing records, which is also what a recording durationSeconds already ended leaves.',
    whenToUse: ['An in-progress replay recording must be stopped.'],
    whenNotToUse: ['No recording is in progress.'],
    inputProps: {},
    required: [],
    effect: 'write',
    costLatency: 'interactive',
    exampleInput: { action: 'stop_recording' },
  }),
];
