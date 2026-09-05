/**
 * PIE session lifecycle records: play, stop, stop_pie, pause, resume, eject,
 * possess.
 *
 * Grounded in src/tools/handlers/editor/editor-session-actions.ts.
 * stop/stop_pie both dispatch as 'stop'; pause/resume are idempotent PIE
 * state toggles. play starts PIE (interactive latency, medium resources).
 */
import type { CapabilityRecordSource } from '../../index.js';
import { buildCoreRecord } from '../core/builder.js';
import { P } from './properties.js';

const F = 'session';
const D = 'editor';
const NR = 'Distinct control_editor PIE lifecycle operation with unique session semantics.';

export const SESSION_RECORDS: readonly CapabilityRecordSource[] = [
  buildCoreRecord({
    parentTool: 'control_editor', action: 'play', domain: D, family: F,
    topics: ['play in editor', 'pie', 'start pie', 'start game', 'run game', 'press play', 'simulate'],
    aliases: ['control_editor.start_pie'],
    summary: 'Start Play-In-Editor (PIE) session.',
    whenToUse: ['A PIE session must be started to test gameplay.'],
    whenNotToUse: ['PIE is already running.'],
    inputProps: {},
    required: [],
    effect: 'write',
    costLatency: 'interactive', costResources: 'medium',
    exampleInput: { action: 'play' },
    exampleOutput: { success: true, message: 'PIE started' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'stop', domain: D, family: F,
    topics: ['stop pie', 'stop game', 'end play', 'exit pie'],
    summary: 'Stop the running PIE session.',
    whenToUse: ['The PIE session must be terminated.'],
    whenNotToUse: ['PIE should only be paused.'],
    inputProps: {},
    required: [],
    effect: 'write', behavior: { idempotency: 'idempotent' },
    costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'stop' },
    exampleOutput: { success: true, message: 'PIE stopped' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'stop_pie', dispatchAction: 'stop',
    domain: D, family: F,
    summary: 'Stop the running PIE session (alias for stop).',
    whenToUse: ['The PIE session must be terminated using the stop_pie alias.'],
    whenNotToUse: ['PIE should only be paused.'],
    inputProps: {},
    required: [],
    effect: 'write', behavior: { idempotency: 'idempotent' },
    costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'stop_pie' },
    exampleOutput: { success: true, message: 'PIE stopped' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET',
    normalizationRationale: 'TS normalizes stop_pie to stop for handler routing; bridge dispatches stop.',
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'pause', domain: D, family: F,
    summary: 'Pause the running PIE session.',
    whenToUse: ['PIE must be paused without stopping.'],
    whenNotToUse: ['PIE is not running or already paused.'],
    inputProps: {},
    required: [],
    effect: 'write', behavior: { idempotency: 'idempotent' },
    costLatency: 'instant', costResources: 'low',
    exampleInput: { action: 'pause' },
    exampleOutput: { success: true, message: 'PIE paused' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'resume', domain: D, family: F,
    summary: 'Resume a paused PIE session.',
    whenToUse: ['A paused PIE session must resume.'],
    whenNotToUse: ['PIE is not paused.'],
    inputProps: {},
    required: [],
    effect: 'write', behavior: { idempotency: 'idempotent' },
    costLatency: 'instant', costResources: 'low',
    exampleInput: { action: 'resume' },
    exampleOutput: { success: true, message: 'PIE resumed' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'eject', domain: D, family: F,
    summary: 'Eject from the currently possessed pawn in PIE.',
    whenToUse: ['The player must detach from the possessed pawn.'],
    whenNotToUse: ['PIE is not running or no pawn is possessed.'],
    inputProps: {},
    required: [],
    effect: 'write',
    costLatency: 'instant', costResources: 'low',
    exampleInput: { action: 'eject' },
    exampleOutput: { success: true, message: 'Ejected from pawn' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'possess', domain: D, family: F,
    summary: 'Possess a specific actor by name in PIE.',
    whenToUse: ['The player must take control of a specific actor.'],
    whenNotToUse: ['PIE is not running or the target actor does not exist.'],
    inputProps: { actorName: P.actorName },
    required: ['actorName'],
    effect: 'write',
    costLatency: 'instant', costResources: 'low',
    exampleInput: { action: 'possess', actorName: 'BP_PlayerCharacter' },
    exampleOutput: { success: true, message: 'Possessed BP_PlayerCharacter' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
];
