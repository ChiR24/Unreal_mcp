// tests/eval/retrieval-fold.test.ts
// Proves the Task-48 retrieval remediation is load-bearing rather than
// incidental. Each breach is INJECTED into a rebuilt index, so a future change
// that quietly undoes one of these properties fails here instead of silently
// costing accuracy.

import { describe, expect, it } from 'vitest';
import {
  capabilityIndex,
  resolveCapability,
  resolveLegacyPair,
} from '../../src/server/gateway/gateway-capability-index.js';
import { describeGatewayCapability } from '../../src/server/gateway/gateway-describe.js';
import { searchGatewayCapabilities } from '../../src/server/gateway/gateway-search.js';
import type { CapabilityRecord } from '../../src/tools/catalog/capabilities/model.js';
import {
  canonicalCapabilityId,
  deriveAliasFold,
} from '../../src/tools/catalog/capabilities/retrieval/alias-fold.js';
import {
  createCapabilitySearchIndex,
  rankCapabilityRecords,
} from '../../src/tools/catalog/capabilities/retrieval/scoring.js';
import { GATEWAY_DEFAULT_SEARCH_LIMIT, retrievalCases } from './fixtures.js';

const records = capabilityIndex().records;
const fold = deriveAliasFold(records);

function top1(index: ReturnType<typeof createCapabilitySearchIndex>, pool: readonly CapabilityRecord[]): number {
  let correct = 0;
  for (const entry of retrievalCases()) {
    const ranked = rankCapabilityRecords(index, pool, entry.intent);
    const top = ranked[0];
    if (top !== undefined && entry.acceptedCapabilityIds.includes(String(top.record.id))) correct += 1;
  }
  return correct / retrievalCases().length;
}

const shipped = createCapabilitySearchIndex(records);
const shippedTop1 = top1(shipped, records);

describe('task 48 alias fold', () => {
  it('Given the catalog, When the fold is derived, Then rationale-declared aliases resolve to a real primary', () => {
    expect(fold.targets.size).toBeGreaterThan(0);
    for (const [alias, primary] of fold.targets) {
      expect(alias).not.toBe(primary);
      expect(records.some((record) => String(record.id) === primary)).toBe(true);
      expect(fold.targets.has(primary)).toBe(false);
    }
  });

  it('Given a folded alias, When search runs, Then the alias is never returned but its primary is indexed', () => {
    const indexedIds = new Set(shipped.documents.map((document) => String(document.record.id)));

    for (const [alias, primary] of fold.targets) {
      expect(indexedIds.has(alias)).toBe(false);
      expect(indexedIds.has(primary)).toBe(true);
    }
  });

  it('Given a folded alias, When execute and describe resolve it, Then it is still fully addressable', () => {
    for (const [alias] of fold.targets) {
      const record = records.find((candidate) => String(candidate.id) === alias);
      if (record === undefined) throw new Error(`missing alias record ${alias}`);

      expect(resolveCapability(alias).kind).toBe('canonical');
      for (const legacy of record.legacyIds) {
        expect(resolveLegacyPair(legacy.tool, legacy.action).kind).toBe('legacy');
      }
      const described = describeGatewayCapability({ operation: 'describe', capability: alias });
      expect(described.success).toBe(true);
    }
  });

  it('Given a search response, When rows are read, Then no row names a folded alias', () => {
    for (const entry of retrievalCases()) {
      const response = searchGatewayCapabilities({ query: entry.intent, limit: GATEWAY_DEFAULT_SEARCH_LIMIT });
      const rows = Array.isArray(response.results) ? response.results : [];
      for (const row of rows) {
        const capability = String((row as Record<string, unknown>).capability);
        expect(fold.targets.has(capability)).toBe(false);
      }
    }
  });

  it('Given a corpus reference to a folded alias, When canonicalised, Then it becomes the primary search answers in', () => {
    const alias = [...fold.targets.keys()][0];
    if (alias === undefined) throw new Error('no folded alias');

    expect(canonicalCapabilityId(fold, alias)).toBe(fold.targets.get(alias));
  });
});

describe('task 48 injected breaches', () => {
  it('Given the alias documents restored, When top-1 is measured, Then accuracy regresses below the shipped ranking', () => {
    // The breach: aliases compete as independent documents again.
    const withAliasDocuments = createCapabilitySearchIndex(
      records.map((record) => (
        fold.targets.has(String(record.id))
          ? { ...record, normalization: { ...record.normalization, rationale: 'canonical record' } }
          : record
      )),
    );

    expect(top1(withAliasDocuments, records)).toBeLessThan(shippedTop1);
  });

  it('Given a wrong alias fold, When recall is measured, Then folding unrelated capabilities loses the expected answers', () => {
    // The breach: every record claims to be an alias of one arbitrary primary.
    const primary = String(records[0]?.id ?? '');
    const collapsed = createCapabilitySearchIndex(
      records.map((record, position) => (
        position === 0
          ? record
          : { ...record, normalization: { ...record.normalization, rationale: `Alias of ${primary}.` } }
      )),
    );
    let recalled = 0;
    for (const entry of retrievalCases()) {
      const ranked = rankCapabilityRecords(collapsed, records, entry.intent);
      if (ranked.some((match) => String(match.record.id) === entry.expectedCapabilityId)) recalled += 1;
    }

    expect(recalled / retrievalCases().length).toBeLessThan(0.98);
  });

  it('Given a token re-projected into a lower-weighted field, When ranking runs, Then repetition buys no accuracy', () => {
    // The collapse makes a token credit only the HIGHEST-weighted field
    // carrying it, so echoing the dispatch action into `when_not_to_use` cannot
    // profit. It is not asserted to be byte-identical: adding tokens still
    // shifts document frequency and average field length, which are corpus-wide
    // statistics the collapse does not claim to freeze.
    const reprojected = createCapabilitySearchIndex(
      records.map((record) => ({
        ...record,
        discovery: {
          ...record.discovery,
          whenNotToUse: [...record.discovery.whenNotToUse, record.routing.dispatchAction],
        },
      })),
    );

    expect(top1(reprojected, records)).toBeLessThanOrEqual(shippedTop1);
  });

  it('Given the measured denominator, When cases are counted, Then it is exactly the 56 positive corpus cases', () => {
    expect(retrievalCases().length).toBe(56);
  });
});
