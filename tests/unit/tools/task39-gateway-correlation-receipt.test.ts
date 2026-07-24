// Task 39 — correlated operation receipts and one typed error algebra (behavioral).
//
// Written failing-first against the current gateway execute seam. Every case
// drives the real `handleUnrealGatewayCall` and asserts the enriched, correlated
// receipt contract the plan requires:
//   * one gateway correlation id minted once and carried across the await into
//     both the success result and the error result,
//   * a bounded external requestId echoed only when it is truthfully available,
//   * an echoed idempotency key with NO dedup/persistence,
//   * three DISTINCT bounded revision strings sourced from the live catalog
//     digest and the resolved record's content/schema hashes,
//   * the typed error algebra classifying output / capability / dispatch / stale
//     failures as their own kinds instead of a catch-all validation error,
//   * a pre-dispatch stale-catalog-revision refusal that never reaches dispatch.
//
// These use only pre-existing imports so each RED failure is a clean assertion
// failure (the behavior is absent), not a module-link error.

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';

import { Logger } from '../../../src/utils/logging/logger.js';
import type { ITools } from '../../../src/types/tools/tool-interfaces.js';
import type { GatewayContext } from '../../../src/server/tool-registry-gateway.js';
import { handleUnrealGatewayCall } from '../../../src/server/tool-registry-gateway.js';
import { runWithMcpRequestContext } from '../../../src/automation/request-context.js';
import { isRecord } from '../../../src/utils/validation/type-guards.js';
import { dynamicToolManager } from '../../../src/tools/dynamic/dynamic-tool-manager.js';
import {
  capabilityIndex,
  catalogRevision
} from '../../../src/server/gateway/gateway-capability-index.js';
import type { CapabilityRecord } from '../../../src/tools/catalog/capabilities/model.js';

const dispatched: Array<{ tool: string; args: Record<string, unknown> }> = [];
let handlerResult: unknown = { success: true, message: 'ok' };

vi.mock('../../../src/tools/orchestration/consolidated-tool-handlers.js', () => ({
  handleConsolidatedToolCall: vi.fn(async (tool: string, args: Record<string, unknown>) => {
    dispatched.push({ tool, args });
    return handlerResult;
  })
}));

function makeContext(connected = true): GatewayContext {
  const tools: ITools = {
    systemTools: {
      executeConsoleCommand: async () => ({ success: false }),
      getProjectSettings: async () => ({})
    },
    assetResources: { list: async () => ({}) }
  };
  return {
    tools,
    logger: new Logger('task39-correlation', 'error'),
    elicitationTimeoutMs: 0,
    ensureConnected: async () => connected
  };
}

async function execute(args: Record<string, unknown>, connected = true): Promise<Record<string, unknown>> {
  return await handleUnrealGatewayCall({ operation: 'execute', ...args }, makeContext(connected));
}

function record(id: string): CapabilityRecord {
  const found = capabilityIndex().byId.get(id);
  if (!found) throw new Error(`fixture capability '${id}' is absent from the generated registry`);
  return found;
}

function asRecord(value: unknown): Record<string, unknown> {
  if (!isRecord(value)) throw new Error(`expected an object, received ${String(value)}`);
  return value;
}

function receiptOf(result: Record<string, unknown>): Record<string, unknown> {
  return asRecord(result.receipt);
}

function errorOf(result: Record<string, unknown>): Record<string, unknown> {
  return asRecord(receiptOf(result).error);
}

beforeEach(() => {
  handlerResult = { success: true, message: 'ok' };
});

afterEach(() => {
  dynamicToolManager.reset();
  dispatched.length = 0;
});

describe('task39: one gateway correlation id is minted and carried across the await', () => {
  it('stamps a gw-prefixed correlation id on the outer envelope and the nested receipt, equal on success', async () => {
    const result = await execute({ capability: 'asset.list', params: {} });

    expect(result.success).toBe(true);
    expect(typeof result.correlationId).toBe('string');
    expect(String(result.correlationId)).toMatch(/^gw-\d+$/);
    expect(receiptOf(result).correlationId).toBe(result.correlationId);
  });

  it('carries the same correlation id into an error result (no misleading success)', async () => {
    handlerResult = { success: false, isError: true, message: 'Automation bridge failure.' };

    const result = await execute({ capability: 'asset.list', params: {} });

    expect(result.success).toBe(false);
    expect(String(result.correlationId)).toMatch(/^gw-\d+$/);
    expect(receiptOf(result).status).toBe('error');
    expect(receiptOf(result).correlationId).toBe(result.correlationId);
  });

  it('gives distinct correlation ids to two separate gateway calls', async () => {
    const a = await execute({ capability: 'asset.list', params: {} });
    const b = await execute({ capability: 'asset.list', params: {} });
    expect(result_correlation(a)).not.toBe(result_correlation(b));
  });
});

function result_correlation(result: Record<string, unknown>): string {
  return String(result.correlationId);
}

describe('task39: bounded external requestId is echoed only when truthfully available', () => {
  it('echoes the canonicalized MCP request id onto the receipt when a request context is active', async () => {
    const result = await runWithMcpRequestContext(
      { requestId: 'str:abc-123' },
      () => execute({ capability: 'asset.list', params: {} })
    );

    expect(receiptOf(result).requestId).toBe('str:abc-123');
  });

  it('omits requestId entirely when no request context is active', async () => {
    const result = await execute({ capability: 'asset.list', params: {} });
    expect(receiptOf(result).requestId).toBeUndefined();
  });
});

describe('task39: three distinct bounded revision strings from live runtime sources', () => {
  it('stamps catalog, capability and schema revisions from the true digests on success', async () => {
    const target = record('asset.list');
    const result = await execute({ capability: 'asset.list', params: {} });
    const receipt = receiptOf(result);

    expect(receipt.catalogRevision).toBe(catalogRevision());
    expect(receipt.capabilityRevision).toBe(target.hashes.content);
    expect(receipt.schemaRevision).toBe(target.hashes.schema);

    const revisions = [receipt.catalogRevision, receipt.capabilityRevision, receipt.schemaRevision];
    expect(new Set(revisions).size).toBe(3);
    for (const revision of revisions) {
      expect(typeof revision).toBe('string');
      expect(String(revision)).toMatch(/^[0-9a-f]+$/);
    }
  });

  it('carries the same revision triple on a resolved-capability error', async () => {
    const target = record('asset.import');
    const result = await execute({ capability: 'asset.import', params: { sourcePath: '/tmp/a.fbx' } });
    const receipt = receiptOf(result);

    expect(receipt.status).toBe('error');
    expect(receipt.catalogRevision).toBe(catalogRevision());
    expect(receipt.capabilityRevision).toBe(target.hashes.content);
    expect(receipt.schemaRevision).toBe(target.hashes.schema);
  });
});

describe('task39: timing is reported on the receipt', () => {
  it('stamps a non-negative timingMs on a successful receipt', async () => {
    const result = await execute({ capability: 'asset.list', params: {} });
    const timingMs = receiptOf(result).timingMs;
    expect(typeof timingMs).toBe('number');
    expect(Number(timingMs)).toBeGreaterThanOrEqual(0);
  });
});

describe('task39: idempotency key is echoed only, never persisted or deduped', () => {
  it('echoes options.idempotencyKey onto the receipt', async () => {
    const result = await execute({
      capability: 'asset.list',
      params: {},
      options: { idempotencyKey: 'k-123' }
    });
    expect(receiptOf(result).idempotencyId).toBe('k-123');
  });

  it('dispatches again for an identical key (no dedup store)', async () => {
    await execute({ capability: 'asset.list', params: {}, options: { idempotencyKey: 'k-dup' } });
    await execute({ capability: 'asset.list', params: {}, options: { idempotencyKey: 'k-dup' } });
    expect(dispatched).toHaveLength(2);
  });
});

describe('task39: typed error algebra classifies every plan failure as its own kind', () => {
  it('classifies an output schema violation as the output kind, not validation', async () => {
    handlerResult = { success: 'yes', message: 'ok' };
    const result = await execute({ capability: 'asset.list', params: {} });
    const error = errorOf(result);
    expect(error.kind).toBe('output');
    expect(error.code).toBe('OUTPUT_SCHEMA_VIOLATION');
  });

  it('classifies an oversized result as the output kind', async () => {
    handlerResult = { success: true, message: 'x'.repeat(150_000) };
    const result = await execute({ capability: 'asset.list', params: {} });
    const error = errorOf(result);
    expect(error.kind).toBe('output');
    expect(error.code).toBe('RESULT_TOO_LARGE');
  });

  it('classifies a disabled tool as the capability kind', async () => {
    dynamicToolManager.disableTools(['manage_asset']);
    const result = await execute({ capability: 'asset.list', params: {} });
    expect(errorOf(result).kind).toBe('capability');
  });

  it('classifies a disconnected engine as the dispatch kind', async () => {
    const result = await execute({ capability: 'asset.list', params: {} }, false);
    expect(errorOf(result).kind).toBe('dispatch');
  });

  it('classifies an unreal execution failure as the execution kind', async () => {
    handlerResult = { success: false, isError: true, message: 'Unreal reported a failure.' };
    const result = await execute({ capability: 'asset.list', params: {} });
    expect(errorOf(result).kind).toBe('execution');
  });
});

describe('task39: pre-dispatch stale catalog revision refusal', () => {
  it('refuses a stale expectedCatalogRevision before any dispatch', async () => {
    const result = await execute({
      capability: 'asset.list',
      params: {},
      options: { expectedCatalogRevision: 'deadbeefdeadbeef' }
    });

    expect(result.success).toBe(false);
    const error = errorOf(result);
    expect(error.kind).toBe('staleState');
    expect(error.code).toBe('STALE_STATE');
    expect(dispatched).toHaveLength(0);
    // The current revision is surfaced so the client can re-read and retry.
    expect(receiptOf(result).catalogRevision).toBe(catalogRevision());
  });

  it('proceeds when expectedCatalogRevision matches the live catalog digest', async () => {
    const result = await execute({
      capability: 'asset.list',
      params: {},
      options: { expectedCatalogRevision: catalogRevision() }
    });

    expect(result.success).toBe(true);
    expect(dispatched).toHaveLength(1);
  });
});
