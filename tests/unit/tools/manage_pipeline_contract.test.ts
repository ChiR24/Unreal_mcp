import { beforeEach, describe, expect, it, vi } from 'vitest';
import { CallToolRequestSchema, ListToolsRequestSchema } from '@modelcontextprotocol/sdk/types.js';
import { consolidatedToolDefinitions } from '../../../src/tools/consolidated-tool-definitions.js';
import { ToolRegistry } from '../../../src/server/tool-registry.js';

const { mockDynamicToolManager, mockHandleConsolidatedToolCall } = vi.hoisted(() => ({
  mockDynamicToolManager: {
    getAllToolDefinitions: vi.fn(),
    getEnabledToolDefinitions: vi.fn(),
    getStatus: vi.fn(),
    isToolEnabled: vi.fn(),
    listCategories: vi.fn(),
    enableCategory: vi.fn()
  },
  mockHandleConsolidatedToolCall: vi.fn()
}));

vi.mock('mcp-client-capabilities', () => ({
  mcpClients: {}
}));

vi.mock('../../../src/tools/dynamic-tool-manager.js', () => ({
  dynamicToolManager: mockDynamicToolManager
}));

vi.mock('../../../src/tools/consolidated-tool-handlers.js', () => ({
  handleConsolidatedToolCall: mockHandleConsolidatedToolCall
}));

type RegisteredHandler = (...args: unknown[]) => Promise<unknown>;

type CatalogSubAction = {
  name?: string;
};

type CatalogTool = {
  toolName?: string;
  subActions?: CatalogSubAction[];
};

function getToolActionEnum(toolName: string): string[] {
  const tool = consolidatedToolDefinitions.find((definition) => definition.name === toolName);
  if (!tool) {
    throw new Error(`Missing consolidated tool definition for ${toolName}`);
  }

  const actionSchema = tool.inputSchema.properties?.action as { enum?: unknown } | undefined;
  if (!Array.isArray(actionSchema?.enum)) {
    throw new Error(`Tool ${toolName} is missing an action enum in the consolidated definitions`);
  }

  return actionSchema.enum.filter((entry): entry is string => typeof entry === 'string');
}

function selectExpectedRuntimeActions(toolName: string, predicate: (action: string) => boolean): string[] {
  const actions = getToolActionEnum(toolName).filter(predicate);
  if (actions.length === 0) {
    throw new Error(`No expected runtime actions selected for ${toolName}`);
  }

  return actions;
}

const requiredRuntimeActions = {
  manage_pipeline: selectExpectedRuntimeActions(
    'manage_pipeline',
    (action) => action === 'list_categories' || action === 'get_status'
  ),
  manage_ui: selectExpectedRuntimeActions(
    'manage_ui',
    (action) => action === 'list_visible_windows' || action === 'resolve_ui_target'
  ),
  control_editor: [
    'screenshot',
    'simulate_input',
    'fit_blueprint_graph',
    'set_blueprint_graph_view',
    'jump_to_blueprint_node',
    'capture_blueprint_graph_review',
    'set_widget_blueprint_mode',
    'fit_widget_designer',
    'set_widget_designer_view',
    'select_widget_in_designer',
    'select_widgets_in_designer_rect',
    'focus_editor_surface'
  ],
  manage_blueprint: selectExpectedRuntimeActions(
    'manage_blueprint',
    (action) => action === 'get_graph_details' || action === 'get_graph_review_summary' || action === 'get_node_details_batch' || action === 'get_pin_details'
  ),
  manage_widget_authoring: selectExpectedRuntimeActions(
    'manage_widget_authoring',
    (action) => action === 'get_widget_tree' || action === 'get_widget_designer_state' || action === 'create_property_binding' || action.startsWith('bind_')
  )
} satisfies Record<string, string[]>;

function findCatalogTool(tools: unknown, toolName: string): CatalogTool {
  if (!Array.isArray(tools)) {
    throw new Error('Catalog response did not include a tools array');
  }

  const tool = tools.find((entry) => {
    return typeof entry === 'object' && entry !== null && (entry as CatalogTool).toolName === toolName;
  }) as CatalogTool | undefined;

  if (!tool) {
    throw new Error(`Catalog response did not include tool ${toolName}`);
  }

  return tool;
}

function expectCatalogToolActions(tool: CatalogTool, expectedActions: string[]): void {
  expect(tool.subActions).toEqual(
    expect.arrayContaining(expectedActions.map((action) => expect.objectContaining({ name: action })))
  );
}

function getInteractionModel(action: string): string {
  if (action === 'list_categories' || action === 'get_status') {
    return 'catalog_discovery';
  }
  if (action === 'screenshot' || action === 'capture_blueprint_graph_review') {
    return 'semantic_capture';
  }
  if (action === 'simulate_input') {
    return 'low_level_input';
  }
  if (action === 'focus_editor_surface') {
    return 'semantic_focus';
  }
  if (action === 'set_widget_blueprint_mode') {
    return 'semantic_mode';
  }
  if (
    action === 'fit_blueprint_graph' ||
    action === 'set_blueprint_graph_view' ||
    action === 'fit_widget_designer' ||
    action === 'set_widget_designer_view'
  ) {
    return 'semantic_view';
  }
  if (action === 'jump_to_blueprint_node' || action === 'select_widget_in_designer' || action === 'select_widgets_in_designer_rect') {
    return 'semantic_selection';
  }
  if (
    action === 'create_property_binding' ||
    action === 'bind_text' ||
    action === 'bind_visibility' ||
    action === 'bind_color' ||
    action === 'bind_enabled' ||
    action === 'bind_on_clicked' ||
    action === 'bind_on_hovered' ||
    action === 'bind_on_value_changed'
  ) {
    return 'semantic_write';
  }

  return 'semantic_read';
}

function getLimitationNote(action: string): string {
  if (action === 'resolve_ui_target') {
    return 'Window titles and tab ids remain live editor state and can still require reopen or re-resolution.';
  }
  if (action === 'focus_editor_surface') {
    return 'Requires a live Blueprint or Widget Blueprint editor surface before keyboard or text input.';
  }
  if (action === 'capture_blueprint_graph_review') {
    return 'Captures the resolved Blueprint editor window after semantic graph activation and can frame a bounded dense-review neighborhood around a matched node; it does not fall back to a viewport screenshot.';
  }
  if (action === 'get_node_details_batch') {
    return 'Use graph details first to inventory node ids before paging through large graphs.';
  }
  if (action === 'get_graph_review_summary') {
    return 'Use reviewTargets[].nodeId for one bounded focused follow-up before raw node-detail batches on large Blueprint graphs.';
  }
  if (action === 'create_property_binding') {
    return 'Binding functions must stay signature-compatible with the target property.';
  }
  if (action === 'bind_on_clicked') {
    return 'Component-bound event authoring stays on engine-generated handlers and explicit ensureVariable promotion.';
  }
  if (action === 'bind_on_hovered') {
    return 'OnUnhovered follow-through remains manual even when the OnHovered event is authored.';
  }
  if (action === 'bind_on_value_changed') {
    return 'Requires a widget that exposes a supported value-changed delegate.';
  }

  return `${action} is published through the bridge-owned runtime capability matrix.`;
}

function buildCatalogTool(toolName: string, category: string, summary: string, actions: string[]): CatalogTool & {
  category: string;
  summary: string;
  public: boolean;
  subActionCount: number;
} {
  const requiresLiveEditor = toolName === 'manage_pipeline' || toolName === 'manage_ui' || toolName === 'control_editor';
  const requiresAssetEditor = toolName === 'control_editor';

  return {
    toolName,
    category,
    summary,
    public: true,
    subActions: actions.map((action) => ({
      name: action,
      summary: `${action} is published through the bridge-owned runtime capability matrix.`,
      editorOnly: true,
      requiresLiveEditor,
      requiresAssetEditor,
      interactionModel: getInteractionModel(action),
      limitationNote: getLimitationNote(action)
    })),
    subActionCount: actions.length
  };
}

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
  const callToolHandler = handlers.get(CallToolRequestSchema);

  if (!listToolsHandler || !callToolHandler) {
    throw new Error('ToolRegistry did not register expected MCP handlers');
  }

  return {
    server,
    automationBridge,
    listToolsHandler,
    callToolHandler
  };
}

async function invokeTool(
  callToolHandler: RegisteredHandler,
  name: string,
  args: Record<string, unknown>
): Promise<Record<string, unknown>> {
  const response = await callToolHandler({
    params: {
      name,
      arguments: args
    }
  });

  const text = (((response as { content?: Array<{ text?: string }> }).content ?? [])[0]?.text) ?? '{}';
  return JSON.parse(text) as Record<string, unknown>;
}

describe('Manage Pipeline Contract', () => {
  beforeEach(() => {
    vi.clearAllMocks();

    mockDynamicToolManager.getAllToolDefinitions.mockReturnValue(consolidatedToolDefinitions);
    mockDynamicToolManager.getEnabledToolDefinitions.mockReturnValue(consolidatedToolDefinitions);
    mockDynamicToolManager.getStatus.mockReturnValue({
      totalTools: consolidatedToolDefinitions.length,
      enabledTools: consolidatedToolDefinitions.length,
      disabledTools: 0,
      categories: []
    });
    mockDynamicToolManager.isToolEnabled.mockReturnValue(true);
    mockDynamicToolManager.listCategories.mockReturnValue([
      { name: 'core', enabled: true, toolCount: 8, enabledCount: 8 },
      { name: 'world', enabled: true, toolCount: 6, enabledCount: 6 }
    ]);
    mockDynamicToolManager.enableCategory.mockReturnValue({
      success: true,
      enabled: ['manage_level', 'manage_navigation'],
      notFound: false
    });
  });
  function getManagePipelineDefinition() {
    const tool = consolidatedToolDefinitions.find((definition) => definition.name === 'manage_pipeline');
    if (!tool) {
      throw new Error('manage_pipeline tool definition is missing');
    }

    return tool;
  }

  it('keeps manage_pipeline visible to ordinary tools/list clients', async () => {
    const { listToolsHandler } = createRegistry('smoke-test');

    const response = await listToolsHandler();
    const tools = (response as { tools?: Array<{ name?: string }> }).tools ?? [];
    const toolNames = tools.map((tool) => tool.name);

    expect(toolNames).toContain('manage_pipeline');

    const managePipelineDefinition = getManagePipelineDefinition();
    expect(managePipelineDefinition.description).toContain('runtime capability matrix');
  });

  it('publishes capability-matrix fields in the manage_pipeline output schema', () => {
    const managePipelineDefinition = getManagePipelineDefinition();
    const outputSchema = managePipelineDefinition.outputSchema as {
      properties?: {
        tools?: {
          items?: {
            properties?: {
              subActions?: {
                items?: {
                  properties?: Record<string, unknown>;
                };
              };
            };
          };
        };
      };
    };

    expect(outputSchema.properties?.tools?.items?.properties?.subActions?.items?.properties).toMatchObject({
      name: expect.any(Object),
      summary: expect.any(Object),
      editorOnly: expect.any(Object),
      requiresLiveEditor: expect.any(Object),
      requiresAssetEditor: expect.any(Object),
      interactionModel: expect.any(Object),
      limitationNote: expect.any(Object)
    });
  });

  it('reserves a semantic-capture runtime slot for Blueprint graph review capture', () => {
    expect(getToolActionEnum('control_editor')).toContain('capture_blueprint_graph_review');

    const catalogTool = buildCatalogTool(
      'control_editor',
      'Core Editor',
      'Drive editor viewport, screenshots, PIE controls, semantic graph navigation, and simulated input.',
      requiredRuntimeActions.control_editor
    );
    const reviewCaptureAction = catalogTool.subActions.find((entry) => entry.name === 'capture_blueprint_graph_review');

    expect(reviewCaptureAction).toMatchObject({
      name: 'capture_blueprint_graph_review',
      interactionModel: 'semantic_capture',
      limitationNote: expect.stringMatching(/bounded dense-review neighborhood/i)
    });
  });

  it('reserves a semantic-read runtime slot for bounded graph review summaries', () => {
    expect(getToolActionEnum('manage_blueprint')).toContain('get_graph_review_summary');

    const catalogTool = buildCatalogTool(
      'manage_blueprint',
      'Blueprint Authoring',
      'Create and modify Blueprint assets, classes, components, and public inspection metadata.',
      ['get_graph_details', 'get_node_details_batch', 'get_pin_details', 'get_graph_review_summary']
    );
    const summaryAction = catalogTool.subActions.find((entry) => entry.name === 'get_graph_review_summary');

    expect(summaryAction).toMatchObject({
      name: 'get_graph_review_summary',
      interactionModel: 'semantic_read',
      limitationNote: expect.stringMatching(/reviewTargets.*nodeId|nodeId.*reviewTargets/i)
    });
  });

  it('returns catalog-backed results for manage_pipeline discovery and status', async () => {
    const { automationBridge, callToolHandler } = createRegistry('smoke-test');

    const catalogTools = [
      buildCatalogTool(
        'manage_pipeline',
        'Diagnostics',
        'Report bridge tool exposure and launch build and test-oriented editor automation.',
        requiredRuntimeActions.manage_pipeline
      ),
      buildCatalogTool(
        'manage_blueprint',
        'Blueprint Authoring',
        'Create and modify Blueprint assets, classes, components, and public inspection metadata.',
        requiredRuntimeActions.manage_blueprint
      ),
      buildCatalogTool(
        'control_editor',
        'Core Editor',
        'Drive editor viewport, screenshots, PIE controls, semantic graph navigation, and simulated input.',
        requiredRuntimeActions.control_editor
      ),
      buildCatalogTool(
        'manage_ui',
        'UI Automation',
        'Discover UI targets, visible windows, commands, menus, and editor utility widgets.',
        requiredRuntimeActions.manage_ui
      ),
      buildCatalogTool(
        'manage_widget_authoring',
        'UI Automation',
        'Author widget assets and widget-related editor metadata.',
        requiredRuntimeActions.manage_widget_authoring
      )
    ];
    const catalogToolNames = catalogTools.map((tool) => tool.toolName);
    const categoryGroupNames = [...new Set(catalogTools.map((tool) => tool.category))];
    const actionCount = catalogTools.reduce((count, tool) => count + tool.subActionCount, 0);

    automationBridge.sendAutomationRequest
      .mockResolvedValueOnce({
        success: true,
        result: {
          categories: catalogToolNames,
          tools: catalogTools,
          categoryGroups: categoryGroupNames,
          categoryGroupNames,
          count: catalogToolNames.length,
          groupCount: categoryGroupNames.length,
          actionCount,
          catalogSource: 'McpAutomationBridgeToolCatalog'
        }
      })
      .mockResolvedValueOnce({
        success: true,
        result: {
          connected: true,
          bridgeType: 'Native C++ WebSocket',
          totalActions: actionCount,
          toolCategories: catalogToolNames.length,
          categoryGroups: categoryGroupNames.length,
          categoryGroupNames,
          categories: catalogToolNames,
          tools: catalogTools,
          catalogSource: 'McpAutomationBridgeToolCatalog'
        }
      });

    const categoriesResult = await invokeTool(callToolHandler, 'manage_pipeline', { action: 'list_categories' });
    const statusResult = await invokeTool(callToolHandler, 'manage_pipeline', { action: 'get_status' });

    const categoryBlueprintTool = findCatalogTool(categoriesResult.tools, 'manage_blueprint');
    const categoryUiTool = findCatalogTool(categoriesResult.tools, 'manage_ui');
    const categoryEditorTool = findCatalogTool(categoriesResult.tools, 'control_editor');
    const categoryWidgetTool = findCatalogTool(categoriesResult.tools, 'manage_widget_authoring');
    const statusBlueprintTool = findCatalogTool(statusResult.tools, 'manage_blueprint');
    const statusUiTool = findCatalogTool(statusResult.tools, 'manage_ui');
    const statusEditorTool = findCatalogTool(statusResult.tools, 'control_editor');
    const statusWidgetTool = findCatalogTool(statusResult.tools, 'manage_widget_authoring');

    expect(categoriesResult).toMatchObject({
      success: true,
      catalogSource: 'McpAutomationBridgeToolCatalog',
      count: catalogToolNames.length,
      groupCount: categoryGroupNames.length,
      actionCount
    });
    expect(categoriesResult).not.toHaveProperty('available');
    expect(categoriesResult).not.toHaveProperty('categoryDetails');

    expect(statusResult).toMatchObject({
      success: true,
      connected: true,
      bridgeType: 'Native C++ WebSocket',
      toolCategories: catalogToolNames.length,
      totalActions: actionCount,
      catalogSource: 'McpAutomationBridgeToolCatalog'
    });
    expect(statusResult).not.toHaveProperty('enabledCount');
    expect(statusResult).not.toHaveProperty('filteredCount');

    expectCatalogToolActions(categoryUiTool, requiredRuntimeActions.manage_ui);
    expectCatalogToolActions(categoryEditorTool, requiredRuntimeActions.control_editor);
    expectCatalogToolActions(categoryBlueprintTool, requiredRuntimeActions.manage_blueprint);
    expectCatalogToolActions(categoryWidgetTool, requiredRuntimeActions.manage_widget_authoring);
    expectCatalogToolActions(statusUiTool, requiredRuntimeActions.manage_ui);
    expectCatalogToolActions(statusEditorTool, requiredRuntimeActions.control_editor);
    expectCatalogToolActions(statusBlueprintTool, requiredRuntimeActions.manage_blueprint);
    expectCatalogToolActions(statusWidgetTool, requiredRuntimeActions.manage_widget_authoring);

    expect(automationBridge.sendAutomationRequest).toHaveBeenNthCalledWith(
      1,
      'manage_pipeline',
      expect.objectContaining({
        action: 'manage_pipeline',
        subAction: 'list_categories'
      }),
      expect.any(Object)
    );
    expect(automationBridge.sendAutomationRequest).toHaveBeenNthCalledWith(
      2,
      'manage_pipeline',
      expect.objectContaining({
        action: 'manage_pipeline',
        subAction: 'get_status'
      }),
      expect.any(Object)
    );
  });

  it('delegates manage_pipeline run_ubt through the consolidated handler', async () => {
    const { automationBridge, callToolHandler } = createRegistry('smoke-test');

    mockHandleConsolidatedToolCall.mockResolvedValueOnce({
      success: true,
      delegated: true,
      action: 'run_ubt'
    });

    const response = await callToolHandler({
      params: {
        name: 'manage_pipeline',
        arguments: {
          action: 'run_ubt',
          target: 'UE_AutomationMCPEditor',
          platform: 'Win64',
          configuration: 'Development'
        }
      }
    });

    const responseObj = response as {
      success?: boolean;
      isError?: boolean;
      structuredContent?: Record<string, unknown>;
      content?: Array<{ text?: string }>;
    };

    expect(responseObj.success).toBe(true);
    expect(responseObj.isError).not.toBe(true);
    expect(responseObj.structuredContent).toMatchObject({
      success: true,
      delegated: true,
      action: 'run_ubt'
    });
    expect(responseObj.content?.[0]?.text).toContain('success: true');
    expect(mockHandleConsolidatedToolCall).toHaveBeenCalledWith(
      'manage_pipeline',
      expect.objectContaining({
        action: 'run_ubt',
        target: 'UE_AutomationMCPEditor',
        platform: 'Win64',
        configuration: 'Development'
      }),
      expect.any(Object)
    );
    expect(automationBridge.sendAutomationRequest).not.toHaveBeenCalled();
  });

  it('keeps category-management semantics on manage_tools', async () => {
    const { server, automationBridge, callToolHandler } = createRegistry('smoke-test');

    const result = await invokeTool(callToolHandler, 'manage_tools', {
      action: 'enable_category',
      category: 'world'
    });

    expect(result).toMatchObject({
      success: true,
      category: 'world',
      enabled: ['manage_level', 'manage_navigation']
    });
    expect(automationBridge.sendAutomationRequest).not.toHaveBeenCalled();
    expect(server.notification).toHaveBeenCalledWith({
      method: 'notifications/tools/list_changed',
      params: {}
    });
  });
});