import { describe, expect, it, vi } from 'vitest';
import type { AutomationBridge } from '../../automation/index.js';
import type { ITools } from '../../types/tool-interfaces.js';
import { handleAnimationAuthoringTools } from './animation-authoring-handlers.js';

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

describe('handleAnimationAuthoringTools numeric normalization', () => {
  it('normalizes invalid sequence lengths before Unreal dispatch', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await handleAnimationAuthoringTools('create_animation_sequence', {
      action: 'create_animation_sequence',
      name: 'TestAnim',
      path: '/Game/Animations',
      numFrames: Number.NaN,
      frameRate: Number.POSITIVE_INFINITY
    }, tools);
    await handleAnimationAuthoringTools('set_sequence_length', {
      action: 'set_sequence_length',
      assetPath: '/Game/Animations/TestAnim',
      numFrames: 12.9,
      frameRate: -30
    }, tools);

    expect(sendAutomationRequest).toHaveBeenNthCalledWith(1, 'manage_animation_authoring', expect.objectContaining({
      subAction: 'create_animation_sequence',
      numFrames: 30,
      frameRate: 30
    }), {});
    expect(sendAutomationRequest).toHaveBeenNthCalledWith(2, 'manage_animation_authoring', expect.objectContaining({
      subAction: 'set_sequence_length',
      numFrames: 12,
      frameRate: undefined
    }), {});
  });

  it('normalizes frame and track indices before Unreal dispatch', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await handleAnimationAuthoringTools('add_notify', {
      action: 'add_notify',
      assetPath: '/Game/Animations/TestAnim',
      frame: 4.8,
      trackIndex: Number.NEGATIVE_INFINITY,
      notifyName: 'Footstep'
    }, tools);
    await handleAnimationAuthoringTools('add_notify_state', {
      action: 'add_notify_state',
      assetPath: '/Game/Animations/TestAnim',
      startFrame: -1,
      endFrame: 9.9,
      trackIndex: 2.7,
      notifyName: 'Window'
    }, tools);

    expect(sendAutomationRequest).toHaveBeenNthCalledWith(1, 'manage_animation_authoring', expect.objectContaining({
      subAction: 'add_notify',
      frame: 4,
      trackIndex: 0
    }), {});
    expect(sendAutomationRequest).toHaveBeenNthCalledWith(2, 'manage_animation_authoring', expect.objectContaining({
      subAction: 'add_notify_state',
      startFrame: 0,
      endFrame: 9,
      trackIndex: 2
    }), {});
  });
});
