import { describe, expect, it, vi } from 'vitest';
import { handleNetworkingTools } from './networking-handlers.js';
import type { ITools } from '../../../types/tools/tool-interfaces.js';
import { getTimeoutMs } from '../foundation/dispatch/common-handlers.js';

function createTools() {
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
    }
  };

  return { tools, sendAutomationRequest };
}

describe('handleNetworkingTools path normalization', () => {
  it('normalizes blueprint paths before dispatch', async () => {
    const { tools, sendAutomationRequest } = createTools();

    await handleNetworkingTools('set_property_replicated', {
      action: 'set_property_replicated',
      blueprintPath: 'Content\\Blueprints\\BP_NetActor',
      propertyName: 'Health'
    }, tools);

    expect(sendAutomationRequest).toHaveBeenCalledWith('manage_networking', {
      action: 'set_property_replicated',
      blueprintPath: '/Game/Blueprints/BP_NetActor',
      propertyName: 'Health',
      subAction: 'set_property_replicated'
    }, {
      timeoutMs: getTimeoutMs()
    });
  });

  it('honors per-call timeoutMs before dispatch', async () => {
    const { tools, sendAutomationRequest } = createTools();

    await handleNetworkingTools('set_property_replicated', {
      action: 'set_property_replicated',
      blueprintPath: '/Game/Blueprints/BP_NetActor',
      propertyName: 'Health',
      timeoutMs: 250000
    }, tools);

    expect(sendAutomationRequest).toHaveBeenCalledWith('manage_networking', {
      action: 'set_property_replicated',
      blueprintPath: '/Game/Blueprints/BP_NetActor',
      propertyName: 'Health',
      subAction: 'set_property_replicated'
    }, {
      timeoutMs: 250000
    });
  });

  it('dispatches get_networking_info for runtime actors without a blueprint path', async () => {
    const { tools, sendAutomationRequest } = createTools();

    await handleNetworkingTools('get_networking_info', {
      action: 'get_networking_info',
      actorName: 'MCP_NetworkTarget'
    }, tools);

    expect(sendAutomationRequest).toHaveBeenCalledWith('manage_networking', {
      action: 'get_networking_info',
      actorName: 'MCP_NetworkTarget',
      subAction: 'get_networking_info'
    }, {
      timeoutMs: getTimeoutMs()
    });
  });

  it('rejects get_networking_info before dispatch when no target is provided', async () => {
    const { tools, sendAutomationRequest } = createTools();

    const result = await handleNetworkingTools('get_networking_info', {
      action: 'get_networking_info'
    }, tools);

    expect(result).toMatchObject({
      success: false,
      error: 'INVALID_ARGUMENT',
      message: 'blueprintPath or actorName is required for get_networking_info'
    });
    expect(sendAutomationRequest).not.toHaveBeenCalled();
  });

});
