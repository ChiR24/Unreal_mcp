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
    parentTool: 'control_editor', action: 'create_bookmark', domain: D, family: F,
    summary: 'Create a viewport bookmark at the current camera position.',
    whenToUse: ['The current viewport camera state must be saved for later recall.'],
    whenNotToUse: ['A bookmark already exists at the desired index.'],
    inputProps: { id: P.id, description: P.description, bookmarkName: P.bookmarkName },
    required: [],
    effect: 'write',
   
    exampleInput: { action: 'create_bookmark', bookmarkName: 'Overview' },
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'jump_to_bookmark', domain: D, family: F,
    summary: 'Jump the viewport camera to a previously created bookmark.',
    whenToUse: ['The viewport must navigate to a saved bookmark position.'],
    whenNotToUse: ['The bookmark does not exist.'],
    inputProps: { id: P.id, bookmarkName: P.bookmarkName },
    required: [],
    effect: 'read',
   
    exampleInput: { action: 'jump_to_bookmark', bookmarkName: 'Overview' },
  }),
];
