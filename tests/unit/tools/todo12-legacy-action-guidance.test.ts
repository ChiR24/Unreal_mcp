// Plan Todo 12 (BB-014, BB-052, BB-053) — discovery must surface the action
// `execute` accepts, never `routing.dispatchAction`.
//
// Every expected action here is derived from the record's own `legacyIds`, and
// each divergent case additionally asserts `dispatchAction` still DIFFERS from
// it. Without that second assertion a catalogue change that collapsed the two
// spellings would let these pass while proving nothing.

import { describe, expect, it, vi } from 'vitest';

import { Logger } from '../../../src/utils/logging/logger.js';
import type { ITools } from '../../../src/types/tools/tool-interfaces.js';
import type { GatewayContext } from '../../../src/server/tool-registry-gateway.js';
import { handleUnrealGatewayCall } from '../../../src/server/tool-registry-gateway.js';
import { isRecord } from '../../../src/utils/validation/type-guards.js';
import {
  capabilityIndex,
  resolveLegacyPair
} from '../../../src/server/gateway/gateway-capability-index.js';
import { primaryExecutableAction } from '../../../src/server/gateway/gateway-capability-view.js';
import type { CapabilityRecord } from '../../../src/tools/catalog/capabilities/model.js';
import { minimalValidParams } from './support/capability-fixtures.js';

const DIVERGENT_FIXTURES = ['asset.create_render_target', 'asset.search_assets'] as const;

const dispatched: Array<{ tool: string; args: Record<string, unknown> }> = [];

vi.mock('../../../src/tools/orchestration/consolidated-tool-handlers.js', () => ({
  handleConsolidatedToolCall: vi.fn(async (tool: string, args: Record<string, unknown>) => {
    dispatched.push({ tool, args });
    return { success: true, message: 'ok' };
  })
}));

function makeContext(): GatewayContext {
  const tools: ITools = {
    systemTools: {
      executeConsoleCommand: async () => ({ success: false }),
      getProjectSettings: async () => ({})
    },
    assetResources: { list: async () => ({}) }
  };
  return {
    tools,
    logger: new Logger('todo12-legacy-action-guidance', 'error'),
    elicitationTimeoutMs: 0,
    ensureConnected: async () => true
  };
}

async function gateway(args: Record<string, unknown>): Promise<Record<string, unknown>> {
  return await handleUnrealGatewayCall(args, makeContext());
}

function record(id: string): CapabilityRecord {
  const found = capabilityIndex().byId.get(id);
  if (!found) throw new Error(`fixture capability '${id}' is absent from the generated registry`);
  return found;
}

function legacyActionOf(id: string): string {
  const target = record(id);
  const legacy = target.legacyIds.find((entry) => entry.tool === target.routing.parentTool)
    ?? target.legacyIds[0];
  if (!legacy) throw new Error(`capability '${id}' declares no legacy pair`);
  return legacy.action;
}

function rowsOf(result: Record<string, unknown>): Array<Record<string, unknown>> {
  const rows = result.results ?? result.capabilities;
  return Array.isArray(rows) ? rows.filter(isRecord) : [];
}

describe('todo12: discovery surfaces the executable action, not the dispatch verb', () => {
  it.each(DIVERGENT_FIXTURES)('%s is a genuinely divergent fixture', (id) => {
    const target = record(id);

    expect(target.routing.dispatchAction).not.toBe(legacyActionOf(id));
  });

  it.each(DIVERGENT_FIXTURES)('describe(%s) contracts the legacy action', async (id) => {
    const target = record(id);
    const expected = legacyActionOf(id);

    const result = await gateway({ operation: 'describe', capability: id });

    expect(result.success).toBe(true);
    expect(result.capability).toBe(id);
    expect(result.action).toBe(expected);
    expect(result.action).not.toBe(target.routing.dispatchAction);
  });

  it.each(DIVERGENT_FIXTURES)('search row for %s carries the legacy action', async (id) => {
    const target = record(id);
    const expected = legacyActionOf(id);

    const result = await gateway({ operation: 'search', query: expected, limit: 25 });
    const row = rowsOf(result).find((entry) => entry.capability === id);

    expect(row).toBeDefined();
    expect(row?.action).toBe(expected);
    expect(row?.action).not.toBe(target.routing.dispatchAction);
  });

  it.each(DIVERGENT_FIXTURES)('the nextCall search returns for %s is executable', async (id) => {
    const expected = legacyActionOf(id);
    const search = await gateway({ operation: 'search', query: expected, limit: 25 });
    const row = rowsOf(search).find((entry) => entry.capability === id);
    const nextCall = row?.nextCall;

    expect(isRecord(nextCall)).toBe(true);
    if (!isRecord(nextCall)) return;

    const followed = await gateway({ ...nextCall });

    expect(followed.success).toBe(true);
    expect(followed.capability).toBe(id);
  });

  it.each(DIVERGENT_FIXTURES)('describe(%s) hands back an execute nextCall naming the legacy action', async (id) => {
    const expected = legacyActionOf(id);
    const contract = await gateway({ operation: 'describe', capability: id });
    const nextCall = contract.nextCall;

    expect(isRecord(nextCall)).toBe(true);
    if (!isRecord(nextCall)) return;
    expect(nextCall.operation).toBe('execute');
    expect(nextCall.action).toBe(expected);

    const resolved = resolveLegacyPair(String(nextCall.tool), String(nextCall.action));

    expect(resolved.kind).toBe('legacy');
    if (resolved.kind !== 'legacy') return;
    expect(resolved.record.id).toBe(id);
  });

  it.each(DIVERGENT_FIXTURES)('the legacy pair for %s resolves back to it', (id) => {
    const target = record(id);
    const resolved = resolveLegacyPair(target.routing.parentTool, legacyActionOf(id));

    expect(resolved.kind).toBe('legacy');
    if (resolved.kind !== 'legacy') return;
    expect(resolved.record.id).toBe(id);
  });

  it('primaryExecutableAction never returns the dispatch verb for a divergent record', () => {
    for (const id of DIVERGENT_FIXTURES) {
      const target = record(id);

      expect(primaryExecutableAction(target)).toBe(legacyActionOf(id));
      expect(primaryExecutableAction(target)).not.toBe(target.routing.dispatchAction);
    }
  });

  it('every surfaced describe action resolves back to the same capability', async () => {
    const sampled = capabilityIndex().records.slice(0, 40);

    for (const target of sampled) {
      const surfaced = primaryExecutableAction(target);
      const resolved = resolveLegacyPair(target.routing.parentTool, surfaced);
      if (resolved.kind !== 'legacy') continue;

      expect(resolved.record.id).toBe(target.id);
    }
  });
});

describe('todo12: BB-053 get_status resolves to the protected management capability', () => {
  it('describe(manage_tools, get_status) is manage_tools.get_status, never inspect.find_by_tag', async () => {
    const result = await gateway({ operation: 'describe', tool: 'manage_tools', action: 'get_status' });

    expect(result.success).toBe(true);
    expect(result.capability).toBe('manage_tools.get_status');
    expect(result.capability).not.toBe('inspect.find_by_tag');
    expect(result.parentTool).toBe('manage_tools');
    expect(result.action).toBe('get_status');
  });

  it('the legacy pair manage_tools::get_status resolves to the management capability', () => {
    const resolved = resolveLegacyPair('manage_tools', 'get_status');

    expect(resolved.kind).toBe('legacy');
    if (resolved.kind !== 'legacy') return;
    expect(resolved.record.id).toBe('manage_tools.get_status');
  });

  it('a typo returns UNKNOWN_ACTION with bounded suggestions and an executable nextCall', async () => {
    const result = await gateway({ operation: 'describe', tool: 'manage_tools', action: 'get_status_typo' });

    expect(result.success).toBe(false);
    expect(result.errorCode).toBe('UNKNOWN_ACTION');
    expect(Array.isArray(result.suggestions)).toBe(true);
    expect((result.suggestions as unknown[]).length).toBeLessThanOrEqual(3);
    expect((result.suggestions as unknown[])).toContain('get_status');

    const nextCall = result.nextCall;
    expect(isRecord(nextCall)).toBe(true);
    if (!isRecord(nextCall)) return;

    const followed = await gateway({ ...nextCall });

    expect(followed.success).toBe(true);
  });

  it('the real catalogue builds an index with no ambiguous selector', () => {
    expect(() => capabilityIndex()).not.toThrow();
    expect(capabilityIndex().records.length).toBeGreaterThan(0);
  });
});

describe('todo12: the execute nextCall discovery hands back actually runs', () => {
  it.each(DIVERGENT_FIXTURES)('describe(%s).nextCall executes and dispatches its parent tool', async (id) => {
    dispatched.length = 0;
    const target = record(id);
    const contract = await gateway({ operation: 'describe', capability: id });
    const nextCall = contract.nextCall;

    expect(isRecord(nextCall)).toBe(true);
    if (!isRecord(nextCall)) return;
    expect(nextCall.operation).toBe('execute');

    const result = await gateway({ ...nextCall, params: minimalValidParams(target) });

    expect(result.success).toBe(true);
    expect(result.errorCode).toBeUndefined();
    expect(dispatched).toHaveLength(1);
    expect(dispatched[0]?.tool).toBe(target.routing.parentTool);
  });

  it('the dispatch verb is refused by execute, so it never became an address', async () => {
    dispatched.length = 0;
    const target = record('asset.create_render_target');

    const result = await gateway({
      operation: 'execute',
      tool: target.routing.parentTool,
      action: target.routing.dispatchAction,
      params: {}
    });

    expect(result.success).toBe(false);
    expect(result.errorCode).toBe('UNKNOWN_ACTION');
    expect(dispatched).toHaveLength(0);
  });
});
