// Task 47 — cardinality and redaction audit.
//
// Bounded cardinality is the SECURITY requirement here, not a tidiness one: a
// metric label is a permanent, unauthenticated-readable string. This test feeds
// the registry the exact shapes that leak (capability ids, content paths,
// bearer tokens, prompt text, per-request ids) and asserts none of them can
// reach a metric NAME, LABEL or the diagnostics snapshot, and that the exported
// series count stays inside the enumerable bound no matter how many distinct
// inputs arrive.

import { describe, expect, it } from 'vitest';

import { TelemetryRegistry } from '../../../src/services/telemetry-registry.js';
import {
  TELEMETRY_ACTION_CLASSES,
  TELEMETRY_FAILURE_CLASSES,
  TELEMETRY_SURFACES,
} from '../../../src/services/telemetry-schema.js';

const SECRETS = [
  'manage_asset.import_asset',
  '/Game/Secret/Levels/ClientPitch',
  '/home/xav/Documents/nda.txt',
  'Bearer sk-live-9f2a7c41',
  'X-MCP-Capability-Token: hunter2',
  'summarize the attached contract for acme corp',
  'req-3f1c-8a90-b7e2',
  'C:\\Users\\xav\\project.uproject',
] as const;

function renderedLabelValues(rendered: string): string[] {
  return [...rendered.matchAll(/\{([^}]*)\}/g)].flatMap((match) =>
    (match[1] ?? '')
      .split(',')
      .map((pair) => pair.split('=')[1] ?? '')
      .map((value) => value.replace(/^"|"$/g, '')),
  );
}

function seriesLines(rendered: string): string[] {
  return rendered.split('\n').filter((line) => line.length > 0 && !line.startsWith('#'));
}

describe('Task 47 telemetry cardinality and redaction audit', () => {
  it('coerces every unbounded or secret-bearing dimension to a bounded value', () => {
    const registry = new TelemetryRegistry({ now: () => 0 });
    for (const secret of SECRETS) {
      registry.observeRequest({
        surface: secret,
        actionClass: secret,
        outcome: secret,
        failureClass: secret,
        durationSeconds: 0.01,
      });
    }

    const rendered = registry.render();
    for (const secret of SECRETS) {
      expect(rendered, `secret leaked into exported metrics: ${secret}`).not.toContain(secret);
    }
    // A prefix check as well, so a truncated leak is caught too.
    expect(rendered).not.toContain('manage_asset');
    expect(rendered).not.toContain('/Game/');
    expect(rendered).not.toContain('sk-live');
    expect(rendered).not.toContain('hunter2');
    expect(rendered).not.toContain('acme');

    const allowed = new Set<string>([
      ...TELEMETRY_SURFACES,
      ...TELEMETRY_ACTION_CLASSES,
      ...TELEMETRY_FAILURE_CLASSES,
      'success',
      'failure',
      'registry',
      'transport',
      'editor',
      '+Inf',
    ]);
    for (const value of renderedLabelValues(rendered)) {
      const numericLabel = /^\d+(\.\d+)?$/.test(value);
      expect(numericLabel || allowed.has(value), `unbounded label value exported: ${value}`).toBe(true);
    }
  });

  it('keeps the exported series count bounded under a high-cardinality flood', () => {
    const registry = new TelemetryRegistry({ now: () => 0 });
    for (let i = 0; i < 5_000; i += 1) {
      registry.observeRequest({
        actionClass: `capability_${i}`,
        outcome: `outcome_${i}`,
        failureClass: `/Game/Generated/Asset_${i}`,
        durationSeconds: 0.01,
      });
    }

    const lines = seriesLines(registry.render());
    // Enumerable ceiling: every family is (bounded dims) x (buckets|quantiles).
    expect(lines.length).toBeLessThan(1_000);
    expect(registry.seriesCount()).toBeLessThan(1_000);
    expect(registry.render()).not.toContain('capability_4999');
  });

  it('never lets a request id reach a label even though it keys in-flight timing', () => {
    const registry = new TelemetryRegistry({ now: () => 0 });
    registry.beginRequest('req-3f1c-8a90-b7e2', { actionClass: 'read' });
    registry.markDispatched('req-3f1c-8a90-b7e2');
    registry.endRequest('req-3f1c-8a90-b7e2', { outcome: 'success' });

    expect(registry.render()).not.toContain('req-3f1c');
    expect(JSON.stringify(registry.snapshot())).not.toContain('req-3f1c');
  });

  it('exposes an anonymous aggregate snapshot with no free-text fields', () => {
    const registry = new TelemetryRegistry({ now: () => 0 });
    registry.observeRequest({
      actionClass: '/Game/Secret/Levels/ClientPitch',
      outcome: 'failure',
      failureClass: 'Bearer sk-live-9f2a7c41',
      durationSeconds: 0.25,
    });

    const snapshot = registry.snapshot();
    const serialized = JSON.stringify(snapshot);
    for (const secret of SECRETS) {
      expect(serialized).not.toContain(secret);
    }
    expect(snapshot.totals.requests).toBe(1);
    expect(snapshot.totals.failures).toBe(1);
    for (const entry of snapshot.byActionClass) {
      expect(TELEMETRY_ACTION_CLASSES).toContain(entry.actionClass);
    }
    for (const entry of snapshot.byFailureClass) {
      expect(TELEMETRY_FAILURE_CLASSES).toContain(entry.failureClass);
    }
  });

  it('rejects a metric name or label name that is not part of the declared schema', () => {
    const registry = new TelemetryRegistry({ now: () => 0 });
    registry.observeRequest({ actionClass: 'read', outcome: 'success', durationSeconds: 0.01 });

    const labelNames = [...registry.render().matchAll(/\{([^}]*)\}/g)]
      .flatMap((match) => (match[1] ?? '').split(','))
      .map((pair) => pair.split('=')[0] ?? '');
    const allowedLabelNames = new Set(['surface', 'action_class', 'outcome', 'failure_class', 'component', 'quantile', 'le']);
    for (const labelName of labelNames) {
      expect(allowedLabelNames.has(labelName), `undeclared label name exported: ${labelName}`).toBe(true);
    }
  });
});
