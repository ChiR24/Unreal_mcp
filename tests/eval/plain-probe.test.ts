// tests/eval/plain-probe.test.ts
// Holds both doors to a floor on the plain-phrasing probe: how many of its requests rank an
// accepted capability first. The floors are measured counts; raise them when a change gains
// cases, never lower them to make a change pass.

import { describe, expect, it } from 'vitest';
import { finalRegistryRecords } from './fixtures.js';
import { gatewaySearchRanker, type GatewayRanker } from './measure-retrieval.js';
import { nativeSearchRanker } from './native-ranker.js';
import { PLAIN_PROBE } from './plain-probe.data.js';

const FLOORS = { typescript: 374, native: 375 } as const;

const firstHits = (rank: GatewayRanker): number =>
  PLAIN_PROBE.filter((entry) => entry.accepted.includes(String(rank(entry.query, 1)[0]))).length;

describe('plain-phrasing probe', () => {
  it('accepts only capabilities that exist', () => {
    const ids = new Set(finalRegistryRecords().map((record) => String(record.id)));
    for (const entry of PLAIN_PROBE) {
      for (const id of entry.accepted) expect(ids.has(id), `${entry.query} -> ${id}`).toBe(true);
    }
  });

  it(`the TypeScript door ranks an accepted capability first for at least ${FLOORS.typescript}`, () => {
    expect(firstHits(gatewaySearchRanker)).toBeGreaterThanOrEqual(FLOORS.typescript);
  });

  it(`the native door ranks an accepted capability first for at least ${FLOORS.native}`, () => {
    expect(firstHits(nativeSearchRanker)).toBeGreaterThanOrEqual(FLOORS.native);
  });
});
