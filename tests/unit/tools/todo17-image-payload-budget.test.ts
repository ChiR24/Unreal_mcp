// Plan Todo 17 (BB-062) - a screenshot is one indivisible base64 image, so the
// flat gateway cap refused a working capture with advice the caller cannot act
// on. The native transport already exempts exactly two capabilities; this pins
// the TypeScript mirror so the same call cannot succeed over /mcp and fail over
// stdio.
//
// Written after the fix landed, so non-vacuity is proven by mutation: toggle the
// exemption off and the discriminating case fails.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';
import { describe, expect, it, vi } from 'vitest';

import { Logger } from '../../../src/utils/logging/logger.js';
import type { GatewayContext } from '../../../src/server/tool-registry-gateway.js';
import type { ITools } from '../../../src/types/tools/tool-interfaces.js';
import { handleUnrealGatewayCall } from '../../../src/server/tool-registry-gateway.js';
import {
  IMAGE_PAYLOAD_CAPABILITIES,
  MAX_EXECUTION_RESULT_CHARS,
  MAX_IMAGE_RESULT_CHARS
} from '../../../src/server/gateway/gateway-execute-dispatch.js';

// Over the 100k flat cap, far under the 6M image budget: the ONLY thing that can
// decide these two cases differently is the image exemption itself.
const PAYLOAD_CHARS = 200_000;

let handlerResult: unknown = { success: true };

const handleConsolidatedToolCall = vi.fn(async (_tool: string, _payload: Record<string, unknown>): Promise<unknown> => handlerResult);

function makeContext(): GatewayContext {
  const tools = {
    automationBridge: {
      isConnected: () => true,
      sendAutomationRequest: async (tool: string, payload: Record<string, unknown>) => handleConsolidatedToolCall(tool, payload)
    }
  } as unknown as ITools;
  return {
    tools,
    logger: new Logger('todo17-image-budget', 'error'),
    ensureConnected: async () => true
  };
}

async function executeWithOversizedResult(capability: string): Promise<Record<string, unknown>> {
  handlerResult = { success: true, imageBase64: 'x'.repeat(PAYLOAD_CHARS) };
  return (await handleUnrealGatewayCall(
    { operation: 'execute', capability, params: {} },
    makeContext()
  )) as Record<string, unknown>;
}

const nativeReceipt = (): string =>
  readFileSync(
    join(
      'plugins', 'McpAutomationBridge', 'Source', 'McpAutomationBridge', 'Private',
      'MCP', 'Gateway', 'McpNativeGatewayExecuteReceiptBuild.cpp'
    ),
    'utf8'
  );

describe('todo17 BB-062: an indivisible image payload is not refused as pageable', () => {
  it.each([
    'control_editor.screenshot',
    'system_control.screenshot'
  ])('%s survives a payload the flat cap would refuse', async (capability) => {
    const result = await executeWithOversizedResult(capability);

    expect(result.errorCode).not.toBe('RESULT_TOO_LARGE');
  });

  it('a non-image capability with the SAME payload is still refused', async () => {
    const result = await executeWithOversizedResult('asset.list');

    // The discriminator: identical bytes, opposite verdict. If the exemption
    // were removed both cases refuse; if it were unscoped neither would.
    expect(result.errorCode).toBe('RESULT_TOO_LARGE');
    expect(typeof result.resultChars).toBe('number');
    expect(result.resultChars as number).toBeGreaterThan(100_000);
  });
});

describe('todo17 BB-062: the exemption mirrors the native budget', () => {
  it('uses the native figures', () => {
    expect(MAX_EXECUTION_RESULT_CHARS).toBe(100_000);
    expect(MAX_IMAGE_RESULT_CHARS).toBe(6_000_000);
    expect(nativeReceipt()).toMatch(/ResultCharBudget = bIsImagePayload \? 6000000 : 100000;/u);
  });

  it('exempts exactly the capabilities the native side names, and no others', () => {
    expect([...IMAGE_PAYLOAD_CAPABILITIES]).toEqual(['control_editor.screenshot', 'system_control.screenshot']);
    const native = nativeReceipt();
    for (const id of IMAGE_PAYLOAD_CAPABILITIES) {
      expect(native).toContain(`CapabilityId == TEXT("${id}")`);
    }
  });
});
