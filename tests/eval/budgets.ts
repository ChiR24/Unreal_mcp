// tests/eval/budgets.ts
// Task 48 - the single source of truth for every release-gating budget.
//
// Nothing in this file measures anything. It declares WHAT must hold and in
// which direction, so a measurement module cannot quietly invent a friendlier
// threshold and a report cannot claim a gate that was never declared. The
// evaluator is fail-closed in both directions: a declared budget with no
// observation, and an observation for an undeclared budget, are both errors.
//
// `kind` splits the table into two honest halves. `deterministic` budgets are
// reproducible byte-for-byte on any machine and enter the report's content
// hash. `timing` budgets are wall-clock and machine-dependent; they are
// enforced exactly as hard, but they are excluded from the content hash
// because a hash that flaps with CPU load proves nothing.

export type BudgetDirection = 'atLeast' | 'atMost' | 'exactly';
export type BudgetUnit = 'ratio' | 'count' | 'milliseconds' | 'bytes';
export type BudgetKind = 'deterministic' | 'timing';

export type BudgetId =
  | 'retrieval.topKRecall'
  | 'retrieval.top1Accuracy'
  | 'retrieval.corpusScorerTop1NotBelowBaseline'
  | 'retrieval.guidedRecoveryRate'
  | 'retrieval.unavailableFilterRate'
  | 'retrieval.destructiveFalseAutoSelections'
  | 'latency.warmSearchP95Ms'
  | 'latency.describeP95Ms'
  | 'latency.validationP95Ms'
  | 'memory.indexBytes'
  | 'payload.searchBytes'
  | 'payload.describeBytes'
  | 'payload.medianDescribeUnionRatio';

export type BudgetSpec = {
  readonly id: BudgetId;
  readonly direction: BudgetDirection;
  readonly threshold: number;
  readonly unit: BudgetUnit;
  readonly kind: BudgetKind;
  readonly requirement: string;
};

export type BudgetObservation = {
  readonly id: BudgetId;
  readonly observed: number;
};

export type BudgetResult = {
  readonly id: BudgetId;
  readonly direction: BudgetDirection;
  readonly threshold: number;
  readonly observed: number;
  readonly unit: BudgetUnit;
  readonly kind: BudgetKind;
  readonly passed: boolean;
};

export class BudgetError extends Error {
  readonly name = 'BudgetError' as const;
  constructor(readonly reason: string, readonly detail?: unknown) {
    super(`task48 budget error: ${reason}`);
  }
}

/** Ratio comparisons are exact rationals in practice; this only absorbs IEEE dust. */
export const BUDGET_EPSILON = 1e-9 as const;

export const MIB = 1024 * 1024;
export const KIB = 1024;

/**
 * The twelve budgets whose thresholds are fixed by the plan text. The
 * thirteenth (`retrieval.top1NotBelowBaseline`) is derived from the recorded
 * pre-Task-48 baseline and is appended by `buildTask48Budgets`.
 */
export const TASK48_STATIC_BUDGETS: readonly BudgetSpec[] = Object.freeze([
  {
    id: 'retrieval.topKRecall', direction: 'atLeast', threshold: 0.98,
    unit: 'ratio', kind: 'deterministic',
    requirement: 'top-K recall >= 98%',
  },
  {
    id: 'retrieval.top1Accuracy', direction: 'atLeast', threshold: 0.9,
    unit: 'ratio', kind: 'deterministic',
    requirement: 'top-1 accuracy >= 90%',
  },
  {
    id: 'retrieval.guidedRecoveryRate', direction: 'atLeast', threshold: 0.97,
    unit: 'ratio', kind: 'deterministic',
    requirement: 'guided second-attempt recovery >= 97%',
  },
  {
    id: 'retrieval.unavailableFilterRate', direction: 'exactly', threshold: 1,
    unit: 'ratio', kind: 'deterministic',
    requirement: 'unavailable-capability filtering is 100%',
  },
  {
    id: 'retrieval.destructiveFalseAutoSelections', direction: 'exactly', threshold: 0,
    unit: 'count', kind: 'deterministic',
    requirement: 'destructive false auto-selection count is 0',
  },
  {
    id: 'latency.warmSearchP95Ms', direction: 'atMost', threshold: 50,
    unit: 'milliseconds', kind: 'timing',
    requirement: 'warm search p95 <= 50 ms',
  },
  {
    id: 'latency.describeP95Ms', direction: 'atMost', threshold: 25,
    unit: 'milliseconds', kind: 'timing',
    requirement: 'describe p95 <= 25 ms',
  },
  {
    id: 'latency.validationP95Ms', direction: 'atMost', threshold: 10,
    unit: 'milliseconds', kind: 'timing',
    requirement: 'execute-input validation p95 <= 10 ms',
  },
  {
    id: 'memory.indexBytes', direction: 'atMost', threshold: 25 * MIB,
    unit: 'bytes', kind: 'timing',
    requirement: 'retained retrieval index memory <= 25 MiB',
  },
  {
    id: 'payload.searchBytes', direction: 'atMost', threshold: 32 * KIB,
    unit: 'bytes', kind: 'deterministic',
    requirement: 'largest search response <= 32 KiB',
  },
  {
    id: 'payload.describeBytes', direction: 'atMost', threshold: 64 * KIB,
    unit: 'bytes', kind: 'deterministic',
    requirement: 'largest describe response <= 64 KiB',
  },
  {
    id: 'payload.medianDescribeUnionRatio', direction: 'atMost', threshold: 0.5,
    unit: 'ratio', kind: 'deterministic',
    requirement: 'median describe bytes <= 50% of the union-dump baseline',
  },
]);

/**
 * @param baselineTop1Accuracy the recorded pre-Task-48 top-1 accuracy. The
 * caller must read it from the recorded evidence rather than re-declaring it,
 * so "no worse than baseline" cannot be satisfied by lowering the baseline.
 */
export function buildTask48Budgets(baselineTop1Accuracy: number): readonly BudgetSpec[] {
  if (!Number.isFinite(baselineTop1Accuracy)) {
    throw new BudgetError('baseline top-1 accuracy is not a finite number', baselineTop1Accuracy);
  }
  if (baselineTop1Accuracy < 0 || baselineTop1Accuracy > 1) {
    throw new BudgetError('baseline top-1 accuracy is outside [0,1]', baselineTop1Accuracy);
  }
  return Object.freeze([
    ...TASK48_STATIC_BUDGETS,
    Object.freeze({
      id: 'retrieval.corpusScorerTop1NotBelowBaseline' as const,
      direction: 'atLeast' as const,
      threshold: baselineTop1Accuracy,
      unit: 'ratio' as const,
      kind: 'deterministic' as const,
      requirement:
        'deterministic corpus-scorer top-1 on the current manifest, no worse than the '
        + `recorded Task-4 baseline (${baselineTop1Accuracy})`,
    }),
  ]);
}

function holds(direction: BudgetDirection, observed: number, threshold: number): boolean {
  switch (direction) {
    case 'atLeast':
      return observed >= threshold - BUDGET_EPSILON;
    case 'atMost':
      return observed <= threshold + BUDGET_EPSILON;
    case 'exactly':
      return Math.abs(observed - threshold) <= BUDGET_EPSILON;
    default: {
      const unreachable: never = direction;
      throw new BudgetError('unhandled budget direction', unreachable);
    }
  }
}

export function evaluateBudgets(
  specs: readonly BudgetSpec[],
  observations: readonly BudgetObservation[],
): readonly BudgetResult[] {
  const declared = new Set<BudgetId>(specs.map((spec) => spec.id));
  const seen = new Map<BudgetId, number>();
  for (const observation of observations) {
    if (!declared.has(observation.id)) {
      throw new BudgetError('observation for an undeclared budget', observation.id);
    }
    if (seen.has(observation.id)) {
      throw new BudgetError('duplicate observation for a budget', observation.id);
    }
    if (!Number.isFinite(observation.observed)) {
      throw new BudgetError('observation is not a finite number', observation);
    }
    seen.set(observation.id, observation.observed);
  }
  return specs.map((spec) => {
    const observed = seen.get(spec.id);
    if (observed === undefined) {
      throw new BudgetError('declared budget has no observation', spec.id);
    }
    return {
      id: spec.id,
      direction: spec.direction,
      threshold: spec.threshold,
      observed,
      unit: spec.unit,
      kind: spec.kind,
      passed: holds(spec.direction, observed, spec.threshold),
    };
  });
}

export function failedBudgets(results: readonly BudgetResult[]): readonly BudgetResult[] {
  return results.filter((result) => !result.passed);
}
