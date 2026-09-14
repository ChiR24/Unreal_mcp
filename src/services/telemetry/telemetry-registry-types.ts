import type {
  TelemetryActionClass,
  TelemetryFailureClass,
  TelemetrySurface,
} from '../telemetry-schema.js';

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
