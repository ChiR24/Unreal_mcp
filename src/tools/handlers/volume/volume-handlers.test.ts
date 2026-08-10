/**
 * Regression tests for Task 21 — set_volume_bounds route divergence.
 *
 * RED (baseline characterization): before the repair, `set_volume_bounds`
 * silently collapsed to `set_volume_extent` (a 3-value extent), dropping the
 * native six-value min/max bounds contract. GREEN after repair: the handler
 * dispatches the exact `set_volume_bounds` subAction so the native
 * HandleSetVolumeBounds (six-value contract) is actually reached.
 */
import { afterEach, describe, expect, it, vi } from 'vitest';
import type { ITools } from '../../../types/tools/tool-interfaces.js';
import { handleVolumeTools } from './volume-handlers.js';

vi.mock('../foundation/dispatch/common-handlers.js', () => ({
  createSubActionDispatcher: vi.fn(),
  executeAutomationRequest: vi.fn(),
}));

import { createSubActionDispatcher, executeAutomationRequest } from '../foundation/dispatch/common-handlers.js';

const mockedCreate = vi.mocked(createSubActionDispatcher);
const mockedExecute = vi.mocked(executeAutomationRequest);

function makeTools(): ITools {
  return {
    automationBridge: {
      isConnected: () => true,
      sendAutomationRequest: vi.fn(),
    },
  } as unknown as ITools;
}

afterEach(() => {
  vi.clearAllMocks();
});

describe('set_volume_bounds route divergence (Task 21)', () => {
  it('dispatches the distinct set_volume_bounds subAction, not set_volume_extent', async () => {
    const captured: { subAction: string; payload: Record<string, unknown> }[] = [];
    mockedCreate.mockImplementation((_tools, _args, _options) => ({
      argsRecord: {},
      async sendRequest(subAction: string, extraPayload: Record<string, unknown> = {}) {
        captured.push({ subAction, payload: extraPayload });
        return { success: true, message: 'ok' };
      },
    }));
    mockedExecute.mockResolvedValue({ success: true } as never);

    await handleVolumeTools('set_volume_bounds', {
      volumeName: 'PP_01',
      bounds: { origin: { x: 0, y: 0, z: 0 }, extent: { x: 1000, y: 1000, z: 500 } },
    } as never, makeTools());

    expect(captured).toHaveLength(1);
    expect(captured[0].subAction).toBe('set_volume_bounds');
    expect(captured[0].subAction).not.toBe('set_volume_extent');
  });

  it('still dispatches set_volume_extent for the extent action', async () => {
    const captured: string[] = [];
    mockedCreate.mockImplementation((_t, _a, _o) => ({
      argsRecord: {},
      async sendRequest(subAction: string) {
        captured.push(subAction);
        return { success: true };
      },
    }));

    await handleVolumeTools('set_volume_extent', {
      volumeName: 'PP_01',
      extent: { x: 1000, y: 1000, z: 500 },
    } as never, makeTools());

    expect(captured).toEqual(['set_volume_extent']);
  });
});
