import type { CapabilityRecordSource } from '../../../index.js';
import { INTERACTION_1 } from './interaction-1.data.js';
import { INTERACTION_2 } from './interaction-2.data.js';
import { applyFolds } from '../../shared/fold.js';
import { MANAGE_INTERACTION_FOLDS } from '../../folds/manage-interaction.folds.js';

/** The authored records before folding; per-action contract tests pin these. */
export const MANAGE_INTERACTION_UNFOLDED_SOURCES: readonly CapabilityRecordSource[] = [
  ...INTERACTION_1,
  ...INTERACTION_2,
];

export const MANAGE_INTERACTION_SOURCES: readonly CapabilityRecordSource[] = applyFolds(MANAGE_INTERACTION_UNFOLDED_SOURCES, MANAGE_INTERACTION_FOLDS, 'manage_interaction');
