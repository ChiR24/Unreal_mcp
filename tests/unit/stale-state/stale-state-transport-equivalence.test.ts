import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';

import { Logger } from '../../../src/utils/logging/logger.js';
import type { ITools } from '../../../src/types/tools/tool-interfaces.js';
import type { GatewayContext } from '../../../src/server/tool-registry-gateway.js';
import { handleUnrealGatewayCall } from '../../../src/server/tool-registry-gateway.js';
import { isRecord } from '../../../src/utils/validation/type-guards.js';
import { dynamicToolManager } from '../../../src/tools/dynamic/dynamic-tool-manager.js';

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
    logger: new Logger('task42-transport-equivalence', 'error'),
    elicitationTimeoutMs: 0,
    ensureConnected: async () => true
  };
}

const asRecord = (value: unknown): Record<string, unknown> => (isRecord(value) ? value : {});
const receiptOf = (r: Record<string, unknown>): Record<string, unknown> => asRecord(r.receipt);

async function execute(args: Record<string, unknown>): Promise<Record<string, unknown>> {
  return await handleUnrealGatewayCall({ operation: 'execute', ...args }, makeContext());
}

beforeEach(() => {
  handlerResult = { success: true, message: 'ok' };
});

afterEach(() => {
  dynamicToolManager.reset();
  vi.clearAllMocks();
});

// Task 42 acceptance: "both transports return equivalent errors/references".
//
// The TypeScript gateway deliberately does NOT compare live revisions - the
// comparison is game-thread only. So a live-state refusal is produced by the
// PLUGIN and travels back through the bridge as a handler failure. If that path
// flattens it to a generic UNREAL_EXECUTION_ERROR, the identical refusal is a
// typed staleState on the native transport but an untyped execution error over
// stdio/WebSocket, and a client cannot react to it the same way.
describe('task 42 stale-state equivalence across transports', () => {
  it('preserves a plugin STALE_STATE refusal as a typed staleState error', async () => {
    handlerResult = {
      success: false,
      errorCode: 'STALE_STATE',
      message: "Editor 'selection' state changed since it was read (expected 3, current 5).",
      currentRevision: '5',
      expectedRevision: '3'
    };

    const result = await execute({ capability: 'asset.list', params: {} });

    expect(result.errorCode).toBe('STALE_STATE');
    const error = asRecord(receiptOf(result).error);
    expect(error.kind).toBe('staleState');
    expect(error.currentRevision).toBe('5');
    expect(error.expectedRevision).toBe('3');
  });

  // The case above asserts the mapping against a HAND-AUTHORED payload. The
  // bridge does not send that shape: it puts the code in `error` (not
  // `errorCode`) and carries `liveRevisions` instead of currentRevision /
  // expectedRevision. So the equivalence was proven on a payload the plugin
  // never produces, and the real refusal kept arriving as an untyped execution
  // error over stdio. Captured verbatim from a live UE 5.7.4 editor:
  //   node scripts/qa/cross-transport-matrix.mjs -> revision dimension.
  it('preserves the refusal in the shape the WebSocket bridge actually sends', async () => {
    handlerResult = {
      type: 'automation_response',
      requestId: 'afaf67d6-aa85-4244-b5fe-e210c9a15a7d',
      success: false,
      message: "Editor 'selection' state changed since it was read (expected 1, current 2). Re-read the state and retry.",
      error: 'STALE_STATE',
      liveRevisions: { selection: 2, level: 2, assetRegistry: 9066, package: 1 },
      isError: true,
      toolName: 'manage_effect',
      action: 'list_debug_shapes'
    };

    const result = await execute({ capability: 'asset.list', params: {} });

    expect(result.errorCode).toBe('STALE_STATE');
    expect(asRecord(receiptOf(result).error).kind).toBe('staleState');
  });

  it('still reports an ordinary plugin failure as UNREAL_EXECUTION_ERROR', async () => {
    handlerResult = { success: false, message: 'the editor refused for an unrelated reason' };

    const result = await execute({ capability: 'asset.list', params: {} });

    expect(result.errorCode).toBe('UNREAL_EXECUTION_ERROR');
    expect(asRecord(receiptOf(result).error).kind).not.toBe('staleState');
  });

  it('does not promote a free-text error that merely mentions staleness', async () => {
    handlerResult = {
      success: false,
      message: 'the editor refused',
      error: 'the selection looked STALE_STATE-ish to the handler'
    };

    const result = await execute({ capability: 'asset.list', params: {} });

    expect(result.errorCode).toBe('UNREAL_EXECUTION_ERROR');
    expect(asRecord(receiptOf(result).error).kind).not.toBe('staleState');
  });
});
