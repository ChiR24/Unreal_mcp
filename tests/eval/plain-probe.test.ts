// tests/eval/plain-probe.test.ts
// Holds search to a floor on the plain-phrasing probe: how many of its requests rank an
// accepted capability first. The floor is a measured count; raise it when a change gains
// cases, never lower it to make a change pass.

import { describe, expect, it } from 'vitest';
import { finalRegistryRecords } from './fixtures.js';
import { nativeSearchRanker } from './native-ranker.js';
import { PLAIN_PROBE } from './plain-probe.data.js';

const FLOORS = { native: 679 } as const;

const firstHits = (): number =>
  PLAIN_PROBE.filter((entry) => entry.accepted.includes(String(nativeSearchRanker(entry.query, 1)[0]))).length;

describe('plain-phrasing probe', () => {
  it('accepts only capabilities that exist', () => {
    const ids = new Set(finalRegistryRecords().map((record) => String(record.id)));
    for (const entry of PLAIN_PROBE) {
      for (const id of entry.accepted) expect(ids.has(id), `${entry.query} -> ${id}`).toBe(true);
    }
  });

  it(`ranks an accepted capability first for at least ${FLOORS.native}`, () => {
    expect(firstHits()).toBeGreaterThanOrEqual(FLOORS.native);
  });
});
