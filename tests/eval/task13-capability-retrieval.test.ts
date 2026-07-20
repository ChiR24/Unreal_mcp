import { resolve } from 'node:path';
import { describe, expect, it } from 'vitest';

import { corpus } from './corpus.js';
import { loadManifestModel } from './manifest-model.js';
import { scoreCorpus } from './scorer.js';
import {
  evaluateTask13Retrieval,
  evaluateTask13UnavailableFiltering,
} from './task13-capability-adapter.js';

const MANIFEST_PATH = resolve(process.cwd(), 'src/gateway/gateway-manifest.generated.json');

describe('Task 13 retrieval evaluation', () => {
  it('Given the neutral Task 4 adapter, When all resolvable pilot intents are ranked, Then corpus-accepted top-1 and exact top-K preserve the Task 4 baseline', () => {
    const baseline = scoreCorpus(corpus, loadManifestModel(MANIFEST_PATH));
    const report = evaluateTask13Retrieval();

    expect(report.eligibleCases).toBe(15);
    expect(report.strictTop1Accuracy).toBeGreaterThan(0);
    expect(report.top1Accuracy).toBeGreaterThanOrEqual(baseline.metrics.top1Accuracy);
    expect(report.topKRecall).toBeGreaterThanOrEqual(baseline.metrics.topKAccuracy);
  });

  it('Given the same corpus and catalog, When evaluation repeats, Then every ranked vector is byte-identical', () => {
    const first = evaluateTask13Retrieval();
    const second = evaluateTask13Retrieval();

    expect(JSON.stringify(second)).toBe(JSON.stringify(first));
  });

  it('Given bounded top-K retrieval, When the adapter captures vectors, Then no case discloses more than five canonical IDs', () => {
    const report = evaluateTask13Retrieval();

    expect(report.perCase.every((entry) => entry.rankedCapabilityIds.length <= 5)).toBe(true);
  });

  it('Given every stage-one exclusion dimension, When unavailable candidates are measured, Then filtering is exactly 100 percent', () => {
    const report = evaluateTask13UnavailableFiltering();

    expect(report.evaluatedUnavailableCandidates).toBeGreaterThan(0);
    expect(report.leakedCandidates).toBe(0);
    expect(report.filterRate).toBe(1);
  });
});
