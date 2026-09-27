import { afterEach, describe, expect, it, vi } from 'vitest';
import { Logger } from '../../../src/utils/logging/logger.js';
import type { GatewayContext } from '../../../src/server/tool-registry-gateway.js';
import type { ITools } from '../../../src/types/tools/tool-interfaces.js';
import { handleUnrealGatewayCall } from '../../../src/server/tool-registry-gateway.js';
import { dynamicToolManager } from '../../../src/tools/dynamic/dynamic-tool-manager.js';

// Mock the consolidated tool handler so execute can reach the RESULT_TOO_LARGE gate
// without a live Unreal connection or a real dispatch.
const handleConsolidatedToolCall = vi.fn(async (_tool: string, _payload: Record<string, unknown>): Promise<unknown> => ({ success: true, data: { big: 'x'.repeat(200_000) } }));

function makeContext(logger: Logger): GatewayContext {
  const tools: ITools = {
    automationBridge: {
      isConnected: () => true,
      sendAutomationRequest: async (tool: string, payload: Record<string, unknown>) => handleConsolidatedToolCall(tool, payload)
    }
  };
  return {
    tools,
    logger,
    ensureConnected: async () => true
  };
}

afterEach(() => {
  // Re-enable any tool disabled during a test so cases stay independent.
  dynamicToolManager.reset();
});

describe('gateway RESULT_TOO_LARGE safety gate', () => {
  it('returns RESULT_TOO_LARGE with resultChars when the execution result exceeds the limit', async () => {
    const result = await handleUnrealGatewayCall(
      { operation: 'execute', capability: 'asset.list', params: {} },
      makeContext(new Logger('result-size', 'error'))
    ) as Record<string, unknown>;
    expect(result.success).toBe(false);
    expect(result.errorCode).toBe('RESULT_TOO_LARGE');
    expect(typeof result.resultChars).toBe('number');
    expect((result.resultChars as number) > 100_000).toBe(true);
  });
});

describe('gateway correlation logging', () => {
  it('logs request correlation and failure errorCode through the logger (no stdout)', async () => {
    const logger = new Logger('c2-correlation', 'debug');
    const debugSpy = vi.spyOn(logger, 'debug');
    const warnSpy = vi.spyOn(logger, 'warn');

    await handleUnrealGatewayCall(
      { operation: 'frobnicate', tool: 'manage_tools', action: 'get_status' },
      makeContext(logger)
    );

    const received = debugSpy.mock.calls.find((call) => call[0] === 'gateway request received');
    expect(received, 'request received must be logged').toBeDefined();
    const meta = received?.[1] as Record<string, unknown>;
    expect(typeof meta?.correlationId).toBe('string');
    expect(meta?.operation).toBe('frobnicate');
    expect(meta?.tool).toBe('manage_tools');
    expect(meta?.action).toBe('get_status');

    const failed = warnSpy.mock.calls.find((call) => call[0] === 'gateway request failed');
    expect(failed, 'request failure must be logged').toBeDefined();
    const failMeta = failed?.[1] as Record<string, unknown>;
    expect(failMeta?.errorCode).toBe('UNKNOWN_OPERATION');
    expect(failMeta?.correlationId).toBe(meta?.correlationId);

    debugSpy.mockRestore();
    warnSpy.mockRestore();
  });

  it('logs a completion line (not a failure) for a successful search', async () => {
    const logger = new Logger('c2-correlation-ok', 'debug');
    const debugSpy = vi.spyOn(logger, 'debug');
    const warnSpy = vi.spyOn(logger, 'warn');

    await handleUnrealGatewayCall({ operation: 'search', query: 'asset' }, makeContext(logger));

    const received = debugSpy.mock.calls.find((call) => call[0] === 'gateway request received');
    expect(received, 'request received must be logged for search').toBeDefined();
    expect(warnSpy.mock.calls.length).toBe(0);
    debugSpy.mockRestore();
    warnSpy.mockRestore();
  });
});
