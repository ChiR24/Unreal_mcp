/**
 * Undo/redo history records: undo, redo.
 *
 * Grounded in src/tools/handlers/editor/editor-viewport-actions.ts.
 * Both are write-effect editor history operations (non-idempotent: each call
 * advances the undo/redo stack to a different state).
 */
import type { CapabilityRecordSource } from '../../index.js';
import { buildCoreRecord } from '../core/builder.js';

const F = 'history';
const D = 'editor';
const NR = 'Distinct control_editor history operation with unique undo/redo semantics.';

export const HISTORY_RECORDS: readonly CapabilityRecordSource[] = [
  buildCoreRecord({
    parentTool: 'control_editor', action: 'undo', domain: D, family: F,
    summary: 'Undo the last editor action.',
    whenToUse: ['The most recent editor action must be reversed.'],
    whenNotToUse: ['There is nothing to undo.'],
    inputProps: {},
    required: [],
    effect: 'write',
    costLatency: 'instant', costResources: 'low',
    exampleInput: { action: 'undo' },
    exampleOutput: { success: true, message: 'Undo performed' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'redo', domain: D, family: F,
    summary: 'Redo the last undone editor action.',
    whenToUse: ['A previously undone action must be re-applied.'],
    whenNotToUse: ['There is nothing to redo.'],
    inputProps: {},
    required: [],
    effect: 'write',
    costLatency: 'instant', costResources: 'low',
    exampleInput: { action: 'redo' },
    exampleOutput: { success: true, message: 'Redo performed' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
];
