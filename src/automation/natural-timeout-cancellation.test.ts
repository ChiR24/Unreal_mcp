import { afterEach, describe, expect, it, vi } from 'vitest';
import { automationRequests, cancelRequests, createDispatcher } from './dispatcher.test-support.js';
import { RequestTracker } from './request-tracker.js';
import {
    ABSOLUTE_MAX_TIMEOUT_MS,
    MAX_PROGRESS_EXTENSIONS,
    PROGRESS_EXTENSION_MS,
} from '../constants.js';

afterEach(() => {
    vi.useRealTimers();
});

describe('AutomationRequestDispatcher natural-timeout cancellation', () => {
    it('baseline: a response before the deadline resolves with no cancel frame and no residual state', async () => {
        vi.useFakeTimers();
        const tracker = new RequestTracker(50);
        const { dispatcher, sent } = createDispatcher(tracker);

        const p = dispatcher.sendAutomationRequest('get_actor', { a: 1 }, { timeoutMs: 1000, mcpRequestId: 'mcp:1' });
        const autoId = automationRequests(sent)[0]?.requestId as string;
        expect(typeof autoId).toBe('string');

        tracker.resolveRequest(autoId, { type: 'automation_response', requestId: autoId, success: true });

        await expect(p).resolves.toMatchObject({ success: true });
        expect(cancelRequests(sent)).toHaveLength(0);
        expect(tracker.getPendingCount()).toBe(0);
        expect(vi.getTimerCount()).toBe(0);

        // Correlation is torn down: a late explicit cancel emits no frame.
        dispatcher.cancelMcpRequest('mcp:1', 'late');
        expect(cancelRequests(sent)).toHaveLength(0);
    });

    it('ordinary deadline timeout sends exactly one cancel_request for the expired request', async () => {
        vi.useFakeTimers();
        const tracker = new RequestTracker(50);
        const { dispatcher, sent } = createDispatcher(tracker);

        const p = dispatcher.sendAutomationRequest('set_actor_transform', { actor: 'Cube' }, { timeoutMs: 1000, mcpRequestId: 'mcp:1' });
        const autoId = automationRequests(sent)[0]?.requestId as string;
        expect(automationRequests(sent)).toHaveLength(1);

        const rejection = expect(p).rejects.toThrow(`Request ${autoId} timed out after 1000ms`);
        await vi.advanceTimersByTimeAsync(1000);
        await rejection;

        const cancels = cancelRequests(sent);
        expect(cancels).toHaveLength(1);
        expect(cancels[0]?.requestId).toBe(autoId);
        expect(cancels[0]?.reason).toBe('natural timeout (ordinary_deadline)');
        expect(tracker.getPendingCount()).toBe(0);
        expect(vi.getTimerCount()).toBe(0);
    });

    it('progress-extension deadline timeout sends exactly one cancel_request', async () => {
        vi.useFakeTimers();
        const tracker = new RequestTracker(50);
        const { dispatcher, sent } = createDispatcher(tracker);

        const p = dispatcher.sendAutomationRequest('long_op', { job: 1 }, { timeoutMs: 1000, mcpRequestId: 'mcp:1' });
        const autoId = automationRequests(sent)[0]?.requestId as string;

        expect(tracker.extendTimeout(autoId, 10)).toBe(true);
        const rejection = expect(p).rejects.toThrow(`Request ${autoId} timed out after extension`);
        await vi.advanceTimersByTimeAsync(PROGRESS_EXTENSION_MS);
        await rejection;

        const cancels = cancelRequests(sent);
        expect(cancels).toHaveLength(1);
        expect(cancels[0]?.requestId).toBe(autoId);
        expect(cancels[0]?.reason).toBe('natural timeout (progress_extension_deadline)');
        expect(tracker.getPendingCount()).toBe(0);
    });

    it('stale progress (unchanged percent) sends exactly one cancel_request', async () => {
        vi.useFakeTimers();
        const tracker = new RequestTracker(50);
        const { dispatcher, sent } = createDispatcher(tracker);

        const p = dispatcher.sendAutomationRequest('long_op', { job: 1 }, { timeoutMs: 1000, mcpRequestId: 'mcp:1' });
        const autoId = automationRequests(sent)[0]?.requestId as string;

        expect(tracker.extendTimeout(autoId, 42)).toBe(true);
        expect(tracker.extendTimeout(autoId, 42)).toBe(true);
        expect(tracker.extendTimeout(autoId, 42)).toBe(true);
        const rejection = expect(p).rejects.toThrow(/stalled - progress unchanged at 42%/);
        expect(tracker.extendTimeout(autoId, 42)).toBe(false);
        await rejection;

        const cancels = cancelRequests(sent);
        expect(cancels).toHaveLength(1);
        expect(cancels[0]?.reason).toBe('natural timeout (stale_progress)');
        expect(tracker.getPendingCount()).toBe(0);
        expect(vi.getTimerCount()).toBe(0);
    });

    it('extension cap sends exactly one cancel_request', async () => {
        vi.useFakeTimers();
        const tracker = new RequestTracker(50);
        const { dispatcher, sent } = createDispatcher(tracker);

        const p = dispatcher.sendAutomationRequest('long_op', { job: 1 }, { timeoutMs: 1000, mcpRequestId: 'mcp:1' });
        const autoId = automationRequests(sent)[0]?.requestId as string;

        for (let i = 0; i < MAX_PROGRESS_EXTENSIONS; i++) {
            expect(tracker.extendTimeout(autoId, i)).toBe(true);
        }
        const rejection = expect(p).rejects.toThrow(/exceeded max progress extensions/);
        expect(tracker.extendTimeout(autoId, MAX_PROGRESS_EXTENSIONS)).toBe(false);
        await rejection;

        const cancels = cancelRequests(sent);
        expect(cancels).toHaveLength(1);
        expect(cancels[0]?.reason).toBe('natural timeout (extension_cap)');
        expect(tracker.getPendingCount()).toBe(0);
        expect(vi.getTimerCount()).toBe(0);
    });

    it('absolute deadline sends exactly one cancel_request even when the ordinary deadline is later', async () => {
        vi.useFakeTimers();
        const tracker = new RequestTracker(50);
        const { dispatcher, sent } = createDispatcher(tracker);

        const p = dispatcher.sendAutomationRequest('long_op', { job: 1 }, { timeoutMs: ABSOLUTE_MAX_TIMEOUT_MS + 1000, mcpRequestId: 'mcp:1' });
        const autoId = automationRequests(sent)[0]?.requestId as string;

        const rejection = expect(p).rejects.toThrow(/exceeded absolute max timeout/);
        await vi.advanceTimersByTimeAsync(ABSOLUTE_MAX_TIMEOUT_MS);
        await rejection;

        const cancels = cancelRequests(sent);
        expect(cancels).toHaveLength(1);
        expect(cancels[0]?.requestId).toBe(autoId);
        expect(cancels[0]?.reason).toBe('natural timeout (absolute_deadline)');
        expect(tracker.getPendingCount()).toBe(0);
        expect(vi.getTimerCount()).toBe(0);
    });

});
