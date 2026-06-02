import { describe, expect, it, vi } from 'vitest';
import type { AutomationBridge } from '../../automation/index.js';
import type { ITools } from '../../types/tool-interfaces.js';
import { handleInputTools } from './input-handlers.js';

function createConnectedTools() {
  const sendAutomationRequest = vi.fn(async () => ({ success: true }));
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

describe('handleInputTools path normalization', () => {
  it('normalizes creation path aliases before dispatch', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await handleInputTools('create_input_action', {
      action: 'create_input_action',
      name: 'IA_Test',
      path: 'Game/MCPTest/Input'
    }, tools);

    expect(sendAutomationRequest).toHaveBeenCalledWith(
      'manage_input',
      expect.objectContaining({
        subAction: 'create_input_action',
        path: '/Game/MCPTest/Input'
      }),
      expect.objectContaining({ timeoutMs: expect.any(Number) })
    );
  });

  it('normalizes mapping asset path aliases before dispatch', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await handleInputTools('add_mapping', {
      action: 'add_mapping',
      contextPath: 'Game/MCPTest/Input/IMC_Test',
      actionPath: 'Content/MCPTest/Input/IA_Test',
      key: 'SpaceBar'
    }, tools);

    expect(sendAutomationRequest).toHaveBeenCalledWith(
      'manage_input',
      expect.objectContaining({
        subAction: 'add_mapping',
        contextPath: '/Game/MCPTest/Input/IMC_Test',
        actionPath: '/Game/MCPTest/Input/IA_Test'
      }),
      expect.objectContaining({ timeoutMs: expect.any(Number) })
    );
  });

  it('forwards legacy axis mappings without path validation', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await handleInputTools('add_legacy_axis_mapping', {
      action: 'add_legacy_axis_mapping',
      axisName: 'MoveForward',
      key: 'W',
      scale: 1
    }, tools);

    expect(sendAutomationRequest).toHaveBeenCalledWith(
      'manage_input',
      expect.objectContaining({
        subAction: 'add_legacy_axis_mapping',
        axisName: 'MoveForward',
        key: 'W',
        scale: 1
      }),
      expect.objectContaining({ timeoutMs: expect.any(Number) })
    );
  });
});
