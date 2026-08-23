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

// Over the 100k flat cap, far under the 6M image budget: the ONLY thing that can
// decide these two cases differently is the image exemption itself.
const PAYLOAD_CHARS = 200_000;

let handlerResult: unknown = { success: true };

vi.mock('../../../src/tools/orchestration/consolidated-tool-handlers.js', () => ({
  handleConsolidatedToolCall: vi.fn(async () => handlerResult)
}));

function makeContext(): GatewayContext {
  const tools = {
    systemTools: {
      executeConsoleCommand: async () => ({ success: false }),
      getProjectSettings: async () => ({})
    },
    assetResources: { list: async () => ({}) }
  } as unknown as ITools;
  return {
    tools,
    logger: new Logger('todo17-image-budget', 'error'),
    elicitationTimeoutMs: 0,
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

const dispatch = (): string =>
  readFileSync(join('src', 'server', 'gateway', 'gateway-execute-dispatch.ts'), 'utf8');

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
    const result = await executeWithOversizedResult('manage_tools.get_status');

    // The discriminator: identical bytes, opposite verdict. If the exemption
    // were removed both cases refuse; if it were unscoped neither would.
    expect(result.errorCode).toBe('RESULT_TOO_LARGE');
    expect(typeof result.resultChars).toBe('number');
    expect(result.resultChars as number).toBeGreaterThan(100_000);
  });
});

describe('todo17 BB-062: the exemption is scoped and mirrors the native budget', () => {
  it('keeps the flat cap as a terminated constant', () => {
    // Anchored through the semicolon so a widened literal cannot satisfy it.
    expect(dispatch()).toMatch(/const MAX_EXECUTION_RESULT_CHARS = 100_000;/u);
  });

  it('raises the image budget to exactly the native figure', () => {
    expect(dispatch()).toMatch(/const MAX_IMAGE_RESULT_CHARS = 6_000_000;/u);
    // 6000000 on the native side, same number, different literal spelling.
    expect(nativeReceipt()).toMatch(/ResultCharBudget = bIsImagePayload \? 6000000 : 100000;/u);
  });

  it('exempts exactly the two capabilities the native side names, and no others', () => {
    const source = dispatch();
    const set = source.slice(
      source.indexOf('IMAGE_PAYLOAD_CAPABILITIES'),
      source.indexOf('export type GatewayContext')
    );
    const ids = [...set.matchAll(/'([a-z_]+\.[a-z_]+)'/gu)].map((match) => match[1]);

    expect(ids).toEqual(['control_editor.screenshot', 'system_control.screenshot']);

    const native = nativeReceipt();
    for (const id of ids) {
      expect(native).toContain(`CapabilityId == TEXT("${id}")`);
    }
  });

  it('selects the budget per capability rather than raising it for everyone', () => {
    const source = dispatch();

    // The membership test must feed the budget, and the budget must feed the
    // size predicate; a raise applied unconditionally would drop the `has(`.
    expect(source).toMatch(
      /const resultCharBudget = IMAGE_PAYLOAD_CAPABILITIES\.has\(record\.id\)\s*\?\s*MAX_IMAGE_RESULT_CHARS\s*:\s*MAX_EXECUTION_RESULT_CHARS;/u
    );
    expect(source).toMatch(/serialized\.length > resultCharBudget;/u);
  });
});
