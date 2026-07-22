// src/server/mcp-primitives/resource-revision.ts
// Task 31 primitive C2: version-aware read-only resource revisions.
//
// This module is the single TypeScript source of truth for the revision
// primitive shared by the resource surface (Task 31) and, later, the
// subscription surface (Task 34). It carries NO transport wiring and NO editor
// reads; it defines the numeric monotonic revision type, the revisioned payload
// envelope, the allowlisted subscribable URIs, and the injected revision
// provider contract. The native mirror is
// `plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Primitives/McpResourceRevision.h`.

/**
 * A numeric, monotonically non-decreasing resource revision. Branded so a raw
 * `number` cannot be mistaken for a revision at a call boundary.
 */
export type ResourceRevision = number & { readonly __brand: 'ResourceRevision' };

/** The revision every subscribable URI starts at before any change is observed. */
export const INITIAL_REVISION: ResourceRevision = 1 as ResourceRevision;

/**
 * Parse an arbitrary number into a `ResourceRevision`. Revisions are integers
 * >= 1 (0 is reserved as "never observed" on the native side); anything else is
 * a programming error and is rejected rather than silently coerced.
 */
export function asResourceRevision(value: number): ResourceRevision {
  if (!Number.isInteger(value) || value < 1) {
    throw new RangeError(`Invalid resource revision: ${String(value)} (expected integer >= 1)`);
  }
  return value as ResourceRevision;
}

/** The next revision after `current`, preserving the monotonic invariant. */
export function nextRevision(current: ResourceRevision): ResourceRevision {
  return (current + 1) as ResourceRevision;
}

/**
 * A resource payload tagged with the URI it was read from and the revision that
 * produced it. `data` is the bounded, redacted body; callers never see raw
 * editor internals or host paths.
 */
export interface RevisionedResource<T> {
  readonly uri: string;
  readonly revision: ResourceRevision;
  readonly data: T;
}

/**
 * The closed allowlist of URIs whose revisions can be tracked and (in Task 34)
 * subscribed to. Any URI outside this list is not revision-bearing.
 */
export const SUBSCRIBABLE_URIS = [
  'ue://capability/catalog',
  'ue://project',
  'ue://level',
  'ue://selection',
  'ue://asset-registry',
  'ue://pie',
  'ue://build',
  'ue://render',
  'ue://logs',
] as const;

export type SubscribableUri = (typeof SUBSCRIBABLE_URIS)[number];

const SUBSCRIBABLE_URI_SET: ReadonlySet<string> = new Set(SUBSCRIBABLE_URIS);

/** Narrow an arbitrary string to a `SubscribableUri` from the closed allowlist. */
export function isSubscribableUri(uri: string): uri is SubscribableUri {
  return SUBSCRIBABLE_URI_SET.has(uri);
}

/**
 * Injected read-only revision source. Task 31 only reads the current revision
 * for a URI; Task 34 owns advancing revisions and emitting notifications. The
 * resource providers depend on this interface so tests inject deterministic
 * revisions and never touch a live editor clock.
 */
export interface RevisionProvider {
  currentRevision(uri: SubscribableUri): ResourceRevision;
}

/**
 * Default in-memory revision provider. Every subscribable URI reports
 * `INITIAL_REVISION` until something advances it. `set` exists for tests and for
 * the future subscription lane; Task 31 never mutates through it at runtime.
 */
export class InMemoryRevisionProvider implements RevisionProvider {
  private readonly revisions = new Map<SubscribableUri, ResourceRevision>();

  currentRevision(uri: SubscribableUri): ResourceRevision {
    return this.revisions.get(uri) ?? INITIAL_REVISION;
  }

  set(uri: SubscribableUri, revision: ResourceRevision): void {
    this.revisions.set(uri, revision);
  }
}
