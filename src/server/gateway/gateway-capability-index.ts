// src/server/gateway/gateway-capability-index.ts
// Lookup structures over the Task 23 generated canonical registry.
//
// The registry is the sole contract source for gateway discovery: the parent
// manifest is a legacy dispatch view and its per-tool union schema is NOT a
// capability contract. Everything here is derived from the record fields
// themselves (`legacyIds`, `aliases`, `discovery`) rather than from the
// separately-namespaced migration/alias tables, so a canonical ID never has to
// be translated between two ID spaces.
//
// The index is built once on first use and only ever read afterwards.

import {
  CANONICAL_CAPABILITY_RECORDS,
  CATALOG_REVISION
} from '../../tools/catalog/capabilities/generated/canonical-registry.generated.js';
import type { CapabilityRecord } from '../../tools/catalog/capabilities/model.js';
import {
  createCapabilitySearchIndex,
  type CapabilitySearchIndex
} from '../../tools/catalog/capabilities/retrieval/scoring.js';
import { compareAscii } from '../../utils/serialization/ordering.js';

export type CapabilityIndex = {
  readonly records: readonly CapabilityRecord[];
  readonly byId: ReadonlyMap<string, CapabilityRecord>;
  readonly byAlias: ReadonlyMap<string, CapabilityRecord>;
  readonly byLegacyPair: ReadonlyMap<string, CapabilityRecord>;
  readonly byParentTool: ReadonlyMap<string, readonly CapabilityRecord[]>;
  readonly domains: readonly string[];
  readonly familiesByDomain: ReadonlyMap<string, readonly string[]>;
  readonly search: CapabilitySearchIndex;
};

export function legacyPairKey(tool: string, action: string): string {
  return `${tool}::${action}`;
}

function groupBy(
  records: readonly CapabilityRecord[],
  key: (record: CapabilityRecord) => string
): ReadonlyMap<string, readonly CapabilityRecord[]> {
  const grouped = new Map<string, CapabilityRecord[]>();
  for (const record of records) {
    const bucket = grouped.get(key(record));
    if (bucket === undefined) grouped.set(key(record), [record]);
    else bucket.push(record);
  }
  return grouped;
}

function buildIndex(): CapabilityIndex {
  const records = Object.freeze(
    [...CANONICAL_CAPABILITY_RECORDS].sort((left, right) => compareAscii(left.id, right.id))
  );

  const byId = new Map<string, CapabilityRecord>();
  const byAlias = new Map<string, CapabilityRecord>();
  const byLegacyPair = new Map<string, CapabilityRecord>();
  for (const record of records) {
    byId.set(record.id, record);
    for (const alias of record.aliases) byAlias.set(alias, record);
    for (const legacy of record.legacyIds) {
      byLegacyPair.set(legacyPairKey(legacy.tool, legacy.action), record);
    }
  }

  const byDomain = groupBy(records, (record) => record.discovery.domain);
  const familiesByDomain = new Map<string, readonly string[]>();
  for (const [domain, domainRecords] of byDomain) {
    const families = [...new Set(domainRecords.map((record) => record.discovery.family))];
    familiesByDomain.set(domain, Object.freeze(families.sort(compareAscii)));
  }

  return Object.freeze({
    records,
    byId,
    byAlias,
    byLegacyPair,
    byParentTool: groupBy(records, (record) => record.routing.parentTool),
    domains: Object.freeze([...byDomain.keys()].sort(compareAscii)),
    familiesByDomain,
    search: createCapabilitySearchIndex(records)
  });
}

let index: CapabilityIndex | undefined;

export function capabilityIndex(): CapabilityIndex {
  index ??= buildIndex();
  return index;
}

export function catalogRevision(): string {
  return CATALOG_REVISION;
}

export type CapabilityResolution =
  | { readonly kind: 'canonical'; readonly record: CapabilityRecord }
  | { readonly kind: 'alias'; readonly record: CapabilityRecord; readonly alias: string }
  | { readonly kind: 'legacy'; readonly record: CapabilityRecord; readonly tool: string; readonly action: string }
  | { readonly kind: 'unknown' };

export function resolveCapability(reference: string): CapabilityResolution {
  const { byId, byAlias } = capabilityIndex();
  const canonical = byId.get(reference);
  if (canonical !== undefined) return { kind: 'canonical', record: canonical };
  const aliased = byAlias.get(reference);
  if (aliased !== undefined) return { kind: 'alias', record: aliased, alias: reference };
  return { kind: 'unknown' };
}

export function resolveLegacyPair(tool: string, action: string): CapabilityResolution {
  const record = capabilityIndex().byLegacyPair.get(legacyPairKey(tool, action));
  return record === undefined ? { kind: 'unknown' } : { kind: 'legacy', record, tool, action };
}

export function allCapabilityIds(): readonly string[] {
  return capabilityIndex().records.map((record) => record.id);
}

export function capabilitiesInFamily(domain: string, family: string): readonly CapabilityRecord[] {
  return capabilityIndex().records.filter(
    (record) => record.discovery.domain === domain && record.discovery.family === family
  );
}
