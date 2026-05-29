import { describe, expect, it, vi } from 'vitest';
import type { AutomationBridge } from '../../automation/index.js';
import type { ITools } from '../../types/tool-interfaces.js';
import { handleInteractionTools } from './interaction-handlers.js';

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

describe('handleInteractionTools creation paths', () => {
  it('normalizes folder aliases before Unreal dispatch', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await handleInteractionTools('create_interactable_interface', {
      action: 'create_interactable_interface',
      name: 'BPI_TestInteractable',
      folder: 'Game/MCPTest/Interaction'
    }, tools);

    expect(sendAutomationRequest).toHaveBeenCalledWith(
      'manage_interaction',
      expect.objectContaining({
        action: 'create_interactable_interface',
        subAction: 'create_interactable_interface',
        folder: '/Game/MCPTest/Interaction'
      }),
      expect.objectContaining({ timeoutMs: expect.any(Number) })
    );
  });
});
