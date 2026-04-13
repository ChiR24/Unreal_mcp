import { beforeEach, describe, expect, it, vi } from 'vitest';

import type { ITools } from '../../types/tool-interfaces.js';
import { consolidatedToolDefinitions } from '../consolidated-tool-definitions.js';

vi.mock('./common-handlers.js', async (importOriginal) => {
  const actual = await importOriginal<typeof import('./common-handlers.js')>();

  return {
    ...actual,
    executeAutomationRequest: vi.fn()
  };
});

import { executeAutomationRequest } from './common-handlers.js';
import { handleWidgetAuthoringTools } from './widget-authoring-handlers.js';

function getManageWidgetAuthoringDefinition() {
  const definition = consolidatedToolDefinitions.find((tool) => tool.name === 'manage_widget_authoring');
  expect(definition).toBeDefined();
  return definition!;
}

describe('widget authoring handlers', () => {
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

  it('rejects get_widget_tree when widgetPath is missing', async () => {
    await expect(handleWidgetAuthoringTools('get_widget_tree', {}, mockTools)).rejects.toThrow(
      'Missing required parameter: widgetPath'
    );
  });

  it('rejects get_widget_designer_state when the top-level asset widgetPath is missing', async () => {
    await expect(handleWidgetAuthoringTools('get_widget_designer_state', {}, mockTools)).rejects.toThrow(
      'Missing required parameter: widgetPath'
    );
  });

  it('forwards get_widget_tree through executeAutomationRequest with a normalized widget path', async () => {
    mockExecuteAutomationRequest.mockResolvedValue({
      success: true,
      widgetTree: {
        name: 'RootCanvas',
        class: 'CanvasPanel',
        isPanel: true,
        isVariable: false,
        slotClass: '',
        children: [
          {
            name: 'StatusLabel',
            class: 'TextBlock',
            isPanel: false,
            isVariable: true,
            slotClass: 'CanvasPanelSlot',
            children: []
          }
        ]
      },
      widgetCount: 2,
      rootWidgetName: 'RootCanvas'
    });

    const result = await handleWidgetAuthoringTools(
      'get_widget_tree',
      { widgetPath: '/Content/UI/WBP_StatusPanel' },
      mockTools
    );

    expect(mockExecuteAutomationRequest).toHaveBeenCalledWith(
      mockTools,
      'manage_widget_authoring',
      {
        widgetPath: '/Game/UI/WBP_StatusPanel',
        subAction: 'get_widget_tree'
      },
      'Automation bridge not available for widget authoring action: get_widget_tree',
      expect.objectContaining({ timeoutMs: expect.any(Number) })
    );

    expect(result).toEqual({
      success: true,
      widgetTree: {
        name: 'RootCanvas',
        class: 'CanvasPanel',
        isPanel: true,
        isVariable: false,
        slotClass: '',
        children: [
          {
            name: 'StatusLabel',
            class: 'TextBlock',
            isPanel: false,
            isVariable: true,
            slotClass: 'CanvasPanelSlot',
            children: []
          }
        ]
      },
      widgetCount: 2,
      rootWidgetName: 'RootCanvas'
    });
  });

  it('forwards get_widget_designer_state with nested selector fields intact', async () => {
    mockExecuteAutomationRequest.mockResolvedValue({
      success: true,
      widgetTree: {
        name: 'RootCanvas',
        class: 'CanvasPanel',
        isPanel: true,
        isVariable: false,
        slotClass: '',
        children: []
      },
      liveEditorContextFound: false,
      selectedWidgetCount: 0,
      selectedWidgets: []
    });

    await handleWidgetAuthoringTools(
      'get_widget_designer_state',
      {
        widgetPath: '/Content/UI/WBP_StatusPanel',
        selector: {
          widgetPath: 'RootCanvas/StatusBorder',
          widgetName: 'StatusBorder'
        },
        openEditorIfNeeded: false
      },
      mockTools
    );

    expect(mockExecuteAutomationRequest).toHaveBeenCalledWith(
      mockTools,
      'manage_widget_authoring',
      {
        widgetPath: '/Game/UI/WBP_StatusPanel',
        selector: {
          widgetPath: 'RootCanvas/StatusBorder',
          widgetName: 'StatusBorder'
        },
        openEditorIfNeeded: false,
        subAction: 'get_widget_designer_state'
      },
      'Automation bridge not available for widget authoring action: get_widget_designer_state',
      expect.objectContaining({ timeoutMs: expect.any(Number) })
    );

    expect(mockExecuteAutomationRequest).not.toHaveBeenCalledWith(
      mockTools,
      'manage_widget_authoring',
      expect.objectContaining({
        widgetPath: 'RootCanvas/StatusBorder'
      }),
      expect.any(String),
      expect.any(Object)
    );
  });

  it('preserves layout metadata in get_widget_designer_state results for canvas-backed widgets', async () => {
    mockExecuteAutomationRequest.mockResolvedValue({
      success: true,
      widgetTree: {
        name: 'RootCanvas',
        class: 'CanvasPanel',
        isPanel: true,
        isVariable: false,
        slotClass: '',
        children: [
          {
            name: 'MarqueeA',
            class: 'Border',
            isPanel: false,
            isVariable: false,
            slotClass: 'CanvasPanelSlot',
            layout: {
              slotClass: 'CanvasPanelSlot',
              position: { x: 120, y: 120 },
              size: { x: 180, y: 120 },
              alignment: { x: 0, y: 0 },
              anchors: {
                minimum: { x: 0, y: 0 },
                maximum: { x: 0, y: 0 }
              },
              designerBounds: {
                left: 480,
                top: 180,
                right: 660,
                bottom: 300,
                width: 180,
                height: 120
              },
              zOrder: 0
            },
            children: []
          }
        ]
      },
      liveEditorContextFound: true,
      selectedWidgetCount: 1,
      selectedWidgets: [
        {
          name: 'MarqueeA',
          widgetClass: 'Border',
          widgetPath: 'RootCanvas/MarqueeA',
          widgetObjectPath: '/Game/UI/WBP_StatusPanel.WBP_StatusPanel:WidgetTree.MarqueeA',
          slotClass: 'CanvasPanelSlot',
          layout: {
            slotClass: 'CanvasPanelSlot',
            position: { x: 120, y: 120 },
            size: { x: 180, y: 120 },
            alignment: { x: 0, y: 0 },
            anchors: {
              minimum: { x: 0, y: 0 },
              maximum: { x: 0, y: 0 }
            },
            designerBounds: {
              left: 480,
              top: 180,
              right: 660,
              bottom: 300,
              width: 180,
              height: 120
            },
            zOrder: 0
          }
        }
      ]
    });

    const result = await handleWidgetAuthoringTools(
      'get_widget_designer_state',
      {
        widgetPath: '/Content/UI/WBP_StatusPanel',
        openEditorIfNeeded: false
      },
      mockTools
    );

    expect(result).toMatchObject({
      widgetTree: {
        children: [
          {
            slotClass: 'CanvasPanelSlot',
            layout: {
              slotClass: 'CanvasPanelSlot',
              position: { x: 120, y: 120 },
              size: { x: 180, y: 120 },
              alignment: { x: 0, y: 0 },
              anchors: {
                minimum: { x: 0, y: 0 },
                maximum: { x: 0, y: 0 }
              },
              designerBounds: {
                left: 480,
                top: 180,
                right: 660,
                bottom: 300,
                width: 180,
                height: 120
              },
              zOrder: 0
            }
          }
        ]
      },
      selectedWidgets: [
        {
          widgetPath: 'RootCanvas/MarqueeA',
          slotClass: 'CanvasPanelSlot',
          layout: {
            slotClass: 'CanvasPanelSlot',
            position: { x: 120, y: 120 },
            size: { x: 180, y: 120 },
            alignment: { x: 0, y: 0 },
            anchors: {
              minimum: { x: 0, y: 0 },
              maximum: { x: 0, y: 0 }
            },
            designerBounds: {
              left: 480,
              top: 180,
              right: 660,
              bottom: 300,
              width: 180,
              height: 120
            },
            zOrder: 0
          }
        }
      ]
    });
  });

  it('preserves widgetTree designer bounds even when no selected widgets are returned', async () => {
    mockExecuteAutomationRequest.mockResolvedValue({
      success: true,
      widgetTree: {
        name: 'RootCanvas',
        class: 'CanvasPanel',
        isPanel: true,
        isVariable: false,
        slotClass: '',
        children: [
          {
            name: 'MarqueeA',
            class: 'Border',
            isPanel: false,
            isVariable: false,
            slotClass: 'CanvasPanelSlot',
            layout: {
              slotClass: 'CanvasPanelSlot',
              position: { x: 120, y: 120 },
              size: { x: 180, y: 120 },
              alignment: { x: 0, y: 0 },
              anchors: {
                minimum: { x: 0, y: 0 },
                maximum: { x: 0, y: 0 }
              },
              designerBounds: {
                left: 480,
                top: 180,
                right: 660,
                bottom: 300,
                width: 180,
                height: 120
              },
              zOrder: 0
            },
            children: []
          }
        ]
      },
      liveEditorContextFound: true,
      selectedWidgetCount: 0,
      selectedWidgets: []
    });

    const result = await handleWidgetAuthoringTools(
      'get_widget_designer_state',
      {
        widgetPath: '/Content/UI/WBP_StatusPanel',
        openEditorIfNeeded: false
      },
      mockTools
    );

    expect(result).toMatchObject({
      selectedWidgetCount: 0,
      selectedWidgets: [],
      widgetTree: {
        children: [
          {
            layout: {
              designerBounds: {
                left: 480,
                top: 180,
                right: 660,
                bottom: 300,
                width: 180,
                height: 120
              }
            }
          }
        ]
      }
    });
  });

  it('publishes binding contract fields for real binding authoring diagnostics', () => {
    const definition = getManageWidgetAuthoringDefinition();
    const inputProperties = definition.inputSchema.properties as Record<string, unknown>;
    const outputProperties = definition.outputSchema?.properties as Record<string, unknown>;

    expect(inputProperties.ensureVariable).toEqual(expect.objectContaining({ type: 'boolean' }));
    expect(outputProperties.bindingApplied).toEqual(expect.objectContaining({ type: 'boolean' }));
    expect(outputProperties.bindingFunctionName).toEqual(expect.objectContaining({ type: 'string' }));
    expect(outputProperties.requiresManualFollowThrough).toEqual(expect.objectContaining({ type: 'boolean' }));
    expect(outputProperties.eventNodeCreated).toEqual(expect.objectContaining({ type: 'boolean' }));
    expect(outputProperties.widgetWasMadeVariable).toEqual(expect.objectContaining({ type: 'boolean' }));
  });

  it('normalizes widget paths and forwards property binding metadata through the shared native seam', async () => {
    mockExecuteAutomationRequest.mockResolvedValue({
      success: true,
      widgetPath: '/Game/UI/WBP_StatusPanel',
      slotName: 'StatusLabel',
      propertyName: 'Text',
      bindingApplied: true,
      bindingFunctionName: '__GetStatusText',
      bindingFunctionCreated: true,
      bindingFunctionExisted: false,
      requiresManualFollowThrough: false,
      manualSteps: []
    });

    const result = await handleWidgetAuthoringTools(
      'bind_text',
      {
        widgetPath: '/Content/UI/WBP_StatusPanel',
        slotName: 'StatusLabel',
        bindingSource: 'StatusText',
        functionName: 'GetStatusText'
      },
      mockTools
    );

    expect(mockExecuteAutomationRequest).toHaveBeenCalledWith(
      mockTools,
      'manage_widget_authoring',
      {
        widgetPath: '/Game/UI/WBP_StatusPanel',
        slotName: 'StatusLabel',
        bindingSource: 'StatusText',
        functionName: 'GetStatusText',
        targetWidget: 'StatusLabel',
        property: 'Text',
        propertyName: 'Text',
        subAction: 'set_widget_binding'
      },
      'Automation bridge not available for widget authoring action: set_widget_binding',
      expect.objectContaining({ timeoutMs: expect.any(Number) })
    );

    expect(result).toEqual({
      success: true,
      widgetPath: '/Game/UI/WBP_StatusPanel',
      slotName: 'StatusLabel',
      propertyName: 'Text',
      bindingApplied: true,
      bindingFunctionName: '__GetStatusText',
      bindingFunctionCreated: true,
      bindingFunctionExisted: false,
      requiresManualFollowThrough: false,
      manualSteps: []
    });
  });

  it('preserves idempotence metadata for repeated property bindings', async () => {
    mockExecuteAutomationRequest.mockResolvedValue({
      success: true,
      widgetPath: '/Game/UI/WBP_StatusPanel',
      slotName: 'StatusLabel',
      propertyName: 'Text',
      bindingApplied: true,
      bindingFunctionName: '__GetStatusText',
      bindingFunctionCreated: false,
      bindingFunctionExisted: true,
      requiresManualFollowThrough: false,
      manualSteps: []
    });

    const result = await handleWidgetAuthoringTools(
      'bind_text',
      {
        widgetPath: '/Game/UI/WBP_StatusPanel',
        slotName: 'StatusLabel',
        bindingSource: 'StatusText'
      },
      mockTools
    );

    expect(result).toEqual({
      success: true,
      widgetPath: '/Game/UI/WBP_StatusPanel',
      slotName: 'StatusLabel',
      propertyName: 'Text',
      bindingApplied: true,
      bindingFunctionName: '__GetStatusText',
      bindingFunctionCreated: false,
      bindingFunctionExisted: true,
      requiresManualFollowThrough: false,
      manualSteps: []
    });
  });

  it('keeps ensureVariable explicit for event bindings and preserves the not-variable failure contract', async () => {
    mockExecuteAutomationRequest.mockResolvedValue({
      success: false,
      bindingApplied: false,
      slotName: 'StartButton',
      eventType: 'OnClicked',
      widgetIsVariable: false,
      widgetWasMadeVariable: false,
      requiresBlueprintVariable: true,
      errorCode: 'WIDGET_NOT_VARIABLE',
      suggestedFix: 'Retry with ensureVariable: true or mark the widget as a variable in the Widget Blueprint.'
    });

    const result = await handleWidgetAuthoringTools(
      'bind_on_clicked',
      {
        widgetPath: '/Content/UI/WBP_StatusPanel',
        slotName: 'StartButton',
        functionName: 'OnStartClicked',
        ensureVariable: false
      },
      mockTools
    );

    expect(mockExecuteAutomationRequest).toHaveBeenCalledWith(
      mockTools,
      'manage_widget_authoring',
      {
        widgetPath: '/Game/UI/WBP_StatusPanel',
        slotName: 'StartButton',
        functionName: 'OnStartClicked',
        ensureVariable: false,
        subAction: 'bind_on_clicked'
      },
      'Automation bridge not available for widget authoring action: bind_on_clicked',
      expect.objectContaining({ timeoutMs: expect.any(Number) })
    );

    expect(result).toEqual({
      success: false,
      bindingApplied: false,
      slotName: 'StartButton',
      eventType: 'OnClicked',
      widgetIsVariable: false,
      widgetWasMadeVariable: false,
      requiresBlueprintVariable: true,
      errorCode: 'WIDGET_NOT_VARIABLE',
      suggestedFix: 'Retry with ensureVariable: true or mark the widget as a variable in the Widget Blueprint.'
    });
  });
});