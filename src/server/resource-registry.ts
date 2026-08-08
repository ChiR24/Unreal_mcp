import { ListResourcesRequestSchema, ListResourceTemplatesRequestSchema } from '@modelcontextprotocol/sdk/types.js';
import { UnrealBridge } from '../unreal-bridge.js';
import { AutomationBridge } from '../automation/index.js';
import { HealthMonitor } from '../services/health-monitor.js';
import { ResourceHandler, type ResourceServer } from '../handlers/resource-handlers.js';
import { AssetResources } from '../resources/assets.js';
import { ActorResources } from '../resources/actors.js';
import { LevelResources } from '../resources/levels.js';
import { DebugService } from '../debug/index.js';

const RESOURCE_DEFINITIONS = [
    { uri: 'ue://assets', name: 'Assets', description: 'Project assets', mimeType: 'application/json' },
    { uri: 'ue://actors', name: 'Actors', description: 'Actors in the current level', mimeType: 'application/json' },
    { uri: 'ue://level', name: 'Current Level', description: 'Current level name and path', mimeType: 'application/json' },
    { uri: 'ue://health', name: 'Health Status', description: 'Server health and performance metrics', mimeType: 'application/json' },
    { uri: 'ue://automation-bridge', name: 'Automation Bridge', description: 'Automation bridge diagnostics and recent activity', mimeType: 'application/json' },
    { uri: 'ue://version', name: 'Engine Version', description: 'Unreal Engine version and compatibility info', mimeType: 'application/json' },
    { uri: 'ue://debug/sessions', name: 'Debug Sessions', description: 'Sidecar debug sessions', mimeType: 'application/json' },
    { uri: 'ue://debug/health', name: 'Debug Health', description: 'Debug host, event, job and artifact health', mimeType: 'application/json' }
];

const RESOURCE_TEMPLATES = [
    { uriTemplate: 'ue://debug/session/{sessionId}', name: 'Debug Session', description: 'A debug session record', mimeType: 'application/json' },
    { uriTemplate: 'ue://debug/events/{sessionId}?after={cursor}&limit={limit}', name: 'Debug Events', description: 'Cursor-based correlated debug events', mimeType: 'application/json' },
    { uriTemplate: 'ue://debug/jobs/{jobId}', name: 'Debug Job', description: 'Asynchronous test or trace job', mimeType: 'application/json' },
    { uriTemplate: 'ue://debug/artifacts/{artifactId}', name: 'Debug Artifact', description: 'Artifact metadata, path, size and SHA-256', mimeType: 'application/json' }
];

type ResourceRegistryServer = ResourceServer & {
    setRequestHandler(
        schema: typeof ListResourcesRequestSchema,
        handler: () => Promise<{ resources: typeof RESOURCE_DEFINITIONS }>
    ): void;
    setRequestHandler(
        schema: typeof ListResourceTemplatesRequestSchema,
        handler: () => Promise<{ resourceTemplates: typeof RESOURCE_TEMPLATES }>
    ): void;
};

export class ResourceRegistry {
    constructor(
        private server: ResourceRegistryServer,
        private bridge: UnrealBridge,
        private automationBridge: AutomationBridge,
        private assetResources: AssetResources,
        private actorResources: ActorResources,
        private levelResources: LevelResources,
        private healthMonitor: HealthMonitor,
        private debugService: DebugService,
        private ensureConnected: () => Promise<boolean>
    ) { }

    register() {
        this.server.setRequestHandler(ListResourcesRequestSchema, async () => {
            return {
                resources: RESOURCE_DEFINITIONS
            };
        });
        this.server.setRequestHandler(ListResourceTemplatesRequestSchema, async () => ({ resourceTemplates: RESOURCE_TEMPLATES }));

        const resourceHandler = new ResourceHandler(
            this.server,
            this.bridge,
            this.automationBridge,
            this.assetResources,
            this.actorResources,
            this.levelResources,
            this.healthMonitor,
            this.debugService,
            this.ensureConnected
        );
        resourceHandler.registerHandlers();
    }
}
