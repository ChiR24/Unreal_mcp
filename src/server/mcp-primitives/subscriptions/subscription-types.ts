// src/server/mcp-primitives/subscriptions/subscription-types.ts
// Task 34 primitive: shared contracts for the per-session resource
// subscription store and the bounded, debounced/coalesced notification engine.
//
// This module is pure data and injected-collaborator contracts only. It carries
// NO transport wiring, NO protocol/method registration, and NO editor/SSE reads;
// Task 37 owns all of that. It re-exports the Task 31 C2 revision primitives and
// the Task 36 C1 catalog reader so the store and coalescer consume them without
// editing either. Native mirror:
// Private/MCP/Primitives/McpSubscriptionStore.h + McpNotificationCoalescer.h.

import type { ResourceRevision, SubscribableUri } from '../resource-revision.js';

/**
 * The closed set of change kinds a `resources/updated` notification can carry.
 * Bounded on purpose: the payload never carries a diff or body, only the fact of
 * a change, so a client re-reads the resource for detail.
 */
export const RESOURCE_CHANGE_KINDS = ['updated', 'invalidated', 'removed'] as const;

export type ResourceChangeKind = (typeof RESOURCE_CHANGE_KINDS)[number];

const CHANGE_KIND_SET: ReadonlySet<string> = new Set(RESOURCE_CHANGE_KINDS);

/** Narrow an arbitrary string to a `ResourceChangeKind` at the boundary. */
export function isResourceChangeKind(value: string): value is ResourceChangeKind {
  return CHANGE_KIND_SET.has(value);
}

/**
 * The bounded `resources/updated` payload. It carries ONLY the URI, the revision
 * that produced the change, and the change kind — never a body, diff, host path,
 * or editor internal. Exactly three keys; the contract test pins that shape.
 */
export interface ResourceUpdatedPayload {
  readonly uri: SubscribableUri;
  readonly revision: ResourceRevision;
  readonly changeKind: ResourceChangeKind;
}

/** The canonical subscribable URI whose revision is the capability catalog's. */
export const CATALOG_SUBSCRIPTION_URI = 'ue://capability/catalog' as SubscribableUri;

/**
 * Injected monotonic millisecond clock. Tests and the manual driver pass a fake
 * clock so debounce/coalesce windows are deterministic and never touch wall time.
 */
export type SubscriptionClock = () => number;

/**
 * Pure emission sink for a single coalesced notification. It is NOT a transport
 * write: the coalescer hands the bounded payload to this callback and nothing
 * more. Task 37 supplies a sink that serializes it to SSE/stdio under the
 * session lifecycle; tests supply an array-push spy.
 */
export type NotificationSink = (sessionId: string, payload: ResourceUpdatedPayload) => void;

/**
 * Injected cleanup hook fired whenever a (session, URI) subscription is released
 * — by explicit unsubscribe, deterministic cap eviction, or session clear. Task
 * 37 wires it to drain the coalescer's pending entry and release the matching
 * native editor delegate; tests spy on it to prove maps/delegates drain to zero.
 */
export type SubscriptionReleaseHook = (sessionId: string, uri: SubscribableUri) => void;

/** Default coalescing window: changes to the same session+URI inside it emit once. */
export const DEFAULT_COALESCE_WINDOW_MS = 50;

/**
 * Default per-session subscription cap. Nine equals the full subscribable
 * allowlist, so a well-behaved client never trips it; a smaller cap (injected in
 * tests) exercises deterministic oldest-first eviction.
 */
export const DEFAULT_MAX_SUBSCRIPTIONS_PER_SESSION = 9;
