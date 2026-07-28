// Task 47 follow-up — the rendered telemetry must be CLIENT-READABLE on BOTH
// transports through the EXISTING `ue://health` resource.
//
// The gap this closes: native counters accumulated in production and rendered
// in the shared exposition format, but no native endpoint served that text, so
// `ue://health` answered RESOURCE_UNAVAILABLE over the wire. A metric that only
// the test suite can reach is telemetry that exists in the type and is absent
// on the wire.
//
// The TypeScript half below goes through the REAL registered request handler.
// The native half is the Task 38 oracle, which is pinned to the C++ by the
// source contracts in tests/unit/plugin/task-47-native-telemetry-contracts.ts;
// the executable native proof is the automation test
// McpAutomationBridge.MCP.Resources.HealthTelemetryServed, which drives
// Classify() + BuildReadBody() — the transport's own read path.

import { describe, expect, it } from 'vitest';

import type { AutomationBridgeStatus } from '../../../src/automation/index.js';
import { ResourceHandler, type ResourceServer } from '../../../src/handlers/resource-handlers.js';
import type { ActorResources } from '../../../src/resources/actors.js';
import type { AssetResources } from '../../../src/resources/assets.js';
import type { LevelResources } from '../../../src/resources/levels.js';
import { HealthMonitor } from '../../../src/services/health-monitor.js';
import { TELEMETRY_METRIC_NAMES } from '../../../src/services/telemetry-schema.js';
import { Logger } from '../../../src/utils/logging/logger.js';
import { NATIVE_HEALTH_DATA, isNativeError, nativeRead } from '../task-38/resources-native-fixture.js';

type RegisteredResourceHandler = (request: { params: { uri: string } }) => Promise<{ contents: Array<{ text: string }> }>;

interface ServedHealth {
  readonly readiness: { readonly ready: boolean; readonly components: Record<string, boolean>; readonly notReady: readonly string[] };
  readonly diagnostics: Record<string, unknown>;
  readonly metricsExposition: string;
}

const SHARED_METRIC_NAMES = Object.values(TELEMETRY_METRIC_NAMES);

function automationStatus(connected: boolean): AutomationBridgeStatus {
  return {
    enabled: true,
    host: '127.0.0.1',
    port: 8091,
    configuredPorts: [8091],
    listeningPorts: [],
    connected,
    connectedAt: '2026-01-01T00:00:00.000Z',
    activePort: 8091,
    negotiatedProtocol: 'mcp-automation',
    supportedProtocols: ['mcp-automation'],
    supportedOpcodes: ['automation_request'],
    expectedResponseOpcodes: ['automation_response'],
    capabilityTokenRequired: true,
    lastHandshakeAt: '2026-01-01T00:00:01.000Z',
    lastHandshakeMetadata: {},
    lastHandshakeAck: { type: 'bridge_ack' },
    lastHandshakeFailure: null,
    lastDisconnect: null,
    lastError: null,
    lastMessageAt: '2026-01-01T00:00:05.000Z',
    lastRequestSentAt: '2026-01-01T00:00:06.000Z',
    pendingRequests: 0,
    pendingRequestDetails: [],
    connections: [],
    webSocketListening: false,
    serverLegacyEnabled: true,
    serverName: 'unreal-engine-mcp',
    serverVersion: '0.0.0',
    maxConcurrentConnections: 1,
    maxPendingRequests: 25,
    heartbeatIntervalMs: 10000,
  };
}

async function readTsHealth(monitor: HealthMonitor): Promise<ServedHealth> {
  let registered: RegisteredResourceHandler | undefined;
  const server = {
    setRequestHandler: (_schema: unknown, handler: unknown) => {
      registered = handler as RegisteredResourceHandler;
    },
  } as ResourceServer;

  new ResourceHandler(
    server,
    { isConnected: true, getEngineVersion: async () => ({}), getFeatureFlags: async () => ({}) },
    { getStatus: () => automationStatus(true) },
    {} as AssetResources,
    {} as ActorResources,
    {} as LevelResources,
    monitor,
    async () => true,
  ).registerHandlers();

  if (!registered) throw new Error('Resource handler was not registered');
  const response = await registered({ params: { uri: 'ue://health' } });
  return JSON.parse(response.contents[0]?.text ?? '{}') as ServedHealth;
}

function nativeHealthBody(): { readonly dataKeys: string[]; readonly text: string } {
  const result = nativeRead('ue://health');
  if (isNativeError(result)) {
    throw new Error(`native ue://health is not served: ${result.code}`);
  }
  const text = result.contents[0]?.text ?? '';
  const parsed = JSON.parse(text) as { data: Record<string, unknown> };
  return { dataKeys: Object.keys(parsed.data).sort(), text };
}

describe('Task 47 rendered telemetry is served through the existing ue://health resource', () => {
  it('the TypeScript read path serves the exposition text with every shared metric name', async () => {
    const monitor = new HealthMonitor(new Logger('HealthServedTest', 'error'));
    monitor.trackPerformance(Date.now(), true, { actionClass: 'read' });

    const served = await readTsHealth(monitor);

    expect(typeof served.metricsExposition).toBe('string');
    for (const name of SHARED_METRIC_NAMES) {
      expect(served.metricsExposition, `missing ${name} on the served ue://health`).toContain(name);
    }
  });

  it('the native read path serves ue://health rather than refusing it as editor state', () => {
    // Before this change nativeRead('ue://health') modelled the plugin's
    // RESOURCE_UNAVAILABLE refusal, which is what the live probe returned.
    expect(isNativeError(nativeRead('ue://health'))).toBe(false);
    const { text } = nativeHealthBody();
    for (const name of SHARED_METRIC_NAMES) {
      expect(text, `missing ${name} on the native served ue://health`).toContain(name);
    }
  });

  it('both transports expose the SAME anonymous aggregate shape on ue://health', async () => {
    const monitor = new HealthMonitor(new Logger('HealthServedTest', 'error'));
    monitor.trackPerformance(Date.now(), false, { actionClass: 'destructive', failureClass: 'timeout' });
    const ts = await readTsHealth(monitor);
    const { dataKeys } = nativeHealthBody();

    expect(dataKeys).toEqual(['diagnostics', 'metricsExposition', 'readiness', 'surface']);
    expect(Object.keys(ts.diagnostics).sort()).toEqual(Object.keys(NATIVE_HEALTH_DATA.diagnostics).sort());
    expect(Object.keys(ts.readiness).sort()).toEqual(Object.keys(NATIVE_HEALTH_DATA.readiness).sort());
    expect(Object.keys(ts.readiness.components).sort()).toEqual(
      Object.keys(NATIVE_HEALTH_DATA.readiness.components).sort(),
    );
  });

  it('the SERVED text carries no capability id, path, token or request id', async () => {
    const monitor = new HealthMonitor(new Logger('HealthServedTest', 'error'));
    monitor.trackPerformance(Date.now(), false, {
      actionClass: 'manage_asset.import_asset',
      failureClass: '/Game/Secret/Levels/ClientPitch',
    });

    const served = await readTsHealth(monitor);
    const wire = JSON.stringify(served);

    for (const secret of ['manage_asset', '/Game/', 'ClientPitch', 'sk-live', 'hunter2']) {
      expect(wire, `secret reached the served ue://health: ${secret}`).not.toContain(secret);
    }
  });

  it('every label value in the SERVED exposition comes from a closed set', async () => {
    const monitor = new HealthMonitor(new Logger('HealthServedTest', 'error'));
    for (let index = 0; index < 500; index += 1) {
      monitor.trackPerformance(Date.now(), false, {
        actionClass: `/Game/Secret/Asset_${index}`,
        failureClass: `Bearer sk-live-${index}`,
      });
    }

    const served = await readTsHealth(monitor);
    const observed = [...served.metricsExposition.matchAll(/="([^"]*)"/gu)].map((match) => match[1] ?? '');
    expect(observed.length).toBeGreaterThan(0);

    const bounded = new Set([
      'native', 'typescript',
      'admin', 'destructive', 'read', 'unknown', 'write',
      'failure', 'success',
      'command_blocked', 'consent_required', 'internal', 'path_not_permitted', 'project_not_permitted',
      'quota_exceeded', 'scope_not_granted', 'timeout', 'transport', 'validation',
      'editor', 'registry',
      '+Inf',
    ]);
    const offenders = observed.filter((value) => !bounded.has(value) && !Number.isFinite(Number(value)));
    expect(offenders).toEqual([]);
  });
});
