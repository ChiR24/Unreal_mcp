/**
 * Timing records: set_game_speed, set_fixed_delta_time, step_frame,
 * single_frame_step.
 *
 * Grounded in src/tools/handlers/editor/editor-session-actions.ts.
 * step_frame loops N times; single_frame_step is an alias dispatching to
 * step_frame. set_game_speed and set_fixed_delta_time are idempotent setters.
 */
import type { CapabilityRecordSource } from '../../index.js';
import { buildCoreRecord } from '../core/builder.js';
import { P } from './properties.js';

const F = 'timing';
const D = 'editor';
const NR = 'Distinct control_editor timing operation with unique frame or delta semantics.';

export const TIMING_RECORDS: readonly CapabilityRecordSource[] = [
  buildCoreRecord({
    parentTool: 'control_editor', action: 'set_game_speed', domain: D, family: F,
    summary: 'Set the PIE game speed multiplier.',
    whenToUse: ['Game speed must be changed for testing.'],
    whenNotToUse: ['PIE is not running.'],
    inputProps: { speed: P.speed },
    required: ['speed'],
    effect: 'write', behavior: { idempotency: 'idempotent' },
    costLatency: 'instant', costResources: 'low',
    exampleInput: { action: 'set_game_speed', speed: 0.5 },
    exampleOutput: { success: true, message: 'Game speed set to 0.5' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'set_fixed_delta_time', domain: D, family: F,
    summary: 'Set the fixed delta time for PIE stepping.',
    whenToUse: ['A fixed delta time is needed for deterministic stepping.'],
    whenNotToUse: ['Variable delta time is preferred.'],
    inputProps: { deltaTime: P.deltaTime },
    required: ['deltaTime'],
    effect: 'write', behavior: { idempotency: 'idempotent' },
    costLatency: 'instant', costResources: 'low',
    exampleInput: { action: 'set_fixed_delta_time', deltaTime: 0.01667 },
    exampleOutput: { success: true, message: 'Fixed delta time set' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'step_frame', domain: D, family: F,
    summary: 'Advance the PIE simulation by one or more frames.',
    whenToUse: ['The simulation must advance a fixed number of frames.'],
    whenNotToUse: ['Real-time simulation is preferred.'],
    inputProps: { steps: P.steps },
    required: [],
    effect: 'write',
    costLatency: 'instant', costResources: 'low',
    exampleInput: { action: 'step_frame', steps: 1 },
    exampleOutput: { success: true, message: 'Stepped 1 frame(s)', steps: 1 },
    outputProps: { steps: P.steps },
    outputRequired: ['steps'],
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'single_frame_step', dispatchAction: 'step_frame',
    domain: D, family: F,
    summary: 'Advance the PIE simulation by one frame (alias for step_frame).',
    whenToUse: ['The simulation must advance exactly one frame.'],
    whenNotToUse: ['Multiple frames are needed (use step_frame with steps).'],
    inputProps: { steps: P.steps },
    required: [],
    effect: 'write',
    costLatency: 'instant', costResources: 'low',
    exampleInput: { action: 'single_frame_step', steps: 1 },
    exampleOutput: { success: true, message: 'Stepped 1 frame(s)', steps: 1 },
    outputProps: { steps: P.steps },
    outputRequired: ['steps'],
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET',
    normalizationRationale: 'TS normalizes single_frame_step to step_frame for handler routing; bridge dispatches step_frame.',
  }),
];
