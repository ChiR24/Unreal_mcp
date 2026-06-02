import { describe, expect, it, vi } from 'vitest';
import type { AutomationBridge } from '../../automation/index.js';
import type { ITools } from '../../types/tool-interfaces.js';
import { handleLevelStructureTools } from './level-structure-handlers.js';

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

describe('handleLevelStructureTools path normalization', () => {
  it('normalizes create_sublevel path aliases before Unreal dispatch', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await handleLevelStructureTools('create_sublevel', {
      action: 'create_sublevel',
      sublevelName: 'Sublevel_A',
      sublevelPath: 'Game/MCPTest/World/Sublevel_A',
      parentLevel: 'Content/MCPTest/World/Persistent'
    }, tools);

    expect(sendAutomationRequest).toHaveBeenCalledWith('manage_level_structure', expect.objectContaining({
      subAction: 'create_sublevel',
      sublevelPath: '/Game/MCPTest/World/Sublevel_A',
      parentLevel: '/Game/MCPTest/World/Persistent'
    }), expect.objectContaining({ timeoutMs: expect.any(Number) }));
  });

  it('normalizes path-like streaming levelName aliases before Unreal dispatch', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await handleLevelStructureTools('configure_level_streaming', {
      action: 'configure_level_streaming',
      levelName: 'Game/MCPTest/World/Sublevel_A',
      streamingMethod: 'Blueprint'
    }, tools);

    expect(sendAutomationRequest).toHaveBeenCalledWith('manage_level_structure', expect.objectContaining({
      subAction: 'configure_level_streaming',
      levelName: '/Game/MCPTest/World/Sublevel_A'
    }), expect.objectContaining({ timeoutMs: expect.any(Number) }));
  });

  it('preserves non-path streaming level names before Unreal dispatch', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await handleLevelStructureTools('set_streaming_distance', {
      action: 'set_streaming_distance',
      levelName: 'Sublevel_A',
      streamingDistance: 5000
    }, tools);

    expect(sendAutomationRequest).toHaveBeenCalledWith('manage_level_structure', expect.objectContaining({
      subAction: 'set_streaming_distance',
      levelName: 'Sublevel_A'
    }), expect.objectContaining({ timeoutMs: expect.any(Number) }));
  });
});
