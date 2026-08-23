/**
 * Focused unit tests for the Task 5 normalization inventory — metrics,
 * canonical model, and schema validation.
 *
 * These tests pin the reviewed, evidence-derived metrics (1,340 occurrences;
 * 36 duplicate names; 83 duplicate-name occurrences; max 47 exact-name
 * reductions; 820 add/create/set/configure) and the structural invariants the
 * plan requires. They read the authoritative source through `buildInventory`
 * and `generateInventory`, so any drift in the 23 tool definitions fails the
 * metrics assertions instead of being forced.
 */

import { describe, expect, it } from 'vitest';
import { buildInventory } from '../../src/tools/catalog/capabilities/normalization/build.js';
import {
  generateInventory,
} from '../../src/tools/catalog/capabilities/normalization/generate.js';
import { naiveNameOnlyCanonicalId } from '../../src/tools/catalog/capabilities/normalization/naive.js';
import type { NormalizationInventory } from '../../src/tools/catalog/capabilities/normalization/types.js';
import { validateInventoryData } from '../../src/tools/catalog/capabilities/normalization/validate.js';

function clone(value: unknown): unknown {
  return JSON.parse(JSON.stringify(value));
}

describe('reviewed fixed metrics (reproduced from authoritative source)', () => {
  const inv = buildInventory();

  it('has exactly 1,341 classified occurrences', () => {
    expect(inv.occurrences.length).toBe(1341);
    expect(inv.metrics.occurrenceCount).toBe(1341);
  });

  it('reports 36 duplicate names', () => {
    expect(inv.metrics.duplicateNames).toBe(36);
  });

  it('reports 83 duplicate-name occurrences', () => {
    expect(inv.metrics.duplicateNameOccurrences).toBe(83);
  });

  it('reports maximum 47 exact-name reductions', () => {
    expect(inv.metrics.maxExactNameReductions).toBe(47);
  });

   it('reports 820 add/create/set/configure verb-family occurrences', () => {
    expect(inv.metrics.verbFamilyAddCreateSetConfigure).toBe(820);
  });

  it('has zero unclassified and zero canonical collisions', () => {
    expect(inv.metrics.unclassifiedOccurrences).toBe(0);
    expect(inv.metrics.canonicalCollisions).toBe(0);
  });

  it('derives 1,336 canonical definitions (5 true-duplicate reductions, under the 47 ceiling)', () => {
    expect(inv.canonicalDefinitions.length).toBe(1336);
    expect(inv.metrics.actualCanonicalReductions).toBe(5);
    expect(inv.metrics.actualCanonicalReductions).toBeLessThan(inv.metrics.maxExactNameReductions);
  });
});

describe('canonical model: no naive name-only merging', () => {
  const inv = buildInventory();
  const byKey = new Map(inv.occurrences.map((o) => [o.occurrenceKey, o]));

  it('keeps the four delete targets as distinct canonical ids', () => {
    const deletes = [
      'manage_asset:delete',
      'control_actor:delete',
      'manage_level:delete',
      'manage_sequence:delete',
    ].map((k) => byKey.get(k)?.canonicalId);
    expect(new Set(deletes).size).toBe(4);
  });

  it('also keeps build_environment:delete distinct from the other four', () => {
    const build = byKey.get('build_environment:delete')?.canonicalId;
    const others = [
      'manage_asset:delete',
      'control_actor:delete',
      'manage_level:delete',
      'manage_sequence:delete',
    ].map((k) => byKey.get(k)?.canonicalId);
    expect(build).toBeDefined();
    expect(others).not.toContain(build);
  });

  it('proves an intentionally naive name-only classifier fails to keep deletes distinct', () => {
    const naive = [
      'manage_asset:delete',
      'control_actor:delete',
      'manage_level:delete',
      'manage_sequence:delete',
    ].map((k) => naiveNameOnlyCanonicalId('any', k.split(':')[1]));
    // Naive classifier collapses them to one id (wrong); inventory must not.
    expect(new Set(naive).size).toBe(1);
  });

  it('merges a true duplicate (console_command) to one shared canonical id', () => {
    const editor = byKey.get('control_editor:console_command');
    const system = byKey.get('system_control:console_command');
    expect(editor?.canonicalId).toBe('cap:shared:console_command');
    expect(system?.canonicalId).toBe('cap:shared:console_command');
  });

  it('has no duplicate canonical ids and no orphan references', () => {
    const ids = new Set(inv.canonicalDefinitions.map((c) => c.canonicalId));
    expect(ids.size).toBe(inv.canonicalDefinitions.length);
    const occKeys = new Set(inv.occurrences.map((o) => o.occurrenceKey));
    for (const o of inv.occurrences) {
      expect(ids.has(o.canonicalId)).toBe(true);
    }
    for (const c of inv.canonicalDefinitions) {
      for (const key of c.occurrences) expect(occKeys.has(key)).toBe(true);
      for (const key of c.aliases) expect(occKeys.has(key)).toBe(true);
    }
  });
});

describe('schema validation rejects malformed / dirty / stale inputs', () => {
  it('rejects an unknown top-level field', () => {
    const base = clone(generateInventory()) as Record<string, unknown>;
    base.unexpectedField = 1;
    expect(() => validateInventoryData(base)).toThrow();
  });

  it('rejects an unknown occurrence field', () => {
    const base = clone(generateInventory()) as NormalizationInventory;
    const mutated = clone(base) as Record<string, unknown>;
    const occ = (mutated.occurrences as Record<string, unknown>[])[0];
    occ.bogus = true;
    expect(() => validateInventoryData(mutated)).toThrow();
  });

  it('rejects a duplicate occurrence key', () => {
    const base = clone(generateInventory()) as NormalizationInventory;
    const mutated = clone(base) as Record<string, unknown>;
    const occs = mutated.occurrences as Record<string, unknown>[];
    occs.push({ ...occs[0] });
    expect(() => validateInventoryData(mutated)).toThrow();
  });

  it('rejects the removed P classification (taxonomy is strictly A-F)', () => {
    const base = clone(generateInventory()) as NormalizationInventory;
    const mutated = clone(base) as Record<string, unknown>;
    const occs = mutated.occurrences as Record<string, unknown>[];
    occs[0].classification = 'P';
    expect(() => validateInventoryData(mutated)).toThrow();
  });

  it('rejects any other invalid classification', () => {
    const base = clone(generateInventory()) as NormalizationInventory;
    const mutated = clone(base) as Record<string, unknown>;
    const occs = mutated.occurrences as Record<string, unknown>[];
    occs[0].classification = 'Z';
    expect(() => validateInventoryData(mutated)).toThrow();
  });

  it('classifies every occurrence strictly A-F with no P and correct totals', () => {
    const inv = buildInventory();
    const cc = inv.metrics.classificationCounts;
    expect('P' in cc).toBe(false);
    expect(cc.A).toBe(10);
    expect(cc.E).toBe(8);
    const sum = cc.A + cc.B + cc.C + cc.D + cc.E + cc.F;
    expect(sum).toBe(1341);
  });

  it('rejects missing evidence', () => {
    const base = clone(generateInventory()) as NormalizationInventory;
    const mutated = clone(base) as Record<string, unknown>;
    const occs = mutated.occurrences as Record<string, unknown>[];
    delete (occs[0].evidence as Record<string, unknown>).source;
    expect(() => validateInventoryData(mutated)).toThrow();
  });

  it('rejects a metric block inconsistent with the occurrences (stale/dirty)', () => {
    const base = clone(generateInventory()) as NormalizationInventory;
    const mutated = clone(base) as Record<string, unknown>;
    const metrics = mutated.metrics as Record<string, unknown>;
    metrics.occurrenceCount = (metrics.occurrenceCount as number) + 1;
    expect(() => validateInventoryData(mutated)).toThrow();
  });

  it('rejects nondeterministic ordering', () => {
    const base = clone(generateInventory()) as NormalizationInventory;
    const mutated = clone(base) as Record<string, unknown>;
    const occs = mutated.occurrences as Record<string, unknown>[];
    occs.reverse();
    expect(() => validateInventoryData(mutated)).toThrow();
  });
});
