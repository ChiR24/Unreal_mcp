// @ts-check
// tests/unit/mcp-primitives/parity-harness-schema.mjs
// Task 38 lane E — the strict, framing-neutral SCHEMA PRIMITIVES for the parity
// harness. This module owns the rejection taxonomy and the low-level guards that
// every domain validator is built from. It is deliberately transport-agnostic:
// it never sees stdio/HTTP/SSE framing or source text — only already-decoded
// normalized values. A fixture that reaches these guards is accepted ONLY when
// it is an exact, closed, bounded, executable capture; anything looser is thrown
// out with a precise, machine-checkable reason so the harness can never be
// coaxed into a source-only or broad-expectation "pass".
//
// It is plain ESM (.mjs) on purpose: it is imported unchanged by both the Vitest
// self-tests (*.test.ts) and the plain-node QA driver (scripts/qa/...), so the
// exact same reject/accept logic gates every surface. JSDoc typedefs below ARE
// the strict normalized schema; `@ts-check` enforces them in-editor and under
// `tsc --checkJs`.

/**
 * Closed rejection taxonomy. Every refusal the harness can emit is one of these
 * — no free-form failures — so callers (and the QA probe) assert on `.reason`.
 * @typedef {'SOURCE_TEXT_CAPTURE'|'BROAD_EXPECTATION'|'UNKNOWN_FIELD'|'SCHEMA_DUMP'|'STALE_REVISION'|'CROSS_SESSION_ID'|'MISSING_CLEANUP'|'UNBOUNDED_FALLBACK'|'NATIVE_CAPTURE_ABSENT'|'VACUOUS_CELL'|'MALFORMED'} RejectionReason
 */

/** @type {Readonly<Record<RejectionReason, RejectionReason>>} */
export const REASONS = Object.freeze({
  SOURCE_TEXT_CAPTURE: 'SOURCE_TEXT_CAPTURE',
  BROAD_EXPECTATION: 'BROAD_EXPECTATION',
  UNKNOWN_FIELD: 'UNKNOWN_FIELD',
  SCHEMA_DUMP: 'SCHEMA_DUMP',
  STALE_REVISION: 'STALE_REVISION',
  CROSS_SESSION_ID: 'CROSS_SESSION_ID',
  MISSING_CLEANUP: 'MISSING_CLEANUP',
  UNBOUNDED_FALLBACK: 'UNBOUNDED_FALLBACK',
  NATIVE_CAPTURE_ABSENT: 'NATIVE_CAPTURE_ABSENT',
  // Task 46. A matrix cell that asserts NOTHING compares equal to every other
  // empty cell, so an over-aggressive framing stripper would turn the whole
  // cross-transport matrix green by deleting the evidence. An empty fact set is
  // therefore a refusal, not a pass.
  VACUOUS_CELL: 'VACUOUS_CELL',
  MALFORMED: 'MALFORMED',
});

/** The only capture kinds that may participate in a parity comparison. */
export const EXECUTABLE_KINDS = Object.freeze(['executable-ts', 'native-protocol']);
/** Every capture kind the schema recognizes (source-text is recognized only to reject it). */
export const CAPTURE_KINDS = Object.freeze([...EXECUTABLE_KINDS, 'native-model', 'source-text']);
/** The normalized domains a capture can carry. `matrix` is the Task 46
 * cross-transport cell: one (dimension, scenario) judged on semantics only. */
export const DOMAINS = Object.freeze(['result', 'error', 'revision', 'profile', 'session', 'pointer', 'matrix']);

/**
 * The 15 runtime/lifecycle dimensions Task 46 gates across both transports.
 * Frozen and closed: a cell naming a dimension outside this list is MALFORMED,
 * so the matrix cannot be quietly narrowed by dropping a dimension that fails.
 */
export const MATRIX_DIMENSIONS = Object.freeze([
  'receipt', 'error', 'scope', 'consent', 'idempotency', 'revision', 'preview',
  'progress', 'task', 'cancellation', 'cache', 'queue', 'reconnect', 'timeout',
  'shutdown',
]);

/** The only outcomes a semantic cell may report. A cell is a refusal or it is
 * not; there is no third "partial" state that both transports could satisfy
 * with different behavior. */
export const MATRIX_OUTCOMES = Object.freeze(['success', 'refusal']);

/** The literal code a SUCCESS cell carries. It is still UPPER_SNAKE so the same
 * typed-code guard runs on every cell, and it can never collide with a real
 * refusal code. */
export const NO_CODE = 'NONE';

/** A precise, typed refusal. Carries the closed `reason`, a JSON pointer, and a human detail. */
export class HarnessRejection extends Error {
  /**
   * @param {RejectionReason} reason
   * @param {string} pointer
   * @param {string} detail
   */
  constructor(reason, pointer, detail) {
    super(`${reason} at ${pointer || '/'}: ${detail}`);
    this.name = 'HarnessRejection';
    /** @type {RejectionReason} */
    this.reason = reason;
    this.pointer = pointer || '/';
    this.detail = detail;
  }
}

/**
 * @param {RejectionReason} reason
 * @param {string} pointer
 * @param {string} detail
 * @returns {never}
 */
export function reject(reason, pointer, detail) {
  throw new HarnessRejection(reason, pointer, detail);
}

/** @param {unknown} v @returns {v is Record<string, unknown>} */
export function isPlainObject(v) {
  return typeof v === 'object' && v !== null && !Array.isArray(v);
}

/**
 * Closed-object guard: the value must be a plain object whose keys are a subset
 * of `allowed`. An extra key is an UNKNOWN_FIELD — this is what refuses fixtures
 * that smuggle undeclared data past a normalized shape.
 * @param {unknown} obj
 * @param {readonly string[]} allowed
 * @param {string} pointer
 * @returns {Record<string, unknown>}
 */
export function assertClosed(obj, allowed, pointer) {
  if (!isPlainObject(obj)) reject(REASONS.MALFORMED, pointer, 'expected a plain object');
  for (const key of Object.keys(/** @type {Record<string, unknown>} */ (obj))) {
    if (!allowed.includes(key)) {
      reject(REASONS.UNKNOWN_FIELD, `${pointer}/${key}`, `field "${key}" is not in the closed schema {${allowed.join(', ')}}`);
    }
  }
  return /** @type {Record<string, unknown>} */ (obj);
}

/** Keys that signal a full-schema dump — a normalized fixture must never carry one. */
export const SCHEMA_DUMP_KEYS = Object.freeze(['schema', 'inputschema', 'outputschema', 'jsonschema', 'properties', 'parameters', 'definitions']);

/**
 * Reject any schema-shaped payload. Progressive discovery forbids dumping full
 * schemas, so a normalized value that enumerates schema keys (as object keys or
 * inside a string list such as `dataKeys`) is a SCHEMA_DUMP.
 * @param {readonly string[]} keys
 * @param {string} pointer
 */
export function assertNoSchemaDump(keys, pointer) {
  for (const key of keys) {
    if (SCHEMA_DUMP_KEYS.includes(String(key).toLowerCase())) {
      reject(REASONS.SCHEMA_DUMP, `${pointer}/${key}`, `"${key}" leaks a schema dump into a normalized capture`);
    }
  }
}

/** Broad-mask tokens: an expectation containing any of these is not exact. */
export const BROAD_TOKENS = Object.freeze(['|', ' or ', '*', 'any', 'success|error', '~', 'includes:', 'matches']);
/** The ONLY comparison mode a strict parity fixture may declare. */
export const EXACT_MATCH = 'exact';

/**
 * Refuse broad expectations. Parity is proven with exact `toEqual`, never with a
 * mask like `success|error` or `success or error`; the only legal `match` is
 * the literal 'exact'.
 * @param {unknown} match
 * @param {string} pointer
 */
export function assertExactMatch(match, pointer) {
  if (typeof match !== 'string') reject(REASONS.MALFORMED, pointer, 'match must be a string');
  const lowered = /** @type {string} */ (match).toLowerCase();
  for (const token of BROAD_TOKENS) {
    if (lowered.includes(token)) reject(REASONS.BROAD_EXPECTATION, pointer, `broad expectation token "${token}" in "${match}"`);
  }
  if (match !== EXACT_MATCH) reject(REASONS.BROAD_EXPECTATION, pointer, `only "${EXACT_MATCH}" comparison is allowed, got "${match}"`);
}

/**
 * A normalized error code is the TYPED contract (UPPER_SNAKE_CASE), never the
 * transport's numeric JSON-RPC code. A numeric or `-32xxx` code is framing that
 * must not appear in a framing-neutral fixture.
 * @param {unknown} code
 * @param {string} pointer
 * @returns {string}
 */
export function assertTypedErrorCode(code, pointer) {
  if (typeof code !== 'string' || code.length === 0) reject(REASONS.MALFORMED, pointer, 'error code must be a non-empty string');
  const value = /** @type {string} */ (code);
  if (/^-?\d+$/.test(value)) reject(REASONS.MALFORMED, pointer, `numeric JSON-RPC code "${value}" is transport framing, not a typed code`);
  if (!/^[A-Z][A-Z0-9_]*$/.test(value)) reject(REASONS.MALFORMED, pointer, `error code "${value}" is not an UPPER_SNAKE typed code`);
  return value;
}

/**
 * Assert a revision sequence never decreases. A drop is a STALE_REVISION at the
 * exact index — this is how a rewound/stale revision is caught deterministically.
 * @param {readonly number[]} revisions
 * @param {string} pointer
 */
export function assertMonotonic(revisions, pointer) {
  for (let i = 0; i < revisions.length; i += 1) {
    const value = revisions[i];
    if (!Number.isInteger(value) || value < 1) reject(REASONS.MALFORMED, `${pointer}/${i}`, `revision must be an integer >= 1, got ${String(value)}`);
    if (i > 0 && value < revisions[i - 1]) {
      reject(REASONS.STALE_REVISION, `${pointer}/${i}`, `revision ${value} is stale below prior ${revisions[i - 1]}`);
    }
  }
}
