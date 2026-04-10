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
import { handleConsolidatedToolCall } from '../../../src/tools/consolidated-tool-handlers.js';
import { executeAutomationRequest } from '../../../src/tools/handlers/common-handlers.js';

describe('UI Handlers', () => {
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

  it('publishes manage_ui with list_visible_windows and the visible-window output shape', () => {
    const uiTool = consolidatedToolDefinitions.find((tool) => tool.name === 'manage_ui');

    expect(uiTool).toBeDefined();

    const inputProperties = ((uiTool?.inputSchema as { properties?: Record<string, unknown> }).properties) ?? {};
    const actionSchema = inputProperties.action as { enum?: string[] } | undefined;
    const actionEnum = actionSchema?.enum ?? [];
    const outputProperties = ((uiTool?.outputSchema as { properties?: Record<string, unknown> }).properties) ?? {};
    const windowsSchema = outputProperties.windows as {
      items?: { properties?: Record<string, unknown> };
    } | undefined;
    const windowProperties = windowsSchema?.items?.properties ?? {};

    expect(actionEnum).toContain('list_visible_windows');
    expect(outputProperties.count).toBeDefined();
    expect(outputProperties.windows).toBeDefined();
    expect(windowProperties.title).toBeDefined();
    expect(windowProperties.isActive).toBeDefined();
    expect(windowProperties.isVisible).toBeDefined();
    expect(windowProperties.x).toBeDefined();
    expect(windowProperties.y).toBeDefined();
    expect(windowProperties.width).toBeDefined();
    expect(windowProperties.height).toBeDefined();
    expect(windowProperties.clientX).toBeDefined();
    expect(windowProperties.clientY).toBeDefined();
    expect(windowProperties.clientWidth).toBeDefined();
    expect(windowProperties.clientHeight).toBeDefined();
  });

  it('routes manage_ui list_visible_windows through the consolidated handler registry', async () => {
    mockExecuteAutomationRequest.mockResolvedValue({
      success: true,
      count: 1,
      windows: [
        {
          title: 'Output Log',
          isActive: true,
          isVisible: true,
          x: 0,
          y: 0,
          width: 1280,
          height: 720,
          clientX: 8,
          clientY: 42,
          clientWidth: 1264,
          clientHeight: 670
        }
      ]
    });

    const result = await handleConsolidatedToolCall(
      'manage_ui',
      { action: 'list_visible_windows' },
      mockTools
    );

    expect(mockExecuteAutomationRequest).toHaveBeenCalledTimes(1);

    const [toolsArg, toolNameArg, payloadArg, errorMessageArg] = mockExecuteAutomationRequest.mock.calls[0] ?? [];
    expect(toolsArg).toBe(mockTools);
    expect(toolNameArg).toBe('manage_ui');
    expect(payloadArg).toMatchObject({
      action: 'list_visible_windows',
      subAction: 'list_visible_windows'
    });
    expect(errorMessageArg).toBe('Automation bridge not available for manage_ui operations');
    expect(result).toEqual({
      success: true,
      count: 1,
      windows: [
        {
          title: 'Output Log',
          isActive: true,
          isVisible: true,
          x: 0,
          y: 0,
          width: 1280,
          height: 720,
          clientX: 8,
          clientY: 42,
          clientWidth: 1264,
          clientHeight: 670
        }
      ]
    });
  });

  it('rejects unsupported list_visible_windows extras instead of silently swallowing them', async () => {
    const result = await handleConsolidatedToolCall(
      'manage_ui',
      { action: 'list_visible_windows', invalidExtraParam: true },
      mockTools
    );

    expect(result).toMatchObject({
      success: false,
      isError: true,
      message: expect.stringMatching(/invalidExtraParam/)
    });

    expect(mockExecuteAutomationRequest).not.toHaveBeenCalled();
  });

  it('publishes manage_ui resolve_ui_target with status, recovery, and bounds fields', () => {
    const uiTool = consolidatedToolDefinitions.find((tool) => tool.name === 'manage_ui');

    expect(uiTool).toBeDefined();

    const inputProperties = ((uiTool?.inputSchema as { properties?: Record<string, unknown> }).properties) ?? {};
    const actionSchema = inputProperties.action as { enum?: string[] } | undefined;
    const actionEnum = actionSchema?.enum ?? [];
    const outputProperties = ((uiTool?.outputSchema as { properties?: Record<string, unknown> }).properties) ?? {};

    expect(actionEnum).toContain('resolve_ui_target');
    expect(outputProperties.targetStatus).toBeDefined();
    expect(outputProperties.requestedIdentifier).toBeDefined();
    expect(outputProperties.requestedTabId).toBeDefined();
    expect(outputProperties.requestedWindowTitle).toBeDefined();
    expect(outputProperties.resolvedIdentifier).toBeDefined();
    expect(outputProperties.resolvedTabId).toBeDefined();
    expect(outputProperties.resolvedWindowTitle).toBeDefined();
    expect(outputProperties.resolvedTargetSource).toBeDefined();
    expect(outputProperties.reResolved).toBeDefined();
    expect(outputProperties.staleReason).toBeDefined();
    expect(outputProperties.recoveryHint).toBeDefined();
    expect(outputProperties.recoveryAction).toBeDefined();
    expect(outputProperties.clientX).toBeDefined();
    expect(outputProperties.clientY).toBeDefined();
    expect(outputProperties.clientWidth).toBeDefined();
    expect(outputProperties.clientHeight).toBeDefined();
  });

  it('routes manage_ui resolve_ui_target through the consolidated handler without mutating target hints', async () => {
    mockExecuteAutomationRequest.mockResolvedValue({
      success: true,
      targetStatus: 'needs_open',
      requestedIdentifier: 'WidgetDesigner',
      requestedTabId: 'SlatePreview',
      requestedWindowTitle: 'WBP_StatusPanel',
      resolvedTargetSource: 'designer_tab',
      recoveryHint: 'Open the target through manage_ui.open_ui_target and retry resolution.',
      recoveryAction: 'open_ui_target'
    });

    const result = await handleConsolidatedToolCall(
      'manage_ui',
      {
        action: 'resolve_ui_target',
        identifier: 'WidgetDesigner',
        tabId: 'SlatePreview',
        windowTitle: 'WBP_StatusPanel'
      },
      mockTools
    );

    expect(mockExecuteAutomationRequest).toHaveBeenCalledTimes(1);

    const [toolsArg, toolNameArg, payloadArg] = mockExecuteAutomationRequest.mock.calls[0] ?? [];
    expect(toolsArg).toBe(mockTools);
    expect(toolNameArg).toBe('manage_ui');
    expect(payloadArg).toMatchObject({
      action: 'resolve_ui_target',
      subAction: 'resolve_ui_target',
      identifier: 'WidgetDesigner',
      tabId: 'SlatePreview',
      windowTitle: 'WBP_StatusPanel'
    });
    expect(payloadArg).not.toHaveProperty('openTarget');
    expect(payloadArg).not.toHaveProperty('focusTarget');
    expect(result).toMatchObject({
      success: true,
      targetStatus: 'needs_open',
      requestedIdentifier: 'WidgetDesigner',
      requestedTabId: 'SlatePreview',
      requestedWindowTitle: 'WBP_StatusPanel',
      resolvedTargetSource: 'designer_tab',
      recoveryHint: expect.any(String),
      recoveryAction: 'open_ui_target'
    });
  });

  it('returns a structured public error when resolve_ui_target is missing all target hints', async () => {
    const result = await handleConsolidatedToolCall(
      'manage_ui',
      { action: 'resolve_ui_target' },
      mockTools
    );

    expect(result).toMatchObject({
      success: false,
      isError: true,
      message: expect.stringMatching(/resolve_ui_target requires identifier or tabId or windowTitle/i)
    });

    expect(mockExecuteAutomationRequest).not.toHaveBeenCalled();
  });

  it('preserves resolved and stale target fields from resolve_ui_target responses', async () => {
    mockExecuteAutomationRequest.mockResolvedValue({
      success: true,
      targetStatus: 'stale',
      requestedIdentifier: 'WidgetDesigner',
      requestedTabId: 'SlatePreview',
      requestedWindowTitle: 'WBP_StatusPanel',
      resolvedIdentifier: 'WidgetDesigner',
      resolvedTabId: 'SlatePreview',
      resolvedWindowTitle: 'WBP_StatusPanel',
      resolvedTargetSource: 'designer_tab',
      reResolved: true,
      staleReason: 'missing_live_tab',
      recoveryHint: 'Open the target through manage_ui.open_ui_target and retry resolution.',
      recoveryAction: 'open_ui_target'
    });

    const result = await handleConsolidatedToolCall(
      'manage_ui',
      {
        action: 'resolve_ui_target',
        identifier: 'WidgetDesigner',
        tabId: 'SlatePreview'
      },
      mockTools
    );

    expect(result).toMatchObject({
      success: true,
      targetStatus: 'stale',
      requestedIdentifier: 'WidgetDesigner',
      requestedTabId: 'SlatePreview',
      requestedWindowTitle: 'WBP_StatusPanel',
      resolvedIdentifier: 'WidgetDesigner',
      resolvedTabId: 'SlatePreview',
      resolvedWindowTitle: 'WBP_StatusPanel',
      resolvedTargetSource: 'designer_tab',
      reResolved: true,
      staleReason: 'missing_live_tab',
      recoveryHint: expect.any(String),
      recoveryAction: 'open_ui_target'
    });
  });
});