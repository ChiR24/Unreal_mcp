import { afterEach, describe, expect, it, vi } from 'vitest';
import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { InMemoryTransport } from '@modelcontextprotocol/sdk/inMemory.js';

async function buildWithGatewayMode(mode: string | undefined) {
    vi.resetModules();
    const original = process.env;
    process.env = { ...original };
    if (mode === undefined) {
        delete process.env.MCP_GATEWAY_MODE;
    } else {
        process.env.MCP_GATEWAY_MODE = mode;
    }
    process.env.MOCK_UNREAL_CONNECTION = 'true';
    process.env.NODE_ENV = 'test';

    const { createServer } = await import('../../../src/server/server-factory.js');
    const built = createServer();
    const client = new Client({ name: 'gw-behavior', version: '1.0.0' }, { capabilities: {} });
    const pair = InMemoryTransport.createLinkedPair();
    await built.server.connect(pair[1]);
    await client.connect(pair[0], { timeout: 15000 });
    return { built, client, pair };
}

describe('gateway-mode behavior (MCP_GATEWAY_MODE)', () => {
    let ctx: Awaited<ReturnType<typeof buildWithGatewayMode>> | undefined;

    afterEach(async () => {
        if (ctx) {
            await ctx.pair[0].close();
            ctx.built.automationBridge?.stop();
            ctx.built.bridge?.dispose();
            ctx.built.metricsServer?.close();
        }
        ctx = undefined;
        process.env = { ...process.env };
        vi.resetModules();
    });

    it('defaults to gateway mode: exactly one public tool (unreal)', async () => {
        ctx = await buildWithGatewayMode(undefined);
        const list = await ctx.client.listTools(undefined, { timeout: 15000 });
        expect(list.tools).toHaveLength(1);
        expect(list.tools[0]?.name).toBe('unreal');
    });

    it('restores legacy multi-tool listing when MCP_GATEWAY_MODE=false', async () => {
        ctx = await buildWithGatewayMode('false');
        const list = await ctx.client.listTools(undefined, { timeout: 15000 });
        expect(list.tools.length).toBeGreaterThan(1);
        expect(list.tools.find((t) => t.name === 'manage_tools')).toBeDefined();
        expect(list.tools.find((t) => t.name === 'unreal')).toBeUndefined();
    });
});
