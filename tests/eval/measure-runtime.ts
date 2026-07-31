// tests/eval/measure-runtime.ts
// Wall-clock and memory measurement for Task 48.
//
// Every latency figure is WARM by construction: a warm-up pass runs before any
// sample is recorded, so the first-call cost of building the capability index
// is never charged to the p95 the budget gates. Memory is measured as retained
// heap across several independent index builds, which averages out allocator
// noise that a single build cannot distinguish from real footprint.

import { performance } from 'node:perf_hooks';
import { describeGatewayCapability } from '../../src/server/gateway/gateway-describe.js';
import { searchGatewayCapabilities } from '../../src/server/gateway/gateway-search.js';
import {
  applyDeclaredDefaults,
  validateAgainstCapabilitySchema,
} from '../../src/server/gateway/gateway-execute-validate.js';
import { createCapabilitySearchIndex } from '../../src/tools/catalog/capabilities/retrieval/scoring.js';
import { corpus } from './corpus.js';
import { finalRegistryRecords, type Percentiles, percentilesOf } from './fixtures.js';

export type LatencyMeasurement = {
  readonly warmupRuns: number;
  readonly percentilesMs: Percentiles;
};

export type IndexMemoryMeasurement = {
  readonly bytesPerIndex: number;
  readonly indexCopies: number;
  readonly recordCount: number;
  readonly gcForced: boolean;
};

export type RuntimeMeasurement = {
  readonly warmSearch: LatencyMeasurement;
  readonly describe: LatencyMeasurement;
  readonly validation: LatencyMeasurement;
  readonly indexMemory: IndexMemoryMeasurement;
};

function collectGarbage(): boolean {
  const hook = (globalThis as { gc?: () => void }).gc;
  if (hook === undefined) return false;
  hook();
  hook();
  return true;
}

function sampleLatency(
  operation: (iteration: number) => void,
  sampleRuns: number,
  warmupRuns: number,
): LatencyMeasurement {
  for (let run = 0; run < warmupRuns; run += 1) operation(run);
  const samples: number[] = [];
  for (let run = 0; run < sampleRuns; run += 1) {
    const started = performance.now();
    operation(run);
    samples.push(performance.now() - started);
  }
  return { warmupRuns, percentilesMs: percentilesOf(samples) };
}

const intents: readonly string[] = Object.freeze(corpus.cases.map((entry) => entry.intent));

function capabilityIds(): readonly string[] {
  return finalRegistryRecords().map((record) => record.id);
}

type ValidationSample = {
  readonly input: Record<string, unknown>;
  readonly schema: unknown;
};

/**
 * One validation sample per capability that ships an example, because an
 * example is the only input in the catalog guaranteed to exercise the real
 * property set rather than an empty object that short-circuits the walk.
 */
export function validationSamples(): readonly ValidationSample[] {
  const samples: ValidationSample[] = [];
  for (const record of finalRegistryRecords()) {
    const example = record.examples[0];
    if (example === undefined) continue;
    samples.push({
      input: { ...example.input } as Record<string, unknown>,
      schema: record.schemas.input,
    });
  }
  return Object.freeze(samples);
}

export function measureIndexMemory(indexCopies = 4): IndexMemoryMeasurement {
  const records = finalRegistryRecords();
  // Build one throwaway index first so lazily-initialised module state is not
  // charged to the measured copies.
  createCapabilitySearchIndex(records);
  const gcForced = collectGarbage();
  const before = process.memoryUsage().heapUsed;
  const built: ReturnType<typeof createCapabilitySearchIndex>[] = [];
  for (let copy = 0; copy < indexCopies; copy += 1) built.push(createCapabilitySearchIndex(records));
  collectGarbage();
  const after = process.memoryUsage().heapUsed;
  // `built` is read after the second sample so the indexes are provably still
  // reachable; otherwise the GC could reclaim them and report near-zero.
  const retained = built.length === 0 ? 0 : Math.max(0, after - before) / built.length;
  return { bytesPerIndex: retained, indexCopies, recordCount: records.length, gcForced };
}

export function measureRuntime(scale = 1): RuntimeMeasurement {
  const ids = capabilityIds();
  const samples = validationSamples();
  const searchRuns = Math.max(1, Math.round(200 * scale));
  const describeRuns = Math.max(1, Math.round(200 * scale));
  const validationRuns = Math.max(1, Math.round(1000 * scale));

  return {
    warmSearch: sampleLatency(
      (run) => {
        searchGatewayCapabilities({ operation: 'search', query: intents[run % intents.length] ?? '' });
      },
      searchRuns,
      Math.max(1, Math.round(50 * scale)),
    ),
    describe: sampleLatency(
      (run) => {
        describeGatewayCapability({ operation: 'describe', capability: ids[run % ids.length] ?? '' });
      },
      describeRuns,
      Math.max(1, Math.round(50 * scale)),
    ),
    validation: sampleLatency(
      (run) => {
        const sample = samples[run % samples.length];
        if (sample === undefined) return;
        validateAgainstCapabilitySchema(
          applyDeclaredDefaults(sample.input, sample.schema),
          sample.schema,
        );
      },
      validationRuns,
      Math.max(1, Math.round(200 * scale)),
    ),
    indexMemory: measureIndexMemory(),
  };
}
