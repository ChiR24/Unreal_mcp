import 'dotenv/config';
import { Server } from '@modelcontextprotocol/sdk/server/index.js';
import { StdioServerTransport } from '@modelcontextprotocol/sdk/server/stdio.js';
import { Logger } from './utils/logger.js';
import { UnrealBridge } from './unreal-bridge.js';
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
import { DebugVisualizationTools } from './tools/debug.js';
import { PerformanceTools } from './tools/performance.js';
import { AudioTools } from './tools/audio.js';
import { UITools } from './tools/ui.js';
import { RcTools } from './tools/rc.js';
import { SequenceTools } from './tools/sequence.js';
import { IntrospectionTools } from './tools/introspection.js';
import { VisualTools } from './tools/visual.js';
import { EngineTools } from './tools/engine.js';
import { toolDefinitions } from './tools/tool-definitions.js';
import { handleToolCall } from './tools/tool-handlers.js';
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
import { ErrorHandler } from './utils/error-handler.js';
import { routeStdoutLogsToStderr } from './utils/stdio-redirect.js';
import { cleanObject } from './utils/safe-json.js';

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
  // Tool mode: true = consolidated (13 tools), false = individual (36+ tools)
  USE_CONSOLIDATED_TOOLS: process.env.USE_CONSOLIDATED_TOOLS !== 'false',
  // Connection retry settings
  MAX_RETRY_ATTEMPTS: 3,
  RETRY_DELAY_MS: 2000,
  // Server info
  SERVER_NAME: 'unreal-engine-mcp',
  SERVER_VERSION: '0.3.1',
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

export async function createServer() {
  const bridge = new UnrealBridge();
  // Disable auto-reconnect loops; connect only on-demand
  bridge.setAutoReconnectEnabled(false);
  
  // Initialize response validation with schemas
  log.debug('Initializing response validation...');
  const toolDefs = CONFIG.USE_CONSOLIDATED_TOOLS ? consolidatedToolDefinitions : toolDefinitions;
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
  const assetTools = new AssetTools(bridge);
  const editorTools = new EditorTools(bridge);
  const materialTools = new MaterialTools(bridge);
  const animationTools = new AnimationTools(bridge);
  const physicsTools = new PhysicsTools(bridge);
  const niagaraTools = new NiagaraTools(bridge);
  const blueprintTools = new BlueprintTools(bridge);
  const levelTools = new LevelTools(bridge);
  const lightingTools = new LightingTools(bridge);
  const landscapeTools = new LandscapeTools(bridge);
  const foliageTools = new FoliageTools(bridge);
  const debugTools = new DebugVisualizationTools(bridge);
  const performanceTools = new PerformanceTools(bridge);
  const audioTools = new AudioTools(bridge);
  const uiTools = new UITools(bridge);
  const rcTools = new RcTools(bridge);
  const sequenceTools = new SequenceTools(bridge);
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
        prompts: {},
        logging: {}
      }
    }
  );

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
      const ok = await ensureConnectedOnDemand();
      if (!ok) {
        return { contents: [{ uri, mimeType: 'text/plain', text: 'Unreal Engine not connected (after 3 attempts).' }] };
      }
      try {
        const exposed = await bridge.getExposed();
        return {
          contents: [{
            uri,
            mimeType: 'application/json',
            text: JSON.stringify(exposed, null, 2)
          }]
        };
      } catch {
        return {
          contents: [{
            uri,
            mimeType: 'text/plain',
            text: 'Failed to get exposed properties. Ensure Remote Control is configured.'
          }]
        };
      }
    }
    
    if (uri === 'ue://health') {
      const uptimeMs = Date.now() - metrics.uptime;
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
          httpPort: process.env.UE_RC_HTTP_PORT || 30010,
          wsPort: process.env.UE_RC_WS_PORT || 30020,
          engineVersion: versionInfo,
          features: {
            pythonEnabled: featureFlags.pythonEnabled === true,
            subsystems: featureFlags.subsystems || {},
            rcHttpReachable: bridge.isConnected
          }
        },
        recentErrors: metrics.recentErrors.slice(-5)
      };
      
      return {
        contents: [{
          uri,
          mimeType: 'application/json',
          text: JSON.stringify(health, null, 2)
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

  // Handle tool listing - switch between consolidated (13) or individual (36) tools
  server.setRequestHandler(ListToolsRequestSchema, async () => {
    log.info(`Serving ${CONFIG.USE_CONSOLIDATED_TOOLS ? 'consolidated' : 'individual'} tools`);
    return {
      tools: CONFIG.USE_CONSOLIDATED_TOOLS ? consolidatedToolDefinitions : toolDefinitions
    };
  });

  // Handle tool calls - switch between consolidated (13) or individual (36) tools
  server.setRequestHandler(CallToolRequestSchema, async (request) => {
    const { name, arguments: args } = request.params;
    const startTime = Date.now();

    // Ensure connection only when needed, with 3 attempts
    const connected = await ensureConnectedOnDemand();
    if (!connected) {
      const notConnected = {
        content: [{ type: 'text', text: 'Unreal Engine is not connected (after 3 attempts). Please open UE and try again.' }],
        success: false,
        error: 'UE_NOT_CONNECTED',
        retriable: false,
        scope: `tool-call/${name}`
      } as any;
      trackPerformance(startTime, false);
      return notConnected;
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
      debugTools,
      performanceTools,
      audioTools,
      uiTools,
      rcTools,
      sequenceTools,
      introspectionTools,
      visualTools,
      engineTools,
      bridge
    };
    
    // Use consolidated or individual handler based on configuration
    try {
      log.debug(`Executing tool: ${name}`);
      
      let result;
      if (CONFIG.USE_CONSOLIDATED_TOOLS) {
        result = await handleConsolidatedToolCall(name, args, tools);
      } else {
        result = await handleToolCall(name, args, tools);
      }
      
      log.debug(`Tool ${name} returned result`);
      
      // Clean the result to remove circular references
      result = cleanObject(result);
      
      // Validate and enhance response
      result = responseValidator.wrapResponse(name, result);
      
      trackPerformance(startTime, true);
      
      log.info(`Tool ${name} completed successfully in ${Date.now() - startTime}ms`);
      
      // Log that we're returning the response
      const responsePreview = JSON.stringify(result).substring(0, 100);
      log.debug(`Returning response to MCP client: ${responsePreview}...`);
      
      return result;
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
      
      return {
        content: [{
          type: 'text',
          text: errorResponse.message || `Failed to execute ${name}`
        }],
        ...errorResponse
      };
    }
  });

  // Handle prompt listing
  server.setRequestHandler(ListPromptsRequestSchema, async () => {
    return {
      prompts: prompts.map(p => ({
        name: p.name,
        description: p.description,
        arguments: Object.entries(p.arguments || {}).map(([name, schema]) => ({
          name,
          description: schema.description,
          required: schema.required || false
        }))
      }))
    };
  });

  // Handle prompt retrieval
  server.setRequestHandler(GetPromptRequestSchema, async (request) => {
    const prompt = prompts.find(p => p.name === request.params.name);
    if (!prompt) {
      throw new Error(`Unknown prompt: ${request.params.name}`);
    }
    
    // Return a template for the lighting setup
    return {
      messages: [
        {
          role: 'user',
          content: {
            type: 'text',
            text: `Set up three-point lighting with ${request.params.arguments?.intensity || 'medium'} intensity`
          }
        }
      ]
    };
  });

  return { server, bridge };
}

export async function startStdioServer() {
  const { server } = await createServer();
  const transport = new StdioServerTransport();
  
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

// Start the server when run directly
if (import.meta.url === `file://${process.argv[1].replace(/\\/g, '/')}`) {
  startStdioServer().catch(error => {
    console.error('Failed to start server:', error);
    process.exit(1);
  });
}
