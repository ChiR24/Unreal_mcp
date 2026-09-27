// Task 47 — percentile and queue-timing correctness under a FAKE clock.
//
// No sleeps, no wall-clock reads: the registry takes an injected `now()` so the
// exact millisecond deltas under test are the ones the production code divides.
// A test that slept would only prove "roughly", which is not a percentile proof.

import { describe, expect, it } from 'vitest';

import { TelemetryRegistry } from '../../../src/services/telemetry-registry.js';
import { TELEMETRY_METRIC_NAMES } from '../../../src/services/telemetry-schema.js';

/** Pull one rendered sample value by its full `name{labels}` prefix. */
function sampleValue(rendered: string, prefix: string): number | undefined {
  for (const line of rendered.split('\n')) {
    if (line.startsWith(`${prefix} `)) {
      return Number(line.slice(prefix.length + 1));
    }
  }
  return undefined;
}

describe('Task 47 TelemetryRegistry under fake clocks', () => {
  it('computes nearest-rank percentiles exactly', () => {
    const registry = new TelemetryRegistry();
    for (let i = 1; i <= 10; i += 1) {
      registry.observeRequest({ actionClass: 'read', outcome: 'success', durationSeconds: i / 100 });
    }

    const selector = { surface: 'typescript', actionClass: 'read' } as const;
    expect(registry.quantileSeconds('request', selector, 0.5)).toBeCloseTo(0.05, 9);
    expect(registry.quantileSeconds('request', selector, 0.9)).toBeCloseTo(0.09, 9);
    expect(registry.quantileSeconds('request', selector, 0.95)).toBeCloseTo(0.1, 9);
    expect(registry.quantileSeconds('request', selector, 0.99)).toBeCloseTo(0.1, 9);
  });

  it('returns null rather than a fabricated zero for a series with no samples', () => {
    const registry = new TelemetryRegistry();
    expect(
      registry.quantileSeconds('request', { surface: 'typescript', actionClass: 'destructive' }, 0.95),
    ).toBeNull();
  });

  it('fills histogram buckets cumulatively with an +Inf bucket equal to count', () => {
    const registry = new TelemetryRegistry();
    for (let i = 1; i <= 10; i += 1) {
      registry.observeRequest({ actionClass: 'read', outcome: 'success', durationSeconds: i / 100 });
    }

    const rendered = registry.render();
    const base = `${TELEMETRY_METRIC_NAMES.requestDurationSeconds}_bucket{surface="typescript",action_class="read",le=`;
    expect(sampleValue(rendered, `${base}"0.005"}`)).toBe(0);
    expect(sampleValue(rendered, `${base}"0.01"}`)).toBe(1);
    expect(sampleValue(rendered, `${base}"0.025"}`)).toBe(2);
    expect(sampleValue(rendered, `${base}"0.05"}`)).toBe(5);
    expect(sampleValue(rendered, `${base}"0.1"}`)).toBe(10);
    expect(sampleValue(rendered, `${base}"+Inf"}`)).toBe(10);
    expect(
      sampleValue(rendered, `${TELEMETRY_METRIC_NAMES.requestDurationSeconds}_count{surface="typescript",action_class="read"}`),
    ).toBe(10);
    expect(
      sampleValue(rendered, `${TELEMETRY_METRIC_NAMES.requestDurationSeconds}_sum{surface="typescript",action_class="read"}`),
    ).toBeCloseTo(0.55, 9);
  });

  it('counts outcomes and failure classes as real counters', () => {
    const registry = new TelemetryRegistry();
    registry.observeRequest({ actionClass: 'write', outcome: 'success', durationSeconds: 0.01 });
    registry.observeRequest({ actionClass: 'write', outcome: 'failure', failureClass: 'timeout', durationSeconds: 0.02 });
    registry.observeRequest({ actionClass: 'write', outcome: 'failure', failureClass: 'timeout', durationSeconds: 0.03 });

    const rendered = registry.render();
    expect(
      sampleValue(rendered, `${TELEMETRY_METRIC_NAMES.requestsByClassTotal}{surface="typescript",action_class="write",outcome="success"}`),
    ).toBe(1);
    expect(
      sampleValue(rendered, `${TELEMETRY_METRIC_NAMES.requestsByClassTotal}{surface="typescript",action_class="write",outcome="failure"}`),
    ).toBe(2);
    expect(
      sampleValue(rendered, `${TELEMETRY_METRIC_NAMES.failuresByClassTotal}{surface="typescript",action_class="write",failure_class="timeout"}`),
    ).toBe(2);
  });

  it('emits quantile gauges alongside the histogram for both timing families', () => {
    const registry = new TelemetryRegistry();
    for (let i = 1; i <= 10; i += 1) {
      registry.observeRequest({
        actionClass: 'read',
        outcome: 'success',
        durationSeconds: i / 100,
        queueWaitSeconds: i / 1000,
      });
    }

    const rendered = registry.render();
    expect(
      sampleValue(rendered, `${TELEMETRY_METRIC_NAMES.requestDurationQuantileSeconds}{surface="typescript",action_class="read",quantile="0.95"}`),
    ).toBeCloseTo(0.1, 9);
    expect(
      sampleValue(rendered, `${TELEMETRY_METRIC_NAMES.queueWaitQuantileSeconds}{surface="typescript",action_class="read",quantile="0.5"}`),
    ).toBeCloseTo(0.005, 9);
  });

});
