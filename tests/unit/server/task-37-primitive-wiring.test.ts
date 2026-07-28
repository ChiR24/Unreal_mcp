// Task 37 — RED integration contract for wiring the Tasks 31-36 MCP primitives
// into the TypeScript stdio server and advertising ONLY the implemented session
// profile. This suite drives the REAL SDK over linked in-memory Server/Client
// transports (same fixture shape as task-28-capability-advertisement.test.ts) and
// asserts the DESIRED wired end-state. It is expected to FAIL today: production
// still advertises `{ tools, resources }` only and registers no subscribe /
// prompts / completions / session-profile handlers (Tasks 31-36 exist as modules
// but are not wired). Every failure is therefore a missing-wiring / missing-
// capability assertion or a server -32601, never an import error and never a
// timing sleep. When Task 37 lands the wiring, this suite turns GREEN unchanged.
//
// Contract constants below are HARD-CODED (an independent oracle), not derived
// from the modules under test, so a wiring bug can never make the suite pass by
// echoing production's own data back at it.

import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { InMemoryTransport } from '@modelcontextprotocol/sdk/inMemory.js';
import {
    EmptyResultSchema,
    ErrorCode,
    type ClientCapabilities,
    type Notification,
} from '@modelcontextprotocol/sdk/types.js';
import { afterEach, describe, expect, it, vi } from 'vitest';
import { isRecord } from '../../../src/utils/validation/type-guards.js';
import {
    CATALOG_SUBSCRIPTION_URI,
    DEFAULT_COALESCE_WINDOW_MS,
} from '../../../src/server/mcp-primitives/subscriptions/subscription-types.js';

// Pay the one-time transform cost during collection; each case reloads the module
// after installing its own environment fixture (mirrors task 28).
await import('../../../src/server/server-factory.js');

// --- Contract constants (independent oracle) ---

/** The exact server capability surface Task 37 must advertise, and nothing else. */
const EXPECTED_SERVER_CAPABILITIES = {
    tools: {},
    resources: { subscribe: true },
    prompts: {},
    completions: {},
    tasks: { list: {}, cancel: {}, requests: { tools: { call: {} } } },
} as const;

/** The single public tool and its four gateway operations. */
const GATEWAY_OPERATIONS = ['search', 'describe', 'execute', 'configure'] as const;

/** The six Task 32 workflow prompts in stable definition order. */
const WORKFLOW_PROMPT_NAMES = [
    'inspect-fix',
    'asset-import',
    'level-build',
    'blueprint-edit',
    'validation',
    'sequence-render',
] as const;

/** The structural profile a fully-capable client MUST resolve to. */
const EXPECTED_FULL_PROFILE = {
    hasResources: true,
    hasPrompts: true,
    hasCompletions: true,
    hasSubscriptions: true,
    hasElicitation: true,
    hasTasks: false,
} as const;

/** The structural profile a client that declared nothing MUST resolve to. */
const EXPECTED_MINIMAL_PROFILE = {
    hasResources: false,
    hasPrompts: false,
    hasCompletions: false,
    hasSubscriptions: false,
    hasElicitation: false,
    hasTasks: false,
} as const;

// A resource that IS listed and readable but is NOT in the subscribable
// allowlist, so subscribing to it must be refused by the server (not the client).
const NON_ALLOWLIST_URI = 'ue://health';

// The consumer capabilities a "full" client declares. `resources`/`prompts`/
// `completions`/`subscriptions` are not standard client keys, so they are nested
// under `experimental` (which the Task 35 profile parser inspects); `elicitation`
// is a first-class client capability. Names/versions vary per client but these
// declared capabilities are what the derived profile depends on.
const FULL_CLIENT_CAPABILITIES: ClientCapabilities = {
    experimental: {
        resources: { subscribe: true },
        prompts: {},
        completions: {},
        subscriptions: {},
    },
    elicitation: {},
};
const MINIMAL_CLIENT_CAPABILITIES: ClientCapabilities = {};

// Advance well past the coalescing window so the debounced flush is deterministic
// under fake timers (no wall-clock sleep). Extra fake time is free.
const FLUSH_ADVANCE_MS = DEFAULT_COALESCE_WINDOW_MS * 4;

interface Built {
    built: ReturnType<typeof import('../../../src/server/server-factory.js').createServer>;
    client: Client;
    pair: ReturnType<typeof InMemoryTransport.createLinkedPair>;
}

const active: Built[] = [];

async function build(
    clientInfo: { name: string; version: string },
    capabilities: ClientCapabilities,
): Promise<Built> {
    vi.resetModules();
    vi.stubEnv('MOCK_UNREAL_CONNECTION', 'true');
    vi.stubEnv('NODE_ENV', 'test');

    const { createServer } = await import('../../../src/server/server-factory.js');
    const built = createServer();
    const client = new Client(clientInfo, { capabilities });
    const pair = InMemoryTransport.createLinkedPair();
    await built.server.connect(pair[1]);
    await client.connect(pair[0], { timeout: 15000 });

    const ctx: Built = { built, client, pair };
    active.push(ctx);
    return ctx;
}

afterEach(async () => {
    vi.useRealTimers();
    for (const ctx of active.splice(0)) {
        try {
            await ctx.pair[0].close();
        } catch {
            // already closed by the test (disconnect cases)
        }
        ctx.built.automationBridge?.stop();
        ctx.built.bridge?.dispose();
        ctx.built.metricsServer?.close();
    }
    vi.unstubAllEnvs();
});

// --- Scaffolding (response shaping / notification capture only; no production logic) ---

function asRecord(value: unknown): Record<string, unknown> | undefined {
    return isRecord(value) ? value : undefined;
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

async function configureStatus(ctx: Built): Promise<Record<string, unknown>> {
    const response = await ctx.client.callTool(
        { name: 'unreal', arguments: { operation: 'configure', action: 'get_status' } },
        undefined,
        { timeout: 15000 },
    );
    return payload(response);
}

// Bump the catalog: a real, deterministic configure visibility change that Task 37
// must fold into the subscribed session's catalog revision and one notification.
function bumpCatalog(ctx: Built): Promise<unknown> {
    return ctx.client.callTool(
        {
            name: 'unreal',
            arguments: {
                operation: 'configure',
                action: 'disable_category',
                params: { category: 'gameplay' },
            },
        },
        undefined,
        { timeout: 15000 },
    );
}

// Capture the RAW `resources/updated` notifications (via the fallback handler, so
// no schema pre-parsing can hide extra params the server may have sent).
function collectResourceUpdated(ctx: Built): Notification[] {
    const updates: Notification[] = [];
    ctx.client.fallbackNotificationHandler = async (notification) => {
        if (notification.method === 'notifications/resources/updated') {
            updates.push(notification);
        }
    };
    return updates;
}

// --- Capability advertisement matrix ---

describe('Task 37 — server capability advertisement', () => {
    it('advertises exactly { tools, resources.subscribe, prompts, completions, tasks }', async () => {
        const ctx = await build({ name: 'task-37-full', version: '1.0.0' }, FULL_CLIENT_CAPABILITIES);
        // When: a full-capability client reads the negotiated server capabilities.
        const capabilities = ctx.client.getServerCapabilities();
        // Then: the surface is exactly the implemented matrix — subscribe, prompts,
        // and completions are present because their handlers are (Task 37) live.
        expect(capabilities).toEqual(EXPECTED_SERVER_CAPABILITIES);
    });

    it('advertises tasks ONLY because all four tasks methods answer, and still omits logging and every listChanged member', async () => {
        const ctx = await build({ name: 'task-37-full', version: '1.0.0' }, FULL_CLIENT_CAPABILITIES);
        const capabilities = (ctx.client.getServerCapabilities() ?? {}) as Record<string, unknown>;
        // Task 44 flipped this assertion, and the pairing below is what makes the
        // flip honest rather than a weakening: the advert is only allowed to be
        // present because every method it implies is proven reachable in the SAME
        // test. Before Task 44 the second half would have thrown -32601 on the
        // first call, so this case could not have been made to pass by editing
        // the expectation alone.
        expect(Object.hasOwn(capabilities, 'tasks')).toBe(true);
        for (const method of ['tasks/get', 'tasks/list', 'tasks/cancel', 'tasks/result']) {
            const params = method === 'tasks/list' ? {} : { taskId: 'no-such-task' };
            let code: unknown;
            try {
                await ctx.client.request({ method, params }, EmptyResultSchema, { timeout: 15000 });
            } catch (caught) {
                code = asRecord(caught)?.code;
            }
            // A backed method answers, or refuses on the ARGUMENT (-32602). What it
            // must never do is report itself unknown.
            expect(code).not.toBe(ErrorCode.MethodNotFound);
        }
        // Unimplemented primitives are still never advertised, and the stable
        // single-tool surface declares no list-changed notifications.
        expect(Object.hasOwn(capabilities, 'logging')).toBe(false);
        expect(Object.hasOwn(asRecord(capabilities.tools) ?? {}, 'listChanged')).toBe(false);
        expect(Object.hasOwn(asRecord(capabilities.resources) ?? {}, 'listChanged')).toBe(false);
    });
});

// --- Single gateway tool surface (must survive the primitive wiring) ---

describe('Task 37 — single gateway tool', () => {
    it('tools/list exposes exactly one `unreal` tool with the four operations', async () => {
        const ctx = await build({ name: 'task-37-full', version: '1.0.0' }, FULL_CLIENT_CAPABILITIES);
        const list = await ctx.client.listTools(undefined, { timeout: 15000 });
        // When/Then: the primitive wiring must not add a second public tool.
        expect(list.tools.map((tool) => tool.name)).toEqual(['unreal']);
        const schema = asRecord(list.tools[0]?.inputSchema) ?? {};
        const operation = asRecord(asRecord(schema.properties)?.operation) ?? {};
        expect(operation.enum).toEqual([...GATEWAY_OPERATIONS]);
    });
});

// --- Existing read-only resource surface stays wired (Task 31 preservation) ---

describe('Task 37 — existing resource surface preserved', () => {
    it('resources/list, resources/templates/list, and resources/read remain available', async () => {
        const ctx = await build({ name: 'task-37-full', version: '1.0.0' }, FULL_CLIENT_CAPABILITIES);
        // When: a client exercises the pre-existing read-only resource surface.
        const listed = await ctx.client.listResources(undefined, { timeout: 15000 });
        const templates = await ctx.client.listResourceTemplates(undefined, { timeout: 15000 });
        const read = await ctx.client.readResource({ uri: 'ue://capability/catalog' }, { timeout: 15000 });
        // Then: all three still resolve and the catalog read returns bounded data.
        expect(Array.isArray(listed.resources)).toBe(true);
        expect(Array.isArray(templates.resourceTemplates)).toBe(true);
        expect(read.contents[0]?.uri).toBe('ue://capability/catalog');
    });
});

// --- Resource subscription + coalesced notification ---

describe('Task 37 — resource subscriptions', () => {
    it('a catalog bump within the window delivers exactly one resources/updated carrying only { uri }', async () => {
        const ctx = await build({ name: 'task-37-full', version: '1.0.0' }, FULL_CLIENT_CAPABILITIES);
        const updates = collectResourceUpdated(ctx);
        // When: the client subscribes to the catalog, then one configure visibility
        // change bumps the catalog revision inside a single debounce window.
        await ctx.client.subscribeResource({ uri: CATALOG_SUBSCRIPTION_URI }, { timeout: 15000 });
        vi.useFakeTimers();
        const bumped = bumpCatalog(ctx);
        await vi.advanceTimersByTimeAsync(FLUSH_ADVANCE_MS);
        await bumped;
        vi.useRealTimers();
        // Then: exactly one bounded notification arrives; its params are ONLY `uri`.
        expect(updates).toHaveLength(1);
        const params = asRecord(updates[0]?.params) ?? {};
        expect(Object.keys(params)).toEqual(['uri']);
        expect(params.uri).toBe(CATALOG_SUBSCRIPTION_URI);
    });

    it('after unsubscribe, a catalog bump within the window is silent', async () => {
        const ctx = await build({ name: 'task-37-full', version: '1.0.0' }, FULL_CLIENT_CAPABILITIES);
        const updates = collectResourceUpdated(ctx);
        await ctx.client.subscribeResource({ uri: CATALOG_SUBSCRIPTION_URI }, { timeout: 15000 });
        await ctx.client.unsubscribeResource({ uri: CATALOG_SUBSCRIPTION_URI }, { timeout: 15000 });
        // When: the same catalog bump happens after the subscription was released.
        vi.useFakeTimers();
        const bumped = bumpCatalog(ctx);
        await vi.advanceTimersByTimeAsync(FLUSH_ADVANCE_MS);
        await bumped;
        vi.useRealTimers();
        // Then: the drained store routes nothing — zero notifications.
        expect(updates).toHaveLength(0);
    });

    it('subscribing to a non-allowlist URI is rejected by the server, not silently accepted', async () => {
        const ctx = await build({ name: 'task-37-full', version: '1.0.0' }, FULL_CLIENT_CAPABILITIES);
        // When: the client subscribes to a real but non-subscribable resource.
        let error: unknown;
        try {
            await ctx.client.subscribeResource({ uri: NON_ALLOWLIST_URI }, { timeout: 15000 });
        } catch (caught) {
            error = caught;
        }
        // Then: it is refused by a server-originated McpError (numeric code) that
        // is NOT MethodNotFound. -32601 would mean "no subscribe handler at all";
        // a genuine allowlist rejection proves the wired handler discriminated the
        // URI. `typeof code === 'number'` also rules out a silent accept (undefined).
        expect(error).toBeDefined();
        const code = asRecord(error)?.code;
        expect(typeof code).toBe('number');
        expect(code).not.toBe(ErrorCode.MethodNotFound);
    });

    it('closing a subscribed session drains it and delivers no late update', async () => {
        const ctx = await build({ name: 'task-37-full', version: '1.0.0' }, FULL_CLIENT_CAPABILITIES);
        const updates = collectResourceUpdated(ctx);
        await ctx.client.subscribeResource({ uri: CATALOG_SUBSCRIPTION_URI }, { timeout: 15000 });
        // When: the session closes while subscribed.
        await ctx.client.close();
        vi.useFakeTimers();
        await vi.advanceTimersByTimeAsync(FLUSH_ADVANCE_MS);
        vi.useRealTimers();
        // Then: no notification is delivered after close (store drained on disconnect).
        expect(updates).toHaveLength(0);
    });
});

// --- Prompts primitive ---

describe('Task 37 — prompts', () => {
    it('prompts/list returns the six workflow prompts in definition order', async () => {
        const ctx = await build({ name: 'task-37-full', version: '1.0.0' }, FULL_CLIENT_CAPABILITIES);
        const list = await ctx.client.listPrompts(undefined, { timeout: 15000 });
        // When/Then: the wired prompt catalog lists exactly the six workflows.
        expect(list.prompts.map((prompt) => prompt.name)).toEqual([...WORKFLOW_PROMPT_NAMES]);
    });

    it('prompts/get inspect-fix returns a bounded guidance message referencing the gateway', async () => {
        const ctx = await build({ name: 'task-37-full', version: '1.0.0' }, FULL_CLIENT_CAPABILITIES);
        const result = await ctx.client.getPrompt(
            { name: 'inspect-fix', arguments: { objectPath: '/Game/Heroes/BP_Hero' } },
            { timeout: 15000 },
        );
        // When/Then: the rendered prompt is a bounded, user-readable text message
        // that points at gateway calls (guidance only, never autonomous execution).
        expect(result.messages.length).toBeGreaterThan(0);
        const content = asRecord(result.messages[0]?.content) ?? {};
        expect(content.type).toBe('text');
        expect(typeof content.text).toBe('string');
        expect(content.text as string).toContain('unreal');
    });
});

// --- Completions primitive ---

describe('Task 37 — completions', () => {
    it('completion/complete for a capabilityId argument returns bounded ranked candidates', async () => {
        const ctx = await build({ name: 'task-37-full', version: '1.0.0' }, FULL_CLIENT_CAPABILITIES);
        const result = await ctx.client.complete(
            {
                ref: { type: 'ref/resource', uri: 'ue://capability/{capabilityId}' },
                argument: { name: 'capabilityId', value: 'asset' },
            },
            { timeout: 15000 },
        );
        // When/Then: a real prefix yields a bounded, non-empty, all-string candidate
        // set (<= the MCP 100-item cap).
        const values = result.completion.values;
        expect(Array.isArray(values)).toBe(true);
        expect(values.length).toBeGreaterThan(0);
        expect(values.length).toBeLessThanOrEqual(100);
        expect(values.every((value) => typeof value === 'string')).toBe(true);
    });
});

// --- Tasks are present and backed (Task 44) ---

describe('Task 44 — tasks primitive backed', () => {
    it('tasks/list answers instead of returning MethodNotFound', async () => {
        const ctx = await build({ name: 'task-37-full', version: '1.0.0' }, FULL_CLIENT_CAPABILITIES);
        // When: the client sends a raw tasks/list request.
        let error: unknown;
        try {
            await ctx.client.request({ method: 'tasks/list', params: {} }, EmptyResultSchema, { timeout: 15000 });
        } catch (caught) {
            error = caught;
        }
        // Then: a backed handler answers rather than reporting itself unknown.
        // The list CONTENT is asserted on raw frames in tasks-on-the-wire.test.ts,
        // where no result schema can strip the tasks array before it is read.
        expect(asRecord(error)?.code).not.toBe(ErrorCode.MethodNotFound);
    });

    it('an unknown method is still MethodNotFound, so the family is not a catch-all', async () => {
        const ctx = await build({ name: 'task-37-full', version: '1.0.0' }, FULL_CLIENT_CAPABILITIES);
        let error: unknown;
        try {
            await ctx.client.request({ method: 'tasks/nope', params: {} }, EmptyResultSchema, { timeout: 15000 });
        } catch (caught) {
            error = caught;
        }
        expect(asRecord(error)?.code).toBe(ErrorCode.MethodNotFound);
    });
});

// --- Adaptive session client-capability profile (brand independent) ---

describe('Task 37 — session client-capability profile', () => {
    it('a full-capability client resolves to a fully-enabled profile', async () => {
        const ctx = await build({ name: 'task-37-full', version: '1.0.0' }, FULL_CLIENT_CAPABILITIES);
        // When: the session reports its derived client profile via configure status.
        const status = await configureStatus(ctx);
        // Then: every consumable primitive the client declared is reflected.
        expect(status.clientProfile).toEqual(EXPECTED_FULL_PROFILE);
    });

    it('a minimal client resolves to an all-absent profile', async () => {
        const ctx = await build({ name: 'task-37-minimal', version: '1.0.0' }, MINIMAL_CLIENT_CAPABILITIES);
        const status = await configureStatus(ctx);
        // When/Then: a client that declared nothing enables no primitive.
        expect(status.clientProfile).toEqual(EXPECTED_MINIMAL_PROFILE);
    });

    it('two clients with different name/version but identical capabilities resolve to the same profile', async () => {
        const alpha = await build({ name: 'alpha-client', version: '1.0.0' }, FULL_CLIENT_CAPABILITIES);
        const beta = await build({ name: 'beta-client', version: '9.9.9' }, FULL_CLIENT_CAPABILITIES);
        // When: two differently-branded but identically-capable clients report.
        const alphaStatus = await configureStatus(alpha);
        const betaStatus = await configureStatus(beta);
        // Then: the profile depends ONLY on declared capabilities, never the brand
        // (both equal the expected full profile, so this cannot pass on two undefineds).
        expect(alphaStatus.clientProfile).toEqual(EXPECTED_FULL_PROFILE);
        expect(betaStatus.clientProfile).toEqual(EXPECTED_FULL_PROFILE);
        expect(alphaStatus.clientProfile).toEqual(betaStatus.clientProfile);
    });
});
