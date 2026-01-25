import { Server } from '@modelcontextprotocol/sdk/server/index.js';
import { ListResourcesRequestSchema } from '@modelcontextprotocol/sdk/types.js';
import { UnrealBridge } from '../unreal-bridge.js';
import { AutomationBridge } from '../automation/index.js';
import { HealthMonitor } from '../services/health-monitor.js';
import { ResourceHandler } from '../handlers/resource-handlers.js';
import { AssetResources } from '../resources/assets.js';
import { ActorResources } from '../resources/actors.js';
import { LevelResources } from '../resources/levels.js';

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
        this.server.setRequestHandler(ListResourcesRequestSchema, async () => {
            return {
                resources: [
                    { uri: 'ue://assets', name: 'Assets', description: 'Project assets', mimeType: 'application/json' },
                    { uri: 'ue://actors', name: 'Actors', description: 'Actors in the current level', mimeType: 'application/json' },
                    { uri: 'ue://level', name: 'Current Level', description: 'Current level name and path', mimeType: 'application/json' },
                    { uri: 'ue://health', name: 'Health Status', description: 'Server health and performance metrics', mimeType: 'application/json' },
                    { uri: 'ue://automation-bridge', name: 'Automation Bridge', description: 'Automation bridge diagnostics and recent activity', mimeType: 'application/json' },
                    { uri: 'ue://version', name: 'Engine Version', description: 'Unreal Engine version and compatibility info', mimeType: 'application/json' }
                ]
            };
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
