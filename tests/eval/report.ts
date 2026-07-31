// tests/eval/report.ts
// Assembles one Task-48 release-gating report and renders a verdict.
//
// The report separates what is reproducible from what is not, and says so in
// `hashedSections`. `deterministicHash` covers the tree, corpus, retrieval,
// corpus-scorer and payload sections only. Wall-clock latency and heap
// footprint are enforced exactly as hard, but folding a machine-dependent
// microsecond into a content hash would produce a hash that changes under CPU
// load, which proves nothing about the artifact.

import { readFileSync } from 'node:fs';
import { corpus, validateCorpus } from './corpus.js';
import { sha256Canonical } from './hash.js';
import { loadManifestModel } from './manifest-model.js';
import { scoreCorpus } from './scorer.js';
import {
  type BudgetObservation,
  type BudgetResult,
  BudgetError,
  buildTask48Budgets,
  evaluateBudgets,
  failedBudgets,
} from './budgets.js';
import {
  type EnvironmentDescriptor,
  environmentDescriptor,
  finalRegistryRecords,
  treeHash,
  treeHashInputs,
} from './fixtures.js';
import { measurePayload, type PayloadMeasurement } from './measure-payload.js';
import {
  type GatewayRanker,
  gatewaySearchRanker,
  measureRetrieval,
  type RetrievalMeasurement,
} from './measure-retrieval.js';
import { measureRuntime, type RuntimeMeasurement } from './measure-runtime.js';
import { type ModelEnv, type ModelInvoker, type ModelReport, runModelEvaluation } from './model-runner.js';

export const DEFAULT_MANIFEST_PATH = 'src/gateway/gateway-manifest.generated.json';
export const DEFAULT_BASELINE_PATH = '.omo/evidence/task-4-baseline.json';
export const TASK48_REPORT_SCHEMA = 'omo.task48.budget-report.v1' as const;

export type RecordedBaseline = {
  readonly source: string;
  readonly top1Accuracy: number;
  readonly corpusHash: string;
  readonly reportHash: string;
};

/**
 * The baseline is READ from the recorded Task-4 artifact, never re-declared
 * here. "No worse than baseline" must not be satisfiable by editing the number
 * the gate compares against in the same file that evaluates the gate.
 */
export function loadRecordedBaseline(path: string = DEFAULT_BASELINE_PATH): RecordedBaseline {
  let parsed: unknown;
  try {
    parsed = JSON.parse(readFileSync(path, 'utf8'));
  } catch (error) {
    throw new BudgetError(`cannot read recorded baseline at ${path}`, String(error));
  }
  if (typeof parsed !== 'object' || parsed === null) {
    throw new BudgetError('recorded baseline is not an object', path);
  }
  const record = parsed as Record<string, unknown>;
  const metrics = record.metrics;
  if (typeof metrics !== 'object' || metrics === null) {
    throw new BudgetError('recorded baseline has no metrics object', path);
  }
  const top1 = (metrics as Record<string, unknown>).top1Accuracy;
  if (typeof top1 !== 'number' || !Number.isFinite(top1)) {
    throw new BudgetError('recorded baseline metrics.top1Accuracy is not a number', path);
  }
  return {
    source: path,
    top1Accuracy: top1,
    corpusHash: typeof record.corpusHash === 'string' ? record.corpusHash : '',
    reportHash: typeof record.reportHash === 'string' ? record.reportHash : '',
  };
}

export type CorpusScorerSummary = {
  readonly corpusHash: string;
  readonly reportHash: string;
  readonly manifestSource: string;
  readonly manifestToolCount: number;
  readonly top1Accuracy: number;
  readonly topKAccuracy: number;
};

export type Task48Report = {
  readonly schema: typeof TASK48_REPORT_SCHEMA;
  readonly generatedAt: string;
  readonly environment: EnvironmentDescriptor;
  readonly treeHash: string;
  readonly treeHashInputCount: number;
  readonly registryRecordCount: number;
  readonly corpusVersion: string;
  readonly corpusScorer: CorpusScorerSummary;
  readonly baseline: RecordedBaseline;
  readonly retrieval: RetrievalMeasurement;
  readonly payload: PayloadMeasurement;
  readonly runtime: RuntimeMeasurement;
  readonly model: ModelReport;
  readonly budgets: readonly BudgetResult[];
  readonly failedBudgetIds: readonly string[];
  readonly verdict: 'PASS' | 'FAIL';
  readonly hashedSections: readonly string[];
  readonly deterministicHash: string;
};

export const HASHED_SECTIONS: readonly string[] = Object.freeze([
  'treeHash',
  'registryRecordCount',
  'corpusScorer',
  'retrieval',
  'payload',
  'deterministicBudgets',
]);

export function observationsFor(
  retrieval: RetrievalMeasurement,
  payload: PayloadMeasurement,
  runtime: RuntimeMeasurement,
  corpusScorer: CorpusScorerSummary,
): readonly BudgetObservation[] {
  return Object.freeze([
    { id: 'retrieval.topKRecall', observed: retrieval.topKRecall },
    { id: 'retrieval.top1Accuracy', observed: retrieval.top1Accuracy },
    { id: 'retrieval.corpusScorerTop1NotBelowBaseline', observed: corpusScorer.top1Accuracy },
    { id: 'retrieval.guidedRecoveryRate', observed: retrieval.guidedRecoveryRate },
    { id: 'retrieval.unavailableFilterRate', observed: retrieval.unavailableFilterRate },
    {
      id: 'retrieval.destructiveFalseAutoSelections',
      observed: retrieval.destructiveFalseAutoSelections,
    },
    { id: 'latency.warmSearchP95Ms', observed: runtime.warmSearch.percentilesMs.p95 },
    { id: 'latency.describeP95Ms', observed: runtime.describe.percentilesMs.p95 },
    { id: 'latency.validationP95Ms', observed: runtime.validation.percentilesMs.p95 },
    { id: 'memory.indexBytes', observed: runtime.indexMemory.bytesPerIndex },
    { id: 'payload.searchBytes', observed: payload.maxSearchBytes },
    { id: 'payload.describeBytes', observed: payload.maxDescribeBytes },
    { id: 'payload.medianDescribeUnionRatio', observed: payload.medianDescribeUnionRatio },
  ] satisfies readonly BudgetObservation[]);
}

export type BuildReportOptions = {
  readonly manifestPath?: string;
  readonly baselinePath?: string;
  readonly env?: ModelEnv;
  readonly modelInvoker?: ModelInvoker;
  readonly ranker?: GatewayRanker;
  readonly runtimeScale?: number;
  readonly projectRoot?: string;
};

export function buildTask48Report(options: BuildReportOptions = {}): Task48Report {
  const manifestPath = options.manifestPath ?? DEFAULT_MANIFEST_PATH;
  const manifest = loadManifestModel(manifestPath);
  validateCorpus(corpus, manifest);
  const scored = scoreCorpus(corpus, manifest);
  const corpusScorer: CorpusScorerSummary = {
    corpusHash: scored.corpusHash,
    reportHash: scored.reportHash,
    manifestSource: scored.manifestSource,
    manifestToolCount: scored.manifestToolCount,
    top1Accuracy: scored.metrics.top1Accuracy,
    topKAccuracy: scored.metrics.topKAccuracy,
  };

  const baseline = loadRecordedBaseline(options.baselinePath ?? DEFAULT_BASELINE_PATH);
  const retrieval = measureRetrieval(options.ranker ?? gatewaySearchRanker);
  const payload = measurePayload();
  const runtime = measureRuntime(options.runtimeScale ?? 1);
  const model = runModelEvaluation(options.env ?? process.env, options.modelInvoker);

  const specs = buildTask48Budgets(baseline.top1Accuracy);
  const budgets = evaluateBudgets(specs, observationsFor(retrieval, payload, runtime, corpusScorer));
  const failed = failedBudgets(budgets);

  const root = options.projectRoot ?? process.cwd();
  const tree = treeHash(root);
  const deterministicHash = sha256Canonical({
    treeHash: tree,
    registryRecordCount: finalRegistryRecords().length,
    corpusScorer,
    retrieval,
    payload,
    deterministicBudgets: budgets.filter((budget) => budget.kind === 'deterministic'),
  });

  return {
    schema: TASK48_REPORT_SCHEMA,
    generatedAt: new Date().toISOString(),
    environment: environmentDescriptor(),
    treeHash: tree,
    treeHashInputCount: treeHashInputs(root).length,
    registryRecordCount: finalRegistryRecords().length,
    corpusVersion: corpus.version,
    corpusScorer,
    baseline,
    retrieval,
    payload,
    runtime,
    model,
    budgets,
    failedBudgetIds: Object.freeze(failed.map((budget) => budget.id)),
    verdict: failed.length === 0 ? 'PASS' : 'FAIL',
    hashedSections: HASHED_SECTIONS,
    deterministicHash,
  };
}
