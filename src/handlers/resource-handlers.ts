import { ListResourcesRequestSchema, ListResourceTemplatesRequestSchema, ReadResourceRequestSchema } from '@modelcontextprotocol/sdk/types.js';
import { readDiagnosticsSnapshots } from '../automation/diagnostics-snapshot-reader.js';
import { AutomationLogger } from '../automation/log-redaction.js';
import { AssetResources } from '../resources/assets.js';
import { listActors } from '../resources/actors.js';
import { HealthMonitor } from '../services/health-monitor.js';
import { createDefaultReadinessProbes, evaluateReadiness } from '../services/readiness.js';
import type { AutomationRequestBridge, AutomationStatusBridge } from '../types/tools/tool-interfaces.js';
import { ResourceReadRouter } from '../resources/resource-read-router.js';
import { config } from '../config.js';
import { sharedRevisionProvider } from '../server/mcp-primitives/resource-revision.js';
import {
  NEW_RESOURCE_DEFINITIONS,
  RESOURCE_TEMPLATES,
  type ResourceDefinition,
  type ResourceTemplateDefinition,
} from '../resources/resource-catalog.js';
import { redactProjectName } from '../resources/resource-errors.js';
import { CapabilityResources, GatewayManifestCapabilitySource } from '../resources/capability-resources.js';
import { BridgeEditorStateSource, EditorStateResources } from '../resources/editor-state-resources.js';
import { KnowledgeResources } from '../resources/knowledge-resources.js';

interface ResourceBridge {
  readonly isConnected: boolean;
  getEngineVersion(): Promise<unknown>;
  getFeatureFlags(): unknown;
}

// Module-level (no constructor churn): redacts secrets before the reader's
// bounded fail-closed warnings reach the log.
const diagnosticsReaderLogger = new AutomationLogger('DiagnosticsSnapshot');

const RESOURCE_DEFINITIONS = [
  { uri: 'ue://assets', name: 'Assets', description: 'Project assets', mimeType: 'application/json' },
  { uri: 'ue://actors', name: 'Actors', description: 'Actors in the current level', mimeType: 'application/json' },
  { uri: 'ue://health', name: 'Health Status', description: 'Server health and performance metrics', mimeType: 'application/json' },
  { uri: 'ue://automation-bridge', name: 'Automation Bridge', description: 'Automation bridge diagnostics and recent activity', mimeType: 'application/json' }
];

export type ResourceServer = {
  setRequestHandler(
    schema: typeof ReadResourceRequestSchema,
    handler: (request: { params: { uri: string } }) => Promise<{ contents: Array<{ uri: string; mimeType: string; text: string }> }>
  ): void;
  setRequestHandler(
    schema: typeof ListResourcesRequestSchema,
    handler: () => Promise<{ resources: ResourceDefinition[] }>
  ): void;
  setRequestHandler(
    schema: typeof ListResourceTemplatesRequestSchema,
    handler: () => Promise<{ resourceTemplates: ResourceTemplateDefinition[] }>
  ): void;
};

// The extended ue:// reader. Revisions are shared with the notification driver
// (primitive-wiring.ts) so a `resources/updated` and the read that follows it
// report the same revision.
function buildResourceReadRouter(
  bridge: ResourceBridge,
  automationBridge: AutomationRequestBridge,
  ensureConnected: () => Promise<boolean>
): ResourceReadRouter {
  const revisions = sharedRevisionProvider();
  const readEngineVersion = async (): Promise<string | null> => {
    try {
      const info = objectDetails(await bridge.getEngineVersion());
      return typeof info.version === 'string' ? info.version : null;
    } catch {
      return null;
    }
  };
  const editorState = new EditorStateResources(
    new BridgeEditorStateSource(automationBridge, ensureConnected, readEngineVersion),
    revisions,
    redactProjectName(config.UE_PROJECT_PATH) ?? null
  );
  return new ResourceReadRouter(
    new CapabilityResources(new GatewayManifestCapabilitySource(), revisions),
    editorState,
    new KnowledgeResources(automationBridge, ensureConnected, revisions)
  );
}

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
    private bridge: ResourceBridge,
    private automationBridge: AutomationStatusBridge & AutomationRequestBridge,
    private assetResources: AssetResources,
    private healthMonitor: HealthMonitor,
    private ensureConnected: () => Promise<boolean>,
    private router: ResourceReadRouter = buildResourceReadRouter(bridge, automationBridge, ensureConnected)
  ) { }

  registerHandlers() {
    this.server.setRequestHandler(ListResourcesRequestSchema, async () => ({
      resources: [...RESOURCE_DEFINITIONS, ...NEW_RESOURCE_DEFINITIONS]
    }));
    this.server.setRequestHandler(ListResourceTemplatesRequestSchema, async () => ({
      resourceTemplates: [...RESOURCE_TEMPLATES]
    }));
    this.server.setRequestHandler(ReadResourceRequestSchema, async (request) => {
      const uri = request.params.uri;
      switch (uri) {
        case 'ue://assets': return this.whenConnected(uri, () => this.assetResources.list('/Game'));
        case 'ue://actors': return this.whenConnected(uri, () => listActors(this.automationBridge));
        case 'ue://health': return jsonResource(uri, await this.health());
        case 'ue://automation-bridge': return jsonResource(uri, await this.bridgeStatus());
        default: return this.router.read(uri);
      }
    });
  }

  private async whenConnected(uri: string, fetch: () => Promise<unknown>): Promise<ResourceContent> {
    return (await this.ensureConnected()) ? jsonResource(uri, await fetch()) : disconnectedResource(uri);
  }

  private async health(): Promise<Record<string, unknown>> {
    const snapshots = await readDiagnosticsSnapshots(diagnosticsReaderLogger);
    const uptimeMs = Date.now() - this.healthMonitor.metrics.uptime;
    const automationStatus = this.automationBridge.getStatus();

    let versionInfo: Record<string, unknown> = {};
    let featureFlags: Record<string, unknown> = {};
    if (this.bridge.isConnected) {
      try { versionInfo = objectDetails(await this.bridge.getEngineVersion()); } catch { versionInfo = {}; }
      try { featureFlags = objectDetails(await this.bridge.getFeatureFlags()); } catch { featureFlags = {}; }
    }

    const automationSummary = {
      connected: automationStatus.connected,
      activePort: automationStatus.activePort,
      pendingRequests: automationStatus.pendingRequests,
      lastHandshakeAt: automationStatus.lastHandshakeAt,
      lastRequestSentAt: automationStatus.lastRequestSentAt,
      maxPendingRequests: automationStatus.maxPendingRequests
    };

    const readiness = evaluateReadiness(createDefaultReadinessProbes({
      automationBridge: this.automationBridge,
      healthMonitor: this.healthMonitor
    }));

    const health = {
      status: this.healthMonitor.metrics.connectionStatus,
      uptimeSeconds: Math.floor(uptimeMs / 1000),
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
      automationBridge: automationSummary,
      readiness,
      diagnostics: this.healthMonitor.telemetry.snapshot(),
      currentSession: snapshots.current,
      previousSession: snapshots.previous,
      // Same exposition text /metrics serves. The native transport has no
      // HTTP metrics endpoint, so ue://health is the only surface both
      // transports can be scraped through - keep them symmetric.
      metricsExposition: this.healthMonitor.telemetry.render(readiness)
    };

    return health;
  }

  private async bridgeStatus(): Promise<Record<string, unknown>> {
    const snapshots = await readDiagnosticsSnapshots(diagnosticsReaderLogger);
    const status = this.automationBridge.getStatus();
    return {
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
      currentSession: snapshots.current,
      previousSession: snapshots.previous
    };
  }
}
