import { describe, expect, it } from 'vitest';

import { capabilityIndex } from '../../src/server/gateway/gateway-capability-index.js';
import { closestMatches } from '../../src/server/gateway/gateway-guidance.js';
import { closestMatches as nativeClosestMatches } from './plugin/gateway/native-discovery-model.js';

describe('gateway guidance ordinal parity', () => {
  it('uses ordinal ordering for score ties', () => {
    // Given: candidates whose score and prefix are identical but whose locale and ordinal orders differ.
    const candidates = ['a.b', 'a_b'];

    // When: suggestions are ranked.
    const suggestions = closestMatches('a', candidates, candidates.length);

    // Then: the TypeScript surface follows the native FString ordinal order.
    expect(suggestions).toEqual(['a.b', 'a_b']);
  });

  it('matches the native model across catalog probes', () => {
    // Given: real capability ids and input parameter names from the canonical registry.
    const records = capabilityIndex().records;
    const capabilityIds = records.map((record) => record.id);
    const parameterNames = [
      ...new Set(records.flatMap((record) => Object.keys(record.schemas.input.properties ?? {}))),
    ];
    const probes = ['asset', 'create', 'get', 'manage', 'name', 'path', 'set'] as const;

    // When/Then: both surfaces rank every deterministic probe identically.
    for (const candidates of [capabilityIds, parameterNames]) {
      for (const probe of probes) {
        expect(closestMatches(probe, [...candidates], 25)).toEqual(
          nativeClosestMatches(probe, candidates, 25),
        );
      }
    }
  });
});
