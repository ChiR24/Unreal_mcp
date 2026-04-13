import { beforeEach, describe, expect, it, vi } from 'vitest';
import type { ITools } from '../../../src/types/tool-interfaces.js';

vi.mock('../../../src/tools/handlers/common-handlers.js', async () => {
  const actual = await vi.importActual<typeof import('../../../src/tools/handlers/common-handlers.js')>(
    '../../../src/tools/handlers/common-handlers.js'
  );

  return {
    ...actual,
    executeAutomationRequest: vi.fn()
  };
});

import { consolidatedToolDefinitions } from '../../../src/tools/consolidated-tool-definitions.js';
import { handleEditorTools } from '../../../src/tools/handlers/editor-handlers.js';
import { executeAutomationRequest } from '../../../src/tools/handlers/common-handlers.js';

describe('Control Editor Navigation Contract', () => {
  const mockExecuteAutomationRequest = vi.mocked(executeAutomationRequest);
  let mockTools: ITools;

  beforeEach(() => {
    vi.clearAllMocks();
    mockTools = {
      automationBridge: {
        isConnected: vi.fn().mockReturnValue(true),
        sendAutomationRequest: vi.fn()
      }
    } as unknown as ITools;
  });

  it('publishes explicit Blueprint graph navigation actions in the public schema', () => {
    const editorTool = consolidatedToolDefinitions.find((tool) => tool.name === 'control_editor');

    expect(editorTool).toBeDefined();

    const inputSchema = editorTool?.inputSchema as {
      properties?: Record<string, unknown>;
    };
    const inputProperties = inputSchema.properties ?? {};
    const outputProperties = ((editorTool?.outputSchema as { properties?: Record<string, unknown> }).properties) ?? {};
    const actionSchema = inputProperties.action as { enum?: string[] } | undefined;

    expect(actionSchema?.enum).toContain('fit_blueprint_graph');
    expect(actionSchema?.enum).toContain('set_blueprint_graph_view');
    expect(actionSchema?.enum).toContain('jump_to_blueprint_node');
    expect(actionSchema?.enum).toContain('capture_blueprint_graph_review');
    expect(inputProperties.graphName).toBeDefined();
    expect(inputProperties.scope).toBeDefined();
    expect(inputProperties.viewLocation).toBeDefined();
    expect(inputProperties.delta).toBeDefined();
    expect(inputProperties.nodeGuid).toBeDefined();
    expect(inputProperties.nodeName).toBeDefined();
    expect(inputProperties.nodeTitle).toBeDefined();
    expect(inputProperties.filename).toBeDefined();
    expect(outputProperties.framingSource).toBeDefined();
    expect(outputProperties.framedNodeCount).toBeDefined();
    expect(outputProperties.truncatedNeighborhood).toBeDefined();
  });

  it('publishes explicit Widget Blueprint Designer navigation actions in the public schema', () => {
    const editorTool = consolidatedToolDefinitions.find((tool) => tool.name === 'control_editor');

    expect(editorTool).toBeDefined();

    const inputSchema = editorTool?.inputSchema as {
      properties?: Record<string, unknown>;
      outputSchema?: Record<string, unknown>;
    };
    const inputProperties = inputSchema.properties ?? {};
    const actionSchema = inputProperties.action as { enum?: string[] } | undefined;
    const outputProperties = ((editorTool?.outputSchema as { properties?: Record<string, unknown> }).properties) ?? {};

    expect(actionSchema?.enum).toContain('set_widget_blueprint_mode');
    expect(actionSchema?.enum).toContain('fit_widget_designer');
    expect(actionSchema?.enum).toContain('select_widget_in_designer');
    expect(actionSchema?.enum).toContain('select_widgets_in_designer_rect');
    expect(actionSchema?.enum).toContain('set_widget_designer_view');
    expect(actionSchema?.enum).toContain('focus_editor_surface');
    expect(inputProperties.surface).toBeDefined();
    expect(inputProperties.viewLocation).toBeDefined();
    expect(inputProperties.delta).toBeDefined();
    expect(inputProperties.preserveZoom).toBeDefined();
    expect(inputProperties.widgetName).toBeDefined();
    expect(inputProperties.widgetPath).toBeDefined();
    expect(inputProperties.widgetObjectPath).toBeDefined();
    expect(inputProperties.rect).toBeDefined();
    expect(inputProperties.appendOrToggle).toBeDefined();
    expect(outputProperties.focusApplied).toBeDefined();
    expect(outputProperties.focusTargetSurface).toBeDefined();
    expect(outputProperties.focusFailureReason).toBeDefined();
    expect(outputProperties.currentMode).toBeDefined();
    expect(outputProperties.queuedDesignerAction).toBeDefined();
    expect(outputProperties.resolvedTargetSource).toBeDefined();
    expect(outputProperties.designerActionDisposition).toBeDefined();
    expect(outputProperties.matchedWidgetPaths).toBeDefined();
    expect(outputProperties.matchedWidgetCount).toBeDefined();
    expect(outputProperties.appendOrToggle).toBeDefined();
    expect(outputProperties.targetStillSelected).toBeDefined();
    expect(outputProperties.selectedWidgetCount).toBeDefined();
  });

  it('forwards graph review, graph navigation, and designer navigation payload fields without stripping semantic targeting data', async () => {
    mockExecuteAutomationRequest.mockResolvedValue({ success: true });

    await handleEditorTools(
      'fit_blueprint_graph',
      {
        assetPath: '/Game/UI/WBP_StatusPanel',
        graphName: 'EventGraph',
        scope: 'selection',
        tabId: 'Document',
        windowTitle: 'WBP_StatusPanel'
      },
      mockTools
    );

    await handleEditorTools(
      'set_blueprint_graph_view',
      {
        assetPath: '/Game/UI/WBP_StatusPanel',
        graphName: 'EventGraph',
        viewLocation: { x: 120, y: 240 },
        delta: { x: 20, y: -10 },
        zoomAmount: 0.9,
        tabId: 'Document',
        windowTitle: 'WBP_StatusPanel'
      },
      mockTools
    );

    await handleEditorTools(
      'jump_to_blueprint_node',
      {
        assetPath: '/Game/UI/WBP_StatusPanel',
        graphName: 'EventGraph',
        nodeGuid: 'NODE_GUID',
        nodeName: 'EventBeginPlay',
        nodeTitle: 'Event BeginPlay',
        tabId: 'Document',
        windowTitle: 'WBP_StatusPanel'
      },
      mockTools
    );

    await handleEditorTools(
      'capture_blueprint_graph_review',
      {
        assetPath: '/Game/UI/WBP_StatusPanel',
        graphName: 'EventGraph',
        nodeGuid: 'NODE_GUID',
        nodeName: 'EventBeginPlay',
        nodeTitle: 'Event BeginPlay',
        scope: 'neighborhood',
        filename: 'graph-review-capture.png',
        tabId: 'Document',
        windowTitle: 'WBP_StatusPanel'
      },
      mockTools
    );

    await handleEditorTools(
      'set_widget_blueprint_mode',
      {
        assetPath: '/Game/UI/WBP_StatusPanel',
        mode: 'designer',
        tabId: 'SlatePreview',
        windowTitle: 'WBP_StatusPanel'
      },
      mockTools
    );

    await handleEditorTools(
      'fit_widget_designer',
      {
        assetPath: '/Game/UI/WBP_StatusPanel',
        tabId: 'SlatePreview',
        windowTitle: 'WBP_StatusPanel'
      },
      mockTools
    );

    await handleEditorTools(
      'select_widget_in_designer',
      {
        assetPath: '/Game/UI/WBP_StatusPanel',
        mode: 'designer',
        widgetName: 'CanvasRoot',
        appendOrToggle: true,
        tabId: 'SlatePreview',
        windowTitle: 'WBP_StatusPanel'
      },
      mockTools
    );

    await handleEditorTools(
      'set_widget_designer_view',
      {
        assetPath: '/Game/UI/WBP_StatusPanel',
        viewLocation: { x: 640, y: 320 },
        delta: { x: -120, y: 40 },
        preserveZoom: true,
        tabId: 'SlatePreview',
        windowTitle: 'WBP_StatusPanel'
      },
      mockTools
    );

    await handleEditorTools(
      'focus_editor_surface',
      {
        surface: 'widget_designer',
        assetPath: '/Game/UI/WBP_StatusPanel',
        tabId: 'SlatePreview',
        windowTitle: 'WBP_StatusPanel'
      },
      mockTools
    );

    expect(mockExecuteAutomationRequest).toHaveBeenNthCalledWith(
      1,
      mockTools,
      'control_editor',
      expect.objectContaining({
        action: 'fit_blueprint_graph',
        assetPath: '/Game/UI/WBP_StatusPanel',
        graphName: 'EventGraph',
        scope: 'selection',
        tabId: 'Document',
        windowTitle: 'WBP_StatusPanel'
      })
    );

    expect(mockExecuteAutomationRequest).toHaveBeenNthCalledWith(
      2,
      mockTools,
      'control_editor',
      expect.objectContaining({
        action: 'set_blueprint_graph_view',
        assetPath: '/Game/UI/WBP_StatusPanel',
        graphName: 'EventGraph',
        viewLocation: { x: 120, y: 240 },
        delta: { x: 20, y: -10 },
        zoomAmount: 0.9,
        tabId: 'Document',
        windowTitle: 'WBP_StatusPanel'
      })
    );

    expect(mockExecuteAutomationRequest).toHaveBeenNthCalledWith(
      3,
      mockTools,
      'control_editor',
      expect.objectContaining({
        action: 'jump_to_blueprint_node',
        assetPath: '/Game/UI/WBP_StatusPanel',
        graphName: 'EventGraph',
        nodeGuid: 'NODE_GUID',
        nodeName: 'EventBeginPlay',
        nodeTitle: 'Event BeginPlay',
        tabId: 'Document',
        windowTitle: 'WBP_StatusPanel'
      })
    );

    expect(mockExecuteAutomationRequest).toHaveBeenNthCalledWith(
      4,
      mockTools,
      'control_editor',
      expect.objectContaining({
        action: 'capture_blueprint_graph_review',
        assetPath: '/Game/UI/WBP_StatusPanel',
        graphName: 'EventGraph',
        nodeGuid: 'NODE_GUID',
        nodeName: 'EventBeginPlay',
        nodeTitle: 'Event BeginPlay',
        scope: 'neighborhood',
        filename: 'graph-review-capture.png',
        tabId: 'Document',
        windowTitle: 'WBP_StatusPanel'
      })
    );

    expect(mockExecuteAutomationRequest).toHaveBeenNthCalledWith(
      5,
      mockTools,
      'control_editor',
      expect.objectContaining({
        action: 'set_widget_blueprint_mode',
        assetPath: '/Game/UI/WBP_StatusPanel',
        mode: 'designer',
        tabId: 'SlatePreview',
        windowTitle: 'WBP_StatusPanel'
      })
    );

    expect(mockExecuteAutomationRequest).toHaveBeenNthCalledWith(
      6,
      mockTools,
      'control_editor',
      expect.objectContaining({
        action: 'fit_widget_designer',
        assetPath: '/Game/UI/WBP_StatusPanel',
        tabId: 'SlatePreview',
        windowTitle: 'WBP_StatusPanel'
      })
    );

    expect(mockExecuteAutomationRequest).toHaveBeenNthCalledWith(
      7,
      mockTools,
      'control_editor',
      expect.objectContaining({
        action: 'select_widget_in_designer',
        assetPath: '/Game/UI/WBP_StatusPanel',
        mode: 'designer',
        widgetName: 'CanvasRoot',
        appendOrToggle: true,
        tabId: 'SlatePreview',
        windowTitle: 'WBP_StatusPanel'
      })
    );

    expect(mockExecuteAutomationRequest).toHaveBeenNthCalledWith(
      8,
      mockTools,
      'control_editor',
      expect.objectContaining({
        action: 'set_widget_designer_view',
        assetPath: '/Game/UI/WBP_StatusPanel',
        viewLocation: { x: 640, y: 320 },
        delta: { x: -120, y: 40 },
        preserveZoom: true,
        tabId: 'SlatePreview',
        windowTitle: 'WBP_StatusPanel'
      })
    );

    expect(mockExecuteAutomationRequest).toHaveBeenNthCalledWith(
      9,
      mockTools,
      'control_editor',
      expect.objectContaining({
        action: 'focus_editor_surface',
        surface: 'widget_designer',
        assetPath: '/Game/UI/WBP_StatusPanel',
        tabId: 'SlatePreview',
        windowTitle: 'WBP_StatusPanel'
      })
    );
  });

  it('preserves semantic navigation diagnostics from the automation bridge response', async () => {
    mockExecuteAutomationRequest.mockResolvedValue({
      success: true,
      currentMode: 'Designer',
      resolvedTargetSource: 'designer_tab',
      queuedDesignerAction: true,
      appendOrToggle: true,
      targetStillSelected: true,
      selectedWidgetCount: 2,
      graphName: 'EventGraph',
      widgetName: 'CanvasRoot'
    });

    const result = await handleEditorTools(
      'select_widget_in_designer',
      {
        assetPath: '/Game/UI/WBP_StatusPanel',
        widgetName: 'CanvasRoot',
        appendOrToggle: true
      },
      mockTools
    );

    expect(result).toMatchObject({
      success: true,
      currentMode: 'Designer',
      resolvedTargetSource: 'designer_tab',
      queuedDesignerAction: true,
      appendOrToggle: true,
      targetStillSelected: true,
      selectedWidgetCount: 2,
      graphName: 'EventGraph',
      widgetName: 'CanvasRoot'
    });
  });

  it('forwards semantic rectangle selection payloads without stripping rect or appendOrToggle', async () => {
    mockExecuteAutomationRequest.mockResolvedValue({ success: true, matchedWidgetCount: 2 });

    await handleEditorTools(
      'select_widgets_in_designer_rect',
      {
        assetPath: '/Game/UI/WBP_StatusPanel',
        rect: { left: 100, top: 120, right: 640, bottom: 320 },
        appendOrToggle: true,
        tabId: 'SlatePreview',
        windowTitle: 'WBP_StatusPanel'
      },
      mockTools
    );

    expect(mockExecuteAutomationRequest).toHaveBeenCalledWith(
      mockTools,
      'control_editor',
      expect.objectContaining({
        action: 'select_widgets_in_designer_rect',
        assetPath: '/Game/UI/WBP_StatusPanel',
        rect: { left: 100, top: 120, right: 640, bottom: 320 },
        appendOrToggle: true,
        tabId: 'SlatePreview',
        windowTitle: 'WBP_StatusPanel'
      })
    );
  });

  it('preserves semantic rectangle-selection diagnostics from the automation bridge response', async () => {
    mockExecuteAutomationRequest.mockResolvedValue({
      success: true,
      currentMode: 'Designer',
      resolvedTargetSource: 'designer_tab',
      matchedWidgetPaths: ['RootCanvas/MarqueeA', 'RootCanvas/MarqueeB'],
      matchedWidgetCount: 2,
      appendOrToggle: false,
      selectedWidgetCount: 2
    });

    const result = await handleEditorTools(
      'select_widgets_in_designer_rect',
      {
        assetPath: '/Game/UI/WBP_StatusPanel',
        rect: { left: 100, top: 120, right: 640, bottom: 320 }
      },
      mockTools
    );

    expect(result).toMatchObject({
      success: true,
      currentMode: 'Designer',
      resolvedTargetSource: 'designer_tab',
      matchedWidgetPaths: ['RootCanvas/MarqueeA', 'RootCanvas/MarqueeB'],
      matchedWidgetCount: 2,
      appendOrToggle: false,
      selectedWidgetCount: 2
    });
  });

  it('preserves semantic focus diagnostics from the automation bridge response', async () => {
    mockExecuteAutomationRequest.mockResolvedValue({
      success: true,
      focusApplied: true,
      focusTargetSurface: 'widget_designer',
      resolvedTargetSource: 'designer_tab',
      targetStatus: 'resolved',
      designerTabFound: true,
      designerViewFound: true,
      keyboardFocusedWidgetType: 'SDesignerView'
    });

    const result = await handleEditorTools(
      'focus_editor_surface',
      {
        surface: 'widget_designer',
        assetPath: '/Game/UI/WBP_StatusPanel',
        tabId: 'SlatePreview',
        windowTitle: 'WBP_StatusPanel'
      },
      mockTools
    );

    expect(result).toMatchObject({
      success: true,
      focusApplied: true,
      focusTargetSurface: 'widget_designer',
      resolvedTargetSource: 'designer_tab',
      targetStatus: 'resolved',
      designerTabFound: true,
      designerViewFound: true,
      keyboardFocusedWidgetType: 'SDesignerView'
    });
  });

  it('preserves stale focus diagnostics without hiding requested target hints', async () => {
    mockExecuteAutomationRequest.mockResolvedValue({
      success: false,
      errorCode: 'FOCUS_FAILED',
      focusApplied: false,
      focusTargetSurface: 'editor_window',
      requestedTabId: 'MissingSlatePreviewValidation',
      requestedWindowTitle: 'WBP_UiTargeting__Stale',
      resolvedTargetSource: 'tab_id',
      targetStatus: 'stale',
      requestedTargetStillLive: false,
      reResolved: false,
      staleReason: 'missing_live_tab',
      focusFailureReason: 'Live tab MissingSlatePreviewValidation was not found',
      recoveryAction: 'resolve_ui_target'
    });

    const result = await handleEditorTools(
      'focus_editor_surface',
      {
        surface: 'editor_window',
        tabId: 'MissingSlatePreviewValidation',
        windowTitle: 'WBP_UiTargeting__Stale'
      },
      mockTools
    );

    expect(result).toMatchObject({
      success: false,
      errorCode: 'FOCUS_FAILED',
      focusApplied: false,
      focusTargetSurface: 'editor_window',
      requestedTabId: 'MissingSlatePreviewValidation',
      requestedWindowTitle: 'WBP_UiTargeting__Stale',
      targetStatus: 'stale',
      requestedTargetStillLive: false,
      reResolved: false,
      staleReason: 'missing_live_tab',
      focusFailureReason: expect.stringMatching(/live tab/i),
      recoveryAction: 'resolve_ui_target'
    });
  });

  it('keeps semantic navigation validation strict for missing selectors', async () => {
    await expect(
      handleEditorTools(
        'capture_blueprint_graph_review',
        {
          filename: 'graph-review-capture.png'
        },
        mockTools
      )
    ).rejects.toThrow(/assetPath/);

    await expect(
      handleEditorTools(
        'capture_blueprint_graph_review',
        {
          assetPath: '/Game/UI/WBP_StatusPanel'
        },
        mockTools
      )
    ).rejects.toThrow(/filename/);

    await expect(
      handleEditorTools(
        'jump_to_blueprint_node',
        {
          assetPath: '/Game/UI/WBP_StatusPanel',
          graphName: 'EventGraph'
        },
        mockTools
      )
    ).rejects.toThrow(/nodeGuid, nodeName, or nodeTitle/);

    await expect(
      handleEditorTools(
        'select_widget_in_designer',
        {
          assetPath: '/Game/UI/WBP_StatusPanel'
        },
        mockTools
      )
    ).rejects.toThrow(/widgetName, widgetPath, widgetObjectPath, or templateObjectPath/);

    await expect(
      handleEditorTools(
        'select_widgets_in_designer_rect',
        {
          assetPath: '/Game/UI/WBP_StatusPanel'
        },
        mockTools
      )
    ).rejects.toThrow(/rect with left, top, right, and bottom/);

    await expect(
      handleEditorTools(
        'set_widget_designer_view',
        {
          assetPath: '/Game/UI/WBP_StatusPanel'
        },
        mockTools
      )
    ).rejects.toThrow(/viewLocation or delta/);
  });
});
