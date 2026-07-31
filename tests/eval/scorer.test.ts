// tests/eval/scorer.test.ts
// Deterministic offline scorer tests: top-1/top-K accuracy, parameter
// completeness, guided recovery, unavailable filtering, ambiguity handling,
// disclosure metrics, shuffled-order determinism, and the near-tie destructive
// guard that must never silently auto-select.

import { resolve } from 'node:path';
import { describe, expect, it } from 'vitest';
import { corpus } from './corpus.js';
import { loadManifestModel, type ManifestModel } from './manifest-model.js';
import { scoreCase, scoreCorpus } from './scorer.js';
import type { Corpus } from './types.js';

const MANIFEST_PATH = resolve(process.cwd(), 'src/gateway/gateway-manifest.generated.json');
const manifest: ManifestModel = loadManifestModel(MANIFEST_PATH);

function shuffled<T>(input: readonly T[]): T[] {
  const arr = [...input];
  let seed = 0x9e3779b9;
  for (let i = arr.length - 1; i > 0; i -= 1) {
    seed = (seed * 1103515245 + 12345) & 0x7fffffff;
    const j = seed % (i + 1);
    const tmp = arr[i];
    arr[i] = arr[j];
    arr[j] = tmp;
  }
  return arr;
}

describe('aggregate score metrics', () => {
  const report = scoreCorpus(corpus, manifest);

  it('covers all 23 parents and 24 collision claims', () => {
    expect(report.metrics.parentCoverage).toHaveLength(23);
    expect(report.metrics.collisionCoverage.length).toBeGreaterThanOrEqual(24);
  });

  it('scores correct top-1 and top-K selection', () => {
    expect(report.metrics.top1Accuracy).toBe(1);
    expect(report.metrics.topKAccuracy).toBe(1);
  });

  it('scores parameter completeness and guided recovery', () => {
    expect(report.metrics.paramCompletenessRate).toBe(1);
    expect(report.metrics.guidedRecoveryRate).toBe(1);
  });

  it('filters unavailable capabilities and surfaces ambiguity', () => {
    expect(report.metrics.unavailableFilterRate).toBe(1);
    expect(report.metrics.ambiguityHandledRate).toBe(1);
  });

  it('reports disclosure byte and token metrics', () => {
    expect(report.disclosure.totalBytes).toBeGreaterThan(0);
    expect(report.disclosure.totalTokens).toBeGreaterThan(0);
    expect(report.disclosure.maxBytes).toBeLessThanOrEqual(report.disclosure.totalBytes);
  });
});

describe('shuffled-order determinism', () => {
  it('yields identical hash and metrics regardless of case order', () => {
    const a = scoreCorpus(corpus, manifest);
    const shuffledCorpus: Corpus = { ...corpus, cases: shuffled(corpus.cases) };
    const b = scoreCorpus(shuffledCorpus, manifest);
    expect(b.corpusHash).toBe(a.corpusHash);
    expect(b.reportHash).toBe(a.reportHash);
    expect(b.metrics).toEqual(a.metrics);
    expect(b.disclosure).toEqual(a.disclosure);
  });

  it('is stable across repeated runs', () => {
    const a = scoreCorpus(corpus, manifest);
    const b = scoreCorpus(corpus, manifest);
    expect(b.reportHash).toBe(a.reportHash);
  });
});

describe('guided second-attempt recovery', () => {
  for (const id of ['e.manage_asset', 'e.control_actor']) {
    it(`recovers on the second attempt for ${id}`, () => {
      const entry = corpus.cases.find((c) => c.id === id);
      expect(entry).toBeDefined();
      const score = scoreCase(entry as (typeof corpus.cases)[number], manifest);
      expect(score.guidedRecovery).toBe(true);
      expect(score.paramCompleteness).toBe(true);
    });
  }
});

describe('ambiguous intents surface alternatives', () => {
  for (const id of ['p.manage_blueprint.widget', 'a.control_actor.findclass']) {
    it(`handles ambiguity for ${id}`, () => {
      const entry = corpus.cases.find((c) => c.id === id);
      const score = scoreCase(entry as (typeof corpus.cases)[number], manifest);
      expect(score.ambiguityHandled).toBe(true);
      expect(score.elicitation).toBe('ambiguous');
      expect(score.autoSelected).toBe(true);
    });
  }
});

describe('version / plugin negative intents are withheld', () => {
  for (const id of ['v.pcg.ue51', 'pl.audio.metasound']) {
    it(`withholds unavailable capability for ${id}`, () => {
      const entry = corpus.cases.find((c) => c.id === id);
      const score = scoreCase(entry as (typeof corpus.cases)[number], manifest);
      expect(score.unavailableFiltered).toBe(true);
      expect(score.top1Correct).toBe(true);
      expect(score.elicitation).toBe('unavailable');
      expect(score.autoSelected).toBe(false);
    });
  }
});

describe('near-tie destructive intent never auto-selects', () => {
  const entry = corpus.cases.find((c) => c.id === 'n.near_tie_del');
  const score = scoreCase(entry as (typeof corpus.cases)[number], manifest);
  it('refuses to auto-select', () => {
    expect(score.autoSelected).toBe(false);
  });
  it('requires confirmation/guidance instead', () => {
    expect(score.ambiguityHandled).toBe(true);
    expect(score.elicitation).toBe('confirm');
  });
  it('still recognizes the expected capability among the tied set', () => {
    expect(score.expectedInTiedSet).toBe(true);
    expect(score.top1Correct).toBe(true);
  });
});

describe('token budget logic', () => {
  it('flags a case whose envelope exceeds its budget', () => {
    const tiny: Corpus = {
      schema: 'omo.eval.corpus.v1',
      version: '1',
      cases: [
        {
          id: 'budget.tight',
          intent: 'status',
          kind: 'exact',
          expected: { tool: 'manage_tools', action: 'get_status' },
          requiredParams: ['a', 'b', 'c', 'd'],
          allowedAlternatives: [],
          elicitationExpectation: 'none',
          tokenBudget: 1,
        },
      ],
    };
    const report = scoreCorpus(tiny, manifest);
    expect(report.metrics.tokenBudgetComplianceRate).toBe(0);
    expect(report.perCase[0]?.tokenBudgetCompliance).toBe(false);
  });

  it('complies when the budget is sufficient', () => {
    const roomy: Corpus = {
      schema: 'omo.eval.corpus.v1',
      version: '1',
      cases: [
        {
          id: 'budget.roomy',
          intent: 'status',
          kind: 'exact',
          expected: { tool: 'manage_tools', action: 'get_status' },
          requiredParams: ['a'],
          allowedAlternatives: [],
          elicitationExpectation: 'none',
          tokenBudget: 500,
        },
      ],
    };
    const report = scoreCorpus(roomy, manifest);
    expect(report.perCase[0]?.tokenBudgetCompliance).toBe(true);
  });
});
