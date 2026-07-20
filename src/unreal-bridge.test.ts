import { afterEach, describe, expect, it, vi } from 'vitest';
import { AutomationBridge } from './automation/index.js';
import { UnrealBridge } from './unreal-bridge.js';

type SendAutomationRequest = (
  action: string,
  payload: Record<string, unknown>,
  options?: { timeoutMs?: number }
) => Promise<Record<string, unknown>>;

function createDisconnectedAutomationBridge(): AutomationBridge {
  return new AutomationBridge({ enabled: false });
}

function createConnectedBridge(sendAutomationRequest: SendAutomationRequest) {
  const automationBridge = new AutomationBridge({ enabled: false });
  vi.spyOn(automationBridge, 'isConnected').mockReturnValue(true);
  const sendMock = vi
    .spyOn(automationBridge, 'sendAutomationRequest')
    .mockImplementation(async (action, payload, options) =>
      sendAutomationRequest(action, payload ?? {}, options));
  const bridge = new UnrealBridge();
  bridge.setAutomationBridge(automationBridge);
  return { bridge, sendMock };
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
      .rejects.toThrow(/Dangerous command blocked/);
    bridge.dispose();
  });

  it('normalizes object property reads from automation results', async () => {
    const { bridge, sendMock } = createConnectedBridge(async () => ({
      success: true,
      requestId: 'read-1',
      message: 'read',
      result: {
        value: 42,
        warnings: ['converted']
      }
    }));

    const result = await bridge.getObjectProperty({
      objectPath: '/Game/Actor.Actor',
      propertyName: 'Health',
      timeoutMs: 250
    });

    expect(sendMock).toHaveBeenCalledWith('get_object_property', {
      objectPath: '/Game/Actor.Actor',
      propertyName: 'Health'
    }, { timeoutMs: 250 });
    expect(result).toMatchObject({
      success: true,
      value: 42,
      propertyValue: 42,
      warnings: ['converted'],
      bridge: {
        requestId: 'read-1',
        success: true
      }
    });
    bridge.dispose();
  });

  it('passes object property write payloads through the automation bridge', async () => {
    const { bridge, sendMock } = createConnectedBridge(async () => ({
      success: true,
      requestId: 'write-1',
      result: {
        message: 'stored'
      }
    }));

    const result = await bridge.setObjectProperty({
      objectPath: '/Game/Actor.Actor',
      propertyName: 'Health',
      value: 100,
      markDirty: true
    });

    expect(sendMock).toHaveBeenCalledWith('set_object_property', {
      objectPath: '/Game/Actor.Actor',
      propertyName: 'Health',
      value: 100,
      markDirty: true
    }, undefined);
    expect(result).toMatchObject({
      success: true,
      message: 'stored',
      bridge: {
        requestId: 'write-1',
        success: true
      }
    });
    bridge.dispose();
  });

  it('filters empty batch console commands before one automation request', async () => {
    const { bridge, sendMock } = createConnectedBridge(async () => ({
      success: true,
      totalCommands: 2,
      executedCount: 2,
      failedCount: 0,
      results: [
        { command: 'stat fps', success: true },
        { command: 'stat unit', success: true }
      ]
    }));

    const result = await bridge.executeBatchConsoleCommands(
      ['stat fps', ' ', 'stat unit'],
      { timeoutMs: 123 }
    );

    expect(sendMock).toHaveBeenCalledWith('batch_console_commands', {
      commands: ['stat fps', 'stat unit']
    }, { timeoutMs: 123 });
    expect(result).toMatchObject({
      success: true,
      totalCommands: 2,
      executedCount: 2,
      failedCount: 0
    });
    bridge.dispose();
  });

  it('skips malformed sequential console command descriptors', async () => {
    process.env.MOCK_UNREAL_CONNECTION = 'true';
    const bridge = new UnrealBridge();

    const result = await bridge.executeConsoleCommands([
      {},
      { command: ' stat fps ' }
    ]);

    expect(result).toHaveLength(1);
    expect(result[0]).toMatchObject({
      success: true,
      message: "Mock execution of 'stat fps' successful"
    });
    bridge.dispose();
  });

  it('flattens successful editor function result payloads', async () => {
    const { bridge, sendMock } = createConnectedBridge(async () => ({
      success: true,
      result: {
        status: 'done',
        count: 3
      }
    }));

    const result = await bridge.executeEditorFunction('Refresh', { force: true }, { timeoutMs: 50 });

    expect(sendMock).toHaveBeenCalledWith('execute_editor_function', {
      functionName: 'Refresh',
      params: {
        force: true
      }
    }, { timeoutMs: 50 });
    expect(result).toMatchObject({
      success: true,
      status: 'done',
      count: 3
    });
    bridge.dispose();
  });

  it('returns safe engine version defaults when automation fails', async () => {
    const { bridge } = createConnectedBridge(async () => {
      throw new Error('offline');
    });

    const result = await bridge.getEngineVersion();

    expect(result).toEqual({
      version: 'unknown',
      major: 0,
      minor: 0,
      patch: 0,
      isUE56OrAbove: false
    });
    bridge.dispose();
  });
});
