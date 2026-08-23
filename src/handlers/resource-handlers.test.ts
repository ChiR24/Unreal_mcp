import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { afterAll, beforeEach, describe, expect, it, vi } from 'vitest';
import type { AutomationBridgeStatus } from '../automation/index.js';
import type { AssetResources } from '../resources/assets.js';
import type { ActorResources } from '../resources/actors.js';
import type { LevelResources } from '../resources/levels.js';
import { AutomationLogger } from '../automation/log-redaction.js';
import { HealthMonitor } from '../services/health-monitor.js';
import { Logger } from '../utils/logging/logger.js';
import { ResourceHandler, type ResourceServer } from './resource-handlers.js';
import type { ExtendedResourceReader } from '../resources/resource-read-router.js';

type RegisteredResourceHandler = (request: { params: { uri: string } }) => Promise<{ contents: Array<{ text: string }> }>;
type BridgeStub = {
  isConnected: boolean;
  getEngineVersion: () => Promise<unknown>;
  getFeatureFlags: () => Promise<unknown>;
};

function createRegisteredHandler(
  status: AutomationBridgeStatus,
  healthMonitor: HealthMonitor,
  bridgeStub: BridgeStub = {
    isConnected: false,
    getEngineVersion: async () => ({}),
    getFeatureFlags: async () => ({})
  },
  extendedReader?: ExtendedResourceReader
): RegisteredResourceHandler {
  let registeredHandler: RegisteredResourceHandler | undefined;
  const server = {
    setRequestHandler: (_schema: unknown, handler: unknown) => {
      registeredHandler = handler as RegisteredResourceHandler;
    }
  } as ResourceServer;

  const automationBridge = { getStatus: () => status };

  new ResourceHandler(
    server,
    bridgeStub,
    automationBridge,
    {} as AssetResources,
    {} as ActorResources,
    {} as LevelResources,
    healthMonitor,
    async () => true,
    extendedReader
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

describe('ResourceHandler extended resource delegation', () => {
  it('delegates a non-legacy URI to the injected reader', async () => {
    // Given
    const healthMonitor = new HealthMonitor(new Logger('ResourceHandlerTest', 'error'));
    const reader: ExtendedResourceReader = {
      read: async (uri) => ({
        contents: [{ uri, mimeType: 'application/json', text: JSON.stringify({ uri, revision: 1, data: { ok: true } }) }]
      })
    };
    const handler = createRegisteredHandler(createAutomationStatus(), healthMonitor, {
      isConnected: false,
      getEngineVersion: async () => ({}),
      getFeatureFlags: async () => ({})
    }, reader);

    // When
    const result = await handler({ params: { uri: 'ue://project' } });

    // Then
    const parsed = JSON.parse(result.contents[0].text) as { data: { ok: boolean } };
    expect(parsed.data.ok).toBe(true);
  });

  it('throws Unknown resource for a non-legacy URI when no reader is injected', async () => {
    // Given
    const healthMonitor = new HealthMonitor(new Logger('ResourceHandlerTest', 'error'));
    const handler = createRegisteredHandler(createAutomationStatus(), healthMonitor);

    // When / Then
    await expect(handler({ params: { uri: 'ue://project' } })).rejects.toThrow('Unknown resource');
  });
});

// Presenter driver over a REAL temp Saved/MCP/diagnostics tree. The reader's
// own fail-closed/warn-once behavior is unit-proven in
// src/automation/diagnostics-snapshot-reader.test.ts; these cases prove the
// ResourceHandler WIRING attaches the allowlisted summaries to both bodies.
const PRESENTER_ROOT = mkdtempSync(join(tmpdir(), 'mcp-diagnostics-presenters-'));
const CURRENT_SNAPSHOT = {
  schemaVersion: 1,
  instance: { instanceId: 'inst-12345', pid: 12345, startTimeUtc: '2026-08-12T11:05:00.000Z' },
  counters: { requests: 3, failures: 1, refusals: 1, queueWaitMs: 42 },
  lastRequest: { requestId: 'req-current', correlationId: 'corr-1', canonicalAction: 'manage_asset.import_asset', origin: 'WebSocket', queueDepth: 2, enqueueAt: '2026-08-12T11:05:00.000Z', dispatchAt: '2026-08-12T11:05:00.120Z', terminalAt: '2026-08-12T11:05:00.500Z', terminalClass: 'success' },
  lastHandshake: null,
  lastDisconnect: null,
  session: null
};
const PREVIOUS_SNAPSHOT = { ...CURRENT_SNAPSHOT, instance: { ...CURRENT_SNAPSHOT.instance, instanceId: 'inst-9999', pid: 9999 }, lastRequest: { ...CURRENT_SNAPSHOT.lastRequest, requestId: 'req-previous' } };

describe('ResourceHandler diagnostics session presenters (real temp Saved tree)', () => {
  let warnSpy: ReturnType<typeof vi.spyOn>;

  beforeEach(() => { vi.stubEnv('UE_PROJECT_PATH', PRESENTER_ROOT); rmSync(join(PRESENTER_ROOT, 'Saved', 'MCP', 'diagnostics'), { recursive: true, force: true }); warnSpy = vi.spyOn(AutomationLogger.prototype, 'warn').mockImplementation(() => {}); });

  afterAll(() => { rmSync(PRESENTER_ROOT, { recursive: true, force: true }); vi.unstubAllEnvs(); vi.restoreAllMocks(); });

  function writeSnapshot(name: string, value: unknown): void { const dir = join(PRESENTER_ROOT, 'Saved', 'MCP', 'diagnostics'); mkdirSync(dir, { recursive: true }); writeFileSync(join(dir, name), JSON.stringify(value), { encoding: 'utf8' }); }

  async function bodies(): Promise<Array<Record<string, unknown>>> {
    const handler = createRegisteredHandler(createAutomationStatus(), new HealthMonitor(new Logger('ResourceHandlerTest', 'error')));
    return Promise.all(['ue://health', 'ue://automation-bridge'].map(async (uri) => JSON.parse((await handler({ params: { uri } })).contents[0].text) as Record<string, unknown>));
  }

  it('exposes allowlisted currentSession/previousSession on both bodies', async () => {
    writeSnapshot('current-session.json', CURRENT_SNAPSHOT);
    writeSnapshot('previous-session.json', PREVIOUS_SNAPSHOT);
    for (const body of await bodies()) {
      const current = body.currentSession as Record<string, unknown>;
      expect(current.schemaVersion).toBe(1);
      expect((current.instance as Record<string, unknown>).pid).toBe(12345);
      expect(((body.previousSession as Record<string, unknown>).instance as Record<string, unknown>).pid).toBe(9999);
    }
  });

  it('projects previousSession null when no previous file exists (NF-6 parity)', async () => {
    writeSnapshot('current-session.json', CURRENT_SNAPSHOT);
    for (const body of await bodies()) {
      expect((body.currentSession as Record<string, unknown>).schemaVersion).toBe(1);
      expect(body.previousSession).toBeNull();
    }
  });

  it('fails closed on corrupt and oversized snapshots with a bounded warning naming the path only', async () => {
    writeSnapshot('current-session.json', '{ not json');
    writeSnapshot('previous-session.json', '[]');
    for (const body of await bodies()) {
      expect(body.currentSession).toBeNull();
      expect(body.previousSession).toBeNull();
    }
    writeSnapshot('current-session.json', { ...CURRENT_SNAPSHOT, counters: { ...CURRENT_SNAPSHOT.counters, requests: 'x'.repeat(70_000) } });
    const [health, automation] = await bodies();
    expect(health.currentSession).toBeNull();
    expect(automation.currentSession).toBeNull();
    expect(warnSpy).toHaveBeenCalled();
    for (const call of warnSpy.mock.calls) {
      expect(call.map(String).join(' ')).not.toContain('not json');
    }
  });

  it('never surfaces hostile payload/secret/idempotency/raw-session fields', async () => {
    writeSnapshot('current-session.json', { ...CURRENT_SNAPSHOT, payload: { code: 'evil' }, capabilityToken: 'super-secret', idempotencyKey: 'raw-key', extraNested: { sessionId: 'raw-session' } });
    const [health, automation] = await bodies();
    const serialized = JSON.stringify({ health, automation });
    for (const key of ['payload', 'capabilityToken', 'idempotencyKey']) {
      for (const body of [health, automation]) {
        expect(body.currentSession).not.toHaveProperty(key);
      }
    }
    for (const forbidden of ['evil', 'super-secret', 'raw-key', 'raw-session']) {
      expect(serialized).not.toContain(forbidden);
    }
  });
});
