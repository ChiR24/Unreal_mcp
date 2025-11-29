import { Server } from '@modelcontextprotocol/sdk/server/index.js';
import { CallToolRequestSchema, ListPromptsRequestSchema, GetPromptRequestSchema, ListToolsRequestSchema, ListResourcesRequestSchema } from '@modelcontextprotocol/sdk/types.js';
import { UnrealBridge } from './unreal-bridge.js';
import { AutomationBridge } from './automation-bridge.js';
import { Logger } from './utils/logger.js';
import { HealthMonitor } from './services/health-monitor.js';
import { ResourceHandler } from './handlers/resource-handlers.js';
import { consolidatedToolDefinitions } from './tools/consolidated-tool-definitions.js';
import { handleConsolidatedToolCall } from './tools/consolidated-tool-handlers.js';
import { responseValidator } from './utils/response-validator.js';
import { prompts } from './prompts/index.js';
import { ErrorHandler } from './utils/error-handler.js';
import { cleanObject } from './utils/safe-json.js';
import { createElicitationHelper } from './utils/elicitation.js';
import { AssetResources } from './resources/assets.js';
import { ActorResources } from './resources/actors.js';
import { LevelResources } from './resources/levels.js';
import { ActorTools } from './tools/actors.js';
import { AssetTools } from './tools/assets.js';
import { EditorTools } from './tools/editor.js';
import { MaterialTools } from './tools/materials.js';
import { AnimationTools } from './tools/animation.js';
import { PhysicsTools } from './tools/physics.js';
import { NiagaraTools } from './tools/niagara.js';
import { BlueprintTools } from './tools/blueprint.js';
import { LevelTools } from './tools/level.js';
import { LightingTools } from './tools/lighting.js';
import { LandscapeTools } from './tools/landscape.js';
import { FoliageTools } from './tools/foliage.js';
import { EnvironmentTools } from './tools/environment.js';
import { DebugVisualizationTools } from './tools/debug.js';
import { PerformanceTools } from './tools/performance.js';
import { AudioTools } from './tools/audio.js';
import { UITools } from './tools/ui.js';
import { SequenceTools } from './tools/sequence.js';
import { IntrospectionTools } from './tools/introspection.js';
import { EngineTools } from './tools/engine.js';
import { getProjectSetting } from './utils/ini-reader.js';
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
  private defaultElicitationTimeoutMs = 60000;

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
    this.registerTools();
    this.registerResources();
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

  private registerResources() {
    this.server.setRequestHandler(ListResourcesRequestSchema, async () => {
      return {
        resources: [
          { uri: 'ue://assets', name: 'Assets', description: 'Project assets', mimeType: 'application/json' },
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
      this.ensureConnectedOnDemand.bind(this)
    );
    resourceHandler.registerHandlers();
  }

  private registerTools() {
    // Initialize tools
    const actorTools = new ActorTools(this.bridge);
    const assetTools = new AssetTools(this.bridge);
    const editorTools = new EditorTools(this.bridge);
    const materialTools = new MaterialTools(this.bridge);
    const animationTools = new AnimationTools(this.bridge);
    const physicsTools = new PhysicsTools(this.bridge);
    const niagaraTools = new NiagaraTools(this.bridge);
    const blueprintTools = new BlueprintTools(this.bridge);
    const levelTools = new LevelTools(this.bridge);
    const lightingTools = new LightingTools(this.bridge);
    const landscapeTools = new LandscapeTools(this.bridge);
    const foliageTools = new FoliageTools(this.bridge);
    const environmentTools = new EnvironmentTools(this.bridge);
    const debugTools = new DebugVisualizationTools(this.bridge);
    const performanceTools = new PerformanceTools(this.bridge);
    const audioTools = new AudioTools(this.bridge);
    const uiTools = new UITools(this.bridge);
    const sequenceTools = new SequenceTools(this.bridge);
    const introspectionTools = new IntrospectionTools(this.bridge);
    const engineTools = new EngineTools(this.bridge);

    // Wire AutomationBridge
    const toolsWithAutomation = [
      materialTools, animationTools, physicsTools, niagaraTools,
      lightingTools, landscapeTools, foliageTools, debugTools,
      performanceTools, audioTools, uiTools, introspectionTools,
      engineTools, environmentTools
    ];
    toolsWithAutomation.forEach(t => t.setAutomationBridge(this.automationBridge));

    // Lightweight system tools facade
    const systemTools = {
      executeConsoleCommand: (command: string) => this.bridge.executeConsoleCommand(command),
      getProjectSettings: async (section?: string) => {
        const category = typeof section === 'string' && section.trim().length > 0 ? section.trim() : 'Project';
        if (!this.automationBridge || !this.automationBridge.isConnected()) {
           // Fallback to reading from disk
           if (process.env.UE_PROJECT_PATH) {
               try {
                   const settings = await getProjectSetting(process.env.UE_PROJECT_PATH, category, '');
                   return {
                       success: true as const,
                       section: category,
                       settings: settings || {},
                       source: 'disk'
                   };
               } catch (_diskErr) {
                   return { success: false as const, error: 'Automation bridge not connected and disk read failed', section: category };
               }
           }
          return { success: false as const, error: 'Automation bridge not connected', section: category };
        }
        try {
          const resp: any = await this.automationBridge.sendAutomationRequest('system_control', {
            action: 'get_project_settings',
            category
          }, { timeoutMs: 30000 });
          
           const rawError = (resp?.error || '').toString();
        const msgLower = (resp?.message || '').toString().toLowerCase();

        const isNotImplemented = rawError.toUpperCase() === 'NOT_IMPLEMENTED' || msgLower.includes('not implemented');

        if (!resp || resp.success === false) {
          if (isNotImplemented) {
             // Fallback to reading from disk
             if (process.env.UE_PROJECT_PATH) {
                 try {
                     const settings = await getProjectSetting(process.env.UE_PROJECT_PATH, category, '');
                     return {
                         success: true as const,
                         section: category,
                         settings: settings || {},
                         source: 'disk'
                     };
                 } catch (_diskErr) {
                     // Ignore and fall through to stub
                 }
             }

            return {
              success: true as const,
              section: category,
              settings: {
                category,
                available: false,
                note: 'Project settings are not exposed by the current runtime but validation can proceed.'
              }
            };
          }

          return {
            success: false as const,
            error: rawError || resp?.message || 'Failed to get project settings',
            section: category,
            settings: resp?.result
          };
        }

        const result = resp.result && typeof resp.result === 'object' ? (resp.result as Record<string, unknown>) : {};
        const settings = (result.settings && typeof result.settings === 'object') ? (result.settings as Record<string, unknown>) : result;

        return {
          success: true as const,
          section: category,
          settings
        };
        } catch (e) {
           // Fallback to reading from disk on error
           if (process.env.UE_PROJECT_PATH) {
               try {
                   const settings = await getProjectSetting(process.env.UE_PROJECT_PATH, category, '');
                   return {
                       success: true as const,
                       section: category,
                       settings: settings || {},
                       source: 'disk'
                   };
               } catch (_diskErr) {
                   // Ignore
               }
           }
           return {
          success: false as const,
          error: `Failed to get project settings: ${e instanceof Error ? e.message : String(e)}`,
          section: category
        };
        }
      }
    };

    const elicitation = createElicitationHelper(this.server, this.logger);

    this.server.setRequestHandler(ListToolsRequestSchema, async () => {
      this.logger.info('Serving consolidated tools');
      const sanitized = (consolidatedToolDefinitions as any[]).map((t) => {
        try {
          const copy = JSON.parse(JSON.stringify(t));
          delete copy.outputSchema;
          return copy;
        } catch (_e) {
          return t;
        }
      });
      return { tools: sanitized };
    });

    this.server.setRequestHandler(CallToolRequestSchema, async (request) => {
      const { name } = request.params;
      let args: any = request.params.arguments || {};
      const startTime = Date.now();

      const connected = await this.ensureConnectedOnDemand();
      if (!connected) {
        // Allow certain tools (pipeline, system checks) to run without connection
        // offlineAllowed was here but unused, inline check below:
        if (name === 'system_control' && args.action === 'get_project_settings') {
            // Allowed
        } else if (name === 'manage_pipeline') {
            // Allowed
        } else {
            this.healthMonitor.trackPerformance(startTime, false);
            return {
              content: [{ type: 'text', text: `Cannot execute tool '${name}': Unreal Engine is not connected.` }],
              isError: true
            };
        }
      }

      const tools = {
        actorTools, assetTools, materialTools, editorTools, animationTools,
        physicsTools, niagaraTools, blueprintTools, levelTools, lightingTools,
        landscapeTools, foliageTools, environmentTools, debugTools, performanceTools,
        audioTools, systemTools, uiTools, sequenceTools, introspectionTools,
        engineTools,
        elicit: elicitation.elicit,
        supportsElicitation: elicitation.supports,
        elicitationTimeoutMs: this.defaultElicitationTimeoutMs,
        assetResources: this.assetResources,
        actorResources: this.actorResources,
        levelResources: this.levelResources,
        bridge: this.bridge,
        automationBridge: this.automationBridge
      };

      try {
        this.logger.debug(`Executing tool: ${name}`);
        
        // ... Elicitation logic ...
         try {
        const toolDef: any = (consolidatedToolDefinitions as any[]).find(t => t.name === name);
        const inputSchema: any = toolDef?.inputSchema;
        const elicitFn: any = (tools as any).elicit;
        if (inputSchema && typeof elicitFn === 'function') {
          const props = inputSchema.properties || {};
          const required: string[] = Array.isArray(inputSchema.required) ? inputSchema.required : [];
          const missing = required.filter((k: string) => {
            const v = (args as any)[k];
            if (v === undefined || v === null) return true;
            if (typeof v === 'string' && v.trim() === '') return true;
            return false;
          });

          const primitiveProps: any = {};
          for (const k of missing) {
            const p = props[k];
            if (!p || typeof p !== 'object') continue;
            const t = (p.type || '').toString();
            const isEnum = Array.isArray(p.enum);
            if (t === 'string' || t === 'number' || t === 'integer' || t === 'boolean' || isEnum) {
              primitiveProps[k] = {
                type: t || (isEnum ? 'string' : undefined),
                title: p.title,
                description: p.description,
                enum: p.enum,
                enumNames: p.enumNames,
                minimum: p.minimum,
                maximum: p.maximum,
                minLength: p.minLength,
                maxLength: p.maxLength,
                pattern: p.pattern,
                format: p.format,
                default: p.default
              };
            }
          }

          if (Object.keys(primitiveProps).length > 0) {
            const elicitOptions: any = { fallback: async () => ({ ok: false, error: 'missing-params' }) };
            if (typeof (tools as any).elicitationTimeoutMs === 'number' && Number.isFinite((tools as any).elicitationTimeoutMs)) {
              elicitOptions.timeoutMs = (tools as any).elicitationTimeoutMs;
            }
            const elicitRes = await elicitFn(
              `Provide missing parameters for ${name}`,
              { type: 'object', properties: primitiveProps, required: Object.keys(primitiveProps) },
              elicitOptions
            );
            if (elicitRes && elicitRes.ok && elicitRes.value) {
              args = { ...args, ...elicitRes.value };
            }
          }
        }
      } catch (e) {
        this.logger.debug('Generic elicitation prefill skipped', { err: (e as any)?.message || String(e) });
      }

        let result = await handleConsolidatedToolCall(name, args, tools);
        this.logger.debug(`Tool ${name} returned result`);
        result = cleanObject(result);

        const explicitSuccess = typeof (result as any)?.success === 'boolean' ? Boolean((result as any).success) : undefined;
        const wrappedResult = await responseValidator.wrapResponse(name, result);
        
        let wrappedSuccess: boolean | undefined = undefined;
        try {
          const sc: any = (wrappedResult as any).structuredContent;
          if (sc && typeof sc.success === 'boolean') wrappedSuccess = Boolean(sc.success);
        } catch { }

        const isErrorResponse = Boolean((wrappedResult as any)?.isError === true);
        const tentative = explicitSuccess ?? wrappedSuccess;
        const finalSuccess = tentative === true && !isErrorResponse;

        this.healthMonitor.trackPerformance(startTime, finalSuccess);
        
         const durationMs = Date.now() - startTime;
      if (finalSuccess) {
        this.logger.info(`Tool ${name} completed successfully in ${durationMs}ms`);
      } else {
        this.logger.warn(`Tool ${name} completed with errors in ${durationMs}ms`);
      }

      const responsePreview = JSON.stringify(wrappedResult).substring(0, 100);
      this.logger.debug(`Returning response to MCP client: ${responsePreview}...`);

        return wrappedResult;
      } catch (error) {
        this.healthMonitor.trackPerformance(startTime, false);
        const errorResponse = ErrorHandler.createErrorResponse(error, name, { ...args, scope: `tool-call/${name}` });
        this.logger.error(`Tool execution failed: ${name}`, errorResponse);
        this.healthMonitor.recordError(errorResponse);
        
        const sanitizedError = cleanObject(errorResponse);
        try {
          (sanitizedError as any).isError = true;
        } catch { }
        return responseValidator.wrapResponse(name, sanitizedError);
      }
    });
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