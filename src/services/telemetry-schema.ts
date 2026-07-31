// src/services/telemetry-schema.ts
// Task 47: the ONE metric schema both transports export.
//
// Every metric family name, label name, bounded label value set, histogram
// bucket bound and reported quantile lives here. The native mirror is
// `plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Foundation/McpTelemetrySchema.h`
// and `tests/unit/telemetry/telemetry-schema-parity.test.ts` reads BOTH files and
// fails on any drift.
//
// Cardinality is a security boundary, not housekeeping: a Prometheus label is a
// durable, unauthenticated-readable string. Every dimension below is a CLOSED
// set, and anything that does not resolve inside it is coerced to `unknown`
// rather than exported. Capability ids, asset/file paths, tokens, prompts,
// session ids and request ids are never dimensions.

/** Which transport produced the sample. */
export const TELEMETRY_SURFACES = ['native', 'typescript'] as const;
export type TelemetrySurface = (typeof TELEMETRY_SURFACES)[number];

/**
 * The effect class of the work, mirroring the canonical capability effect
 * (`read` / `write` / `destructive`) plus `admin` for capability administration
 * and `unknown` for anything unresolved. This is deliberately the SCOPE-shaped
 * axis rather than the capability id: it answers "how dangerous was it" without
 * naming what the operator was working on.
 */
export const TELEMETRY_ACTION_CLASSES = ['admin', 'destructive', 'read', 'unknown', 'write'] as const;
export type TelemetryActionClass = (typeof TELEMETRY_ACTION_CLASSES)[number];

export const TELEMETRY_OUTCOMES = ['failure', 'success'] as const;
export type TelemetryOutcome = (typeof TELEMETRY_OUTCOMES)[number];

/**
 * Failure taxonomy. The first six mirror the shared cross-transport refusal
 * codes (`SCOPE_NOT_GRANTED`, `CONSENT_REQUIRED`, `PATH_NOT_PERMITTED`,
 * `PROJECT_NOT_PERMITTED`, `QUOTA_EXCEEDED`, `COMMAND_BLOCKED`) lowercased; the
 * rest cover transport/runtime outcomes that never reach the authorization
 * layer. Add a class to BOTH surfaces or neither.
 */
export const TELEMETRY_FAILURE_CLASSES = [
  'command_blocked',
  'consent_required',
  'internal',
  'path_not_permitted',
  'project_not_permitted',
  'quota_exceeded',
  'scope_not_granted',
  'timeout',
  'transport',
  'unknown',
  'validation',
] as const;
export type TelemetryFailureClass = (typeof TELEMETRY_FAILURE_CLASSES)[number];

/** Readiness is the conjunction of these three components. */
export const TELEMETRY_READINESS_COMPONENTS = ['editor', 'registry', 'transport'] as const;
export type TelemetryReadinessComponent = (typeof TELEMETRY_READINESS_COMPONENTS)[number];

/** Histogram bucket upper bounds in seconds (cumulative, `+Inf` implied). */
export const TELEMETRY_LATENCY_BUCKETS_SECONDS = [
  0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1, 2.5, 5, 10, 30,
] as const;

/** Quantiles published as gauges beside the histogram. */
export const TELEMETRY_QUANTILES = [0.5, 0.9, 0.95, 0.99] as const;

export const TELEMETRY_METRIC_NAMES = {
  requestDurationSeconds: 'unreal_mcp_request_duration_seconds',
  requestDurationQuantileSeconds: 'unreal_mcp_request_duration_quantile_seconds',
  queueWaitSeconds: 'unreal_mcp_queue_wait_seconds',
  queueWaitQuantileSeconds: 'unreal_mcp_queue_wait_quantile_seconds',
  requestsByClassTotal: 'unreal_mcp_requests_by_class_total',
  failuresByClassTotal: 'unreal_mcp_failures_by_class_total',
  readinessComponent: 'unreal_mcp_readiness_component',
  ready: 'unreal_mcp_ready',
} as const;

export const TELEMETRY_LABEL_NAMES = {
  surface: 'surface',
  actionClass: 'action_class',
  outcome: 'outcome',
  failureClass: 'failure_class',
  component: 'component',
  quantile: 'quantile',
  le: 'le',
} as const;

const surfaceSet: ReadonlySet<string> = new Set(TELEMETRY_SURFACES);
const actionClassSet: ReadonlySet<string> = new Set(TELEMETRY_ACTION_CLASSES);
const outcomeSet: ReadonlySet<string> = new Set(TELEMETRY_OUTCOMES);
const failureClassSet: ReadonlySet<string> = new Set(TELEMETRY_FAILURE_CLASSES);

function normalize(value: unknown): string {
  return typeof value === 'string' ? value.trim().toLowerCase() : '';
}

/** Coerce to a declared surface; anything else becomes the local surface. */
export function coerceSurface(value: unknown, fallback: TelemetrySurface = 'typescript'): TelemetrySurface {
  const candidate = normalize(value);
  return surfaceSet.has(candidate) ? (candidate as TelemetrySurface) : fallback;
}

/**
 * Coerce to a declared action class. An unresolved value becomes `unknown` — it
 * is NEVER passed through, which is what keeps a capability id or a content
 * path out of the exported label set.
 */
export function coerceActionClass(value: unknown): TelemetryActionClass {
  const candidate = normalize(value);
  return actionClassSet.has(candidate) ? (candidate as TelemetryActionClass) : 'unknown';
}

export function coerceOutcome(value: unknown, fallback: TelemetryOutcome = 'success'): TelemetryOutcome {
  const candidate = normalize(value);
  return outcomeSet.has(candidate) ? (candidate as TelemetryOutcome) : fallback;
}

/** Coerce to a declared failure class; unresolved becomes `unknown`. */
export function coerceFailureClass(value: unknown): TelemetryFailureClass {
  const candidate = normalize(value).replace(/-/g, '_');
  return failureClassSet.has(candidate) ? (candidate as TelemetryFailureClass) : 'unknown';
}

/**
 * Map a canonical capability effect string onto the action-class axis. Kept
 * beside the schema so the TypeScript and native derivations agree on the same
 * closed output set even though they read different catalogs.
 */
export function actionClassFromEffect(effect: unknown): TelemetryActionClass {
  return coerceActionClass(effect);
}
