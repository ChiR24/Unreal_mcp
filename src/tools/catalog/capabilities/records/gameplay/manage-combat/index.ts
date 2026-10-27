import type { CapabilityRecordSource } from '../../../model.js';
import { COMBAT_RECORDS } from './combat.data.js';
import { applyFolds } from '../../shared/fold.js';
import { MANAGE_COMBAT_FOLDS } from '../../folds/manage-combat.folds.js';

/** The authored records before folding; per-action contract tests pin these. */
export const MANAGE_COMBAT_UNFOLDED_SOURCES: readonly CapabilityRecordSource[] = [
  ...COMBAT_RECORDS,];

export const MANAGE_COMBAT_SOURCES: readonly CapabilityRecordSource[] = applyFolds(MANAGE_COMBAT_UNFOLDED_SOURCES, MANAGE_COMBAT_FOLDS, 'manage_combat');
