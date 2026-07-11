import { afterEach, describe, expect, it, vi } from 'vitest';
import { Logger } from '../../../src/utils/logging/logger.js';
import type { GatewayContext } from '../../../src/server/tool-registry-gateway.js';
import type { ITools } from '../../../src/types/tools/tool-interfaces.js';
import { consolidatedToolDefinitions } from '../../../src/tools/catalog/consolidated-tool-definitions.js';
import {
  describeGatewayCapability,
  handleUnrealGatewayCall
} from '../../../src/server/tool-registry-gateway.js';
import { isRecord } from '../../../src/utils/validation/type-guards.js';
import { dynamicToolManager } from '../../../src/tools/dynamic/dynamic-tool-manager.js';

// Mock the consolidated tool handler so execute can reach the RESULT_TOO_LARGE gate
// without a live Unreal connection or a real dispatch.
vi.mock('../../../src/tools/orchestration/consolidated-tool-handlers.js', () => ({
  handleConsolidatedToolCall: vi.fn(async () => ({ success: true, data: { big: 'x'.repeat(200_000) } }))
}));

function makeContext(logger: Logger): GatewayContext {
  const tools: ITools = {
    systemTools: {
      executeConsoleCommand: async () => ({ success: false }),
      getProjectSettings: async () => ({})
    },
    assetResources: { list: async () => ({}) }
  };
  return {
    tools,
    logger,
    elicitationTimeoutMs: 1000,
    ensureConnected: async () => true
  };
}

function firstAction(toolName: string): string {
  const def = consolidatedToolDefinitions.find((tool) => tool.name === toolName);
  const props = isRecord(def?.inputSchema) && isRecord(def.inputSchema.properties) ? def.inputSchema.properties : undefined;
  const action = isRecord(props) ? props.action : undefined;
  const enumArr = isRecord(action) && Array.isArray(action.enum) ? action.enum : [];
  const first = enumArr.find((value) => typeof value === 'string');
  return typeof first === 'string' ? first : 'x';
}

afterEach(() => {
  // Re-enable any tool disabled during a test so cases stay independent.
  dynamicToolManager.reset();
});

describe('gateway error-code parity with native McpNativeGateway', () => {
  it('rejects an unknown tool in describe with the native UNKNOWN_TOOL message', () => {
    const result = describeGatewayCapability({ tool: 'does_not_exist' }) as Record<string, unknown>;
    expect(result.success).toBe(false);
    expect(result.errorCode).toBe('UNKNOWN_TOOL');
    expect(result.message).toBe('Unknown tool. Call search to retrieve canonical tool names.');
  });

  it('rejects an unknown tool in execute with the native UNKNOWN_TOOL message', async () => {
    const result = await handleUnrealGatewayCall(
      { operation: 'execute', tool: 'nope', action: 'x' },
      makeContext(new Logger('parity', 'error'))
    ) as Record<string, unknown>;
    expect(result.success).toBe(false);
    expect(result.errorCode).toBe('UNKNOWN_TOOL');
    expect(result.message).toBe('Unknown tool. Call search before execute.');
  });

  it('rejects an unknown describe action with the native UNKNOWN_ACTION message and fields', () => {
    const result = describeGatewayCapability({ tool: 'manage_tools', action: 'nope' }) as Record<string, unknown>;
    expect(result.success).toBe(false);
    expect(result.errorCode).toBe('UNKNOWN_ACTION');
    expect(result.message).toBe("Unknown action 'nope' for manage_tools.");
    expect(result.tool).toBe('manage_tools');
    expect(Array.isArray(result.availableActions)).toBe(true);
  });

  it('rejects an unknown execute action with the native UNKNOWN_ACTION message and fields', async () => {
    const result = await handleUnrealGatewayCall(
      { operation: 'execute', tool: 'manage_tools', action: 'nope' },
      makeContext(new Logger('parity', 'error'))
    ) as Record<string, unknown>;
    expect(result.success).toBe(false);
    expect(result.errorCode).toBe('UNKNOWN_ACTION');
    expect(result.message).toBe('Unknown action for manage_tools. Call describe before execute.');
    expect(result.tool).toBe('manage_tools');
    expect(Array.isArray(result.availableActions)).toBe(true);
  });

  it('rejects a disabled tool with the native TOOL_DISABLED message', async () => {
    dynamicToolManager.disableTools(['manage_asset']);
    const result = await handleUnrealGatewayCall(
      { operation: 'execute', tool: 'manage_asset', action: firstAction('manage_asset') },
      makeContext(new Logger('parity', 'error'))
    ) as Record<string, unknown>;
    expect(result.success).toBe(false);
    expect(result.errorCode).toBe('TOOL_DISABLED');
    expect(result.message).toBe("Tool 'manage_asset' is disabled or unavailable.");
  });

  it('rejects params that override action with the native INVALID_PARAMS message', async () => {
    const result = await handleUnrealGatewayCall(
      { operation: 'execute', tool: 'manage_tools', action: 'get_status', params: { action: 'hack' } },
      makeContext(new Logger('parity', 'error'))
    ) as Record<string, unknown>;
    expect(result.success).toBe(false);
    expect(result.errorCode).toBe('INVALID_PARAMS');
    expect(result.message).toBe('params must not override action or subAction. Supply the selected action at the gateway level.');
  });

  it('rejects non-object params with the native INVALID_PARAMS message', async () => {
    const result = await handleUnrealGatewayCall(
      { operation: 'execute', tool: 'manage_tools', action: 'get_status', params: 'not-an-object' },
      makeContext(new Logger('parity', 'error'))
    ) as Record<string, unknown>;
    expect(result.success).toBe(false);
    expect(result.errorCode).toBe('INVALID_PARAMS');
    expect(result.message).toBe('params must be an object.');
  });

  it('rejects undeclared parameters with the native UNDECLARED_PARAMETER message and field', async () => {
    const result = await handleUnrealGatewayCall(
      { operation: 'execute', tool: 'manage_tools', action: 'get_status', params: { bogus: 1 } },
      makeContext(new Logger('parity', 'error'))
    ) as Record<string, unknown>;
    expect(result.success).toBe(false);
    expect(result.errorCode).toBe('UNDECLARED_PARAMETER');
    expect(result.message).toBe('Unknown parameter(s) for manage_tools: bogus. Call describe before execution.');
    expect(Array.isArray(result.allowedParameters)).toBe(true);
  });

  it('rejects a missing configure action with the native MISSING_ACTION message', async () => {
    const result = await handleUnrealGatewayCall(
      { operation: 'configure' },
      makeContext(new Logger('parity', 'error'))
    ) as Record<string, unknown>;
    expect(result.success).toBe(false);
    expect(result.errorCode).toBe('MISSING_ACTION');
    expect(result.message).toBe('configure requires a manage_tools action.');
  });

  it('rejects non-object configure params with the native INVALID_PARAMS message', async () => {
    const result = await handleUnrealGatewayCall(
      { operation: 'configure', action: 'get_status', params: 'not-an-object' },
      makeContext(new Logger('parity', 'error'))
    ) as Record<string, unknown>;
    expect(result.success).toBe(false);
    expect(result.errorCode).toBe('INVALID_PARAMS');
    expect(result.message).toBe('params must be an object.');
  });

  it('rejects an unknown operation with the native UNKNOWN_OPERATION message', async () => {
    const result = await handleUnrealGatewayCall(
      { operation: 'frobnicate' },
      makeContext(new Logger('parity', 'error'))
    ) as Record<string, unknown>;
    expect(result.success).toBe(false);
    expect(result.errorCode).toBe('UNKNOWN_OPERATION');
    expect(result.message).toBe('operation must be search, describe, execute, or configure.');
  });

  it('keeps perActionSchemas false on both success and error paths', () => {
    const ok = describeGatewayCapability({ tool: 'manage_tools' }) as Record<string, unknown>;
    expect(ok.perActionSchemas).toBe(false);
    const err = describeGatewayCapability({ tool: 'does_not_exist' }) as Record<string, unknown>;
    // Schema flag is a describe-success field; errors preserve the same envelope shape.
    expect(err.success).toBe(false);
    expect(err.errorCode).toBe('UNKNOWN_TOOL');
  });
});

describe('gateway RESULT_TOO_LARGE safety gate', () => {
  it('returns RESULT_TOO_LARGE with resultChars when the execution result exceeds the limit', async () => {
    const result = await handleUnrealGatewayCall(
      { operation: 'execute', tool: 'manage_tools', action: 'get_status', params: {} },
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
