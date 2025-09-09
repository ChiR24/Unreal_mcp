import { Server } from '@modelcontextprotocol/sdk/server/index.js';
import { StdioServerTransport } from '@modelcontextprotocol/sdk/server/stdio.js';
import { Logger } from './utils/logger.js';
import { UnrealBridge } from './unreal-bridge.js';
import { AssetResources } from './resources/assets.js';
import { ActorTools } from './tools/actors.js';
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

  const assets = new AssetResources(bridge);
  const actors = new ActorTools(bridge);

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
      const list = await assets.list('/Game', true);
      return {
        contents: [{
          uri,
          mimeType: 'application/json',
          text: JSON.stringify(list, null, 2)
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

  // Handle tool listing
  server.setRequestHandler(ListToolsRequestSchema, async () => {
    return {
      tools: [
        {
          name: 'spawn_actor',
          description: 'Spawn a new actor in the level',
          inputSchema: {
            type: 'object',
            properties: {
              classPath: { type: 'string', description: 'Blueprint/class path (e.g. /Game/Blueprints/BP_Actor)' },
              location: {
                type: 'object',
                properties: {
                  x: { type: 'number' },
                  y: { type: 'number' },
                  z: { type: 'number' }
                }
              },
              rotation: {
                type: 'object',
                properties: {
                  pitch: { type: 'number' },
                  yaw: { type: 'number' },
                  roll: { type: 'number' }
                }
              }
            },
            required: ['classPath']
          }
        },
        {
          name: 'console_command',
          description: 'Execute a console command in Unreal Engine',
          inputSchema: {
            type: 'object',
            properties: {
              command: { type: 'string', description: 'Console command to execute' }
            },
            required: ['command']
          }
        }
      ]
    };
  });

  // Handle tool calls
  server.setRequestHandler(CallToolRequestSchema, async (request) => {
    const { name, arguments: args } = request.params;
    
    if (name === 'spawn_actor') {
      try {
        const result = await actors.spawn(args as any);
        return {
          content: [{
            type: 'text',
            text: `Actor spawned: ${JSON.stringify(result)}`
          }]
        };
      } catch (err) {
        return {
          content: [{
            type: 'text',
            text: `Failed to spawn actor: ${err}`
          }],
          isError: true
        };
      }
    }
    
    if (name === 'console_command') {
      try {
        const result = await bridge.httpCall('/remote/object/call', 'PUT', {
          objectPath: '/Script/Engine.Default__KismetSystemLibrary',
          functionName: 'ExecuteConsoleCommand',
          parameters: {
            WorldContextObject: null,
            Command: args.command
          }
        });
        return {
          content: [{
            type: 'text',
            text: `Command executed: ${args.command}`
          }]
        };
      } catch (err) {
        return {
          content: [{
            type: 'text',
            text: `Failed to execute command: ${err}`
          }],
          isError: true
        };
      }
    }
    
    throw new Error(`Unknown tool: ${name}`);
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
