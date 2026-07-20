// Task 28 — truthful capability advertisement on the TypeScript stdio surface.
//
// Two independently runnable groups:
//   npx vitest run tests/unit/server/task-28-capability-advertisement.test.ts -t 'Task 28 baseline'
//   npx vitest run tests/unit/server/task-28-capability-advertisement.test.ts -t 'Task 28 desired'
//
// Decisions (.omo/notepads/pure-unreal-mcp-implementation/decisions.md, 2026-07-20):
// gateway mode OMITS `tools.listChanged` because `tools/list` is one stable
// `unreal` tool whose membership never changes; legacy mode KEEPS
// `listChanged: true` until Task 30 removes the legacy surface, because its
// public membership genuinely does change. TypeScript keeps its backed
// `resources` capability and drops the unbacked `prompts` claim until Wave 4
// (Tasks 32/37) wires real prompt handlers.

import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { InMemoryTransport } from '@modelcontextprotocol/sdk/inMemory.js';
import { afterEach, describe, expect, it, vi } from 'vitest';
import { isRecord } from '../../../src/utils/validation/type-guards.js';

// Pay the one-time transform cost during collection; each case still reloads
// the module after installing its own environment fixture.
await import('../../../src/server/server-factory.js');

async function build(mode: string | undefined) {
    vi.resetModules();
    vi.stubEnv('MCP_GATEWAY_MODE', mode);
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
        ctx = await build(undefined);
        expect(await listNames(ctx)).toEqual(['unreal']);
    });

    it('advertised `resources` capability is backed by a live resources/list handler', async () => {
        ctx = await build(undefined);
        expect(ctx.client.getServerCapabilities()?.resources).toBeDefined();
        const listed = await ctx.client.listResources(undefined, { timeout: 15000 });
        expect(Array.isArray(listed.resources)).toBe(true);
    });

    it('legacy tool visibility genuinely changes when dynamic state changes', async () => {
        ctx = await build('false');
        const before = await listNames(ctx);
        expect(before.length).toBeGreaterThan(1);
        expect(before).toContain('manage_ai');

        ctx.dynamicToolManager.disableCategory('gameplay');

        const after = await listNames(ctx);
        expect(after).not.toContain('manage_ai');
        expect(after.length).toBeLessThan(before.length);
    });

    it('protected tools survive a core disable and reset restores the exact listing', async () => {
        ctx = await build('false');
        const pristine = await listNames(ctx);

        ctx.dynamicToolManager.disableCategory('core');
        expect(ctx.dynamicToolManager.isToolEnabled('manage_tools')).toBe(true);
        expect(ctx.dynamicToolManager.isToolEnabled('inspect')).toBe(true);

        ctx.dynamicToolManager.disableCategory('gameplay');
        ctx.dynamicToolManager.reset();

        expect(await listNames(ctx)).toEqual(pristine);
    });
});

describe('Task 28 desired — truthful capability advertisement', () => {
    it('gateway initialize advertises `tools` with no `listChanged` member', async () => {
        ctx = await build(undefined);
        const tools = ctx.client.getServerCapabilities()?.tools;
        expect(tools).toBeDefined();
        // Omission, not `listChanged: false`: the single `unreal` tool never
        // changes, so the server must not declare the notification at all.
        expect(Object.hasOwn(tools ?? {}, 'listChanged')).toBe(false);
    });

    it('legacy initialize still advertises `listChanged: true` until Task 30', async () => {
        ctx = await build('false');
        expect(ctx.client.getServerCapabilities()?.tools?.listChanged).toBe(true);
    });

    it('keeps the backed `resources` capability and drops the unbacked `prompts` claim', async () => {
        ctx = await build(undefined);
        const capabilities = ctx.client.getServerCapabilities();
        expect(capabilities?.resources).toBeDefined();
        expect(capabilities?.prompts).toBeUndefined();
    });

    it('does not claim an unbacked `prompts` capability in legacy mode either', async () => {
        ctx = await build('false');
        expect(ctx.client.getServerCapabilities()?.prompts).toBeUndefined();
    });

    it('gateway configure status reports both catalog revisions', async () => {
        ctx = await build(undefined);
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
});
