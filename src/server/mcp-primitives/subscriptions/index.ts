// src/server/mcp-primitives/subscriptions/index.ts
// Task 34 primitive barrel: the pure subscription store + notification engine.
// Task 37 imports from here to wire protocol registration, SSE, and session
// lifecycle; this module adds no behavior of its own.

export {
  CATALOG_SUBSCRIPTION_URI,
  DEFAULT_COALESCE_WINDOW_MS,
  DEFAULT_MAX_SUBSCRIPTIONS_PER_SESSION,
  RESOURCE_CHANGE_KINDS,
  isResourceChangeKind,
  type NotificationSink,
  type ResourceChangeKind,
  type ResourceUpdatedPayload,
  type SubscriptionClock,
  type SubscriptionReleaseHook,
} from './subscription-types.js';

export {
  SubscriptionStore,
  type SubscribeRejectReason,
  type SubscribeResult,
  type SubscriptionStoreOptions,
} from './subscription-store.js';

export {
  NotificationCoalescer,
  type NotificationCoalescerDeps,
  type RecordResult,
  type RecordSkipReason,
} from './notification-coalescer.js';
