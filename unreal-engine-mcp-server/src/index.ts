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

  // Resources
  const assetResources = new AssetResources(bridge);
  const actorResources = new ActorResources(bridge);
  const levelResources = new LevelResources(bridge);
  
  // Tools
  const actorTools = new ActorTools(bridge);
  const assetTools = new AssetTools(bridge);
  const editorTools = new EditorTools(bridge);
  const materialTools = new MaterialTools(bridge);

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
        },
        {
          name: 'play_in_editor',
          description: 'Start Play In Editor (PIE) mode',
          inputSchema: {
            type: 'object',
            properties: {}
          }
        },
        {
          name: 'stop_play_in_editor',
          description: 'Stop Play In Editor (PIE) mode',
          inputSchema: {
            type: 'object',
            properties: {}
          }
        },
        {
          name: 'set_camera',
          description: 'Set viewport camera position and rotation',
          inputSchema: {
            type: 'object',
            properties: {
              location: {
                type: 'object',
                properties: {
                  x: { type: 'number' },
                  y: { type: 'number' },
                  z: { type: 'number' }
                },
                required: ['x', 'y', 'z']
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
            required: ['location']
          }
        },
        {
          name: 'build_lighting',
          description: 'Build lighting for the current level',
          inputSchema: {
            type: 'object',
            properties: {}
          }
        },
        {
          name: 'save_level',
          description: 'Save the current level',
          inputSchema: {
            type: 'object',
            properties: {}
          }
        },
        {
          name: 'import_asset',
          description: 'Import an asset from file system',
          inputSchema: {
            type: 'object',
            properties: {
              sourcePath: { type: 'string', description: 'File system path to import from' },
              destinationPath: { type: 'string', description: 'Project path to import to (e.g. /Game/Assets)' }
            },
            required: ['sourcePath', 'destinationPath']
          }
        },
        {
          name: 'create_material',
          description: 'Create a new material asset',
          inputSchema: {
            type: 'object',
            properties: {
              name: { type: 'string', description: 'Material name' },
              path: { type: 'string', description: 'Path to create material (e.g. /Game/Materials)' }
            },
            required: ['name', 'path']
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
        const result = await actorTools.spawn(args as any);
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
        if (!args || !args.command) {
          throw new Error('Command argument is required');
        }
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
    
    if (name === 'play_in_editor') {
      const result = await editorTools.playInEditor();
      return {
        content: [{
          type: 'text',
          text: result.message || 'PIE started'
        }],
        isError: !result.success
      };
    }
    
    if (name === 'stop_play_in_editor') {
      const result = await editorTools.stopPlayInEditor();
      return {
        content: [{
          type: 'text',
          text: result.message || 'PIE stopped'
        }],
        isError: !result.success
      };
    }
    
    if (name === 'set_camera') {
      if (!args || !args.location) {
        throw new Error('Location is required');
      }
      const result = await editorTools.setViewportCamera(
        args.location as { x: number; y: number; z: number },
        args.rotation as { pitch: number; yaw: number; roll: number } | undefined
      );
      return {
        content: [{
          type: 'text',
          text: result.message || 'Camera set'
        }],
        isError: !result.success
      };
    }
    
    if (name === 'build_lighting') {
      const result = await editorTools.buildLighting();
      return {
        content: [{
          type: 'text',
          text: result.message || 'Lighting built'
        }],
        isError: !result.success
      };
    }
    
    if (name === 'save_level') {
      const result = await levelResources.saveCurrentLevel();
      return {
        content: [{
          type: 'text',
          text: 'Level saved'
        }]
      };
    }
    
    if (name === 'import_asset') {
      if (!args || !args.sourcePath || !args.destinationPath) {
        throw new Error('sourcePath and destinationPath are required');
      }
      const result = await assetTools.importAsset(
        args.sourcePath as string,
        args.destinationPath as string
      );
      return {
        content: [{
          type: 'text',
          text: result.error || `Asset imported to ${args.destinationPath}`
        }],
        isError: !!result.error
      };
    }
    
    if (name === 'create_material') {
      if (!args || !args.name || !args.path) {
        throw new Error('name and path are required');
      }
      const result = await materialTools.createMaterial(
        args.name as string,
        args.path as string
      );
      return {
        content: [{
          type: 'text',
          text: result.success ? `Material created: ${result.path}` : result.error
        }],
        isError: !result.success
      };
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
