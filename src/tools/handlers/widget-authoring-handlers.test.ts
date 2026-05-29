import { describe, expect, it, vi } from 'vitest';
import type { AutomationBridge } from '../../automation/index.js';
import type { ITools } from '../../types/tool-interfaces.js';
import { handleWidgetAuthoringTools } from './widget-authoring-handlers.js';

type SendAutomationRequest = (
  action: string,
  payload: Record<string, unknown>,
  options?: { timeoutMs?: number }
) => Promise<{ success: boolean }>;

function createConnectedTools() {
  const sendAutomationRequest = vi.fn<SendAutomationRequest>(async () => ({ success: true }));
  const tools: ITools = {
    systemTools: {
      executeConsoleCommand: vi.fn(async () => ({ success: true })),
      getProjectSettings: vi.fn(async () => ({}))
    },
    assetResources: {
      list: vi.fn(async () => ({}))
    },
    automationBridge: {
      isConnected: () => true,
      sendAutomationRequest
    } as unknown as AutomationBridge
  };

  return { tools, sendAutomationRequest };
}

describe('handleWidgetAuthoringTools payload normalization', () => {
  it('normalizes create paths before Unreal dispatch', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await handleWidgetAuthoringTools('create_widget_blueprint', {
      action: 'create_widget_blueprint',
      name: 'WBP_Menu',
      path: 'UI\\Menus'
    }, tools);

    expect(sendAutomationRequest).toHaveBeenCalledWith('manage_widget_authoring', expect.objectContaining({
      subAction: 'create_widget_blueprint',
      path: '/Game/UI/Menus'
    }), expect.any(Object));
  });

  it('maps scalar layout aliases to native vector payloads', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await handleWidgetAuthoringTools('set_position', {
      action: 'set_position',
      widgetPath: '/Game/UI/WBP_Menu.WBP_Menu',
      slotName: 'TitleText',
      positionX: 80,
      positionY: 40
    }, tools);
    await handleWidgetAuthoringTools('set_size', {
      action: 'set_size',
      widgetPath: '/Game/UI/WBP_Menu.WBP_Menu',
      slotName: 'TitleText',
      sizeX: 420,
      sizeY: 72
    }, tools);

    expect(sendAutomationRequest).toHaveBeenNthCalledWith(1, 'manage_widget_authoring', expect.objectContaining({
      subAction: 'set_position',
      position: { x: 80, y: 40 }
    }), expect.any(Object));
    expect(sendAutomationRequest).toHaveBeenNthCalledWith(2, 'manage_widget_authoring', expect.objectContaining({
      subAction: 'set_size',
      size: { x: 420, y: 72 }
    }), expect.any(Object));
    const firstPayload = sendAutomationRequest.mock.calls[0]?.[1] ?? {};
    const secondPayload = sendAutomationRequest.mock.calls[1]?.[1] ?? {};
    expect(firstPayload).not.toHaveProperty('positionX');
    expect(firstPayload).not.toHaveProperty('positionY');
    expect(secondPayload).not.toHaveProperty('sizeX');
    expect(secondPayload).not.toHaveProperty('sizeY');
  });
});
