/// <reference types="node" />

// Task 43 finding F1 — the compensation receipt must survive gateway narrowing.
//
// `Private/Foundation/McpCompensationReceipt.h` states the block is emitted on
// "EVERY path including the all-succeeded path", and `DescribeInto()` genuinely
// does emit it unconditionally. The native test
// `McpAutomationBridge.Foundation.CompensationReceipt.PartialCompletionIsHonest`
// genuinely passes. Every one of those statements is true, and every one of them
// is IN-PROCESS: none of them observes what a client receives.
//
// On the wire the claim was false. `control_editor.save_all` declared
// `output = {success, message}` with `additionalProperties:false`, and
// `projectCanonicalOutput` (src/server/gateway/gateway-execute-dispatch.ts,
// mirrored by `McpProjectCanonicalOutput` in the native transport) copies ONLY
// the declared properties into the payload the client reads. `compensation` was
// not declared, so it was dropped from the success payload; on the failure path
// it survived only because the raw handler result is preserved un-narrowed
// beside the typed error.
//
// These assertions therefore run through `handleUnrealGatewayCall` and read the
// CLIENT-VISIBLE payload after narrowing. A test on `DescribeInto()` cannot
// falsify this defect — that is exactly the test that stayed green while the
// wire was broken.

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';

import { Logger } from '../../../src/utils/logging/logger.js';
import type { ITools } from '../../../src/types/tools/tool-interfaces.js';
import type { GatewayContext } from '../../../src/server/tool-registry-gateway.js';
import { handleUnrealGatewayCall } from '../../../src/server/tool-registry-gateway.js';
import { isRecord } from '../../../src/utils/validation/type-guards.js';
import { dynamicToolManager } from '../../../src/tools/dynamic/dynamic-tool-manager.js';
import { capabilityIndex } from '../../../src/server/gateway/gateway-capability-index.js';

const CAPABILITY_ID = 'control_editor.save_all';

let handlerResult: unknown = { success: true, message: 'ok' };

vi.mock('../../../src/tools/orchestration/consolidated-tool-handlers.js', () => ({
  handleConsolidatedToolCall: vi.fn(async () => handlerResult)
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
    logger: new Logger('task43-compensation', 'error'),
    elicitationTimeoutMs: 0,
    ensureConnected: async () => true
  };
}

function asRecord(value: unknown): Record<string, unknown> {
  if (!isRecord(value)) throw new Error(`expected an object, received ${String(value)}`);
  return value;
}

async function saveAll(): Promise<Record<string, unknown>> {
  return await handleUnrealGatewayCall(
    { operation: 'execute', tool: 'control_editor', action: 'save_all', params: {} },
    makeContext()
  );
}

/** The payload a client reads: the narrowed canonical output on the receipt. */
function clientData(envelope: Record<string, unknown>): Record<string, unknown> {
  return asRecord(asRecord(envelope.receipt).data);
}

// Byte-for-byte the block `FMcpCompensationReceipt::DescribeInto()` writes, so
// these fixtures fail if the C++ emitter and the declared contract diverge.
const ROLLBACK_REASON =
  'Completed steps are already durable on disk. No editor transaction can reach a '
  + 'finished save, build or render, so nothing here was or can be undone.';

function compensation(overrides: Record<string, unknown> = {}): Record<string, unknown> {
  return {
    operation: CAPABILITY_ID,
    atomic: false,
    rollback: 'unavailable',
    rollbackReason: ROLLBACK_REASON,
    state: 'completed',
    completed: [{ step: 'save:/Game/Maps/EntryMap', detail: 'level package written to disk' }],
    notCompleted: [],
    skipped: [],
    compensatingCapabilities: [],
    callerAction: '',
    ...overrides
  };
}

beforeEach(() => {
  handlerResult = { success: true, message: 'All assets saved', compensation: compensation() };
});

afterEach(() => {
  dynamicToolManager.reset();
  vi.clearAllMocks();
});

describe('F1: the compensation receipt reaches the client after gateway narrowing', () => {
  it('delivers the block at the documented data location on the ALL-SUCCEEDED path', async () => {
    const envelope = await saveAll();

    expect(envelope.success).toBe(true);
    expect(envelope.capability).toBe(CAPABILITY_ID);

    const block = asRecord(clientData(envelope).compensation);
    // "everything worked" still does not mean "this could be undone": a client
    // must be able to learn the operation was non-atomic from a SUCCESS.
    expect(block.atomic).toBe(false);
    expect(block.rollback).toBe('unavailable');
    expect(block.state).toBe('completed');
    expect(block.rollbackReason).toBe(ROLLBACK_REASON);
    expect(block.completed).toEqual([
      { step: 'save:/Game/Maps/EntryMap', detail: 'level package written to disk' }
    ]);
  });

  it('proves narrowing is live: an undeclared sibling is dropped from the same payload', async () => {
    handlerResult = {
      success: true,
      message: 'All assets saved',
      compensation: compensation(),
      // Not declared by the capability's output schema, so the projection drops
      // it. This is the exact mechanism that dropped `compensation` itself, so
      // the assertion above only passes because the schema now declares it.
      savedWorldCount: 1
    };

    const data = clientData(await saveAll());

    expect(data.compensation).toBeDefined();
    expect(data.savedWorldCount).toBeUndefined();
  });

  it('declares compensation in the generated output contract the gateway loads', () => {
    const record = capabilityIndex().byId.get(CAPABILITY_ID);
    if (record === undefined) throw new Error(`${CAPABILITY_ID} is absent from the registry`);

    const properties = asRecord(asRecord(record.schemas.output).properties);
    const block = asRecord(properties.compensation);
    const fields = asRecord(block.properties);

    expect(block.type).toBe('object');
    // The honest values are pinned by the contract itself, not just by prose in
    // a header: there is no schema-legal way to advertise a rollback here.
    expect(asRecord(fields.atomic).enum).toEqual([false]);
    expect(asRecord(fields.rollback).enum).toEqual(['unavailable']);
    expect(asRecord(fields.state).enum).toEqual(['completed', 'partial', 'failed', 'noop']);
    // Optional at the top level: a bridge build that predates the receipt must
    // not turn a successful save into an output-contract violation.
    expect(asRecord(record.schemas.output).required).toEqual(['success']);
  });

  it('refuses a block that claims a rollback instead of delivering the claim', async () => {
    handlerResult = {
      success: true,
      message: 'All assets saved',
      compensation: compensation({ rollback: 'completed', atomic: true })
    };

    const envelope = await saveAll();

    expect(envelope.success).toBe(false);
    expect(envelope.errorCode).toBe('OUTPUT_SCHEMA_VIOLATION');
    expect(envelope.pointer).toBe('/compensation/atomic');
  });

  it('still carries the block beside the typed error on the PARTIAL path', async () => {
    handlerResult = {
      success: false,
      message: 'Some packages failed to save',
      compensation: compensation({
        state: 'partial',
        notCompleted: [
          { step: 'save:/Game/Maps/Broken', detail: 'level save failed; this package is unchanged on disk' }
        ],
        callerAction: 'Re-run control_editor.save_all, or discard the listed packages.'
      })
    };

    const envelope = await saveAll();

    expect(envelope.success).toBe(false);
    expect(envelope.errorCode).toBe('UNREAL_EXECUTION_ERROR');
    const block = asRecord(asRecord(envelope.result).compensation);
    expect(block.state).toBe('partial');
    expect(block.rollback).toBe('unavailable');
  });
});
