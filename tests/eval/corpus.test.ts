// tests/eval/corpus.test.ts
// Strict validation tests for the golden corpus: schema/parse fail-closed,
// manifest drift detection, and full parent + collision coverage.

import { resolve } from 'node:path';
import { describe, expect, it } from 'vitest';
import {
  assertCollisionCoverage,
  assertFullParentCoverage,
  corpus,
  parseCorpus,
  validateCorpus,
} from './corpus.js';
import { CorpusValidationError } from './errors.js';
import { loadManifestModel, type ManifestModel } from './manifest-model.js';
import type { Corpus } from './types.js';

const MANIFEST_PATH = resolve(process.cwd(), 'src/gateway/gateway-manifest.generated.json');
const manifest: ManifestModel = loadManifestModel(MANIFEST_PATH);

const minimalCase = {
  id: 'x',
  intent: 'do a thing',
  kind: 'exact',
  expected: { tool: 'manage_tools', action: 'list_tools' },
  requiredParams: [],
  allowedAlternatives: [],
  elicitationExpectation: 'none',
  tokenBudget: 80,
} as const;

describe('corpus parse (fail-closed)', () => {
  it('parses the bundled golden corpus', () => {
    expect(corpus.cases.length).toBeGreaterThanOrEqual(23);
    expect(corpus.schema).toBe('omo.eval.corpus.v1');
  });

  it('rejects a wrong schema', () => {
    expect(() => parseCorpus({ schema: 'wrong', version: '1', cases: [] })).toThrow(CorpusValidationError);
  });

  it('rejects a non-array cases field', () => {
    expect(() => parseCorpus({ schema: 'omo.eval.corpus.v1', version: '1', cases: {} })).toThrow(CorpusValidationError);
  });

  it('rejects a duplicate case id', () => {
    const input = {
      schema: 'omo.eval.corpus.v1',
      version: '1',
      cases: [minimalCase, { ...minimalCase, intent: 'other' }],
    };
    expect(() => parseCorpus(input)).toThrow(CorpusValidationError);
  });

  it('rejects an invalid case kind', () => {
    const input = {
      schema: 'omo.eval.corpus.v1',
      version: '1',
      cases: [{ ...minimalCase, kind: 'bogus' }],
    };
    expect(() => parseCorpus(input)).toThrow(CorpusValidationError);
  });

  it('rejects an invalid elicitation expectation', () => {
    const input = {
      schema: 'omo.eval.corpus.v1',
      version: '1',
      cases: [{ ...minimalCase, elicitationExpectation: 'maybe' }],
    };
    expect(() => parseCorpus(input)).toThrow(CorpusValidationError);
  });

  it('rejects a malformed capability ref', () => {
    const input = {
      schema: 'omo.eval.corpus.v1',
      version: '1',
      cases: [{ ...minimalCase, expected: { tool: 'manage_tools' } }],
    };
    expect(() => parseCorpus(input)).toThrow(CorpusValidationError);
  });

  it('rejects an unknown corpus case field (drift guard)', () => {
    const input = {
      schema: 'omo.eval.corpus.v1',
      version: '1',
      cases: [{ ...minimalCase, inventedTarget: true }],
    };
    expect(() => parseCorpus(input)).toThrow(CorpusValidationError);
  });
});

describe('corpus/manifest drift detection', () => {
  it('validates against the real gateway manifest', () => {
    expect(() => validateCorpus(corpus, manifest)).not.toThrow();
  });

  it('fails when a referenced tool is absent from the manifest', () => {
    const drifted: ManifestModel = {
      version: manifest.version,
      source: manifest.source,
      tools: manifest.tools.filter((t) => t.name !== 'manage_asset'),
    };
    expect(() => validateCorpus(corpus, drifted)).toThrow(CorpusValidationError);
  });
});

describe('coverage assertions', () => {
  it('asserts full 23-parent coverage', () => {
    expect(() => assertFullParentCoverage(corpus, manifest)).not.toThrow();
    const covered = new Set(corpus.cases.map((c) => c.expected.tool));
    expect(covered.size).toBe(23);
  });

  it('throws when a parent is missing', () => {
    const missingOne: Corpus = {
      schema: 'omo.eval.corpus.v1',
      version: '1',
      cases: corpus.cases.filter((c) => c.expected.tool !== 'manage_tools'),
    };
    expect(() => assertFullParentCoverage(missingOne, manifest)).toThrow(CorpusValidationError);
  });

  it('asserts collision coverage for every reviewed claim', () => {
    const ids = assertCollisionCoverage(corpus);
    expect(ids.length).toBeGreaterThanOrEqual(24);
  });
});

describe('manifest loader', () => {
  it('rejects a nonexistent manifest path', () => {
    expect(() => loadManifestModel('/no/such/manifest.json')).toThrow();
  });

  it('rejects a nonexistent non-JSON manifest path', () => {
    expect(() => loadManifestModel(resolve(process.cwd(), 'tests/eval/_bad_manifest.json'))).toThrow();
  });
});
