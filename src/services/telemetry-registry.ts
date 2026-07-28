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
  TELEMETRY_LABEL_NAMES,
  TELEMETRY_LATENCY_BUCKETS_SECONDS,
  TELEMETRY_METRIC_NAMES,
  TELEMETRY_QUANTILES,
  TELEMETRY_READINESS_COMPONENTS,
  coerceActionClass,
  coerceFailureClass,
  coerceOutcome,
  coerceSurface,
  type TelemetryActionClass,
  type TelemetryFailureClass,
  type TelemetrySurface,
} from './telemetry-schema.js';

export type TelemetryTimingFamily = 'request' | 'queue';

export interface TelemetryRegistryOptions {
  /** Milliseconds. Injected so timing is deterministic under test. */
  readonly now?: () => number;
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

interface HistogramState {
  readonly bucketCounts: number[];
  sumSeconds: number;
  count: number;
  samples: number[];
}

interface InFlightState {
  readonly actionClass: TelemetryActionClass;
  readonly surface: TelemetrySurface;
  readonly startedAtMs: number;
  dispatchedAtMs?: number;
}

const DEFAULT_SAMPLE_WINDOW = 256;
/** Hard ceiling on concurrently tracked ids so an unterminated request cannot leak. */
const MAX_IN_FLIGHT = 1024;

function nonNegativeSeconds(value: number): number {
  return Number.isFinite(value) && value > 0 ? value : 0;
}

function formatNumber(value: number): string {
  return String(value);
}

export class TelemetryRegistry {
  private readonly now: () => number;
  private readonly surface: TelemetrySurface;
  private readonly sampleWindow: number;

  private readonly histograms = new Map<string, HistogramState>();
  private readonly requestCounters = new Map<string, number>();
  private readonly failureCounters = new Map<string, number>();
  private readonly inFlight = new Map<string, InFlightState>();

  private totalRequests = 0;
  private totalFailures = 0;

  constructor(options: TelemetryRegistryOptions = {}) {
    this.now = options.now ?? (() => Date.now());
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

  /**
   * Start clock-driven tracking for one request id. The id is a MAP KEY only —
   * it is never a label and never reaches exported text or the snapshot.
   */
  beginRequest(id: string, meta: { actionClass?: unknown; surface?: unknown } = {}): void {
    if (!id) return;
    if (this.inFlight.size >= MAX_IN_FLIGHT) {
      const oldest = this.inFlight.keys().next();
      if (!oldest.done) this.inFlight.delete(oldest.value);
    }
    this.inFlight.set(id, {
      actionClass: coerceActionClass(meta.actionClass),
      surface: coerceSurface(meta.surface, this.surface),
      startedAtMs: this.now(),
    });
  }

  /** Close the queue-wait interval: the request is now being dispatched. */
  markDispatched(id: string): void {
    const state = this.inFlight.get(id);
    if (!state) return;
    state.dispatchedAtMs = this.now();
  }

  /** Terminal result: emits queue wait + duration and drops the tracking entry. */
  endRequest(id: string, result: { outcome?: unknown; failureClass?: unknown } = {}): void {
    const state = this.inFlight.get(id);
    if (!state) return;
    this.inFlight.delete(id);

    const endedAtMs = this.now();
    const dispatchedAtMs = state.dispatchedAtMs ?? state.startedAtMs;
    this.observeRequest({
      surface: state.surface,
      actionClass: state.actionClass,
      outcome: result.outcome,
      failureClass: result.failureClass,
      durationSeconds: nonNegativeSeconds((endedAtMs - dispatchedAtMs) / 1000),
      queueWaitSeconds: nonNegativeSeconds((dispatchedAtMs - state.startedAtMs) / 1000),
    });
  }

  /** Nearest-rank percentile over the retained window, or null when empty. */
  quantileSeconds(
    family: TelemetryTimingFamily,
    selector: TelemetrySeriesSelector,
    quantile: number,
  ): number | null {
    const state = this.histograms.get(this.seriesKey(family, selector));
    if (!state || state.samples.length === 0) return null;
    const sorted = [...state.samples].sort((a, b) => a - b);
    const rank = Math.min(sorted.length, Math.max(1, Math.ceil(quantile * sorted.length)));
    return sorted[rank - 1] ?? null;
  }

  retainedSampleCount(family: TelemetryTimingFamily, selector: TelemetrySeriesSelector): number {
    return this.histograms.get(this.seriesKey(family, selector))?.samples.length ?? 0;
  }

  inFlightCount(): number {
    return this.inFlight.size;
  }

  /** Total exported series, used by the cardinality audit. */
  seriesCount(): number {
    return this.histograms.size + this.requestCounters.size + this.failureCounters.size;
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
    const lines: string[] = [];

    this.renderHistogram(lines, 'request', TELEMETRY_METRIC_NAMES.requestDurationSeconds, 'Handler dispatch duration in seconds.');
    this.renderQuantiles(lines, 'request', TELEMETRY_METRIC_NAMES.requestDurationQuantileSeconds, 'Handler dispatch duration percentiles in seconds.');
    this.renderHistogram(lines, 'queue', TELEMETRY_METRIC_NAMES.queueWaitSeconds, 'Time a request waited in the serialized editor queue, in seconds.');
    this.renderQuantiles(lines, 'queue', TELEMETRY_METRIC_NAMES.queueWaitQuantileSeconds, 'Queue wait percentiles in seconds.');

    lines.push(`# HELP ${TELEMETRY_METRIC_NAMES.requestsByClassTotal} Requests by bounded action class and outcome.`);
    lines.push(`# TYPE ${TELEMETRY_METRIC_NAMES.requestsByClassTotal} counter`);
    for (const [key, value] of [...this.requestCounters].sort(sortByKey)) {
      const [surface, actionClass, outcome] = key.split('\u0000');
      lines.push(
        `${TELEMETRY_METRIC_NAMES.requestsByClassTotal}{${TELEMETRY_LABEL_NAMES.surface}="${surface}",${TELEMETRY_LABEL_NAMES.actionClass}="${actionClass}",${TELEMETRY_LABEL_NAMES.outcome}="${outcome}"} ${value}`,
      );
    }

    lines.push(`# HELP ${TELEMETRY_METRIC_NAMES.failuresByClassTotal} Failures by bounded action class and failure class.`);
    lines.push(`# TYPE ${TELEMETRY_METRIC_NAMES.failuresByClassTotal} counter`);
    for (const [key, value] of [...this.failureCounters].sort(sortByKey)) {
      const [surface, actionClass, failureClass] = key.split('\u0000');
      lines.push(
        `${TELEMETRY_METRIC_NAMES.failuresByClassTotal}{${TELEMETRY_LABEL_NAMES.surface}="${surface}",${TELEMETRY_LABEL_NAMES.actionClass}="${actionClass}",${TELEMETRY_LABEL_NAMES.failureClass}="${failureClass}"} ${value}`,
      );
    }

    lines.push(`# HELP ${TELEMETRY_METRIC_NAMES.readinessComponent} Readiness of each dependency (1 ready, 0 not ready).`);
    lines.push(`# TYPE ${TELEMETRY_METRIC_NAMES.readinessComponent} gauge`);
    if (readiness) {
      for (const component of TELEMETRY_READINESS_COMPONENTS) {
        const ok = readiness.components[component] === true;
        lines.push(`${TELEMETRY_METRIC_NAMES.readinessComponent}{${TELEMETRY_LABEL_NAMES.component}="${component}"} ${ok ? 1 : 0}`);
      }
    }

    lines.push(`# HELP ${TELEMETRY_METRIC_NAMES.ready} Whether the server is ready to serve requests (1 ready, 0 not ready).`);
    lines.push(`# TYPE ${TELEMETRY_METRIC_NAMES.ready} gauge`);
    if (readiness) {
      lines.push(`${TELEMETRY_METRIC_NAMES.ready} ${readiness.ready ? 1 : 0}`);
    }

    return `${lines.join('\n')}\n`;
  }

  private renderHistogram(lines: string[], family: TelemetryTimingFamily, name: string, help: string): void {
    lines.push(`# HELP ${name} ${help}`);
    lines.push(`# TYPE ${name} histogram`);
    for (const [key, state] of this.seriesFor(family)) {
      const labels = `${TELEMETRY_LABEL_NAMES.surface}="${key.surface}",${TELEMETRY_LABEL_NAMES.actionClass}="${key.actionClass}"`;
      let cumulative = 0;
      TELEMETRY_LATENCY_BUCKETS_SECONDS.forEach((bound, index) => {
        cumulative += state.bucketCounts[index] ?? 0;
        lines.push(`${name}_bucket{${labels},${TELEMETRY_LABEL_NAMES.le}="${formatNumber(bound)}"} ${cumulative}`);
      });
      lines.push(`${name}_bucket{${labels},${TELEMETRY_LABEL_NAMES.le}="+Inf"} ${state.count}`);
      lines.push(`${name}_sum{${labels}} ${formatNumber(state.sumSeconds)}`);
      lines.push(`${name}_count{${labels}} ${state.count}`);
    }
  }

  private renderQuantiles(lines: string[], family: TelemetryTimingFamily, name: string, help: string): void {
    lines.push(`# HELP ${name} ${help}`);
    lines.push(`# TYPE ${name} gauge`);
    for (const [key] of this.seriesFor(family)) {
      const labels = `${TELEMETRY_LABEL_NAMES.surface}="${key.surface}",${TELEMETRY_LABEL_NAMES.actionClass}="${key.actionClass}"`;
      for (const quantile of TELEMETRY_QUANTILES) {
        const value = this.quantileSeconds(family, key, quantile);
        if (value === null) continue;
        lines.push(`${name}{${labels},${TELEMETRY_LABEL_NAMES.quantile}="${formatNumber(quantile)}"} ${formatNumber(value)}`);
      }
    }
  }

  private seriesFor(
    family: TelemetryTimingFamily,
  ): Array<[{ surface: TelemetrySurface; actionClass: TelemetryActionClass }, HistogramState]> {
    const prefix = `${family}\u0000`;
    return [...this.histograms]
      .filter(([key]) => key.startsWith(prefix))
      .sort(sortByKey)
      .map(([key, state]) => {
        const [, surface, actionClass] = key.split('\u0000');
        return [
          {
            surface: coerceSurface(surface, this.surface),
            actionClass: coerceActionClass(actionClass),
          },
          state,
        ];
      });
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
    if (samples.length === 0) return null;
    samples.sort((a, b) => a - b);
    const rank = Math.min(samples.length, Math.max(1, Math.ceil(quantile * samples.length)));
    return samples[rank - 1] ?? null;
  }
}

function sortByKey(a: [string, unknown], b: [string, unknown]): number {
  return a[0].localeCompare(b[0]);
}
