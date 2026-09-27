/**
 * Undo/redo history records: undo, redo.
 */
import type { CapabilityRecordSource } from '../../model.js';
import { buildCoreRecord } from '../core/builder.js';

const F = 'history';
const D = 'editor';

export const HISTORY_RECORDS: readonly CapabilityRecordSource[] = [
  buildCoreRecord({
    parentTool: 'control_editor', action: 'undo', domain: D, family: F,
    topics: ['undo last change', 'revert last action', 'ctrl z'],
    summary: 'Undo the last editor transaction; the reply names it, and NOTHING_TO_UNDO means there was none.',
    whenToUse: ['The most recent editor action must be reversed.'],
    whenNotToUse: ['There is nothing to undo.'],
    inputProps: {},
    required: [],
    effect: 'write',
   
    exampleInput: { action: 'undo' },
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'redo', domain: D, family: F,
    summary: 'Redo the last undone editor transaction; the reply names it, and NOTHING_TO_REDO means there was none.',
    whenToUse: ['A previously undone action must be re-applied.'],
    whenNotToUse: ['There is nothing to redo.'],
    inputProps: {},
    required: [],
    effect: 'write',
   
    exampleInput: { action: 'redo' },
  }),
];
