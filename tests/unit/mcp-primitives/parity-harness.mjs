// @ts-check
// tests/unit/mcp-primitives/parity-harness.mjs
// Task 38 lane E — the PUBLIC FACADE of the strict, framing-neutral normalized
// parity harness. It is the single entry point both the Vitest self-tests and
// the plain-node QA driver import. It adds the framing-neutral comparator and
// the two orchestration entry points (checkFixture, compareCaptures) on top of
// the schema/validators/drift/native-requirement modules, and re-exports them so
// a consumer needs exactly one import.
//
// The comparator is deliberately structural and transport-blind: it diffs two
// already-normalized values by JSON pointer. It never sees stdio/HTTP/SSE framing
// or source text, so a reported mismatch is always a real behavioral divergence,
// and a single injected field surfaces as exactly one pointer.

// `export *` only from the canonical type sources (their Norm*/Reason typedefs are
// disjoint; native-capture adds only new runtime names). Drift and native-requirement
// are re-exported by VALUE name so their internal typedef aliases don't collide.
export * from './parity-harness-schema.mjs';
export * from './parity-harness-validators.mjs';
export * from './parity-harness-native-capture.mjs';
export {
  clone,
  driftResultField,
  wrongErrorCode,
  staleRevision,
  falseCapability,
  unboundFallback,
  omitCleanup,
  crossSessionRecord,
  toSourceText,
  broadenMatch,
  addUnknownField,
  injectSchemaDump,
} from './parity-harness-drift.mjs';
export {
  NATIVE_CAPTURE_REQUIREMENT,
  isGenuineNativeCapture,
  assertParityReady,
} from './parity-harness-native-requirement.mjs';

import { isPlainObject } from './parity-harness-schema.mjs';
import { validateCapture } from './parity-harness-validators.mjs';
import { assertParityReady } from './parity-harness-native-requirement.mjs';

/** @typedef {import('./parity-harness-validators.mjs').Capture} Capture */
/** @typedef {{ pointer: string, left: unknown, right: unknown }} Mismatch */

/**
 * Structural, framing-neutral diff producing JSON-pointer mismatches. Object keys
 * are visited in sorted order so the output is deterministic regardless of input
 * key order. Identical values yield `[]`; a single differing leaf yields exactly
 * one mismatch at its pointer.
 * @param {unknown} left @param {unknown} right @param {string} [pointer] @returns {Mismatch[]}
 */
export function diff(left, right, pointer = '') {
  if (JSON.stringify(left) === JSON.stringify(right)) return [];
  if (Array.isArray(left) && Array.isArray(right)) {
    /** @type {Mismatch[]} */
    const out = [];
    if (left.length !== right.length) out.push({ pointer: `${pointer}/length`, left: left.length, right: right.length });
    const max = Math.max(left.length, right.length);
    for (let i = 0; i < max; i += 1) out.push(...diff(left[i], right[i], `${pointer}/${i}`));
    return out;
  }
  if (isPlainObject(left) && isPlainObject(right)) {
    /** @type {Mismatch[]} */
    const out = [];
    const keys = new Set([...Object.keys(left), ...Object.keys(right)]);
    for (const key of [...keys].sort()) out.push(...diff(left[key], right[key], `${pointer}/${key}`));
    return out;
  }
  return [{ pointer: pointer === '' ? '/' : pointer, left, right }];
}

/**
 * Ingest one raw fixture object into a validated, closed Capture (or throw a
 * HarnessRejection). This is the boundary: after checkFixture, callers hold a
 * normalized value they never re-validate.
 * @param {unknown} rawFixture @returns {Capture}
 */
export function checkFixture(rawFixture) {
  return validateCapture(rawFixture, '/');
}

/**
 * @typedef {{ ready: true, mismatches: Mismatch[], drift: boolean }} ParityResult
 * @typedef {import('./parity-harness-native-requirement.mjs').ParityBlocked} ParityBlocked
 */

/**
 * Compare a TS capture against a native capture on normalized semantics. Gated by
 * assertParityReady: if the native side is absent, modelled, or a source snapshot,
 * this returns the RED blocker and NEVER a mismatch verdict — a blocked parity is
 * not a passing parity. Only when both are genuine executed captures does it run
 * the framing-neutral diff.
 * @param {Capture} tsCapture @param {Capture|null|undefined} nativeCapture
 * @returns {ParityResult|ParityBlocked}
 */
export function compareCaptures(tsCapture, nativeCapture) {
  const gate = assertParityReady(tsCapture, nativeCapture);
  if (!gate.ready) return gate;
  const native = /** @type {Capture} */ (nativeCapture);
  if (tsCapture.domain !== native.domain) {
    return { ready: true, mismatches: [{ pointer: '/domain', left: tsCapture.domain, right: native.domain }], drift: true };
  }
  const mismatches = diff(tsCapture.value, native.value);
  return { ready: true, mismatches, drift: mismatches.length > 0 };
}

/**
 * Stable id-ordered copy of a capture list, so iteration/reporting is
 * deterministic regardless of the order fixtures were discovered on disk.
 * @param {readonly Capture[]} captures @returns {Capture[]}
 */
export function stableSortById(captures) {
  return [...captures].sort((a, b) => a.id.localeCompare(b.id));
}
