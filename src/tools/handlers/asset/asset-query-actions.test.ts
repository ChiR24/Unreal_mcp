/**
 * Regression tests for Task 21 — asset get_source_control_state route divergence.
 *
 * RED (baseline characterization): before the repair, get_source_control_state
 * dispatched to the weaker `asset_query` subAction handler (single path only,
 * errors on disabled source control, flat response). GREEN after repair: it
 * routes to the canonical `manage_asset` action (array-capable, disabled-SC
 * tolerant, richer envelope), matching the registered manage_asset handler.
 */
import { afterEach, describe, expect, it, vi } from 'vitest';
import type { ITools } from '../../../types/tools/tool-interfaces.js';
import type { HandlerArgs } from '../../../types/handlers/handler-types.js';

vi.mock('../foundation/dispatch/common-handlers.js', () => ({
  executeAutomationRequest: vi.fn(),
}));

import { executeAutomationRequest } from '../foundation/dispatch/common-handlers.js';
import { handleSimpleQueryAction } from './asset-query-actions.js';
import { createAssetContext } from './asset-handler-types.js';

const mockedExecute = vi.mocked(executeAutomationRequest);

function makeTools(): ITools {
  return {
    automationBridge: { isConnected: () => true, sendAutomationRequest: vi.fn() },
  } as unknown as ITools;
}

afterEach(() => {
  vi.clearAllMocks();
});

describe('asset get_source_control_state route (Task 21)', () => {
  it('routes get_source_control_state to the canonical manage_asset action', async () => {
    const captured: { action: string; payload: Record<string, unknown> }[] = [];
    mockedExecute.mockImplementation(async (_tools: unknown, action: string, payload: HandlerArgs) => {
      captured.push({ action, payload: payload as Record<string, unknown> });
      return { success: true, result: { sourceControlEnabled: true, states: [] } };
    });

    await handleSimpleQueryAction('get_source_control_state', createAssetContext({ assetPath: '/Game/Asset', action: 'get_source_control_state' } as never, makeTools()));

    expect(captured).toHaveLength(1);
    expect(captured[0].action).toBe('manage_asset');
    expect(captured[0].action).not.toBe('asset_query');
    expect(captured[0].payload.subAction).toBe('get_source_control_state');
  });

  it('keeps analyze_graph mapped to get_asset_graph (dependency graph)', async () => {
    const captured: { action: string; payload: Record<string, unknown> }[] = [];
    mockedExecute.mockImplementation(async (_tools: unknown, action: string, payload: HandlerArgs) => {
      captured.push({ action, payload: payload as Record<string, unknown> });
      return { success: true, result: { graph: {} } };
    });

    await handleSimpleQueryAction('analyze_graph', createAssetContext({ assetPath: '/Game/Asset', action: 'analyze_graph' } as never, makeTools()));

    expect(captured).toHaveLength(1);
    expect(captured[0].action).toBe('get_asset_graph');
    expect(captured[0].payload.assetPath).toBe('/Game/Asset');
  });
});
