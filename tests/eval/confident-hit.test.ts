// tests/eval/confident-hit.test.ts
// A search's first row carries a signature and an execute call only when the hit is clear (the runner-up scores at
// most SEARCH_CONFIDENT_RATIO_PERCENT of it) and needs no consent, so a caller may skip describe. Held here to its
// precision: on the corpus and on the plain probe, at least 95% of the rows that carry it are a capability the request
// accepts, on both doors. The coverage floors are measured counts; raise them when a change gains cases, never lower
// them to make a change pass.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';
import { describe, expect, it } from 'vitest';
import { SEARCH_CONFIDENT_RATIO_PERCENT, searchGatewayCapabilities } from '../../src/server/gateway/gateway-search.js';
import { finalRegistryRecords, retrievalCases } from './fixtures.js';
import { nativeScored } from './native-ranker.js';
import { PLAIN_PROBE } from './plain-probe.data.js';

type Case = { readonly query: string; readonly accepted: readonly string[] };
/** The capability a door hands a signature for, or undefined when its first hit is not clear. */
type ConfidentHit = (query: string) => string | undefined;

const CASES: Readonly<Record<'corpus' | 'probe', readonly Case[]>> = {
  corpus: retrievalCases().map((entry) => ({ query: entry.intent, accepted: entry.acceptedCapabilityIds })),
  probe: PLAIN_PROBE.map((entry) => ({ query: entry.query, accepted: entry.accepted }))
};
const FLOORS = { typescript: { corpus: 68, probe: 523 }, native: { corpus: 60, probe: 519 } } as const;

const nativeHeader = readFileSync(join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'MCP', 'Gateway', 'McpNativeGatewaySearch.h'), 'utf8');
const NATIVE_RATIO_PERCENT = Number(nativeHeader.match(/constexpr int32 McpSearchConfidentRatioPercent = (\d+);/u)?.[1]);
const consentFree = new Set(finalRegistryRecords().filter((record) => record.policy.consent === 'none').map((record) => String(record.id)));

const typescriptHit: ConfidentHit = (query) => {
  const results = searchGatewayCapabilities({ operation: 'search', query }).results;
  const first = Array.isArray(results) ? (results[0] as Record<string, unknown> | undefined) : undefined;
  return first?.signature === undefined ? undefined : String(first.capability);
};

// The rule McpGatewaySearchCapabilities applies, over the TypeScript copy of the native ranker.
const nativeHit: ConfidentHit = (query) => {
  const [first, second] = nativeScored(query);
  if (first === undefined || first.score <= 0 || !consentFree.has(first.id)) return undefined;
  return second !== undefined && second.score * 100 > first.score * NATIVE_RATIO_PERCENT ? undefined : first.id;
};

function measure(hit: ConfidentHit, cases: readonly Case[]): { confident: number; precision: number; wrong: string[] } {
  let confident = 0;
  let right = 0;
  const wrong: string[] = [];
  for (const entry of cases) {
    const id = hit(entry.query);
    if (id === undefined) continue;
    confident += 1;
    if (entry.accepted.includes(id)) right += 1;
    else wrong.push(`${entry.query} -> ${id}`);
  }
  return { confident, precision: right / Math.max(1, confident), wrong };
}

describe('a confident first hit', () => {
  it('uses the same ratio on both doors', () => {
    expect(NATIVE_RATIO_PERCENT).toBe(SEARCH_CONFIDENT_RATIO_PERCENT);
  });

  for (const [door, hit] of [['typescript', typescriptHit], ['native', nativeHit]] as const) {
    for (const set of ['corpus', 'probe'] as const) {
      it(`is a capability the ${set} accepts at least 95% of the time on the ${door} door`, () => {
        const measured = measure(hit, CASES[set]);
        expect(measured.precision, measured.wrong.join('\n')).toBeGreaterThanOrEqual(0.95);
        expect(measured.confident, 'rows carrying a signature').toBeGreaterThanOrEqual(FLOORS[door][set]);
      });
    }
  }

  it('a wrong hit fails the measure', () => {
    expect(measure(() => 'definitely.not.a.capability', CASES.corpus).precision).toBe(0);
  });
});
