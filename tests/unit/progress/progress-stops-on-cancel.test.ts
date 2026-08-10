// Task 44 lane B — advisory cancellation must silence progress, and settling
// must release the per-request marker.
//
// Cancellation here is ADVISORY: `notifications/cancelled` suppresses the
// response and stops the notification stream, but any editor work already
// dispatched to Unreal runs to completion. Nothing in this suite asserts that
// work was interrupted, because it is not.
//
// What IS asserted is what the client observes: once it has cancelled, no
// further `notifications/progress` for that request may reach it, and
// cancelling one request must not silence a concurrent one.

import { afterEach, describe, expect, it, vi } from 'vitest';
import { InMemoryTransport } from '@modelcontextprotocol/sdk/inMemory.js';
import { getMcpRequestContext } from '../../../src/automation/request-context.js';

type Frame = Record<string, unknown>;

interface Deferred {
    readonly promise: Promise<void>;
    release: () => void;
}

function deferred(): Deferred {
    let release: () => void = () => undefined;
    const promise = new Promise<void>((resolve) => {
        release = resolve;
    });
    return { promise, release };
}

// Populated per case: lets a case observe the in-flight request id and hold the
// handler open until it chooses to let it finish. No timers involved.
let onHandlerEntered: ((requestId: string | undefined) => void) | undefined;
let gate: Deferred | undefined;

vi.mock('../../../src/tools/orchestration/consolidated-tool-handlers.js', async (importOriginal) => {
    const actual = await importOriginal<Record<string, unknown>>();
    return {
        ...actual,
        handleConsolidatedToolCall: async () => {
            onHandlerEntered?.(getMcpRequestContext()?.requestId);
            if (gate) await gate.promise;
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

async function settle(): Promise<void> {
    for (let turn = 0; turn < 40; turn += 1) {
        await Promise.resolve();
        await new Promise<void>((resolve) => setImmediate(resolve));
    }
}

async function waitFor<T>(produce: () => T | undefined): Promise<T> {
    for (let attempt = 0; attempt < 2000; attempt += 1) {
        const value = produce();
        if (value !== undefined) return value;
        await Promise.resolve();
        await new Promise<void>((resolve) => setImmediate(resolve));
    }
    throw new Error('condition never became true');
}

const progressFrames = (frames: readonly Frame[]): Frame[] =>
    frames.filter((frame) => frame.method === 'notifications/progress');

async function initialize(ctx: Harness): Promise<void> {
    await ctx.send({
        jsonrpc: '2.0',
        id: 1,
        method: 'initialize',
        params: {
            protocolVersion: '2025-11-25',
            capabilities: {},
            clientInfo: { name: 'task-44-cancel', version: '1.0.0' },
        },
    });
    await waitFor(() => ctx.frames.find((f) => f.id === 1 && 'result' in f));
    await ctx.send({ jsonrpc: '2.0', method: 'notifications/initialized' });
}

function callFrame(id: number, token: string): Frame {
    return {
        jsonrpc: '2.0',
        id,
        method: 'tools/call',
        params: {
            name: 'unreal',
            arguments: { operation: 'execute', tool: 'inspect', action: 'inspect_object', params: {} },
            _meta: { progressToken: token },
        },
    };
}

afterEach(async () => {
    onHandlerEntered = undefined;
    gate?.release();
    gate = undefined;
    for (const ctx of active.splice(0)) await ctx.close();
    vi.unstubAllEnvs();
});

describe('Task 44 — a cancelled request stops receiving progress', () => {
    it('emits NO progress frame once the client has cancelled', async () => {
        const ctx = await harness();
        await initialize(ctx);
        gate = deferred();
        let inflightId: string | undefined;
        onHandlerEntered = (requestId) => {
            inflightId = requestId;
        };

        await ctx.send(callFrame(2, 'tok'));
        const requestId = await waitFor(() => inflightId);

        // Progress before the cancel is legitimate and proves the stream is live.
        ctx.built.automationBridge.reportRequestProgress(requestId, { progress: 10 });
        await settle();
        expect(progressFrames(ctx.frames)).toHaveLength(1);

        await ctx.send({ jsonrpc: '2.0', method: 'notifications/cancelled', params: { requestId: 2 } });
        await settle();

        // Work already dispatched to Unreal keeps running — that is advisory
        // cancellation — but the client asked to stop hearing about it.
        ctx.built.automationBridge.reportRequestProgress(requestId, { progress: 50 });
        ctx.built.automationBridge.reportRequestProgress(requestId, { progress: 90 });
        await settle();

        expect(progressFrames(ctx.frames)).toHaveLength(1);

        gate.release();
        await settle();
        expect(progressFrames(ctx.frames)).toHaveLength(1);
    });

    it('does not silence a CONCURRENT request when one is cancelled', async () => {
        const ctx = await harness();
        await initialize(ctx);
        gate = deferred();
        const seen: string[] = [];
        onHandlerEntered = (requestId) => {
            if (requestId) seen.push(requestId);
        };

        await ctx.send(callFrame(2, 'tok-a'));
        await ctx.send(callFrame(3, 'tok-b'));
        await waitFor(() => (seen.length >= 2 ? seen.length : undefined));
        const [first, second] = seen;

        await ctx.send({ jsonrpc: '2.0', method: 'notifications/cancelled', params: { requestId: 2 } });
        await settle();

        if (first) ctx.built.automationBridge.reportRequestProgress(first, { progress: 70 });
        if (second) ctx.built.automationBridge.reportRequestProgress(second, { progress: 70 });
        await settle();

        const tokens = progressFrames(ctx.frames)
            .map((frame) => (frame.params as Record<string, unknown>).progressToken);
        expect(tokens).toEqual(['tok-b']);

        gate.release();
        await settle();
    });

    it('releases the per-request marker once the call settles', async () => {
        const ctx = await harness();
        await initialize(ctx);
        let inflightId: string | undefined;
        onHandlerEntered = (requestId) => {
            inflightId = requestId;
        };

        await ctx.send(callFrame(2, 'tok'));
        const requestId = await waitFor(() => inflightId);
        await waitFor(() => ctx.frames.find((f) => f.id === 2 && 'result' in f));
        await settle();

        // The request is over; a late frame for it must find no sink at all.
        ctx.built.automationBridge.reportRequestProgress(requestId, { progress: 99 });
        await settle();

        expect(progressFrames(ctx.frames)).toEqual([]);
    });
});
