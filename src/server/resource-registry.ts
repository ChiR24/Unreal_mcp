import { Server } from '@modelcontextprotocol/sdk/server/index.js';
import { ListResourcesRequestSchema, ListResourceTemplatesRequestSchema } from '@modelcontextprotocol/sdk/types.js';
import { UnrealBridge } from '../unreal-bridge.js';
import { AutomationBridge } from '../automation/index.js';
import { HealthMonitor } from '../services/health-monitor.js';
import { ResourceHandler } from '../handlers/resource-handlers.js';
import { AssetResources } from '../resources/assets.js';
import { ActorResources } from '../resources/actors.js';
import { LevelResources } from '../resources/levels.js';
import { resourceTemplates } from '../resources/templates.js';

export class ResourceRegistry {
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

    register() {
        // List static resources
        this.server.setRequestHandler(ListResourcesRequestSchema, async () => {
            return {
                resources: [
                    { uri: 'ue://assets', name: 'Assets', description: 'Project assets', mimeType: 'application/json' },
                    { uri: 'ue://health', name: 'Health Status', description: 'Server health and performance metrics', mimeType: 'application/json' },
                    { uri: 'ue://automation-bridge', name: 'Automation Bridge', description: 'Automation bridge diagnostics and recent activity', mimeType: 'application/json' },
                    { uri: 'unreal://logs', name: 'Editor Logs', description: 'Recent output logs (last 1000 lines)', mimeType: 'text/plain' },
                    { uri: 'unreal://editor/status', name: 'Editor Status', description: 'Current editor state (PIE, Map, Version)', mimeType: 'application/json' },
                    { uri: 'ue://version', name: 'Engine Version', description: 'Unreal Engine version and compatibility info', mimeType: 'application/json' }
                ]
            };
        });

        // List resource templates (Phase E2)
        this.server.setRequestHandler(ListResourceTemplatesRequestSchema, async () => {
            return { resourceTemplates };
        });

        const resourceHandler = new ResourceHandler(
            this.server,
            this.bridge,
            this.automationBridge,
            this.assetResources,
            this.actorResources,
            this.levelResources,
            this.healthMonitor,
            this.ensureConnected
        );
        resourceHandler.registerHandlers();
    }
}
