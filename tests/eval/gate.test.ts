// tests/eval/gate.test.ts
// The release gate: retrieval quality, response size and warm latency of the
// real gateway search/describe, one assertion per budget.

import { performance } from 'node:perf_hooks';
import { describe, expect, it } from 'vitest';
import { describeGatewayCapability } from '../../src/server/gateway/gateway-describe.js';
import { applyDeclaredDefaults, validateAgainstCapabilitySchema } from '../../src/server/gateway/gateway-schema-validate.js';
import { searchGatewayCapabilities } from '../../src/server/gateway/gateway-search.js';
import { createCapabilitySearchIndex } from '../../src/tools/catalog/capabilities/retrieval/scoring.js';
import { corpus } from './corpus.data.js';
import { finalRegistryRecords, p95 } from './fixtures.js';
import { measureRetrieval } from './measure-retrieval.js';

const KIB = 1024;
const jsonBytes = (value: unknown): number => Buffer.byteLength(JSON.stringify(value) ?? '', 'utf8');
const records = finalRegistryRecords();
const intents = corpus.map((entry) => entry.intent);

/** Warm p95 in ms: a warm-up pass runs first so index construction is never charged. */
function warmP95(operation: (run: number) => void, runs: number): number {
  for (let run = 0; run < Math.ceil(runs / 4); run += 1) operation(run);
  const samples: number[] = [];
  for (let run = 0; run < runs; run += 1) {
    const started = performance.now();
    operation(run);
    samples.push(performance.now() - started);
  }
  return p95(samples);
}

describe('retrieval', () => {
  const measured = measureRetrieval();

  it('top-K recall is at least 98%', () => {
    expect(measured.topKRecall, measured.misses.join('\n')).toBeGreaterThanOrEqual(0.98);
  });

  it('top-1 accuracy is at least 90%', () => {
    expect(measured.top1Accuracy, measured.misses.join('\n')).toBeGreaterThanOrEqual(0.9);
  });

  it('a degraded or wrong ranker fails both', () => {
    for (const ranker of [() => [], () => ['definitely.not.a.capability']]) {
      const broken = measureRetrieval(ranker);
      expect(broken.top1Accuracy).toBe(0);
      expect(broken.topKRecall).toBe(0);
    }
  });
});

describe('payload size', () => {
  it('no search response exceeds 32 KiB', () => {
    const sizes = [...intents.map((query) => jsonBytes(searchGatewayCapabilities({ operation: 'search', query }))),
      jsonBytes(searchGatewayCapabilities({ operation: 'search' }))];
    expect(Math.max(...sizes)).toBeLessThanOrEqual(32 * KIB);
  });

  it('no describe response exceeds 64 KiB', () => {
    const sizes = records.map((record) => jsonBytes(describeGatewayCapability({ operation: 'describe', capability: record.id })));
    expect(Math.max(...sizes)).toBeLessThanOrEqual(64 * KIB);
  });
});

describe('warm latency', () => {
  it('search p95 is within 50 ms', () => {
    expect(warmP95((run) => { searchGatewayCapabilities({ operation: 'search', query: intents[run % intents.length] ?? '' }); }, 50))
      .toBeLessThanOrEqual(50);
  });

  it('describe p95 is within 25 ms', () => {
    expect(warmP95((run) => { describeGatewayCapability({ operation: 'describe', capability: records[run % records.length]?.id ?? '' }); }, 50))
      .toBeLessThanOrEqual(25);
  });

  it('execute-input validation p95 is within 10 ms', () => {
    const samples = records.flatMap((record) => {
      const example = record.examples[0];
      return example === undefined ? [] : [{ input: { ...example.input } as Record<string, unknown>, schema: record.schemas.input }];
    });
    expect(warmP95((run) => {
      const sample = samples[run % samples.length];
      if (sample !== undefined) validateAgainstCapabilitySchema(applyDeclaredDefaults(sample.input, sample.schema), sample.schema);
    }, 250)).toBeLessThanOrEqual(10);
  });

  it('a search index retains at most 25 MiB', () => {
    createCapabilitySearchIndex(records);
    const before = process.memoryUsage().heapUsed;
    const built = [createCapabilitySearchIndex(records), createCapabilitySearchIndex(records)];
    const retained = Math.max(0, process.memoryUsage().heapUsed - before) / built.length;
    expect(retained).toBeLessThanOrEqual(25 * 1024 * KIB);
  });
});
