import { describe, expect, it, vi } from 'vitest';
import type { AutomationBridge } from '../../automation/index.js';
import type { ITools } from '../../types/tool-interfaces.js';
import { handleGASTools } from './gas-handlers.js';

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

describe('handleGASTools gameplay effect creation', () => {
  it('preserves duration payload fields while sanitizing the asset name and path', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await handleGASTools('create_gameplay_effect', {
      action: 'create_gameplay_effect',
      name: ' GE Bad Name ',
      path: 'MCP Test//Effects',
      durationType: 'HasDuration',
      duration: 5,
      period: 1
    }, tools);

    expect(sendAutomationRequest).toHaveBeenCalledWith(
      'manage_gas',
      expect.objectContaining({
        action: 'create_gameplay_effect',
        subAction: 'create_gameplay_effect',
        name: 'GE_Bad_Name',
        path: '/Game/MCP_Test/Effects',
        durationType: 'has_duration',
        duration: 5,
        period: 1
      }),
      expect.objectContaining({ timeoutMs: expect.any(Number) })
    );
  });
});
