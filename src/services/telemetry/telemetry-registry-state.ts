import type {
  TelemetryActionClass,
  TelemetrySurface,
} from '../telemetry-schema.js';

export interface HistogramState {
  readonly bucketCounts: number[];
  sumSeconds: number;
  count: number;
  samples: number[];
}

export interface InFlightState {
  readonly actionClass: TelemetryActionClass;
  readonly surface: TelemetrySurface;
  readonly startedAtMs: number;
  dispatchedAtMs?: number;
}
