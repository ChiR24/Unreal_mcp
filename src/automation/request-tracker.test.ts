import { afterEach, describe, expect, it, vi } from 'vitest';
import { RequestTracker } from './request-tracker.js';

afterEach(() => {
  vi.useRealTimers();
});

describe('RequestTracker coalescing', () => {
  it('clears absolute timeout when the request timeout fires', async () => {
    vi.useFakeTimers();
    const tracker = new RequestTracker(10);

    const { promise } = tracker.createRequest({ action: 'get_actor', payload: {}, timeoutMs: 100 });
    expect(vi.getTimerCount()).toBe(2);
    const rejection = expect(promise).rejects.toThrow(/timed out after 100ms/);

    await vi.advanceTimersByTimeAsync(100);

    await rejection;
    expect(tracker.getPendingCount()).toBe(0);
    expect(vi.getTimerCount()).toBe(0);
  });

  it('clears request timers when resolving a request', async () => {
    vi.useFakeTimers();
    const tracker = new RequestTracker(10);

    const { requestId, promise } = tracker.createRequest({ action: 'get_actor', payload: {}, timeoutMs: 1000 });
    expect(vi.getTimerCount()).toBe(2);

    tracker.resolveRequest(requestId, { type: 'response', requestId, success: true });

    await expect(promise).resolves.toMatchObject({ success: true });
    expect(tracker.getPendingCount()).toBe(0);
    expect(vi.getTimerCount()).toBe(0);
  });

  it('clears request timers when rejecting all requests', async () => {
    vi.useFakeTimers();
    const tracker = new RequestTracker(10);

    const first = tracker.createRequest({ action: 'get_actor', payload: {}, timeoutMs: 1000 });
    const second = tracker.createRequest({ action: 'list_assets', payload: {}, timeoutMs: 1000 });
    const firstRejection = expect(first.promise).rejects.toThrow('Connection lost');
    const secondRejection = expect(second.promise).rejects.toThrow('Connection lost');
    expect(vi.getTimerCount()).toBe(4);

    tracker.rejectAll(new Error('Connection lost'));

    await firstRejection;
    await secondRejection;
    expect(tracker.getPendingCount()).toBe(0);
    expect(vi.getTimerCount()).toBe(0);
  });
});

