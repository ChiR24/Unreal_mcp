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
    summary: 'Start a replay (demo) recording of the running game; Play In Editor must be running. Fails when no recorder started.',
    whenToUse: ['A demo recording must be started for replay capture.'],
    whenNotToUse: ['A recording is already in progress.'],
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
    summary: 'Stop the active demo recording session.',
    whenToUse: ['An in-progress demo recording must be stopped.'],
    whenNotToUse: ['No recording is in progress.'],
    inputProps: {},
    required: [],
    effect: 'write',
    costLatency: 'interactive',
    exampleInput: { action: 'stop_recording' },
  }),
];
