import 'dotenv/config';
import { Server } from '@modelcontextprotocol/sdk/server/index.js';
import { StdioServerTransport } from '@modelcontextprotocol/sdk/server/stdio.js';
import { Logger } from './utils/logger.js';
import { UnrealBridge } from './unreal-bridge.js';
import { AutomationBridge } from './automation-bridge.js';
import { createRequire } from 'node:module';
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
import { BuildEnvironmentAdvanced } from './tools/build_environment_advanced.js';
import { FoliageTools } from './tools/foliage.js';
import { DebugVisualizationTools } from './tools/debug.js';
import { PerformanceTools } from './tools/performance.js';
import { AudioTools } from './tools/audio.js';
import { UITools } from './tools/ui.js';
import { RcTools } from './tools/rc.js';
import { SequenceTools } from './tools/sequence.js';
import { IntrospectionTools } from './tools/introspection.js';
import { VisualTools } from './tools/visual.js';
import { EngineTools } from './tools/engine.js';
import { consolidatedToolDefinitions } from './tools/consolidated-tool-definitions.js';
import { handleConsolidatedToolCall } from './tools/consolidated-tool-handlers.js';
import { prompts } from './prompts/index.js';
import { 
  CallToolRequestSchema, 
  ListToolsRequestSchema,
  ListResourcesRequestSchema,
  ReadResourceRequestSchema,
  ListPromptsRequestSchema,
  GetPromptRequestSchema
} from '@modelcontextprotocol/sdk/types.js';
import { responseValidator } from './utils/response-validator.js';
import { z } from 'zod';
import { routeStdoutLogsToStderr } from './utils/stdio-redirect.js';
import { createElicitationHelper } from './utils/elicitation.js';
import { cleanObject } from './utils/safe-json.js';
import { ErrorHandler } from './utils/error-handler.js';

const require = createRequire(import.meta.url);
const packageInfo: { name?: string; version?: string } = (() => {
  try {
    return require('../package.json');
  } catch (error) {
    const log = new Logger('UE-MCP');
    log.debug('Unable to read package.json for server metadata', error);
    return {};
  }
})();
const DEFAULT_SERVER_NAME = typeof packageInfo.name === 'string' && packageInfo.name.trim().length > 0
  ? packageInfo.name
  : 'unreal-engine-mcp';
const DEFAULT_SERVER_VERSION = typeof packageInfo.version === 'string' && packageInfo.version.trim().length > 0
  ? packageInfo.version
  : '0.0.0';

const ListCommandsRequestSchema = z.object({
  method: z.literal('commands/list'),
  params: z
    .object({
      _meta: z.any().optional()
    })
    .partial()
    .optional()
});

const listCommandsResponse = {
  commands: [
    { name: 'ping', description: 'Check connectivity' },
    { name: 'tools/list', description: 'List available tools' },
    { name: 'tools/call', description: 'Invoke a tool by name' },
    { name: 'resources/list', description: 'List available resources' },
    { name: 'resources/read', description: 'Read the contents of a resource' },
    { name: 'prompts/list', description: 'List prompt templates' },
    { name: 'prompts/get', description: 'Fetch a prompt template' }
  ]
} as const;

const log = new Logger('UE-MCP');

// Ensure stdout remains JSON-only for MCP by routing logs to stderr unless opted out.
routeStdoutLogsToStderr();

// Performance metrics
interface PerformanceMetrics {
  totalRequests: number;
  successfulRequests: number;
  failedRequests: number;
  averageResponseTime: number;
  responseTimes: number[];
  connectionStatus: 'connected' | 'disconnected' | 'error';
  lastHealthCheck: Date;
  uptime: number;
  recentErrors: Array<{ time: string; scope: string; type: string; message: string; retriable: boolean }>;
}

const metrics: PerformanceMetrics = {
  totalRequests: 0,
  successfulRequests: 0,
  failedRequests: 0,
  averageResponseTime: 0,
  responseTimes: [],
  connectionStatus: 'disconnected',
  lastHealthCheck: new Date(),
  uptime: Date.now(),
  recentErrors: []
};

// Health check timer and last success tracking (stop pings after inactivity)
let healthCheckTimer: NodeJS.Timeout | undefined;
let lastHealthSuccessAt = 0;

// Configuration
const CONFIG = {
  // Tooling: use consolidated tools only (13 tools)
  // Connection retry settings
  MAX_RETRY_ATTEMPTS: 3,
  RETRY_DELAY_MS: 2000,
  // Server info
  SERVER_NAME: DEFAULT_SERVER_NAME,
  SERVER_VERSION: DEFAULT_SERVER_VERSION,
  AUTOMATION_HEARTBEAT_MS: 15000,
  // Monitoring
  HEALTH_CHECK_INTERVAL_MS: 30000 // 30 seconds
};

// Helper function to track performance
function trackPerformance(startTime: number, success: boolean) {
  const responseTime = Date.now() - startTime;
  metrics.totalRequests++;
  if (success) {
    metrics.successfulRequests++;
  } else {
    metrics.failedRequests++;
  }
  
  // Keep last 100 response times for average calculation
  metrics.responseTimes.push(responseTime);
  if (metrics.responseTimes.length > 100) {
    metrics.responseTimes.shift();
  }
  
  // Calculate average
  metrics.averageResponseTime = metrics.responseTimes.reduce((a, b) => a + b, 0) / metrics.responseTimes.length;
}

// Health check function
async function performHealthCheck(bridge: UnrealBridge): Promise<boolean> {
  // If not connected, do not attempt any ping (stay quiet)
  if (!bridge.isConnected) {
    return false;
  }
  try {
    // Use a safe echo command that doesn't affect any settings
    await bridge.executeConsoleCommand('echo MCP Server Health Check');
    metrics.connectionStatus = 'connected';
    metrics.lastHealthCheck = new Date();
    lastHealthSuccessAt = Date.now();
    return true;
  } catch (err1) {
    // Fallback: minimal Python ping (if Python plugin is enabled)
    try {
      await bridge.executePython("import sys; sys.stdout.write('OK')");
      metrics.connectionStatus = 'connected';
      metrics.lastHealthCheck = new Date();
      lastHealthSuccessAt = Date.now();
      return true;
    } catch (err2) {
      metrics.connectionStatus = 'error';
      metrics.lastHealthCheck = new Date();
      // Avoid noisy warnings when engine may be shutting down; log at debug
      log.debug('Health check failed (console and python):', err1, err2);
      return false;
    }
  }
}

export function createServer() {
  const bridge = new UnrealBridge();
  // Disable auto-reconnect loops; connect only on-demand
  bridge.setAutoReconnectEnabled(false);

  const automationBridge = new AutomationBridge({
    serverName: CONFIG.SERVER_NAME,
    serverVersion: CONFIG.SERVER_VERSION,
    heartbeatIntervalMs: CONFIG.AUTOMATION_HEARTBEAT_MS,
    clientMode: process.env.MCP_AUTOMATION_CLIENT_MODE === 'true'
  });
  bridge.setAutomationBridge(automationBridge);
  automationBridge.start();

  automationBridge.on('connected', ({ metadata, port, protocol }) => {
    log.info(
      `Automation bridge connected (port=${port}, protocol=${protocol ?? 'none'})`,
      metadata
    );
  });

  automationBridge.on('disconnected', ({ code, reason, port, protocol }) => {
    log.info(
      `Automation bridge disconnected (code=${code}, reason=${reason || 'n/a'}, port=${port}, protocol=${protocol ?? 'none'})`
    );
  });

  automationBridge.on('handshakeFailed', ({ reason, port }) => {
    log.warn(`Automation bridge handshake failed (port=${port}): ${reason}`);
  });

  automationBridge.on('message', (message) => {
    log.debug('Automation bridge inbound message', message);
  });

  automationBridge.on('error', (error) => {
    log.error('Automation bridge error', error);
  });
  
// Initialize response validation with schemas
  log.debug('Initializing response validation...');
  const toolDefs = consolidatedToolDefinitions;
  toolDefs.forEach((tool: any) => {
    if (tool.outputSchema) {
      responseValidator.registerSchema(tool.name, tool.outputSchema);
    }
  });
  // Summary at debug level to avoid repeated noisy blocks in some shells
  log.debug(`Registered ${responseValidator.getStats().totalSchemas} output schemas for validation`);

  // Do NOT connect to Unreal at startup; connect on demand
  log.debug('Server starting without connecting to Unreal Engine');
  metrics.connectionStatus = 'disconnected';

  // Health checks manager (only active when connected)
  const startHealthChecks = () => {
    if (healthCheckTimer) return;
    lastHealthSuccessAt = Date.now();
    healthCheckTimer = setInterval(async () => {
      // Only attempt health pings while connected; stay silent otherwise
      if (!bridge.isConnected) {
        // Optionally pause fully after 5 minutes of no success
        const FIVE_MIN_MS = 5 * 60 * 1000;
        if (!lastHealthSuccessAt || Date.now() - lastHealthSuccessAt > FIVE_MIN_MS) {
          if (healthCheckTimer) {
            clearInterval(healthCheckTimer);
            healthCheckTimer = undefined;
          }
          log.info('Health checks paused after 5 minutes without a successful response');
        }
        return;
      }

      await performHealthCheck(bridge);
      // Stop sending echoes if we haven't had a successful response in > 5 minutes
      const FIVE_MIN_MS = 5 * 60 * 1000;
      if (!lastHealthSuccessAt || Date.now() - lastHealthSuccessAt > FIVE_MIN_MS) {
        if (healthCheckTimer) {
          clearInterval(healthCheckTimer);
          healthCheckTimer = undefined;
          log.info('Health checks paused after 5 minutes without a successful response');
        }
      }
    }, CONFIG.HEALTH_CHECK_INTERVAL_MS);
  };

  // On-demand connection helper
  const ensureConnectedOnDemand = async (): Promise<boolean> => {
    if (bridge.isConnected) return true;
    const ok = await bridge.tryConnect(3, 5000, 1000);
    if (ok) {
      metrics.connectionStatus = 'connected';
      startHealthChecks();
    } else {
      metrics.connectionStatus = 'disconnected';
    }
    return ok;
  };

  // Resources
  const assetResources = new AssetResources(bridge);
  const actorResources = new ActorResources(bridge);
  const levelResources = new LevelResources(bridge);
  
  // Tools
  const actorTools = new ActorTools(bridge);
  const assetTools = new AssetTools(bridge, automationBridge);
  const editorTools = new EditorTools(bridge);
  const materialTools = new MaterialTools(bridge);
  const animationTools = new AnimationTools(bridge);
  const physicsTools = new PhysicsTools(bridge);
  const niagaraTools = new NiagaraTools(bridge);
  const blueprintTools = new BlueprintTools(bridge, automationBridge);
  const levelTools = new LevelTools(bridge);
  const lightingTools = new LightingTools(bridge);
  const landscapeTools = new LandscapeTools(bridge);
  const foliageTools = new FoliageTools(bridge);
  const buildEnvAdvanced = new BuildEnvironmentAdvanced(bridge);
  const debugTools = new DebugVisualizationTools(bridge);
  const performanceTools = new PerformanceTools(bridge);
  const audioTools = new AudioTools(bridge);
  const uiTools = new UITools(bridge);
  const rcTools = new RcTools(bridge);
  const sequenceTools = new SequenceTools(bridge, automationBridge);
  const introspectionTools = new IntrospectionTools(bridge);
  const visualTools = new VisualTools(bridge);
  const engineTools = new EngineTools(bridge);

  const server = new Server(
    {
      name: CONFIG.SERVER_NAME,
      version: CONFIG.SERVER_VERSION
    },
    {
      capabilities: {
        resources: {},
        tools: {},
        prompts: {
          listChanged: false
        },
        logging: {}
      }
    }
  );

  // Optional elicitation helper – used only if client supports it.
  const elicitation = createElicitationHelper(server as any, log);
  const defaultElicitationTimeoutMs = elicitation.getDefaultTimeoutMs();

  const createNotConnectedResponse = (toolName: string) => {
    const payload = {
      success: false,
      error: 'UE_NOT_CONNECTED',
      message: 'Unreal Engine is not connected (after 3 attempts). Please open UE and try again.',
      retriable: false,
      scope: `tool-call/${toolName}`
    } as const;

    return responseValidator.wrapResponse(toolName, {
      ...payload,
      content: [
        {
          type: 'text',
          text: JSON.stringify(payload, null, 2)
        }
      ]
    });
  };

  // Handle commands listing for MCP inspector compatibility
  server.setRequestHandler(ListCommandsRequestSchema, async () => listCommandsResponse);

  // Handle resource listing
  server.setRequestHandler(ListResourcesRequestSchema, async () => {
    return {
      resources: [
        {
          uri: 'ue://assets',
          name: 'Project Assets',
          description: 'List all assets in the project',
          mimeType: 'application/json'
        },
        {
          uri: 'ue://actors', 
          name: 'Level Actors',
          description: 'List all actors in the current level',
          mimeType: 'application/json'
        },
        {
          uri: 'ue://level',
          name: 'Current Level',
          description: 'Information about the current level',
          mimeType: 'application/json'
        },
        {
          uri: 'ue://exposed',
          name: 'Remote Control Exposed',
          description: 'List all exposed properties via Remote Control',
          mimeType: 'application/json'
        },
        {
          uri: 'ue://health',
          name: 'Health Status',
          description: 'Server health and performance metrics',
          mimeType: 'application/json'
        },
        {
          uri: 'ue://automation-bridge',
          name: 'Automation Bridge',
          description: 'Automation bridge diagnostics and recent activity',
          mimeType: 'application/json'
        },
        {
          uri: 'ue://version',
          name: 'Engine Version',
          description: 'Unreal Engine version and compatibility info',
          mimeType: 'application/json'
        }
      ]
    };
  });

  // Handle resource reading
  server.setRequestHandler(ReadResourceRequestSchema, async (request) => {
    const uri = request.params.uri;
    
    if (uri === 'ue://assets') {
      const ok = await ensureConnectedOnDemand();
      if (!ok) {
        return { contents: [{ uri, mimeType: 'text/plain', text: 'Unreal Engine not connected (after 3 attempts).' }] };
      }
      const list = await assetResources.list('/Game', true);
      return {
        contents: [{
          uri,
          mimeType: 'application/json',
          text: JSON.stringify(list, null, 2)
        }]
      };
    }
    
    if (uri === 'ue://actors') {
      const ok = await ensureConnectedOnDemand();
      if (!ok) {
        return { contents: [{ uri, mimeType: 'text/plain', text: 'Unreal Engine not connected (after 3 attempts).' }] };
      }
      const list = await actorResources.listActors();
      return {
        contents: [{
          uri,
          mimeType: 'application/json',
          text: JSON.stringify(list, null, 2)
        }]
      };
    }
    
    if (uri === 'ue://level') {
      const ok = await ensureConnectedOnDemand();
      if (!ok) {
        return { contents: [{ uri, mimeType: 'text/plain', text: 'Unreal Engine not connected (after 3 attempts).' }] };
      }
      const level = await levelResources.getCurrentLevel();
      return {
        contents: [{
          uri,
          mimeType: 'application/json',
          text: JSON.stringify(level, null, 2)
        }]
      };
    }
    
    if (uri === 'ue://exposed') {
      return {
        contents: [{
          uri,
          mimeType: 'text/plain',
          text: 'Remote Control exposed properties are no longer available; tools now rely on the automation bridge.'
        }]
      };
    }
    
    if (uri === 'ue://health') {
      const uptimeMs = Date.now() - metrics.uptime;
      const automationStatus = automationBridge.getStatus();
      // Query engine version and feature flags only when connected
      let versionInfo: any = {};
      let featureFlags: any = {};
      if (bridge.isConnected) {
        try { versionInfo = await bridge.getEngineVersion(); } catch {}
        try { featureFlags = await bridge.getFeatureFlags(); } catch {}
      }
      const health = {
        status: metrics.connectionStatus,
        uptime: Math.floor(uptimeMs / 1000),
        performance: {
          totalRequests: metrics.totalRequests,
          successfulRequests: metrics.successfulRequests,
          failedRequests: metrics.failedRequests,
          successRate: metrics.totalRequests > 0 ? 
            (metrics.successfulRequests / metrics.totalRequests * 100).toFixed(2) + '%' : 'N/A',
          averageResponseTime: Math.round(metrics.averageResponseTime) + 'ms'
        },
        lastHealthCheck: metrics.lastHealthCheck.toISOString(),
        unrealConnection: {
          status: bridge.isConnected ? 'connected' : 'disconnected',
          host: process.env.UE_HOST || 'localhost',
          transport: 'automation_bridge',
          engineVersion: versionInfo,
          features: {
            pythonEnabled: featureFlags.pythonEnabled === true,
            subsystems: featureFlags.subsystems || {},
            automationBridgeConnected: automationStatus.connected
          }
        },
        recentErrors: metrics.recentErrors.slice(-5),
        automationBridge: automationStatus
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
      const status = automationBridge.getStatus();
      const content = {
        summary: {
          enabled: status.enabled,
          connected: status.connected,
          host: status.host,
          port: status.port,
          capabilityTokenRequired: status.capabilityTokenRequired,
          pendingRequests: status.pendingRequests
        },
        // Detailed active connections for admin/health dashboards
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
      const ok = await ensureConnectedOnDemand();
      if (!ok) {
        return { contents: [{ uri, mimeType: 'text/plain', text: 'Unreal Engine not connected (after 3 attempts).' }] };
      }
      const info = await bridge.getEngineVersion();
      return {
        contents: [{
          uri,
          mimeType: 'application/json',
          text: JSON.stringify(info, null, 2)
        }]
      };
    }
    
    throw new Error(`Unknown resource: ${uri}`);
  });

// Handle tool listing - consolidated tools only
  server.setRequestHandler(ListToolsRequestSchema, async () => {
    log.info('Serving consolidated tools');
    // Return a sanitized copy of tool definitions to clients. Some client
    // SDKs attempt to treat an "outputSchema" as a Zod schema and call
    // `.parse()` on it, which fails when the server provides a JSON Schema
    // object. To remain compatible with diverse client SDKs, strip
    // non-serializable or ambiguous schema objects (outputSchema) from the
    // tool listing while keeping the server-side validation of those
    // schemas intact.
    const sanitized = (consolidatedToolDefinitions as any[]).map((t) => {
      try {
        const copy = JSON.parse(JSON.stringify(t));
        // Remove outputSchema so clients won't accidentally treat it as a
        // Zod schema instance and attempt to call `.parse()` on it.
        delete copy.outputSchema;
        return copy;
      } catch (_e) {
        return t;
      }
    });
    return { tools: sanitized };
  });

  // Handle tool calls - consolidated tools only (13)
  server.setRequestHandler(CallToolRequestSchema, async (request) => {
    const { name } = request.params;
    let args: any = request.params.arguments || {};
    const startTime = Date.now();

    // Ensure connection only when needed, with 3 attempts
    const connected = await ensureConnectedOnDemand();
    if (!connected) {
      trackPerformance(startTime, false);
      return createNotConnectedResponse(name);
    }
    
// Create tools object for handler
    const tools = {
      actorTools,
      assetTools,
      materialTools,
      editorTools,
      animationTools,
      physicsTools,
      niagaraTools,
      blueprintTools,
      levelTools,
      lightingTools,
      landscapeTools,
      foliageTools,
      buildEnvAdvanced,
      debugTools,
      performanceTools,
      audioTools,
      uiTools,
      rcTools,
      sequenceTools,
      introspectionTools,
      visualTools,
      engineTools,
      // Elicitation (client-optional)
      elicit: elicitation.elicit,
      supportsElicitation: elicitation.supports,
  elicitationTimeoutMs: defaultElicitationTimeoutMs,
      // Resources for listing and info
      assetResources,
      actorResources,
      levelResources,
      bridge,
      automationBridge
    };
    
// Execute consolidated tool handler
    try {
      log.debug(`Executing tool: ${name}`);
      
      // Opportunistic generic elicitation for missing primitive required fields
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

          // Build a flat primitive-only schema subset per MCP Elicitation rules
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
        log.debug('Generic elicitation prefill skipped', { err: (e as any)?.message || String(e) });
      }

      let result = await handleConsolidatedToolCall(name, args, tools);

      log.debug(`Tool ${name} returned result`);

      // Clean the result to remove circular references
      result = cleanObject(result);

      const explicitSuccess = typeof (result as any)?.success === 'boolean' ? Boolean((result as any).success) : undefined;

      // Validate and enhance response
      const wrappedResult = responseValidator.wrapResponse(name, result);

      const wrappedSuccess = typeof (wrappedResult as any)?.success === 'boolean'
        ? Boolean((wrappedResult as any).success)
        : undefined;
      const isErrorResponse = Boolean((wrappedResult as any)?.isError === true);

      const tentative = explicitSuccess ?? wrappedSuccess;
      const finalSuccess = tentative !== undefined ? (tentative && !isErrorResponse) : !isErrorResponse;

      trackPerformance(startTime, finalSuccess);

      const durationMs = Date.now() - startTime;
      if (finalSuccess) {
        log.info(`Tool ${name} completed successfully in ${durationMs}ms`);
      } else {
        log.warn(`Tool ${name} completed with errors in ${durationMs}ms`);
      }

      // Log that we're returning the response
      const responsePreview = JSON.stringify(wrappedResult).substring(0, 100);
      log.debug(`Returning response to MCP client: ${responsePreview}...`);

      return wrappedResult;
    } catch (error) {
      trackPerformance(startTime, false);
      
      // Use consistent error handling
      const errorResponse = ErrorHandler.createErrorResponse(error, name, { ...args, scope: `tool-call/${name}` });
      log.error(`Tool execution failed: ${name}`, errorResponse);
      // Record error for health diagnostics
      try {
        metrics.recentErrors.push({
          time: new Date().toISOString(),
          scope: (errorResponse as any).scope || `tool-call/${name}`,
          type: (errorResponse as any)._debug?.errorType || 'UNKNOWN',
          message: (errorResponse as any).error || (errorResponse as any).message || 'Unknown error',
          retriable: Boolean((errorResponse as any).retriable)
        });
        if (metrics.recentErrors.length > 20) metrics.recentErrors.splice(0, metrics.recentErrors.length - 20);
      } catch {}

      const sanitizedError = cleanObject(errorResponse);
      let errorText = '';
      try {
        errorText = JSON.stringify(sanitizedError, null, 2);
      } catch {
        errorText = sanitizedError.message || errorResponse.message || `Failed to execute ${name}`;
      }

      const wrappedError = {
        ...sanitizedError,
        isError: true,
        content: [{
          type: 'text',
          text: errorText
        }]
      };

      return responseValidator.wrapResponse(name, wrappedError);
    }
  });

  // Handle prompt listing
  server.setRequestHandler(ListPromptsRequestSchema, async () => {
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

  // Handle prompt retrieval
  server.setRequestHandler(GetPromptRequestSchema, async (request) => {
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

  return { server, bridge, automationBridge };
}

// Export configuration schema for Smithery session UI and validation
export const configSchema = z.object({
  ueHost: z.string().optional().default('127.0.0.1').describe('Unreal Engine host (used for display and telemetry only)'),
  logLevel: z.enum(['debug', 'info', 'warn', 'error']).optional().default('info').describe('Runtime log level'),
  projectPath: z.string().optional().default('C:/Users/YourName/Documents/Unreal Projects/YourProject').describe('Absolute path to your Unreal .uproject file')
});

// Default export expected by Smithery TypeScript runtime. Accepts an optional config object
// and injects values into the environment before creating the server.
export default function createServerDefault({ config }: { config?: any } = {}) {
  try {
    if (config) {
      if (typeof config.ueHost === 'string' && config.ueHost.trim()) process.env.UE_HOST = config.ueHost;
  if (typeof config.logLevel === 'string') process.env.LOG_LEVEL = config.logLevel;
  if (typeof config.projectPath === 'string' && config.projectPath.trim()) process.env.UE_PROJECT_PATH = config.projectPath;
    }
  } catch (e) {
    // Non-fatal: log and continue (console to avoid circular logger dependencies at top-level)
    console.debug('[createServerDefault] Failed to apply config to environment:', (e as any)?.message || e);
  }

  const { server } = createServer();
  return server;
}

export async function startStdioServer() {
  const { server, automationBridge } = createServer();
  const transport = new StdioServerTransport();
  let shuttingDown = false;

  const handleShutdown = async (signal?: NodeJS.Signals) => {
    if (shuttingDown) {
      return;
    }
    shuttingDown = true;
    const reason = signal ? ` due to ${signal}` : '';
    log.info(`Shutting down MCP server${reason}`);
    try {
      automationBridge.stop();
    } catch (error) {
      log.warn('Failed to stop automation bridge cleanly', error);
    }

    try {
      if (typeof (server as any).close === 'function') {
        await (server as any).close();
      }
    } catch (error) {
      log.warn('Failed to close MCP server transport cleanly', error);
    }

    if (signal) {
      process.exit(0);
    }
  };

  ['SIGINT', 'SIGTERM'].forEach((signal) => {
    process.once(signal as NodeJS.Signals, () => {
      void handleShutdown(signal as NodeJS.Signals);
    });
  });

  process.once('beforeExit', () => {
    automationBridge.stop();
  });

  process.once('exit', () => {
    automationBridge.stop();
  });
  
  // Add debugging for transport messages
  const originalWrite = process.stdout.write;
  process.stdout.write = function(...args: any[]) {
    const message = args[0];
    if (typeof message === 'string' && message.includes('jsonrpc')) {
      log.debug(`Sending to client: ${message.substring(0, 200)}...`);
    }
    return originalWrite.apply(process.stdout, args as any);
  } as any;
  
  await server.connect(transport);
  log.info('Unreal Engine MCP Server started on stdio');
}

// Direct execution is handled via src/cli.ts to keep this module side-effect free.
