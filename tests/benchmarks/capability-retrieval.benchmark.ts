import { resolve } from 'node:path';
import { performance } from 'node:perf_hooks';
import { pathToFileURL } from 'node:url';

import {
  PILOT_CAPABILITY_CATALOG,
  PILOT_PARENT_CATEGORIES,
  retrieveCapabilities,
} from '../../src/tools/catalog/capabilities/retrieval/index.js';
import { corpus } from '../eval/corpus.js';

export const RETRIEVAL_P95_BUDGET_MS = 50 as const;

export type CapabilityRetrievalBenchmarkReport = {
  readonly catalogRecords: number;
  readonly queryCount: number;
  readonly warmupRuns: number;
  readonly sampleRuns: number;
  readonly p50Ms: number;
  readonly p95Ms: number;
  readonly maxMs: number;
  readonly budgetMs: number;
  readonly passed: boolean;
};

const PARENTS = [...new Set(
  PILOT_CAPABILITY_CATALOG.map((record) => record.routing.parentTool),
)].sort();
const CATEGORIES = [...new Set(Object.values(PILOT_PARENT_CATEGORIES))].sort();
const PLUGINS = [...new Set(
  PILOT_CAPABILITY_CATALOG.flatMap((record) => record.availability.requiredPlugins),
)].sort();
const PARENT_SET = new Set<string>(PARENTS);
const QUERIES = corpus.cases
  .filter((entry) => PARENT_SET.has(entry.expected.tool))
  .map((entry) => entry.intent);

const PROFILE = {
  unrealVersion: { major: 5, minor: 7, patch: 4, channel: 'stable' },
  installedPlugins: PLUGINS,
  editorState: 'edit',
  enabledParents: PARENTS,
  enabledCategories: CATEGORIES,
  authorizedScopes: ['read', 'write', 'destructive', 'admin'],
  requestedEffects: ['read', 'write', 'destructive'],
  requiredOutputFields: [],
} as const;

function percentile(sorted: readonly number[], ratio: number): number {
  if (sorted.length === 0) return 0;
  const index = Math.max(0, Math.ceil(sorted.length * ratio) - 1);
  return sorted[index] ?? 0;
}

function executeQuery(query: string): void {
  retrieveCapabilities({ query, limit: 5, profile: PROFILE });
}

export function runCapabilityRetrievalBenchmark(
  sampleRuns = 500,
  warmupRuns = 100,
): CapabilityRetrievalBenchmarkReport {
  if (QUERIES.length === 0) throw new TypeError('Task 13 benchmark requires pilot corpus queries');
  for (let run = 0; run < warmupRuns; run += 1) {
    executeQuery(QUERIES[run % QUERIES.length] ?? '');
  }
  const samples: number[] = [];
  for (let run = 0; run < sampleRuns; run += 1) {
    const started = performance.now();
    executeQuery(QUERIES[run % QUERIES.length] ?? '');
    samples.push(performance.now() - started);
  }
  samples.sort((left, right) => left - right);
  const p95Ms = percentile(samples, 0.95);
  return Object.freeze({
    catalogRecords: PILOT_CAPABILITY_CATALOG.length,
    queryCount: QUERIES.length,
    warmupRuns,
    sampleRuns,
    p50Ms: percentile(samples, 0.5),
    p95Ms,
    maxMs: samples[samples.length - 1] ?? 0,
    budgetMs: RETRIEVAL_P95_BUDGET_MS,
    passed: p95Ms <= RETRIEVAL_P95_BUDGET_MS,
  });
}

const invokedPath = process.argv[1];
if (invokedPath !== undefined && import.meta.url === pathToFileURL(resolve(invokedPath)).href) {
  const report = runCapabilityRetrievalBenchmark();
  process.stdout.write(`${JSON.stringify(report, null, 2)}\n`);
  if (!report.passed) process.exitCode = 1;
}
