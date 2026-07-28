// @ts-check
// tests/unit/task-46/matrix-projection.mjs
// Task 46 gate - the SINGLE framing-neutral projection.
//
// This is the load-bearing module. Both transports' raw bytes go through THIS
// function and no other, so a divergence cannot be introduced by two drivers
// normalizing differently. Give it a stdio CallToolResult or a native /mcp
// JSON-RPC envelope (or an SSE frame carrying one) and it produces the same
// semantic cell for the same behavior.
//
// Two guards keep the stripping honest, in opposite directions:
//   - UNDER-stripping: any fact whose VALUE still looks like framing (a session
//     id, a port, an absolute path, a duration, a uuid) is refused. If framing
//     survived, the matrix would fail on every case for reasons that are not
//     behavioral, and someone would "fix" it by loosening the comparison.
//   - OVER-stripping: a cell with no facts is refused by validateMatrixCell as
//     VACUOUS_CELL. If the stripper deleted the evidence, the matrix would pass
//     on every case while asserting nothing.
// Those two together are why this file can be trusted to compare semantics.

import { REASONS, reject } from '../task-38/parity-harness-schema.mjs';
import { validateMatrixCell } from '../task-38/parity-harness-validators.mjs';

/** JSON-RPC / HTTP / SSE envelope keys. Named explicitly so "what counts as
 * framing" is a reviewable list rather than a regex someone widened. */
export const FRAMING_KEYS = Object.freeze([
  'jsonrpc', 'id', 'sessionId', 'mcp-session-id', '_meta', 'event', 'data:',
]);

/** A projected fact VALUE matching any of these still carries framing. */
const FRAMING_VALUE_PATTERNS = Object.freeze([
  /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/iu,
  /^\/(?:home|data|tmp|usr|var)\//u,
  /^[A-Za-z]:\\/u,
  /^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}/u,
  /^(?:127\.0\.0\.1|localhost)(?::\d+)?$/u,
  /^\d+ms$/u,
]);

/**
 * Unwrap whatever the transport handed back down to the MCP CallToolResult.
 * Handles a bare result, a JSON-RPC envelope, and an SSE frame whose `data:`
 * line carries the envelope. Doing all three HERE (rather than in each driver)
 * is what makes "the two transports were unwrapped the same way" true by
 * construction instead of by inspection.
 * @param {unknown} raw @returns {{ result: Record<string, unknown>|null, rpcError: Record<string, unknown>|null }}
 */
export function unwrap(raw) {
  let value = raw;
  if (typeof value === 'string') {
    const dataLine = value.split(/\r?\n/u).find((line) => line.startsWith('data:'));
    const payload = dataLine ? dataLine.slice(5).trim() : value;
    try {
      value = JSON.parse(payload);
    } catch {
      return { result: null, rpcError: null };
    }
  }
  if (typeof value !== 'object' || value === null) return { result: null, rpcError: null };
  const envelope = /** @type {Record<string, unknown>} */ (value);
  if (envelope.error && typeof envelope.error === 'object') {
    return { result: null, rpcError: /** @type {Record<string, unknown>} */ (envelope.error) };
  }
  const inner = envelope.result ?? envelope;
  if (typeof inner !== 'object' || inner === null) return { result: null, rpcError: null };
  return { result: /** @type {Record<string, unknown>} */ (inner), rpcError: null };
}

/**
 * Gateway ENVELOPE keys — the bookkeeping both surfaces wrap a receipt in.
 * Named explicitly (never pattern-matched) because the two transports package
 * the same payload differently: the native surface nests the capability's own
 * result under `structuredContent.data`, while the TypeScript surface flattens
 * it into `structuredContent` next to these keys. That difference is packaging,
 * not behavior, so the projection has to see through it — otherwise every
 * execute row reports a divergence that is only envelope shape, and someone
 * "fixes" the matrix by comparing less.
 *
 * `success` is deliberately ABSENT from this list: it is the capability's own
 * outcome on both surfaces and must stay comparable.
 */
export const GATEWAY_ENVELOPE_KEYS = Object.freeze([
  'operation', 'correlationId', 'catalogRevision', 'capability', 'tool', 'action',
  'migratedFrom', 'liveRevisions', 'receipt', 'isError', 'toolName', 'errorCode',
  'error', 'message', 'content', 'structuredContent', 'nextCall', 'suggestions',
]);

/** The capability payload a client actually reads, whichever envelope carried it.
 * @param {Record<string, unknown>|null} result @returns {Record<string, unknown>|null} */
function structuredData(result) {
  const structured = result?.structuredContent;
  if (typeof structured !== 'object' || structured === null) return null;
  const envelope = /** @type {Record<string, unknown>} */ (structured);
  if (typeof envelope.data === 'object' && envelope.data !== null) {
    return /** @type {Record<string, unknown>} */ (envelope.data);
  }
  const payload = Object.fromEntries(
    Object.entries(envelope).filter(([key]) => !GATEWAY_ENVELOPE_KEYS.includes(key) && !FRAMING_KEYS.includes(key)),
  );
  return Object.keys(payload).length > 0 ? payload : null;
}

/** Sorted, framing-free key list of an object, or [].
 * @param {unknown} value @returns {string[]} */
function keysOf(value) {
  if (typeof value !== 'object' || value === null) return [];
  return Object.keys(/** @type {Record<string, unknown>} */ (value))
    .filter((key) => !FRAMING_KEYS.includes(key))
    .sort();
}

/** Deep-search for the first typed UPPER_SNAKE code. Both surfaces nest the
 * typed code differently (TS under typedError, native under error/details), and
 * that nesting IS framing - the code itself is the contract.
 * @param {unknown} value @param {number} [depth] @returns {string|null} */
export function findTypedCode(value, depth = 0) {
  if (depth > 6 || typeof value !== 'object' || value === null) return null;
  const record = /** @type {Record<string, unknown>} */ (value);
  for (const key of ['code', 'errorCode', 'typedCode']) {
    const candidate = record[key];
    if (typeof candidate === 'string' && /^[A-Z][A-Z0-9_]*$/u.test(candidate)) return candidate;
  }
  for (const nested of Object.values(record)) {
    /** @type {string|null} */
    const found = findTypedCode(nested, depth + 1);
    if (found) return found;
  }
  return null;
}

/** @param {string} key @param {unknown} value @returns {string} */
const fact = (key, value) => `${key}=${String(value)}`;

/**
 * The named fact extractors. Each returns ONLY semantics. They are keyed by the
 * `extractor` a generated case declares, so adding a dimension cannot silently
 * reuse another dimension's evidence.
 * @type {Record<string, (result: Record<string, unknown>|null, extra: Record<string, unknown>) => string[]>}
 */
export const EXTRACTORS = {
  receipt: (result) => {
    const data = structuredData(result);
    return [
      fact('structuredContentPresent', result?.structuredContent != null),
      fact('dataKeys', keysOf(data).join('|') || 'none'),
      fact('dataSuccess', data?.success ?? 'absent'),
    ];
  },
  error: (result, extra) => [
    fact('isError', result?.isError === true),
    fact('hasSuggestions', extra.hasSuggestions === true),
    fact('hasNextCall', extra.hasNextCall === true),
  ],
  policy: (_result, extra) => [
    fact('requiredScope', extra.requiredScope ?? 'absent'),
    fact('consent', extra.consent ?? 'absent'),
    fact('effect', extra.effect ?? 'absent'),
  ],
  idempotency: (_result, extra) => [
    fact('secondCallReplayed', extra.replayed === true),
    fact('receiptsIdentical', extra.receiptsIdentical === true),
    fact('mutationsObserved', extra.mutationsObserved ?? 'unknown'),
  ],
  progress: (_result, extra) => [
    fact('clientTokenPreserved', extra.tokenPreserved !== false),
    fact('tokenInvented', extra.tokenInvented === true),
    fact('terminalResults', extra.terminalResults ?? 1),
  ],
  task: (_result, extra) => [
    fact('taskSupported', extra.taskSupported === true),
    fact('taskTerminalOnCreate', extra.taskTerminal === true),
    fact('visibleToOtherSession', extra.crossSessionVisible === true),
  ],
  cache: (_result, extra) => [
    fact('catalogRevisionPresent', extra.revisionPresent === true),
    fact('catalogRevisionStable', extra.revisionStable === true),
  ],
  queue: (_result, extra) => [
    fact('completed', extra.completed ?? 0),
    fact('duplicates', extra.duplicates ?? 0),
    fact('lost', extra.lost ?? 0),
  ],
  session: (_result, extra) => [
    fact('unknownSessionRefused', extra.refused === true),
    fact('silentlyRecreated', extra.recreated === true),
  ],
  cost: (_result, extra) => [
    fact('latency', extra.latency ?? 'absent'),
    fact('resources', extra.resources ?? 'absent'),
  ],
};

/**
 * Project one observation into a validated semantic cell.
 * @param {{ id: string, dimension: string, scenario: string, extractor: string }} matrixCase
 * @param {{ raw: unknown, extra?: Record<string, unknown> }} observation
 * @returns {import('../task-38/parity-harness-validators.mjs').NormMatrixCell}
 */
export function projectCell(matrixCase, observation) {
  const extract = EXTRACTORS[matrixCase.extractor];
  if (!extract) reject(REASONS.MALFORMED, `/${matrixCase.id}/extractor`, `no extractor named "${matrixCase.extractor}"`);
  const { result, rpcError } = unwrap(observation.raw);
  const extra = observation.extra ?? {};
  const typedCode = findTypedCode(rpcError ?? result);
  // A refusal is a refusal whether it arrived as isError, as a JSON-RPC error,
  // or as a typed code in the structured payload. Treating those three as
  // different outcomes would report a divergence where the two transports only
  // differ in framing - which is exactly the false positive that gets a real
  // parity test deleted.
  const refused = rpcError !== null || result?.isError === true || (typedCode !== null && extra.refusalExpectedAsCode === true);
  const facts = extract(result, extra);
  for (const entry of facts) {
    const value = entry.slice(entry.indexOf('=') + 1);
    for (const pattern of FRAMING_VALUE_PATTERNS) {
      if (pattern.test(value)) {
        reject(REASONS.MALFORMED, `/${matrixCase.id}/facts`, `fact "${entry}" still carries transport framing; the projection must strip it before comparison`);
      }
    }
  }
  return validateMatrixCell({
    dimension: matrixCase.dimension,
    scenario: matrixCase.scenario,
    outcome: refused ? 'refusal' : 'success',
    code: refused ? (typedCode ?? 'UNTYPED_REFUSAL') : 'NONE',
    facts,
  }, `/${matrixCase.id}`);
}

/**
 * Wrap a projected cell as a harness Capture. `captureKind` decides which side
 * of the parity comparison it may serve as; `native-protocol` additionally
 * requires the transcript provenance block, which is what stops a hand-written
 * native side from being accepted.
 * @param {string} id @param {'executable-ts'|'native-protocol'} captureKind
 * @param {unknown} cell @param {string} provenance @param {unknown} [transcript]
 */
export function asCapture(id, captureKind, cell, provenance, transcript) {
  const capture = { id, captureKind, domain: 'matrix', match: 'exact', provenance, value: cell };
  return transcript ? { ...capture, transcript } : capture;
}
