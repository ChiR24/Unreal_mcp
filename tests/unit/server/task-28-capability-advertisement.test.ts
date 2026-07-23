// Task 28 / Task 30 — truthful capability advertisement on the permanent
// single-`unreal` TypeScript stdio surface. Task 30 makes single-tool mode
// permanent, so there is no legacy multi-tool surface to advertise: gateway is
// the only surface under test. TypeScript keeps its backed `resources`
// capability, omits `tools.listChanged` (the lone `unreal` tool never changes
// membership), and — since Task 37 wired the prompts/completions primitives —
// now advertises the once-unbacked `prompts` primitive truthfully. A direct call
// to a canonical parent tool is refused with an executable migration receipt.

import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { InMemoryTransport } from '@modelcontextprotocol/sdk/inMemory.js';
import { afterEach, describe, expect, it, vi } from 'vitest';
import { isRecord } from '../../../src/utils/validation/type-guards.js';

// Pay the one-time transform cost during collection; each case still reloads
// the module after installing its own environment fixture.
await import('../../../src/server/server-factory.js');

async function build() {
    vi.resetModules();
    vi.stubEnv('MOCK_UNREAL_CONNECTION', 'true');
    vi.stubEnv('NODE_ENV', 'test');

    const { createServer } = await import('../../../src/server/server-factory.js');
    // Imported in the SAME module generation as the server under test, so
    // mutating this manager really moves the surface the client observes.
    const { dynamicToolManager } = await import('../../../src/tools/dynamic/dynamic-tool-manager.js');

    const built = createServer();
    const client = new Client({ name: 'task-28-caps', version: '1.0.0' }, { capabilities: {} });
    const pair = InMemoryTransport.createLinkedPair();
    await built.server.connect(pair[1]);
    await client.connect(pair[0], { timeout: 15000 });
    return { built, client, pair, dynamicToolManager };
}

let ctx: Awaited<ReturnType<typeof build>> | undefined;

afterEach(async () => {
    if (ctx) {
        ctx.dynamicToolManager.reset();
        await ctx.pair[0].close();
        ctx.built.automationBridge?.stop();
        ctx.built.bridge?.dispose();
        ctx.built.metricsServer?.close();
    }
    ctx = undefined;
    vi.unstubAllEnvs();
});

async function listNames(active: NonNullable<typeof ctx>): Promise<string[]> {
    const list = await active.client.listTools(undefined, { timeout: 15000 });
    return list.tools.map((tool) => tool.name);
}

function payload(response: unknown): Record<string, unknown> {
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

describe('Task 28 baseline — capability surface Task 28 must preserve', () => {
    it('gateway tools/list exposes exactly one `unreal` tool', async () => {
        ctx = await build();
        expect(await listNames(ctx)).toEqual(['unreal']);
    });

    it('advertised `resources` capability is backed by a live resources/list handler', async () => {
        ctx = await build();
        expect(ctx.client.getServerCapabilities()?.resources).toBeDefined();
        const listed = await ctx.client.listResources(undefined, { timeout: 15000 });
        expect(Array.isArray(listed.resources)).toBe(true);
    });
});

describe('Task 28 desired — truthful capability advertisement', () => {
    it('initialize advertises `tools` with no `listChanged` member', async () => {
        ctx = await build();
        const tools = ctx.client.getServerCapabilities()?.tools;
        expect(tools).toBeDefined();
        // Omission, not `listChanged: false`: the single `unreal` tool never
        // changes, so the server must not declare the notification at all.
        expect(Object.hasOwn(tools ?? {}, 'listChanged')).toBe(false);
    });

    it('keeps the backed `resources` capability and, since Task 37, advertises the now-backed `prompts` primitive', async () => {
        ctx = await build();
        const capabilities = ctx.client.getServerCapabilities();
        expect(capabilities?.resources).toBeDefined();
        // Task 37 wired the prompts + completions primitives, so the once-unbacked
        // `prompts` claim is now truthful (backed by prompts/list + prompts/get)
        // and is therefore advertised rather than dropped.
        expect(capabilities?.prompts).toBeDefined();
    });

    it('gateway configure status reports both catalog revisions', async () => {
        ctx = await build();
        const response = await ctx.client.callTool(
            { name: 'unreal', arguments: { operation: 'configure', action: 'get_status' } },
            undefined,
            { timeout: 15000 }
        );
        const envelope = payload(response);
        const status = isRecord(envelope.result) ? envelope.result : envelope;
        expect(typeof status.catalogRevision).toBe('string');
        expect(typeof status.catalogStateRevision).toBe('number');
    });

    it('refuses a direct canonical-tool call with an executable DIRECT_TOOL_CALL_REMOVED migration', async () => {
        // Given the permanent single-tool surface...
        ctx = await build();
        // When a client calls a canonical parent tool directly...
        const response = await ctx.client.callTool(
            { name: 'manage_asset', arguments: { action: 'list_assets', assetPath: '/Game/Env' } },
            undefined,
            { timeout: 15000 }
        );
        // Then the surface refuses it with a copy-paste-executable migration.
        expect(response.isError).toBe(true);
        const envelope = payload(response);
        expect(envelope.errorCode).toBe('DIRECT_TOOL_CALL_REMOVED');
        expect(envelope.nextCall).toEqual({
            operation: 'execute',
            tool: 'manage_asset',
            action: 'list_assets',
            params: { assetPath: '/Game/Env' }
        });
    });
});
