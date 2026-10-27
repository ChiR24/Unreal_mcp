import type { CapabilityRecordSource } from '../../../model.js';
import { INTERACTION_RECORDS } from './interaction.data.js';
import { applyFolds } from '../../shared/fold.js';
import { MANAGE_INTERACTION_FOLDS } from '../../folds/manage-interaction.folds.js';

/** The authored records before folding; per-action contract tests pin these. */
export const MANAGE_INTERACTION_UNFOLDED_SOURCES: readonly CapabilityRecordSource[] = [
  ...INTERACTION_RECORDS,];

export const MANAGE_INTERACTION_SOURCES: readonly CapabilityRecordSource[] = applyFolds(MANAGE_INTERACTION_UNFOLDED_SOURCES, MANAGE_INTERACTION_FOLDS, 'manage_interaction');
