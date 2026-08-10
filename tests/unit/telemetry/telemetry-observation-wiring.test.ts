// Task 47 — the action_class dimension must be REAL on the wire.
//
// A bounded dimension that is only ever populated with its fallback is a
// dimension in the type and nothing on the wire. These assertions run the
// derivation against the REAL generated capability registry (not a fixture) in
// the exact argument shape the `unreal` gateway receives, and then check that
// the production call sites actually pass it.

import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

import { capabilityIndex } from '../../../src/server/gateway/gateway-capability-index.js';
import { HealthMonitor } from '../../../src/services/health-monitor.js';
import {
  actionClassForGatewayArgs,
  failureClassForError,
} from '../../../src/services/telemetry-observation.js';
import { TELEMETRY_METRIC_NAMES } from '../../../src/services/telemetry-schema.js';
import { Logger } from '../../../src/utils/logging/logger.js';

function firstCapabilityWithScope(scope: string): string {
  for (const record of capabilityIndex().records) {
    if (record.policy.requiredScope === scope) return record.id;
  }
  throw new Error(`the generated registry has no capability with requiredScope=${scope}`);
}

describe('Task 47 action_class derivation against the real registry', () => {
  it('derives read/write/destructive from the canonical capability policy', () => {
    for (const scope of ['read', 'write', 'destructive']) {
      const capability = firstCapabilityWithScope(scope);
      expect(
        actionClassForGatewayArgs({ operation: 'execute', capability }),
        `capability ${capability} should classify as ${scope}`,
      ).toBe(scope);
    }
  });

  it('derives the class from a legacy tool+action pair as well as a canonical id', () => {
    const record = capabilityIndex().records.find((entry) => entry.legacyIds.length > 0);
    expect(record).toBeDefined();
    if (!record) return;
    const legacy = record.legacyIds[0];
    expect(legacy).toBeDefined();
    if (!legacy) return;

    expect(actionClassForGatewayArgs({ operation: 'execute', tool: legacy.tool, action: legacy.action })).toBe(
      record.policy.requiredScope,
    );
  });

  it('classifies discovery operations as reads, not as unknown', () => {
    expect(actionClassForGatewayArgs({ operation: 'search', query: 'spawn actor' })).toBe('read');
    expect(actionClassForGatewayArgs({ operation: 'describe', tool: 'control_actor' })).toBe('read');
  });

  it('never classifies an unresolved capability as anything but unknown', () => {
    expect(actionClassForGatewayArgs({ operation: 'execute', capability: 'not.a.capability' })).toBe('unknown');
    expect(actionClassForGatewayArgs({ operation: 'execute', capability: '/Game/Secret/Level' })).toBe('unknown');
    expect(actionClassForGatewayArgs({})).toBe('unknown');
  });

  it('maps the shared cross-transport refusal codes onto the bounded failure classes', () => {
    expect(failureClassForError({ errorCode: 'SCOPE_NOT_GRANTED' })).toBe('scope_not_granted');
    expect(failureClassForError({ errorCode: 'CONSENT_REQUIRED' })).toBe('consent_required');
    expect(failureClassForError({ code: 'PATH_NOT_PERMITTED' })).toBe('path_not_permitted');
    expect(failureClassForError({ code: 'PROJECT_NOT_PERMITTED' })).toBe('project_not_permitted');
    expect(failureClassForError({ code: 'QUOTA_EXCEEDED' })).toBe('quota_exceeded');
    expect(failureClassForError({ code: 'COMMAND_BLOCKED' })).toBe('command_blocked');
  });

  it('classifies transport and timeout failures without echoing their message', () => {
    expect(failureClassForError(new Error('Automation bridge not connected'))).toBe('transport');
    expect(failureClassForError(new Error('Request timed out after 60000ms'))).toBe('timeout');
    expect(failureClassForError(new Error('/Game/Secret/ClientPitch failed to load'))).toBe('unknown');
  });

  it('reaches the exported metric text with a non-fallback dimension', () => {
    const monitor = new HealthMonitor(new Logger('ObservationTest', 'error'));
    const capability = firstCapabilityWithScope('destructive');
    monitor.trackPerformance(Date.now(), false, {
      actionClass: actionClassForGatewayArgs({ operation: 'execute', capability }),
      failureClass: failureClassForError({ errorCode: 'SCOPE_NOT_GRANTED' }),
    });

    const rendered = monitor.telemetry.render();
    expect(rendered).toContain(
      `${TELEMETRY_METRIC_NAMES.failuresByClassTotal}{surface="typescript",action_class="destructive",failure_class="scope_not_granted"} 1`,
    );
    expect(rendered).not.toContain(capability);
  });
});

describe('Task 47 production call sites pass the bounded dimensions', () => {
  it('never calls trackPerformance without dimensions in the tool registry', () => {
    const source = readFileSync(resolve(process.cwd(), 'src/server/tool-registry.ts'), 'utf8');
    const calls = [...source.matchAll(/trackPerformance\(([^)]*)\)/g)].map((match) => match[1] ?? '');
    expect(calls.length, 'expected the registry to still track performance').toBeGreaterThan(0);
    for (const call of calls) {
      const argumentCount = call.split(',').length;
      expect(argumentCount, `trackPerformance(${call}) drops the telemetry dimensions`).toBeGreaterThanOrEqual(3);
    }
  });
});
