// tests/eval/gate.test.ts
// The Task-48 release gate applied to the real measurement, one assertion per
// declared budget so a breach names itself instead of collapsing the whole
// suite into a single red result.
//
// The gate proper is the pair at the end: the failing budgets must be exactly
// the reviewed ledger in `known-breaches.ts`, which is currently empty,
// so the verdict must be PASS. Should a budget ever break, the ledger keeps the
// difference between an investigated breach and a fresh regression explicit.

import { beforeAll, describe, expect, it } from 'vitest';
import { sha256Canonical } from './hash.js';
import type { BudgetResult } from './budgets.js';
import {
  knownBreachFor,
  knownBreachIds,
  staleLedgerEntries,
  unreviewedBreaches,
} from './known-breaches.js';
import { buildTask48Report, type Task48Report } from './report.js';
import { GATEWAY_DEFAULT_SEARCH_LIMIT, retrievalCases } from './fixtures.js';
import { measurePayload, unionBaselineBytes, jsonBytes } from './measure-payload.js';
import { measureDestructiveAutoSelection, measureRetrieval } from './measure-retrieval.js';
import { describeGatewayCapability } from '../../src/server/gateway/gateway-describe.js';
import { finalRegistryRecords } from './fixtures.js';

const BUILD_TIMEOUT_MS = 600_000;

let report: Task48Report;

beforeAll(async () => {
  report = buildTask48Report({ runtimeScale: 0.25 });
}, BUILD_TIMEOUT_MS);

function budget(id: string): BudgetResult {
  const found = report.budgets.find((entry) => entry.id === id);
  if (found === undefined) throw new Error(`budget ${id} was not evaluated`);
  return found;
}

describe('task 48 report provenance', () => {
  it('Given a built report, When provenance is read, Then environment, tree and corpus hashes are all present', () => {
    expect(report.treeHash).toMatch(/^[0-9a-f]{64}$/u);
    expect(report.deterministicHash).toMatch(/^[0-9a-f]{64}$/u);
    expect(report.corpusScorer.corpusHash).toMatch(/^[0-9a-f]{64}$/u);
    expect(report.treeHashInputCount).toBeGreaterThan(0);
    expect(report.environment.nodeVersion.length).toBeGreaterThan(0);
    expect(report.registryRecordCount).toBeGreaterThan(1000);
  });

  it('Given the reviewed Task-4 corpus, When Task 48 rescores it, Then the corpus hash still matches the recorded baseline', () => {
    expect(report.corpusScorer.corpusHash).toBe(report.baseline.corpusHash);
  });

  it('Given latency samples, When percentiles are reported, Then each carries its sample count and ordered quantiles', () => {
    const search = report.runtime.warmSearch.percentilesMs;

    expect(search.sampleCount).toBeGreaterThan(0);
    expect(report.runtime.warmSearch.warmupRuns).toBeGreaterThan(0);
    expect(search.p50).toBeLessThanOrEqual(search.p95);
    expect(search.p95).toBeLessThanOrEqual(search.max);
  });

  it('Given repeated measurement, When the deterministic sections are recomputed, Then they are byte-identical', () => {
    const first = sha256Canonical({ retrieval: measureRetrieval(), payload: measurePayload() });
    const second = sha256Canonical({ retrieval: measureRetrieval(), payload: measurePayload() });

    expect(second).toBe(first);
  }, BUILD_TIMEOUT_MS);
});

describe('task 48 adversarial classes', () => {
  it('Given a degraded ranker, When retrieval is measured, Then top-1 and recall both collapse', () => {
    const degraded = measureRetrieval(() => []);

    expect(degraded.top1Accuracy).toBe(0);
    expect(degraded.topKRecall).toBe(0);
  }, BUILD_TIMEOUT_MS);

  it('Given a ranker that answers every intent with one wrong capability, When measured, Then recall detects it', () => {
    const wrong = measureRetrieval(() => ['definitely.not.a.capability']);

    expect(wrong.top1Accuracy).toBe(0);
    expect(wrong.topKRecall).toBe(0);
  }, BUILD_TIMEOUT_MS);

  it('Given a schema dump, When compared to the shipped describe, Then the union baseline is far larger', () => {
    const record = finalRegistryRecords()[0];
    if (record === undefined) throw new Error('registry is empty');
    const actual = describeGatewayCapability({ operation: 'describe', capability: record.id });
    const union = unionBaselineBytes(record, actual);

    expect(union).toBeDefined();
    expect(union ?? 0).toBeGreaterThan(jsonBytes(actual));
  });

  it('Given the destructive corpus intents, When the shipping retriever decides, Then it never auto-selects a destructive capability', () => {
    const destructive = measureDestructiveAutoSelection();

    expect(destructive.evaluated).toBeGreaterThan(0);
    expect(destructive.violations).toBe(0);
  }, BUILD_TIMEOUT_MS);

  it('Given no configured model, When the report is built, Then the external claim is BLOCKED_EXTERNAL and never a score', () => {
    expect(report.model.status).toBe('BLOCKED_EXTERNAL');
  });

  it('Given the gated top-K, When read, Then it is the gateway default search page rather than a smaller flattering K', () => {
    expect(report.retrieval.gatedK).toBe(GATEWAY_DEFAULT_SEARCH_LIMIT);
    expect(report.retrieval.eligibleCases).toBe(retrievalCases().length);
  });
});

describe('task 48 budget enforcement', () => {
  it('Given the final registry, When top-K recall is measured, Then it is at least 98%', () => {
    expect(budget('retrieval.topKRecall').passed).toBe(true);
  });

  it('Given the recorded Task-4 baseline, When the corpus scorer reruns, Then top-1 has not regressed', () => {
    expect(budget('retrieval.corpusScorerTop1NotBelowBaseline').passed).toBe(true);
  });

  it('Given guided second attempts, When measured, Then recovery is at least 97%', () => {
    expect(budget('retrieval.guidedRecoveryRate').passed).toBe(true);
  });

  it('Given unavailable capabilities, When filtered, Then filtering is total', () => {
    expect(budget('retrieval.unavailableFilterRate').passed).toBe(true);
  });

  it('Given destructive intents, When selection runs, Then there are zero false auto-selections', () => {
    expect(budget('retrieval.destructiveFalseAutoSelections').passed).toBe(true);
  });

  it('Given a warm index, When search runs, Then p95 is within 50 ms', () => {
    expect(budget('latency.warmSearchP95Ms').passed).toBe(true);
  });

  it('Given a warm registry, When describe runs, Then p95 is within 25 ms', () => {
    expect(budget('latency.describeP95Ms').passed).toBe(true);
  });

  it('Given example inputs, When execute-input validation runs, Then p95 is within 10 ms', () => {
    expect(budget('latency.validationP95Ms').passed).toBe(true);
  });

  it('Given the built search index, When retained heap is measured, Then it is within 25 MiB', () => {
    expect(budget('memory.indexBytes').passed).toBe(true);
  });

  it('Given corpus intents, When search responses are sized, Then none exceeds 32 KiB', () => {
    expect(budget('payload.searchBytes').passed).toBe(true);
  });

  it('Given every capability, When describe responses are sized, Then none exceeds 64 KiB', () => {
    expect(budget('payload.describeBytes').passed).toBe(true);
  });

  it('Given the union dump baseline, When describe bytes are compared, Then the median is at most half', () => {
    expect(budget('payload.medianDescribeUnionRatio').passed).toBe(true);
  });

  it('Given the top-1 budget, When its declaration is read, Then the threshold is still the full 90% the plan requires', () => {
    const top1 = budget('retrieval.top1Accuracy');

    expect(top1.threshold).toBe(0.9);
    expect(top1.direction).toBe('atLeast');
    expect(top1.kind).toBe('deterministic');
  });

  it('Given the reviewed ledger, When the failing budgets are audited, Then no breach is unreviewed and no entry is stale', () => {
    expect(unreviewedBreaches(report.failedBudgetIds)).toEqual([]);
    expect(staleLedgerEntries(report.failedBudgetIds)).toEqual([]);
  });

  it('Given an empty ledger, When the gate renders a verdict, Then every declared budget passed on its own merits', () => {
    expect(knownBreachIds()).toEqual([]);
    expect(report.failedBudgetIds).toEqual([]);
    expect(report.verdict).toBe('PASS');
  });

  it('Given each known breach, When the ledger is audited, Then it names a root cause and the remedies that were measured', () => {
    for (const id of knownBreachIds()) {
      const breach = knownBreachFor(id);
      if (breach === undefined) throw new Error(`unlisted breach ${id}`);

      expect(breach.rootCause.length).toBeGreaterThan(80);
      expect(breach.whyNotClosable.length).toBeGreaterThan(80);
      expect(breach.measuredRemedies.length).toBeGreaterThan(0);
    }
  });

  it('Given the final registry, When top-1 accuracy is measured, Then it clears the full 90% bar', () => {
    expect(budget('retrieval.top1Accuracy').passed).toBe(true);
  });
});
