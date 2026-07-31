// Characterization + preservation contract for the gateway EXECUTE seam.
//
// Task 24 extracts `executeGatewayCall` out of `src/server/tool-registry-gateway.ts`
// into `src/server/gateway/gateway-execute.ts` so Task 26 can own execute
// semantics without touching the orchestrator. This file was written and run
// GREEN against the pre-extraction implementation, so every expectation here is
// observed pre-existing behavior, not a new contract. It must stay GREEN across
// the extraction: that is what makes the seam "behavior-preserving".
//
// Task 26 owns the NEW execute semantics. It may add cases; it must not need to
// weaken one below to land the seam itself.

import { afterEach, describe, expect, it, vi } from 'vitest';
import { Logger } from '../../../src/utils/logging/logger.js';
import type { ITools } from '../../../src/types/tools/tool-interfaces.js';
import type { GatewayContext } from '../../../src/server/tool-registry-gateway.js';
import { handleUnrealGatewayCall } from '../../../src/server/tool-registry-gateway.js';
import { isRecord } from '../../../src/utils/validation/type-guards.js';
import { dynamicToolManager } from '../../../src/tools/dynamic/dynamic-tool-manager.js';
import { capabilityIndex, legacyPairKey } from '../../../src/server/gateway/gateway-capability-index.js';
import { minimalValidOutput } from './support/capability-fixtures.js';

const dispatched: Array<{ tool: string; args: Record<string, unknown> }> = [];

// Task 26 validates the handler result against the capability's declared output
// contract, so the stand-in payload this suite used before ({ success, echo })
// is now itself a contract violation. The mock answers with the smallest result
// the dispatched record actually promises, derived from the record rather than
// hand-written, so these cases still exercise dispatch instead of the output gate.
vi.mock('../../../src/tools/orchestration/consolidated-tool-handlers.js', () => ({
  handleConsolidatedToolCall: vi.fn(async (tool: string, args: Record<string, unknown>) => {
    dispatched.push({ tool, args });
    const action = typeof args.action === 'string' ? args.action : '';
    const record = capabilityIndex().byLegacyPair.get(legacyPairKey(tool, action));
    return record === undefined ? { success: true } : minimalValidOutput(record);
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
    logger: new Logger('gateway-execute-seam', 'error'),
    elicitationTimeoutMs: 0,
    ensureConnected: async () => connected
  };
}

async function execute(args: Record<string, unknown>, connected = true): Promise<Record<string, unknown>> {
  return await handleUnrealGatewayCall({ operation: 'execute', ...args }, makeContext(connected));
}

afterEach(() => {
  dynamicToolManager.reset();
  dispatched.length = 0;
});

describe('execute seam: guided error envelopes are preserved verbatim', () => {
  it('UNKNOWN_TOOL keeps its message, suggestions and describe nextCall', async () => {
    const result = await execute({ tool: 'manage_asts' });
    expect(result.success).toBe(false);
    expect(result.operation).toBe('execute');
    expect(result.errorCode).toBe('UNKNOWN_TOOL');
    expect(result.message).toBe('Unknown tool. Call search before execute.');
    expect(result.error).toBe('Unknown tool. Call search before execute.');
    expect(result.suggestions).toContain('manage_asset');
    expect(result.nextCall).toEqual({ operation: 'describe', tool: 'manage_asset' });
  });

  it('UNKNOWN_ACTION keeps availableActions plus a describe nextCall on the closest action', async () => {
    const result = await execute({ tool: 'manage_tools', action: 'get_stat' });
    expect(result.errorCode).toBe('UNKNOWN_ACTION');
    expect(result.message).toBe('Unknown action for manage_tools. Call describe before execute.');
    expect(result.tool).toBe('manage_tools');
    expect(Array.isArray(result.availableActions)).toBe(true);
    expect(result.suggestions).toContain('get_status');
    expect(result.nextCall).toEqual({ operation: 'describe', tool: 'manage_tools', action: 'get_status' });
  });

  it('a missing action takes the same UNKNOWN_ACTION branch', async () => {
    const result = await execute({ tool: 'manage_tools' });
    expect(result.errorCode).toBe('UNKNOWN_ACTION');
    expect(result.tool).toBe('manage_tools');
  });

  it('TOOL_DISABLED keeps the configure nextCall for the disabled tool', async () => {
    dynamicToolManager.disableTools(['manage_asset']);
    const result = await execute({ tool: 'manage_asset', action: 'list' });
    expect(result.errorCode).toBe('TOOL_DISABLED');
    expect(result.message).toBe("Tool 'manage_asset' is disabled or unavailable.");
    expect(result.tool).toBe('manage_asset');
    expect(Array.isArray(result.suggestions)).toBe(true);
    expect(result.nextCall).toEqual({ operation: 'configure', tool: 'manage_asset' });
  });

  it('INVALID_PARAMS (non-object params) keeps tool/action plus a describe nextCall', async () => {
    const result = await execute({ tool: 'manage_tools', action: 'get_status', params: 'nope' });
    expect(result.errorCode).toBe('INVALID_PARAMS');
    expect(result.message).toBe('params must be an object.');
    expect(result.tool).toBe('manage_tools');
    expect(result.action).toBe('get_status');
    expect(Array.isArray(result.suggestions)).toBe(true);
    expect(result.nextCall).toEqual({ operation: 'describe', tool: 'manage_tools', action: 'get_status' });
  });

  it('INVALID_PARAMS (action override) stays bare — no suggestions, no nextCall', async () => {
    const result = await execute({ tool: 'manage_tools', action: 'get_status', params: { action: 'hack' } });
    expect(result.errorCode).toBe('INVALID_PARAMS');
    expect(result.message).toBe('params must not override action or subAction. Supply the selected action at the gateway level.');
    expect(result.suggestions).toBeUndefined();
    expect(result.nextCall).toBeUndefined();
  });

  it('subAction override takes the same bare INVALID_PARAMS branch', async () => {
    const result = await execute({ tool: 'manage_tools', action: 'get_status', params: { subAction: 'hack' } });
    expect(result.errorCode).toBe('INVALID_PARAMS');
    expect(result.nextCall).toBeUndefined();
  });

  it('UNDECLARED_PARAMETER keeps allowedParameters plus a describe nextCall', async () => {
    const result = await execute({ tool: 'manage_tools', action: 'get_status', params: { bogus: 1 } });
    expect(result.errorCode).toBe('UNDECLARED_PARAMETER');
    // Task 26 supersession: the canonical validator names the offending key and
    // the capability, matching the native wording (McpNativeGatewaySchemaValidation
    // "Undeclared parameter '%s'") that the pre-canonical tool-union text did not.
    expect(result.message).toContain("Undeclared parameter 'bogus' for manage_tools.get_status");
    expect(Array.isArray(result.allowedParameters)).toBe(true);
    expect(isRecord(result.nextCall)).toBe(true);
    expect((result.nextCall as Record<string, unknown>).operation).toBe('describe');
    expect((result.nextCall as Record<string, unknown>).tool).toBe('manage_tools');
  });

  it('NOT_CONNECTED keeps tool/action plus a search nextCall', async () => {
    const result = await execute({ tool: 'manage_tools', action: 'get_status', params: {} }, false);
    expect(result.errorCode).toBe('NOT_CONNECTED');
    expect(result.message).toBe('Unreal Engine is not connected.');
    expect(result.tool).toBe('manage_tools');
    expect(result.action).toBe('get_status');
    expect(result.nextCall).toEqual({ operation: 'search' });
  });
});

describe('execute seam: dispatch and success envelope are preserved verbatim', () => {
  it('dispatches through handleConsolidatedToolCall with action and subAction merged in', async () => {
    const result = await execute({ tool: 'manage_tools', action: 'get_status', params: {} });
    expect(result.success).toBe(true);
    expect(result.operation).toBe('execute');
    expect(result.tool).toBe('manage_tools');
    expect(result.action).toBe('get_status');
    expect(dispatched).toHaveLength(1);
    expect(dispatched[0].tool).toBe('manage_tools');
    expect(dispatched[0].args.action).toBe('get_status');
    expect(dispatched[0].args.subAction).toBe('get_status');
  });

  it('system_control:get_project_settings still runs while disconnected', async () => {
    const result = await execute({ tool: 'system_control', action: 'get_project_settings', params: {} }, false);
    expect(result.errorCode).toBeUndefined();
    expect(result.success).toBe(true);
    expect(dispatched).toHaveLength(1);
  });

  it('every other action is blocked while disconnected before any dispatch', async () => {
    const result = await execute({ tool: 'manage_tools', action: 'get_status', params: {} }, false);
    expect(result.errorCode).toBe('NOT_CONNECTED');
    expect(dispatched).toHaveLength(0);
  });

  it('omitted params are treated as an empty object rather than rejected', async () => {
    const result = await execute({ tool: 'manage_tools', action: 'get_status' });
    expect(result.success).toBe(true);
    expect(dispatched).toHaveLength(1);
  });
});

describe('execute seam: orchestrator routing is preserved verbatim', () => {
  it('an unknown operation returns UNKNOWN_OPERATION echoing the operation name', async () => {
    const result = await handleUnrealGatewayCall({ operation: 'explode' }, makeContext());
    expect(result.success).toBe(false);
    expect(result.operation).toBe('explode');
    expect(result.errorCode).toBe('UNKNOWN_OPERATION');
    expect(result.message).toBe('operation must be search, describe, execute, or configure.');
  });

  it('a missing operation reports the literal "unknown" operation', async () => {
    const result = await handleUnrealGatewayCall({}, makeContext());
    expect(result.operation).toBe('unknown');
    expect(result.errorCode).toBe('UNKNOWN_OPERATION');
  });

  it('configure requires an action and rejects non-object params', async () => {
    const missing = await handleUnrealGatewayCall({ operation: 'configure' }, makeContext());
    expect(missing.errorCode).toBe('MISSING_ACTION');
    expect(missing.message).toBe('configure requires a manage_tools action.');

    const invalid = await handleUnrealGatewayCall({ operation: 'configure', action: 'get_status', params: 7 }, makeContext());
    expect(invalid.errorCode).toBe('INVALID_PARAMS');
    expect(invalid.message).toBe('params must be an object.');
  });

  it('configure runs manage_tools locally without reaching the execute dispatch', async () => {
    const result = await handleUnrealGatewayCall({ operation: 'configure', action: 'get_status' }, makeContext());
    expect(result.operation).toBe('configure');
    expect(result.action).toBe('get_status');
    expect(result.success).toBe(true);
    expect(dispatched).toHaveLength(0);
  });
});
