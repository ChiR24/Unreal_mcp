/**
 * manage_level_structure capability record catalog.
 *
 * Exactly 45 canonical CapabilityRecordSource entries: 17 structural actions
 * plus 28 volume actions (split across the volume-a / volume-b shards, 13 + 15),
 * mapped 1:1 to the manage_level_structure action enum (non-volume actions
 * first, then VOLUME_ACTIONS in definition order). Each record is grounded in
 * the world tool definition, the native LevelStructure domain dispatch, and the
 * normalization inventory (all 45 occurrences are classification C,
 * disposition retain, no aliases).
 */
import type { CapabilityRecordSource } from '../../index.js';

import { LEVEL_STRUCTURE_RECORDS } from './manage-level-structure.structure.data.js';
import { LEVEL_VOLUME_A_RECORDS } from './manage-level-structure.volume-a.data.js';
import { LEVEL_VOLUME_B_RECORDS } from './manage-level-structure.volume-b.data.js';

// Records are emitted in the exact legacy manage_level_structure action-enum
// order. The data shards below are authored in definition order (structural
// actions, then the volume A/B shards), so concatenating them preserves that
// order verbatim. Do NOT re-sort: the record order is a contractual parity
// assertion against consolidatedToolDefinitions (see
// tests/unit/world-capability-records.test.ts), not a free-standing ordering.
export const MANAGE_LEVEL_STRUCTURE_SOURCES: readonly CapabilityRecordSource[] = [
  ...LEVEL_STRUCTURE_RECORDS,
  ...LEVEL_VOLUME_A_RECORDS,
  ...LEVEL_VOLUME_B_RECORDS,
];

export const MANAGE_LEVEL_STRUCTURE_RECORDS: readonly CapabilityRecordSource[] = MANAGE_LEVEL_STRUCTURE_SOURCES;

export const MANAGE_LEVEL_STRUCTURE_RECORD_COUNT = MANAGE_LEVEL_STRUCTURE_RECORDS.length;
