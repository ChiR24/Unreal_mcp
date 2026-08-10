import { describe, expect, it, vi } from 'vitest';
import type { AutomationRequestBridge, ITools } from '../../../types/tools/tool-interfaces.js';
import { handleConsoleCommand, handleSystemTools } from './system-handlers.js';

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

describe('handleSystemTools console-style actions', () => {
  it('toggles fps display through validated console commands', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    const result = await handleSystemTools('show_fps', { action: 'show_fps', enabled: false }, tools);

    expect(sendAutomationRequest).toHaveBeenCalledWith('console_command', { command: 'stat fps 0' }, { timeoutMs: expect.any(Number) });
    expect(result).toMatchObject({
      success: true,
      action: 'show_fps',
      message: 'FPS display disabled'
    });
  });

  it('maps quality names and categories to scalability CVars', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    const result = await handleSystemTools('set_quality', {
      action: 'set_quality',
      category: 'GlobalIllumination',
      level: 'cinematic'
    }, tools);

    expect(sendAutomationRequest).toHaveBeenCalledWith('console_command', {
      command: 'sg.GlobalIlluminationQuality 4'
    }, { timeoutMs: expect.any(Number) });
    expect(result).toMatchObject({
      success: true,
      action: 'set_quality'
    });
  });
});

describe('handleSystemTools widget and asset actions', () => {
  it('derives widget name and folder from widgetPath during creation', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools({ success: true, widgetPath: '/Game/UI/WBP_Menu' });

    const result = await handleSystemTools('create_widget', {
      action: 'create_widget',
      widgetPath: '/Game/UI/WBP_Menu',
      widgetType: 'UserWidget'
    }, tools);

    expect(sendAutomationRequest).toHaveBeenCalledWith('manage_widget_authoring', {
      action: 'create_widget',
      name: 'WBP_Menu',
      type: 'UserWidget',
      savePath: '/Game/UI',
      folder: '/Game/UI'
    }, { timeoutMs: expect.any(Number) });
    expect(result).toMatchObject({
      success: true,
      action: 'create_widget',
      widgetPath: '/Game/UI/WBP_Menu'
    });
  });

  it('promotes native validate_assets result arrays without fallback probes', async () => {
    const nativeResult = {
      success: true,
      result: {
        results: [{ assetPath: '/Game/M_Test', success: true }]
      }
    };
    const { tools, sendAutomationRequest } = createConnectedTools(nativeResult);

    const result = await handleSystemTools('validate_assets', {
      action: 'validate_assets',
      assetPath: '/Game/M_Test'
    }, tools);

    expect(sendAutomationRequest).toHaveBeenCalledTimes(1);
    expect(sendAutomationRequest).toHaveBeenCalledWith('system_control', {
      action: 'validate_assets',
      paths: ['/Game/M_Test'],
      recursive: undefined
    }, { timeoutMs: expect.any(Number) });
    expect(result).toMatchObject({
      success: true,
      action: 'validate_assets',
      results: [{ assetPath: '/Game/M_Test', success: true }]
    });
  });

  it('treats zero-volume play_sound as a handled no-op', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    const result = await handleSystemTools('play_sound', {
      action: 'play_sound',
      soundPath: '/Game/Sounds/S_Notify',
      volume: 0,
      pitch: 1.25
    }, tools);

    expect(sendAutomationRequest).not.toHaveBeenCalled();
    expect(result).toMatchObject({
      success: true,
      action: 'play_sound',
      handled: true,
      volume: 0,
      pitch: 1.25
    });
  });
});

describe('handleSystemTools window and screenshot actions', () => {
  it('routes game viewport screenshots through system_control with base64 enabled', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools({ success: true, image: 'base64-data' });

    const result = await handleSystemTools('screenshot', {
      action: 'screenshot',
      filename: 'GameShot',
      mode: 'game_viewport'
    }, tools);

    expect(sendAutomationRequest).toHaveBeenCalledWith('system_control', {
      action: 'screenshot',
      filename: 'GameShot',
      resolution: undefined,
      mode: 'game_viewport',
      returnBase64: true
    }, { timeoutMs: expect.any(Number) });
    expect(result).toMatchObject({
      success: true,
      action: 'screenshot',
      image: 'base64-data'
    });
  });

  it('rejects unsupported screenshot modes before dispatch', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    const result = await handleSystemTools('screenshot', {
      action: 'screenshot',
      mode: 'scene_capture'
    }, tools);

    expect(sendAutomationRequest).not.toHaveBeenCalled();
    expect(result).toMatchObject({
      success: false,
      error: 'INVALID_ARGUMENT',
      message: 'Unknown screenshot mode: scene_capture. Supported: editor_viewport, game_viewport, full_editor_window'
    });
  });

  it('toggles fullscreen mode when no resolution is provided', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    const result = await handleSystemTools('set_fullscreen', {
      action: 'set_fullscreen',
      enabled: false
    }, tools);

    expect(sendAutomationRequest).toHaveBeenCalledWith('console_command', {
      command: 'r.FullScreenMode 1'
    }, { timeoutMs: expect.any(Number) });
    expect(result).toMatchObject({
      success: true,
      action: 'set_fullscreen',
      handled: true
    });
  });
});

describe('handleConsoleCommand', () => {
  it('trims console commands before dispatching', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools({ success: true, command: 'stat fps' });

    const result = await handleConsoleCommand({ command: '  stat fps  ' }, tools);

    expect(sendAutomationRequest).toHaveBeenCalledWith('console_command', { command: 'stat fps' }, { timeoutMs: expect.any(Number) });
    expect(result).toMatchObject({
      success: true,
      command: 'stat fps'
    });
  });

  it('returns a structured error for empty console commands', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    const result = await handleConsoleCommand({ command: '   ' }, tools);

    expect(sendAutomationRequest).not.toHaveBeenCalled();
    expect(result).toMatchObject({
      success: false,
      error: 'EMPTY_COMMAND',
      command: '   '
    });
  });
});
