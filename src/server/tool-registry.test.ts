import { beforeEach, describe, expect, it, vi } from 'vitest';
import { ListToolsRequestSchema } from '@modelcontextprotocol/sdk/types.js';
import { consolidatedToolDefinitions } from '../tools/consolidated-tool-definitions.js';
import { ToolRegistry } from './tool-registry.js';

const { mockDynamicToolManager } = vi.hoisted(() => ({
  mockDynamicToolManager: {
    getAllToolDefinitions: vi.fn(),
    getStatus: vi.fn(),
    isToolEnabled: vi.fn()
  }
}));

vi.mock('mcp-client-capabilities', () => ({
  mcpClients: {}
}));

vi.mock('../tools/dynamic-tool-manager.js', () => ({
  dynamicToolManager: mockDynamicToolManager
}));

vi.mock('../tools/consolidated-tool-handlers.js', () => ({
  handleConsolidatedToolCall: vi.fn()
}));

type RegisteredHandler = (...args: unknown[]) => Promise<unknown>;

function createRegistry(clientName = 'smoke-test') {
  const handlers = new Map<unknown, RegisteredHandler>();
  const server = {
    _clientVersion: { name: clientName },
    setRequestHandler: vi.fn((schema: unknown, handler: RegisteredHandler) => {
      handlers.set(schema, handler);
    }),
    notification: vi.fn().mockResolvedValue(undefined)
  };

  const automationBridge = {
    isConnected: vi.fn().mockReturnValue(true),
    sendAutomationRequest: vi.fn()
  };

  const registry = new ToolRegistry(
    server as never,
    {
      executeConsoleCommand: vi.fn(),
      getAutomationBridge: vi.fn().mockReturnValue(automationBridge)
    } as never,
    automationBridge as never,
    {
      debug: vi.fn(),
      info: vi.fn(),
      warn: vi.fn(),
      error: vi.fn()
    } as never,
    {
      trackPerformance: vi.fn()
    } as never,
    {} as never,
    {} as never,
    {} as never,
    vi.fn().mockResolvedValue(true)
  );

  registry.register();

  const listToolsHandler = handlers.get(ListToolsRequestSchema);
  if (!listToolsHandler) {
    throw new Error('ToolRegistry did not register a tools/list handler');
  }

  return { listToolsHandler };
}

function getPublishedTool(tools: unknown, toolName: string): Record<string, unknown> {
  if (!Array.isArray(tools)) {
    throw new Error('tools/list did not return a tools array');
  }

  const tool = tools.find((entry) => {
    return typeof entry === 'object' && entry !== null && (entry as { name?: unknown }).name === toolName;
  });

  if (!tool || typeof tool !== 'object') {
    throw new Error(`Expected tool ${toolName} to be published`);
  }

  return tool as Record<string, unknown>;
}

function getActionEnum(tool: Record<string, unknown>): string[] {
  const inputSchema = tool.inputSchema as { properties?: Record<string, unknown> } | undefined;
  const actionSchema = inputSchema?.properties?.action as { enum?: unknown } | undefined;

  return Array.isArray(actionSchema?.enum)
    ? actionSchema.enum.filter((entry): entry is string => typeof entry === 'string')
    : [];
}

describe('ToolRegistry tools/list contract', () => {
  beforeEach(() => {
    vi.clearAllMocks();
    vi.unstubAllEnvs();

    mockDynamicToolManager.getAllToolDefinitions.mockReturnValue(consolidatedToolDefinitions);
    mockDynamicToolManager.getStatus.mockReturnValue({
      totalTools: consolidatedToolDefinitions.length,
      enabledTools: consolidatedToolDefinitions.length,
      disabledTools: 0,
      categories: []
    });
    mockDynamicToolManager.isToolEnabled.mockReturnValue(true);
  });

  it('keeps outputSchema visible for representative consolidated tools', async () => {
    const { listToolsHandler } = createRegistry();
    const response = await listToolsHandler();
    const tools = (response as { tools?: unknown }).tools;

    const manageBlueprint = getPublishedTool(tools, 'manage_blueprint');
    const controlEditor = getPublishedTool(tools, 'control_editor');

    expect(manageBlueprint.outputSchema).toMatchObject({
      type: 'object',
      properties: expect.any(Object)
    });
    expect(controlEditor.outputSchema).toMatchObject({
      type: 'object',
      properties: expect.any(Object)
    });
  });

  it('keeps canonical merged authoring tools discoverable as primary public routes', async () => {
    const { listToolsHandler } = createRegistry();
    const response = await listToolsHandler();
    const tools = (response as { tools?: unknown }).tools;

    const manageBlueprint = getPublishedTool(tools, 'manage_blueprint');
    const manageAudio = getPublishedTool(tools, 'manage_audio');
    const manageEffect = getPublishedTool(tools, 'manage_effect');
    const animationPhysics = getPublishedTool(tools, 'animation_physics');

    expect(getActionEnum(manageBlueprint)).toEqual(expect.arrayContaining(['get_graph_details', 'get_pin_details']));
    expect(getActionEnum(manageAudio)).toEqual(expect.arrayContaining(['create_metasound', 'get_audio_info']));
    expect(getActionEnum(manageEffect)).toEqual(expect.arrayContaining(['create_niagara_system', 'get_niagara_info']));
    expect(getActionEnum(animationPhysics)).toEqual(expect.arrayContaining(['create_animation_blueprint', 'create_control_rig']));
  });

  it('only hides outputSchema when the named compatibility opt-out is enabled', async () => {
    vi.stubEnv('MCP_TOOLS_LIST_HIDE_OUTPUT_SCHEMA', 'true');

    const { listToolsHandler } = createRegistry();
    const response = await listToolsHandler();
    const tools = (response as { tools?: unknown }).tools;

    const manageBlueprint = getPublishedTool(tools, 'manage_blueprint');

    expect(manageBlueprint.outputSchema).toBeUndefined();
  });
});