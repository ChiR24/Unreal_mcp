// src/server/mcp-primitives/primitive-notifications.ts
// Task 37: the notification driver that gives the pure Task 34 subscription
// store + coalescer their real timer, transport sink, and session lifecycle. It
// owns the ONE shared SubscriptionStore + NotificationCoalescer for a server, a
// single unref'd flush timer driven by nextDueAt/flushDue, and the URI-only
// resources/updated emission. A configure visibility change folds into at most
// one notification per debounce window; a session teardown drains its state so
// no post-close delivery ever happens.

import { NotificationCoalescer } from './subscriptions/notification-coalescer.js';
import { SubscriptionStore } from './subscriptions/subscription-store.js';
import type { CatalogRevisionReader } from './catalog-revision-reader.js';
import type { RevisionProvider } from './resource-revision.js';
import type { ResourceUpdatedPayload } from './subscriptions/subscription-types.js';

// The minimal server surface the driver needs: emit one JSON-RPC notification.
export interface NotifyingServer {
  notification(notification: { method: string; params?: Record<string, unknown> }): Promise<void>;
}

export interface PrimitiveNotificationDeps {
  readonly server: NotifyingServer;
  readonly revisions: RevisionProvider;
  readonly catalog: CatalogRevisionReader;
  readonly clock?: () => number;
}

export class PrimitiveNotificationDriver {
  /** The shared per-session subscription state the primitive handlers mutate. */
  readonly store: SubscriptionStore;

  private readonly server: NotifyingServer;
  private readonly coalescer: NotificationCoalescer;
  private readonly clock: () => number;
  private flushTimer: ReturnType<typeof setTimeout> | undefined;
  private disposed = false;

  constructor(deps: PrimitiveNotificationDeps) {
    this.server = deps.server;
    this.clock = deps.clock ?? ((): number => Date.now());
    // onRelease fires whenever a (session, URI) is released (unsubscribe, cap
    // eviction, or clear); it drops the matching coalescer pending so a released
    // subscription can never flush a late update. `this.coalescer` is assigned
    // just below and only read when the hook fires at runtime.
    this.store = new SubscriptionStore({
      onRelease: (sessionId, uri) => {
        this.coalescer.dropPending(sessionId, uri);
      },
    });
    this.coalescer = new NotificationCoalescer({
      store: this.store,
      revisions: deps.revisions,
      catalog: deps.catalog,
      sink: (sessionId, payload) => {
        this.deliver(sessionId, payload);
      },
      clock: this.clock,
    });
  }

  /**
   * Fold a session's catalog visibility change into (at most) one coalesced
   * `resources/updated` and arm the single flush timer. Idempotent when the
   * per-session catalog revision has not effectively advanced.
   */
  syncCatalog(sessionId: string): void {
    if (this.disposed) {
      return;
    }
    this.coalescer.syncCatalog(sessionId);
    this.arm();
  }

  /**
   * Drain one session on disconnect/teardown: release its subscriptions (which
   * drops matching pending via onRelease) and clear its coalescer cursor, then
   * re-evaluate the timer so an emptied schedule cancels it. Idempotent.
   */
  releaseSession(sessionId: string): void {
    this.store.clearSession(sessionId);
    this.coalescer.clearSession(sessionId);
    this.clearTimer();
    this.arm();
  }

  /** Tear the driver down entirely: no further delivery, no lingering timer. */
  dispose(): void {
    this.disposed = true;
    this.clearTimer();
  }

  private deliver(_sessionId: string, payload: ResourceUpdatedPayload): void {
    if (this.disposed) {
      return;
    }
    // The MCP resources/updated wire params are URI-only (the client re-reads the
    // resource for the new revision); the coalescer's revision/changeKind never
    // cross the wire. The session is implicit on the single stdio transport.
    void this.server
      .notification({
        method: 'notifications/resources/updated',
        params: { uri: payload.uri },
      })
      .catch(() => {
        // The transport closed between flush and write; the session is gone.
      });
  }

  private arm(): void {
    if (this.disposed || this.flushTimer !== undefined) {
      return;
    }
    const dueAt = this.coalescer.nextDueAt();
    if (dueAt === null) {
      return;
    }
    const delay = Math.max(0, dueAt - this.clock());
    this.flushTimer = setTimeout(() => {
      this.flushTimer = undefined;
      this.flush();
    }, delay);
    this.flushTimer.unref?.();
  }

  private flush(): void {
    if (this.disposed) {
      return;
    }
    this.coalescer.flushDue(this.clock());
    this.arm();
  }

  private clearTimer(): void {
    if (this.flushTimer !== undefined) {
      clearTimeout(this.flushTimer);
      this.flushTimer = undefined;
    }
  }
}
