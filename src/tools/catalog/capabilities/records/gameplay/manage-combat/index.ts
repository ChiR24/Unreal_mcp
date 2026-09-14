import type { CapabilityRecordSource } from '../../../index.js';
import { COMBAT_1 } from './combat-1.data.js';
import { COMBAT_2 } from './combat-2.data.js';
import { COMBAT_3 } from './combat-3.data.js';
import { COMBAT_4 } from './combat-4.data.js';
import { COMBAT_5 } from './combat-5.data.js';
import { applyFolds } from '../../shared/fold.js';
import { MANAGE_COMBAT_FOLDS } from '../../folds/manage-combat.folds.js';

/** The authored records before folding; per-action contract tests pin these. */
export const MANAGE_COMBAT_UNFOLDED_SOURCES: readonly CapabilityRecordSource[] = [
  ...COMBAT_1,
  ...COMBAT_2,
  ...COMBAT_3,
  ...COMBAT_4,
  ...COMBAT_5,
];

export const MANAGE_COMBAT_SOURCES: readonly CapabilityRecordSource[] = applyFolds(MANAGE_COMBAT_UNFOLDED_SOURCES, MANAGE_COMBAT_FOLDS, 'manage_combat');
