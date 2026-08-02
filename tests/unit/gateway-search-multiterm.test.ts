import { describe, expect, it } from 'vitest';

import { searchGatewayCapabilities } from '../../src/server/gateway/gateway-search.js';

function total(query: string): number {
  const result = searchGatewayCapabilities({ operation: 'search', query, limit: 25 }) as {
    total?: number;
  };
  return result.total ?? 0;
}

describe('multi-term search does not collapse to the empty set', () => {
  it.each([
    ['spawn', 'actor', 'spawn actor'],
    ['create', 'material', 'create material'],
    ['delete', 'actor', 'delete actor']
  ])('%s + %s still returns results as "%s"', (left, right, phrase) => {
    const leftTotal = total(left);
    const rightTotal = total(right);
    expect(leftTotal, `single term '${left}' must match something`).toBeGreaterThan(0);
    expect(rightTotal, `single term '${right}' must match something`).toBeGreaterThan(0);
    expect(
      total(phrase),
      `'${phrase}' returned nothing while '${left}' matched ${leftTotal} and '${right}' matched ${rightTotal}; `
        + 'a zero total is indistinguishable from "no such capability" and sends callers to invent action names'
    ).toBeGreaterThan(0);
  });

  it('a verb-noun phrase naming a real capability surfaces that capability', () => {
    const result = searchGatewayCapabilities({ operation: 'search', query: 'spawn actor', limit: 25 }) as {
      results?: Array<{ capability?: string }>;
    };
    const ids = (result.results ?? []).map((row) => row.capability);
    expect(ids, 'control_actor.spawn is the canonical answer to "spawn actor"').toContain('control_actor.spawn');
  });

  it('an unmatched token does not annihilate an otherwise matching query', () => {
    const matched = total('list actors');
    expect(matched).toBeGreaterThan(0);
    expect(
      total('list actors zzzqqqnotatoken'),
      'one unknown token dropped every result, so any typo reads as "capability does not exist"'
    ).toBeGreaterThan(0);
  });
});
