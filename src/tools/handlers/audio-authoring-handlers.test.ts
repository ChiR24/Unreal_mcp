import { describe, expect, it, vi } from 'vitest';
import type { AutomationBridge } from '../../automation/index.js';
import type { ITools } from '../../types/tool-interfaces.js';
import { handleAudioAuthoringTools } from './audio-authoring-handlers.js';

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

describe('handleAudioAuthoringTools payload mapping', () => {
  it('normalizes creation path aliases before dispatch', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await handleAudioAuthoringTools('create_metasound', {
      action: 'create_metasound',
      name: 'TestMetaSound',
      path: 'Game/MCPTest/Audio'
    }, tools);

    expect(sendAutomationRequest).toHaveBeenCalledWith('manage_audio_authoring', expect.objectContaining({
      subAction: 'create_metasound',
      path: '/Game/MCPTest/Audio'
    }), expect.any(Object));
  });

  it('maps spatialization alias without overriding explicit spatialize false', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await handleAudioAuthoringTools('configure_spatialization', {
      action: 'configure_spatialization',
      assetPath: '/Game/Audio/TestAttenuation',
      spatialization: 'HRTF',
      spatialize: false
    }, tools);

    expect(sendAutomationRequest).toHaveBeenCalledWith('manage_audio_authoring', expect.objectContaining({
      subAction: 'configure_spatialization',
      assetPath: '/Game/Audio/TestAttenuation.TestAttenuation',
      spatializationAlgorithm: 'HRTF',
      spatialize: false
    }), expect.any(Object));
    const payload = sendAutomationRequest.mock.calls[0]?.[1] ?? {};
    expect(payload).not.toHaveProperty('spatialization');
  });

  it('maps boolean spatialization alias to spatialize', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await handleAudioAuthoringTools('configure_spatialization', {
      action: 'configure_spatialization',
      assetPath: '/Game/Audio/TestAttenuation',
      spatialization: false
    }, tools);

    expect(sendAutomationRequest).toHaveBeenCalledWith('manage_audio_authoring', expect.objectContaining({
      subAction: 'configure_spatialization',
      spatialize: false
    }), expect.any(Object));
    const payload = sendAutomationRequest.mock.calls[0]?.[1] ?? {};
    expect(payload).not.toHaveProperty('spatialization');
  });
});
