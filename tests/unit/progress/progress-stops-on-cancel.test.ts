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

import { afterEach, describe, expect, it } from 'vitest';
import { getMcpRequestContext } from '../../../src/automation/request-context.js';
import { connectFrames, type Frame, initialize, progressFrames, waitFor } from '../support/in-memory-server.js';

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

type Harness = Awaited<ReturnType<typeof connectFrames>>;
const active: Harness[] = [];

async function harness(): Promise<Harness> {
    const ctx = await connectFrames(async () => {
        onHandlerEntered?.(getMcpRequestContext()?.requestId);
        if (gate) await gate.promise;
        return { success: true, operation: 'execute', message: 'done' };
    });
    active.push(ctx);
    await initialize(ctx, 'task-44-cancel');
    return ctx;
}

async function settle(): Promise<void> {
    for (let turn = 0; turn < 40; turn += 1) {
        await Promise.resolve();
        await new Promise<void>((resolve) => setImmediate(resolve));
    }
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
});

describe('Task 44 — a cancelled request stops receiving progress', () => {
    it('emits NO progress frame once the client has cancelled', async () => {
        const ctx = await harness();
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
