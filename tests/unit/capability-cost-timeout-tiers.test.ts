// tests/unit/capability-cost-timeout-tiers.test.ts
//
// Task 45 (timeout tiers): a request's deadline must be DERIVED from the
// capability's declared cost class, not from one flat number shared by every
// action. A cheap read (`inspect::inspect_object`, cost instant|low) and a
// destructive asset delete (`manage_asset::delete_asset`, cost
// long-running|high) are three orders of magnitude apart in real cost, so a
// single budget is wrong in both directions at once: it either kills the
// expensive action early or makes the cheap one hang far past the point the
// caller could have been told the editor is gone.
//
// Everything here is a pure function of the committed capability records and a
// captured call argument. There is no timer, no sleep and no wall-clock read,
// so the assertions cannot flake: the same records always produce the same
// numbers.

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import {
  CAPABILITY_TIMEOUT_TIER_MS,
  MIN_CAPABILITY_TIMEOUT_MS,
} from '../../src/config.js';
import {
  executeAutomationRequest,
  resolveActionTimeoutMs,
  resolveCostTimeoutMs,
} from '../../src/tools/handlers/foundation/dispatch/common-handlers.js';
import type { ITools } from '../../src/types/tools/tool-interfaces.js';

const LATENCIES = ['instant', 'interactive', 'long-running'] as const;
const RESOURCES = ['low', 'medium', 'high'] as const;

type CapturedOptions = { timeoutMs?: number } | undefined;

function createRecordingTools(): {
  tools: ITools;
  timeoutFor: (tool: string, action: string) => number | undefined;
} {
  const captured = new Map<string, CapturedOptions>();
  const sendAutomationRequest = vi.fn(
    async (tool: string, args: Record<string, unknown>, options?: CapturedOptions) => {
      captured.set(`${tool}::${String(args.action)}`, options);
      return { success: true };
    },
  );

  const tools = {
    automationBridge: {
      isConnected: () => true,
      sendAutomationRequest,
    },
  } as unknown as ITools;

  return {
    tools,
    timeoutFor: (tool, action) => captured.get(`${tool}::${action}`)?.timeoutMs,
  };
}

describe('Task 45 capability-cost timeout tiers', () => {
  const savedCanonical = process.env.MCP_REQUEST_TIMEOUT_MS;
  const savedLegacy = process.env.MCP_AUTOMATION_REQUEST_TIMEOUT_MS;

  beforeEach(() => {
    // Tier derivation is what is under test; an operator override would mask it.
    delete process.env.MCP_REQUEST_TIMEOUT_MS;
    delete process.env.MCP_AUTOMATION_REQUEST_TIMEOUT_MS;
  });

  afterEach(() => {
    if (savedCanonical === undefined) delete process.env.MCP_REQUEST_TIMEOUT_MS;
    else process.env.MCP_REQUEST_TIMEOUT_MS = savedCanonical;
    if (savedLegacy === undefined) delete process.env.MCP_AUTOMATION_REQUEST_TIMEOUT_MS;
    else process.env.MCP_AUTOMATION_REQUEST_TIMEOUT_MS = savedLegacy;
  });

  it('gives a cheap read a strictly smaller budget than a long-running mutation', async () => {
    const { tools, timeoutFor } = createRecordingTools();

    // inspect::inspect_object -> cost instant|low
    await executeAutomationRequest(tools, 'inspect', {
      action: 'inspect_object',
      objectPath: '/Game/Example',
    });
    // manage_asset::delete_asset -> cost long-running|high
    await executeAutomationRequest(tools, 'manage_asset', {
      action: 'delete_asset',
      assetPath: '/Game/Example',
    });

    const cheapRead = timeoutFor('inspect', 'inspect_object');
    const expensiveDelete = timeoutFor('manage_asset', 'delete_asset');

    expect(cheapRead).toBeTypeOf('number');
    expect(expensiveDelete).toBeTypeOf('number');
    // The whole point of the tier: these must NOT be the same number.
    expect(expensiveDelete).toBeGreaterThan(cheapRead as number);
  });

  it('resolves each declared cost class to its own deterministic tier', () => {
    expect(resolveCostTimeoutMs({ latency: 'instant', resources: 'low' })).toBe(
      CAPABILITY_TIMEOUT_TIER_MS.instant.low,
    );
    expect(resolveCostTimeoutMs({ latency: 'interactive', resources: 'medium' })).toBe(
      CAPABILITY_TIMEOUT_TIER_MS.interactive.medium,
    );
    expect(resolveCostTimeoutMs({ latency: 'long-running', resources: 'high' })).toBe(
      CAPABILITY_TIMEOUT_TIER_MS['long-running'].high,
    );

    // Tiers are monotonic in both dimensions, so a more expensive capability can
    // never be handed a smaller budget than a cheaper one.
    for (const resource of RESOURCES) {
      for (let i = 1; i < LATENCIES.length; i += 1) {
        expect(CAPABILITY_TIMEOUT_TIER_MS[LATENCIES[i]][resource]).toBeGreaterThan(
          CAPABILITY_TIMEOUT_TIER_MS[LATENCIES[i - 1]][resource],
        );
      }
    }
    for (const latency of LATENCIES) {
      for (let i = 1; i < RESOURCES.length; i += 1) {
        expect(CAPABILITY_TIMEOUT_TIER_MS[latency][RESOURCES[i]]).toBeGreaterThan(
          CAPABILITY_TIMEOUT_TIER_MS[latency][RESOURCES[i - 1]],
        );
      }
    }
  });

  it('never budgets below the cold-load floor that the 10s timeout finding failed', () => {
    // The earlier finding: a fixed 10s deadline expired while the editor was
    // still cold-loading, reporting a dead bridge for work that was fine. No
    // tier — not even the cheapest — may sit at or under that number again.
    for (const latency of LATENCIES) {
      for (const resource of RESOURCES) {
        expect(CAPABILITY_TIMEOUT_TIER_MS[latency][resource])
          .toBeGreaterThanOrEqual(MIN_CAPABILITY_TIMEOUT_MS);
      }
    }
    expect(MIN_CAPABILITY_TIMEOUT_MS).toBeGreaterThan(10_000);
  });

  it('routes a known tool/action pair to its record-declared tier', () => {
    expect(resolveActionTimeoutMs('inspect', 'inspect_object')).toBe(
      CAPABILITY_TIMEOUT_TIER_MS.instant.low,
    );
    expect(resolveActionTimeoutMs('manage_asset', 'delete_asset')).toBe(
      CAPABILITY_TIMEOUT_TIER_MS['long-running'].high,
    );
  });

  it('lets an explicit operator override win over every tier', async () => {
    process.env.MCP_REQUEST_TIMEOUT_MS = '4242';
    const { tools, timeoutFor } = createRecordingTools();

    await executeAutomationRequest(tools, 'inspect', {
      action: 'inspect_object',
      objectPath: '/Game/Example',
    });
    await executeAutomationRequest(tools, 'manage_asset', {
      action: 'delete_asset',
      assetPath: '/Game/Example',
    });

    expect(timeoutFor('inspect', 'inspect_object')).toBe(4242);
    expect(timeoutFor('manage_asset', 'delete_asset')).toBe(4242);
  });

  it('lets an explicit per-call timeout win over the tier', async () => {
    const { tools, timeoutFor } = createRecordingTools();

    await executeAutomationRequest(
      tools,
      'inspect',
      { action: 'inspect_object', objectPath: '/Game/Example' },
      undefined,
      { timeoutMs: 777 },
    );

    expect(timeoutFor('inspect', 'inspect_object')).toBe(777);
  });
});
