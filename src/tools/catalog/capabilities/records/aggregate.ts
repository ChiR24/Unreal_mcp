// src/tools/catalog/capabilities/records/aggregate.ts
//
// In-src aggregator of every canonical capability record, in AUTHORED order.
//
// Composes the per-category SOURCE arrays (world / gameplay / utility / core)
// rather than their id-sorted `*_CAPABILITY_CATALOG` projections, so each
// parent keeps the record sequence its record directory declares. The
// canonical-registry generator derives each parent's action enum from that
// first-seen sequence, so an id-sorted feed alphabetises the generated enums.
//
// `scripts/qa/capability-metadata-audit.ts#loadAllCapabilityRecords` re-exports
// this composition, so the generator and the audit share one ordering.

import { WORLD_SOURCE_RECORDS } from './world/index.js';
import { GAMEPLAY_SOURCE_RECORDS } from './gameplay/index.js';
import { UTILITY_SOURCE_RECORDS } from './utility/index.js';
import { CORE_CAPABILITY_SOURCE_RECORDS } from '../retrieval/aggregate.js';
import { createCapabilityRecord, parseCapabilityCatalog } from '../parser.js';
import type { CapabilityRecord } from '../model.js';

export const ALL_CAPABILITY_RECORD_COUNT = 1401 as const;

export const ALL_CAPABILITY_RECORDS: readonly CapabilityRecord[] = (() => {
  const parsed = parseCapabilityCatalog([
    ...WORLD_SOURCE_RECORDS.map(createCapabilityRecord),
    ...GAMEPLAY_SOURCE_RECORDS.map(createCapabilityRecord),
    ...UTILITY_SOURCE_RECORDS,
    ...CORE_CAPABILITY_SOURCE_RECORDS,
  ]);
  const uniqueIdCount = new Set(parsed.map((record) => record.id)).size;
  if (
    parsed.length !== ALL_CAPABILITY_RECORD_COUNT
    || uniqueIdCount !== ALL_CAPABILITY_RECORD_COUNT
  ) {
    throw new TypeError(
      `Capability record aggregate must contain exactly ${ALL_CAPABILITY_RECORD_COUNT} `
      + `records and unique IDs; received ${parsed.length} records and ${uniqueIdCount} unique IDs`,
    );
  }
  return Object.freeze(parsed);
})();

export const loadAllCapabilityRecordsInSrc = (): readonly CapabilityRecord[] => ALL_CAPABILITY_RECORDS;
