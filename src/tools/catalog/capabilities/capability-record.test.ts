import { describe, expect, it } from 'vitest';
import { ZodError } from 'zod';

import { DRAFT_SCHEMA_URI, validCapabilitySource } from './capability-record.test-support.js';
import { CapabilitySerializationError } from './hashing.js';
import {
  capabilityErrorPointers,
  createCapabilityRecord,
  parseCapabilityRecord,
  stableJsonStringify
} from './index.js';

function sourceRejectionPointers(input: unknown): readonly string[] {
  try {
    createCapabilityRecord(input);
    return [];
  } catch (error) {
    if (error instanceof ZodError) return capabilityErrorPointers(error);
    throw error;
  }
}

function recordRejectionPointers(input: unknown): readonly string[] {
  try {
    parseCapabilityRecord(input);
    return [];
  } catch (error) {
    if (error instanceof ZodError) return capabilityErrorPointers(error);
    throw error;
  }
}

describe('CapabilityRecord validation boundary', () => {
  it('round-trips a version-gated destructive long-running record with stable hashes', () => {
    // Given
    const source = validCapabilitySource();

    // When
    const first = createCapabilityRecord(source);
    const second = createCapabilityRecord(source);
    const roundTripped = parseCapabilityRecord(JSON.parse(JSON.stringify(first)));

    // Then
    expect(roundTripped).toEqual(first);
    expect(second).toEqual(first);
    expect(stableJsonStringify({ z: 1, a: { y: 2, b: 3 } }))
      .toBe(stableJsonStringify({ a: { b: 3, y: 2 }, z: 1 }));
  });

  it('rejects unknown record fields at the exact JSON pointer', () => {
    // Given
    const source = { ...validCapabilitySource(), surprise: true };

    // When
    const pointers = sourceRejectionPointers(source);

    // Then
    expect(pointers).toContain('/surprise');
  });

  it('rejects malformed canonical and alias IDs', () => {
    // Given
    const source = validCapabilitySource();

    // When
    const canonicalPointers = sourceRejectionPointers({ ...source, id: 'Manage Asset.Delete' });
    const aliasPointers = sourceRejectionPointers({ ...source, aliases: ['asset/delete'] });

    // Then
    expect(canonicalPointers).toContain('/id');
    expect(aliasPointers).toContain('/aliases/0');
  });

  it('rejects a reversed Unreal Engine availability range', () => {
    // Given
    const source = validCapabilitySource();
    const reversed = {
      ...source,
      availability: {
        ...source.availability,
        unreal: {
          min: { major: 5, minor: 8, patch: 0, channel: 'stable' },
          max: { major: 5, minor: 7, patch: 4, channel: 'stable' }
        }
      }
    };

    // When
    const pointers = sourceRejectionPointers(reversed);

    // Then
    expect(pointers).toContain('/availability/unreal/max');
  });

  it('rejects missing output and policy contracts', () => {
    // Given
    const source = validCapabilitySource();
    const missingOutput = { ...source, schemas: { input: source.schemas.input } };
    const missingPolicy = {
      id: source.id,
      aliases: source.aliases,
      legacyIds: source.legacyIds,
      discovery: source.discovery,
      schemas: source.schemas,
      examples: source.examples,
      availability: source.availability,
      behavior: source.behavior,
      cost: source.cost,
      routing: source.routing,
      normalization: source.normalization,
      deprecation: source.deprecation
    };

    // When
    const outputPointers = sourceRejectionPointers(missingOutput);
    const policyPointers = sourceRejectionPointers(missingPolicy);

    // Then
    expect(outputPointers).toContain('/schemas/output');
    expect(policyPointers).toContain('/policy');
  });

  it('rejects invalid and stale schema or content hashes', () => {
    // Given
    const record = createCapabilityRecord(validCapabilitySource());

    // When
    const malformedPointers = recordRejectionPointers({
      ...record,
      hashes: { algorithm: 'sha256', schema: 'ABC', content: 'not-a-hash' }
    });
    const stalePointers = recordRejectionPointers({
      ...record,
      discovery: {
        ...validCapabilitySource().discovery,
        summary: 'Content changed after hashing.'
      }
    });

    // Then
    expect(malformedPointers).toEqual(expect.arrayContaining(['/hashes/schema', '/hashes/content']));
    expect(stalePointers).toContain('/hashes/content');
  });

  it('rejects unbounded generic objects without an explicit reflection boundary', () => {
    // Given
    const source = validCapabilitySource();
    const freeform = {
      ...source,
      schemas: {
        ...source.schemas,
        input: {
          ...source.schemas.input,
          properties: {
            properties: {
              type: 'object',
              description: 'Arbitrary reflected Unreal properties.',
              additionalProperties: true
            }
          },
          required: ['properties']
        }
      }
    };

    // When
    const pointers = sourceRejectionPointers(freeform);

    // Then
    expect(pointers).toContain('/schemas/input/properties/properties');
  });

  it('rejects nested object with omitted additionalProperties as unbounded', () => {
    // Given a nested object schema that omits additionalProperties entirely
    const source = validCapabilitySource();
    const freeform = {
      ...source,
      schemas: {
        ...source.schemas,
        input: {
          ...source.schemas.input,
          properties: {
            props: {
              type: 'object',
              description: 'Omitted additionalProperties means unbounded.',
              properties: { a: { type: 'string' } }
            }
          },
          required: ['props']
        }
      }
    };

    // When
    const pointers = sourceRejectionPointers(freeform);

    // Then the omitted (unbounded) nested object is rejected at the exact pointer
    expect(pointers).toContain('/schemas/input/properties/props');
  });

  it('accepts a documented explicit Unreal reflection boundary', () => {
    // Given
    const source = validCapabilitySource();
    const reflected = {
      ...source,
      schemas: {
        ...source.schemas,
        input: {
          ...source.schemas.input,
          properties: {
            properties: {
              type: 'object',
              description: 'Arbitrary reflected Unreal properties resolved at runtime.',
              additionalProperties: true,
              'x-unreal-reflection-boundary': true
            }
          },
          required: ['properties']
        }
      }
    };

    // When
    const record = createCapabilityRecord(reflected);

    // Then
    expect(parseCapabilityRecord(record)).toEqual(record);
    expect(record.schemas.input.$schema).toBe(DRAFT_SCHEMA_URI);
  });
});

describe('Capability serializer invariants', () => {
  it('rejects non-JSON and non-finite values with a typed error', () => {
    // Given unsupported inputs
    const unsupported: readonly unknown[] = [
      undefined,
      () => 1,
      Symbol('x'),
      1n,
      NaN,
      Infinity,
      -Infinity
    ];

    // When / Then each is rejected with the same typed error
    for (const value of unsupported) {
      expect(() => stableJsonStringify(value)).toThrow(CapabilitySerializationError);
    }
  });

  it('normalizes -0 deterministically and sorts object keys', () => {
    // Given
    const negativeZero = -0;
    const unsorted = { b: 1, a: { d: 2, c: 3 } };

    // When / Then
    expect(stableJsonStringify(negativeZero)).toBe('0');
    expect(stableJsonStringify(unsorted)).toBe('{"a":{"c":3,"d":2},"b":1}');
  });
});
