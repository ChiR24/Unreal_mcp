// @ts-check
// tests/unit/task-38/parity-harness-validators.mjs
// Task 38 lane E — the DOMAIN validators. Each parses one untrusted normalized
// value at the boundary into a closed, framing-neutral shape (parse-don't-
// validate) or throws a precise HarnessRejection. Together they define the
// strict result / error / revision / profile / session / pointer schema for both
// executable-TS captures and native-protocol captures. `validateCapture` is the
// single envelope gate: it refuses source-text captures outright and tags every
// accepted capture as executable or not.

import {
  REASONS,
  CAPTURE_KINDS,
  EXECUTABLE_KINDS,
  DOMAINS,
  reject,
  isPlainObject,
  assertClosed,
  assertNoSchemaDump,
  assertExactMatch,
  assertTypedErrorCode,
  assertMonotonic,
} from './parity-harness-schema.mjs';

/**
 * @typedef {{ uri: string, mimeType: string, revision: number, dataPresent: boolean, dataKeys: string[] }} NormResult
 * @typedef {{ code: string, uri: string }} NormError
 * @typedef {{ uri: string, revisions: number[] }} NormRevision
 * @typedef {{ hasResources: boolean, hasPrompts: boolean, hasCompletions: boolean, hasSubscriptions: boolean, hasElicitation: boolean, hasTasks: boolean }} NormProfile
 * @typedef {{ uri: string, ownerSessionId: string }} SessionRecord
 * @typedef {{ sessionId: string, records: SessionRecord[], cleaned: boolean }} NormSession
 * @typedef {{ primitive: string, mode: 'native'|'gateway', reference: string }} NormPointer
 * @typedef {'executable-ts'|'native-protocol'|'native-model'|'source-text'} CaptureKind
 * @typedef {'result'|'error'|'revision'|'profile'|'session'|'pointer'} Domain
 * @typedef {{ mechanism: string, testName: string, engineVersion: string, protocolVersion: string, capturedAt: string, transcriptRef: string, transcriptSha256: string, sourceHash: string, packageHash: string }} NativeTranscript
 * @typedef {{ id: string, captureKind: CaptureKind, domain: Domain, executable: boolean, provenance: string, value: unknown, transcript?: NativeTranscript }} Capture
 */

/** @param {unknown} v @param {string} p @returns {string} */
function str(v, p) {
  if (typeof v !== 'string' || v.length === 0) reject(REASONS.MALFORMED, p, 'expected a non-empty string');
  return /** @type {string} */ (v);
}

/** @param {unknown} v @param {string} p @returns {boolean} */
function bool(v, p) {
  if (typeof v !== 'boolean') reject(REASONS.MALFORMED, p, 'expected a strict boolean');
  return /** @type {boolean} */ (v);
}

/** The only mechanisms that produce a genuine native-protocol capture: an executed
 * compiled native surface, never a hand-authored model or a source grep. */
export const NATIVE_MECHANISMS = Object.freeze(['native-automation-inprocess', 'native-http-sse']);
/** The MCP versions the native /mcp surface supports (stricter than the TS SDK:
 * no legacy 2024-* and not the fictional 2026-07-28 RC). */
export const NATIVE_PROTOCOL_VERSIONS = Object.freeze(['2025-11-25', '2025-06-18', '2025-03-26']);
/** The closed provenance block every native-protocol capture must carry. */
export const NATIVE_TRANSCRIPT_KEYS = Object.freeze([
  'mechanism', 'testName', 'engineVersion', 'protocolVersion', 'capturedAt',
  'transcriptRef', 'transcriptSha256', 'sourceHash', 'packageHash',
]);

const HEX64 = /^[0-9a-f]{64}$/;

/** @param {unknown} v @param {string} p @returns {string} */
function hex64(v, p) {
  const s = str(v, p);
  if (!HEX64.test(s)) reject(REASONS.MALFORMED, p, `expected a lowercase 64-char sha-256 hex digest, got "${s}"`);
  return s;
}

/**
 * Parse the closed transcript-provenance block a native-protocol capture must carry.
 * This is the schema half of the ground-truth: it proves the capture DECLARES an
 * executed mechanism, a supported protocol version, an ISO capture time, a bounded
 * relative transcript path, and three sha-256 anchors (transcript/source/package).
 * The fs half (that those anchors actually match) lives in parity-harness-native-capture.mjs.
 * @param {unknown} raw @param {string} [p] @returns {NativeTranscript}
 */
export function validateNativeTranscript(raw, p = '/transcript') {
  const o = assertClosed(raw, NATIVE_TRANSCRIPT_KEYS, p);
  for (const key of NATIVE_TRANSCRIPT_KEYS) {
    if (!(key in o)) reject(REASONS.MALFORMED, `${p}/${key}`, `native transcript is missing "${key}"`);
  }
  const mechanism = str(o.mechanism, `${p}/mechanism`);
  if (!NATIVE_MECHANISMS.includes(mechanism)) reject(REASONS.MALFORMED, `${p}/mechanism`, `mechanism "${mechanism}" is not an executed native mechanism {${NATIVE_MECHANISMS.join(', ')}}`);
  const protocolVersion = str(o.protocolVersion, `${p}/protocolVersion`);
  if (!NATIVE_PROTOCOL_VERSIONS.includes(protocolVersion)) reject(REASONS.MALFORMED, `${p}/protocolVersion`, `protocolVersion "${protocolVersion}" is not a native /mcp version {${NATIVE_PROTOCOL_VERSIONS.join(', ')}}`);
  const capturedAt = str(o.capturedAt, `${p}/capturedAt`);
  if (Number.isNaN(Date.parse(capturedAt))) reject(REASONS.MALFORMED, `${p}/capturedAt`, `capturedAt "${capturedAt}" is not an ISO-8601 timestamp`);
  const transcriptRef = str(o.transcriptRef, `${p}/transcriptRef`);
  if (transcriptRef.startsWith('/') || transcriptRef.includes('..')) reject(REASONS.MALFORMED, `${p}/transcriptRef`, `transcriptRef "${transcriptRef}" must be a bounded relative path`);
  return {
    mechanism,
    testName: str(o.testName, `${p}/testName`),
    engineVersion: str(o.engineVersion, `${p}/engineVersion`),
    protocolVersion,
    capturedAt,
    transcriptRef,
    transcriptSha256: hex64(o.transcriptSha256, `${p}/transcriptSha256`),
    sourceHash: hex64(o.sourceHash, `${p}/sourceHash`),
    packageHash: hex64(o.packageHash, `${p}/packageHash`),
  };
}

/** @param {unknown} raw @param {string} [p] @returns {NormResult} */
export function validateResult(raw, p = '/value') {
  const o = assertClosed(raw, ['uri', 'mimeType', 'revision', 'dataPresent', 'dataKeys'], p);
  const revision = o.revision;
  if (!Number.isInteger(revision) || /** @type {number} */ (revision) < 1) reject(REASONS.MALFORMED, `${p}/revision`, 'revision must be an integer >= 1');
  if (!Array.isArray(o.dataKeys)) reject(REASONS.MALFORMED, `${p}/dataKeys`, 'dataKeys must be an array');
  const dataKeys = /** @type {unknown[]} */ (o.dataKeys).map((k, i) => str(k, `${p}/dataKeys/${i}`));
  assertNoSchemaDump(dataKeys, `${p}/dataKeys`);
  return { uri: str(o.uri, `${p}/uri`), mimeType: str(o.mimeType, `${p}/mimeType`), revision: /** @type {number} */ (revision), dataPresent: bool(o.dataPresent, `${p}/dataPresent`), dataKeys: [...dataKeys].sort() };
}

/** @param {unknown} raw @param {string} [p] @returns {NormError} */
export function validateError(raw, p = '/value') {
  const o = assertClosed(raw, ['code', 'uri'], p);
  return { code: assertTypedErrorCode(o.code, `${p}/code`), uri: str(o.uri, `${p}/uri`) };
}

/** @param {unknown} raw @param {string} [p] @returns {NormRevision} */
export function validateRevision(raw, p = '/value') {
  const o = assertClosed(raw, ['uri', 'revisions'], p);
  if (!Array.isArray(o.revisions) || o.revisions.length === 0) reject(REASONS.MALFORMED, `${p}/revisions`, 'revisions must be a non-empty array');
  const revisions = /** @type {number[]} */ (o.revisions);
  assertMonotonic(revisions, `${p}/revisions`);
  return { uri: str(o.uri, `${p}/uri`), revisions: [...revisions] };
}

const PROFILE_KEYS = ['hasResources', 'hasPrompts', 'hasCompletions', 'hasSubscriptions', 'hasElicitation', 'hasTasks'];

/** @param {unknown} raw @param {string} [p] @returns {NormProfile} */
export function validateProfile(raw, p = '/value') {
  const o = assertClosed(raw, PROFILE_KEYS, p);
  for (const key of PROFILE_KEYS) {
    if (!(key in o)) reject(REASONS.MALFORMED, `${p}/${key}`, `missing profile boolean "${key}"`);
  }
  return {
    hasResources: bool(o.hasResources, `${p}/hasResources`),
    hasPrompts: bool(o.hasPrompts, `${p}/hasPrompts`),
    hasCompletions: bool(o.hasCompletions, `${p}/hasCompletions`),
    hasSubscriptions: bool(o.hasSubscriptions, `${p}/hasSubscriptions`),
    hasElicitation: bool(o.hasElicitation, `${p}/hasElicitation`),
    hasTasks: bool(o.hasTasks, `${p}/hasTasks`),
  };
}

/** @param {unknown} raw @param {string} [p] @returns {NormSession} */
export function validateSession(raw, p = '/value') {
  const o = assertClosed(raw, ['sessionId', 'records', 'cleaned'], p);
  const sessionId = str(o.sessionId, `${p}/sessionId`);
  if (!Array.isArray(o.records)) reject(REASONS.MALFORMED, `${p}/records`, 'records must be an array');
  const records = /** @type {unknown[]} */ (o.records).map((rec, i) => {
    const r = assertClosed(rec, ['uri', 'ownerSessionId'], `${p}/records/${i}`);
    const owner = str(r.ownerSessionId, `${p}/records/${i}/ownerSessionId`);
    if (owner !== sessionId) reject(REASONS.CROSS_SESSION_ID, `${p}/records/${i}/ownerSessionId`, `record owned by "${owner}" leaks into session "${sessionId}"`);
    return { uri: str(r.uri, `${p}/records/${i}/uri`), ownerSessionId: owner };
  });
  if (o.cleaned !== true) reject(REASONS.MISSING_CLEANUP, `${p}/cleaned`, `session "${sessionId}" has no cleanup receipt (cleaned !== true)`);
  return { sessionId, records, cleaned: true };
}

/** The single-key executable references a bounded fallback pointer may carry. */
const POINTER_CALL_KEYS = ['method', 'operation'];
const MAX_POINTER_JSON = 280;
const MAX_POINTER_HINT = 200;

/**
 * Validate a fallback pointer and normalize it to {primitive, mode, reference}.
 * Accepts the production raw shape {primitive, mode, hint, nextCall} and the
 * already-normalized shape. Enforces boundedness: exactly one executable
 * reference, no schema dump, hint and total size within budget.
 * @param {unknown} raw @param {string} [p] @returns {NormPointer}
 */
export function validatePointer(raw, p = '/value') {
  if (!isPlainObject(raw)) reject(REASONS.MALFORMED, p, 'pointer must be an object');
  const o = /** @type {Record<string, unknown>} */ (raw);
  const primitive = str(o.primitive, `${p}/primitive`);
  const mode = o.mode;
  if (mode !== 'native' && mode !== 'gateway') reject(REASONS.MALFORMED, `${p}/mode`, `mode must be native|gateway, got ${String(mode)}`);
  let reference;
  if ('nextCall' in o) {
    assertClosed(o, ['primitive', 'mode', 'hint', 'nextCall'], p);
    if (typeof o.hint === 'string' && o.hint.length > MAX_POINTER_HINT) reject(REASONS.UNBOUNDED_FALLBACK, `${p}/hint`, `hint of ${o.hint.length} chars exceeds ${MAX_POINTER_HINT}`);
    const call = assertClosed(o.nextCall, POINTER_CALL_KEYS, `${p}/nextCall`);
    const keys = Object.keys(call);
    assertNoSchemaDump(keys, `${p}/nextCall`);
    if (keys.length !== 1) reject(REASONS.UNBOUNDED_FALLBACK, `${p}/nextCall`, `a bounded pointer carries exactly one call key, found ${keys.length}`);
    reference = str(call[keys[0]], `${p}/nextCall/${keys[0]}`);
  } else {
    assertClosed(o, ['primitive', 'mode', 'reference'], p);
    reference = str(o.reference, `${p}/reference`);
  }
  const normalized = { primitive, mode: /** @type {'native'|'gateway'} */ (mode), reference };
  if (JSON.stringify(normalized).length > MAX_POINTER_JSON) reject(REASONS.UNBOUNDED_FALLBACK, p, `normalized pointer exceeds ${MAX_POINTER_JSON} bytes`);
  return normalized;
}

/** @type {Record<Domain, (raw: unknown, p?: string) => unknown>} */
const DOMAIN_VALIDATORS = {
  result: validateResult,
  error: validateError,
  revision: validateRevision,
  profile: validateProfile,
  session: validateSession,
  pointer: validatePointer,
};

/**
 * The single capture-envelope gate. Refuses a source-text capture outright,
 * validates the inner value by domain, and returns a closed Capture tagged with
 * whether it is executable (the only kind a completion claim may rest on).
 * @param {unknown} raw @param {string} [p] @returns {Capture}
 */
export function validateCapture(raw, p = '/') {
  const o = assertClosed(raw, ['id', 'captureKind', 'domain', 'match', 'provenance', 'value', 'transcript'], p);
  const id = str(o.id, `${p}id`);
  const captureKind = str(o.captureKind, `${p}captureKind`);
  if (!CAPTURE_KINDS.includes(captureKind)) reject(REASONS.MALFORMED, `${p}captureKind`, `unknown captureKind "${captureKind}"`);
  if (captureKind === 'source-text') reject(REASONS.SOURCE_TEXT_CAPTURE, `${p}captureKind`, `capture "${id}" is a source-text snapshot; only executed runtime captures are admissible`);
  const domain = str(o.domain, `${p}domain`);
  if (!DOMAINS.includes(domain)) reject(REASONS.MALFORMED, `${p}domain`, `unknown domain "${domain}"`);
  assertExactMatch(o.match, `${p}match`);
  const provenance = str(o.provenance, `${p}provenance`);
  const value = DOMAIN_VALIDATORS[/** @type {Domain} */ (domain)](o.value, `${p}value`);
  // The transcript block is the native-protocol capture's provenance; it is REQUIRED
  // there and forbidden on every other kind so an executable-ts (or a modelled) side
  // can never smuggle a native provenance in, and a native side can never omit it.
  const hasTranscript = 'transcript' in o;
  let transcript;
  if (captureKind === 'native-protocol') {
    if (!hasTranscript) reject(REASONS.MALFORMED, `${p}transcript`, `native-protocol capture "${id}" requires a transcript provenance block`);
    transcript = validateNativeTranscript(o.transcript, `${p}transcript`);
  } else if (hasTranscript) {
    reject(REASONS.UNKNOWN_FIELD, `${p}transcript`, `only a native-protocol capture may carry a transcript block; "${id}" is ${captureKind}`);
  }
  const capture = { id, captureKind: /** @type {CaptureKind} */ (captureKind), domain: /** @type {Domain} */ (domain), executable: EXECUTABLE_KINDS.includes(captureKind), provenance, value };
  return transcript ? { ...capture, transcript } : capture;
}
