import { ReadResourceRequestSchema } from '@modelcontextprotocol/sdk/types.js';
import { Server } from '@modelcontextprotocol/sdk/server/index.js';
import { UnrealBridge } from '../unreal-bridge.js';
import { AutomationBridge } from '../automation/index.js';
import { AssetResources } from '../resources/assets.js';
import { ActorResources } from '../resources/actors.js';
import { LevelResources } from '../resources/levels.js';
import { HealthMonitor } from '../services/health-monitor.js';

export class ResourceHandler {
  constructor(
    private server: Server,
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
          return { contents: [{ uri, mimeType: 'text/plain', text: 'Unreal Engine not connected (after 3 attempts).' }] };
        }
        const list = await this.assetResources.list('/Game', true);
        return {
          contents: [{
            uri,
            mimeType: 'application/json',
            text: JSON.stringify(list, null, 2)
          }]
        };
      }

      if (uri === 'ue://actors') {
        const ok = await this.ensureConnected();
        if (!ok) {
          return { contents: [{ uri, mimeType: 'text/plain', text: 'Unreal Engine not connected (after 3 attempts).' }] };
        }
        const list = await this.actorResources.listActors();
        return {
          contents: [{
            uri,
            mimeType: 'application/json',
            text: JSON.stringify(list, null, 2)
          }]
        };
      }

      if (uri === 'ue://level') {
        const ok = await this.ensureConnected();
        if (!ok) {
          return { contents: [{ uri, mimeType: 'text/plain', text: 'Unreal Engine not connected (after 3 attempts).' }] };
        }
        const level = await this.levelResources.getCurrentLevel();
        return {
          contents: [{
            uri,
            mimeType: 'application/json',
            text: JSON.stringify(level, null, 2)
          }]
        };
      }

      if (uri === 'ue://health') {
        const uptimeMs = Date.now() - this.healthMonitor.metrics.uptime;
        const automationStatus = this.automationBridge.getStatus();

        let versionInfo: Record<string, unknown> = {};
        let featureFlags: Record<string, unknown> = {};
        if (this.bridge.isConnected) {
          try { versionInfo = await this.bridge.getEngineVersion(); } catch { /* Version info unavailable - non-critical for health check */ }
          try { featureFlags = await this.bridge.getFeatureFlags(); } catch { /* Feature flags unavailable - non-critical for health check */ }
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
              subsystems: featureFlags.subsystems || {},
              automationBridgeConnected: automationStatus.connected
            }
          },
          recentErrors: this.healthMonitor.metrics.recentErrors.slice(-10),
          automationBridge: automationSummary,
          raw: {
            metrics: this.healthMonitor.metrics,
            automationStatus
          }
        };

        return {
          contents: [{
            uri,
            mimeType: 'application/json',
            text: JSON.stringify(health, null, 2)
          }]
        };
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
          connections: status.connections,
          timestamps: {
            connectedAt: status.connectedAt,
            lastHandshakeAt: status.lastHandshakeAt,
            lastMessageAt: status.lastMessageAt,
            lastRequestSentAt: status.lastRequestSentAt
          },
          lastDisconnect: status.lastDisconnect,
          lastHandshakeFailure: status.lastHandshakeFailure,
          lastError: status.lastError,
          lastHandshakeMetadata: status.lastHandshakeMetadata,
          pendingRequestDetails: status.pendingRequestDetails,
          listening: status.webSocketListening
        };

        return {
          contents: [{
            uri,
            mimeType: 'application/json',
            text: JSON.stringify(content, null, 2)
          }]
        };
      }

      if (uri === 'ue://version') {
        const ok = await this.ensureConnected();
        if (!ok) {
          return { contents: [{ uri, mimeType: 'text/plain', text: 'Unreal Engine not connected (after 3 attempts).' }] };
        }
        const info = await this.bridge.getEngineVersion();
        return {
          contents: [{
            uri,
            mimeType: 'application/json',
            text: JSON.stringify(info, null, 2)
          }]
        };
      }

      if (uri === 'unreal://logs') {
        const ok = await this.ensureConnected();
        if (!ok) {
          return { contents: [{ uri, mimeType: 'text/plain', text: 'Unreal Engine not connected (after 3 attempts).' }] };
        }
        const response = await this.automationBridge.sendAutomationRequest('get_output_log', {});
        const result = (response.result as { logs?: string[] }) || {};
        const logs = result.logs || [];
        const text = logs.join('\n');
        return {
          contents: [{
            uri,
            mimeType: 'text/plain',
            text
          }]
        };
      }

      if (uri === 'unreal://editor/status') {
        const ok = await this.ensureConnected();
        if (!ok) {
          return { contents: [{ uri, mimeType: 'text/plain', text: 'Unreal Engine not connected (after 3 attempts).' }] };
        }
        const response = await this.automationBridge.sendAutomationRequest('get_editor_status', {});
        const result = response.result || {};
        return {
          contents: [{
            uri,
            mimeType: 'application/json',
            text: JSON.stringify(result, null, 2)
          }]
        };
      }

      throw new Error(`Unknown resource: ${uri}`);
    });
  }
}
