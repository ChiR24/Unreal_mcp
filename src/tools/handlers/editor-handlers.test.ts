import { describe, expect, it, vi } from 'vitest';
import type { AutomationBridge } from '../../automation/index.js';
import type { ITools } from '../../types/tool-interfaces.js';
import { handleEditorTools } from './editor-handlers.js';

function createConnectedTools() {
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
    } as unknown as AutomationBridge
  };

  return { tools, sendAutomationRequest };
}

describe('handleEditorTools', () => {
  it('exposes all supported simulate_input parameters in the public schemas', async () => {
    const { consolidatedToolDefinitions } = await import('../consolidated-tool-definitions.js');
    const { coreToolDefinitions } = await import('../schemas/core-tools.js');
    const tools = [
      consolidatedToolDefinitions.find((tool) => tool.name === 'control_editor'),
      coreToolDefinitions.find((tool) => tool.name === 'control_editor')
    ];

    for (const tool of tools) {
      const properties = (tool?.inputSchema as Record<string, unknown> & {
        properties: Record<string, unknown>;
      }).properties;

      expect(properties).toHaveProperty('type');
      expect(properties).toHaveProperty('inputType');
      expect(properties).toHaveProperty('inputAction');
      expect(properties).toHaveProperty('x');
      expect(properties).toHaveProperty('y');
      expect(properties).toHaveProperty('button');
      expect(properties).toHaveProperty('mode');
      expect(properties).toHaveProperty('returnBase64');
    }
  });

  it('allows screenshot without a filename so Unreal can generate one', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await handleEditorTools('screenshot', { action: 'screenshot' }, tools);

    expect(sendAutomationRequest).toHaveBeenCalledWith('control_editor', {
      action: 'screenshot',
      filename: undefined,
      resolution: undefined
    }, {});
  });

  it('requests image data for full editor window screenshots', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await handleEditorTools('screenshot', { action: 'screenshot', filename: 'FullEditor', mode: 'full_editor_window' }, tools);

    expect(sendAutomationRequest).toHaveBeenCalledWith('control_editor', {
      action: 'screenshot',
      filename: 'FullEditor',
      resolution: undefined,
      mode: 'full_editor_window',
      returnBase64: true
    }, {});
  });

  it('routes game viewport screenshots to the game viewport capture path', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await handleEditorTools('screenshot', { action: 'screenshot', filename: 'GameViewport', mode: 'game_viewport' }, tools);

    expect(sendAutomationRequest).toHaveBeenCalledWith('system_control', {
      action: 'screenshot',
      filename: 'GameViewport',
      resolution: undefined,
      mode: 'game_viewport',
      returnBase64: true
    }, {});
  });

  it('returns string action for invalid screenshot modes', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    const result = await handleEditorTools('screenshot', { action: 'screenshot', mode: 'bad_mode' }, tools);

    expect(result).toMatchObject({
      success: false,
      type: 'INVALID_ARGUMENT',
      error: 'INVALID_ARGUMENT',
      action: 'screenshot'
    });
    expect(sendAutomationRequest).not.toHaveBeenCalled();
  });

  it('maps simulate_input from inputAction without reading the routing action', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await handleEditorTools('simulate_input', { action: 'simulate_input', inputAction: 'pressed', key: 'K' }, tools);

    expect(sendAutomationRequest).toHaveBeenCalledWith('control_editor', {
      action: 'simulate_input',
      type: 'key_down',
      key: 'K',
      x: undefined,
      y: undefined,
      button: undefined
    }, {});
  });

  it('rejects simulate_input when only the routing action is present', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await expect(handleEditorTools('simulate_input', { action: 'simulate_input', key: 'K' }, tools))
      .rejects.toThrow('type|inputType|inputAction');
    expect(sendAutomationRequest).not.toHaveBeenCalled();
  });
});
