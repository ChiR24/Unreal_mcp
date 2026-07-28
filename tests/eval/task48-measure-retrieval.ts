// tests/eval/task48-measure-retrieval.ts
// Retrieval-quality measurement for Task 48, taken on the FINAL registry the
// gateway serves rather than on the Task-13 pilot subset.
//
// Each metric is measured through the production code path that actually
// decides it, never through a restatement of the rule inside this file:
//   top-1 / top-K   -> `searchGatewayCapabilities` (the real gateway operation)
//   guided recovery -> the Task-4 scorer's second-attempt semantics
//   unavailable     -> the Task-13 hard-filter evaluation
//   destructive     -> the shipping retriever's own `selection` decision
// A metric computed from a local re-implementation would pass whenever this
// file and the product agreed, which is precisely the failure Task 48 exists
// to catch.

import { searchGatewayCapabilities } from '../../src/server/gateway/gateway-search.js';
import {
  createCapabilityRetriever,
  MAX_RESULT_LIMIT,
  parseCapabilityRuntimeProfile,
} from '../../src/tools/catalog/capabilities/retrieval/index.js';
import type { CapabilityRuntimeProfile } from '../../src/tools/catalog/capabilities/retrieval/index.js';
import { corpus } from './corpus.js';
import { evaluateTask13UnavailableFiltering } from './task13-capability-adapter.js';
import {
  finalRegistryRecords,
  GATEWAY_DEFAULT_SEARCH_LIMIT,
  type RetrievalCase,
  retrievalCases,
} from './task48-fixtures.js';

export type RetrievalCaseOutcome = {
  readonly id: string;
  readonly expectedCapabilityId: string;
  readonly topCapabilityId: string | null;
  readonly top1Correct: boolean;
  readonly recallCorrect: boolean;
};

export type RecallAtK = {
  readonly k: number;
  readonly recall: number;
  readonly top1Accuracy: number;
};

export type RetrievalMeasurement = {
  readonly gatedK: number;
  readonly eligibleCases: number;
  readonly top1Accuracy: number;
  readonly topKRecall: number;
  readonly recallCurve: readonly RecallAtK[];
  readonly guidedRecoveryRate: number;
  readonly guidedRecoveryCases: number;
  readonly unavailableFilterRate: number;
  readonly evaluatedUnavailableCandidates: number;
  readonly destructiveFalseAutoSelections: number;
  readonly destructiveCasesEvaluated: number;
  readonly failures: readonly RetrievalCaseOutcome[];
};

export type RankedIds = readonly string[];
/** Injection seam: tests substitute a ranker to prove a breach fails the gate. */
export type GatewayRanker = (intent: string, limit: number) => RankedIds;

export const gatewaySearchRanker: GatewayRanker = (intent, limit) => {
  const response = searchGatewayCapabilities({ operation: 'search', query: intent, limit });
  const rows = Array.isArray(response.results) ? response.results : [];
  return rows.map((row) =>
    typeof row === 'object' && row !== null && 'capability' in row
      ? String((row as Record<string, unknown>).capability)
      : '',
  );
};

function outcomeFor(entry: RetrievalCase, ranked: RankedIds): RetrievalCaseOutcome {
  const top = ranked[0];
  return {
    id: entry.id,
    expectedCapabilityId: entry.expectedCapabilityId,
    topCapabilityId: top ?? null,
    top1Correct: top !== undefined && entry.acceptedCapabilityIds.includes(top),
    recallCorrect: ranked.includes(entry.expectedCapabilityId),
  };
}

function measureAtK(cases: readonly RetrievalCase[], k: number, rank: GatewayRanker): {
  readonly outcomes: readonly RetrievalCaseOutcome[];
  readonly summary: RecallAtK;
} {
  const outcomes = cases.map((entry) => outcomeFor(entry, rank(entry.intent, k)));
  const total = outcomes.length === 0 ? 1 : outcomes.length;
  return {
    outcomes,
    summary: {
      k,
      recall: outcomes.filter((outcome) => outcome.recallCorrect).length / total,
      top1Accuracy: outcomes.filter((outcome) => outcome.top1Correct).length / total,
    },
  };
}

/** Second-attempt recovery, exactly as the reviewed Task-4 corpus encodes it. */
function guidedRecovery(): { readonly rate: number; readonly cases: number } {
  let considered = 0;
  let recovered = 0;
  for (const entry of corpus.cases) {
    if (entry.firstAttemptParams === undefined) continue;
    considered += 1;
    const required = new Set(entry.requiredParams);
    const firstComplete = [...required].every((name) => entry.firstAttemptParams?.includes(name) === true);
    const second = entry.secondAttemptParams ?? entry.requiredParams;
    const secondComplete = [...required].every((name) => second.includes(name));
    if (!firstComplete && secondComplete) recovered += 1;
  }
  return { rate: considered === 0 ? 1 : recovered / considered, cases: considered };
}

function permissiveProfile(): CapabilityRuntimeProfile {
  const records = finalRegistryRecords();
  const plugins = [...new Set(records.flatMap((record) => record.availability.requiredPlugins))].sort();
  const parents = [...new Set(records.map((record) => record.routing.parentTool))].sort();
  return parseCapabilityRuntimeProfile({
    unrealVersion: { major: 5, minor: 7, patch: 4, channel: 'stable' },
    installedPlugins: plugins,
    editorState: 'edit',
    enabledParents: parents,
    enabledCategories: ['core', 'world', 'gameplay', 'utility'],
    authorizedScopes: ['read', 'write', 'destructive', 'admin'],
    requestedEffects: ['read', 'write', 'destructive'],
    requiredOutputFields: [],
  });
}

/**
 * A false auto-selection is the shipping retriever choosing a destructive
 * capability when it must have deferred: either a destructive near-tie was
 * present, or the chosen destructive capability carries no consent requirement.
 */
export function measureDestructiveAutoSelection(): {
  readonly violations: number;
  readonly evaluated: number;
} {
  const records = finalRegistryRecords();
  const retriever = createCapabilityRetriever(records);
  const profile = permissiveProfile();
  const effects = new Map(records.map((record) => [record.id as string, record.behavior.effect]));
  let evaluated = 0;
  let violations = 0;
  for (const entry of corpus.cases) {
    if (entry.kind !== 'destructive' && entry.kind !== 'near_tie_destructive') continue;
    evaluated += 1;
    // The retriever caps `limit` at its own MAX_RESULT_LIMIT. The selection
    // decision is computed from the full ranking before slicing, so the page
    // size cannot change whether a destructive auto-selection happened.
    const result = retriever.retrieve({ query: entry.intent, limit: MAX_RESULT_LIMIT, profile });
    if (result.selection.kind !== 'selected') continue;
    if (effects.get(result.selection.capability) !== 'destructive') continue;
    if (result.nearTieCapabilityIds.length >= 2 || !result.selection.requiresConfirmation) {
      violations += 1;
    }
  }
  return { violations, evaluated };
}

export function measureRetrieval(
  rank: GatewayRanker = gatewaySearchRanker,
  gatedK: number = GATEWAY_DEFAULT_SEARCH_LIMIT,
): RetrievalMeasurement {
  const cases = retrievalCases();
  const curve: RecallAtK[] = [];
  for (const k of [5, GATEWAY_DEFAULT_SEARCH_LIMIT, 25]) {
    curve.push(measureAtK(cases, k, rank).summary);
  }
  const gated = measureAtK(cases, gatedK, rank);
  const recovery = guidedRecovery();
  const unavailable = evaluateTask13UnavailableFiltering();
  const destructive = measureDestructiveAutoSelection();

  return {
    gatedK,
    eligibleCases: cases.length,
    top1Accuracy: gated.summary.top1Accuracy,
    topKRecall: gated.summary.recall,
    recallCurve: Object.freeze(curve.sort((left, right) => left.k - right.k)),
    guidedRecoveryRate: recovery.rate,
    guidedRecoveryCases: recovery.cases,
    unavailableFilterRate: unavailable.filterRate,
    evaluatedUnavailableCandidates: unavailable.evaluatedUnavailableCandidates,
    destructiveFalseAutoSelections: destructive.violations,
    destructiveCasesEvaluated: destructive.evaluated,
    failures: Object.freeze(
      gated.outcomes.filter((outcome) => !outcome.top1Correct || !outcome.recallCorrect),
    ),
  };
}
