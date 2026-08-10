/**
 * Bookmark records: create_bookmark, jump_to_bookmark.
 *
 * Grounded in src/tools/handlers/editor/editor-viewport-actions.ts.
 * Bookmarks are identified by index (derived from id or bookmarkName).
 * create_bookmark is write-effect (creates a new viewport bookmark);
 * jump_to_bookmark is read-effect (navigates to an existing bookmark).
 */
import type { CapabilityRecordSource } from '../../index.js';
import { buildCoreRecord } from '../core/builder.js';
import { P } from './properties.js';

const F = 'bookmark';
const D = 'editor';
const NR = 'Distinct control_editor bookmark operation with unique viewport navigation semantics.';

export const BOOKMARK_RECORDS: readonly CapabilityRecordSource[] = [
  buildCoreRecord({
    parentTool: 'control_editor', action: 'create_bookmark', domain: D, family: F,
    summary: 'Create a viewport bookmark at the current camera position.',
    whenToUse: ['The current viewport camera state must be saved for later recall.'],
    whenNotToUse: ['A bookmark already exists at the desired index.'],
    inputProps: { id: P.id, description: P.description, bookmarkName: P.bookmarkName },
    required: [],
    effect: 'write',
    costLatency: 'instant', costResources: 'low',
    exampleInput: { action: 'create_bookmark', bookmarkName: 'Overview' },
    exampleOutput: { success: true, message: 'Bookmark created' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'jump_to_bookmark', domain: D, family: F,
    summary: 'Jump the viewport camera to a previously created bookmark.',
    whenToUse: ['The viewport must navigate to a saved bookmark position.'],
    whenNotToUse: ['The bookmark does not exist.'],
    inputProps: { id: P.id, bookmarkName: P.bookmarkName },
    required: [],
    effect: 'read',
    costLatency: 'instant', costResources: 'low',
    exampleInput: { action: 'jump_to_bookmark', bookmarkName: 'Overview' },
    exampleOutput: { success: true, message: 'Jumped to bookmark' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
];
