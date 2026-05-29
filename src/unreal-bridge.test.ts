import { EventEmitter } from 'node:events';
import { afterEach, describe, expect, it, vi } from 'vitest';
import type { AutomationBridge } from './automation/index.js';
import { UnrealBridge } from './unreal-bridge.js';

function createDisconnectedAutomationBridge(): AutomationBridge {
  const emitter = new EventEmitter() as EventEmitter & { isConnected: () => boolean };
  emitter.isConnected = () => false;
  return emitter as unknown as AutomationBridge;
}

describe('UnrealBridge timeout env parsing', () => {
  const originalTimeout = process.env.UNREAL_CONNECTION_TIMEOUT;
  const originalMockMode = process.env.MOCK_UNREAL_CONNECTION;

  afterEach(() => {
    if (originalTimeout === undefined) {
      delete process.env.UNREAL_CONNECTION_TIMEOUT;
    } else {
      process.env.UNREAL_CONNECTION_TIMEOUT = originalTimeout;
    }
    if (originalMockMode === undefined) {
      delete process.env.MOCK_UNREAL_CONNECTION;
    } else {
      process.env.MOCK_UNREAL_CONNECTION = originalMockMode;
    }
    vi.restoreAllMocks();
  });

  it('rejects partial numeric connection timeout strings', async () => {
    process.env.UNREAL_CONNECTION_TIMEOUT = '5000ms';
    const bridge = new UnrealBridge();
    bridge.setAutomationBridge(createDisconnectedAutomationBridge());
    const connect = vi.spyOn(bridge, 'connect').mockResolvedValue(undefined);

    await bridge.tryConnect(1, 1234, 1);

    expect(connect).toHaveBeenCalledWith(1234);
  });

  it('accepts positive decimal integer connection timeout strings', async () => {
    process.env.UNREAL_CONNECTION_TIMEOUT = '5000';
    const bridge = new UnrealBridge();
    bridge.setAutomationBridge(createDisconnectedAutomationBridge());
    const connect = vi.spyOn(bridge, 'connect').mockResolvedValue(undefined);

    await bridge.tryConnect(1, 1234, 1);

    expect(connect).toHaveBeenCalledWith(5000);
  });

  it('executes safe console commands in mock mode without a configured bridge', async () => {
    process.env.MOCK_UNREAL_CONNECTION = 'true';
    const bridge = new UnrealBridge();

    const result = await bridge.executeConsoleCommand('stat fps');

    expect(result).toMatchObject({
      success: true,
      message: "Mock execution of 'stat fps' successful",
      transport: 'mock_bridge'
    });
    bridge.dispose();
  });

  it('keeps command validation active in mock mode', async () => {
    process.env.MOCK_UNREAL_CONNECTION = 'true';
    const bridge = new UnrealBridge();

    await expect(bridge.executeConsoleCommand('py print("unsafe")'))
      .rejects.toThrow(/Python console commands are blocked/);
    bridge.dispose();
  });
});
