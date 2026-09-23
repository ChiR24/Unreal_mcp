// Plain phrasings a small model uses, measured one by one through the production
// search. The aggregate top-1 budget can clear while one of these misses, so each
// is pinned to rank first on its own.
import { describe, expect, it } from 'vitest';
import { gatewaySearchRanker } from './measure-retrieval.js';
import { retrievalCases } from './fixtures.js';

describe('plain-phrasing retrieval', () => {
  const cases = retrievalCases().filter((entry) => entry.id.startsWith('plain.'));

  it('covers the plain-phrasing corpus section', () => {
    expect(cases.length).toBe(6);
  });

  it.each(cases.map((entry) => [entry.id, entry] as const))('%s ranks an accepted capability first', (_id, entry) => {
    const top = gatewaySearchRanker(entry.intent, 5)[0];
    expect(entry.acceptedCapabilityIds).toContain(top);
  });
});
