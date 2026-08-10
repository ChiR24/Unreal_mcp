// Task 44 — the MCP Tasks lifecycle, driven end to end over raw JSON-RPC frames.
//
// The store is proven in isolation by bounded-task-store.test.ts. This file
// proves the store is REACHABLE: every assertion here goes through the real
// server, over a real transport, using the same frames a client sends. A store
// that no client can reach would pass the unit suite and fail every case below.
//
// Raw frames rather than the SDK Client because the Client coerces and reshapes
// task payloads; only literal frames answer "what does the client actually see".

import { afterEach, describe, expect, it, vi } from 'vitest';
import { InMemoryTransport } from '@modelcontextprotocol/sdk/inMemory.js';

type Frame = Record<string, unknown>;

const { createServer } = await import('../../../src/server/server-factory.js');

interface Harness {
    readonly frames: Frame[];
    readonly send: (message: Frame) => Promise<void>;
    readonly built: ReturnType<typeof createServer>;
    readonly close: () => Promise<void>;
}

const active: Harness[] = [];

async function harness(): Promise<Harness> {
    vi.stubEnv('MOCK_UNREAL_CONNECTION', 'true');
    vi.stubEnv('NODE_ENV', 'test');

    const built = createServer();
    const [clientSide, serverSide] = InMemoryTransport.createLinkedPair();
    const frames: Frame[] = [];
    clientSide.onmessage = (message: unknown) => {
        frames.push(message as Frame);
    };
    await built.server.connect(serverSide);
    await clientSide.start();

    const ctx: Harness = {
        frames,
        send: (message) => clientSide.send(message as never),
        built,
        close: async () => {
            await clientSide.close();
            built.automationBridge?.stop();
            built.bridge?.dispose();
            built.metricsServer?.close();
        },
    };
    active.push(ctx);
    return ctx;
}

async function waitForId(frames: readonly Frame[], id: number): Promise<Frame> {
    for (let attempt = 0; attempt < 4000; attempt += 1) {
        const found = frames.find((frame) => frame.id === id && ('result' in frame || 'error' in frame));
        if (found) return found;
        await Promise.resolve();
        await new Promise<void>((resolve) => setImmediate(resolve));
    }
    throw new Error(`expected a response for id ${id}`);
}

const TASKS_CAPABLE = { tasks: { requests: { tools: { call: {} } } } } as const;

let nextId = 100;

async function call(ctx: Harness, method: string, params: Record<string, unknown>): Promise<Frame> {
    const id = (nextId += 1);
    await ctx.send({ jsonrpc: '2.0', id, method, params });
    return waitForId(ctx.frames, id);
}

async function initialize(ctx: Harness): Promise<void> {
    await ctx.send({
        jsonrpc: '2.0',
        id: (nextId += 1),
        method: 'initialize',
        params: {
            protocolVersion: '2025-11-25',
            capabilities: TASKS_CAPABLE,
            clientInfo: { name: 'task-44-wire', version: '1.0.0' },
        },
    });
    await waitForId(ctx.frames, nextId);
}

const resultOf = (frame: Frame): Record<string, unknown> => (frame.result ?? {}) as Record<string, unknown>;
const errorOf = (frame: Frame): Record<string, unknown> => (frame.error ?? {}) as Record<string, unknown>;

async function createCheckpoint(ctx: Harness, args: Record<string, unknown> = { operation: 'search', query: 'actor' }) {
    const response = await call(ctx, 'tools/call', { name: 'unreal', arguments: args, task: { ttl: 60_000 } });
    return resultOf(response).task as Record<string, unknown> | undefined;
}

afterEach(async () => {
    for (const ctx of active.splice(0)) await ctx.close();
    vi.unstubAllEnvs();
});

describe('Task 44 — a read-only checkpoint is reachable from the wire', () => {
    it('answers a task-augmented search with a real task handle', async () => {
        const ctx = await harness();
        await initialize(ctx);

        const task = await createCheckpoint(ctx);
        expect(typeof task?.taskId).toBe('string');
        expect(task?.status).toBe('completed');
        // The bounded store never grants the unlimited lifetime a null ttl asks
        // for, so ttl is always a number on the wire.
        expect(typeof task?.ttl).toBe('number');
        expect(typeof task?.createdAt).toBe('string');
    });

    it('retains a result that tasks/result hands back, identical to the synchronous answer', async () => {
        const ctx = await harness();
        await initialize(ctx);

        const args = { operation: 'search', query: 'actor' };
        const synchronous = resultOf(await call(ctx, 'tools/call', { name: 'unreal', arguments: args }));
        const task = await createCheckpoint(ctx, args);

        const retained = resultOf(await call(ctx, 'tasks/result', { taskId: String(task?.taskId) }));
        // `_meta` carries the related-task marker the SDK adds; the payload
        // itself must equal what a plain tools/call returned, or the two answer
        // paths have silently drifted.
        const { _meta, ...payload } = retained;
        expect(_meta).toBeDefined();
        expect(payload).toEqual(synchronous);
    });

    it('reports the task through tasks/get and lists it through tasks/list', async () => {
        const ctx = await harness();
        await initialize(ctx);
        const task = await createCheckpoint(ctx);

        const fetched = resultOf(await call(ctx, 'tasks/get', { taskId: String(task?.taskId) }));
        expect(fetched.taskId).toBe(task?.taskId);
        expect(fetched.status).toBe('completed');

        const listed = resultOf(await call(ctx, 'tasks/list', {}));
        const ids = (listed.tasks as Array<Record<string, unknown>>).map((entry) => entry.taskId);
        expect(ids).toContain(task?.taskId);
    });

    it('accepts a task-augmented describe as well as search', async () => {
        const ctx = await harness();
        await initialize(ctx);
        const task = await createCheckpoint(ctx, { operation: 'describe', tool: 'inspect' });
        expect(typeof task?.taskId).toBe('string');
    });
});

describe('Task 44 — exactly one terminal state escapes to the client', () => {
    it('refuses to cancel a task that already settled, and never rewrites its status', async () => {
        const ctx = await harness();
        await initialize(ctx);
        const task = await createCheckpoint(ctx);

        const cancelled = await call(ctx, 'tasks/cancel', { taskId: String(task?.taskId) });
        expect(errorOf(cancelled).message).toContain('terminal');

        const after = resultOf(await call(ctx, 'tasks/get', { taskId: String(task?.taskId) }));
        expect(after.status).toBe('completed');
    });

    it('reports an unknown task id without inventing one', async () => {
        const ctx = await harness();
        await initialize(ctx);

        const missing = await call(ctx, 'tasks/get', { taskId: 'does-not-exist' });
        expect(missing.result).toBeUndefined();
        expect(String(errorOf(missing).message)).toMatch(/not found/i);
    });
});

describe('Task 44 — cleanup leaves nothing retained', () => {
    it('drops every task and result when the server closes', async () => {
        const ctx = await harness();
        await initialize(ctx);
        const task = await createCheckpoint(ctx);
        expect(ctx.built.taskStore.totalSize()).toBe(1);

        await ctx.built.server.close();

        expect(ctx.built.taskStore.totalSize()).toBe(0);
        expect(await ctx.built.taskStore.getTask(String(task?.taskId), undefined)).toBeNull();
    });

    it('keeps one connection worth of tasks out of another connection store', async () => {
        const first = await harness();
        await initialize(first);
        await createCheckpoint(first);

        const second = await harness();
        await initialize(second);

        // Each createServer owns its own bounded store, so a task created on one
        // connection is not visible to another even before session scoping.
        expect(second.built.taskStore.totalSize()).toBe(0);
        const listed = resultOf(await call(second, 'tasks/list', {}));
        expect(listed.tasks).toHaveLength(0);
    });
});
