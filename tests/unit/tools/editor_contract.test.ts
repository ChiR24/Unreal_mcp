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

describe('Editor Contract', () => {
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

  it('publishes explicit screenshot and simulated-input diagnostics in the public schema', () => {
    const editorTool = consolidatedToolDefinitions.find((tool) => tool.name === 'control_editor');

    expect(editorTool).toBeDefined();

    const inputProperties = ((editorTool?.inputSchema as { properties?: Record<string, unknown> }).properties) ?? {};
    const outputProperties = ((editorTool?.outputSchema as { properties?: Record<string, unknown> }).properties) ?? {};

    expect(inputProperties.mode).toBeDefined();
    expect(inputProperties.includeMenus).toBeDefined();
    expect(inputProperties.windowTitle).toBeDefined();
    expect(inputProperties.tabId).toBeDefined();
    expect(inputProperties.captureScreenshots).toBeDefined();
    expect(inputProperties.text).toBeDefined();
    expect(inputProperties.submit).toBeDefined();
    expect(inputProperties.button).toBeDefined();
    expect(inputProperties.start).toBeDefined();
    expect(inputProperties.end).toBeDefined();

    expect(outputProperties.captureTarget).toBeDefined();
    expect(outputProperties.requestedCaptureMode).toBeDefined();
    expect(outputProperties.requestedWindowTitle).toBeDefined();
    expect(outputProperties.requestedTabId).toBeDefined();
    expect(outputProperties.resolvedTargetSource).toBeDefined();
    expect(outputProperties.windowTitle).toBeDefined();
    expect(outputProperties.includeMenus).toBeDefined();
    expect(outputProperties.includedMenuWindowCount).toBeDefined();
    expect(outputProperties.tabId).toBeDefined();
    expect(outputProperties.targetWidgetPathValid).toBeDefined();
    expect(outputProperties.targetWidgetPath).toBeDefined();
    expect(outputProperties.keyboardFocusedWidgetType).toBeDefined();
    expect(outputProperties.userFocusedWidgetType).toBeDefined();
    expect(outputProperties.captureIntentWarning).toBeDefined();
    expect(outputProperties.captureIntentSource).toBeDefined();
    expect(outputProperties.suggestedMode).toBeDefined();
    expect(outputProperties.suggestedPreflightAction).toBeDefined();
    expect(outputProperties.targetStatus).toBeDefined();
    expect(outputProperties.requestedTargetStillLive).toBeDefined();
    expect(outputProperties.reResolved).toBeDefined();
    expect(outputProperties.staleReason).toBeDefined();
    expect(outputProperties.recoveryHint).toBeDefined();
    expect(outputProperties.recoveryAction).toBeDefined();
  });

  it('publishes focus_editor_surface with explicit surface targeting fields in the public schema', () => {
    const editorTool = consolidatedToolDefinitions.find((tool) => tool.name === 'control_editor');

    expect(editorTool).toBeDefined();

    const inputProperties = ((editorTool?.inputSchema as { properties?: Record<string, unknown> }).properties) ?? {};
    const actionSchema = inputProperties.action as { enum?: string[] } | undefined;
    const outputProperties = ((editorTool?.outputSchema as { properties?: Record<string, unknown> }).properties) ?? {};

    expect(actionSchema?.enum).toContain('focus_editor_surface');
    expect(inputProperties.surface).toBeDefined();
    expect(inputProperties.assetPath).toBeDefined();
    expect(inputProperties.tabId).toBeDefined();
    expect(inputProperties.windowTitle).toBeDefined();
    expect(outputProperties.focusApplied).toBeDefined();
    expect(outputProperties.focusTargetSurface).toBeDefined();
    expect(outputProperties.focusFailureReason).toBeDefined();
  });

  it('forwards focus_editor_surface without stripping target hints', async () => {
    mockExecuteAutomationRequest.mockResolvedValue({ success: true, focusApplied: true });

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

    expect(mockExecuteAutomationRequest).toHaveBeenCalledTimes(1);

    const [toolsArg, toolNameArg, payloadArg] = mockExecuteAutomationRequest.mock.calls[0] ?? [];
    expect(toolsArg).toBe(mockTools);
    expect(toolNameArg).toBe('control_editor');
    expect(payloadArg).toMatchObject({
      action: 'focus_editor_surface',
      surface: 'widget_designer',
      assetPath: '/Game/UI/WBP_StatusPanel',
      tabId: 'SlatePreview',
      windowTitle: 'WBP_StatusPanel'
    });
  });

  it('forwards screenshot target fields with windowTitle as the public alias', async () => {
    mockExecuteAutomationRequest.mockResolvedValue({ success: true, captureTarget: 'editor_window' });

    await handleEditorTools(
      'screenshot',
      {
        filename: 'asset-editor.png',
        resolution: '1280x720',
        mode: 'editor',
        includeMenus: false,
        windowTitle: 'WBP_StatusPanel',
        tabId: 'Document'
      },
      mockTools
    );

    expect(mockExecuteAutomationRequest).toHaveBeenCalledTimes(1);

    const [toolsArg, toolNameArg, payloadArg] = mockExecuteAutomationRequest.mock.calls[0] ?? [];
    expect(toolsArg).toBe(mockTools);
    expect(toolNameArg).toBe('control_editor');
    expect(payloadArg).toMatchObject({
      action: 'screenshot',
      filename: 'asset-editor.png',
      resolution: '1280x720',
      mode: 'editor',
      includeMenus: false,
      name: 'WBP_StatusPanel',
      tabId: 'Document'
    });
  });

  it('forwards targeted text input diagnostics through simulate_input', async () => {
    mockExecuteAutomationRequest.mockResolvedValue({ success: true, type: 'text' });

    await handleEditorTools(
      'simulate_input',
      {
        inputAction: 'text',
        windowTitle: 'WBP_StatusPanel',
        text: 'Hello bridge',
        submit: true,
        captureScreenshots: false
      },
      mockTools
    );

    expect(mockExecuteAutomationRequest).toHaveBeenCalledTimes(1);

    const [, toolNameArg, payloadArg] = mockExecuteAutomationRequest.mock.calls[0] ?? [];
    expect(toolNameArg).toBe('control_editor');
    expect(payloadArg).toMatchObject({
      action: 'simulate_input',
      type: 'text',
      windowTitle: 'WBP_StatusPanel',
      text: 'Hello bridge',
      submit: true,
      captureScreenshots: false
    });
  });

  it('forwards targeted drag payloads through simulate_input', async () => {
    mockExecuteAutomationRequest.mockResolvedValue({ success: true, type: 'mouse_drag' });

    await handleEditorTools(
      'simulate_input',
      {
        inputAction: 'mouse_drag',
        tabId: 'LevelEditor.LevelEditorToolBox',
        captureScreenshots: true,
        button: 'right',
        start: { clientX: 20, clientY: 40 },
        end: { clientX: 160, clientY: 90 },
        durationMs: 180,
        holdBeforeMoveMs: 15,
        holdAfterMoveMs: 25,
        steps: 6
      },
      mockTools
    );

    expect(mockExecuteAutomationRequest).toHaveBeenCalledTimes(1);

    const [, toolNameArg, payloadArg] = mockExecuteAutomationRequest.mock.calls[0] ?? [];
    expect(toolNameArg).toBe('control_editor');
    expect(payloadArg).toMatchObject({
      action: 'simulate_input',
      type: 'mouse_drag',
      tabId: 'LevelEditor.LevelEditorToolBox',
      captureScreenshots: true,
      button: 'right',
      start: { clientX: 20, clientY: 40 },
      end: { clientX: 160, clientY: 90 },
      durationMs: 180,
      holdBeforeMoveMs: 15,
      holdAfterMoveMs: 25,
      steps: 6
    });
  });

  it('preserves resolved target diagnostics from screenshot and simulate_input responses', async () => {
    mockExecuteAutomationRequest
      .mockResolvedValueOnce({
        success: true,
        captureTarget: 'editor_window',
        requestedCaptureMode: 'editor',
        requestedWindowTitle: 'WBP_StatusPanel',
        requestedTabId: 'Document',
        resolvedTargetSource: 'windowTitle',
        windowTitle: 'WBP_StatusPanel'
      })
      .mockResolvedValueOnce({
        success: true,
        requestedWindowTitle: 'WBP_StatusPanel',
        requestedTabId: 'Document',
        resolvedTargetSource: 'tabId',
        targetWidgetPathValid: true,
        keyboardFocusedWidgetType: 'SEditableTextBox'
      });

    const screenshotResult = await handleEditorTools(
      'screenshot',
      {
        filename: 'asset-editor.png',
        mode: 'editor',
        windowTitle: 'WBP_StatusPanel',
        tabId: 'Document'
      },
      mockTools
    );

    const inputResult = await handleEditorTools(
      'simulate_input',
      {
        inputAction: 'text',
        windowTitle: 'WBP_StatusPanel',
        tabId: 'Document',
        text: 'Hello bridge'
      },
      mockTools
    );

    expect(screenshotResult).toMatchObject({
      success: true,
      captureTarget: 'editor_window',
      requestedCaptureMode: 'editor',
      requestedWindowTitle: 'WBP_StatusPanel',
      requestedTabId: 'Document',
      resolvedTargetSource: 'windowTitle'
    });

    expect(inputResult).toMatchObject({
      success: true,
      requestedWindowTitle: 'WBP_StatusPanel',
      requestedTabId: 'Document',
      resolvedTargetSource: 'tabId',
      targetWidgetPathValid: true,
      keyboardFocusedWidgetType: 'SEditableTextBox'
    });
  });

  it('preserves screenshot ambiguity failures and stale-target diagnostics instead of implying tabId selected capture', async () => {
    mockExecuteAutomationRequest
      .mockResolvedValueOnce({
        success: false,
        errorCode: 'AMBIGUOUS_CAPTURE_TARGET',
        requestedCaptureMode: 'viewport',
        requestedWindowTitle: '',
        requestedTabId: 'SlatePreview',
        captureIntentWarning: 'tabId is ignored for screenshot targeting; use windowTitle or resolve_ui_target first.',
        suggestedMode: 'editor',
        suggestedPreflightAction: 'resolve_ui_target',
        targetStatus: 'stale',
        requestedTargetStillLive: false,
        reResolved: false,
        staleReason: 'missing_live_tab',
        recoveryHint: 'Resolve the target again before capturing.',
        recoveryAction: 'resolve_ui_target'
      })
      .mockResolvedValueOnce({
        success: true,
        requestedWindowTitle: 'WBP_StatusPanel',
        requestedTabId: 'SlatePreview',
        resolvedTargetSource: 'designer_tab',
        targetStatus: 'resolved',
        requestedTargetStillLive: true,
        reResolved: false,
        focusApplied: true,
        keyboardFocusedWidgetType: 'SDesignerView'
      });

    const screenshotResult = await handleEditorTools(
      'screenshot',
      {
        filename: 'ambiguous.png',
        tabId: 'SlatePreview'
      },
      mockTools
    );

    const inputResult = await handleEditorTools(
      'simulate_input',
      {
        inputAction: 'text',
        windowTitle: 'WBP_StatusPanel',
        tabId: 'SlatePreview',
        text: 'Hello bridge'
      },
      mockTools
    );

    expect(screenshotResult).toMatchObject({
      success: false,
      errorCode: 'AMBIGUOUS_CAPTURE_TARGET',
      requestedTabId: 'SlatePreview',
      captureIntentWarning: expect.stringMatching(/tabId is ignored/i),
      suggestedMode: 'editor',
      suggestedPreflightAction: 'resolve_ui_target',
      targetStatus: 'stale',
      requestedTargetStillLive: false,
      reResolved: false,
      staleReason: 'missing_live_tab',
      recoveryAction: 'resolve_ui_target'
    });
    expect(screenshotResult).not.toHaveProperty('captureTarget');

    expect(inputResult).toMatchObject({
      success: true,
      requestedWindowTitle: 'WBP_StatusPanel',
      requestedTabId: 'SlatePreview',
      resolvedTargetSource: 'designer_tab',
      targetStatus: 'resolved',
      requestedTargetStillLive: true,
      reResolved: false,
      focusApplied: true,
      keyboardFocusedWidgetType: 'SDesignerView'
    });
  });

  it('preserves editor-mode screenshot ambiguity failures instead of implying editor-window fallback', async () => {
    mockExecuteAutomationRequest.mockResolvedValueOnce({
      success: false,
      errorCode: 'AMBIGUOUS_CAPTURE_TARGET',
      requestedCaptureMode: 'editor',
      requestedWindowTitle: '',
      requestedTabId: 'SlatePreview',
      captureIntentWarning: 'Requested tabId is not live and screenshot targeting does not consume tabId directly; resolve a windowTitle before retrying.',
      suggestedMode: 'editor',
      suggestedPreflightAction: 'resolve_ui_target',
      targetStatus: 'stale',
      requestedTargetStillLive: false,
      reResolved: false,
      staleReason: 'missing_live_tab',
      recoveryHint: 'Resolve the target again before capturing.',
      recoveryAction: 'resolve_ui_target'
    });

    const screenshotResult = await handleEditorTools(
      'screenshot',
      {
        filename: 'ambiguous-editor.png',
        mode: 'editor',
        tabId: 'SlatePreview'
      },
      mockTools
    );

    expect(screenshotResult).toMatchObject({
      success: false,
      errorCode: 'AMBIGUOUS_CAPTURE_TARGET',
      requestedCaptureMode: 'editor',
      requestedTabId: 'SlatePreview',
      captureIntentWarning: expect.stringMatching(/tabId/i),
      suggestedMode: 'editor',
      suggestedPreflightAction: 'resolve_ui_target',
      targetStatus: 'stale',
      requestedTargetStillLive: false,
      reResolved: false,
      staleReason: 'missing_live_tab',
      recoveryAction: 'resolve_ui_target'
    });
    expect(screenshotResult).not.toHaveProperty('captureTarget');
  });

  it('still rejects unsupported simulate_input extras', async () => {
    await expect(
      handleEditorTools(
        'simulate_input',
        {
          inputAction: 'text',
          text: 'Hello bridge',
          invalidExtraParam: true
        },
        mockTools
      )
    ).rejects.toThrow(/invalidExtraParam/);

    expect(mockExecuteAutomationRequest).not.toHaveBeenCalled();
  });
});