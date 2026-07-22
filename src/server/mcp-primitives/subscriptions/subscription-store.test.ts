// Task 34 — revisioned per-session resource subscription store.
//
// RED-first: this suite is written before subscription-store.ts stabilizes, so it
// fails to import until the store lands. It proves explicit-session isolation,
// idempotent duplicate subscribe, boundary rejection of malformed session/URI,
// the bounded per-session cap with deterministic oldest-first eviction, the
// onRelease cleanup hook firing on eviction/unsubscribe/clear, and that
// clearSession drains a session's map to zero. No transport, no timers.

import { describe, expect, it, vi } from 'vitest';
import { SubscriptionStore } from './subscription-store.js';

const SELECTION = 'ue://selection';
const LEVEL = 'ue://level';
const PROJECT = 'ue://project';
const A = 'session-A';
const B = 'session-B';

describe('SubscriptionStore — explicit-session subscription state', () => {
  it('reports an unknown session as empty without reseeding it', () => {
    // Given a fresh store
    const store = new SubscriptionStore();

    // When / Then a never-seen session is empty and unsubscribed
    expect(store.count('never-seen')).toBe(0);
    expect(store.isSubscribed('never-seen', SELECTION)).toBe(false);
    expect(store.subscriptions('never-seen')).toEqual([]);
    expect(store.hasSession('never-seen')).toBe(false);
  });

  it('accepts a first subscription and reports it', () => {
    // Given
    const store = new SubscriptionStore();

    // When
    const result = store.subscribe(A, SELECTION);

    // Then
    expect(result).toEqual({ accepted: true, alreadySubscribed: false, evicted: null, reason: null });
    expect(store.isSubscribed(A, SELECTION)).toBe(true);
    expect(store.count(A)).toBe(1);
  });

  it('treats a duplicate subscribe as idempotent (no growth, no eviction)', () => {
    // Given a session already subscribed
    const store = new SubscriptionStore();
    store.subscribe(A, SELECTION);

    // When it subscribes to the same URI again
    const again = store.subscribe(A, SELECTION);

    // Then it is reported as already subscribed and the set did not grow
    expect(again).toEqual({ accepted: true, alreadySubscribed: true, evicted: null, reason: null });
    expect(store.count(A)).toBe(1);
    expect(store.subscriptions(A)).toEqual([SELECTION]);
  });

  it('rejects a URI outside the Task 31 allowlist with no state change', () => {
    // Given
    const store = new SubscriptionStore();

    // When a non-subscribable URI is used
    const result = store.subscribe(A, 'ue://assets');

    // Then it is rejected and nothing is stored
    expect(result.accepted).toBe(false);
    expect(result.reason).toBe('NOT_SUBSCRIBABLE');
    expect(store.hasSession(A)).toBe(false);
  });

  it('rejects a blank session id at the boundary', () => {
    // Given
    const store = new SubscriptionStore();

    // When / Then blank and whitespace session ids are rejected
    expect(store.subscribe('', SELECTION).reason).toBe('INVALID_SESSION');
    expect(store.subscribe('   ', SELECTION).reason).toBe('INVALID_SESSION');
    expect(store.sessionCount()).toBe(0);
  });

  it('isolates one session from another', () => {
    // Given A is subscribed to selection
    const store = new SubscriptionStore();
    store.subscribe(A, SELECTION);

    // When B is inspected and then cleared
    // Then B never inherits A's subscription and clearing B leaves A intact
    expect(store.isSubscribed(B, SELECTION)).toBe(false);
    expect(store.clearSession(B)).toBe(0);
    expect(store.isSubscribed(A, SELECTION)).toBe(true);
  });

  it('caps a session and evicts the oldest subscription deterministically', () => {
    // Given a cap of two with an onRelease spy
    const onRelease = vi.fn();
    const store = new SubscriptionStore({ maxPerSession: 2, onRelease });
    store.subscribe(A, SELECTION);
    store.subscribe(A, LEVEL);

    // When a third distinct URI is added
    const result = store.subscribe(A, PROJECT);

    // Then the oldest (SELECTION) is evicted, released, and order is preserved
    expect(result.accepted).toBe(true);
    expect(result.evicted).toBe(SELECTION);
    expect(store.subscriptions(A)).toEqual([LEVEL, PROJECT]);
    expect(store.count(A)).toBe(2);
    expect(onRelease).toHaveBeenCalledWith(A, SELECTION);
  });

  it('never evicts on an idempotent re-subscribe at the cap', () => {
    // Given a full session at cap two
    const onRelease = vi.fn();
    const store = new SubscriptionStore({ maxPerSession: 2, onRelease });
    store.subscribe(A, SELECTION);
    store.subscribe(A, LEVEL);

    // When an already-subscribed URI is re-subscribed
    const again = store.subscribe(A, SELECTION);

    // Then nothing is evicted and no release fires
    expect(again.alreadySubscribed).toBe(true);
    expect(again.evicted).toBeNull();
    expect(onRelease).not.toHaveBeenCalled();
    expect(store.subscriptions(A)).toEqual([SELECTION, LEVEL]);
  });

  it('unsubscribes, releasing the URI, and is well-defined on repeats', () => {
    // Given a subscribed session with a release spy
    const onRelease = vi.fn();
    const store = new SubscriptionStore({ onRelease });
    store.subscribe(A, SELECTION);

    // When it unsubscribes twice and an unknown session unsubscribes
    const first = store.unsubscribe(A, SELECTION);
    const second = store.unsubscribe(A, SELECTION);
    const unknown = store.unsubscribe('ghost', SELECTION);

    // Then only the first release counts and the session drains away
    expect(first).toBe(true);
    expect(second).toBe(false);
    expect(unknown).toBe(false);
    expect(onRelease).toHaveBeenCalledTimes(1);
    expect(onRelease).toHaveBeenCalledWith(A, SELECTION);
    expect(store.hasSession(A)).toBe(false);
  });

  it('drains a whole session to zero on clearSession, releasing each URI', () => {
    // Given three subscriptions with a release spy
    const onRelease = vi.fn();
    const store = new SubscriptionStore({ onRelease });
    store.subscribe(A, SELECTION);
    store.subscribe(A, LEVEL);
    store.subscribe(A, PROJECT);

    // When the session is cleared
    const released = store.clearSession(A);

    // Then every URI is released and the map drains to zero
    expect(released).toBe(3);
    expect(onRelease).toHaveBeenCalledTimes(3);
    expect(store.count(A)).toBe(0);
    expect(store.hasSession(A)).toBe(false);
    expect(store.sessionCount()).toBe(0);
  });

  it('lists the sessions subscribed to a URI for producer fan-out', () => {
    // Given A and B on selection, C on level
    const store = new SubscriptionStore();
    store.subscribe(A, SELECTION);
    store.subscribe(B, SELECTION);
    store.subscribe('session-C', LEVEL);

    // When / Then only the selection subscribers are returned
    expect(store.sessionsSubscribedTo(SELECTION).sort()).toEqual([A, B]);
    expect(store.sessionsSubscribedTo('ue://assets')).toEqual([]);
  });

  it('rejects an invalid cap at construction', () => {
    // Given / When / Then a non-positive or non-integer cap is a programming error
    expect(() => new SubscriptionStore({ maxPerSession: 0 })).toThrow(RangeError);
    expect(() => new SubscriptionStore({ maxPerSession: -3 })).toThrow(RangeError);
    expect(() => new SubscriptionStore({ maxPerSession: 2.5 })).toThrow(RangeError);
  });
});
