import { Server } from '@modelcontextprotocol/sdk/server/index.js';
import { ListPromptsRequestSchema, GetPromptRequestSchema } from '@modelcontextprotocol/sdk/types.js';
import { UnrealBridge } from './unreal-bridge.js';
import { AutomationBridge } from './automation/index.js';
import { Logger } from './utils/logger.js';
import { HealthMonitor } from './services/health-monitor.js';
import { prompts } from './prompts/index.js';
import { AssetResources } from './resources/assets.js';
import { ActorResources } from './resources/actors.js';
import { LevelResources } from './resources/levels.js';
import { ResourceRegistry } from './server/resource-registry.js';
import { ToolRegistry } from './server/tool-registry.js';
import fs from 'fs';

export class ServerSetup {
  private server: Server;
  private bridge: UnrealBridge;
  private automationBridge: AutomationBridge;
  private logger: Logger;
  private healthMonitor: HealthMonitor;
  private assetResources: AssetResources;
  private actorResources: ActorResources;
  private levelResources: LevelResources;

  constructor(
    server: Server,
    bridge: UnrealBridge,
    automationBridge: AutomationBridge,
    logger: Logger,
    healthMonitor: HealthMonitor
  ) {
    this.server = server;
    this.bridge = bridge;
    this.automationBridge = automationBridge;
    this.logger = logger;
    this.healthMonitor = healthMonitor;

    // Initialize resources
    this.assetResources = new AssetResources(bridge);
    this.actorResources = new ActorResources(bridge);
    this.levelResources = new LevelResources(bridge);
  }

  async setup() {
    this.validateEnvironment();

    const ensureConnected = this.ensureConnectedOnDemand.bind(this);

    // Register Resources
    const resourceRegistry = new ResourceRegistry(
      this.server,
      this.bridge,
      this.automationBridge,
      this.assetResources,
      this.actorResources,
      this.levelResources,
      this.healthMonitor,
      ensureConnected
    );
    resourceRegistry.register();

    // Register Tools
    const toolRegistry = new ToolRegistry(
      this.server,
      this.bridge,
      this.automationBridge,
      this.logger,
      this.healthMonitor,
      this.assetResources,
      this.actorResources,
      this.levelResources,
      ensureConnected
    );
    toolRegistry.register();

    this.registerPrompts();
  }

  private validateEnvironment() {
    const projectPath = process.env.UE_PROJECT_PATH;
    if (projectPath) {
      if (!fs.existsSync(projectPath)) {
        this.logger.warn(`UE_PROJECT_PATH is set to '${projectPath}' but the path does not exist.`);
      } else {
        this.logger.info(`UE_PROJECT_PATH validated: ${projectPath}`);
      }
    } else {
      this.logger.info('UE_PROJECT_PATH is not set. Offline project settings fallback will be disabled.');
    }

    const enginePath = process.env.UE_ENGINE_PATH || process.env.UNREAL_ENGINE_PATH;
    if (enginePath) {
      if (!fs.existsSync(enginePath)) {
        this.logger.warn(`UE_ENGINE_PATH is set to '${enginePath}' but the path does not exist.`);
      } else {
        this.logger.info(`UE_ENGINE_PATH validated: ${enginePath}`);
      }
    }
  }

  private async ensureConnectedOnDemand(): Promise<boolean> {
    if (this.bridge.isConnected) return true;
    const ok = await this.bridge.tryConnect(3, 5000, 1000);
    if (ok) {
      this.healthMonitor.metrics.connectionStatus = 'connected';
      this.healthMonitor.startHealthChecks(this.bridge);
    } else {
      this.healthMonitor.metrics.connectionStatus = 'disconnected';
    }
    return ok;
  }

  private registerPrompts() {
    this.server.setRequestHandler(ListPromptsRequestSchema, async () => {
      return {
        prompts: prompts.map(p => ({
          name: p.name,
          description: p.description,
          arguments: Object.entries(p.arguments || {}).map(([name, schema]) => {
            const meta: Record<string, unknown> = {};
            if (schema.type) meta.type = schema.type;
            if (schema.enum) meta.enum = schema.enum;
            if (schema.default !== undefined) meta.default = schema.default;
            return {
              name,
              description: schema.description,
              required: schema.required ?? false,
              ...(Object.keys(meta).length ? { _meta: meta } : {})
            };
          })
        }))
      };
    });

    this.server.setRequestHandler(GetPromptRequestSchema, async (request) => {
      const prompt = prompts.find(p => p.name === request.params.name);
      if (!prompt) {
        throw new Error(`Unknown prompt: ${request.params.name}`);
      }
      const args = (request.params.arguments || {}) as Record<string, unknown>;
      const messages = prompt.build(args);
      return {
        description: prompt.description,
        messages
      };
    });
  }
}