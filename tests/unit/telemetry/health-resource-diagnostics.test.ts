// Task 47 — readiness and anonymous aggregate diagnostics on the EXISTING
// `ue://health` resource surface.
//
// The resource is exercised through the real registered request handler (the
// same path an MCP client takes), not through a hand-authored payload builder,
// so a field that exists in the type but never reaches the wire fails here.

import { describe, expect, it } from 'vitest';

import type { AutomationBridgeStatus } from '../../../src/automation/index.js';
import { ResourceHandler, type ResourceServer } from '../../../src/handlers/resource-handlers.js';
import type { ActorResources } from '../../../src/resources/actors.js';
import type { AssetResources } from '../../../src/resources/assets.js';
import type { LevelResources } from '../../../src/resources/levels.js';
import { HealthMonitor } from '../../../src/services/health-monitor.js';
import { TELEMETRY_ACTION_CLASSES, TELEMETRY_FAILURE_CLASSES } from '../../../src/services/telemetry-schema.js';
import { Logger } from '../../../src/utils/logging/logger.js';

type RegisteredResourceHandler = (request: { params: { uri: string } }) => Promise<{ contents: Array<{ text: string }> }>;

interface HealthPayload {
  readonly readiness: {
    readonly ready: boolean;
    readonly components: Record<string, boolean>;
    readonly notReady: readonly string[];
  };
  readonly diagnostics: {
    readonly totals: { readonly requests: number; readonly failures: number };
    readonly byActionClass: ReadonlyArray<{ readonly actionClass: string; readonly count: number }>;
    readonly byFailureClass: ReadonlyArray<{ readonly failureClass: string; readonly count: number }>;
  };
}

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

function registerHandler(healthMonitor: HealthMonitor, connected: boolean): RegisteredResourceHandler {
  let registered: RegisteredResourceHandler | undefined;
  const server = {
    setRequestHandler: (_schema: unknown, handler: unknown) => {
      registered = handler as RegisteredResourceHandler;
    },
  } as ResourceServer;

  new ResourceHandler(
    server,
    {
      isConnected: connected,
      getEngineVersion: async () => ({}),
      getFeatureFlags: async () => ({}),
    },
    { getStatus: () => automationStatus(connected) },
    {} as AssetResources,
    {} as ActorResources,
    {} as LevelResources,
    healthMonitor,
    async () => true,
  ).registerHandlers();

  if (!registered) throw new Error('Resource handler was not registered');
  return registered;
}

async function readHealth(healthMonitor: HealthMonitor, connected: boolean): Promise<HealthPayload> {
  const handler = registerHandler(healthMonitor, connected);
  const response = await handler({ params: { uri: 'ue://health' } });
  return JSON.parse(response.contents[0]?.text ?? '{}') as HealthPayload;
}

describe('Task 47 ue://health readiness and anonymous diagnostics', () => {
  it('publishes readiness components on the existing health resource', async () => {
    const monitor = new HealthMonitor(new Logger('HealthResourceTest', 'error'));
    const payload = await readHealth(monitor, false);

    expect(payload.readiness).toBeDefined();
    expect(Object.keys(payload.readiness.components).sort()).toEqual(['editor', 'registry', 'transport']);
    // Disconnected transport must not read as ready.
    expect(payload.readiness.ready).toBe(false);
    expect(payload.readiness.notReady).toContain('transport_disconnected');
  });

  it('publishes bounded anonymous failure/latency aggregates', async () => {
    const monitor = new HealthMonitor(new Logger('HealthResourceTest', 'error'));
    monitor.trackPerformance(Date.now(), true, { actionClass: 'read' });
    monitor.trackPerformance(Date.now(), false, { actionClass: 'destructive', failureClass: 'timeout' });

    const payload = await readHealth(monitor, true);

    expect(payload.diagnostics.totals.requests).toBe(2);
    expect(payload.diagnostics.totals.failures).toBe(1);
    for (const entry of payload.diagnostics.byActionClass) {
      expect(TELEMETRY_ACTION_CLASSES).toContain(entry.actionClass);
    }
    for (const entry of payload.diagnostics.byFailureClass) {
      expect(TELEMETRY_FAILURE_CLASSES).toContain(entry.failureClass);
    }
  });

  it('never leaks a capability id, path or token supplied as a telemetry dimension', async () => {
    const monitor = new HealthMonitor(new Logger('HealthResourceTest', 'error'));
    monitor.trackPerformance(Date.now(), false, {
      actionClass: 'manage_asset.import_asset',
      failureClass: '/Game/Secret/Levels/ClientPitch',
    });

    const payload = await readHealth(monitor, true);
    const serialized = JSON.stringify(payload);

    expect(serialized).not.toContain('manage_asset');
    expect(serialized).not.toContain('/Game/Secret');
    expect(serialized).not.toContain('ClientPitch');
  });
});
