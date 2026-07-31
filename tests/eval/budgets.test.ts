// tests/eval/budgets.test.ts
// Proves the Task-48 gate MECHANISM: that each declared budget is genuinely
// enforced, that a breach of exactly one budget fails exactly that budget, and
// that the external-model arm blocks rather than fabricates.
//
// A gate nobody has watched fail is not a gate. Every budget below is breached
// individually and asserted to fail.

import { describe, expect, it } from 'vitest';
import {
  type BudgetId,
  type BudgetObservation,
  BudgetError,
  buildTask48Budgets,
  evaluateBudgets,
  failedBudgets,
  KIB,
  MIB,
  TASK48_STATIC_BUDGETS,
} from './budgets.js';
import {
  MODEL_RUNNER_CLIENT,
  reportLeaksSecret,
  resolveModelConfig,
  runModelEvaluation,
} from './model-runner.js';

const BASELINE_TOP1 = 1;

/** A measurement set in which every declared budget holds with margin. */
function passingObservations(): BudgetObservation[] {
  return [
    { id: 'retrieval.topKRecall', observed: 1 },
    { id: 'retrieval.top1Accuracy', observed: 1 },
    { id: 'retrieval.corpusScorerTop1NotBelowBaseline', observed: 1 },
    { id: 'retrieval.guidedRecoveryRate', observed: 1 },
    { id: 'retrieval.unavailableFilterRate', observed: 1 },
    { id: 'retrieval.destructiveFalseAutoSelections', observed: 0 },
    { id: 'latency.warmSearchP95Ms', observed: 1 },
    { id: 'latency.describeP95Ms', observed: 1 },
    { id: 'latency.validationP95Ms', observed: 1 },
    { id: 'memory.indexBytes', observed: 1 * MIB },
    { id: 'payload.searchBytes', observed: 1 * KIB },
    { id: 'payload.describeBytes', observed: 1 * KIB },
    { id: 'payload.medianDescribeUnionRatio', observed: 0.1 },
  ];
}

function withBreach(id: BudgetId, observed: number): BudgetObservation[] {
  return passingObservations().map((entry) => (entry.id === id ? { id, observed } : entry));
}

const BREACHES: readonly (readonly [BudgetId, number, string])[] = Object.freeze([
  ['retrieval.topKRecall', 0.9799, 'top-K recall just under 98%'],
  ['retrieval.top1Accuracy', 0.8999, 'top-1 just under 90%'],
  ['retrieval.corpusScorerTop1NotBelowBaseline', 0.99, 'corpus-scorer top-1 below the recorded baseline'],
  ['retrieval.guidedRecoveryRate', 0.9699, 'guided recovery just under 97%'],
  ['retrieval.unavailableFilterRate', 0.999, 'one unavailable capability leaked'],
  ['retrieval.destructiveFalseAutoSelections', 1, 'a single destructive auto-selection'],
  ['latency.warmSearchP95Ms', 50.5, 'warm search p95 over 50 ms'],
  ['latency.describeP95Ms', 25.5, 'describe p95 over 25 ms'],
  ['latency.validationP95Ms', 10.5, 'validation p95 over 10 ms'],
  ['memory.indexBytes', 25 * MIB + 1, 'index memory over 25 MiB'],
  ['payload.searchBytes', 32 * KIB + 1, 'a search response over 32 KiB'],
  ['payload.describeBytes', 64 * KIB + 1, 'a describe response over 64 KiB'],
  ['payload.medianDescribeUnionRatio', 0.5001, 'median describe over half the union dump'],
]);

describe('task 48 budget table', () => {
  it('Given the plan thresholds, When the budget table is built, Then all thirteen gates are declared exactly once', () => {
    const specs = buildTask48Budgets(BASELINE_TOP1);

    expect(specs).toHaveLength(13);
    expect(TASK48_STATIC_BUDGETS).toHaveLength(12);
    expect(new Set(specs.map((spec) => spec.id)).size).toBe(13);
  });

  it('Given the declared table, When thresholds are read, Then each matches the plan text exactly', () => {
    const byId = new Map(buildTask48Budgets(BASELINE_TOP1).map((spec) => [spec.id, spec]));

    expect(byId.get('retrieval.topKRecall')?.threshold).toBe(0.98);
    expect(byId.get('retrieval.top1Accuracy')?.threshold).toBe(0.9);
    expect(byId.get('retrieval.guidedRecoveryRate')?.threshold).toBe(0.97);
    expect(byId.get('retrieval.unavailableFilterRate')?.threshold).toBe(1);
    expect(byId.get('retrieval.destructiveFalseAutoSelections')?.threshold).toBe(0);
    expect(byId.get('latency.warmSearchP95Ms')?.threshold).toBe(50);
    expect(byId.get('latency.describeP95Ms')?.threshold).toBe(25);
    expect(byId.get('latency.validationP95Ms')?.threshold).toBe(10);
    expect(byId.get('memory.indexBytes')?.threshold).toBe(26_214_400);
    expect(byId.get('payload.searchBytes')?.threshold).toBe(32_768);
    expect(byId.get('payload.describeBytes')?.threshold).toBe(65_536);
    expect(byId.get('payload.medianDescribeUnionRatio')?.threshold).toBe(0.5);
  });

  it('Given a fully compliant measurement set, When budgets are evaluated, Then nothing fails', () => {
    const results = evaluateBudgets(buildTask48Budgets(BASELINE_TOP1), passingObservations());

    expect(failedBudgets(results)).toEqual([]);
    expect(results.every((result) => result.passed)).toBe(true);
  });
});

describe('task 48 injected budget breaches', () => {
  for (const [id, observed, label] of BREACHES) {
    it(`Given ${label}, When budgets are evaluated, Then exactly ${id} fails`, () => {
      const results = evaluateBudgets(buildTask48Budgets(BASELINE_TOP1), withBreach(id, observed));
      const failed = failedBudgets(results);

      expect(failed.map((result) => result.id)).toEqual([id]);
      expect(failed[0]?.observed).toBe(observed);
    });
  }

  it('Given a declared budget with no observation, When evaluated, Then the gate refuses instead of passing', () => {
    const partial = passingObservations().filter((entry) => entry.id !== 'payload.describeBytes');

    expect(() => evaluateBudgets(buildTask48Budgets(BASELINE_TOP1), partial)).toThrow(BudgetError);
  });

  it('Given an observation for an undeclared budget, When evaluated, Then the gate refuses', () => {
    const stray = [
      ...passingObservations(),
      { id: 'retrieval.invented' as BudgetId, observed: 1 },
    ];

    expect(() => evaluateBudgets(buildTask48Budgets(BASELINE_TOP1), stray)).toThrow(BudgetError);
  });

  it('Given a NaN measurement, When evaluated, Then the gate refuses rather than silently comparing', () => {
    expect(() => evaluateBudgets(
      buildTask48Budgets(BASELINE_TOP1),
      withBreach('latency.describeP95Ms', Number.NaN),
    )).toThrow(BudgetError);
  });

  it('Given a baseline outside [0,1], When the table is built, Then it refuses to lower the bar', () => {
    expect(() => buildTask48Budgets(1.5)).toThrow(BudgetError);
    expect(() => buildTask48Budgets(Number.NaN)).toThrow(BudgetError);
  });
});

describe('task 48 external model arm', () => {
  it('Given no model configuration, When the runner is asked, Then it blocks and claims no accuracy', () => {
    const report = runModelEvaluation({});

    expect(report.status).toBe('BLOCKED_EXTERNAL');
    if (report.status !== 'BLOCKED_EXTERNAL') throw new Error('expected a blocked report');
    expect(report.reason).toBe('NOT_ENABLED');
    expect(Object.keys(report)).not.toContain('top1Accuracy');
  });

  it('Given enablement but a named credential variable that is empty, When resolved, Then it blocks on the credential', () => {
    const resolution = resolveModelConfig({
      TASK48_MODEL_ENABLE: '1',
      TASK48_MODEL_PROVIDER: 'example',
      TASK48_MODEL_ID: 'example-model',
      TASK48_MODEL_API_KEY_ENV: 'EXAMPLE_KEY',
    });

    expect(resolution.kind).toBe('blocked');
    if (resolution.kind !== 'blocked') throw new Error('expected blocked');
    expect(resolution.reason).toBe('API_KEY_NOT_SET');
  });

  it('Given a full configuration but no injected transport, When run, Then it blocks because this module performs no network I/O', () => {
    const report = runModelEvaluation({
      TASK48_MODEL_ENABLE: '1',
      TASK48_MODEL_PROVIDER: 'example',
      TASK48_MODEL_ID: 'example-model',
      TASK48_MODEL_API_KEY_ENV: 'EXAMPLE_KEY',
      EXAMPLE_KEY: 'sk-not-a-real-secret-value',
    });

    expect(report.status).toBe('BLOCKED_EXTERNAL');
    if (report.status !== 'BLOCKED_EXTERNAL') throw new Error('expected a blocked report');
    expect(report.reason).toBe('NO_INVOKER_SUPPLIED');
  });

  it('Given a configured run with an injected transport, When evaluated, Then it identifies model/client/temperature and leaks no secret', () => {
    const secret = 'sk-not-a-real-secret-value';
    const report = runModelEvaluation(
      {
        TASK48_MODEL_ENABLE: '1',
        TASK48_MODEL_PROVIDER: 'example',
        TASK48_MODEL_ID: 'example-model',
        TASK48_MODEL_TEMPERATURE: '0',
        TASK48_MODEL_API_KEY_ENV: 'EXAMPLE_KEY',
        EXAMPLE_KEY: secret,
      },
      () => ['some.capability'],
    );

    expect(report.status).toBe('EVALUATED');
    if (report.status !== 'EVALUATED') throw new Error('expected an evaluated report');
    expect(report.client).toBe(MODEL_RUNNER_CLIENT);
    expect(report.provider).toBe('example');
    expect(report.modelId).toBe('example-model');
    expect(report.temperature).toBe(0);
    expect(report.apiKeyEnvVar).toBe('EXAMPLE_KEY');
    expect(report.apiKeyPresent).toBe(true);
    expect(reportLeaksSecret(report, [secret])).toBe(false);
  });

  it('Given a model report, When serialized, Then it carries case IDs but no corpus prompt text', () => {
    const report = runModelEvaluation(
      {
        TASK48_MODEL_ENABLE: '1',
        TASK48_MODEL_PROVIDER: 'example',
        TASK48_MODEL_ID: 'example-model',
        TASK48_MODEL_API_KEY_ENV: 'EXAMPLE_KEY',
        EXAMPLE_KEY: 'k',
      },
      () => [],
    );

    const serialized = JSON.stringify(report);
    expect(serialized).toContain('e.manage_asset');
    expect(serialized).not.toContain('import an asset from an fbx file');
  });
});
