// Task 39 REMEDIATION — behavioral RED for the review blockers that survived the
// first pass. Every case drives a real seam and asserts an absent behavior, so
// each RED is a clean assertion failure (not a module-link error):
//   (h) a success receipt never populates reusable handles / changed entities /
//       task state from the real handler output;
//   (g) the error receipt message and the outer legacy envelope re-emit a raw
//       secret that must be masked, and a secret in a success payload survives;
//   (c) the single client-facing gateway correlation id is not carried into the
//       outbound automation-bridge request metadata (so it cannot join
//       gateway -> bridge -> queue -> result);
//   (9) revision-separation regression anchors (stay GREEN).

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';

import { Logger } from '../../../src/utils/logging/logger.js';
import type { ITools } from '../../../src/types/tools/tool-interfaces.js';
import type { GatewayContext } from '../../../src/server/tool-registry-gateway.js';
import { handleUnrealGatewayCall } from '../../../src/server/tool-registry-gateway.js';
import { executeAutomationRequest } from '../../../src/tools/handlers/foundation/dispatch/automation-request-dispatch.js';
import { runWithGatewayCorrelation } from '../../../src/automation/gateway-correlation-context.js';
import { CorrelationIdSchema } from '../../../src/tools/catalog/capabilities/semantic/ids.js';
import { isRecord } from '../../../src/utils/validation/type-guards.js';
import { dynamicToolManager } from '../../../src/tools/dynamic/dynamic-tool-manager.js';
import { catalogRevision } from '../../../src/server/gateway/gateway-capability-index.js';

let handlerResult: unknown = { success: true, message: 'ok' };

vi.mock('../../../src/tools/orchestration/consolidated-tool-handlers.js', () => ({
  handleConsolidatedToolCall: vi.fn(async () => handlerResult)
}));

import { handleConsolidatedToolCall } from '../../../src/tools/orchestration/consolidated-tool-handlers.js';
const dispatchMock = vi.mocked(handleConsolidatedToolCall);

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
    logger: new Logger('task39-remediation', 'error'),
    elicitationTimeoutMs: 0,
    ensureConnected: async () => connected
  };
}

function asRecord(value: unknown): Record<string, unknown> {
  if (!isRecord(value)) throw new Error(`expected an object, received ${String(value)}`);
  return value;
}

async function execute(args: Record<string, unknown>): Promise<Record<string, unknown>> {
  return await handleUnrealGatewayCall({ operation: 'execute', ...args }, makeContext());
}

const receiptOf = (r: Record<string, unknown>): Record<string, unknown> => asRecord(r.receipt);

beforeEach(() => {
  handlerResult = { success: true, message: 'ok' };
});

afterEach(() => {
  dynamicToolManager.reset();
  vi.clearAllMocks();
});

describe('task39 (h): success receipt populates reusable outcome metadata from real handler output', () => {
  it('carries changed entities the handler reported', async () => {
    handlerResult = { success: true, message: 'ok', changedEntities: ['/Game/Meshes/SM_Rock'] };
    const receipt = receiptOf(await execute({ capability: 'asset.list', params: {} }));
    expect(receipt.status).toBe('success');
    expect(receipt.changes).toContain('/Game/Meshes/SM_Rock');
  });

  it('passes through an explicit, schema-valid typed handle', async () => {
    handlerResult = { success: true, message: 'ok', handles: [{ kind: 'actor', ref: 'PersistentLevel.MyActor' }] };
    const receipt = receiptOf(await execute({ capability: 'asset.list', params: {} }));
    const handles = receipt.handles;
    expect(Array.isArray(handles) ? handles : []).toContainEqual({ kind: 'actor', ref: 'PersistentLevel.MyActor' });
  });

  it('derives a reusable asset handle + changed entity from a mutation result', async () => {
    handlerResult = { success: true, message: 'ok', assetPath: '/Game/Meshes/SM_New', changedEntities: ['/Game/Meshes/SM_New'] };
    const receipt = receiptOf(await execute({ capability: 'asset.list', params: {} }));
    expect(receipt.changes).toContain('/Game/Meshes/SM_New');
    const handles = Array.isArray(receipt.handles) ? receipt.handles : [];
    expect(handles).toContainEqual({ kind: 'asset', path: '/Game/Meshes/SM_New' });
  });

  it('carries a schema-valid task state when the handler reports one', async () => {
    handlerResult = { success: true, message: 'ok', task: { taskId: 'job-7', state: 'running', progress: 0.5 } };
    const receipt = receiptOf(await execute({ capability: 'asset.list', params: {} }));
    expect(receipt.task).toEqual({ taskId: 'job-7', state: 'running', progress: 0.5 });
  });
});

describe('task39 (g): secrets are masked in the receipt error and the outer legacy envelope', () => {
  const SECRET = 'supersecrettokenvalue1234567890';

  it('masks a secret in a failed handler message on both the receipt error and the outer envelope', async () => {
    handlerResult = { success: false, isError: true, message: `auth failed token=${SECRET}` };
    const result = await execute({ capability: 'asset.list', params: {} });
    const error = asRecord(receiptOf(result).error);
    expect(String(error.message)).not.toContain(SECRET);
    expect(String(error.message)).toContain('[REDACTED]');
    // The outer legacy envelope must not re-emit the same secret unredacted.
    expect(JSON.stringify(result)).not.toContain(SECRET);
  });

  it('masks a secret embedded in a successful result payload on the outer envelope', async () => {
    handlerResult = { success: true, message: 'ok', note: `Bearer ${SECRET}` };
    const result = await execute({ capability: 'asset.list', params: {} });
    expect(JSON.stringify(result)).not.toContain(SECRET);
  });
});

describe('task39 (c): the client-facing correlation id crosses into automation-bridge request metadata', () => {
  it('stamps the active gateway correlation id onto the outbound automation request options', async () => {
    const captured: Array<{ action: string; options: Record<string, unknown> }> = [];
    const bridge = {
      isConnected: () => true,
      sendAutomationRequest: async (action: string, _payload: unknown, options: Record<string, unknown> = {}) => {
        captured.push({ action, options });
        return { success: true };
      }
    };
    const tools = { automationBridge: bridge } as unknown as ITools;

    await runWithGatewayCorrelation(CorrelationIdSchema.parse('gw-777'), () =>
      executeAutomationRequest(tools, 'inspect', { action: 'ping' })
    );

    expect(captured).toHaveLength(1);
    expect(captured[0]?.options.correlationId).toBe('gw-777');
  });

  it('does not leak a correlation id when none is active', async () => {
    const captured: Array<Record<string, unknown>> = [];
    const bridge = {
      isConnected: () => true,
      sendAutomationRequest: async (_a: string, _p: unknown, options: Record<string, unknown> = {}) => {
        captured.push(options);
        return { success: true };
      }
    };
    const tools = { automationBridge: bridge } as unknown as ITools;
    await executeAutomationRequest(tools, 'inspect', { action: 'ping' });
    expect(captured[0]?.correlationId).toBeUndefined();
  });
});

describe('task39 (polish): malformed expectedCatalogRevision fails closed as validation/INVALID_OPTIONS (native parity)', () => {
  const MALFORMED: ReadonlyArray<readonly [string, unknown]> = [
    ['empty string', ''],
    ['non-string number', 123],
    ['non-string object', { nested: true }],
    ['non-string boolean', true],
    ['non-hex string', 'ZZZ-not-hex'],
    ['over-length hex', 'a'.repeat(65)]
  ];

  for (const [label, value] of MALFORMED) {
    it(`refuses a ${label} pin before dispatch with a validation error and pointer`, async () => {
      const result = await execute({ capability: 'asset.list', params: {}, options: { expectedCatalogRevision: value } });
      const receipt = receiptOf(result);
      expect(receipt.status).toBe('error');
      expect(result.errorCode).toBe('INVALID_OPTIONS');
      const error = asRecord(receipt.error);
      expect(error.kind).toBe('validation');
      expect(error.pointer).toBe('/options/expectedCatalogRevision');
      expect(dispatchMock).not.toHaveBeenCalled();
    });
  }

  it('still refuses a well-formed but stale pin as staleState (not validation)', async () => {
    const result = await execute({ capability: 'asset.list', params: {}, options: { expectedCatalogRevision: 'deadbeef' } });
    const error = asRecord(receiptOf(result).error);
    expect(error.kind).toBe('staleState');
    expect(dispatchMock).not.toHaveBeenCalled();
  });

  it('proceeds to dispatch when the pin matches the live catalog revision', async () => {
    const result = await execute({ capability: 'asset.list', params: {}, options: { expectedCatalogRevision: catalogRevision() } });
    expect(receiptOf(result).status).toBe('success');
    expect(dispatchMock).toHaveBeenCalledTimes(1);
  });
});

describe('task39 (polish): the nested receipt.data is deep-masked, not just the outer envelope', () => {
  it('masks a secret carried in a projected output field on receipt.data while preserving legitimate data', async () => {
    handlerResult = { success: true, folders: ['authorization: Bearer sk-live-abcdef0123456789', '/Game/Meshes'] };
    const envelope = await execute({ capability: 'asset.list', params: {} });
    const receipt = receiptOf(envelope);
    expect(receipt.status).toBe('success');
    // The receipt binds to the published payload through a digest; the payload itself lives on
    // the envelope's top-level `data` (dogfood #11), which must be masked the same way.
    expect(typeof receipt.dataDigest).toBe('string');
    const data = JSON.stringify(envelope.data);
    expect(data).not.toContain('sk-live-abcdef0123456789');
    expect(data).toContain('[REDACTED]');
    expect(data).toContain('/Game/Meshes');
  });
});

describe('task39 (9): revision-separation regression anchors (stay GREEN)', () => {
  it('keeps catalog/capability/schema revisions distinct hex digests, catalogRevision equal to the live digest', async () => {
    const receipt = receiptOf(await execute({ capability: 'asset.list', params: {} }));
    expect(receipt.catalogRevision).toBe(catalogRevision());
    const trio = [receipt.catalogRevision, receipt.capabilityRevision, receipt.schemaRevision];
    expect(new Set(trio).size).toBe(3);
    for (const rev of trio) {
      expect(typeof rev).toBe('string');
      expect(String(rev)).toMatch(/^[0-9a-f]+$/);
    }
  });

  it('never overloads catalogRevision with a numeric value', async () => {
    const receipt = receiptOf(await execute({ capability: 'asset.list', params: {} }));
    expect(typeof receipt.catalogRevision).not.toBe('number');
  });
});
