import { afterEach, describe, expect, it, vi } from 'vitest';
import { automationRequests, cancelRequests, createDispatcher } from './dispatcher.test-support.js';
import { MessageHandler } from './message-handler.js';
import { McpRequestCancelledError } from './request-cancellation-error.js';
import { RequestTracker } from './request-tracker.js';

afterEach(() => {
    vi.useRealTimers();
});

describe('AutomationRequestDispatcher natural-timeout cancellation races', () => {
    it('an explicit cancel after a natural timeout emits no second frame', async () => {
        vi.useFakeTimers();
        const tracker = new RequestTracker(50);
        const { dispatcher, sent } = createDispatcher(tracker);

        const p = dispatcher.sendAutomationRequest('get_actor', { a: 1 }, { timeoutMs: 1000, mcpRequestId: 'mcp:1' });

        const rejection = expect(p).rejects.toThrow('timed out');
        await vi.advanceTimersByTimeAsync(1000);
        await rejection;
        expect(cancelRequests(sent)).toHaveLength(1);

        dispatcher.cancelMcpRequest('mcp:1', 'explicit late');
        expect(cancelRequests(sent)).toHaveLength(1);
        expect(tracker.getPendingCount()).toBe(0);
    });

    it('an explicit cancel before the deadline and the natural timeout together emit at most one frame', async () => {
        vi.useFakeTimers();
        const tracker = new RequestTracker(50);
        const { dispatcher, sent } = createDispatcher(tracker);

        const p = dispatcher.sendAutomationRequest('get_actor', { a: 1 }, { timeoutMs: 1000, mcpRequestId: 'mcp:1' });

        dispatcher.cancelMcpRequest('mcp:1', 'explicit');
        await expect(p).rejects.toBeInstanceOf(McpRequestCancelledError);
        expect(cancelRequests(sent)).toHaveLength(1);

        // The cleared natural deadline must not emit a second frame.
        await vi.advanceTimersByTimeAsync(2000);
        expect(cancelRequests(sent)).toHaveLength(1);
        expect(tracker.getPendingCount()).toBe(0);
        expect(vi.getTimerCount()).toBe(0);
    });

    it('late response, progress, event, and unknown frames cannot resolve or resurrect an expired request', async () => {
        vi.useFakeTimers();
        const tracker = new RequestTracker(50);
        const handler = new MessageHandler(tracker);
        const { dispatcher, sent } = createDispatcher(tracker);

        const p = dispatcher.sendAutomationRequest('get_actor', { a: 1 }, { timeoutMs: 1000, mcpRequestId: 'mcp:1' });
        const autoId = automationRequests(sent)[0]?.requestId as string;

        const rejection = expect(p).rejects.toThrow('timed out');
        await vi.advanceTimersByTimeAsync(1000);
        await rejection;

        handler.handleMessage({ type: 'automation_response', requestId: autoId, success: true, result: { ok: 1 } });
        handler.handleMessage({ type: 'progress_update', requestId: autoId, percent: 50 });
        handler.handleMessage({ type: 'automation_event', requestId: autoId, event: 'late_event' });
        handler.handleMessage({ type: 'bogus_frame', requestId: autoId });

        expect(tracker.getPendingCount()).toBe(0);
        expect(vi.getTimerCount()).toBe(0);
        expect(cancelRequests(sent)).toHaveLength(1);
    });
});
