// Task 41: bounded, principal-scoped idempotency ledger (TypeScript side).
//
// One in-memory ledger keyed on principal + capability + a DIGEST of the client's
// idempotency key. The raw key never enters the map, so it cannot reach a log
// line, a receipt or an evidence file; only its SHA-256 does. Entries are
// process-local by construction and are never persisted, so a restart correctly
// forgets every key rather than replaying a receipt for state that may have
// changed underneath it.
//
// Two properties are deliberate and load-bearing:
//   * A failure is NEVER cached. `abandon` removes the entry outright, so a
//     transient error is retryable and an authorization/validation/consent
//     refusal (which never reaches `begin` at all) can never be replayed as a
//     successful receipt.
//   * Eviction only ever removes a COMPLETED entry. Evicting an in-flight entry
//     would let a concurrent duplicate through as a second real dispatch, which
//     is precisely the mutation this ledger exists to prevent.
//
// Native mirror: Private/Foundation/McpIdempotencyLedger.{h,cpp} at 4096 entries.

import { createHash, type Hash } from 'node:crypto';

/** 24 hours, per plan line 423. */
export const DEFAULT_IDEMPOTENCY_TTL_MS = 24 * 60 * 60 * 1000;
/** TypeScript-side cap; the native ledger carries 4096. */
export const DEFAULT_MAX_IDEMPOTENCY_ENTRIES = 1024;

/** The identity a key is scoped to. `key` is the client's raw value and is hashed on entry. */
export interface LedgerScope {
  readonly principal: string;
  readonly capabilityId: string;
  readonly key: string;
}

/** Opaque claim on one in-flight slot, handed back to `complete` / `abandon`. */
export interface LedgerHandle {
  readonly slot: string;
}

export type BeginOutcome =
  | { readonly kind: 'first'; readonly handle: LedgerHandle }
  | { readonly kind: 'in-flight' }
  | { readonly kind: 'replay'; readonly receipt: Record<string, unknown> }
  | { readonly kind: 'conflict' };

export interface IdempotencyLedgerOptions {
  /** Injected so TTL is testable without sleeping. */
  readonly clock: () => number;
  readonly maxEntries?: number;
  readonly ttlMs?: number;
}

interface LedgerEntry {
  readonly principal: string;
  readonly fingerprint: string;
  state: 'in-flight' | 'completed';
  receipt: Record<string, unknown> | null;
  completedAt: number;
}

// Netstring-style field encoding: <utf8ByteLength> ':' <utf8Bytes>. Gluing the
// fields with a single delimiter is NOT injective - a delimiter occurring inside
// a field is indistinguishable from a field boundary, so a crafted principal or
// capability id can shift bytes across a boundary and land two distinct scopes on
// one slot. Prefixing each field with its own byte length removes the ambiguity:
// the reader never has to guess where a field ends.
//
// The native mirror (Private/Foundation/McpIdempotencyLedger.cpp) builds the
// identical preimage, so both surfaces digest the same bytes for the same inputs;
// the vectors in the colocated test are pinned on both sides.
function appendLengthPrefixedField(hash: Hash, field: string): void {
  const bytes = Buffer.from(field, 'utf8');
  hash.update(`${bytes.length}:`, 'utf8');
  hash.update(bytes);
}

export class IdempotencyLedger {
  readonly maxEntries: number;
  readonly ttlMs: number;

  /** Insertion-ordered, which is what makes oldest-first eviction deterministic. */
  private readonly entries = new Map<string, LedgerEntry>();
  private readonly clock: () => number;

  constructor(options: IdempotencyLedgerOptions) {
    const cap = options.maxEntries ?? DEFAULT_MAX_IDEMPOTENCY_ENTRIES;
    if (!Number.isInteger(cap) || cap < 1) {
      throw new RangeError(`Invalid maxEntries: ${String(cap)} (expected integer >= 1)`);
    }
    const ttl = options.ttlMs ?? DEFAULT_IDEMPOTENCY_TTL_MS;
    if (!Number.isInteger(ttl) || ttl < 1) {
      throw new RangeError(`Invalid ttlMs: ${String(ttl)} (expected integer >= 1)`);
    }
    this.maxEntries = cap;
    this.ttlMs = ttl;
    this.clock = options.clock;
  }

  /**
   * Claim a slot for one execution. `fingerprint` is the caller's canonical
   * POST-normalization digest of the request, so a key replayed with different
   * effective params is a conflict rather than a silent replay of the old result.
   */
  begin(scope: LedgerScope, fingerprint: string): BeginOutcome {
    const slot = IdempotencyLedger.computeSlot(scope);
    const existing = this.entries.get(slot);

    if (existing !== undefined && this.isExpired(existing)) {
      this.entries.delete(slot);
      return this.admit(slot, scope.principal, fingerprint);
    }
    if (existing === undefined) {
      return this.admit(slot, scope.principal, fingerprint);
    }
    if (existing.fingerprint !== fingerprint) {
      // No prior receipt is returned: a caller that guessed another caller's key
      // must not learn what that call produced.
      return { kind: 'conflict' };
    }
    if (existing.state === 'in-flight') {
      return { kind: 'in-flight' };
    }
    return { kind: 'replay', receipt: existing.receipt ?? {} };
  }

  /** Record the receipt for a finished execution and enforce the cap. */
  complete(handle: LedgerHandle, receipt: Record<string, unknown>): void {
    const entry = this.entries.get(handle.slot);
    if (entry === undefined || entry.state === 'completed') {
      return;
    }
    entry.state = 'completed';
    entry.receipt = receipt;
    entry.completedAt = this.clock();
    this.evictCompletedOverCap();
  }

  /**
   * Release a slot without recording anything. Called when the execution failed,
   * so the key stays retryable and no failure is ever cached.
   */
  abandon(handle: LedgerHandle): void {
    this.entries.delete(handle.slot);
  }

  size(): number {
    return this.entries.size;
  }

  /** Drop every entry for one principal (session teardown / principal rebind). */
  clearPrincipal(principal: string): number {
    let removed = 0;
    for (const [slot, entry] of [...this.entries]) {
      if (entry.principal === principal) {
        this.entries.delete(slot);
        removed++;
      }
    }
    return removed;
  }

  /**
   * Diagnostic snapshot. Deliberately contains slot DIGESTS only — asserted by
   * test F to never carry a raw idempotency key.
   */
  debugState(): string {
    const rows = [...this.entries].map(([slot, entry]) => ({
      slot,
      principal: entry.principal,
      state: entry.state,
      completedAt: entry.completedAt,
    }));
    return JSON.stringify(rows);
  }

  private admit(slot: string, principal: string, fingerprint: string): BeginOutcome {
    this.entries.set(slot, {
      principal,
      fingerprint,
      state: 'in-flight',
      receipt: null,
      completedAt: 0,
    });
    return { kind: 'first', handle: { slot } };
  }

  private isExpired(entry: LedgerEntry): boolean {
    if (entry.state !== 'completed') {
      return false;
    }
    return this.clock() - entry.completedAt > this.ttlMs;
  }

  /**
   * Evict oldest-completed-first until the completed population is within the
   * cap. In-flight entries are never evicted, so a running execution keeps its
   * claim and a concurrent duplicate still sees `in-flight`.
   */
  private evictCompletedOverCap(): void {
    let completed = 0;
    for (const entry of this.entries.values()) {
      if (entry.state === 'completed') {
        completed++;
      }
    }
    for (const [slot, entry] of this.entries) {
      if (completed <= this.maxEntries) {
        return;
      }
      if (entry.state === 'completed') {
        this.entries.delete(slot);
        completed--;
      }
    }
  }

  /**
   * Scope digest over the length-prefixed preimage. The principal and capability
   * bound the key so one principal can never replay, collide with, or observe
   * another's execution, and the encoding is injective so no crafted field can
   * shift a boundary and merge two scopes onto one slot. The raw key is hashed,
   * never stored, so it cannot reach a log line, a receipt or an evidence file.
   * Public so both mirrors can pin the same digest vectors.
   */
  static computeSlot(scope: LedgerScope): string {
    const hash = createHash('sha256');
    appendLengthPrefixedField(hash, scope.principal);
    appendLengthPrefixedField(hash, scope.capabilityId);
    appendLengthPrefixedField(hash, scope.key);
    return hash.digest('hex');
  }
}
