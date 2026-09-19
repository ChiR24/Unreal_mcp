import { beforeEach, describe, expect, it, vi } from 'vitest';

const { executeAutomationRequestMock, executeBatchConsoleCommandsMock } = vi.hoisted(() => ({
  executeAutomationRequestMock: vi.fn(async (..._args: unknown[]): Promise<Record<string, unknown>> => ({ success: true, result: {} })),
  executeBatchConsoleCommandsMock: vi.fn(async (..._args: unknown[]): Promise<Record<string, unknown>> => ({ success: true }))
}));

vi.mock('../foundation/dispatch/common-handlers.js', async () => {
  const actual = await vi.importActual<typeof import('../foundation/dispatch/common-handlers.js')>('../foundation/dispatch/common-handlers.js');
  return {
    ...actual,
    executeAutomationRequest: executeAutomationRequestMock,
    executeBatchConsoleCommands: executeBatchConsoleCommandsMock
  };
});

import { handleLightingTools } from './lighting-handlers.js';
import { TOOL_ACTIONS } from '../../../utils/commands/action-constants.js';

describe('handleLightingTools', () => {
  beforeEach(() => {
    executeAutomationRequestMock.mockClear();
    executeBatchConsoleCommandsMock.mockClear();
  });

  it('dispatches generic create_light to the normalized point light payload', async () => {
    await handleLightingTools('create_light', {
      lightType: 'PointLight',
      name: 'KeyLight',
      location: [10, 20, 30],
      intensity: 750,
      radius: 600,
      color: [1, 0.5, 0.25],
      castShadows: true
    }, {} as never);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      TOOL_ACTIONS.SPAWN_LIGHT,
      expect.objectContaining({
        lightClass: 'PointLight',
        name: 'KeyLight',
        location: { x: 10, y: 20, z: 30 },
        properties: expect.objectContaining({
          intensity: 750,
          attenuationRadius: 600,
          color: { r: 1, g: 0.5, b: 0.25, a: 1 },
          castShadows: true
        })
      }),
      'Automation bridge not available for light spawning'
    );
  });

  it('falls back to console commands when global illumination bridge dispatch is unavailable', async () => {
    executeAutomationRequestMock.mockResolvedValueOnce({
      success: false,
      error: 'Connection not available'
    });

    const result = await handleLightingTools('setup_global_illumination', {
      method: 'lumen',
      quality: 'Epic',
      indirectLightingIntensity: 2,
      bounces: 4
    }, {} as never);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      TOOL_ACTIONS.SETUP_GLOBAL_ILLUMINATION,
      {
        method: 'LumenGI',
        quality: 'Epic',
        indirectLightingIntensity: 2,
        bounces: 4
      }
    );
    expect(executeBatchConsoleCommandsMock).toHaveBeenCalledWith(
      {},
      [
        'r.DynamicGlobalIlluminationMethod 1',
        'r.Lumen.Quality 3',
        'r.IndirectLightingIntensity 2',
        'r.Lumen.MaxReflectionBounces 4'
      ]
    );
    expect(result).toEqual({ success: true, message: 'Global illumination configured (console)' });
  });
});
