import { describe, expect, it } from 'vitest';

import { runCapabilityRetrievalBenchmark } from './task13-capability-retrieval.benchmark.js';

describe('capability retrieval benchmark', () => {
  it('Given a warmed 493-record index, When representative intents are ranked, Then p95 stays within 50 milliseconds', () => {
    const report = runCapabilityRetrievalBenchmark(200, 50);

    expect(report.sampleRuns).toBe(200);
    expect(report.warmupRuns).toBe(50);
    expect(report.p95Ms).toBeLessThanOrEqual(report.budgetMs);
    expect(report.passed).toBe(true);
  });
});
