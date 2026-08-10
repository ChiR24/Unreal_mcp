/**
 * Regression tests for Task 21 — ragdoll reachability divergence.
 *
 * RED (baseline characterization): before the repair, both `setup_ragdoll`
 * and `activate_ragdoll` public verbs collapsed to the single native
 * `setup_ragdoll` action, so `activate_ragdoll` was advertised but
 * unreachable as a distinct toggle. GREEN after repair: `activate_ragdoll`
 * dispatches the distinct native `activate_ragdoll` action and the
 * capability record is a reachable canonical action.
 */
import { afterEach, describe, expect, it, vi } from 'vitest';
import type { ITools } from '../../../../types/tools/tool-interfaces.js';

vi.mock('../../foundation/dispatch/common-handlers.js', () => ({
  executeAutomationRequest: vi.fn(),
  cleanObject: (v: unknown) => v,
}));

import { executeAutomationRequest } from '../../foundation/dispatch/common-handlers.js';
import { tryHandleSpecialAnimationAction } from './animation-special-handlers.js';

const mockedExecute = vi.mocked(executeAutomationRequest);

function makeTools(): ITools {
  return {
    automationBridge: { isConnected: () => true, sendAutomationRequest: vi.fn() },
  } as unknown as ITools;
}

afterEach(() => {
  vi.clearAllMocks();
});

describe('ragdoll reachability (Task 21)', () => {
  it('routes activate_ragdoll to the distinct native activate_ragdoll action', async () => {
    const captured: string[] = [];
    mockedExecute.mockImplementation(async (_tools, action: string) => {
      captured.push(action);
      return { success: true, result: { message: 'ok', ragdollActive: true } };
    });

    await tryHandleSpecialAnimationAction('activate_ragdoll', { action: 'activate_ragdoll', actorName: 'Char_1', activate: true } as never, { action: 'activate_ragdoll', actorName: 'Char_1', activate: true } as never, makeTools());

    expect(captured).toContain('activate_ragdoll');
    expect(captured).not.toContain('setup_ragdoll');
  });

  it('routes setup_ragdoll to the native setup_ragdoll action', async () => {
    const captured: string[] = [];
    mockedExecute.mockImplementation(async (_tools, action: string) => {
      captured.push(action);
      return { success: true, result: { message: 'ok', ragdollActive: true } };
    });

    await tryHandleSpecialAnimationAction('setup_ragdoll', { action: 'setup_ragdoll', actorName: 'Char_1' } as never, { action: 'setup_ragdoll', actorName: 'Char_1' } as never, makeTools());

    expect(captured).toContain('setup_ragdoll');
    expect(captured).not.toContain('activate_ragdoll');
  });
});
