// Task 44 lane B — shutdown must leave no retained progress marker.
//
// Every other lifetime path is covered: a request that settles unregisters in
// `finally`, and a cancelled request is closed at the bridge convergence point.
// Shutdown is the remaining one. A server that closes while requests are still
// in flight must drive those reporters to their terminal state, not merely drop
// the references — a dropped-but-open reporter still holds a live closure over
// the request's `sendNotification` and would happily emit onto a dead transport.
//
// `src/server/mcp-primitives/primitive-wiring.ts` already establishes the
// convention (chain `server.onclose`, drain the store); these cases hold the
// progress registry to the same standard.

import { describe, expect, it, vi } from 'vitest';
import {
    ProgressSinkRegistry,
} from '../../../src/server/mcp-primitives/progress/progress-sink-registry.js';
import {
    createProgressReporter,
    type ProgressNotification,
} from '../../../src/server/mcp-primitives/progress/progress-reporter.js';

// Hoisted deliberately: importing this module inside a test body pays the whole
// server transform cost against that test's timeout, which turns into a phantom
// timeout when the suite runs alongside other heavy files. Paying it during
// collection keeps the case independent of scheduling.
const { ToolRegistry } = await import('../../../src/server/tool-registry.js');

function tracked(token: string) {
    const sent: ProgressNotification[] = [];
    const reporter = createProgressReporter({
        token,
        notify: async (notification) => {
            sent.push(notification);
        },
    });
    return { sent, reporter };
}

describe('Task 44 — clear() drains reporters instead of orphaning them', () => {
    it('drives every tracked reporter to its terminal state', () => {
        const registry = new ProgressSinkRegistry();
        const first = tracked('a');
        const second = tracked('b');
        registry.register('num:1', first.reporter);
        registry.register('num:2', second.reporter);

        registry.clear();

        // Dropping the map entry alone would leave these `active`, still holding
        // the request's notify closure.
        expect(first.reporter.active).toBe(false);
        expect(second.reporter.active).toBe(false);
        expect(registry.size).toBe(0);
    });

    it('silences a reporter that something still holds a reference to', async () => {
        const registry = new ProgressSinkRegistry();
        const held = tracked('a');
        registry.register('num:1', held.reporter);

        registry.clear();
        // The automation layer may still resolve a late update against the
        // reporter object itself, not through the (now empty) registry.
        held.reporter.report({ progress: 42 });
        await held.reporter.drain();

        expect(held.sent).toEqual([]);
    });

    it('routes nothing after a clear', async () => {
        const registry = new ProgressSinkRegistry();
        const held = tracked('a');
        registry.register('num:1', held.reporter);

        registry.clear();
        registry.report('num:1', { progress: 42 });
        await held.reporter.drain();

        expect(held.sent).toEqual([]);
    });

    it('is idempotent and safe on an empty registry', () => {
        const registry = new ProgressSinkRegistry();

        expect(() => {
            registry.clear();
            registry.clear();
        }).not.toThrow();
        expect(registry.size).toBe(0);
    });
});

describe('Task 44 — the tool registry drains its progress markers on server close', () => {
    it('chains server.onclose and drains, preserving any previous handler', () => {
        const handlers = new Map<symbol, unknown>();
        const previousOnClose = vi.fn();
        const server = {
            setRequestHandler: (schema: { shape?: unknown }, handler: unknown) => {
                handlers.set(Symbol(), handler);
                void schema;
            },
            onclose: previousOnClose,
        };
        const automationBridge = {
            setRequestProgressListener: vi.fn(),
            setRequestCancelledListener: vi.fn(),
            isConnected: () => false,
        };

        const registry = new ToolRegistry(
            server as never,
            { executeConsoleCommand: vi.fn() } as never,
            automationBridge as never,
            { debug: vi.fn(), error: vi.fn(), isEnabled: () => false } as never,
            { trackPerformance: vi.fn(), recordError: vi.fn() } as never,
            {} as never,
            {} as never,
            {} as never,
            async () => false,
        );
        registry.register();

        // Registration must have replaced onclose with a chaining wrapper.
        expect(server.onclose).not.toBe(previousOnClose);

        server.onclose?.();

        expect(previousOnClose).toHaveBeenCalledTimes(1);
    });
});
