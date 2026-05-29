import { ReadResourceRequestSchema } from '@modelcontextprotocol/sdk/types.js';
import { UnrealBridge } from '../unreal-bridge.js';
import { AutomationBridge } from '../automation/index.js';
import { AssetResources } from '../resources/assets.js';
import { ActorResources } from '../resources/actors.js';
import { LevelResources } from '../resources/levels.js';
import { HealthMonitor } from '../services/health-monitor.js';

export type ResourceServer = {
  setRequestHandler(
    schema: typeof ReadResourceRequestSchema,
    handler: (request: { params: { uri: string } }) => Promise<{ contents: Array<{ uri: string; mimeType: string; text: string }> }>
  ): void;
};

type ResourceContent = { contents: Array<{ uri: string; mimeType: string; text: string }> };

function resourceContent(uri: string, mimeType: string, text: string): ResourceContent {
  return { contents: [{ uri, mimeType, text }] };
}

function jsonResource(uri: string, value: unknown): ResourceContent {
  return resourceContent(uri, 'application/json', JSON.stringify(value, null, 2));
}

function disconnectedResource(uri: string): ResourceContent {
  return resourceContent(uri, 'text/plain', 'Unreal Engine not connected (after 3 attempts).');
}

function redactRecentErrors(errors: Array<{ time: string; scope: string; type: string; message: string; retriable: boolean }>) {
  return errors.map(error => ({
    time: error.time,
    scope: error.scope,
    type: error.type,
    retriable: error.retriable
  }));
}

function objectDetails(value: unknown): Record<string, unknown> {
  return typeof value === 'object' && value !== null && !Array.isArray(value)
    ? value as Record<string, unknown>
    : {};
}

export class ResourceHandler {
  constructor(
    private server: ResourceServer,
    private bridge: UnrealBridge,
    private automationBridge: AutomationBridge,
    private assetResources: AssetResources,
    private actorResources: ActorResources,
    private levelResources: LevelResources,
    private healthMonitor: HealthMonitor,
    private ensureConnected: () => Promise<boolean>
  ) { }

  registerHandlers() {
    this.server.setRequestHandler(ReadResourceRequestSchema, async (request) => {
      const uri = request.params.uri;

      if (uri === 'ue://assets') {
        const ok = await this.ensureConnected();
        if (!ok) {
          return disconnectedResource(uri);
        }
        const list = await this.assetResources.list('/Game', true);
        return jsonResource(uri, list);
      }

      if (uri === 'ue://actors') {
        const ok = await this.ensureConnected();
        if (!ok) {
          return disconnectedResource(uri);
        }
        const list = await this.actorResources.listActors();
        return jsonResource(uri, list);
      }

      if (uri === 'ue://level') {
        const ok = await this.ensureConnected();
        if (!ok) {
          return disconnectedResource(uri);
        }
        const level = await this.levelResources.getCurrentLevel();
        return jsonResource(uri, level);
      }

      if (uri === 'ue://health') {
        const uptimeMs = Date.now() - this.healthMonitor.metrics.uptime;
        const automationStatus = this.automationBridge.getStatus();

        let versionInfo: Record<string, unknown> = {};
        let featureFlags: Record<string, unknown> = {};
        if (this.bridge.isConnected) {
          try { versionInfo = objectDetails(await this.bridge.getEngineVersion()); } catch { versionInfo = {}; }
          try { featureFlags = objectDetails(await this.bridge.getFeatureFlags()); } catch { featureFlags = {}; }
        }

        const responseTimes = this.healthMonitor.metrics.responseTimes.slice(-25);
        const automationSummary = {
          connected: automationStatus.connected,
          activePort: automationStatus.activePort,
          pendingRequests: automationStatus.pendingRequests,
          listeningPorts: automationStatus.listeningPorts,
          lastHandshakeAt: automationStatus.lastHandshakeAt,
          lastRequestSentAt: automationStatus.lastRequestSentAt,
          maxPendingRequests: automationStatus.maxPendingRequests,
          maxConcurrentConnections: automationStatus.maxConcurrentConnections
        };

        const health = {
          status: this.healthMonitor.metrics.connectionStatus,
          uptimeSeconds: Math.floor(uptimeMs / 1000),
          performance: {
            totalRequests: this.healthMonitor.metrics.totalRequests,
            successfulRequests: this.healthMonitor.metrics.successfulRequests,
            failedRequests: this.healthMonitor.metrics.failedRequests,
            successRate: this.healthMonitor.metrics.totalRequests > 0 ? Number(((this.healthMonitor.metrics.successfulRequests / this.healthMonitor.metrics.totalRequests) * 100).toFixed(2)) : null,
            averageResponseTimeMs: Math.round(this.healthMonitor.metrics.averageResponseTime),
            recentResponseTimesMs: responseTimes
          },
          lastHealthCheckIso: this.healthMonitor.metrics.lastHealthCheck.toISOString(),
          unrealConnection: {
            status: this.bridge.isConnected ? 'connected' : 'disconnected',
            transport: 'automation_bridge',
            engineVersion: versionInfo,
            features: {
              pythonEnabled: false,
              subsystems: objectDetails(featureFlags.subsystems),
              automationBridgeConnected: automationStatus.connected
            }
          },
          recentErrors: redactRecentErrors(this.healthMonitor.metrics.recentErrors.slice(-10)),
          automationBridge: automationSummary
        };

        return jsonResource(uri, health);
      }

      if (uri === 'ue://automation-bridge') {
        const status = this.automationBridge.getStatus();
        const content = {
          summary: {
            enabled: status.enabled,
            connected: status.connected,
            host: status.host,
            port: status.port,
            capabilityTokenRequired: status.capabilityTokenRequired,
            pendingRequests: status.pendingRequests
          },
          timestamps: {
            connectedAt: status.connectedAt,
            lastHandshakeAt: status.lastHandshakeAt,
            lastMessageAt: status.lastMessageAt,
            lastRequestSentAt: status.lastRequestSentAt
          },
          lastDisconnect: status.lastDisconnect ? { code: status.lastDisconnect.code, at: status.lastDisconnect.at } : null,
          lastHandshakeFailure: status.lastHandshakeFailure ? { at: status.lastHandshakeFailure.at } : null,
          lastError: status.lastError ? { at: status.lastError.at } : null,
          listening: status.webSocketListening
        };

        return jsonResource(uri, content);
      }

      if (uri === 'ue://version') {
        const ok = await this.ensureConnected();
        if (!ok) {
          return disconnectedResource(uri);
        }
        const info = await this.bridge.getEngineVersion();
        return jsonResource(uri, info);
      }

      throw new Error(`Unknown resource: ${uri}`);
    });
  }
}
