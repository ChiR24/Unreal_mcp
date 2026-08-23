import { afterEach, describe, expect, it, vi } from 'vitest';
import { Logger } from '../utils/logging/logger.js';
import {
    AutomationRequestDispatcher,
    type AutomationRequestDispatcherDependencies,
} from './bridge-request-dispatcher.js';
import { RequestTracker } from './request-tracker.js';
import type { AutomationBridgeMessage } from './types.js';

afterEach(() => {
    vi.useRealTimers();
});

function createDispatcher(
    tracker: RequestTracker,
    overrides: Partial<AutomationRequestDispatcherDependencies> = {},
) {
    const sent: Array<Record<string, unknown>> = [];
    const deps: AutomationRequestDispatcherDependencies = {
        enabled: true,
        maxQueuedRequests: 5,
        connectionTimeoutMs: 1000,
        requestTracker: tracker,
        log: new Logger('test'),
        isConnected: () => true,
        send: (payload: AutomationBridgeMessage) => {
            sent.push(payload as Record<string, unknown>);
            return true;
        },
        startClient: () => {},
        abortPendingConnection: () => {},
        once: () => {},
        off: () => {},
        ...overrides,
    };
    return { dispatcher: new AutomationRequestDispatcher(deps), sent };
}

const automationRequests = (sent: Array<Record<string, unknown>>) =>
    sent.filter((m) => m.type === 'automation_request');
const cancelRequests = (sent: Array<Record<string, unknown>>) =>
    sent.filter((m) => m.type === 'cancel_request');

describe('AutomationRequestDispatcher natural-timeout cancellation delivery', () => {
    it('a send(false) result for the cancel frame settles without throwing and is attempted once', async () => {
        vi.useFakeTimers();
        const tracker = new RequestTracker(50);
        let cancelSend = 0;
        const { dispatcher, sent } = createDispatcher(tracker, {
            send: (payload) => {
                if (payload.type === 'cancel_request') {
                    cancelSend += 1;
                    return false;
                }
                sent.push(payload as Record<string, unknown>);
                return true;
            }
        });

        const p = dispatcher.sendAutomationRequest('get_actor', { a: 1 }, { timeoutMs: 1000, mcpRequestId: 'mcp:1' });
        const rejection = expect(p).rejects.toThrow('timed out');
        await vi.advanceTimersByTimeAsync(1000);
        await rejection;

        expect(cancelSend).toBe(1);
        expect(tracker.getPendingCount()).toBe(0);
    });

    it('a throwing cancel frame delivery does not break the natural timeout rejection', async () => {
        vi.useFakeTimers();
        const tracker = new RequestTracker(50);
        const { dispatcher, sent } = createDispatcher(tracker, {
            send: (payload) => {
                if (payload.type === 'cancel_request') throw new Error('delivery boom');
                sent.push(payload as Record<string, unknown>);
                return true;
            }
        });

        const p = dispatcher.sendAutomationRequest('get_actor', { a: 1 }, { timeoutMs: 1000, mcpRequestId: 'mcp:1' });
        const rejection = expect(p).rejects.toThrow('timed out');
        await vi.advanceTimersByTimeAsync(1000);
        await rejection;

        expect(tracker.getPendingCount()).toBe(0);
        expect(vi.getTimerCount()).toBe(0);
    });

    it('a timed-out mutation is never replayed and emits exactly one original request', async () => {
        vi.useFakeTimers();
        const tracker = new RequestTracker(50);
        const { dispatcher, sent } = createDispatcher(tracker);

        const p = dispatcher.sendAutomationRequest('delete_asset', { path: '/Game/Hero' }, { timeoutMs: 1000, mcpRequestId: 'mcp:1' });
        const autoId = automationRequests(sent)[0]?.requestId as string;

        const rejection = expect(p).rejects.toThrow('timed out');
        await vi.advanceTimersByTimeAsync(1000);
        await rejection;

        expect(automationRequests(sent)).toHaveLength(1);
        const cancels = cancelRequests(sent);
        expect(cancels).toHaveLength(1);
        expect(cancels[0]?.requestId).toBe(autoId);
        expect(tracker.getPendingCount()).toBe(0);
    });

    it('queue draining resumes after a timeout frees pending capacity', async () => {
        vi.useFakeTimers();
        const tracker = new RequestTracker(1);
        const { dispatcher, sent } = createDispatcher(tracker);

        const first = dispatcher.sendAutomationRequest('spawn', { a: 1 }, { timeoutMs: 500, mcpRequestId: 'mcp:1' });
        const second = dispatcher.sendAutomationRequest('spawn', { a: 2 }, { timeoutMs: 500, mcpRequestId: 'mcp:2' });
        expect(automationRequests(sent)).toHaveLength(1);

        const firstRejection = expect(first).rejects.toThrow('timed out');
        const secondRejection = expect(second).rejects.toThrow('timed out');

        await vi.advanceTimersByTimeAsync(500);
        await firstRejection;

        // Draining after the timeout frees capacity must send the queued request.
        expect(automationRequests(sent)).toHaveLength(2);
        await vi.advanceTimersByTimeAsync(500);
        await secondRejection;

        expect(cancelRequests(sent)).toHaveLength(2);
        expect(tracker.getPendingCount()).toBe(0);
        expect(vi.getTimerCount()).toBe(0);
    });
});
