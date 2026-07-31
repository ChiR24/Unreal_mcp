// @ts-check
// tests/unit/mcp-primitives/parity-harness-drift.mjs
// Task 38 lane E — DRIFT INJECTORS. Each function deep-clones a valid normalized
// value (or capture) and introduces EXACTLY ONE violation. They are the adversary
// the harness must catch: the self-tests feed each injected artifact back through
// the validators / comparator and assert the exact reason (or the exact single
// JSON pointer) fires — proving the harness cannot be fooled into a false GREEN.
// A drift that changed two things at once would make "fails exactly" unprovable,
// so every injector documents the single field it mutates.

/** @typedef {import('./parity-harness-validators.mjs').NormResult} NormResult */
/** @typedef {import('./parity-harness-validators.mjs').NormError} NormError */
/** @typedef {import('./parity-harness-validators.mjs').NormRevision} NormRevision */
/** @typedef {import('./parity-harness-validators.mjs').NormProfile} NormProfile */
/** @typedef {import('./parity-harness-validators.mjs').NormSession} NormSession */
/** @typedef {import('./parity-harness-validators.mjs').Capture} Capture */

/**
 * Deterministic deep clone of any JSON-safe value (structuredClone would also work).
 * @template T @param {T} value @returns {T}
 */
export function clone(value) {
  return JSON.parse(JSON.stringify(value));
}

/**
 * One-field result drift: change a single result field to a new value. The
 * comparator must report a mismatch at exactly `/<field>` and nowhere else.
 * @param {NormResult} result @param {keyof NormResult} field @param {unknown} value @returns {NormResult}
 */
export function driftResultField(result, field, value) {
  const next = clone(result);
  (/** @type {Record<string, unknown>} */ (next))[field] = value;
  return next;
}

/**
 * Wrong error code: swap the typed code for a different but still well-formed
 * code. Both validate; parity must catch the difference at `/code`.
 * @param {NormError} error @param {string} badCode @returns {NormError}
 */
export function wrongErrorCode(error, badCode = 'RESOURCE_UNAVAILABLE') {
  const next = clone(error);
  next.code = badCode;
  return next;
}

/**
 * Stale revision: append a revision below the prior one so the sequence rewinds.
 * validateRevision must throw STALE_REVISION at the offending index.
 * @param {NormRevision} revision @returns {NormRevision}
 */
export function staleRevision(revision) {
  const next = clone(revision);
  const last = next.revisions[next.revisions.length - 1];
  next.revisions.push(last - 1);
  return next;
}

/**
 * False capability: flip one profile boolean to `true` that the honest profile
 * left `false` — an over-claimed capability. Parity must catch it at that field.
 * @param {NormProfile} profile @param {keyof NormProfile} field @returns {NormProfile}
 */
export function falseCapability(profile, field = 'hasTasks') {
  const next = clone(profile);
  next[field] = true;
  return next;
}

/**
 * Unbounded fallback: return a RAW pointer whose nextCall carries MORE THAN ONE
 * executable reference (both a gateway operation AND a native method) instead of
 * exactly one, so validatePointer throws UNBOUNDED_FALLBACK. This is the "hand the
 * client a menu instead of one bounded next step" failure.
 * @param {string} primitive @returns {Record<string, unknown>}
 */
export function unboundFallback(primitive = 'resources') {
  return {
    primitive,
    mode: 'gateway',
    hint: 'unbounded',
    nextCall: { operation: 'search', method: 'resources/list' },
  };
}

/**
 * Cleanup omission: mark a session as not cleaned up. validateSession must throw
 * MISSING_CLEANUP — a session fixture without a teardown receipt is inadmissible.
 * @param {NormSession} session @returns {NormSession}
 */
export function omitCleanup(session) {
  const next = clone(session);
  next.cleaned = false;
  return next;
}

/**
 * Cross-session leak: point one record at a foreign owner session. validateSession
 * must throw CROSS_SESSION_ID.
 * @param {NormSession} session @param {string} foreignId @returns {NormSession}
 */
export function crossSessionRecord(session, foreignId = 'session-OTHER') {
  const next = clone(session);
  if (next.records.length === 0) next.records.push({ uri: 'ue://selection', ownerSessionId: foreignId });
  else next.records[0].ownerSessionId = foreignId;
  return next;
}

/**
 * Turn an executable capture into a source-text snapshot. validateCapture must
 * throw SOURCE_TEXT_CAPTURE — a grep of source is never admissible evidence.
 * @param {Record<string, unknown>} capture @returns {Record<string, unknown>}
 */
export function toSourceText(capture) {
  const next = clone(capture);
  next.captureKind = 'source-text';
  return next;
}

/**
 * Broaden the comparison mode to a mask. validateCapture must throw
 * BROAD_EXPECTATION — parity is exact or it is nothing.
 * @param {Record<string, unknown>} capture @param {string} mask @returns {Record<string, unknown>}
 */
export function broadenMatch(capture, mask = 'success|error') {
  const next = clone(capture);
  next.match = mask;
  return next;
}

/**
 * Add an undeclared field to any closed object. The relevant validator must
 * throw UNKNOWN_FIELD.
 * @param {Record<string, unknown>} obj @param {string} key @param {unknown} value @returns {Record<string, unknown>}
 */
export function addUnknownField(obj, key = 'extra', value = 'x') {
  const next = clone(obj);
  next[key] = value;
  return next;
}

/**
 * Inject a schema dump into a result's bounded body. validateResult must throw
 * SCHEMA_DUMP because dataKeys now enumerates a schema.
 * @param {NormResult} result @returns {NormResult}
 */
export function injectSchemaDump(result) {
  const next = clone(result);
  next.dataKeys = [...next.dataKeys, 'inputSchema'].sort();
  return next;
}
