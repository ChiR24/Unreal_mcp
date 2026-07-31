// Task 44 — the Tasks FALLBACK path, asserted on raw wire frames.
//
// MCP Tasks is now implemented on both transports, so this file guards the half
// of the plan that is easiest to break by landing it: "clients without Tasks
// receive normal progress plus synchronous result". A client without Tasks is
// one that does NOT send `params.task`; it must keep getting exactly what it got
// before Tasks existed — progress notifications and a synchronous tool result
// with no task marker anywhere in it.
//
// The honesty invariant is pinned the other way round now: the server DOES
// advertise Tasks, and every method that advert implies must really answer.

import { afterEach, describe, expect, it, vi } from 'vitest';
import { InMemoryTransport } from '@modelcontextprotocol/sdk/inMemory.js';
import { getMcpRequestContext } from '../../../src/automation/request-context.js';

type Frame = Record<string, unknown>;

let duringCall: (() => void) | undefined;

vi.mock('../../../src/tools/orchestration/consolidated-tool-handlers.js', async (importOriginal) => {
    const actual = await importOriginal<Record<string, unknown>>();
    return {
        ...actual,
        handleConsolidatedToolCall: async () => {
            duringCall?.();
            return { success: true, operation: 'execute', message: 'done' };
        },
    };
});

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
    for (let attempt = 0; attempt < 2000; attempt += 1) {
        const found = frames.find((frame) => frame.id === id && ('result' in frame || 'error' in frame));
        if (found) return found;
        await Promise.resolve();
        await new Promise<void>((resolve) => setImmediate(resolve));
    }
    throw new Error(`expected a response for id ${id}`);
}

const TASKS_CAPABLE = {
    tasks: { requests: { 'tools/call': true } },
    experimental: { tasks: {} },
} as const;

async function initialize(ctx: Harness, capabilities: Record<string, unknown>): Promise<Frame> {
    await ctx.send({
        jsonrpc: '2.0',
        id: 1,
        method: 'initialize',
        params: {
            protocolVersion: '2025-11-25',
            capabilities,
            clientInfo: { name: 'task-44-tasks-fallback', version: '1.0.0' },
        },
    });
    return waitForId(ctx.frames, 1);
}

const progressFrames = (frames: readonly Frame[]): Frame[] =>
    frames.filter((frame) => frame.method === 'notifications/progress');

const resultOf = (frame: Frame): Record<string, unknown> =>
    (frame.result ?? {}) as Record<string, unknown>;

afterEach(async () => {
    duringCall = undefined;
    for (const ctx of active.splice(0)) await ctx.close();
    vi.unstubAllEnvs();
});

describe('Task 44 — a Tasks-capable client still gets progress plus a synchronous result', () => {
    it('answers tools/call synchronously and never with a CreateTaskResult', async () => {
        const ctx = await harness();
        await initialize(ctx, TASKS_CAPABLE);
        duringCall = () => {
            const requestId = getMcpRequestContext()?.requestId;
            if (requestId) ctx.built.automationBridge.reportRequestProgress(requestId, { progress: 60, total: 100 });
        };

        await ctx.send({
            jsonrpc: '2.0',
            id: 2,
            method: 'tools/call',
            params: {
                name: 'unreal',
                arguments: { operation: 'execute', tool: 'inspect', action: 'inspect_object', params: {} },
                _meta: { progressToken: 'fallback-tok' },
            },
        });
        const response = await waitForId(ctx.frames, 2);

        // A task-augmented answer would carry `task`; the synchronous contract
        // is tool content in the same response that closed the request.
        expect('error' in response).toBe(false);
        expect(resultOf(response).task).toBeUndefined();
        expect(resultOf(response).content).toBeDefined();

        const params = progressFrames(ctx.frames).map((f) => f.params as Record<string, unknown>);
        expect(params).toHaveLength(1);
        expect(params[0]?.progressToken).toBe('fallback-tok');
    });

    it('REFUSES a task-augmented MUTATION, and refuses it before the mutation runs', async () => {
        // The distinction that matters: a client "without Tasks" does not send
        // `params.task` at all, and those clients get progress plus a synchronous
        // result (the cases either side of this one). A client that DOES send
        // `params.task` is asking for a pollable handle.
        //
        // For `execute` this server refuses, because cancelling a task cannot
        // interrupt work already dispatched to the editor — handing back a
        // cancellable handle for a mutation would promise an interruption it
        // cannot deliver. The refusal must land BEFORE the mutation runs, which
        // is what `handlerRan` proves: an implementation that ran the work and
        // then refused would look identical on the error frame alone.
        const ctx = await harness();
        await initialize(ctx, TASKS_CAPABLE);
        let handlerRan = false;
        duringCall = () => {
            handlerRan = true;
        };

        await ctx.send({
            jsonrpc: '2.0',
            id: 2,
            method: 'tools/call',
            params: {
                name: 'unreal',
                arguments: { operation: 'execute', tool: 'inspect', action: 'inspect_object', params: {} },
                task: { ttl: 60_000 },
            },
        });
        const response = await waitForId(ctx.frames, 2);

        const error = response.error as { code?: number; message?: string; data?: unknown } | undefined;
        expect(response.result).toBeUndefined();
        expect(error?.message).toContain('safe read-only checkpoints');
        expect(error?.message).toContain('without params.task');
        expect(handlerRan).toBe(false);
        // The refusal is executable and names what may be task-augmented.
        const data = (error?.data ?? {}) as Record<string, unknown>;
        expect(data.code).toBe('TASK_CHECKPOINT_NOT_AVAILABLE');
        expect(data.taskableOperations).toEqual(['search', 'describe']);
        // No half-created task escapes the refusal.
        expect(JSON.stringify(response)).not.toContain('taskId');
    });

    it('gives a Tasks-capable and a Tasks-blind client the identical answer shape', async () => {
        const capable = await harness();
        await initialize(capable, TASKS_CAPABLE);
        await capable.send({
            jsonrpc: '2.0', id: 2, method: 'tools/call',
            params: { name: 'unreal', arguments: { operation: 'search', query: 'actor' } },
        });
        const capableResponse = await waitForId(capable.frames, 2);

        const blind = await harness();
        await initialize(blind, {});
        await blind.send({
            jsonrpc: '2.0', id: 2, method: 'tools/call',
            params: { name: 'unreal', arguments: { operation: 'search', query: 'actor' } },
        });
        const blindResponse = await waitForId(blind.frames, 2);

        expect(Object.keys(resultOf(capableResponse)).sort())
            .toEqual(Object.keys(resultOf(blindResponse)).sort());
        expect(resultOf(capableResponse).task).toBeUndefined();
        expect(resultOf(blindResponse).task).toBeUndefined();
    });
});

describe('Task 44 — the server advertises only Tasks it really backs', () => {
    it('advertises tasks in the negotiated initialize capabilities', async () => {
        const ctx = await harness();
        const response = await initialize(ctx, TASKS_CAPABLE);

        const capabilities = (resultOf(response).capabilities ?? {}) as Record<string, unknown>;
        expect(capabilities.tasks).toEqual({ list: {}, cancel: {}, requests: { tools: { call: {} } } });
        expect(Object.keys(capabilities).sort()).toEqual(['completions', 'prompts', 'resources', 'tasks', 'tools']);
    });

    it('answers every advertised tasks/* method rather than reporting it unknown', async () => {
        const ctx = await harness();
        await initialize(ctx, TASKS_CAPABLE);

        const methods = [
            ['tasks/get', { taskId: 'nope' }],
            ['tasks/list', {}],
            ['tasks/cancel', { taskId: 'nope' }],
            ['tasks/result', { taskId: 'nope' }],
        ] as const;

        let id = 10;
        for (const [method, params] of methods) {
            id += 1;
            await ctx.send({ jsonrpc: '2.0', id, method, params });
            const response = await waitForId(ctx.frames, id);
            const error = response.error as { code?: number } | undefined;
            // Backed: it either answers, or refuses on the ARGUMENT (-32602 for
            // an unknown taskId). -32601 would mean the advert was a lie.
            expect(error?.code).not.toBe(-32601);
        }
    });

    it('leaks no task marker into a normal tool result', async () => {
        const ctx = await harness();
        await initialize(ctx, TASKS_CAPABLE);

        await ctx.send({
            jsonrpc: '2.0', id: 2, method: 'tools/call',
            params: { name: 'unreal', arguments: { operation: 'search', query: 'actor' } },
        });
        const response = await waitForId(ctx.frames, 2);

        const serialized = JSON.stringify(response);
        expect(serialized).not.toContain('taskId');
        expect(serialized).not.toContain('"task"');
    });
});
