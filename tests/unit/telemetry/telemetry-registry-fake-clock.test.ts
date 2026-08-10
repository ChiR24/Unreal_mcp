// Task 47 — percentile and queue-timing correctness under a FAKE clock.
//
// No sleeps, no wall-clock reads: the registry takes an injected `now()` so the
// exact millisecond deltas under test are the ones the production code divides.
// A test that slept would only prove "roughly", which is not a percentile proof.

import { describe, expect, it } from 'vitest';

import { TelemetryRegistry } from '../../../src/services/telemetry-registry.js';
import { TELEMETRY_METRIC_NAMES } from '../../../src/services/telemetry-schema.js';

function fakeClock(start = 1_000) {
  let current = start;
  return {
    now: () => current,
    advance: (ms: number) => {
      current += ms;
    },
    set: (ms: number) => {
      current = ms;
    },
  };
}

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
  it('derives queue wait from enqueue->dispatch and duration from dispatch->terminal', () => {
    const clock = fakeClock(1_000);
    const registry = new TelemetryRegistry({ now: clock.now });

    registry.beginRequest('r1', { actionClass: 'write' });
    clock.advance(120);
    registry.markDispatched('r1');
    clock.advance(380);
    registry.endRequest('r1', { outcome: 'success' });

    const rendered = registry.render();
    const queueSum = `${TELEMETRY_METRIC_NAMES.queueWaitSeconds}_sum{surface="typescript",action_class="write"}`;
    const durationSum = `${TELEMETRY_METRIC_NAMES.requestDurationSeconds}_sum{surface="typescript",action_class="write"}`;

    expect(sampleValue(rendered, queueSum)).toBeCloseTo(0.12, 9);
    expect(sampleValue(rendered, durationSum)).toBeCloseTo(0.38, 9);
  });

  it('treats an inline dispatch (no queue hop) as zero queue wait, not as a missing sample', () => {
    const clock = fakeClock(5_000);
    const registry = new TelemetryRegistry({ now: clock.now });

    registry.beginRequest('inline', { actionClass: 'read' });
    clock.advance(40);
    registry.endRequest('inline', { outcome: 'success' });

    const rendered = registry.render();
    const queueCount = `${TELEMETRY_METRIC_NAMES.queueWaitSeconds}_count{surface="typescript",action_class="read"}`;
    const queueSum = `${TELEMETRY_METRIC_NAMES.queueWaitSeconds}_sum{surface="typescript",action_class="read"}`;
    const durationSum = `${TELEMETRY_METRIC_NAMES.requestDurationSeconds}_sum{surface="typescript",action_class="read"}`;

    expect(sampleValue(rendered, queueCount)).toBe(1);
    expect(sampleValue(rendered, queueSum)).toBe(0);
    expect(sampleValue(rendered, durationSum)).toBeCloseTo(0.04, 9);
  });

  it('never emits a negative duration when the clock is not monotonic', () => {
    const clock = fakeClock(10_000);
    const registry = new TelemetryRegistry({ now: clock.now });

    registry.beginRequest('back', { actionClass: 'read' });
    clock.set(9_000);
    registry.endRequest('back', { outcome: 'success' });

    const durationSum = `${TELEMETRY_METRIC_NAMES.requestDurationSeconds}_sum{surface="typescript",action_class="read"}`;
    expect(sampleValue(registry.render(), durationSum)).toBe(0);
  });

  it('computes nearest-rank percentiles exactly', () => {
    const registry = new TelemetryRegistry({ now: () => 0 });
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
    const registry = new TelemetryRegistry({ now: () => 0 });
    expect(
      registry.quantileSeconds('request', { surface: 'typescript', actionClass: 'destructive' }, 0.95),
    ).toBeNull();
  });

  it('fills histogram buckets cumulatively with an +Inf bucket equal to count', () => {
    const registry = new TelemetryRegistry({ now: () => 0 });
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

  it('bounds the retained sample window so percentiles cannot grow unboundedly', () => {
    const registry = new TelemetryRegistry({ now: () => 0, sampleWindow: 8 });
    for (let i = 1; i <= 100; i += 1) {
      registry.observeRequest({ actionClass: 'read', outcome: 'success', durationSeconds: i });
    }

    // Window keeps the LAST 8 samples (93..100); nearest-rank p50 = 4th = 96.
    expect(registry.quantileSeconds('request', { actionClass: 'read' }, 0.5)).toBe(96);
    expect(registry.retainedSampleCount('request', { actionClass: 'read' })).toBe(8);
    // The cumulative histogram/counter is NOT windowed - it keeps counting.
    expect(
      sampleValue(registry.render(), `${TELEMETRY_METRIC_NAMES.requestDurationSeconds}_count{surface="typescript",action_class="read"}`),
    ).toBe(100);
  });

  it('counts outcomes and failure classes as real counters', () => {
    const registry = new TelemetryRegistry({ now: () => 0 });
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
    const registry = new TelemetryRegistry({ now: () => 0 });
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

  it('drops in-flight bookkeeping on terminal so the tracking map cannot leak', () => {
    const clock = fakeClock();
    const registry = new TelemetryRegistry({ now: clock.now });
    for (let i = 0; i < 50; i += 1) {
      registry.beginRequest(`r${i}`, { actionClass: 'read' });
      clock.advance(1);
      registry.endRequest(`r${i}`, { outcome: 'success' });
    }
    expect(registry.inFlightCount()).toBe(0);
  });
});
