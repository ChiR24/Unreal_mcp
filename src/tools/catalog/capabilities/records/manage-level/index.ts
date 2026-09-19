/**
 * manage_level capability record catalog.
 *
 * 25 authored CapabilityRecordSource entries covering the 25 manage_level
 * actions in manage-level-tool.ts, folded by MANAGE_LEVEL_FOLDS into the 17
 * shipped records. Each record is grounded in
 * the TypeScript level handlers (src/tools/handlers/level/runtime/), native
 * Level domain dispatch (Private/Domains/Level/McpAutomationBridge_LevelHandlers.cpp),
 * and the normalization inventory (25 manage_level occurrences, all
 * classification C, disposition keep, no aliases).
 *
 * Families:
 * - lifecycle (11): load, load_level, save, save_level, save_as,
 *   save_level_as, create_level, delete, delete_level, rename_level,
 *   duplicate_level
 * - streaming (3): stream, unload, unload_level
 * - lighting (2): create_light, build_lighting
 * - metadata (1): set_metadata
 * - query (4): list_levels, get_current_level, get_summary, validate_level
 * - io (2): export_level, import_level
 * - sublevel (1): add_sublevel
 * - settings (1): set_world_settings
 *
 * Total: 11 + 3 + 2 + 1 + 4 + 2 + 1 + 1 = 25
 */
import { type CapabilityRecord, type CapabilityRecordSource, createCapabilityRecord } from '../../index.js';

import { LIFECYCLE_RECORDS } from './lifecycle.data.js';
import { OPERATIONS_RECORDS } from './operations.data.js';
import { applyFolds } from '../shared/fold.js';
import { MANAGE_LEVEL_FOLDS } from '../folds/manage-level.folds.js';

/**
 * Record order is the authored data-file concatenation; this module does not
 * re-derive an action order.
 */
/** The authored records before folding; per-action contract tests pin these. */
export const MANAGE_LEVEL_UNFOLDED_SOURCES: readonly CapabilityRecordSource[] = [...LIFECYCLE_RECORDS, ...OPERATIONS_RECORDS];

const SOURCES: readonly CapabilityRecordSource[] = applyFolds(MANAGE_LEVEL_UNFOLDED_SOURCES, MANAGE_LEVEL_FOLDS, 'manage_level');

export const MANAGE_LEVEL_SOURCES: readonly CapabilityRecordSource[] = SOURCES;

export const MANAGE_LEVEL_RECORDS: readonly CapabilityRecord[] = SOURCES.map(
  (source) => createCapabilityRecord(source),
);

export const MANAGE_LEVEL_RECORD_COUNT = MANAGE_LEVEL_RECORDS.length;
