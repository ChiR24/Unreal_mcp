// src/server/mcp-primitives/subscriptions/subscription-store.ts
// Task 34 primitive: revisioned per-session resource subscription store.
//
// Pure subscribe/unsubscribe/clear state keyed by an EXPLICIT session id (Task 37
// supplies the real transport/native ids). Each session owns an INDEPENDENT,
// insertion-ordered set of subscribed URIs drawn from the Task 31 C2 allowlist;
// one session's subscriptions can never be read or drained as another's. The
// store holds NO timers, NO transport, and NO revisions — the notification
// engine (notification-coalescer.ts) layers those on top. Native mirror:
// Private/MCP/Primitives/McpSubscriptionStore.{h,cpp}.

import { isSubscribableUri, type SubscribableUri } from '../resource-revision.js';
import {
  DEFAULT_MAX_SUBSCRIPTIONS_PER_SESSION,
  type SubscriptionReleaseHook,
} from './subscription-types.js';
import { evictOldestUntilUnder } from '../../../utils/collections/bounded.js';

/** Why a subscribe request was rejected without mutating any session state. */
export type SubscribeRejectReason = 'NOT_SUBSCRIBABLE' | 'INVALID_SESSION';

/**
 * Deterministic outcome of a subscribe request. `evicted` names the oldest URI
 * dropped to make room under the per-session cap (null when nothing was evicted);
 * `alreadySubscribed` marks the idempotent duplicate case.
 */
export interface SubscribeResult {
  readonly accepted: boolean;
  readonly alreadySubscribed: boolean;
  readonly evicted: SubscribableUri | null;
  readonly reason: SubscribeRejectReason | null;
}

export interface SubscriptionStoreOptions {
  /** Per-session cap; exceeding it evicts the oldest subscription (default 9). */
  readonly maxPerSession?: number;
  /** Fired for every (session, URI) released by unsubscribe, eviction, or clear. */
  readonly onRelease?: SubscriptionReleaseHook;
}

export class SubscriptionStore {
  private readonly sessions = new Map<string, Set<SubscribableUri>>();
  private readonly maxPerSession: number;
  private readonly onRelease: SubscriptionReleaseHook | null;

  constructor(options: SubscriptionStoreOptions = {}) {
    const cap = options.maxPerSession ?? DEFAULT_MAX_SUBSCRIPTIONS_PER_SESSION;
    if (!Number.isInteger(cap) || cap < 1) {
      throw new RangeError(`Invalid maxPerSession: ${String(cap)} (expected integer >= 1)`);
    }
    this.maxPerSession = cap;
    this.onRelease = options.onRelease ?? null;
  }

  /**
   * Subscribe a session to a URI. Rejects a blank session or a URI outside the
   * Task 31 allowlist without side effects; a duplicate is idempotent; at the cap
   * the oldest subscription is deterministically evicted (and released) first.
   */
  subscribe(sessionId: string, uri: string): SubscribeResult {
    if (!SubscriptionStore.isValidSession(sessionId)) {
      return { accepted: false, alreadySubscribed: false, evicted: null, reason: 'INVALID_SESSION' };
    }
    if (!isSubscribableUri(uri)) {
      return { accepted: false, alreadySubscribed: false, evicted: null, reason: 'NOT_SUBSCRIBABLE' };
    }
    const set = this.ensureSession(sessionId);
    if (set.has(uri)) {
      return { accepted: true, alreadySubscribed: true, evicted: null, reason: null };
    }
    let evicted: SubscribableUri | null = null;
    evictOldestUntilUnder(set, this.maxPerSession, (uriEvicted) => {
      evicted = uriEvicted;
      this.release(sessionId, uriEvicted);
    });
    set.add(uri);
    return { accepted: true, alreadySubscribed: false, evicted, reason: null };
  }

  /** Remove one subscription. Returns false for an unknown session or URI. */
  unsubscribe(sessionId: string, uri: string): boolean {
    const set = this.sessions.get(sessionId);
    if (set === undefined || !isSubscribableUri(uri) || !set.has(uri)) {
      return false;
    }
    set.delete(uri);
    this.release(sessionId, uri);
    if (set.size === 0) {
      this.sessions.delete(sessionId);
    }
    return true;
  }

  isSubscribed(sessionId: string, uri: string): boolean {
    return isSubscribableUri(uri) && (this.sessions.get(sessionId)?.has(uri) ?? false);
  }

  /** Snapshot of a session's subscriptions in oldest-first insertion order. */
  subscriptions(sessionId: string): readonly SubscribableUri[] {
    const set = this.sessions.get(sessionId);
    return set === undefined ? [] : [...set];
  }

  count(sessionId: string): number {
    return this.sessions.get(sessionId)?.size ?? 0;
  }

  hasSession(sessionId: string): boolean {
    return this.sessions.has(sessionId);
  }

  sessionCount(): number {
    return this.sessions.size;
  }

  /** Every session currently subscribed to `uri` (for producer fan-out). */
  sessionsSubscribedTo(uri: string): string[] {
    if (!isSubscribableUri(uri)) {
      return [];
    }
    const result: string[] = [];
    for (const [sessionId, set] of this.sessions) {
      if (set.has(uri)) {
        result.push(sessionId);
      }
    }
    return result;
  }

  /**
   * Drop a whole session, releasing every subscription (fires `onRelease` per URI
   * so delegates/pending drain to zero). Returns how many were released.
   */
  clearSession(sessionId: string): number {
    const set = this.sessions.get(sessionId);
    if (set === undefined) {
      return 0;
    }
    let released = 0;
    for (const uri of set) {
      this.release(sessionId, uri);
      released++;
    }
    this.sessions.delete(sessionId);
    return released;
  }

  private ensureSession(sessionId: string): Set<SubscribableUri> {
    let set = this.sessions.get(sessionId);
    if (set === undefined) {
      set = new Set<SubscribableUri>();
      this.sessions.set(sessionId, set);
    }
    return set;
  }

  private release(sessionId: string, uri: SubscribableUri): void {
    this.onRelease?.(sessionId, uri);
  }

  private static isValidSession(sessionId: string): boolean {
    return typeof sessionId === 'string' && sessionId.trim().length > 0;
  }
}
