import {
  TELEMETRY_LABEL_NAMES,
  TELEMETRY_LATENCY_BUCKETS_SECONDS,
  TELEMETRY_METRIC_NAMES,
  TELEMETRY_QUANTILES,
  TELEMETRY_READINESS_COMPONENTS,
  coerceActionClass,
  coerceSurface,
  type TelemetryActionClass,
  type TelemetrySurface,
} from '../telemetry-schema.js';
import { formatNumber, sortByKey } from '../telemetry-stats.js';
import type { HistogramState } from './telemetry-registry-state.js';
import type { TelemetryReadinessView, TelemetryTimingFamily } from './telemetry-registry-types.js';

type SeriesKey = { surface: TelemetrySurface; actionClass: TelemetryActionClass };
type QuantileLookup = (
  family: TelemetryTimingFamily,
  selector: SeriesKey,
  quantile: number,
) => number | null;

/**
 * Prometheus text exposition. Family headers are always present, even when the
 * registry holds no samples for them.
 */
export function renderPrometheus(
  histograms: ReadonlyMap<string, HistogramState>,
  requestCounters: ReadonlyMap<string, number>,
  failureCounters: ReadonlyMap<string, number>,
  quantileSeconds: QuantileLookup,
  fallbackSurface: TelemetrySurface,
  readiness?: TelemetryReadinessView,
): string {
  const lines: string[] = [];

  renderHistogram(histograms, fallbackSurface, lines, 'request', TELEMETRY_METRIC_NAMES.requestDurationSeconds, 'Handler dispatch duration in seconds.');
  renderQuantiles(histograms, fallbackSurface, quantileSeconds, lines, 'request', TELEMETRY_METRIC_NAMES.requestDurationQuantileSeconds, 'Handler dispatch duration percentiles in seconds.');
  renderHistogram(histograms, fallbackSurface, lines, 'queue', TELEMETRY_METRIC_NAMES.queueWaitSeconds, 'Time a request waited in the serialized editor queue, in seconds.');
  renderQuantiles(histograms, fallbackSurface, quantileSeconds, lines, 'queue', TELEMETRY_METRIC_NAMES.queueWaitQuantileSeconds, 'Queue wait percentiles in seconds.');

  lines.push(`# HELP ${TELEMETRY_METRIC_NAMES.requestsByClassTotal} Requests by bounded action class and outcome.`);
  lines.push(`# TYPE ${TELEMETRY_METRIC_NAMES.requestsByClassTotal} counter`);
  for (const [key, value] of [...requestCounters].sort(sortByKey)) {
    const [surface, actionClass, outcome] = key.split('\u0000');
    lines.push(
      `${TELEMETRY_METRIC_NAMES.requestsByClassTotal}{${TELEMETRY_LABEL_NAMES.surface}="${surface}",${TELEMETRY_LABEL_NAMES.actionClass}="${actionClass}",${TELEMETRY_LABEL_NAMES.outcome}="${outcome}"} ${value}`,
    );
  }

  lines.push(`# HELP ${TELEMETRY_METRIC_NAMES.failuresByClassTotal} Failures by bounded action class and failure class.`);
  lines.push(`# TYPE ${TELEMETRY_METRIC_NAMES.failuresByClassTotal} counter`);
  for (const [key, value] of [...failureCounters].sort(sortByKey)) {
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

function renderHistogram(
  histograms: ReadonlyMap<string, HistogramState>,
  fallbackSurface: TelemetrySurface,
  lines: string[],
  family: TelemetryTimingFamily,
  name: string,
  help: string,
): void {
  lines.push(`# HELP ${name} ${help}`);
  lines.push(`# TYPE ${name} histogram`);
  for (const [key, state] of seriesFor(histograms, family, fallbackSurface)) {
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

function renderQuantiles(
  histograms: ReadonlyMap<string, HistogramState>,
  fallbackSurface: TelemetrySurface,
  quantileSeconds: QuantileLookup,
  lines: string[],
  family: TelemetryTimingFamily,
  name: string,
  help: string,
): void {
  lines.push(`# HELP ${name} ${help}`);
  lines.push(`# TYPE ${name} gauge`);
  for (const [key] of seriesFor(histograms, family, fallbackSurface)) {
    const labels = `${TELEMETRY_LABEL_NAMES.surface}="${key.surface}",${TELEMETRY_LABEL_NAMES.actionClass}="${key.actionClass}"`;
    for (const quantile of TELEMETRY_QUANTILES) {
      const value = quantileSeconds(family, key, quantile);
      if (value === null) continue;
      lines.push(`${name}{${labels},${TELEMETRY_LABEL_NAMES.quantile}="${formatNumber(quantile)}"} ${formatNumber(value)}`);
    }
  }
}

export function seriesFor(
  histograms: ReadonlyMap<string, HistogramState>,
  family: TelemetryTimingFamily,
  fallbackSurface: TelemetrySurface,
): Array<[SeriesKey, HistogramState]> {
  const prefix = `${family}\u0000`;
  return [...histograms]
    .filter(([key]) => key.startsWith(prefix))
    .sort(sortByKey)
    .map(([key, state]) => {
      const [, surface, actionClass] = key.split('\u0000');
      return [
        {
          surface: coerceSurface(surface, fallbackSurface),
          actionClass: coerceActionClass(actionClass),
        },
        state,
      ];
    });
}
