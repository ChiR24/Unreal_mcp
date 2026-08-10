import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { InMemoryTransport } from '@modelcontextprotocol/sdk/inMemory.js';
import { afterEach, describe, expect, it, vi } from 'vitest';

import { isRecord } from '../../../src/utils/validation/type-guards.js';

// Task 30: the single-`unreal` surface is permanent. `MCP_GATEWAY_MODE` is
// removed, so the env var can no longer restore a legacy multi-tool listing, and
// a direct call to a canonical parent tool is answered with an executable
// migration receipt instead of being routed.

// Pay the one-time transform cost during collection; each case still reloads
// the module after installing its own environment fixture.
await import('../../../src/server/server-factory.js');

async function buildWithGatewayMode(mode: string | undefined) {
    vi.resetModules();
    // Still stubbed on purpose: the contract is that setting the removed toggle
    // has no effect, so a `false` value must NOT resurrect the legacy surface.
    vi.stubEnv('MCP_GATEWAY_MODE', mode);
    vi.stubEnv('MOCK_UNREAL_CONNECTION', 'true');
    vi.stubEnv('NODE_ENV', 'test');

    const { createServer } = await import('../../../src/server/server-factory.js');
    const built = createServer();
    const client = new Client({ name: 'gw-behavior', version: '1.0.0' }, { capabilities: {} });
    const pair = InMemoryTransport.createLinkedPair();
    await built.server.connect(pair[1]);
    await client.connect(pair[0], { timeout: 15000 });
    return { built, client, pair };
}

function structuredPayload(response: unknown): Record<string, unknown> {
    if (!isRecord(response)) return {};
    if (isRecord(response.structuredContent)) return response.structuredContent;
    const content = response.content;
    if (Array.isArray(content)) {
        for (const part of content) {
            if (isRecord(part) && typeof part.text === 'string') {
                return JSON.parse(part.text) as Record<string, unknown>;
            }
        }
    }
    return {};
}

describe('permanent single-`unreal` public surface (Task 30)', () => {
    let ctx: Awaited<ReturnType<typeof buildWithGatewayMode>> | undefined;

    afterEach(async () => {
        if (ctx) {
            await ctx.pair[0].close();
            ctx.built.automationBridge?.stop();
            ctx.built.bridge?.dispose();
            ctx.built.metricsServer?.close();
        }
        ctx = undefined;
        vi.unstubAllEnvs();
    });

    it('exposes exactly one public tool (`unreal`) when no gateway env var is set', async () => {
        // Given a server started with no legacy toggle present...
        ctx = await buildWithGatewayMode(undefined);
        // When the client lists tools...
        const list = await ctx.client.listTools(undefined, { timeout: 15000 });
        // Then the single `unreal` gateway tool is the entire public surface.
        expect(list.tools).toHaveLength(1);
        expect(list.tools[0]?.name).toBe('unreal');
    });

    it('still exposes only `unreal` when MCP_GATEWAY_MODE=false (removed toggle is inert)', async () => {
        // Given a client that sets the removed toggle to `false`...
        ctx = await buildWithGatewayMode('false');
        // When the client lists tools...
        const list = await ctx.client.listTools(undefined, { timeout: 15000 });
        // Then the toggle is inert: the surface is permanently the single tool.
        expect(list.tools).toHaveLength(1);
        expect(list.tools[0]?.name).toBe('unreal');
        expect(list.tools.find((tool) => tool.name === 'manage_tools')).toBeUndefined();
    });

    it('refuses a direct canonical-tool call with an executable DIRECT_TOOL_CALL_REMOVED migration', async () => {
        // Given the permanent single-tool surface...
        ctx = await buildWithGatewayMode(undefined);
        // When a client calls a canonical parent tool (`manage_asset`) directly...
        const response = await ctx.client.callTool(
            { name: 'manage_asset', arguments: { action: 'list_assets', assetPath: '/Game/Env' } },
            undefined,
            { timeout: 15000 }
        );
        // Then it is refused with a bounded, copy-paste-executable migration receipt.
        expect(response.isError).toBe(true);
        const sc = structuredPayload(response);
        expect(sc.errorCode).toBe('DIRECT_TOOL_CALL_REMOVED');
        expect(sc.nextCall).toEqual({
            operation: 'execute',
            tool: 'manage_asset',
            action: 'list_assets',
            params: { assetPath: '/Game/Env' }
        });
        // Bounded: the receipt guides migration, it does not dump a full schema.
        expect(Object.hasOwn(sc, 'inputSchema')).toBe(false);
        expect(JSON.stringify(sc).length).toBeLessThanOrEqual(2048);
    });
});
