// @ts-check
// tests/unit/adversarial/security-properties-source.mjs
// Task 51 — the src-bound bindings for the runtime security properties.
//
// Same split, same single reason as command-parity-source.mjs: vitest can import
// TypeScript from `src/`, plain node cannot and reaches `dist/` instead. Keeping the
// predicates themselves free of `src/` imports lets both callers exercise ONE
// implementation rather than growing a second one that could agree with itself.

import { sanitizePath } from '../../../src/utils/paths/path-security.js';
import { IdempotencyLedger } from '../../../src/server/gateway/idempotency-ledger.js';

import { exerciseIdempotencyLedgerWith, uePathVerdictWith } from './security-properties.mjs';

/** The UE-path verdict, bound to the real sanitizePath. */
export const uePathVerdict = uePathVerdictWith(sanitizePath);

/**
 * The ledger interleaving, bound to the real IdempotencyLedger.
 * @param {import('./fuzz-random.mjs').Rng} rng
 * @param {{ operations?: number, maxEntries?: number }} [options]
 */
export function exerciseIdempotencyLedger(rng, options = {}) {
  return exerciseIdempotencyLedgerWith(IdempotencyLedger, rng, options);
}
