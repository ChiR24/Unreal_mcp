import { describe, expect, it, vi } from 'vitest';
import type { AutomationBridge } from '../../automation/index.js';
import type { ITools } from '../../types/tool-interfaces.js';
import { handleInventoryTools } from './inventory-handlers.js';

function createConnectedTools(result: Record<string, unknown> = { success: true }) {
  const sendAutomationRequest = vi.fn(async (
    _toolName: string,
    _payload: Record<string, unknown>,
    _options?: Record<string, unknown>
  ) => result);
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

describe('handleInventoryTools creation paths', () => {
  it('normalizes path aliases before Unreal dispatch', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await handleInventoryTools('create_item_data_asset', {
      action: 'create_item_data_asset',
      name: 'DA_TestItem',
      path: 'Game/MCPTest/Inventory'
    }, tools);

    expect(sendAutomationRequest).toHaveBeenCalledWith(
      'manage_inventory',
      expect.objectContaining({
        action: 'create_item_data_asset',
        subAction: 'create_item_data_asset',
        path: '/Game/MCPTest/Inventory'
      }),
      expect.objectContaining({ timeoutMs: expect.any(Number) })
    );
  });
});
