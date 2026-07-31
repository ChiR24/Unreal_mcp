/// <reference types="node" />

import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { afterEach, describe, expect, it, vi } from 'vitest';

import { Logger } from '../../../src/utils/logging/logger.js';
import type { ITools } from '../../../src/types/tools/tool-interfaces.js';
import type { GatewayContext } from '../../../src/server/tool-registry-gateway.js';
import { handleUnrealGatewayCall } from '../../../src/server/tool-registry-gateway.js';
import { isRecord } from '../../../src/utils/validation/type-guards.js';
import { dynamicToolManager } from '../../../src/tools/dynamic/dynamic-tool-manager.js';
import { handleConsolidatedToolCall } from '../../../src/tools/orchestration/consolidated-tool-handlers.js';
import { unsupportedPreviewMessage } from '../../../src/server/gateway/gateway-execute-validate.js';

vi.mock('../../../src/tools/orchestration/consolidated-tool-handlers.js', () => ({
  handleConsolidatedToolCall: vi.fn(async () => ({ success: true, message: 'ok' }))
}));

// The dispatch mock IS the editor. Every assertion that it was not called is an
// assertion that no irreversible editor work was performed.
const dispatch = vi.mocked(handleConsolidatedToolCall);

function makeContext(connected: boolean): GatewayContext {
  const tools: ITools = {
    systemTools: {
      executeConsoleCommand: async () => ({ success: false }),
      getProjectSettings: async () => ({})
    },
    assetResources: { list: async () => ({}) }
  };
  return {
    tools,
    logger: new Logger('task43-preview', 'error'),
    elicitationTimeoutMs: 0,
    ensureConnected: async () => connected
  };
}

const asRecord = (value: unknown): Record<string, unknown> => (isRecord(value) ? value : {});

async function execute(
  args: Record<string, unknown>,
  connected = true
): Promise<Record<string, unknown>> {
  return await handleUnrealGatewayCall({ operation: 'execute', ...args }, makeContext(connected));
}

// asset.delete: behavior.effect === 'destructive', behavior.supportsPreview === true.
// Nothing about this request is rejected before dispatch today.
const DELETE_CALL = {
  capability: 'asset.delete',
  params: { assetPath: '/Game/Task43/DoomedAsset' }
} as const;

afterEach(() => {
  dynamicToolManager.reset();
  vi.clearAllMocks();
});

// Task 43 prohibition: "Do not fake dry runs."
//
// RED (today's behavior): `options.preview` is accepted and type-checked as a
// boolean by BOTH transports, and then read by NOTHING. `options.preview`
// appears nowhere in gateway-execute-dispatch.ts, gateway-execute.ts, or
// src/tools/handlers/foundation/dispatch/. So a client that sends
// `preview: true` to ask "show me what WOULD happen" gets the real,
// irreversible mutation, and gateway-execute-envelope.ts echoes
// `options: { preview: true }` back inside the SUCCESS envelope as if the dry
// run had been honoured.
//
// No dispatch path can preview, so the only honest, fail-closed behavior is to
// refuse before dispatch. Reporting unsupported preview explicitly is the fix;
// inventing a diff or an "estimated cost" would be a second lie.
describe('task 43: options.preview must refuse, never fake a dry run', () => {
  it('refuses preview on a destructive capability instead of really deleting', async () => {
    const result = await execute({ ...DELETE_CALL, options: { preview: true } });

    // This is the bug in one line: today the delete is dispatched for real.
    expect(dispatch, 'preview:true must reach NO editor work').not.toHaveBeenCalled();
    expect(result.success).toBe(false);
    expect(result.errorCode).toBe('UNSUPPORTED_PREVIEW');
  });

  it('never returns a success envelope echoing an unhonoured preview flag', async () => {
    const result = await execute({ ...DELETE_CALL, options: { preview: true } });

    expect(result.success).toBe(false);
    // executeSuccessEnvelope echoes `options` verbatim; a refusal has no options
    // to echo, so `preview: true` can never be reflected as though it was honoured.
    expect(result.options).toBeUndefined();
  });

  it('refuses even when the capability record declares supportsPreview: true', async () => {
    // asset.duplicate declares behavior.supportsPreview === true. 124 records do,
    // including 10 destructive ones. That declaration is unbacked: no dispatch
    // path reads the option, so honouring the declaration would preserve the fake
    // dry run for exactly the most dangerous capabilities.
    const result = await execute({
      capability: 'asset.duplicate',
      params: { sourcePath: '/Game/Task43/Source' },
      options: { preview: true }
    });

    expect(dispatch).not.toHaveBeenCalled();
    expect(result.errorCode).toBe('UNSUPPORTED_PREVIEW');
  });

  it('refuses a read-only capability with the same code, so one rule covers the catalog', async () => {
    // asset.list is behavior.effect === 'read'. It is refused too: a declared
    // effect is metadata, not proof, and a uniform rule is the one a client can
    // reason about. Refusing costs a caller one retry; guessing wrong mutates.
    const result = await execute({
      capability: 'asset.list',
      params: { path: '/Game' },
      options: { preview: true }
    });

    expect(dispatch).not.toHaveBeenCalled();
    expect(result.errorCode).toBe('UNSUPPORTED_PREVIEW');
  });

  it('refuses before the connection gate, so no bridge or queue work is attempted', async () => {
    const result = await execute({ ...DELETE_CALL, options: { preview: true } }, false);

    // NOT_CONNECTED would prove the refusal happened at/after the connection gate.
    expect(result.errorCode).toBe('UNSUPPORTED_PREVIEW');
    expect(dispatch).not.toHaveBeenCalled();
  });

  it('names the capability and explains that no dry run exists', async () => {
    const result = await execute({ ...DELETE_CALL, options: { preview: true } });

    expect(result.message).toBe(unsupportedPreviewMessage('asset.delete'));
  });

  it('returns executable guidance that re-runs the same call without preview', async () => {
    const result = await execute({
      ...DELETE_CALL,
      options: { preview: true, timeoutMs: 1000 }
    });

    const nextCall = asRecord(result.nextCall);
    expect(nextCall.operation).toBe('execute');
    expect(nextCall.tool).toBe('manage_asset');
    expect(nextCall.action).toBe('delete');
    expect(asRecord(nextCall.params).assetPath).toBe('/Game/Task43/DoomedAsset');
    // The remaining options survive; only the unhonoured control is dropped.
    expect(asRecord(nextCall.options).timeoutMs).toBe(1000);
    expect(asRecord(nextCall.options).preview).toBeUndefined();
    expect(result.suggestions).not.toContain('preview');
  });

  it('drops the options envelope from nextCall when preview was the only option', async () => {
    const result = await execute({ ...DELETE_CALL, options: { preview: true } });

    expect(asRecord(result.nextCall).options).toBeUndefined();
  });
});

// The fix must be narrow: it may only intercept a request that actually asked
// for a dry run. Everything else keeps dispatching exactly as before.
describe('task 43 control: the refusal is narrow', () => {
  it('still dispatches a normal call that never mentions preview', async () => {
    const result = await execute({ ...DELETE_CALL });

    expect(dispatch).toHaveBeenCalledTimes(1);
    expect(result.success).toBe(true);
  });

  it('still dispatches when preview is explicitly false', async () => {
    const result = await execute({ ...DELETE_CALL, options: { preview: false } });

    expect(dispatch).toHaveBeenCalledTimes(1);
    expect(result.success).toBe(true);
  });

  it('keeps the existing INVALID_OPTIONS refusal for a non-boolean preview', async () => {
    const result = await execute({ ...DELETE_CALL, options: { preview: 'yes' } });

    expect(dispatch).not.toHaveBeenCalled();
    expect(result.errorCode).toBe('INVALID_OPTIONS');
  });

  it('still dispatches other honoured options untouched', async () => {
    const result = await execute({ ...DELETE_CALL, options: { timeoutMs: 1000 } });

    expect(dispatch).toHaveBeenCalledTimes(1);
    expect(result.success).toBe(true);
  });
});

// Both transports must refuse with the SAME code string. A live `/mcp` editor
// harness is not available in unit scope and UE BuildPlugin is the authoritative
// compile gate, so native equivalence is asserted against the plugin source the
// same way the Task 27/39 native contracts are.
describe('task 43 transport equivalence: native /mcp refuses identically', () => {
  const executeDir = resolve(
    process.cwd(),
    'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Execute'
  );
  const read = (file: string): string => readFileSync(resolve(executeDir, file), 'utf8');

  it('emits the same UNSUPPORTED_PREVIEW code string', () => {
    expect(read('McpNativeGatewayPreview.cpp')).toContain('UNSUPPORTED_PREVIEW');
  });

  it('emits the same refusal message as TypeScript', () => {
    const native = read('McpNativeGatewayPreview.cpp');
    const shared = unsupportedPreviewMessage('%s').split('%s');
    for (const fragment of shared) {
      expect(native, `native message must contain "${fragment}"`).toContain(fragment);
    }
  });

  it('gates preview during request parsing, before the subsystem queue', () => {
    // ValidateAndResolveGatewayExecute owns the parse, and the Task 27 contract
    // already pins it to run before StreamToolCall.
    expect(read('McpNativeGatewayExecuteRequest.cpp'))
      .toContain('McpValidateExecuteOptionsForCapability(');
  });

  it('does not let a record\'s supportsPreview declaration buy a pass', () => {
    expect(read('McpNativeGatewayPreview.cpp')).not.toContain('SupportsPreview');
  });

  it('points the refusal at the same /options/preview pointer', () => {
    expect(read('McpNativeGatewayPreview.cpp')).toContain('/options/preview');
  });
});
