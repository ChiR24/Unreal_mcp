// src/server/mcp-primitives/subscriptions/notification-coalescer.ts
// Task 34 primitive: bounded, debounced/coalesced resource-notification engine.
//
// A PURE state machine over the subscription store. It records change signals per
// session+URI, folds a burst into a single pending entry inside a fixed
// coalescing window (injected fake clock in tests), and on flush emits ONE bounded
// `resources/updated` payload carrying only URI/revision/changeKind. It NEVER
// writes a transport: the injected `sink` receives the payload and Task 37 owns
// SSE/stdio serialization, real timers, and lifecycle. Revisions are stamped from
// the Task 31 C2 RevisionProvider (so a notification matches a later resource
// read) and the catalog change cursor is driven by the Task 36 C1 reader. Native
// mirror: Private/MCP/Primitives/McpNotificationCoalescer.{h,cpp}.

import { BASELINE_CATALOG_STATE_REVISION, type CatalogRevisionReader } from '../catalog-revision-reader.js';
import type { ResourceRevision, RevisionProvider, SubscribableUri } from '../resource-revision.js';
import type { SubscriptionStore } from './subscription-store.js';
import {
  CATALOG_SUBSCRIPTION_URI,
  DEFAULT_COALESCE_WINDOW_MS,
  isResourceChangeKind,
  type NotificationSink,
  type ResourceChangeKind,
  type SubscriptionClock,
} from './subscription-types.js';

/** Why a change signal did not produce (or coalesce into) a pending entry. */
// UNCHANGED is distinct from NOT_SUBSCRIBED: the catalog cursor simply has not
// moved, which is the normal no-op path. Reporting that as NOT_SUBSCRIBED made a
// quiet catalog indistinguishable from a session that could not receive it.
export type RecordSkipReason = 'NOT_SUBSCRIBED' | 'INVALID_CHANGE_KIND' | 'UNCHANGED';

export interface RecordResult {
  readonly recorded: boolean;
  readonly reason: RecordSkipReason | null;
}

export interface NotificationCoalescerDeps {
  readonly store: SubscriptionStore;
  /** Task 31 C2: stamps the payload revision a subsequent resource read returns. */
  readonly revisions: RevisionProvider;
  /** Task 36 C1: per-session catalog state cursor driving `syncCatalog`. */
  readonly catalog: CatalogRevisionReader;
  readonly sink: NotificationSink;
  readonly clock: SubscriptionClock;
  /** Coalescing window in ms; a burst inside it emits once (default 50). */
  readonly windowMs?: number;
}

interface PendingChange {
  readonly sessionId: string;
  readonly uri: SubscribableUri;
  changeKind: ResourceChangeKind;
  readonly dueAt: number;
}

const KEY_SEPARATOR = '\u0000';

export class NotificationCoalescer {
  private readonly store: SubscriptionStore;
  private readonly revisions: RevisionProvider;
  private readonly catalog: CatalogRevisionReader;
  private readonly sink: NotificationSink;
  private readonly clock: SubscriptionClock;
  private readonly windowMs: number;

  private readonly pending = new Map<string, PendingChange>();
  private readonly lastEmitted = new Map<string, ResourceRevision>();
  private readonly catalogCursor = new Map<string, number>();

  constructor(deps: NotificationCoalescerDeps) {
    this.store = deps.store;
    this.revisions = deps.revisions;
    this.catalog = deps.catalog;
    this.sink = deps.sink;
    this.clock = deps.clock;
    this.windowMs = deps.windowMs ?? DEFAULT_COALESCE_WINDOW_MS;
  }

  /**
   * Record a change signal. Dropped (nothing emitted, ever) when the session is
   * not subscribed to the URI — so an unsubscribed or disconnected session, or a
   * URI outside the allowlist, gets nothing. A burst to the same session+URI
   * inside the window folds into one pending entry (latest change kind wins) and
   * keeps the original due time, so the window does not slide indefinitely.
   */
  recordChange(sessionId: string, uri: string, changeKind: string = 'updated'): RecordResult {
    if (!isResourceChangeKind(changeKind)) {
      return { recorded: false, reason: 'INVALID_CHANGE_KIND' };
    }
    if (!this.store.isSubscribed(sessionId, uri)) {
      return { recorded: false, reason: 'NOT_SUBSCRIBED' };
    }
    const subscribableUri = uri as SubscribableUri;
    const key = NotificationCoalescer.key(sessionId, subscribableUri);
    const existing = this.pending.get(key);
    if (existing !== undefined) {
      existing.changeKind = changeKind;
    } else {
      this.pending.set(key, {
        sessionId,
        uri: subscribableUri,
        changeKind,
        dueAt: this.clock() + this.windowMs,
      });
    }
    return { recorded: true, reason: null };
  }

  /**
   * The one real producer available in Task 34: when a session's Task 36 catalog
   * state revision advances, enqueue a coalesced catalog notification. Non-catalog
   * producers are wired in Task 42. Idempotent while the cursor is unchanged.
   */
  syncCatalog(sessionId: string): RecordResult {
    const current = this.catalog.getCatalogStateRevision(sessionId);
    const last = this.catalogCursor.get(sessionId) ?? BASELINE_CATALOG_STATE_REVISION;
    if (current <= last) {
      // The cursor has not moved — a different condition from "not subscribed",
      // and reporting it as such made an unchanged catalog indistinguishable
      // from a dropped notification.
      return { recorded: false, reason: 'UNCHANGED' };
    }
    // Record FIRST, advance the cursor only if the change was actually queued.
    // Advancing first consumed the edge: a session not yet subscribed had its
    // cursor moved to `current` and the change dropped, so when it later
    // subscribed no further sync could re-report that transition and it kept a
    // stale view until some unrelated change happened to move the revision again.
    const result = this.recordChange(sessionId, CATALOG_SUBSCRIPTION_URI, 'updated');
    if (result.recorded) {
      this.catalogCursor.set(sessionId, current);
    }
    return result;
  }

  /** Fan a global content change out to every subscribed session (Task 42 helper). */
  recordGlobalChange(uri: string, changeKind: string = 'updated'): number {
    let recorded = 0;
    for (const sessionId of this.store.sessionsSubscribedTo(uri)) {
      if (this.recordChange(sessionId, uri, changeKind).recorded) {
        recorded++;
      }
    }
    return recorded;
  }

  /**
   * Emit every pending change whose window has elapsed by `now`. Each emission
   * re-checks the live subscription (so a timer that fires after unsubscribe/clear
   * is suppressed) and enforces a monotonic revision (a stale, lower revision is
   * dropped). Returns the number of notifications emitted.
   */
  flushDue(now: number = this.clock()): number {
    let emitted = 0;
    for (const [key, change] of this.pending) {
      if (change.dueAt > now) {
        continue;
      }
      this.pending.delete(key);
      if (!this.store.isSubscribed(change.sessionId, change.uri)) {
        continue;
      }
      const revision = this.revisions.currentRevision(change.uri);
      const previous = this.lastEmitted.get(key);
      if (previous !== undefined && revision < previous) {
        continue;
      }
      this.lastEmitted.set(key, revision);
      this.sink(change.sessionId, { uri: change.uri, revision, changeKind: change.changeKind });
      emitted++;
    }
    return emitted;
  }

  /** Earliest pending due time, so an owner can arm a single real timer (Task 37). */
  nextDueAt(): number | null {
    let earliest: number | null = null;
    for (const change of this.pending.values()) {
      if (earliest === null || change.dueAt < earliest) {
        earliest = change.dueAt;
      }
    }
    return earliest;
  }

  pendingCount(sessionId?: string): number {
    if (sessionId === undefined) {
      return this.pending.size;
    }
    let count = 0;
    for (const change of this.pending.values()) {
      if (change.sessionId === sessionId) {
        count++;
      }
    }
    return count;
  }

  /** Release hook target: drop one (session, URI)'s pending + revision memory. */
  dropPending(sessionId: string, uri: SubscribableUri): void {
    const key = NotificationCoalescer.key(sessionId, uri);
    this.pending.delete(key);
    this.lastEmitted.delete(key);
  }

  /** Drain all coalescer state for a session (companion to store.clearSession). */
  clearSession(sessionId: string): void {
    const prefix = `${sessionId}${KEY_SEPARATOR}`;
    for (const key of [...this.pending.keys()]) {
      if (key.startsWith(prefix)) {
        this.pending.delete(key);
      }
    }
    for (const key of [...this.lastEmitted.keys()]) {
      if (key.startsWith(prefix)) {
        this.lastEmitted.delete(key);
      }
    }
    this.catalogCursor.delete(sessionId);
  }

  private static key(sessionId: string, uri: SubscribableUri): string {
    return `${sessionId}${KEY_SEPARATOR}${uri}`;
  }
}
