/**
 * manage_level_structure capability record catalog: the structural actions,
 * then the volume records (manage-level-structure.volume.data.ts).
 */
import type { CapabilityRecordSource } from '../../model.js';

import { LEVEL_STRUCTURE_RECORDS } from './manage-level-structure.structure.data.js';
import { LEVEL_VOLUME_RECORDS } from './manage-level-structure.volume.data.js';
import { applyFolds } from '../shared/fold.js';
import { MANAGE_LEVEL_STRUCTURE_FOLDS } from '../folds/manage-level-structure.folds.js';

// Records are emitted in the exact legacy manage_level_structure action-enum
// order. The data shards below are authored in definition order (structural
// actions, then the volume A/B shards), so concatenating them preserves that
// order verbatim. Keep that historical order: the generated registry sorts by
// id, so re-sorting here only reshuffles generated output.
/** The authored records before folding; per-action contract tests pin these. */
export const MANAGE_LEVEL_STRUCTURE_UNFOLDED_SOURCES: readonly CapabilityRecordSource[] = [
  ...LEVEL_STRUCTURE_RECORDS,
  ...LEVEL_VOLUME_RECORDS,];

export const MANAGE_LEVEL_STRUCTURE_SOURCES: readonly CapabilityRecordSource[] = applyFolds(MANAGE_LEVEL_STRUCTURE_UNFOLDED_SOURCES, MANAGE_LEVEL_STRUCTURE_FOLDS, 'manage_level_structure');

