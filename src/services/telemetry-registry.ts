// src/services/telemetry-registry.ts
// Task 47: real counters, histograms and percentiles for the TypeScript surface.
//
// Design constraints that are load-bearing:
//  * The clock is INJECTED. Queue-wait and duration are millisecond deltas from
//    `now()`, so tests drive exact values instead of sleeping.
//  * Every dimension is coerced through the closed sets in `telemetry-schema.ts`
//    BEFORE it becomes a map key, so an unbounded or secret-bearing input can
//    neither create a series nor appear in exported text.
//  * Percentile samples live in a bounded ring per series; the counters and
//    histogram buckets keep counting so a scrape is still cumulative.
//  * This module is OBSERVATION ONLY. Nothing here feeds back into routing,
//    retry, scheduling or authorization decisions.

import {
  TELEMETRY_ACTION_CLASSES,
  TELEMETRY_FAILURE_CLASSES,
  TELEMETRY_LATENCY_BUCKETS_SECONDS,
  coerceActionClass,
  coerceFailureClass,
  coerceOutcome,
  coerceSurface,
  type TelemetryActionClass,
  type TelemetryFailureClass,
  type TelemetrySurface,
} from './telemetry-schema.js';
import { renderPrometheus } from './telemetry/prometheus-exposition.js';

export type TelemetryTimingFamily = 'request' | 'queue';

export interface HistogramState {
  readonly bucketCounts: number[];
  sumSeconds: number;
  count: number;
  samples: number[];
}

export interface TelemetryRegistryOptions {
  /** Surface recorded for locally produced samples. */
  readonly surface?: TelemetrySurface;
  /** Percentile ring size per series. */
  readonly sampleWindow?: number;
}

export interface RequestObservation {
  readonly surface?: unknown;
  readonly actionClass?: unknown;
  readonly outcome?: unknown;
  readonly failureClass?: unknown;
  readonly durationSeconds: number;
  readonly queueWaitSeconds?: number;
}

export interface TelemetrySeriesSelector {
  readonly surface?: unknown;
  readonly actionClass?: unknown;
}

export interface TelemetryReadinessView {
  readonly ready: boolean;
  readonly components: Readonly<Record<string, boolean>>;
}

export interface TelemetrySnapshot {
  readonly totals: { readonly requests: number; readonly failures: number };
  readonly byActionClass: ReadonlyArray<{
    readonly actionClass: TelemetryActionClass;
    readonly count: number;
    readonly failures: number;
    readonly p50Seconds: number | null;
    readonly p95Seconds: number | null;
  }>;
  readonly byFailureClass: ReadonlyArray<{
    readonly failureClass: TelemetryFailureClass;
    readonly count: number;
  }>;
  readonly queueWait: { readonly p50Seconds: number | null; readonly p95Seconds: number | null };
}

function nonNegativeSeconds(value: number): number {
  return Number.isFinite(value) && value > 0 ? value : 0;
}

/** Nearest-rank percentile over an unsorted window (copied, so the caller's ring keeps its order). */
function nearestRank(samples: readonly number[], quantile: number): number | null {
  if (samples.length === 0) return null;
  const sorted = [...samples].sort((a, b) => a - b);
  const rank = Math.min(sorted.length, Math.max(1, Math.ceil(quantile * sorted.length)));
  return sorted[rank - 1] ?? null;
}

const DEFAULT_SAMPLE_WINDOW = 256;

export class TelemetryRegistry {
  private readonly surface: TelemetrySurface;
  private readonly sampleWindow: number;

  private readonly histograms = new Map<string, HistogramState>();
  private readonly requestCounters = new Map<string, number>();
  private readonly failureCounters = new Map<string, number>();

  private totalRequests = 0;
  private totalFailures = 0;

  constructor(options: TelemetryRegistryOptions = {}) {
    this.surface = coerceSurface(options.surface, 'typescript');
    this.sampleWindow =
      Number.isInteger(options.sampleWindow) && (options.sampleWindow ?? 0) > 0
        ? (options.sampleWindow as number)
        : DEFAULT_SAMPLE_WINDOW;
  }

  /** Record a completed request whose timings the caller already measured. */
  observeRequest(observation: RequestObservation): void {
    const surface = coerceSurface(observation.surface, this.surface);
    const actionClass = coerceActionClass(observation.actionClass);
    const outcome = coerceOutcome(observation.outcome);

    this.observeHistogram('request', surface, actionClass, nonNegativeSeconds(observation.durationSeconds));
    if (observation.queueWaitSeconds !== undefined) {
      this.observeHistogram('queue', surface, actionClass, nonNegativeSeconds(observation.queueWaitSeconds));
    }

    this.totalRequests += 1;
    this.bump(this.requestCounters, `${surface}\u0000${actionClass}\u0000${outcome}`);

    if (outcome === 'failure') {
      this.totalFailures += 1;
      const failureClass = coerceFailureClass(observation.failureClass);
      this.bump(this.failureCounters, `${surface}\u0000${actionClass}\u0000${failureClass}`);
    }
  }

  /** Nearest-rank percentile over the retained window, or null when empty. */
  quantileSeconds(
    family: TelemetryTimingFamily,
    selector: TelemetrySeriesSelector,
    quantile: number,
  ): number | null {
    const state = this.histograms.get(this.seriesKey(family, selector));
    if (!state || state.samples.length === 0) return null;
    return nearestRank(state.samples, quantile);
  }

  /** Bounded, anonymous aggregate for the read-only resource surface. */
  snapshot(): TelemetrySnapshot {
    const byActionClass = TELEMETRY_ACTION_CLASSES.map((actionClass) => {
      const count = this.sumMatching(this.requestCounters, actionClass, 1);
      const failures = this.sumMatching(this.failureCounters, actionClass, 1);
      return {
        actionClass,
        count,
        failures,
        p50Seconds: this.quantileSeconds('request', { actionClass }, 0.5),
        p95Seconds: this.quantileSeconds('request', { actionClass }, 0.95),
      };
    }).filter((entry) => entry.count > 0);

    const byFailureClass = TELEMETRY_FAILURE_CLASSES.map((failureClass) => ({
      failureClass,
      count: this.sumMatching(this.failureCounters, failureClass, 2),
    })).filter((entry) => entry.count > 0);

    return {
      totals: { requests: this.totalRequests, failures: this.totalFailures },
      byActionClass,
      byFailureClass,
      queueWait: {
        p50Seconds: this.aggregateQuantile('queue', 0.5),
        p95Seconds: this.aggregateQuantile('queue', 0.95),
      },
    };
  }

  /** Prometheus text exposition. Family headers are always present. */
  render(readiness?: TelemetryReadinessView): string {
    return renderPrometheus(
      this.histograms,
      this.requestCounters,
      this.failureCounters,
      (family, selector, quantile) => this.quantileSeconds(family, selector, quantile),
      this.surface,
      readiness,
    );
  }

  private seriesKey(family: TelemetryTimingFamily, selector: TelemetrySeriesSelector): string {
    const surface = coerceSurface(selector.surface, this.surface);
    const actionClass = coerceActionClass(selector.actionClass);
    return `${family}\u0000${surface}\u0000${actionClass}`;
  }

  private observeHistogram(
    family: TelemetryTimingFamily,
    surface: TelemetrySurface,
    actionClass: TelemetryActionClass,
    seconds: number,
  ): void {
    const key = `${family}\u0000${surface}\u0000${actionClass}`;
    let state = this.histograms.get(key);
    if (!state) {
      state = {
        bucketCounts: new Array<number>(TELEMETRY_LATENCY_BUCKETS_SECONDS.length).fill(0),
        sumSeconds: 0,
        count: 0,
        samples: [],
      };
      this.histograms.set(key, state);
    }

    const bucketIndex = TELEMETRY_LATENCY_BUCKETS_SECONDS.findIndex((bound) => seconds <= bound);
    if (bucketIndex >= 0) {
      state.bucketCounts[bucketIndex] = (state.bucketCounts[bucketIndex] ?? 0) + 1;
    }
    state.sumSeconds += seconds;
    state.count += 1;

    state.samples.push(seconds);
    if (state.samples.length > this.sampleWindow) {
      state.samples = state.samples.slice(state.samples.length - this.sampleWindow);
    }
  }

  private bump(counters: Map<string, number>, key: string): void {
    counters.set(key, (counters.get(key) ?? 0) + 1);
  }

  private sumMatching(counters: Map<string, number>, value: string, position: number): number {
    let total = 0;
    for (const [key, count] of counters) {
      if (key.split('\u0000')[position] === value) total += count;
    }
    return total;
  }

  private aggregateQuantile(family: TelemetryTimingFamily, quantile: number): number | null {
    const samples: number[] = [];
    for (const [key, state] of this.histograms) {
      if (key.startsWith(`${family}\u0000`)) samples.push(...state.samples);
    }
    return nearestRank(samples, quantile);
  }
}
