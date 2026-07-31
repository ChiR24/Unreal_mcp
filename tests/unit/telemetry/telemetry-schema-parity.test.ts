// Task 47 — transport-consistent telemetry schema parity.
//
// The metric schema is ONE contract exported by two surfaces. This test reads
// BOTH implementations (the TypeScript module and the native C++ header) and
// asserts they publish byte-identical metric names, label names, bounded label
// value sets, histogram bucket bounds and reported quantiles.
//
// It deliberately does not trust either side's own type declaration: the native
// header is parsed as TEXT out of the plugin source tree, which is what the
// compiler actually sees, so "true in the type, false on the wire" cannot pass.

import { existsSync, readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

import {
  TELEMETRY_ACTION_CLASSES,
  TELEMETRY_FAILURE_CLASSES,
  TELEMETRY_LABEL_NAMES,
  TELEMETRY_LATENCY_BUCKETS_SECONDS,
  TELEMETRY_METRIC_NAMES,
  TELEMETRY_OUTCOMES,
  TELEMETRY_QUANTILES,
  TELEMETRY_READINESS_COMPONENTS,
  TELEMETRY_SURFACES,
} from '../../../src/services/telemetry-schema.js';

const NATIVE_SCHEMA_PATH = resolve(
  process.cwd(),
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Foundation/McpTelemetrySchema.h',
);

function readNativeSchema(): string {
  expect(existsSync(NATIVE_SCHEMA_PATH), `missing native schema mirror: ${NATIVE_SCHEMA_PATH}`).toBe(true);
  return readFileSync(NATIVE_SCHEMA_PATH, 'utf8');
}

/**
 * Extract the source between `// MCP_TELEMETRY_SCHEMA_BEGIN <region>` and the
 * matching END marker. Regions are the explicit mirror contract: adding a metric
 * on one surface without the other leaves a region mismatch this test reports.
 */
function region(source: string, name: string): string {
  const begin = `MCP_TELEMETRY_SCHEMA_BEGIN ${name}`;
  const end = `MCP_TELEMETRY_SCHEMA_END ${name}`;
  const beginIndex = source.indexOf(begin);
  const endIndex = source.indexOf(end);
  expect(beginIndex, `native schema region not found: ${name}`).toBeGreaterThanOrEqual(0);
  expect(endIndex, `native schema region not terminated: ${name}`).toBeGreaterThan(beginIndex);
  return source.slice(beginIndex + begin.length, endIndex);
}

function textLiterals(block: string): string[] {
  return [...block.matchAll(/TEXT\("([^"]*)"\)/g)].map((match) => match[1] ?? '');
}

function numberLiterals(block: string): number[] {
  // Strip comments first so a documented example number cannot masquerade as a
  // bucket bound.
  const code = block.replace(/\/\/[^\n]*/g, '').replace(/\/\*[\s\S]*?\*\//g, '');
  return [...code.matchAll(/-?\d+\.?\d*(?:e-?\d+)?/gi)].map((match) => Number(match[0]));
}

const sorted = (values: readonly string[]): string[] => [...values].sort();

describe('Task 47 telemetry schema parity (TypeScript <-> native)', () => {
  it('exports the same metric family names on both surfaces', () => {
    const native = textLiterals(region(readNativeSchema(), 'MetricNames'));
    expect(sorted(native)).toEqual(sorted(Object.values(TELEMETRY_METRIC_NAMES)));
  });

  it('exports the same label names on both surfaces', () => {
    const native = textLiterals(region(readNativeSchema(), 'LabelNames'));
    expect(sorted(native)).toEqual(sorted(Object.values(TELEMETRY_LABEL_NAMES)));
  });

  it('bounds the surface label to the same values on both surfaces', () => {
    const native = textLiterals(region(readNativeSchema(), 'SurfaceValues'));
    expect(sorted(native)).toEqual(sorted(TELEMETRY_SURFACES));
  });

  it('bounds the action_class label to the same values on both surfaces', () => {
    const native = textLiterals(region(readNativeSchema(), 'ActionClassValues'));
    expect(sorted(native)).toEqual(sorted(TELEMETRY_ACTION_CLASSES));
  });

  it('bounds the outcome label to the same values on both surfaces', () => {
    const native = textLiterals(region(readNativeSchema(), 'OutcomeValues'));
    expect(sorted(native)).toEqual(sorted(TELEMETRY_OUTCOMES));
  });

  it('bounds the failure_class label to the same values on both surfaces', () => {
    const native = textLiterals(region(readNativeSchema(), 'FailureClassValues'));
    expect(sorted(native)).toEqual(sorted(TELEMETRY_FAILURE_CLASSES));
  });

  it('bounds the readiness component label to the same values on both surfaces', () => {
    const native = textLiterals(region(readNativeSchema(), 'ReadinessComponentValues'));
    expect(sorted(native)).toEqual(sorted(TELEMETRY_READINESS_COMPONENTS));
  });

  it('uses the same histogram bucket upper bounds on both surfaces', () => {
    const native = numberLiterals(region(readNativeSchema(), 'LatencyBuckets'));
    expect(native).toEqual([...TELEMETRY_LATENCY_BUCKETS_SECONDS]);
  });

  it('reports the same quantiles on both surfaces', () => {
    const native = numberLiterals(region(readNativeSchema(), 'Quantiles'));
    expect(native).toEqual([...TELEMETRY_QUANTILES]);
  });

  it('keeps every bounded label value set strictly bounded and lowercase', () => {
    const allValues = [
      ...TELEMETRY_SURFACES,
      ...TELEMETRY_ACTION_CLASSES,
      ...TELEMETRY_OUTCOMES,
      ...TELEMETRY_FAILURE_CLASSES,
      ...TELEMETRY_READINESS_COMPONENTS,
    ];
    for (const value of allValues) {
      expect(value, `label value must be lowercase snake_case: ${value}`).toMatch(/^[a-z][a-z0-9_]*$/);
    }
    // A bounded set is only bounded if it is small enough to enumerate. The
    // product of every dimension is the worst-case series count.
    const worstCaseSeries =
      TELEMETRY_SURFACES.length *
      TELEMETRY_ACTION_CLASSES.length *
      TELEMETRY_FAILURE_CLASSES.length;
    expect(worstCaseSeries).toBeLessThanOrEqual(256);
  });
});
