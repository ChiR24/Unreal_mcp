/**
 * manage_level capability record catalog.
 */
import { type CapabilityRecordSource } from '../../model.js';

import { LIFECYCLE_RECORDS } from './lifecycle.data.js';
import { OPERATIONS_RECORDS } from './operations.data.js';
import { applyFolds } from '../shared/fold.js';
import { MANAGE_LEVEL_FOLDS } from '../folds/manage-level.folds.js';

/**
 * Record order is the authored data-file concatenation; this module does not
 * re-derive an action order.
 */
/** The authored records before folding; per-action contract tests pin these. */
export const MANAGE_LEVEL_UNFOLDED_SOURCES: readonly CapabilityRecordSource[] = [...LIFECYCLE_RECORDS, ...OPERATIONS_RECORDS];

export const MANAGE_LEVEL_SOURCES: readonly CapabilityRecordSource[] = applyFolds(MANAGE_LEVEL_UNFOLDED_SOURCES, MANAGE_LEVEL_FOLDS, 'manage_level');

