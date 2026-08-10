/**
 * Demo recording records: start_recording, stop_recording.
 *
 * Grounded in src/tools/handlers/editor/editor-session-actions.ts.
 * start_recording dispatches to control_editor with a console_command
 * DemoRec fallback if the bridge call fails. stop_recording stops the
 * active demo recording.
 */
import type { CapabilityRecordSource } from '../../index.js';
import { buildCoreRecord } from '../core/builder.js';
import { P } from './properties.js';

const F = 'recording';
const D = 'editor';
const NR = 'Distinct control_editor demo recording operation with unique lifecycle semantics.';

export const RECORDING_RECORDS: readonly CapabilityRecordSource[] = [
  buildCoreRecord({
    parentTool: 'control_editor', action: 'start_recording', domain: D, family: F,
    summary: 'Start a demo recording session. Falls back to DemoRec console command if bridge call fails.',
    whenToUse: ['A demo recording must be started for replay capture.'],
    whenNotToUse: ['A recording is already in progress.'],
    inputProps: { filename: P.filename, name: P.name, frameRate: P.frameRate, durationSeconds: P.durationSeconds, metadata: P.metadata },
    required: [],
    effect: 'write',
    costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'start_recording', filename: 'TestRecording' },
    exampleOutput: { success: true, message: 'Started recording to TestRecording', filename: 'TestRecording' },
    outputProps: { filename: P.filename },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET',
    normalizationRationale: 'Primary dispatch to control_editor; fallback cross-parent to console_command DemoRec if bridge call fails.',
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'stop_recording', domain: D, family: F,
    summary: 'Stop the active demo recording session.',
    whenToUse: ['An in-progress demo recording must be stopped.'],
    whenNotToUse: ['No recording is in progress.'],
    inputProps: {},
    required: [],
    effect: 'write',
    costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'stop_recording' },
    exampleOutput: { success: true, message: 'Recording stopped' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
];
