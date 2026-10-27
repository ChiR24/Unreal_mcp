/**
 * Undo/redo history records: undo, redo, restore_point.
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
  buildCoreRecord({
    parentTool: 'control_editor', action: 'restore_point', domain: D, family: F,
    topics: ['restore point', 'roll back my changes', 'undo everything since', 'savepoint', 'try and revert'],
    summary: 'Mark a restore point to roll the editor back to, undoing only what calls through this server changed, chosen by op. begin opens it (one at a time; label names it); status lists the editor\'s transactions since it, newest first, each with byMcp, and the assets created, removed or saved since (assetsCreated, assetsRemoved, packagesSaved); rollback undoes every step since it and refuses before undoing anything when a step was not made through this server (USER_STEP_SINCE_RESTORE_POINT names it) or the editor\'s history was cleared since (UNDO_HISTORY_CLEARED: a Blueprint compile or a level load clears it); end closes it. Undo deletes no created asset and restores no saved file: status and rollback list them, and cleanupCall is the manage_asset bulk_delete of the assets calls through this server created that are still there, ready to send (it asks for consent); an asset made by hand in the editor is listed but never in it. A rollback is refused while Play runs (PIE_RUNNING).',
    whenToUse: ['A multi-step change may need throwing away: begin first, rollback if the result is wrong.', 'Before trying an approach that touches many actors or assets.'],
    whenNotToUse: ['Only the last step must be reversed (use undo).'],
    inputProps: {
      op: { type: 'string', enum: ['begin', 'status', 'rollback', 'end'], description: 'begin opens the restore point, status lists what changed since it (the default), rollback undoes it, end closes it.' },
      label: { type: 'string', description: 'Name for the restore point (begin); default restore point.' },
    },
    required: [],
    effect: 'write',
    exampleInput: { action: 'restore_point', op: 'begin', label: 'before lighting pass' },
  }),
];
