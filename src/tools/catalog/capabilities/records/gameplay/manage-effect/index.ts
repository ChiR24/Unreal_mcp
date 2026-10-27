import type { CapabilityRecordSource } from '../../../model.js';
import { EFFECT_RECORDS } from './effect.data.js';
import { applyFolds } from '../../shared/fold.js';
import { MANAGE_EFFECT_FOLDS } from '../../folds/manage-effect.folds.js';

/** The authored records before folding; per-action contract tests pin these. */
export const MANAGE_EFFECT_UNFOLDED_SOURCES: readonly CapabilityRecordSource[] = [
  ...EFFECT_RECORDS,];

export const MANAGE_EFFECT_SOURCES: readonly CapabilityRecordSource[] = applyFolds(MANAGE_EFFECT_UNFOLDED_SOURCES, MANAGE_EFFECT_FOLDS, 'manage_effect');
