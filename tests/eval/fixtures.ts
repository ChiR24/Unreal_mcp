// tests/eval/fixtures.ts
// The retrieval population and the small maths the eval measurements share.
// The population is the final registry the gateway actually serves.

import type { CapabilityRecord } from '../../src/tools/catalog/capabilities/model.js';
import { capabilityIndex, resolveLegacyPair } from '../../src/server/gateway/gateway-capability-index.js';
import { canonicalCapabilityId } from '../../src/tools/catalog/capabilities/retrieval/alias-fold.js';
import { type CapabilityRef, corpus } from './corpus.data.js';

/** The gateway's own default `search` page. "Top-K" is that page. */
export const GATEWAY_DEFAULT_SEARCH_LIMIT = 12 as const;

export function finalRegistryRecords(): readonly CapabilityRecord[] {
  return capabilityIndex().records;
}

/**
 * Resolves a corpus reference to the id search answers in. A declared alias is
 * folded into its primary through the registry's own fold, never the corpus, so
 * this cannot make a wrong answer look right.
 */
function canonicalIdFor(reference: CapabilityRef): string | undefined {
  const resolved = resolveLegacyPair(reference.tool, reference.action);
  if (resolved.kind === 'unknown') return undefined;
  return canonicalCapabilityId(capabilityIndex().search.aliasFold, String(resolved.record.id));
}

export type RetrievalCase = {
  readonly id: string;
  readonly intent: string;
  readonly expectedCapabilityId: string;
  readonly acceptedCapabilityIds: readonly string[];
};

/** Corpus cases that expect a positive match; the two negative kinds expect none. */
export function retrievalCases(): readonly RetrievalCase[] {
  const cases: RetrievalCase[] = [];
  for (const entry of corpus) {
    if (entry.kind === 'version_negative' || entry.kind === 'plugin_negative') continue;
    const expectedCapabilityId = canonicalIdFor(entry.expected);
    if (expectedCapabilityId === undefined) continue;
    const accepted = new Set<string>([expectedCapabilityId]);
    for (const alternative of entry.allowedAlternatives) {
      const id = canonicalIdFor(alternative);
      if (id !== undefined) accepted.add(id);
    }
    cases.push({ id: entry.id, intent: entry.intent, expectedCapabilityId, acceptedCapabilityIds: [...accepted] });
  }
  return cases;
}

/** Nearest-rank p95. */
export function p95(samples: readonly number[]): number {
  const sorted = [...samples].sort((left, right) => left - right);
  return sorted[Math.max(0, Math.ceil(sorted.length * 0.95) - 1)] ?? 0;
}

