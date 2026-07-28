import { describe, expect, it, vi } from 'vitest';
import { AutomationBridge } from '../../../../automation/index.js';
import type { ITools } from '../../../../types/tools/tool-interfaces.js';
import { handleAnimationTools } from './animation-handlers.js';

type SendAutomationRequest = (
  action: string,
  payload: Record<string, unknown>,
  options?: { timeoutMs?: number }
) => Promise<Record<string, unknown>>;

function createTools(sendAutomationRequest: SendAutomationRequest) {
  const automationBridge = new AutomationBridge({ enabled: false });
  vi.spyOn(automationBridge, 'isConnected').mockReturnValue(true);
  const sendMock = vi.spyOn(automationBridge, 'sendAutomationRequest')
    .mockImplementation(async (action, payload, options) => sendAutomationRequest(action, payload ?? {}, options));

  const tools: ITools = {
    systemTools: {
      executeConsoleCommand: vi.fn(async () => ({ success: true })),
      getProjectSettings: vi.fn(async () => ({}))
    },
    assetResources: {
      list: vi.fn(async () => ({}))
    },
    automationBridge
  };

  return {
    tools,
    sendAutomationRequest: sendMock
  };
}

describe('handleAnimationTools payload mapping', () => {
  it('resolves a skeletal mesh component for animation blueprint creation', async () => {
    const { tools, sendAutomationRequest } = createTools(async (requestAction) => {
      if (requestAction === 'control_actor') {
        return {
          success: true,
          components: [{ type: 'SkeletalMeshComponent', path: '/Game/Characters/SK_Test' }]
        };
      }
      return { success: true };
    });

    await handleAnimationTools('create_anim_blueprint', {
      action: 'create_anim_blueprint',
      actorName: 'Hero',
      name: 'ABP_Hero',
      path: '/Game/Animations'
    }, tools);

    expect(sendAutomationRequest).toHaveBeenNthCalledWith(1, 'control_actor', {
      action: 'get_components',
      actorName: 'Hero'
    }, { timeoutMs: expect.any(Number) });
    expect(sendAutomationRequest).toHaveBeenNthCalledWith(2, 'create_animation_blueprint', expect.objectContaining({
      name: 'ABP_Hero',
      meshPath: '/Game/Characters/SK_Test',
      savePath: '/Game/Animations'
    }), { timeoutMs: expect.any(Number) });
  });

  it('falls back to explicit payload values when component lookup fails', async () => {
    const { tools, sendAutomationRequest } = createTools(async (requestAction) => {
      if (requestAction === 'control_actor') {
        throw new Error('lookup unavailable');
      }
      return { success: true };
    });

    await handleAnimationTools('create_anim_blueprint', {
      action: 'create_anim_blueprint',
      actorName: 'Hero',
      name: 'ABP_Hero',
      path: '/Game/Animations',
      skeletonPath: '/Game/Characters/SK_Test_Skeleton'
    }, tools);

    expect(sendAutomationRequest).toHaveBeenCalledWith('create_animation_blueprint', expect.objectContaining({
      name: 'ABP_Hero',
      skeletonPath: '/Game/Characters/SK_Test_Skeleton',
      savePath: '/Game/Animations'
    }), { timeoutMs: expect.any(Number) });
  });

  it('rejects invalid blend tree child animation paths before dispatch', async () => {
    const { tools, sendAutomationRequest } = createTools(async () => ({ success: true }));

    const result = await handleAnimationTools('create_blend_tree', {
      action: 'create_blend_tree',
      blueprintPath: '/Game/Animations/ABP_Hero',
      children: [{ animationPath: '/Game/../Invalid' }]
    }, tools);

    expect(result).toMatchObject({
      success: false,
      error: 'SECURITY_VIOLATION',
      message: 'Invalid animationPath in children: path traversal or illegal characters detected'
    });
    expect(sendAutomationRequest).not.toHaveBeenCalled();
  });
});
