/// <reference types="node" />

import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

const read = (path: string) => readFileSync(resolve(process.cwd(), path), 'utf8').replace(/\r\n/gu, '\n');
const native = read('plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Gateway/McpNativeGatewayExecuteReceiptBuild.cpp');
const ts = read('src/server/gateway/gateway-execute-dispatch.ts');

describe('RESULT_TOO_LARGE guidance parity', () => {
  it('names the capability narrowing parameters on the native gateway too', () => {
    expect(native).toContain('return McpBuildErrorReceipt(CapabilityId, TooLarge, Context, NarrowingGuidance(CapabilityId));');
    expect(native).toContain('FString::Printf(TEXT("narrow with \'%s\'"), *Pair.Key)');
  });

  it('matches the same words on both gateways, in the same order', () => {
    const tsWords = /const NARROWING_PARAM = \/([^/]+)\/i;/u.exec(ts)?.[1].split('|') ?? [];
    const block = /NarrowingWords\[\] = \{([^}]*)\};/u.exec(native)?.[1] ?? '';
    const nativeWords = [...block.matchAll(/TEXT\("([^"]+)"\)/gu)].map((match) => match[1]);
    expect(tsWords.length).toBeGreaterThan(0);
    expect(nativeWords).toEqual(tsWords);
  });
});
