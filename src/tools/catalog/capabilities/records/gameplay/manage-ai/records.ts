/**
 * manage_ai capability record catalog: 65 authored CapabilityRecordSource
 * entries (add 16, configure 17, create/read 17, set 15), folded by
 * MANAGE_AI_FOLDS into MANAGE_AI_SOURCES. The authored count is pinned by
 * manage-ai-records.test.ts.
 */
import type { CapabilityRecordSource } from '../../../model.js';

import { AI_ADD_RECORDS } from './add-actions.data.js';
import { AI_CONFIGURE_RECORDS } from './configure-actions.data.js';
import { AI_CREATE_READ_RECORDS } from './create-read-actions.data.js';
import { AI_SET_RECORDS } from './set-actions.data.js';
import { applyFolds } from '../../shared/fold.js';
import { MANAGE_AI_FOLDS } from '../../folds/manage-ai.folds.js';

/** The authored records before folding; per-action contract tests pin these. */
export const MANAGE_AI_UNFOLDED_SOURCES: readonly CapabilityRecordSource[] = [
  ...AI_ADD_RECORDS,
  ...AI_CONFIGURE_RECORDS,
  ...AI_CREATE_READ_RECORDS,
  ...AI_SET_RECORDS,
];

export const MANAGE_AI_SOURCES: readonly CapabilityRecordSource[] = applyFolds(MANAGE_AI_UNFOLDED_SOURCES, MANAGE_AI_FOLDS, 'manage_ai');
