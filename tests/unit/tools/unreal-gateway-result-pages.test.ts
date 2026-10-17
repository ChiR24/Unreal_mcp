// A result too large for one reply is paged instead of refused: the first execute returns the items that fit and a
// page block whose nextCall reads on; the handler runs once, and the pages put together are the whole result.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';
import { afterEach, describe, expect, it } from 'vitest';

import { handleUnrealGatewayCall } from '../../../src/server/tool-registry-gateway.js';
import { resetPagesForTest } from '../../../src/server/gateway/gateway-result-pages.js';
import { EXECUTION_OPTION_KEYS } from '../../../src/tools/catalog/capabilities/semantic/execution-options.js';
import { gatewayContext } from './support/gateway-context-fixture.js';

const nativeExecute = (file: string): string =>
  readFileSync(join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'MCP', 'Execute', file), 'utf8')
    .replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');

const assets = Array.from({ length: 3000 }, (_, index) => ({
  path: `/Game/Props/Crates/SM_Crate_${String(index).padStart(4, '0')}`,
  name: `SM_Crate_${String(index).padStart(4, '0')}`,
  class: 'StaticMesh'
}));
const CALL = { operation: 'execute', tool: 'manage_asset', action: 'list', params: { path: '/Game/Props' } };

function context(calls: { count: number }) {
  return gatewayContext({
    isConnected: () => true,
    sendAutomationRequest: async () => {
      calls.count += 1;
      return { success: true, assets, totalCount: assets.length, count: assets.length, hasMore: false };
    }
  }, 'result-pages');
}

afterEach(() => resetPagesForTest());

describe('a result too large for one reply is paged', () => {
  it('reads to the end through page.nextCall with the handler run once', async () => {
    const calls = { count: 0 };
    const ctx = context(calls);
    let reply = await handleUnrealGatewayCall(CALL, ctx);
    expect(reply.success, JSON.stringify(reply).slice(0, 400)).toBe(true);
    const read: unknown[] = [];
    for (let guard = 0; guard < 20; guard += 1) {
      expect(JSON.stringify(reply).length, 'every page fits one reply').toBeLessThan(200_000);
      const data = reply.data as { assets: unknown[]; totalCount: number };
      expect(data.totalCount, 'the rest of the result rides with each page').toBe(3000);
      read.push(...data.assets);
      const page = reply.page as { offset: number; total: number; nextCall?: Record<string, unknown> };
      expect(page.total).toBe(3000);
      if (page.nextCall === undefined) break;
      reply = await handleUnrealGatewayCall(page.nextCall, ctx);
    }
    expect(read).toEqual(assets);
    expect(calls.count, 'pages come from the kept result, not a second run').toBe(1);
  });

  it('refuses a cursor sent with other params, and one that was never issued', async () => {
    const ctx = context({ count: 0 });
    const first = await handleUnrealGatewayCall(CALL, ctx);
    const nextCall = (first.page as { nextCall: { options: { pageCursor: string } } }).nextCall;
    const otherParams = await handleUnrealGatewayCall({ ...CALL, params: { path: '/Game/Elsewhere' }, options: nextCall.options }, ctx);
    expect(otherParams.errorCode).toBe('PAGE_CURSOR_MISMATCH');
    const unknown = await handleUnrealGatewayCall({ ...CALL, options: { pageCursor: 'never-issued' } }, ctx);
    expect(unknown.errorCode).toBe('PAGE_CURSOR_EXPIRED');
  });

  it('holds every page to the scope the first call needed', async () => {
    const scopes = ['read'];
    const ctx = gatewayContext({
      isConnected: () => true,
      getAuthority: () => ({ profile: 'scoped:reader', scopes: [...scopes] }),
      sendAutomationRequest: async () => ({ success: true, assets, totalCount: assets.length, count: assets.length, hasMore: false })
    }, 'result-pages-scope');
    const first = await handleUnrealGatewayCall(CALL, ctx);
    const nextCall = (first.page as { nextCall: Record<string, unknown> }).nextCall;
    scopes.splice(0, 1, 'write');
    expect((await handleUnrealGatewayCall(nextCall, ctx)).errorCode).toBe('SCOPE_NOT_GRANTED');
  });

  it('the native door honours the same option keys and checks scope before it reads a page', () => {
    const keys = /McpExecutionOptionKeys\(\)[\s\S]*?\{([\s\S]*?)\};/u.exec(nativeExecute('McpNativeGatewayExecuteRequest.cpp'))?.[1] ?? '';
    expect([...keys.matchAll(/TEXT\("(\w+)"\)/gu)].map((match) => match[1]).sort()).toEqual([...EXECUTION_OPTION_KEYS].sort());
    const execute = nativeExecute('McpNativeTransportGatewayExecute.cpp');
    expect(execute.indexOf('CheckScope(')).toBeGreaterThan(0);
    expect(execute.indexOf('CheckScope(')).toBeLessThan(execute.indexOf('McpNextResultPage('));
  });
});
