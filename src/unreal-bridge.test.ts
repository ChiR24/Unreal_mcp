import { afterEach, describe, expect, it, vi } from 'vitest';
import { AutomationBridge } from './automation/index.js';
import { UnrealBridge } from './unreal-bridge.js';

function connectedBridge(reply: unknown) {
  const automationBridge = new AutomationBridge({ enabled: false });
  vi.spyOn(automationBridge, 'isConnected').mockReturnValue(true);
  const send = vi.spyOn(automationBridge, 'sendAutomationRequest').mockResolvedValue(reply as never);
  const bridge = new UnrealBridge();
  bridge.setAutomationBridge(automationBridge);
  return { bridge, send };
}

afterEach(() => {
  vi.unstubAllEnvs();
  vi.restoreAllMocks();
});

describe('UnrealBridge', () => {
  it('answers safe console commands locally in mock mode', async () => {
    vi.stubEnv('MOCK_UNREAL_CONNECTION', 'true');
    await expect(new UnrealBridge().executeConsoleCommand(' stat fps ')).resolves.toMatchObject({
      success: true,
      message: "Mock execution of 'stat fps' successful"
    });
  });

  it('keeps command validation active in mock mode', async () => {
    vi.stubEnv('MOCK_UNREAL_CONNECTION', 'true');
    await expect(new UnrealBridge().executeConsoleCommand('py print("unsafe")')).rejects.toThrow(/Dangerous command blocked/);
  });

  it('sends a trimmed console_command and surfaces a failed reply as an error', async () => {
    const { bridge, send } = connectedBridge({ success: false, message: 'no such command' });

    await expect(bridge.executeConsoleCommand(' stat unit ')).rejects.toThrow('no such command');
    expect(send).toHaveBeenCalledWith('console_command', { command: 'stat unit' }, expect.objectContaining({ timeoutMs: expect.any(Number) }));
  });

  it('reads the engine version from inspect get_project_settings', async () => {
    const version = '5.7.4-46000000+++UE5+Release-5.7';
    const { bridge, send } = connectedBridge({ success: true, result: { engineVersion: version } });

    await expect(bridge.getEngineVersion()).resolves.toEqual({ version, major: 5, minor: 7, patch: 4, isUE56OrAbove: true });
    expect(send).toHaveBeenCalledWith('inspect', { action: 'get_project_settings' }, expect.objectContaining({ timeoutMs: expect.any(Number) }));
  });

  it('reports the editor subsystems present exactly when connected', () => {
    const { bridge } = connectedBridge({});
    expect(bridge.getFeatureFlags()).toEqual({ subsystems: { unrealEditor: true, levelEditor: true, editorActor: true } });
    expect(new UnrealBridge().getFeatureFlags()).toEqual({ subsystems: { unrealEditor: false, levelEditor: false, editorActor: false } });
  });

  it('delegates tryConnect to the automation bridge when disconnected', async () => {
    const automationBridge = new AutomationBridge({ enabled: false });
    const connect = vi.spyOn(automationBridge, 'connect').mockResolvedValue(false);
    const bridge = new UnrealBridge();
    bridge.setAutomationBridge(automationBridge);

    await expect(bridge.tryConnect()).resolves.toBe(false);
    expect(connect).toHaveBeenCalledOnce();
  });
});
