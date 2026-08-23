/**
 * manage_level capability record catalog.
 *
 * Exactly 25 canonical CapabilityRecord entries mapped 1:1 to the 25
 * manage_level actions in manage-level-tool.ts. Each record is grounded in
 * the TypeScript level handlers (src/tools/handlers/level/runtime/), native
 * Level domain dispatch (Private/Domains/Level/McpAutomationBridge_LevelHandlers.cpp),
 * and the normalization inventory (24 manage_level occurrences, all
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

/**
 * Record order is the authored data-file concatenation; this module does not
 * re-derive an action order.
 */
const SOURCES: readonly CapabilityRecordSource[] = [...LIFECYCLE_RECORDS, ...OPERATIONS_RECORDS];

export const MANAGE_LEVEL_SOURCES: readonly CapabilityRecordSource[] = SOURCES;

export const MANAGE_LEVEL_RECORDS: readonly CapabilityRecord[] = SOURCES.map(
  (source) => createCapabilityRecord(source),
);

export const MANAGE_LEVEL_RECORD_COUNT = MANAGE_LEVEL_RECORDS.length;
