import type { CapabilityRecordSource } from '../../../index.js';
import { EFFECT_1 } from './effect-1.data.js';
import { EFFECT_2 } from './effect-2.data.js';
import { EFFECT_3 } from './effect-3.data.js';
import { applyFolds } from '../../shared/fold.js';
import { MANAGE_EFFECT_FOLDS } from '../../folds/manage-effect.folds.js';

/** The authored records before folding; per-action contract tests pin these. */
export const MANAGE_EFFECT_UNFOLDED_SOURCES: readonly CapabilityRecordSource[] = [
  ...EFFECT_1,
  ...EFFECT_2,
  ...EFFECT_3,
];

export const MANAGE_EFFECT_SOURCES: readonly CapabilityRecordSource[] = applyFolds(MANAGE_EFFECT_UNFOLDED_SOURCES, MANAGE_EFFECT_FOLDS, 'manage_effect');
