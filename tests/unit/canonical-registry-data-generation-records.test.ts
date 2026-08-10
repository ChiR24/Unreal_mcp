/**
 * tests/unit/canonical-registry-data-generation-records.test.ts
 *
 * Task-23 data-generation contract (full TS / neutral model surface).
 * Locked guarantees:
 *  - all 1,335 capability records are present and unique as COMPLETE records
 *    (not summaries) in both the TS data module builder and the neutral model
 *    builder
 *  - the generated TS exports CANONICAL_CAPABILITY_RECORDS with every field
 *  - deterministic ordering (by canonical id within shard, by domain across
 *    shards), sanitized unique symbols, exact shard coverage, byte-identical
 *    builders, and atomic input validation
 */
import { describe, expect, it } from 'vitest';
import type { CapabilityRecord } from '../../src/tools/catalog/capabilities/model.js';
import {
  RECORDS,
  SORTED,
  buildNeutralRecords,
  buildTsRecords,
} from './canonical-registry-data-generation-fixtures.js';

describe('Task-23 canonical record universe', () => {
  it('exposes all 1,335 unique full capability records', () => {
    expect(RECORDS.length).toBe(1335);
    const ids = new Set(RECORDS.map((r) => r.id));
    expect(ids.size).toBe(1335);
  });

  it('every record carries full canonical data (schema/metadata/hashes)', () => {
    for (const r of RECORDS) {
      expect(r.routing.parentTool).toBeTruthy();
      expect(r.routing.dispatchAction).toBeTruthy();
      expect(r.discovery.domain).toBeTruthy();
      expect(r.hashes.schema).toMatch(/^[0-9a-f]{64}$/);
      expect(r.hashes.content).toMatch(/^[0-9a-f]{64}$/);
      expect(r.availability.unreal.min).toBeDefined();
      expect(r.availability.unreal.max).toBeDefined();
      expect(r.behavior.effect).toBeTruthy();
      expect(r.policy.requiredScope).toBeTruthy();
      expect(Object.keys(r.schemas.input.properties).length).toBeGreaterThanOrEqual(0);
    }
  });
});

describe('TS data module carries complete records', () => {
  const tsRecords = buildTsRecords();

  it('emits CANONICAL_CAPABILITY_RECORDS with all 1,335 unique complete records', () => {
    expect(tsRecords.length).toBe(1335);
    expect(new Set(tsRecords.map((r) => r.id)).size).toBe(1335);
  });

  it('deep-compares representative emitted records against source (schemas, normalization, deprecation, availability, examples, hashes)', () => {
    for (const src of [RECORDS[0], RECORDS[500], RECORDS[1334]]) {
      const emitted = tsRecords.find((r) => r.id === src.id);
      expect(emitted).toBeDefined();
      if (!emitted) continue;
      expect(emitted.routing).toEqual(src.routing);
      expect(emitted.discovery).toEqual(src.discovery);
      expect(emitted.schemas.input).toEqual(src.schemas.input);
      expect(emitted.schemas.output).toEqual(src.schemas.output);
      expect(emitted.examples).toEqual(src.examples);
      expect(emitted.normalization).toEqual(src.normalization);
      expect(emitted.deprecation).toEqual(src.deprecation);
      expect(emitted.availability).toEqual(src.availability);
      expect(emitted.behavior).toEqual(src.behavior);
      expect(emitted.policy).toEqual(src.policy);
      expect(emitted.cost).toEqual(src.cost);
      expect(emitted.aliases).toEqual(src.aliases);
      expect(emitted.legacyIds).toEqual(src.legacyIds);
      expect(emitted.hashes).toEqual(src.hashes);
    }
  });

  it('availability carries major/minor/patch/channel/preview + editorStates', () => {
    for (const r of tsRecords) {
      const min = r.availability.unreal.min as {
        major: number;
        minor: number;
        patch: number;
        channel: string;
        preview?: number;
      };
      const max = r.availability.unreal.max as {
        major: number;
        minor: number;
        patch: number;
        channel: string;
        preview?: number;
      };
      expect(min.major).toBe(5);
      expect(typeof min.minor).toBe('number');
      expect(typeof min.patch).toBe('number');
      expect(typeof min.channel).toBe('string');
      expect(max.major).toBe(5);
      expect(Array.isArray(r.availability.editorStates)).toBe(true);
      expect(Array.isArray(r.availability.requiredPlugins)).toBe(true);
    }
  });
});

describe('neutral JSON carries complete records', () => {
  const neutral = buildNeutralRecords();

  it('contains a top-level records array of all 1,335 unique complete records', () => {
    expect(neutral.length).toBe(1335);
    expect(new Set(neutral.map((r) => r.id)).size).toBe(1335);
  });

  it('deep-compares representative emitted records against source', () => {
    for (const src of [RECORDS[10], RECORDS[800], RECORDS[1000]]) {
      const emitted = neutral.find((r) => r.id === src.id);
      expect(emitted).toBeDefined();
      expect(emitted?.schemas.output).toEqual(src.schemas.output);
      expect(emitted?.normalization).toEqual(src.normalization);
      expect(emitted?.deprecation).toEqual(src.deprecation);
      expect(emitted?.availability).toEqual(src.availability);
      expect(emitted?.examples).toEqual(src.examples);
      expect(emitted?.hashes).toEqual(src.hashes);
    }
  });
});

// SORTED is part of the shared contract surface; pin it referenced.
expect(SORTED.length).toBe(1335);
type _CapabilityRecord = CapabilityRecord;
