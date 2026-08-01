import { ListResourcesRequestSchema, ListResourceTemplatesRequestSchema } from '@modelcontextprotocol/sdk/types.js';
import { UnrealBridge } from '../unreal-bridge.js';
import { AutomationBridge } from '../automation/index.js';
import { HealthMonitor } from '../services/health-monitor.js';
import { ResourceHandler, type ResourceServer } from '../handlers/resource-handlers.js';
import { AssetResources } from '../resources/assets.js';
import { ActorResources } from '../resources/actors.js';
import { LevelResources } from '../resources/levels.js';
import { config } from '../config.js';
import { sharedRevisionProvider } from './mcp-primitives/resource-revision.js';
import {
    NEW_RESOURCE_DEFINITIONS,
    RESOURCE_TEMPLATES,
    type ResourceDefinition,
    type ResourceTemplateDefinition,
} from '../resources/resource-catalog.js';
import { redactProjectName } from '../resources/resource-errors.js';
import { CapabilityResources, GatewayManifestCapabilitySource } from '../resources/capability-resources.js';
import { BridgeEditorStateSource, EditorStateResources } from '../resources/editor-state-resources.js';
import { BridgeAssetLookupSource, KnowledgeResources } from '../resources/knowledge-resources.js';
import { ResourceReadRouter } from '../resources/resource-read-router.js';

const RESOURCE_DEFINITIONS = [
    { uri: 'ue://assets', name: 'Assets', description: 'Project assets', mimeType: 'application/json' },
    { uri: 'ue://actors', name: 'Actors', description: 'Actors in the current level', mimeType: 'application/json' },
    { uri: 'ue://level', name: 'Current Level', description: 'Current level name and path', mimeType: 'application/json' },
    { uri: 'ue://health', name: 'Health Status', description: 'Server health and performance metrics', mimeType: 'application/json' },
    { uri: 'ue://automation-bridge', name: 'Automation Bridge', description: 'Automation bridge diagnostics and recent activity', mimeType: 'application/json' },
    { uri: 'ue://version', name: 'Engine Version', description: 'Unreal Engine version and compatibility info', mimeType: 'application/json' }
];

type ListResourcesServer = {
    setRequestHandler(
        schema: typeof ListResourcesRequestSchema,
        handler: () => Promise<{ resources: ResourceDefinition[] }>
    ): void;
};

type ListResourceTemplatesServer = {
    setRequestHandler(
        schema: typeof ListResourceTemplatesRequestSchema,
        handler: () => Promise<{ resourceTemplates: ResourceTemplateDefinition[] }>
    ): void;
};

type ResourceRegistryServer = ResourceServer & ListResourcesServer & ListResourceTemplatesServer;

export class ResourceRegistry {
    constructor(
        private server: ResourceRegistryServer,
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
                resources: [...RESOURCE_DEFINITIONS, ...NEW_RESOURCE_DEFINITIONS]
            };
        });

        this.server.setRequestHandler(ListResourceTemplatesRequestSchema, async () => {
            return {
                resourceTemplates: [...RESOURCE_TEMPLATES]
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
            this.ensureConnected,
            this.buildExtendedReader()
        );
        resourceHandler.registerHandlers();
    }

    // Built here (not server-setup) to keep the constructor signature stable; revisions injected, Task 34 swaps the default.
    private buildExtendedReader(): ResourceReadRouter {
        // Shared with the notification driver (primitive-wiring.ts) so a
        // `resources/updated` and the read that follows it report the same
        // revision. A private instance here made the two permanently disagree.
        const revisions = sharedRevisionProvider();
        const capability = new CapabilityResources(new GatewayManifestCapabilitySource(), revisions);
        const editorSource = new BridgeEditorStateSource(
            this.automationBridge,
            this.ensureConnected,
            () => this.readEngineVersion()
        );
        const projectName = redactProjectName(config.UE_PROJECT_PATH) ?? null;
        const editorState = new EditorStateResources(editorSource, revisions, projectName);
        const assetLookup = new BridgeAssetLookupSource(this.automationBridge, this.ensureConnected);
        const knowledge = new KnowledgeResources(assetLookup, revisions);
        return new ResourceReadRouter(capability, editorState, knowledge);
    }

    private async readEngineVersion(): Promise<string | null> {
        try {
            const info = await this.bridge.getEngineVersion();
            return info.version ?? null;
        } catch {
            return null;
        }
    }
}
