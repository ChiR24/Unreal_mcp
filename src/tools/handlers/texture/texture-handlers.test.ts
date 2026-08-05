import { describe, expect, it, vi } from 'vitest';
import type { AutomationRequestBridge, ITools } from '../../../types/tools/tool-interfaces.js';
import { handleTextureTools } from './texture-handlers.js';

type SendAutomationRequest = AutomationRequestBridge['sendAutomationRequest'];

function createConnectedTools(result: Record<string, unknown> = { success: true }) {
  const sendAutomationRequest = vi.fn<SendAutomationRequest>(async () => result);
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

describe('handleTextureTools dispatch', () => {
  it('derives texture name and folder from texturePath aliases', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools({ success: true });

    const result = await handleTextureTools('create_texture', {
      action: 'create_texture',
      texturePath: '/Game/Textures/T_Rock',
      width: 64,
      height: 32,
      save: false
    }, tools);

    expect(sendAutomationRequest).toHaveBeenCalledWith('manage_texture', {
      subAction: 'create_noise_texture',
      name: 'T_Rock',
      path: '/Game/Textures',
      noiseType: 'Perlin',
      width: 64,
      height: 32,
      scale: 1,
      octaves: 4,
      persistence: 0.5,
      lacunarity: 2,
      seed: 0,
      seamless: false,
      hdr: false,
      save: false
    }, { timeoutMs: expect.any(Number) });
    expect(result).toMatchObject({
      success: true,
      message: "Noise texture 'T_Rock' created"
    });
  });

  it('maps compatibility setting aliases before Unreal dispatch', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools({ success: true });

    await handleTextureTools('set_texture_filter', {
      action: 'set_texture_filter',
      texturePath: '/Game/Textures/T_Rock',
      filter: 'Nearest'
    }, tools);

    await handleTextureTools('set_texture_wrap', {
      action: 'set_texture_wrap',
      texturePath: '/Game/Textures/T_Rock',
      wrap: 'Clamp'
    }, tools);

    expect(sendAutomationRequest).toHaveBeenNthCalledWith(1, 'manage_texture', {
      subAction: 'set_texture_filter',
      assetPath: '/Game/Textures/T_Rock',
      filter: 'Nearest'
    }, { timeoutMs: expect.any(Number) });
    expect(sendAutomationRequest).toHaveBeenNthCalledWith(2, 'manage_texture', {
      subAction: 'set_texture_wrap',
      assetPath: '/Game/Textures/T_Rock',
      wrapMode: 'Clamp'
    }, { timeoutMs: expect.any(Number) });
  });

  it('preserves native error envelopes from texture operations', async () => {
    const { tools } = createConnectedTools({
      success: false,
      error: 'texture_error',
      errorCode: 'TEXTURE_ERROR',
      message: 'native failure'
    });

    const result = await handleTextureTools('get_texture_info', {
      action: 'get_texture_info',
      assetPath: '/Game/Textures/Missing'
    }, tools);

    expect(result).toMatchObject({
      success: false,
      message: 'texture_error',
      data: null
    });
  });

  it('reports unknown texture actions without touching Unreal', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools({ success: true });

    const result = await handleTextureTools('unknown_texture_action', {
      action: 'unknown_texture_action'
    }, tools);

    expect(sendAutomationRequest).not.toHaveBeenCalled();
    expect(result).toMatchObject({
      success: false,
      error: 'UNKNOWN_ACTION',
      message: 'Unknown texture action: unknown_texture_action'
    });
  });
});
