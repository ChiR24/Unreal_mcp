// Task 41: the execute-path idempotency seam.
//
// Sits between the gateway's authorization stages and dispatch. Placing it AFTER
// scope/consent/validation is what makes the "never cache a refusal" rule
// structural rather than a promise: a refused request returns before it can
// reach `begin`, so no refusal can ever occupy or replay a slot.
//
// Participation is narrow on purpose. Only the `idempotency-key` behaviour class
// dedups, and only when the client actually supplied a key — an `idempotent`
// capability is already safe to repeat and a `non-idempotent` one has no
// meaningful replay semantics, so neither should pay for a ledger entry.
//
// The fingerprint is computed from the POST-normalization params, so a key
// replayed after defaults/aliases resolved differently is correctly a conflict
// rather than a silent replay of a result the client did not actually ask for.

import { createHash } from 'node:crypto';

import { IdempotencyLedger } from './idempotency-ledger.js';

/** Mirrored verbatim by the native surface, so both transports refuse alike. */
export const IDEMPOTENCY_CONFLICT_CODE = 'IDEMPOTENCY_CONFLICT';

/** Stands in when the plugin advertised no profile. Never holds a token. */
export const LOCAL_PRINCIPAL = 'local';

let sharedLedger: IdempotencyLedger | undefined;

/**
 * Process-local ledger for the execute path. Deliberately a module singleton and
 * deliberately never persisted: a restart must forget every key rather than
 * replay a receipt for editor state that may have changed while we were down.
 */
export function sharedExecuteLedger(): IdempotencyLedger {
  sharedLedger ??= new IdempotencyLedger({ clock: () => Date.now() });
  return sharedLedger;
}

/** Test seam: drop all dedup state between cases. */
export function resetSharedExecuteLedger(): void {
  sharedLedger = undefined;
}

export function conflictMessage(reason: ConflictReason): string {
  return reason === 'IN_FLIGHT'
    ? 'This idempotency key is already executing. Wait for the first call to finish and read its receipt; do not retry with the same key.'
    : 'This idempotency key was already used with different parameters. Use a new idempotency key, or resend the original parameters to replay the recorded receipt.';
}

export type IdempotencyClass = 'non-idempotent' | 'idempotent' | 'idempotency-key';

/** Why a duplicate was refused. Distinct values so a client can tell "wait" from "you reused a key". */
export type ConflictReason = 'IN_FLIGHT' | 'FINGERPRINT_MISMATCH';

export interface IdempotentRequest {
  readonly capabilityId: string;
  readonly idempotencyClass: IdempotencyClass;
  /** Non-secret principal identity; the plugin owns the real one. */
  readonly principal: string;
  /** Params AFTER defaults and normalization, so the fingerprint reflects real effect. */
  readonly params: Record<string, unknown>;
  readonly idempotencyKey: string | undefined;
}

type Receipt = Record<string, unknown>;

/**
 * Order-independent digest of the effective request. Object keys are sorted
 * recursively so a client that serializes its params differently still hits the
 * same slot; array order is preserved because it carries meaning.
 */
export function canonicalFingerprint(capabilityId: string, params: Record<string, unknown>): string {
  return createHash('sha256').update(capabilityId).update('\u0000').update(stableStringify(params)).digest('hex');
}

function stableStringify(value: unknown): string {
  if (value === null || typeof value !== 'object') {
    return JSON.stringify(value) ?? 'null';
  }
  if (Array.isArray(value)) {
    return `[${value.map(stableStringify).join(',')}]`;
  }
  const entries = Object.entries(value as Record<string, unknown>)
    .filter(([, v]) => v !== undefined)
    .sort(([a], [b]) => (a < b ? -1 : a > b ? 1 : 0))
    .map(([k, v]) => `${JSON.stringify(k)}:${stableStringify(v)}`);
  return `{${entries.join(',')}}`;
}

/**
 * Run one dispatch under the ledger.
 *
 * `isCacheable` decides what may be replayed later. An error receipt is NOT
 * cacheable, so a failed execution releases its slot and stays retryable — the
 * same guarantee a thrown dispatch gets. Without this, one transient bridge
 * failure would be frozen into a permanent replayed error for 24 hours.
 */
export async function runWithIdempotency(
  request: IdempotentRequest,
  ledger: IdempotencyLedger,
  dispatch: () => Promise<Receipt>,
  isCacheable: (receipt: Receipt) => boolean,
  conflict: (reason: ConflictReason) => Receipt
): Promise<Receipt> {
  const key = request.idempotencyKey;
  if (request.idempotencyClass !== 'idempotency-key' || key === undefined || key.length === 0) {
    return dispatch();
  }

  const fingerprint = canonicalFingerprint(request.capabilityId, request.params);
  const outcome = ledger.begin(
    { principal: request.principal, capabilityId: request.capabilityId, key },
    fingerprint
  );

  if (outcome.kind === 'replay') {
    return outcome.receipt;
  }
  if (outcome.kind === 'in-flight') {
    return conflict('IN_FLIGHT');
  }
  if (outcome.kind === 'conflict') {
    return conflict('FINGERPRINT_MISMATCH');
  }

  const handle = outcome.handle;
  let receipt: Receipt;
  try {
    receipt = await dispatch();
  } catch (error: unknown) {
    ledger.abandon(handle);
    throw error;
  }
  if (isCacheable(receipt)) {
    ledger.complete(handle, receipt);
  } else {
    ledger.abandon(handle);
  }
  return receipt;
}
