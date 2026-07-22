// Task 34 — bounded, debounced/coalesced resource-notification engine.
//
// RED-first fake-clock suite. It proves: a burst inside the window coalesces to
// one emission; the emitted payload is bounded to exactly URI/revision/changeKind;
// revisions are monotonic (a stale lower revision is suppressed); an unsubscribed
// or disconnected session gets nothing; a flush that fires after unsubscribe/clear
// is suppressed and pending drains to zero; the release hook drains the coalescer;
// and a catalog notification's revision equals a SUBSEQUENT resource read through
// the shared injected RevisionProvider. No wall clock, no transport write.

import { describe, expect, it } from 'vitest';
import type { CatalogRevisionReader } from '../catalog-revision-reader.js';
import {
  CapabilityResources,
  type CapabilitySource,
} from '../../../resources/capability-resources.js';
import { InMemoryRevisionProvider, asResourceRevision } from '../resource-revision.js';
import { NotificationCoalescer } from './notification-coalescer.js';
import { SubscriptionStore } from './subscription-store.js';
import { CATALOG_SUBSCRIPTION_URI, type ResourceUpdatedPayload } from './subscription-types.js';

const A = 'session-A';
const B = 'session-B';
const SELECTION = 'ue://selection';
const LEVEL = 'ue://level';

/** Per-session catalog state cursor test double (Task 36 C1 shape). */
class CountingCatalogReader implements CatalogRevisionReader {
  private readonly counts = new Map<string, number>();

  getCatalogStateRevision(sessionId: string): number {
    return this.counts.get(sessionId) ?? 0;
  }

  bump(sessionId: string): void {
    this.counts.set(sessionId, this.getCatalogStateRevision(sessionId) + 1);
  }
}

interface Harness {
  readonly store: SubscriptionStore;
  readonly coalescer: NotificationCoalescer;
  readonly revisions: InMemoryRevisionProvider;
  readonly catalog: CountingCatalogReader;
  readonly emitted: ReadonlyArray<{ sessionId: string; payload: ResourceUpdatedPayload }>;
  advance(ms: number): void;
  now(): number;
}

function setup(options: { windowMs?: number; maxPerSession?: number; wireRelease?: boolean } = {}): Harness {
  const clockState = { value: 0 };
  const emitted: Array<{ sessionId: string; payload: ResourceUpdatedPayload }> = [];
  const revisions = new InMemoryRevisionProvider();
  const catalog = new CountingCatalogReader();
  // Holder breaks the store<->coalescer construction cycle (store built first).
  const ref: { coalescer?: NotificationCoalescer } = {};
  const store = new SubscriptionStore({
    maxPerSession: options.maxPerSession,
    onRelease: options.wireRelease === true ? (sessionId, uri) => ref.coalescer?.dropPending(sessionId, uri) : undefined,
  });
  const coalescer = new NotificationCoalescer({
    store,
    revisions,
    catalog,
    clock: () => clockState.value,
    windowMs: options.windowMs ?? 50,
    sink: (sessionId, payload) => emitted.push({ sessionId, payload }),
  });
  ref.coalescer = coalescer;
  return {
    store,
    coalescer,
    revisions,
    catalog,
    emitted,
    advance: (ms) => {
      clockState.value += ms;
    },
    now: () => clockState.value,
  };
}

describe('NotificationCoalescer — debounce, coalesce, and bounded emission', () => {
  it('holds a change until the window elapses, then emits one bounded payload', () => {
    // Given a subscribed session and an advanced revision
    const h = setup({ windowMs: 50 });
    h.store.subscribe(A, SELECTION);
    h.revisions.set(SELECTION, asResourceRevision(3));

    // When the change is recorded and flushed before the window
    expect(h.coalescer.recordChange(A, SELECTION, 'updated').recorded).toBe(true);
    expect(h.coalescer.flushDue(h.now())).toBe(0);

    // Then only after the window does it emit, carrying exactly three keys
    h.advance(50);
    expect(h.coalescer.flushDue(h.now())).toBe(1);
    expect(h.emitted).toEqual([
      { sessionId: A, payload: { uri: SELECTION, revision: 3, changeKind: 'updated' } },
    ]);
    expect(Object.keys(h.emitted[0].payload).sort()).toEqual(['changeKind', 'revision', 'uri']);
  });

  it('coalesces a burst on the same session+URI into one emission (latest kind wins)', () => {
    // Given three changes inside one window
    const h = setup({ windowMs: 50 });
    h.store.subscribe(A, SELECTION);
    h.revisions.set(SELECTION, asResourceRevision(2));
    h.coalescer.recordChange(A, SELECTION, 'updated');
    h.advance(10);
    h.coalescer.recordChange(A, SELECTION, 'invalidated');
    h.advance(10);
    h.coalescer.recordChange(A, SELECTION, 'updated');

    // Then they fold into a single pending entry
    expect(h.coalescer.pendingCount(A)).toBe(1);

    // When the fixed window elapses (it does not slide with each change)
    h.advance(40);
    expect(h.coalescer.flushDue(h.now())).toBe(1);
    expect(h.emitted).toHaveLength(1);
    expect(h.emitted[0].payload.changeKind).toBe('updated');
  });

  it('emits nothing for an unsubscribed or disconnected session', () => {
    // Given a session that never subscribed
    const h = setup();

    // When a change is recorded for it
    const result = h.coalescer.recordChange(A, SELECTION, 'updated');

    // Then it is dropped at record time and never emitted
    expect(result).toEqual({ recorded: false, reason: 'NOT_SUBSCRIBED' });
    h.advance(1000);
    expect(h.coalescer.flushDue(h.now())).toBe(0);
    expect(h.emitted).toHaveLength(0);
  });

  it('suppresses a flush that fires after the subscription was unsubscribed', () => {
    // Given a recorded change on a live subscription
    const h = setup({ windowMs: 50 });
    h.store.subscribe(A, SELECTION);
    h.coalescer.recordChange(A, SELECTION, 'updated');

    // When the session unsubscribes before the window elapses (the "late timer")
    h.store.unsubscribe(A, SELECTION);
    h.advance(100);

    // Then the flush emits nothing and drains the pending entry
    expect(h.coalescer.flushDue(h.now())).toBe(0);
    expect(h.emitted).toHaveLength(0);
    expect(h.coalescer.pendingCount()).toBe(0);
  });

  it('drains coalescer state on clearSession so a later flush is suppressed', () => {
    // Given a recorded change
    const h = setup({ windowMs: 50 });
    h.store.subscribe(A, SELECTION);
    h.coalescer.recordChange(A, SELECTION, 'updated');

    // When the coalescer session is cleared
    h.coalescer.clearSession(A);

    // Then pending drains to zero and a late flush emits nothing
    expect(h.coalescer.pendingCount(A)).toBe(0);
    h.advance(100);
    expect(h.coalescer.flushDue(h.now())).toBe(0);
  });

  it('drains a pending entry through the release hook on unsubscribe', () => {
    // Given the store release hook wired to the coalescer
    const h = setup({ windowMs: 50, wireRelease: true });
    h.store.subscribe(A, SELECTION);
    h.coalescer.recordChange(A, SELECTION, 'updated');
    expect(h.coalescer.pendingCount(A)).toBe(1);

    // When the session unsubscribes (fires onRelease -> dropPending)
    h.store.unsubscribe(A, SELECTION);

    // Then the coalescer pending drained immediately
    expect(h.coalescer.pendingCount(A)).toBe(0);
  });

  it('drops the evicted URI pending when the cap evicts through the release hook', () => {
    // Given a cap of one with the release hook wired
    const h = setup({ windowMs: 50, maxPerSession: 1, wireRelease: true });
    h.store.subscribe(A, SELECTION);
    h.coalescer.recordChange(A, SELECTION, 'updated');
    expect(h.coalescer.pendingCount(A)).toBe(1);

    // When a new subscription evicts SELECTION
    h.store.subscribe(A, LEVEL);

    // Then the evicted URI's pending was dropped
    expect(h.coalescer.pendingCount(A)).toBe(0);
  });

  it('enforces monotonic revisions, suppressing a stale lower revision', () => {
    // Given a first emission at revision 5
    const h = setup({ windowMs: 10 });
    h.store.subscribe(A, SELECTION);
    h.revisions.set(SELECTION, asResourceRevision(5));
    h.coalescer.recordChange(A, SELECTION, 'updated');
    h.advance(10);
    h.coalescer.flushDue(h.now());

    // When a stale lower revision is observed for the same key
    h.revisions.set(SELECTION, asResourceRevision(4));
    h.coalescer.recordChange(A, SELECTION, 'updated');
    h.advance(10);
    const suppressed = h.coalescer.flushDue(h.now());

    // And then a higher revision is observed
    h.revisions.set(SELECTION, asResourceRevision(7));
    h.coalescer.recordChange(A, SELECTION, 'updated');
    h.advance(10);
    h.coalescer.flushDue(h.now());

    // Then only revisions 5 and 7 were emitted, strictly increasing
    expect(suppressed).toBe(0);
    expect(h.emitted.map((e) => e.payload.revision)).toEqual([5, 7]);
  });

  it('emits a catalog notification whose revision equals a subsequent resource read', () => {
    // Given ONE RevisionProvider shared by the coalescer and the resource reader
    const h = setup({ windowMs: 20 });
    const stubCapabilitySource: CapabilitySource = {
      entries: () => [{ id: 'manage_asset', category: 'core', actionCount: 1 }],
      record: () => undefined,
    };
    const capabilityReader = new CapabilityResources(stubCapabilitySource, h.revisions);
    h.store.subscribe(A, CATALOG_SUBSCRIPTION_URI);

    // When the catalog content revision advances and the session's catalog state moves
    h.revisions.set(CATALOG_SUBSCRIPTION_URI, asResourceRevision(5));
    h.catalog.bump(A);
    expect(h.coalescer.syncCatalog(A).recorded).toBe(true);
    h.advance(20);
    expect(h.coalescer.flushDue(h.now())).toBe(1);

    // Then the notification revision equals what a subsequent catalog read returns
    const read = capabilityReader.readCatalog();
    expect(read.revision).toBe(5);
    expect(h.emitted[0].payload).toEqual({ uri: CATALOG_SUBSCRIPTION_URI, revision: 5, changeKind: 'updated' });
    expect(h.emitted[0].payload.revision).toBe(read.revision);
  });

  it('makes syncCatalog idempotent while the catalog state cursor is unchanged', () => {
    // Given a subscribed session whose catalog state advanced once
    const h = setup({ windowMs: 10 });
    h.store.subscribe(A, CATALOG_SUBSCRIPTION_URI);
    h.catalog.bump(A);
    expect(h.coalescer.syncCatalog(A).recorded).toBe(true);
    h.advance(10);
    h.coalescer.flushDue(h.now());

    // When syncCatalog runs again with no further state change
    const repeat = h.coalescer.syncCatalog(A);

    // Then nothing new is recorded
    expect(repeat.recorded).toBe(false);
    h.advance(10);
    expect(h.coalescer.flushDue(h.now())).toBe(0);
  });

  it('rejects a malformed change kind at the boundary', () => {
    // Given a subscribed session
    const h = setup();
    h.store.subscribe(A, SELECTION);

    // When an unknown change kind is recorded
    const result = h.coalescer.recordChange(A, SELECTION, 'exploded');

    // Then it is rejected and nothing is pending
    expect(result).toEqual({ recorded: false, reason: 'INVALID_CHANGE_KIND' });
    expect(h.coalescer.pendingCount(A)).toBe(0);
  });

  it('fans a global change out to every subscribed session, isolating non-subscribers', () => {
    // Given A and B subscribed to selection, and an advanced revision
    const h = setup({ windowMs: 10 });
    h.store.subscribe(A, SELECTION);
    h.store.subscribe(B, SELECTION);
    h.revisions.set(SELECTION, asResourceRevision(4));

    // When a global change is recorded
    expect(h.coalescer.recordGlobalChange(SELECTION, 'updated')).toBe(2);

    // Then each subscribed session receives exactly one notification
    h.advance(10);
    expect(h.coalescer.flushDue(h.now())).toBe(2);
    expect(h.emitted.map((e) => e.sessionId).sort()).toEqual([A, B]);
  });

  it('reports the earliest pending due time for an owner timer, then null once drained', () => {
    // Given one pending change at t=0 with a 50ms window
    const h = setup({ windowMs: 50 });
    h.store.subscribe(A, SELECTION);
    h.coalescer.recordChange(A, SELECTION, 'updated');

    // Then nextDueAt reports the window end, and null after it flushes
    expect(h.coalescer.nextDueAt()).toBe(50);
    h.advance(50);
    h.coalescer.flushDue(h.now());
    expect(h.coalescer.nextDueAt()).toBeNull();
  });
});
