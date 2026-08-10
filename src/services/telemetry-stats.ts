// src/services/telemetry-stats.ts
// Pure numeric helpers for the TelemetryRegistry: nearest-rank percentiles and
// byte-order series-key ordering. Extracted from telemetry-registry.ts so the
// registry class keeps only state and orchestration.

import { compareEntryKey } from '../utils/serialization/ordering.js';

export function nonNegativeSeconds(value: number): number {
  return Number.isFinite(value) && value > 0 ? value : 0;
}

export function formatNumber(value: number): string {
  return String(value);
}

/**
 * Nearest-rank percentile over an unsorted sample window.
 *
 * One implementation for both the per-series and the aggregate percentile; they
 * carried the same four lines and had to be kept in step by hand. Copies the
 * input before sorting so a caller's retained window is never reordered.
 */
export function nearestRank(samples: readonly number[], quantile: number): number | null {
  if (samples.length === 0) return null;
  const sorted = [...samples].sort((a, b) => a - b);
  const rank = Math.min(sorted.length, Math.max(1, Math.ceil(quantile * sorted.length)));
  return sorted[rank - 1] ?? null;
}

// Byte order, not localeCompare: these keys are NUL-joined
// `surface\u0000actionClass\u0000outcome` tuples, and U+0000 carries no collation
// weight, so localeCompare compares the CONCATENATION rather than the fields and
// can report two structurally different series as equal. It is also locale- and
// ICU-build dependent, which a scrape ordering must never be.
export const sortByKey = compareEntryKey;
