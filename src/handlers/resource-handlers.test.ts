import { describe, expect, it } from 'vitest';
import type { UnrealBridge } from '../unreal-bridge.js';
import type { AutomationBridge, AutomationBridgeStatus } from '../automation/index.js';
import type { AssetResources } from '../resources/assets.js';
import type { ActorResources } from '../resources/actors.js';
import type { LevelResources } from '../resources/levels.js';
import { HealthMonitor } from '../services/health-monitor.js';
import { Logger } from '../utils/logger.js';
import { ResourceHandler, type ResourceServer } from './resource-handlers.js';

type RegisteredResourceHandler = (request: { params: { uri: string } }) => Promise<{ contents: Array<{ text: string }> }>;
type BridgeStub = {
  isConnected: boolean;
  getEngineVersion?: () => Promise<unknown>;
  getFeatureFlags?: () => Promise<unknown>;
};

function createRegisteredHandler(
  status: AutomationBridgeStatus,
  healthMonitor: HealthMonitor,
  bridgeStub: BridgeStub = { isConnected: false }
): RegisteredResourceHandler {
  let registeredHandler: RegisteredResourceHandler | undefined;
  const server = {
    setRequestHandler: (_schema: unknown, handler: unknown) => {
      registeredHandler = handler as RegisteredResourceHandler;
    }
  } as ResourceServer;

  const bridge = bridgeStub as unknown as UnrealBridge;
  const automationBridge = { getStatus: () => status } as unknown as AutomationBridge;

  new ResourceHandler(
    server,
    bridge,
    automationBridge,
    {} as AssetResources,
    {} as ActorResources,
    {} as LevelResources,
    healthMonitor,
    async () => true
  ).registerHandlers();

  if (!registeredHandler) {
    throw new Error('Resource handler was not registered');
  }

  return registeredHandler;
}

function createAutomationStatus(): AutomationBridgeStatus {
  return {
    enabled: true,
    host: '127.0.0.1',
    port: 8091,
    configuredPorts: [8091],
    listeningPorts: [],
    connected: true,
    connectedAt: '2026-01-01T00:00:00.000Z',
    activePort: 8091,
    negotiatedProtocol: 'mcp-automation',
    supportedProtocols: ['mcp-automation'],
    supportedOpcodes: ['automation_request'],
    expectedResponseOpcodes: ['automation_response'],
    capabilityTokenRequired: true,
    lastHandshakeAt: '2026-01-01T00:00:01.000Z',
    lastHandshakeMetadata: { capabilityToken: 'secret-token', sessionId: 'secret-session' },
    lastHandshakeAck: { type: 'bridge_ack' },
    lastHandshakeFailure: { reason: 'secret handshake failure', at: '2026-01-01T00:00:02.000Z' },
    lastDisconnect: { code: 1006, reason: 'secret disconnect reason', at: '2026-01-01T00:00:03.000Z' },
    lastError: { message: 'secret error message', at: '2026-01-01T00:00:04.000Z' },
    lastMessageAt: '2026-01-01T00:00:05.000Z',
    lastRequestSentAt: '2026-01-01T00:00:06.000Z',
    pendingRequests: 1,
    pendingRequestDetails: [{ requestId: 'secret-request-id', action: 'system_control', ageMs: 10 }],
    connections: [{
      connectionId: 'secret-connection-id',
      sessionId: 'secret-session-id',
      remoteAddress: '10.0.0.2',
      remotePort: 49152,
      port: 8091,
      connectedAt: '2026-01-01T00:00:00.000Z',
      protocol: 'mcp-automation',
      readyState: 1,
      isPrimary: true
    }],
    webSocketListening: false,
    serverLegacyEnabled: true,
    serverName: 'unreal-engine-mcp',
    serverVersion: '0.0.0',
    maxConcurrentConnections: 1,
    maxPendingRequests: 25,
    heartbeatIntervalMs: 10000
  };
}

describe('ResourceHandler diagnostics redaction', () => {
  it('does not expose raw bridge internals through health or automation resources', async () => {
    const healthMonitor = new HealthMonitor(new Logger('ResourceHandlerTest', 'error'));
    healthMonitor.metrics.recentErrors.push({
      time: '2026-01-01T00:00:00.000Z',
      scope: 'test',
      type: 'TEST_ERROR',
      message: 'secret recent error detail',
      retriable: false
    });

    const handler = createRegisteredHandler(createAutomationStatus(), healthMonitor);
    const health = JSON.parse((await handler({ params: { uri: 'ue://health' } })).contents[0].text) as Record<string, unknown>;
    const automation = JSON.parse((await handler({ params: { uri: 'ue://automation-bridge' } })).contents[0].text) as Record<string, unknown>;
    const serialized = JSON.stringify({ health, automation });

    expect(health).not.toHaveProperty('raw');
    expect(automation).not.toHaveProperty('connections');
    expect(automation).not.toHaveProperty('pendingRequestDetails');
    expect(automation).not.toHaveProperty('lastHandshakeMetadata');

    for (const forbidden of [
      'secret recent error detail',
      'secret-token',
      'secret-session',
      'secret handshake failure',
      'secret disconnect reason',
      'secret error message',
      'secret-request-id',
      'secret-connection-id',
      '10.0.0.2'
    ]) {
      expect(serialized).not.toContain(forbidden);
    }
  });

  it('keeps health engine and subsystem details object-shaped', async () => {
    const healthMonitor = new HealthMonitor(new Logger('ResourceHandlerTest', 'error'));
    const handler = createRegisteredHandler(createAutomationStatus(), healthMonitor, {
      isConnected: true,
      getEngineVersion: async () => ['unexpected-version-array'],
      getFeatureFlags: async () => ({ subsystems: ['unexpected-subsystem-array'] })
    });

    const health = JSON.parse((await handler({ params: { uri: 'ue://health' } })).contents[0].text) as {
      unrealConnection: {
        engineVersion: unknown;
        features: { subsystems: unknown };
      };
    };

    expect(health.unrealConnection.engineVersion).toEqual({});
    expect(health.unrealConnection.features.subsystems).toEqual({});
  });
});
