// Plan Todo 17 (BB-062) - an image is one indivisible base64 string, so the flat
// gateway cap refused a working capture (a screenshot, a widget preview) with advice
// the caller cannot act on. The exemption used to be a hard-coded pair of capability
// ids, which left a 90 KB widget preview refused as RESULT_TOO_LARGE.
//
// The budget now follows the reply's shape, on both doors: 100000 characters plus the
// length of a top-level `imageBase64` string when that string is at most 6,000,000
// characters. The rest of any reply is still held to 100k, and the image has its own
// ceiling. That is the same field the MCP image content promotion reads.
//
// Written after the fix landed, so non-vacuity is proven by mutation: drop the image
// term and the image cases fail; widen it and the refusal cases pass.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';
import { describe, expect, it, vi } from 'vitest';

import { Logger } from '../../../src/utils/logging/logger.js';
import type { GatewayContext } from '../../../src/server/tool-registry-gateway.js';
import type { ITools } from '../../../src/types/tools/tool-interfaces.js';
import { handleUnrealGatewayCall } from '../../../src/server/tool-registry-gateway.js';
import * as dispatch from '../../../src/server/gateway/gateway-execute-dispatch.js';
import {
  MAX_EXECUTION_RESULT_CHARS,
  MAX_IMAGE_BASE64_CHARS,
  resultCharBudget
} from '../../../src/server/gateway/gateway-execute-dispatch.js';

// Over the 100k flat cap, far under the 6M image ceiling: the ONLY thing that can
// decide the image and non-image cases differently is the image term itself.
const PAYLOAD_CHARS = 200_000;
const WIDGET = '/Game/UI/WBP_HUD';

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

async function execute(
  capability: string,
  result: Record<string, unknown>,
  params: Record<string, unknown> = {}
): Promise<Record<string, unknown>> {
  handlerResult = { success: true, ...result };
  return (await handleUnrealGatewayCall(
    { operation: 'execute', capability, params },
    makeContext()
  )) as Record<string, unknown>;
}

const preview = (result: Record<string, unknown>): Promise<Record<string, unknown>> =>
  execute('blueprint.preview_widget', { widgetPath: WIDGET, ...result }, { widgetPath: WIDGET });

const nativeReceipt = (): string =>
  readFileSync(
    join(
      'plugins', 'McpAutomationBridge', 'Source', 'McpAutomationBridge', 'Private',
      'MCP', 'Gateway', 'McpNativeGatewayExecuteReceiptBuild.cpp'
    ),
    'utf8'
  ).replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');

describe('todo17 BB-062: an image reply earns its budget from its shape, whichever capability sent it', () => {
  it('a widget preview with a 200k image passes the budget', async () => {
    const result = await preview({ imageBase64: 'x'.repeat(PAYLOAD_CHARS), mimeType: 'image/png' });

    expect(result.errorCode).not.toBe('RESULT_TOO_LARGE');
    expect(result.success).toBe(true);
  });

  it.each([
    'control_editor.screenshot',
    'system_control.screenshot'
  ])('%s still survives a payload the flat cap would refuse', async (capability) => {
    const result = await execute(capability, { imageBase64: 'x'.repeat(PAYLOAD_CHARS) });

    expect(result.errorCode).not.toBe('RESULT_TOO_LARGE');
  });

  it('a capability that was never on the old list gets the same treatment', async () => {
    const result = await execute('asset.list', { imageBase64: 'x'.repeat(PAYLOAD_CHARS) });

    expect(result.errorCode).not.toBe('RESULT_TOO_LARGE');
  });

  it('the same 200k characters in a non-image field are still refused', async () => {
    const result = await preview({ note: 'x'.repeat(PAYLOAD_CHARS) });

    // The discriminator: identical bytes, opposite verdict.
    expect(result.errorCode).toBe('RESULT_TOO_LARGE');
    expect(typeof result.resultChars).toBe('number');
    expect(result.resultChars as number).toBeGreaterThan(100_000);
  });

  it('the rest of an image reply is still held to 100k', async () => {
    const image = 'x'.repeat(PAYLOAD_CHARS);

    expect((await preview({ imageBase64: image, note: 'y'.repeat(90_000) })).errorCode).not.toBe('RESULT_TOO_LARGE');
    expect((await preview({ imageBase64: image, note: 'y'.repeat(120_000) })).errorCode).toBe('RESULT_TOO_LARGE');
  });

  it('an image has its own ceiling: 6,000,000 characters pass, one more is refused', async () => {
    expect((await preview({ imageBase64: 'x'.repeat(MAX_IMAGE_BASE64_CHARS) })).errorCode).not.toBe('RESULT_TOO_LARGE');
    expect((await preview({ imageBase64: 'x'.repeat(MAX_IMAGE_BASE64_CHARS + 1) })).errorCode).toBe('RESULT_TOO_LARGE');
  });

  it('only a string earns the term: an object under imageBase64 is held to 100k', async () => {
    const result = await preview({ imageBase64: { data: 'x'.repeat(PAYLOAD_CHARS) } });

    expect(result.errorCode).toBe('RESULT_TOO_LARGE');
  });
});

describe('todo17 BB-062: the budget rule and its native mirror', () => {
  it('adds the image length to the flat cap, only for a top-level string within its own ceiling', () => {
    expect(MAX_EXECUTION_RESULT_CHARS).toBe(100_000);
    expect(MAX_IMAGE_BASE64_CHARS).toBe(6_000_000);
    expect(resultCharBudget({ success: true })).toBe(100_000);
    expect(resultCharBudget({ imageBase64: 'x'.repeat(10) })).toBe(100_010);
    expect(resultCharBudget({ imageBase64: 'x'.repeat(MAX_IMAGE_BASE64_CHARS) })).toBe(100_000 + MAX_IMAGE_BASE64_CHARS);
    expect(resultCharBudget({ imageBase64: 'x'.repeat(MAX_IMAGE_BASE64_CHARS + 1) })).toBe(100_000);
    expect(resultCharBudget({ data: { imageBase64: 'x'.repeat(10) } })).toBe(100_000);
    expect(resultCharBudget({ imageBase64: 12_345 })).toBe(100_000);
    expect(resultCharBudget('text')).toBe(100_000);
  });

  it('the native gateway applies the same rule by the same figures', () => {
    const native = nativeReceipt();

    expect(native).toMatch(/int32 ResultCharBudget = 100000;/u);
    expect(native).toMatch(
      /Result->TryGetStringField\(TEXT\("imageBase64"\), ImageBase64\) && ImageBase64\.Len\(\) <= 6000000\)\s*\{\s*ResultCharBudget \+= ImageBase64\.Len\(\);/u
    );
    expect(native).toMatch(/McpSerializedResultExceeds\(Result, ResultCharBudget, &SerializedChars\)/u);
  });

  it('neither door names a capability any more', () => {
    expect('IMAGE_PAYLOAD_CAPABILITIES' in dispatch).toBe(false);
    expect('MAX_IMAGE_RESULT_CHARS' in dispatch).toBe(false);
    expect(nativeReceipt()).not.toMatch(/control_editor\.screenshot|system_control\.screenshot|bIsImagePayload/u);
  });
});

// A screenshot answered only a file path unless returnBase64 was set, and a model on the far side
// of the bridge cannot open that file, so "take a screenshot" showed it nothing. The image now
// comes back inline by default, fitted to 1600x900 when no resolution is given (a native-size PNG
// is what used to blow the base64 cap); returnBase64 false keeps the file-only, full-size capture.
describe('a screenshot hands its image back by default', () => {
  const plugin = join('plugins', 'McpAutomationBridge', 'Source', 'McpAutomationBridge', 'Private');
  const read = (...segments: string[]): string => readFileSync(join(plugin, ...segments), 'utf8');

  it('returnBase64 defaults to true and an inline image without a resolution fits the box', () => {
    const resample = read('Foundation', 'McpScreenshotResample.cpp');
    expect(resample).toMatch(/bool McpScreenshotReturnsImage[\s\S]*bool bReturnBase64 = true;/u);
    expect(resample).toMatch(/if \(Resolution\.IsEmpty\(\) && McpScreenshotReturnsImage\(Payload\)\) \{\s*Resolution = McpInlineScreenshotBox;/u);
    expect(read('Foundation', 'McpScreenshotResample.h')).toMatch(/McpInlineScreenshotBox = TEXT\("1600x900"\)/u);
  });

  it('both screenshot capabilities read the same default', () => {
    expect(read('Domains', 'ControlEditor', 'McpAutomationBridge_ControlEditorScreenshotSupport.cpp'))
      .toMatch(/const bool bReturnBase64 = McpScreenshotReturnsImage\(Payload\);/u);
    expect(read('Domains', 'Ui', 'McpAutomationBridge_UiHandlersScreenshot.cpp'))
      .toMatch(/const bool bReturnBase64 = McpScreenshotReturnsImage\(Payload\);/u);
  });
});
