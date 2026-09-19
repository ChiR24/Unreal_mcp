/**
 * World capability aggregate (frozen, fail-closed, deterministic).
 *
 * Combines the reused build_environment records (referenced, never
 * regenerated) with the net-new world records into a single frozen catalog,
 * sorted by canonical ID and frozen with Object.freeze so accidental mutation
 * fails closed.
 *
 * Counts are POST-FOLD and are the three constants below, each pinned by
 * tests/unit/world-capability-records.test.ts:
 *   build_environment       150 authored -> 40 folded  (WORLD_REUSED_BUILD_ENVIRONMENT_COUNT)
 *   manage_level_structure   24 authored ->  8 folded
 *   manage_geometry          86 authored -> 15 folded
 *   manage_pcg               30 authored ->  3 folded
 *   net-new subtotal        140 authored -> 26 folded  (WORLD_NET_NEW_COUNT)
 *   aggregate               290 authored -> 66 folded  (WORLD_AGGREGATE_COUNT)
 *
 * Ownership: this file and the four record directories it imports are the only
 * world-aggregate surface. It reuses build_environment data by import only.
 */
import type { CapabilityCatalog, CapabilityRecord, CapabilityRecordSource } from '../../index.js';
import { createCapabilityRecord, parseCapabilityCatalog } from '../../parser.js';
import { BUILD_ENVIRONMENT_RECORDS } from '../build-environment/index.js';
import { MANAGE_LEVEL_STRUCTURE_RECORDS } from './manage-level-structure.index.js';
import { MANAGE_GEOMETRY_RECORDS } from './manage-geometry.index.js';
import { MANAGE_PCG_RECORDS } from './manage-pcg.index.js';
import { compareById as compareCanonicalIds } from '../../../../../utils/serialization/ordering.js';

export const WORLD_NET_NEW_COUNT = 26 as const;
export const WORLD_REUSED_BUILD_ENVIRONMENT_COUNT = 40 as const;
export const WORLD_AGGREGATE_COUNT = 66 as const;

/**
 * Raw source records (build_environment is reused by object identity; the rest
 * are the net-new world set). Exposed for verification that the
 * build_environment objects are not re-created.
 */
export const WORLD_SOURCE_RECORDS: readonly CapabilityRecordSource[] = Object.freeze([
  ...BUILD_ENVIRONMENT_RECORDS,
  ...MANAGE_LEVEL_STRUCTURE_RECORDS,
  ...MANAGE_GEOMETRY_RECORDS,
  ...MANAGE_PCG_RECORDS,
]);

function deepFreezeCapability(value: unknown): void {
  if (value === null || value === undefined || typeof value !== 'object') return;
  if (Object.isFrozen(value)) return;
  Object.freeze(value);
  const obj = value as Record<string, unknown>;
  if (Array.isArray(value)) {
    for (const item of value) deepFreezeCapability(item);
  } else {
    for (const key of Object.keys(obj)) deepFreezeCapability(obj[key]);
  }
}

const WORLD_BUILT_RECORDS: readonly CapabilityRecord[] = (() => {
  const environmentRecords = BUILD_ENVIRONMENT_RECORDS.map((source) => createCapabilityRecord(source));
  const newRecords = [
    ...MANAGE_LEVEL_STRUCTURE_RECORDS,
    ...MANAGE_GEOMETRY_RECORDS,
    ...MANAGE_PCG_RECORDS,
  ].map((source) => createCapabilityRecord(source));
  const parsed = parseCapabilityCatalog([...environmentRecords, ...newRecords]);
  const uniqueIdCount = new Set(parsed.map((record) => record.id)).size;
  if (
    parsed.length !== WORLD_AGGREGATE_COUNT
    || uniqueIdCount !== WORLD_AGGREGATE_COUNT
  ) {
    throw new Error(
      `World capability aggregate must contain exactly ${WORLD_AGGREGATE_COUNT} `
      + `records and unique IDs; received ${parsed.length} records and ${uniqueIdCount} unique IDs`,
    );
  }
  const frozen = [...parsed].sort(compareCanonicalIds);
  for (const record of frozen) deepFreezeCapability(record);
  return Object.freeze(frozen);
})();

export const WORLD_CAPABILITY_CATALOG: CapabilityCatalog = WORLD_BUILT_RECORDS;


export const WORLD_CAPABILITY_RECORD_COUNT = WORLD_BUILT_RECORDS.length;
