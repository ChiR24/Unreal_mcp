/**
 * tests/unit/canonical-registry-drift.test.ts
 *
 * RED first: a mutated copy of the committed neutral canonical-registry JSON
 * MUST be reported by compareCanonicalRegistry() with the EXACT canonical id
 * and JSON pointer of the first mismatch. This proves the drift detector is
 * not a rubber-stamp and that top-level fields (catalogRevision, recordCount,
 * lexicalIndex, migrationData, aliasData, docsData) are compared, not just the
 * summaries list. GREEN: the committed artifact compares clean against a
 * freshly cloned copy (no entries).
 */
import { describe, expect, it } from 'vitest';
import { readFileSync, readdirSync } from 'node:fs';
import { resolve } from 'node:path';
import { compareCanonicalRegistry } from '../../scripts/generate-canonical-registry.js';
import { ALL_CAPABILITY_RECORD_COUNT } from '../../src/tools/catalog/capabilities/records/aggregate.js';

const JSON_PATH = resolve(process.cwd(), 'src/tools/catalog/capabilities/generated/canonical-registry.generated.json');

interface Summary {
  id: string;
  schemaHash: string;
  contentHash: string;
  parentTool: string;
  dispatchAction: string;
  domain: string;
}

interface Registry {
  catalogRevision: string;
  recordCount: number;
  summaries: Summary[];
  lexicalIndex: Record<string, string[]>;
  migrationData: Record<string, unknown>;
  aliasData: Record<string, unknown>;
  docsData: unknown[];
}

function loadRegistry(): Registry {
  return JSON.parse(readFileSync(JSON_PATH, 'utf8')) as Registry;
}

describe('canonical registry drift detection', () => {
  it('a mutated schemaHash is reported with exact id and JSON pointer', () => {
    const base = loadRegistry();
    const mutated = structuredClone(base);
    const targetIndex = 3;
    const targetId = mutated.summaries[targetIndex].id;
    const original = mutated.summaries[targetIndex].schemaHash;
    mutated.summaries[targetIndex].schemaHash = '0'.repeat(original.length);

    const drift = compareCanonicalRegistry(base, mutated);
    expect(drift.length).toBeGreaterThan(0);
    expect(drift[0].id).toBe(targetId);
    expect(drift[0].pointer).toBe(`/summaries/${targetIndex}/schemaHash`);
  });

  it('a dropped record is reported with exact id and summary pointer', () => {
    const base = loadRegistry();
    const mutated = structuredClone(base);
    const dropped = mutated.summaries.pop();
    expect(dropped).toBeDefined();
    if (dropped === undefined) {
      throw new TypeError('Expected the registry fixture to contain a summary');
    }
    const expectedIndex = base.summaries.findIndex((s) => s.id === dropped.id);
    if (expectedIndex < 0) {
      throw new TypeError('Expected the dropped id to exist in the base registry');
    }

    const drift = compareCanonicalRegistry(base, mutated);
    expect(drift.some((d) => d.id === dropped.id && d.pointer === `/summaries/${expectedIndex}`)).toBe(true);
  });

  it('a mutated catalogRevision is reported with exact id and pointer', () => {
    const base = loadRegistry();
    const mutated = structuredClone(base);
    mutated.catalogRevision = '0'.repeat(base.catalogRevision.length);

    const drift = compareCanonicalRegistry(base, mutated);
    expect(drift.some((d) => d.id === 'catalogRevision' && d.pointer === '/catalogRevision')).toBe(true);
  });

  it('a mutated recordCount is reported with exact id and pointer', () => {
    const base = loadRegistry();
    const mutated = structuredClone(base);
    mutated.recordCount = base.recordCount + 1;

    const drift = compareCanonicalRegistry(base, mutated);
    expect(drift.some((d) => d.id === 'recordCount' && d.pointer === '/recordCount')).toBe(true);
  });

  it('a mutated lexicalIndex entry is reported with exact id and pointer', () => {
    const base = loadRegistry();
    const mutated = structuredClone(base);
    const firstKey = Object.keys(mutated.lexicalIndex)[0];
    if (firstKey === undefined) {
      throw new TypeError('Expected the lexicalIndex fixture to contain an entry');
    }
    const originalLength = mutated.lexicalIndex[firstKey].length;
    mutated.lexicalIndex[firstKey] = [...mutated.lexicalIndex[firstKey], '__drift_marker__'];
    const expectedPointer = `/lexicalIndex/${firstKey}/${originalLength}`;

    const drift = compareCanonicalRegistry(base, mutated);
    expect(drift.some((d) => d.id === 'lexicalIndex' && d.pointer === expectedPointer)).toBe(true);
  });

  it('a mutated migrationData field is reported with exact id and pointer', () => {
    const base = loadRegistry();
    const mutated = structuredClone(base);
    if (!mutated.migrationData || typeof mutated.migrationData !== 'object') {
      throw new TypeError('Expected migrationData to be present');
    }
    const originalCount = mutated.migrationData.entryCount;
    mutated.migrationData.entryCount =
      (typeof originalCount === 'number' ? originalCount : 0) + 1;

    const drift = compareCanonicalRegistry(base, mutated);
    expect(drift.some((d) => d.id === 'migrationData' && d.pointer === '/migrationData/entryCount')).toBe(true);
  });

  it('a mutated aliasData field is reported with exact id and pointer', () => {
    const base = loadRegistry();
    const mutated = structuredClone(base);
    if (!mutated.aliasData || typeof mutated.aliasData !== 'object') {
      throw new TypeError('Expected aliasData to be present');
    }
    const originalCount = mutated.aliasData.aliasCount;
    mutated.aliasData.aliasCount =
      (typeof originalCount === 'number' ? originalCount : 0) + 1;

    const drift = compareCanonicalRegistry(base, mutated);
    expect(drift.some((d) => d.id === 'aliasData' && d.pointer === '/aliasData/aliasCount')).toBe(true);
  });

  it('a mutated docsData entry is reported with exact id and pointer', () => {
    const base = loadRegistry();
    const mutated = structuredClone(base);
    if (!Array.isArray(mutated.docsData) || mutated.docsData.length === 0) {
      throw new TypeError('Expected docsData to be a non-empty array');
    }
    (mutated.docsData[0] as { description?: string }).description = '__drift_marker__';

    const drift = compareCanonicalRegistry(base, mutated);
    expect(drift.some((d) => d.id === 'docsData' && d.pointer === '/docsData/0/description')).toBe(true);
  });

  it('an extra summary present only in actual is reported with exact id and pointer', () => {
    const base = loadRegistry();
    const mutated = structuredClone(base);
    const extraId = 'zzz_extra_only_record';
    const extraIndex = mutated.summaries.length;
    mutated.summaries.push({
      id: extraId,
      parentTool: 'manage_asset',
      dispatchAction: 'import_asset',
      domain: 'asset management',
      schemaHash: 'a'.repeat(64),
      contentHash: 'b'.repeat(64),
    });

    const drift = compareCanonicalRegistry(base, mutated);
    expect(drift.some((d) => d.id === extraId && d.pointer === `/summaries/${extraIndex}`)).toBe(true);
  });

  it('a duplicate actual id is not silently collapsed and the extra copy is reported', () => {
    const base = loadRegistry();
    const mutated = structuredClone(base);
    const dupSource = mutated.summaries[0];
    const dupIndex = mutated.summaries.length;
    mutated.summaries.push({
      id: dupSource.id,
      parentTool: dupSource.parentTool,
      dispatchAction: dupSource.dispatchAction,
      domain: dupSource.domain,
      schemaHash: dupSource.schemaHash,
      contentHash: dupSource.contentHash,
    });

    const drift = compareCanonicalRegistry(base, mutated);
    const extraReports = drift.filter((d) => d.id === dupSource.id);
    // The expected match at index 0 is consumed; the unmatched duplicate actual
    // copy must still be reported (not silently collapsed by the id index).
    expect(extraReports.some((d) => d.pointer === `/summaries/${dupIndex}`)).toBe(true);
    expect(extraReports.some((d) => d.pointer === '/summaries/0')).toBe(false);
  });

  it('a malformed (non-object) boundary input is reported deterministically, not thrown', () => {
    const base = loadRegistry();
    const drift = compareCanonicalRegistry(base, 'not-a-registry');
    expect(Array.isArray(drift)).toBe(true);
    expect(drift.some((d) => d.id === 'registry' && d.pointer === '/')).toBe(true);
  });

  it('a nested drift is still reported when an earlier payload also drifted', () => {
    // The four top-level payloads are compared by four sibling pushPointerDiff
    // calls sharing ONE entries array. Guarding recursion on that array's total
    // length let the first drifting payload suppress every later one: the
    // docsData walk returned at element 0 because lexicalIndex had already
    // pushed, so a real mutation deeper in the array was never examined.
    // docsData is the probe precisely because it only drifts BELOW an object
    // boundary -- a payload whose first sorted key is a scalar reports either
    // way and would not exercise the guard at all.
    const base = loadRegistry();
    const mutated = structuredClone(base);
    const lexicalKey = Object.keys(mutated.lexicalIndex)[0];
    mutated.lexicalIndex[lexicalKey] = ['drifted-token'];
    const entry = mutated.docsData[3] as { actionCount: number };
    entry.actionCount = entry.actionCount + 1;

    const drift = compareCanonicalRegistry(base, mutated);

    expect(drift.some((d) => d.id === 'lexicalIndex')).toBe(true);
    expect(drift.some((d) => d.id === 'docsData' && d.pointer === '/docsData/3/actionCount')).toBe(true);
  });

  it('GREEN: the committed artifact compares clean against an equivalent model', () => {
    const base = loadRegistry();
    const copy = structuredClone(base);
    const drift = compareCanonicalRegistry(base, copy);
    expect(drift).toEqual([]);
  });

  it('GREEN: full universe is present (ALL_CAPABILITY_RECORD_COUNT folded records)', () => {
    const base = loadRegistry();
    expect(base.summaries.length).toBe(ALL_CAPABILITY_RECORD_COUNT);
    expect(new Set(base.summaries.map((s) => s.id)).size).toBe(ALL_CAPABILITY_RECORD_COUNT);
  });
});

describe('canonical registry target directory cleanliness', () => {
  it('GREEN: no Task23 temporary *.tmp-* artifacts remain in target directories', () => {
    const generatedDir = resolve(
      process.cwd(),
      'src/tools/catalog/capabilities/generated',
    );
    const dir = readdirSync(generatedDir);
    const tempLeftovers = dir.filter((entry) => entry.includes('.tmp-'));
    expect(tempLeftovers, `leftover temp artifacts: ${tempLeftovers.join(', ')}`).toEqual([]);
  });
});
