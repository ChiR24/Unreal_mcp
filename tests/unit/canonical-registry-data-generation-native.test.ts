/**
 * tests/unit/canonical-registry-data-generation-native.test.ts
 *
 * Task-23 data-generation contract (native capability shard surface).
 * Locked guarantees:
 *  - native capability shards carry the COMPLETE CapabilityRecord per entry
 *    (aliases, legacyIds, discovery, schemas.input+output, examples,
 *    availability major/minor/patch/channel/preview + plugins + editorStates,
 *    behavior, policy, cost, routing, normalization, full deprecation, hashes)
 *  - deterministic ordering (by canonical id within shard, by parent across
 *    shards), sanitized unique symbols, exact shard coverage, byte-identical
 *    builders, and atomic input validation
 */
import { describe, expect, it } from 'vitest';
import {
  buildNativeCapabilityShards,
  buildNativeCapabilityIndexHeader,
  buildNativeCapabilityShardSource,
  serializeNativeCapabilityRecord,
  sanitizeParentSymbol,
  MAX_CHUNK_CHARS,
} from '../../scripts/canonical-registry/native-shards.js';
import type { CapabilityRecord } from '../../src/tools/catalog/capabilities/model.js';
import { RECORDS, SORTED } from './canonical-registry-data-generation-fixtures.js';

describe('native capability shard plan', () => {
  const shards = buildNativeCapabilityShards(SORTED);

  it('produces exactly 23 reviewable parent shards', () => {
    expect(shards.length).toBe(23);
  });

  it('every shard symbol is a sanitized C++ identifier (no spaces/hyphens) and unique', () => {
    const symbols = new Set<string>();
    for (const s of shards) {
      expect(s.symbol).toMatch(/^MCP_CAP_SHARD_[A-Z0-9_]+$/);
      expect(s.symbol).not.toMatch(/[\s-]/);
      if (symbols.has(s.symbol)) throw new Error(`duplicate native symbol ${s.symbol}`);
      symbols.add(s.symbol);
    }
    expect(symbols.size).toBe(23);
  });

  it('covers all 1,335 COMPLETE records exactly once (no schema boolean; full record)', () => {
    const total = shards.reduce((n, s) => n + s.count, 0);
    expect(total).toBe(1335);
    const seen = new Set<string>();
    for (const s of shards) {
      const parsed = JSON.parse(s.json) as { record: CapabilityRecord }[];
      expect(parsed.length).toBe(s.count);
      for (const { record: e } of parsed) {
        expect(seen.has(e.id)).toBe(false);
        seen.add(e.id);
        expect(e.schemas.input).toBeDefined();
        expect(e.schemas.output).toBeDefined();
        expect(e.discovery).toBeDefined();
        expect(e.normalization).toBeDefined();
        expect(e.deprecation).toBeDefined();
        expect(e.availability.unreal.min).toBeDefined();
        expect(e.availability.unreal.max).toBeDefined();
        expect(e.hashes.schema).toMatch(/^[0-9a-f]{64}$/);
        expect(e.hashes.content).toMatch(/^[0-9a-f]{64}$/);
        expect((e as unknown as { sch?: unknown }).sch).toBeUndefined();
      }
    }
    expect(seen.size).toBe(1335);
  });

  it('is deterministically ordered: shards by parent, entries by canonical id', () => {
    const parents = shards.map((s) => s.parent);
    const sortedParents = [...parents].sort((a, b) => (a < b ? -1 : 1));
    expect(parents).toEqual(sortedParents);
    for (const s of shards) {
      const ids = (JSON.parse(s.json) as { record: CapabilityRecord }[]).map((x) => x.record.id);
      const sortedIds = [...ids].sort((a, b) => (a < b ? -1 : 1));
      expect(ids).toEqual(sortedIds);
    }
  });

  it('deep-compares every parsed native record against source (full field set)', () => {
    const byId = new Map(RECORDS.map((r) => [r.id, r]));
    for (const s of shards) {
      const parsed = JSON.parse(s.json) as { record: CapabilityRecord }[];
      for (const { record: e } of parsed) {
        const src = byId.get(e.id) as CapabilityRecord;
        expect(e.schemas.input).toEqual(src.schemas.input);
        expect(e.schemas.output).toEqual(src.schemas.output);
        expect(e.discovery).toEqual(src.discovery);
        expect(e.examples).toEqual(src.examples);
        expect(e.normalization).toEqual(src.normalization);
        expect(e.deprecation).toEqual(src.deprecation);
        expect(e.availability).toEqual(src.availability);
        expect(e.behavior).toEqual(src.behavior);
        expect(e.policy).toEqual(src.policy);
        expect(e.cost).toEqual(src.cost);
        expect(e.routing).toEqual(src.routing);
        expect(e.aliases).toEqual(src.aliases);
        expect(e.legacyIds).toEqual(src.legacyIds);
        expect(e.hashes).toEqual(src.hashes);
      }
    }
  });

  it('index header enumerates every shard symbol with its record count', () => {
    const header = buildNativeCapabilityIndexHeader(shards, 'revision00000000');
    expect(header).toContain('#pragma once');
    for (const s of shards) {
      expect(header).toContain(`extern const TCHAR* const ${s.symbol}_CHUNKS[];`);
      expect(header).toContain(
        `{ TEXT("${s.parent}"), Detail::${s.symbol}_CHUNKS, ${s.chunks.length}, ${s.count} },`,
      );
    }
    expect(header).toContain('23 shards, 1335 records total.');
    expect(header).toContain('inline const TCHAR* CatalogRevision() { return TEXT("revision00000000"); }');
  });

  it('each shard source defines its symbol as bounded, escaped, non-raw literals', () => {
    for (const s of shards) {
      const src = buildNativeCapabilityShardSource(s);
      expect(src).toContain(`const TCHAR* const ${s.symbol}_CHUNKS[] = {`);
      expect(src).not.toContain('TEXT(R"');
      expect(src).not.toContain('MCPCS');
      for (const chunk of s.chunks) {
        expect(chunk.length).toBeLessThanOrEqual(MAX_CHUNK_CHARS);
      }
    }
  });
});

describe('byte-identical builders', () => {
  it('buildNativeCapabilityShards is deterministic across runs', () => {
    const a = buildNativeCapabilityShards(SORTED);
    const b = buildNativeCapabilityShards(SORTED);
    expect(JSON.stringify(a)).toEqual(JSON.stringify(b));
  });

  it('serializeNativeCapabilityRecord wraps the complete record and is stable', () => {
    for (const r of RECORDS) {
      const once = JSON.parse(serializeNativeCapabilityRecord(r)) as { record: CapabilityRecord };
      expect(once.record).toEqual(r);
      expect(serializeNativeCapabilityRecord(r)).toEqual(serializeNativeCapabilityRecord(r));
    }
  });
});

describe('atomic input validation', () => {
  it('rejects duplicate canonical ids before producing shards', () => {
    const dup = [RECORDS[0], { ...RECORDS[1], id: RECORDS[0].id }] as unknown as CapabilityRecord[];
    expect(() => buildNativeCapabilityShards(dup)).toThrow(/duplicate canonical id/);
  });

  it('rejects a record with empty/missing parent', () => {
    const broken = [
      { ...RECORDS[0], routing: { ...RECORDS[0].routing, parentTool: '' } },
    ] as unknown as CapabilityRecord[];
    expect(() => buildNativeCapabilityShards(broken)).toThrow(/empty\/missing parent/);
  });

  it('rejects a symbol collision across distinct parents', () => {
    const a = { ...RECORDS[0], id: 'x.a', routing: { ...RECORDS[0].routing, parentTool: 'foo-bar' } };
    const b = { ...RECORDS[1], id: 'x.b', routing: { ...RECORDS[1].routing, parentTool: 'foo bar' } };
    const fake = [a, b] as unknown as CapabilityRecord[];
    expect(() => buildNativeCapabilityShards(fake)).toThrow(/symbol collision/);
  });

  it('sanitizeParentSymbol yields identical symbols for spaces vs hyphens', () => {
    expect(sanitizeParentSymbol('foo bar')).toBe(sanitizeParentSymbol('foo-bar'));
    expect(sanitizeParentSymbol('animation physics')).toBe('MCP_CAP_SHARD_ANIMATION_PHYSICS');
  });
});
