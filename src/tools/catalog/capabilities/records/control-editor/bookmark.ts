/**
 * Bookmark records: create_bookmark, jump_to_bookmark.
 */
import type { CapabilityRecordSource } from '../../model.js';
import { buildCoreRecord } from '../core/builder.js';
import { P } from './properties.js';

const F = 'bookmark';
const D = 'editor';

export const BOOKMARK_RECORDS: readonly CapabilityRecordSource[] = [
  buildCoreRecord({
    parentTool: 'control_editor', action: 'create_bookmark', topics: ['bookmark camera view'], domain: D, family: F,
    summary: 'Store the level viewport camera in a numbered bookmark slot (id).',
    whenToUse: ['The current viewport camera state must be saved for later recall.'],
    whenNotToUse: ['A bookmark already exists at the desired index.'],
    inputProps: { id: P.id },
    required: ['id'],
    effect: 'write',
   
    exampleInput: { action: 'create_bookmark', id: 1 },
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'jump_to_bookmark', topics: ['go to bookmark'], domain: D, family: F,
    summary: 'Move the level viewport camera to a bookmark slot (id); fails with BOOKMARK_NOT_FOUND when that slot is empty.',
    whenToUse: ['The viewport must navigate to a saved bookmark position.'],
    whenNotToUse: ['The bookmark does not exist.'],
    inputProps: { id: P.id },
    required: ['id'],
    effect: 'read',
   
    exampleInput: { action: 'jump_to_bookmark', id: 1 },
  }),
];
