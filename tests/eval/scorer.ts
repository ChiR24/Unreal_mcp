// tests/eval/scorer.ts
// Deterministic offline scorer. Evaluates a model-call fixture (or an explicit
// ranking override) against the corpus expectations and the available-capability
// universe from the gateway manifest. Produces stable per-case scores and an
// aggregate report whose hashes are independent of corpus/candidate ordering.

import { sha256Canonical, stableStringifyValue } from './hash.js';
import { availableManifest, findTool, hasCapability } from './manifest-model.js';
import { isDestructive, modelCall } from './model-fixture.js';
import {
  type CapabilityRef,
  type CaseScore,
  type Corpus,
  type CorpusCase,
  type DisclosureMetrics,
  type ElicitationExpectation,
  EPSILON,
  type EvalMetrics,
  type ManifestModel,
  type ModelRanking,
  NEAR_TIE_RATIO,
  type RankedCandidate,
  REPORT_SCHEMA,
  type ScoreReport,
  TOKEN_BYTES_PER_TOKEN,
  TOP_K,
} from './types.js';

function isNearTie(top: number, candidate: number): boolean {
  if (top <= 0) return false;
  const threshold = Math.max(EPSILON, top * NEAR_TIE_RATIO);
  return top - candidate <= threshold;
}

function deriveElicitation(kind: CorpusCase['kind']): ElicitationExpectation {
  if (kind === 'destructive' || kind === 'near_tie_destructive') return 'confirm';
  if (kind === 'ambiguous') return 'ambiguous';
  if (kind === 'version_negative' || kind === 'plugin_negative') return 'unavailable';
  return 'none';
}

function envelopeBytes(tool: string, action: string, params: readonly string[]): number {
  const envelope = JSON.stringify({
    tool,
    action,
    params: [...params],
    guidance: `describe ${tool}.${action}`,
  });
  return Buffer.byteLength(envelope, 'utf8');
}

function contains(candidates: ModelRanking, ref: CapabilityRef): boolean {
  return candidates.some((c) => c.tool === ref.tool && c.action === ref.action);
}

function supersetOf(provided: readonly string[], required: readonly string[]): boolean {
  const set = new Set(provided);
  return required.every((r) => set.has(r));
}

function resolveRanking(entry: CorpusCase, manifest: ManifestModel): ModelRanking {
  const base: ModelRanking =
    entry.modelRankingOverride !== undefined
      ? entry.modelRankingOverride
      : modelCall(entry.intent, availableManifest(manifest, entry.unavailableTool), TOP_K);
  if (entry.unavailableTool === undefined) return base;
  const available = availableManifest(manifest, entry.unavailableTool);
  return base.filter((c) => hasCapability(available, { tool: c.tool, action: c.action }));
}

export function scoreCase(entry: CorpusCase, manifest: ManifestModel): CaseScore {
  const ranking = resolveRanking(entry, manifest);
  const top = ranking[0];
  const tiedSet = top === undefined ? [] : ranking.filter((c) => isNearTie(top.score, c.score));
  const anyDestructiveTied = tiedSet.some((c) => isDestructive(c.action));
  const nearTie = tiedSet.length >= 2;
  const autoSelected = !(nearTie && anyDestructiveTied) && top !== undefined;
  const selected = autoSelected && top !== undefined
    ? { tool: top.tool, action: top.action }
    : null;
  const expectedInTiedSet = tiedSet.some(
    (c) => c.tool === entry.expected.tool && c.action === entry.expected.action,
  );
  const negative = entry.kind === 'version_negative' || entry.kind === 'plugin_negative';

  const top1Correct = negative
    ? !contains(ranking, entry.expected)
    : selected !== null
      ? selected.tool === entry.expected.tool && selected.action === entry.expected.action
      : expectedInTiedSet;
  const topKCorrect = negative ? !contains(ranking, entry.expected) : contains(ranking, entry.expected);

  const finalParams = entry.secondAttemptParams ?? entry.requiredParams;
  const paramCompleteness = supersetOf(finalParams, entry.requiredParams);

  let guidedRecovery: boolean | null = null;
  if (entry.firstAttemptParams !== undefined) {
    const firstComplete = supersetOf(entry.firstAttemptParams, entry.requiredParams);
    const secondComplete = supersetOf(finalParams, entry.requiredParams);
    guidedRecovery = !firstComplete && secondComplete;
  }

  let unavailableFiltered: boolean | null = null;
  if (negative) {
    unavailableFiltered = !contains(ranking, entry.expected);
  }

  let ambiguityHandled: boolean | null = null;
  if (entry.kind === 'ambiguous') {
    ambiguityHandled = entry.allowedAlternatives.every((alt) => contains(ranking, alt));
  } else if (entry.kind === 'near_tie_destructive') {
    ambiguityHandled = !autoSelected && expectedInTiedSet;
  }

  const elicitation = deriveElicitation(entry.kind);

  const disclosed = autoSelected && selected !== null ? [selected] : tiedSet;
  const disclosureBytes = disclosed.reduce(
    (sum, ref) => sum + envelopeBytes(ref.tool, ref.action, entry.requiredParams),
    0,
  );
  const estimatedTokens = Math.ceil(disclosureBytes / TOKEN_BYTES_PER_TOKEN);
  const tokenBudgetCompliance = estimatedTokens <= entry.tokenBudget;

  return {
    id: entry.id,
    kind: entry.kind,
    top1Correct,
    topKCorrect,
    selected,
    expectedInTiedSet,
    paramCompleteness,
    guidedRecovery,
    unavailableFiltered,
    ambiguityHandled,
    elicitation,
    autoSelected,
    disclosureBytes,
    estimatedTokens,
    tokenBudgetCompliance,
  };
}

function rate(values: readonly (boolean | null)[]): number {
  const present = values.filter((v): v is boolean => v !== null);
  if (present.length === 0) return 1;
  return present.filter((v) => v).length / present.length;
}

function normalizeCorpus(corpus: Corpus): Corpus {
  const cases = [...corpus.cases].sort((a, b) => (a.id < b.id ? -1 : a.id > b.id ? 1 : 0));
  return { schema: corpus.schema, version: corpus.version, cases };
}

export function scoreCorpus(corpus: Corpus, manifest: ManifestModel): ScoreReport {
  const perCase = corpus.cases.map((entry) => scoreCase(entry, manifest));
  const sortedPerCase = [...perCase].sort((a, b) => (a.id < b.id ? -1 : a.id > b.id ? 1 : 0));

  const total = sortedPerCase.length;
  const parentCoverage = [...new Set(corpus.cases.map((c) => c.expected.tool))].sort();
  const collisionCoverage = [
    ...new Set(corpus.cases.map((c) => c.collisionId).filter((v): v is string => v !== undefined)),
  ].sort();

  const metrics: EvalMetrics = {
    totalCases: total,
    top1Accuracy: sortedPerCase.filter((c) => c.top1Correct).length / total,
    topKAccuracy: sortedPerCase.filter((c) => c.topKCorrect).length / total,
    paramCompletenessRate: sortedPerCase.filter((c) => c.paramCompleteness).length / total,
    guidedRecoveryRate: rate(sortedPerCase.map((c) => c.guidedRecovery)),
    unavailableFilterRate: rate(sortedPerCase.map((c) => c.unavailableFiltered)),
    ambiguityHandledRate: rate(sortedPerCase.map((c) => c.ambiguityHandled)),
    tokenBudgetComplianceRate: sortedPerCase.filter((c) => c.tokenBudgetCompliance).length / total,
    parentCoverage,
    collisionCoverage,
  };

  const disclosure: DisclosureMetrics = {
    totalBytes: sortedPerCase.reduce((s, c) => s + c.disclosureBytes, 0),
    maxBytes: sortedPerCase.reduce((m, c) => Math.max(m, c.disclosureBytes), 0),
    totalTokens: sortedPerCase.reduce((s, c) => s + c.estimatedTokens, 0),
    maxTokens: sortedPerCase.reduce((m, c) => Math.max(m, c.estimatedTokens), 0),
  };

  const normalized = normalizeCorpus(corpus);
  const corpusHash = sha256Canonical(normalized);
  const reportHashSource = {
    corpusHash,
    manifestSource: manifest.source,
    manifestToolCount: manifest.tools.length,
    metrics,
    disclosure,
    perCase: sortedPerCase,
  };
  const reportHash = sha256Canonical(reportHashSource);

  return {
    schema: REPORT_SCHEMA,
    corpusHash,
    reportHash,
    manifestSource: manifest.source,
    manifestToolCount: manifest.tools.length,
    metrics,
    disclosure,
    perCase: sortedPerCase,
  };
}

export function formatStable(value: unknown): string {
  return stableStringifyValue(value);
}

export function manifestToolNames(manifest: ManifestModel): readonly string[] {
  return manifest.tools.map((t) => t.name).sort();
}

export function toolExists(manifest: ManifestModel, name: string): boolean {
  return findTool(manifest, name) !== null;
}

export type CurrentGatewayBaselineAttempt = {
  readonly caseId: string;
  readonly rankedCandidates: readonly RankedCandidate[];
  readonly top1: { readonly tool: string; readonly action: string } | null;
  readonly autoSelected: boolean;
  readonly disclosureBytes: number;
};

export function buildCurrentGatewayBaselineAttempts(
  corpus: Corpus,
  manifest: ManifestModel,
): readonly CurrentGatewayBaselineAttempt[] {
  return corpus.cases.map((entry) => {
    const ranking = modelCall(entry.intent, availableManifest(manifest, entry.unavailableTool), TOP_K);
    const top = ranking[0];
    const top1 = top === undefined ? null : { tool: top.tool, action: top.action };
    const disclosed = ranking.slice(0, TOP_K);
    const disclosureBytes = disclosed.reduce(
      (sum, ref) => sum + envelopeBytes(ref.tool, ref.action, entry.requiredParams),
      0,
    );
    return {
      caseId: entry.id,
      rankedCandidates: ranking,
      top1,
      autoSelected: top1 !== null,
      disclosureBytes,
    };
  });
}
