import { afterEach, describe, expect, it, vi } from 'vitest';
import { RequestTracker } from './request-tracker.js';

afterEach(() => {
  vi.useRealTimers();
});

describe('RequestTracker coalescing', () => {
  it('uses stable keys for equivalent nested payloads', () => {
    const tracker = new RequestTracker(10);

    const first = tracker.createCoalesceKey('get_actor', {
      filter: { tags: ['enemy', 'flying'], bounds: { z: 10, x: 1 } }
    });
    const second = tracker.createCoalesceKey('get_actor', {
      filter: { bounds: { x: 1, z: 10 }, tags: ['enemy', 'flying'] }
    });

    expect(first).toBe(second);
  });

  it('does not coalesce different nested payloads', () => {
    const tracker = new RequestTracker(10);

    const first = tracker.createCoalesceKey('get_actor', { filter: { bounds: { x: 1 } } });
    const second = tracker.createCoalesceKey('get_actor', { filter: { bounds: { x: 2 } } });

    expect(first).not.toBe(second);
  });

  it('does not coalesce mutating actions', () => {
    const tracker = new RequestTracker(10);

    expect(tracker.createCoalesceKey('set_actor_transform', { actor: 'Cube' })).toBe('');
  });

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

describe('RequestTracker ownership settlement', () => {
  it('stamps the owner on a pending request after creation', () => {
    const tracker = new RequestTracker(10);

    const { requestId } = tracker.createRequest({ action: 'get_actor', payload: {}, timeoutMs: 1000 });
    tracker.setOwnerId(requestId, 'conn-primary');

    expect(tracker.getPendingRequest(requestId)?.ownerId).toBe('conn-primary');
  });

  it('is a no-op when stamping an unknown request id', () => {
    const tracker = new RequestTracker(10);

    tracker.setOwnerId('missing', 'conn-primary');

    expect(tracker.getPendingCount()).toBe(0);
  });

  it('rejects only the requests owned by the given connection and clears their timers', async () => {
    vi.useFakeTimers();
    const tracker = new RequestTracker(10);

    const owned = tracker.createRequest({ action: 'get_actor', payload: {}, timeoutMs: 1000 });
    const other = tracker.createRequest({ action: 'list_assets', payload: {}, timeoutMs: 1000 });
    tracker.setOwnerId(owned.requestId, 'conn-primary');
    tracker.setOwnerId(other.requestId, 'conn-secondary');
    expect(vi.getTimerCount()).toBe(4);

    const settled = tracker.rejectOwnedBy('conn-primary', new Error('primary lost'));

    expect(settled).toBe(1);
    await expect(owned.promise).rejects.toThrow('primary lost');
    expect(tracker.getPendingCount()).toBe(1);
    expect(vi.getTimerCount()).toBe(2);
  });

  it('never notifies the natural-timeout observer when settling by owner', () => {
    const tracker = new RequestTracker(10);
    const observer = vi.fn();
    tracker.setNaturalTimeoutObserver(observer);

    const { requestId, promise } = tracker.createRequest({ action: 'get_actor', payload: {}, timeoutMs: 1000 });
    tracker.setOwnerId(requestId, 'conn-primary');
    tracker.rejectOwnedBy('conn-primary', new Error('primary lost'));

    expect(observer).not.toHaveBeenCalled();
    void promise.catch(() => undefined);
  });

  it('is idempotent: a second owner sweep settles nothing', () => {
    const tracker = new RequestTracker(10);

    const { requestId, promise } = tracker.createRequest({ action: 'get_actor', payload: {}, timeoutMs: 1000 });
    tracker.setOwnerId(requestId, 'conn-primary');
    void promise.catch(() => undefined);

    expect(tracker.rejectOwnedBy('conn-primary', new Error('lost'))).toBe(1);
    expect(tracker.rejectOwnedBy('conn-primary', new Error('lost'))).toBe(0);
    expect(tracker.getPendingCount()).toBe(0);
  });
});
