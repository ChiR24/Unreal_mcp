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

const log = new Logger('UE-MCP');

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
}

const metrics: PerformanceMetrics = {
  totalRequests: 0,
  successfulRequests: 0,
  failedRequests: 0,
  averageResponseTime: 0,
  responseTimes: [],
  connectionStatus: 'disconnected',
  lastHealthCheck: new Date(),
  uptime: Date.now()
};

// Configuration
const CONFIG = {
  // Tool mode: true = consolidated (10 tools), false = individual (36+ tools)
  USE_CONSOLIDATED_TOOLS: process.env.USE_CONSOLIDATED_TOOLS !== 'false',
  // Connection retry settings
  MAX_RETRY_ATTEMPTS: 3,
  RETRY_DELAY_MS: 2000,
  // Server info
  SERVER_NAME: 'unreal-engine-mcp',
  SERVER_VERSION: '1.0.0',
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
  try {
    // Try to get exposed properties as a health check
    await bridge.getExposed();
    metrics.connectionStatus = 'connected';
    metrics.lastHealthCheck = new Date();
    return true;
  } catch (err) {
    metrics.connectionStatus = 'error';
    metrics.lastHealthCheck = new Date();
    log.warn('Health check failed:', err);
    return false;
  }
}

export async function createServer() {
  const bridge = new UnrealBridge();
  
  // Connect to UE5 Remote Control with retries and timeout
  const connected = await bridge.tryConnect(
    CONFIG.MAX_RETRY_ATTEMPTS,
    5000, // 5 second timeout per attempt
    CONFIG.RETRY_DELAY_MS
  );
  
  if (connected) {
    metrics.connectionStatus = 'connected';
    log.info('Successfully connected to Unreal Engine');
  } else {
    log.warn('Could not connect to Unreal Engine after retries');
    log.info('Server will start anyway - connection will be retried periodically');
    log.info('Make sure Unreal Engine is running with Remote Control enabled');
    metrics.connectionStatus = 'disconnected';
    
    // Schedule automatic reconnection attempts
    const reconnectInterval = setInterval(async () => {
      if (!bridge.isConnected) {
        log.info('Attempting to reconnect to Unreal Engine...');
        const reconnected = await bridge.tryConnect(1, 5000, 0); // Single attempt
        if (reconnected) {
          log.info('Reconnected to Unreal Engine successfully');
          metrics.connectionStatus = 'connected';
        }
      }
    }, 10000); // Try every 10 seconds
  }
  
  // Start periodic health checks
  setInterval(() => {
    performHealthCheck(bridge);
  }, CONFIG.HEALTH_CHECK_INTERVAL_MS);

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
        }
      ]
    };
  });

  // Handle resource reading
  server.setRequestHandler(ReadResourceRequestSchema, async (request) => {
    const uri = request.params.uri;
    
    if (uri === 'ue://assets') {
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
      try {
        const exposed = await bridge.getExposed();
        return {
          contents: [{
            uri,
            mimeType: 'application/json',
            text: JSON.stringify(exposed, null, 2)
          }]
        };
      } catch (err) {
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
          wsPort: process.env.UE_RC_WS_PORT || 30020
        }
      };
      
      return {
        contents: [{
          uri,
          mimeType: 'application/json',
          text: JSON.stringify(health, null, 2)
        }]
      };
    }
    
    throw new Error(`Unknown resource: ${uri}`);
  });

  // Handle tool listing - switch between consolidated (10) or individual (36) tools
  server.setRequestHandler(ListToolsRequestSchema, async () => {
    log.info(`Serving ${CONFIG.USE_CONSOLIDATED_TOOLS ? 'consolidated' : 'individual'} tools`);
    return {
      tools: CONFIG.USE_CONSOLIDATED_TOOLS ? consolidatedToolDefinitions : toolDefinitions
    };
  });

  // Handle tool calls - switch between consolidated (10) or individual (36) tools
  server.setRequestHandler(CallToolRequestSchema, async (request) => {
    const { name, arguments: args } = request.params;
    const startTime = Date.now();
    
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
      bridge
    };
    
    // Use consolidated or individual handler based on configuration
    try {
      let result;
      if (CONFIG.USE_CONSOLIDATED_TOOLS) {
        result = await handleConsolidatedToolCall(name, args, tools);
      } else {
        result = await handleToolCall(name, args, tools);
      }
      trackPerformance(startTime, true);
      return result;
    } catch (error) {
      trackPerformance(startTime, false);
      log.error(`Tool execution failed: ${name}`, error);
      throw error;
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
  await server.connect(transport);
  log.info('Unreal Engine MCP Server started on stdio');
}
