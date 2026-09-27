import { UnrealBridge } from './unreal-bridge.js';
import { AutomationBridge } from './automation/index.js';
import { Logger } from './utils/logging/logger.js';
import { HealthMonitor } from './services/health-monitor.js';
import { AssetResources } from './resources/assets.js';
import { ResourceHandler } from './handlers/resource-handlers.js';
import { ToolRegistry } from './server/tool-registry.js';
import fs from 'node:fs';

type McpServer = ConstructorParameters<typeof ToolRegistry>[0];

export class ServerSetup {
  private server: McpServer;
  private bridge: UnrealBridge;
  private automationBridge: AutomationBridge;
  private logger: Logger;
  private healthMonitor: HealthMonitor;
  private assetResources: AssetResources;

  constructor(
    server: McpServer,
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

    this.assetResources = new AssetResources(bridge);
  }

  async setup(): Promise<void> {
    this.validateEnvironment();

    const ensureConnected = this.ensureConnectedOnDemand.bind(this);

    new ResourceHandler(
      this.server,
      this.bridge,
      this.automationBridge,
      this.assetResources,
      this.healthMonitor,
      ensureConnected
    ).registerHandlers();

    const toolRegistry = new ToolRegistry(
      this.server,
      this.automationBridge,
      this.logger,
      this.healthMonitor,
      ensureConnected
    );
    toolRegistry.register();
  }

  private validateEnvironment(): void {
    const enginePath = process.env.UE_ENGINE_PATH || process.env.UNREAL_ENGINE_PATH;

    this.validateConfiguredPath(
      'UE_PROJECT_PATH',
      process.env.UE_PROJECT_PATH,
      'UE_PROJECT_PATH is not set; the bridge port is read from MCP_AUTOMATION_PORT or the default.'
    );
    this.validateConfiguredPath('UE_ENGINE_PATH', enginePath);
  }

  private validateConfiguredPath(envName: string, configuredPath: string | undefined, notSetMessage?: string): void {
    const pathToValidate = configuredPath?.trim();

    if (!pathToValidate) {
      if (notSetMessage) {
        this.logger.info(notSetMessage);
      }
      return;
    }

    if (!fs.existsSync(pathToValidate)) {
      this.logger.warn(`${envName} is set to '${pathToValidate}' but the path does not exist.`);
      return;
    }

    this.logger.info(`${envName} validated: ${pathToValidate}`);
  }

  private async ensureConnectedOnDemand(): Promise<boolean> {
    if (this.bridge.isConnected) return true;
    const ok = await this.bridge.tryConnect();
    if (ok) {
      this.healthMonitor.metrics.connectionStatus = 'connected';
      this.healthMonitor.startHealthChecks(this.bridge);
    } else {
      this.healthMonitor.metrics.connectionStatus = 'disconnected';
    }
    return ok;
  }


}
