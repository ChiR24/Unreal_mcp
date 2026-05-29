import { describe, expect, it, vi } from 'vitest';
import { handleLevelTools } from './level-handlers.js';
import type { ITools } from '../../types/tool-interfaces.js';

function createTools() {
  const sendAutomationRequest = vi.fn(async () => ({ success: true }));
  const tools = {
    automationBridge: {
      isConnected: () => true,
      sendAutomationRequest
    }
  } as unknown as ITools;

  return { tools, sendAutomationRequest };
}

describe('handleLevelTools path normalization', () => {
  it('normalizes snake_case Content paths before dispatch', async () => {
    const { tools, sendAutomationRequest } = createTools();

    await handleLevelTools('save_as', { action: 'save_as', save_path: 'Content\\Maps\\Demo' }, tools);

    expect(sendAutomationRequest).toHaveBeenCalledWith('manage_level', {
      action: 'save_level_as',
      savePath: '/Game/Maps/Demo'
    }, {});
  });

  it('validates normalized /Content level paths before dispatch', async () => {
    const { tools, sendAutomationRequest } = createTools();

    await handleLevelTools('load', { action: 'load', levelPath: '/Content/Maps/Demo' }, tools);

    expect(sendAutomationRequest).toHaveBeenCalledWith('manage_level', {
      action: 'load',
      levelPath: '/Game/Maps/Demo',
      streaming: false
    }, {});
  });

  it('maps DirectionalLight spawn_light requests to the directional light class path', async () => {
    const { tools, sendAutomationRequest } = createTools();

    await handleLevelTools('spawn_light', {
      action: 'spawn_light',
      lightType: 'DirectionalLight',
      name: 'Sun'
    }, tools);

    expect(sendAutomationRequest).toHaveBeenCalledWith(
      'control_actor',
      expect.objectContaining({
        action: 'spawn',
        actorName: 'Sun',
        classPath: '/Script/Engine.DirectionalLight'
      }),
      {}
    );
  });

  it('keeps World Partition actions out of manage_level dispatch', async () => {
    const { tools, sendAutomationRequest } = createTools();

    const result = await handleLevelTools('load_cells', {
      action: 'load_cells',
      levelPath: String.raw`Content\Maps\Demo`,
      cells: ['Cell_0'],
      customFlag: true
    }, tools);

    expect(sendAutomationRequest).not.toHaveBeenCalled();
    expect(result).toMatchObject({
      success: false,
      error: 'INVALID_ACTION',
      action: 'load_cells',
      tool: 'manage_level_structure'
    });
  });

  it('normalizes delete_level path aliases and level path arrays before dispatch', async () => {
    const { tools, sendAutomationRequest } = createTools();

    await handleLevelTools('delete_level', { action: 'delete_level', path: 'Content\\Maps\\Demo' }, tools);
    await handleLevelTools('delete', {
      action: 'delete',
      level_paths: ['Content\\Maps\\A', '/Content/Maps/B']
    }, tools);

    expect(sendAutomationRequest).toHaveBeenNthCalledWith(1, 'manage_level', {
      action: 'delete_level',
      levelPath: '/Game/Maps/Demo'
    }, {});
    expect(sendAutomationRequest).toHaveBeenNthCalledWith(2, 'manage_level', {
      action: 'delete_level',
      levelPath: '/Game/Maps/A'
    }, {});
    expect(sendAutomationRequest).toHaveBeenNthCalledWith(3, 'manage_level', {
      action: 'delete_level',
      levelPath: '/Game/Maps/B'
    }, {});
  });
});
