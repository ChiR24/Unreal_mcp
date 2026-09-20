import type { CapabilityRecordSource } from '../../../index.js';
import { CHARACTER_1 } from './character-1.data.js';
import { CHARACTER_2 } from './character-2.data.js';
import { CHARACTER_3 } from './character-3.data.js';
import { applyFolds } from '../../shared/fold.js';
import { MANAGE_CHARACTER_FOLDS } from '../../folds/manage-character.folds.js';

/** The authored records before folding; per-action contract tests pin these. */
export const MANAGE_CHARACTER_UNFOLDED_SOURCES: readonly CapabilityRecordSource[] = [
  ...CHARACTER_1,
  ...CHARACTER_2,
  ...CHARACTER_3,
];

export const MANAGE_CHARACTER_SOURCES: readonly CapabilityRecordSource[] = applyFolds(MANAGE_CHARACTER_UNFOLDED_SOURCES, MANAGE_CHARACTER_FOLDS, 'manage_character');
