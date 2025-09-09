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

// Configuration: Set to true to use consolidated tools (10 tools), false for individual tools (36 tools)
const USE_CONSOLIDATED_TOOLS = true; // Change this to switch between modes

export async function createServer() {
  const bridge = new UnrealBridge();
  
  // Connect to UE5 Remote Control
  try {
    await bridge.connect();
  } catch (err) {
    log.error('Failed to connect to Unreal Engine:', err);
    log.info('Make sure Unreal Engine is running with Remote Control enabled');
    // Continue anyway - connection can be retried
  }

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
      name: 'unreal-engine-5.6',
      version: '0.1.0'
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
    
    throw new Error(`Unknown resource: ${uri}`);
  });

  // Handle tool listing - switch between consolidated (10) or individual (36) tools
  server.setRequestHandler(ListToolsRequestSchema, async () => {
    return {
      tools: USE_CONSOLIDATED_TOOLS ? consolidatedToolDefinitions : toolDefinitions
    };
  });

  // Handle tool calls - switch between consolidated (10) or individual (36) tools
  server.setRequestHandler(CallToolRequestSchema, async (request) => {
    const { name, arguments: args } = request.params;
    
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
    if (USE_CONSOLIDATED_TOOLS) {
      return await handleConsolidatedToolCall(name, args, tools);
    } else {
      return await handleToolCall(name, args, tools);
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
