// Task 44 lane A (RED first) — what a CLIENT actually observes on the wire.
//
// Every other Task 44 case asserts a primitive in isolation, which is exactly
// the shape of test that has repeatedly passed on this plan while the behaviour
// was absent on the wire. This suite therefore reads RAW JSON-RPC frames off a
// linked transport: no SDK Client wrapper is used for observation, so what is
// asserted is literally the bytes a client would receive.
//
// The decisive assertion is that `notifications/progress` carries back the
// CLIENT's own `_meta.progressToken`, with its original JavaScript type, and
// that no progress frame is emitted after the tools/call result.

import { afterEach, describe, expect, it, vi } from 'vitest';
import { InMemoryTransport } from '@modelcontextprotocol/sdk/inMemory.js';
import { getMcpRequestContext } from '../../../src/automation/request-context.js';

type Frame = Record<string, unknown>;

// Set by each case; the mocked tool handler calls it while the request is still
// in flight, standing in for an Unreal `progress_update` arriving mid-operation.
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

// Deterministic settle: resolve as soon as the awaited frame exists, driven by
// the event loop rather than a fixed sleep.
async function waitForFrame(
    frames: readonly Frame[],
    match: (frame: Frame) => boolean,
): Promise<Frame> {
    for (let attempt = 0; attempt < 2000; attempt += 1) {
        const found = frames.find(match);
        if (found) return found;
        await Promise.resolve();
        await new Promise<void>((resolve) => setImmediate(resolve));
    }
    throw new Error('expected frame never arrived');
}

const isResult = (id: number) => (frame: Frame) => frame.id === id && 'result' in frame;
const progressFrames = (frames: readonly Frame[]): Frame[] =>
    frames.filter((frame) => frame.method === 'notifications/progress');
const progressParams = (frames: readonly Frame[]): Record<string, unknown>[] =>
    progressFrames(frames).map((frame) => frame.params as Record<string, unknown>);

async function initialize(ctx: Harness): Promise<void> {
    await ctx.send({
        jsonrpc: '2.0',
        id: 1,
        method: 'initialize',
        params: {
            protocolVersion: '2025-11-25',
            capabilities: {},
            clientInfo: { name: 'task-44-wire', version: '1.0.0' },
        },
    });
    await waitForFrame(ctx.frames, isResult(1));
    await ctx.send({ jsonrpc: '2.0', method: 'notifications/initialized' });
}

async function callWithToken(ctx: Harness, meta: Record<string, unknown> | undefined): Promise<void> {
    await ctx.send({
        jsonrpc: '2.0',
        id: 2,
        method: 'tools/call',
        params: {
            name: 'unreal',
            arguments: { operation: 'execute', tool: 'inspect', action: 'inspect_object', params: {} },
            ...(meta ? { _meta: meta } : {}),
        },
    });
    await waitForFrame(ctx.frames, isResult(2));
}

afterEach(async () => {
    duringCall = undefined;
    for (const ctx of active.splice(0)) await ctx.close();
    vi.unstubAllEnvs();
});

describe('Task 44 — the client sees its own progress token on the wire', () => {
    it('echoes a STRING token back as a string', async () => {
        const ctx = await harness();
        await initialize(ctx);
        duringCall = () => {
            const requestId = getMcpRequestContext()?.requestId;
            if (requestId) ctx.built.automationBridge.reportRequestProgress(requestId, { progress: 30, total: 100 });
        };

        await callWithToken(ctx, { progressToken: 'client-string-token' });

        const params = progressParams(ctx.frames);
        expect(params).toHaveLength(1);
        expect(params[0]?.progressToken).toBe('client-string-token');
        expect(typeof params[0]?.progressToken).toBe('string');
        expect(params[0]?.progress).toBe(30);
    });

    it('echoes a NUMBER token back as a number, never stringified', async () => {
        const ctx = await harness();
        await initialize(ctx);
        duringCall = () => {
            const requestId = getMcpRequestContext()?.requestId;
            if (requestId) ctx.built.automationBridge.reportRequestProgress(requestId, { progress: 1 });
        };

        await callWithToken(ctx, { progressToken: 4242 });

        const params = progressParams(ctx.frames);
        expect(params).toHaveLength(1);
        expect(params[0]?.progressToken).toBe(4242);
        expect(typeof params[0]?.progressToken).toBe('number');
    });

    it('emits NO progress frame when the client sent no token', async () => {
        const ctx = await harness();
        await initialize(ctx);
        duringCall = () => {
            const requestId = getMcpRequestContext()?.requestId;
            if (requestId) ctx.built.automationBridge.reportRequestProgress(requestId, { progress: 50 });
        };

        await callWithToken(ctx, undefined);

        // No token means no correlation is possible, so silence is the only
        // honest answer — a server-invented token would be worse than nothing.
        expect(progressFrames(ctx.frames)).toEqual([]);
    });

    it('never sends a progress frame after the terminal result', async () => {
        const ctx = await harness();
        await initialize(ctx);
        let late: (() => void) | undefined;
        duringCall = () => {
            const requestId = getMcpRequestContext()?.requestId;
            if (!requestId) return;
            ctx.built.automationBridge.reportRequestProgress(requestId, { progress: 10 });
            late = () => ctx.built.automationBridge.reportRequestProgress(requestId, { progress: 90 });
        };

        await callWithToken(ctx, { progressToken: 'tok' });
        const beforeLate = progressFrames(ctx.frames).length;
        late?.();
        await new Promise<void>((resolve) => setImmediate(resolve));

        expect(beforeLate).toBe(1);
        expect(progressFrames(ctx.frames)).toHaveLength(1);
        const resultIndex = ctx.frames.findIndex(isResult(2));
        const lastProgressIndex = ctx.frames.map((f) => f.method).lastIndexOf('notifications/progress');
        expect(lastProgressIndex).toBeLessThan(resultIndex);
    });

    it('keeps progress monotonic on the wire', async () => {
        const ctx = await harness();
        await initialize(ctx);
        duringCall = () => {
            const requestId = getMcpRequestContext()?.requestId;
            if (!requestId) return;
            const report = (progress: number) =>
                ctx.built.automationBridge.reportRequestProgress(requestId, { progress });
            report(10);
            report(5);
            report(10);
            report(40);
        };

        await callWithToken(ctx, { progressToken: 'tok' });

        expect(progressParams(ctx.frames).map((p) => p.progress)).toEqual([10, 40]);
    });
});
