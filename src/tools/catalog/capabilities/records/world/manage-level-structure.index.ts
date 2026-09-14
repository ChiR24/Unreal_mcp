/**
 * manage_level_structure capability record catalog.
 *
 * Exactly 24 canonical CapabilityRecordSource entries: 18 structural actions
 * plus 6 volume records (volume-a holds the single `create_volume` operation
 * that folds the 23 former create_* and add_* names; volume-b holds the 5
 * management actions), mapped 1:1 to the advertised manage_level_structure
 * action enum (non-volume actions first, then the volume records in definition
 * order). The 23 folded names stay callable as legacy pairs and are counted by
 * the normalization audit; only `create_volume` itself is post-migration.
 */
import type { CapabilityRecordSource } from '../../index.js';

import { LEVEL_STRUCTURE_RECORDS } from './manage-level-structure.structure.data.js';
import { LEVEL_VOLUME_A_RECORDS } from './manage-level-structure.volume-a.data.js';
import { LEVEL_VOLUME_B_RECORDS } from './manage-level-structure.volume-b.data.js';
import { applyFolds } from '../shared/fold.js';
import { MANAGE_LEVEL_STRUCTURE_FOLDS } from '../folds/manage-level-structure.folds.js';

// Records are emitted in the exact legacy manage_level_structure action-enum
// order. The data shards below are authored in definition order (structural
// actions, then the volume A/B shards), so concatenating them preserves that
// order verbatim. Do NOT re-sort: the record order is a contractual parity
// assertion against consolidatedToolDefinitions (see
// tests/unit/world-capability-records.test.ts), not a free-standing ordering.
/** The authored records before folding; per-action contract tests pin these. */
export const MANAGE_LEVEL_STRUCTURE_UNFOLDED_SOURCES: readonly CapabilityRecordSource[] = [
  ...LEVEL_STRUCTURE_RECORDS,
  ...LEVEL_VOLUME_A_RECORDS,
  ...LEVEL_VOLUME_B_RECORDS,
];

export const MANAGE_LEVEL_STRUCTURE_SOURCES: readonly CapabilityRecordSource[] = applyFolds(MANAGE_LEVEL_STRUCTURE_UNFOLDED_SOURCES, MANAGE_LEVEL_STRUCTURE_FOLDS, 'manage_level_structure');

export const MANAGE_LEVEL_STRUCTURE_RECORDS: readonly CapabilityRecordSource[] = MANAGE_LEVEL_STRUCTURE_SOURCES;

export const MANAGE_LEVEL_STRUCTURE_RECORD_COUNT = MANAGE_LEVEL_STRUCTURE_RECORDS.length;
