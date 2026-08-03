// @ts-check
// tests/unit/adversarial/fuzz-protocol.mjs
// Task 51 — the PROTOCOL generators: JSON-RPC frames, SSE/HTTP framing, session
// and auth envelopes, and the execute-envelope siblings (consent, revisions,
// idempotency, cancellation).
//
// These are shaped like the wire, not like the API. A generator that produced
// well-formed objects and then handed them to a validator would only ever test the
// happy path; the point is to produce the frames a hostile or broken client sends:
// a `params` that is an array, an `id` that is an object, a batch containing a
// batch, an SSE event split three ways across TCP reads, a session header for
// somebody else's session, a consent grant naming the WRONG capability.
//
// A note on the consent shape, because it is the one most easily got wrong: consent
// is an `automation_request` ENVELOPE SIBLING and never a handler parameter, and a
// grant is honoured only when it names THAT capability. So `consentEnvelope` can
// emit a grant for a different capability id, which must never be accepted — that
// is the case the generator exists for.

import { fuzzString, fuzzNumeric, ADVERSARIAL_TEXT } from './fuzz-generators.mjs';

/** Versions the native `/mcp` surface accepts. Anything else must be refused there. */
export const NATIVE_VERSIONS = Object.freeze(['2025-11-25', '2025-06-18', '2025-03-26']);
/** Accepted by the TypeScript SDK only — the intentional asymmetry. */
export const LEGACY_VERSIONS = Object.freeze(['2024-11-05', '2024-10-07']);
/** Never supported anywhere, including the fictional RC this project refuses to claim. */
export const REJECTED_VERSIONS = Object.freeze([
  '2026-07-28', '2025-11-26', '1.0', '', 'latest', '2025-11-25 ', ' 2025-11-25',
  '2025-11-25\u0000', '2025-11-２5',
]);

/** JSON-RPC `id` spellings, most of which are protocol violations. */
export const ID_SHAPES = Object.freeze([
  1, 0, -1, 2 ** 53, 1.5, '1', '', 'a'.repeat(512), null,
]);

/** @typedef {{ frame: unknown, shape: string, legal: boolean }} ProtocolCase */

/** @param {unknown} frame @param {string} shape @param {boolean} legal @returns {ProtocolCase} */
function protocolCase(frame, shape, legal) {
  return { frame, shape, legal };
}

/**
 * One JSON-RPC frame. `legal` states whether the frame is well-formed BY THE
 * SPEC — not whether the server should succeed, which depends on the method.
 * @param {import('./fuzz-random.mjs').Rng} rng
 * @returns {ProtocolCase}
 */
export function fuzzJsonRpcFrame(rng) {
  /** @type {readonly (readonly [number, () => ProtocolCase])[]} */
  const table = [
    [4, () => protocolCase(
      { jsonrpc: '2.0', id: rng.int(1, 9999), method: 'tools/call', params: { name: 'unreal', arguments: { operation: 'search', query: fuzzString(rng) } } },
      'well-formed-call', true)],
    [2, () => protocolCase(
      { jsonrpc: '2.0', id: rng.pick(ID_SHAPES), method: 'tools/call', params: {} },
      'odd-id', false)],
    [2, () => protocolCase(
      { jsonrpc: rng.pick(['1.0', '2', '', '2.0.0', 2.0]), id: 1, method: 'tools/list', params: {} },
      'bad-version', false)],
    [2, () => protocolCase(
      { jsonrpc: '2.0', id: 1, method: rng.pick([null, 42, {}, [], '', 'tools/../list']), params: {} },
      'bad-method', false)],
    [2, () => protocolCase(
      { jsonrpc: '2.0', id: 1, method: 'tools/call', params: rng.pick([[], 'string', 42, null, true]) },
      'bad-params', false)],
    [1, () => protocolCase(
      { jsonrpc: '2.0', method: 'notifications/initialized' },
      'notification', true)],
    [1, () => protocolCase(
      rng.list(rng.int(1, 4), (stream) => ({ jsonrpc: '2.0', id: stream.int(1, 99), method: 'tools/list', params: {} })),
      'batch', true)],
    [1, () => protocolCase(deepNest(rng, rng.int(8, 64)), 'deep-nesting', false)],
    [1, () => protocolCase(
      { jsonrpc: '2.0', id: 1, method: 'tools/call', params: { name: 'unreal', arguments: { operation: fuzzString(rng) } } },
      'unknown-operation', true)],
  ];
  return rng.weighted(table)();
}

/** A nested object `depth` levels deep — the shape that finds unbounded recursion. @param {import('./fuzz-random.mjs').Rng} rng @param {number} depth */
export function deepNest(rng, depth) {
  /** @type {Record<string, unknown>} */
  let node = { leaf: fuzzString(rng) };
  for (let level = 0; level < depth; level += 1) node = { [`k${level}`]: node };
  return { jsonrpc: '2.0', id: 1, method: 'tools/call', params: { name: 'unreal', arguments: node } };
}

/**
 * Serialize an SSE event and CHOP it into chunks at arbitrary offsets. The chop is
 * the point: a reader that splits a buffered body on "data: " passes this only when
 * every event happens to arrive whole, which is exactly the assumption the Task 49
 * SseReader was written to remove.
 * @param {import('./fuzz-random.mjs').Rng} rng @param {{ data: string, event?: string, id?: string }} spec
 * @returns {{ chunks: string[], expected: string }}
 */
export function fragmentSse(rng, spec) {
  const lines = [];
  if (spec.event !== undefined) lines.push(`event: ${spec.event}`);
  if (spec.id !== undefined) lines.push(`id: ${spec.id}`);
  for (const line of spec.data.split('\n')) lines.push(`data: ${line}`);
  const body = `${lines.join('\n')}\n\n`;
  const cuts = rng.shuffle([...Array(body.length).keys()]).slice(0, rng.int(0, 6)).sort((a, b) => a - b);
  /** @type {string[]} */
  const chunks = [];
  let previous = 0;
  for (const cut of cuts) {
    if (cut > previous) chunks.push(body.slice(previous, cut));
    previous = cut;
  }
  chunks.push(body.slice(previous));
  return { chunks: chunks.filter((chunk) => chunk.length > 0), expected: spec.data };
}

/**
 * Auth/session header sets. `authorized` is TRUE only when a correct token is
 * presented; every other row is a fail-closed expectation.
 * @param {import('./fuzz-random.mjs').Rng} rng @param {{ token: string, sessionId: string }} valid
 * @returns {{ headers: Record<string, string>, shape: string, authorized: boolean }}
 */
export function fuzzAuthHeaders(rng, valid) {
  const near = mutateToken(rng, valid.token);
  /** @type {readonly (readonly [number, () => { headers: Record<string, string>, shape: string, authorized: boolean }])[]} */
  const table = [
    [3, () => ({ headers: { 'X-MCP-Capability-Token': valid.token, 'Mcp-Session-Id': valid.sessionId }, shape: 'valid', authorized: true })],
    [3, () => ({ headers: { 'Mcp-Session-Id': valid.sessionId }, shape: 'no-token', authorized: false })],
    [3, () => ({ headers: { 'X-MCP-Capability-Token': near, 'Mcp-Session-Id': valid.sessionId }, shape: 'near-miss-token', authorized: false })],
    [2, () => ({ headers: { 'X-MCP-Capability-Token': '', 'Mcp-Session-Id': valid.sessionId }, shape: 'empty-token', authorized: false })],
    [2, () => ({ headers: { 'X-MCP-Capability-Token': valid.token, 'Mcp-Session-Id': fuzzString(rng) }, shape: 'foreign-session', authorized: false })],
    [1, () => ({ headers: { 'x-mcp-capability-token': valid.token, 'Mcp-Session-Id': valid.sessionId }, shape: 'lowercase-header', authorized: true })],
    [1, () => ({ headers: { 'X-MCP-Capability-Token': ` ${valid.token} `, 'Mcp-Session-Id': valid.sessionId }, shape: 'padded-token', authorized: false })],
  ];
  return rng.weighted(table)();
}

/** Near-miss tokens: prefixes, one-character edits, case flips, appended NUL. @param {import('./fuzz-random.mjs').Rng} rng @param {string} token */
export function mutateToken(rng, token) {
  if (token.length === 0) return rng.pick(['x', '\u0000']);
  return rng.weighted([
    [3, () => token.slice(0, rng.int(0, token.length - 1))],
    [3, () => `${token.slice(0, -1)}${token.endsWith('a') ? 'b' : 'a'}`],
    [2, () => token.toUpperCase()],
    [2, () => `${token}${rng.pick(ADVERSARIAL_TEXT)}`],
    [1, () => `${token}${token}`],
  ])();
}

/**
 * An execute envelope's SIBLINGS. Consent, revision pins, idempotency keys and
 * cancellation are generated together because their interactions are where the
 * interesting races live: a replayed key with a moved revision, a consent grant
 * for a different capability, a cancel for an id that already settled.
 * @param {import('./fuzz-random.mjs').Rng} rng @param {{ capabilityId: string, otherCapabilityId: string }} ids
 */
export function fuzzExecuteEnvelope(rng, ids) {
  /** @type {readonly (readonly [number, () => unknown])[]} */
  const consentTable = [
    [3, () => null],
    [3, () => ({ capability: ids.capabilityId, acknowledge: rng.pick(['explicit', 'elevated']) })],
    [3, () => ({ capability: ids.otherCapabilityId, acknowledge: 'explicit' })],
    [2, () => ({ capability: ids.capabilityId, acknowledge: rng.pick(['none', '', 'ELEVATED', 'yes', null]) })],
    [1, () => ({ acknowledge: 'explicit' })],
  ];
  /** @type {readonly (readonly [number, () => string|null])[]} */
  const keyTable = [
    [4, () => null],
    [3, () => `k-${rng.int(0, 3)}`],
    [2, () => fuzzString(rng)],
    [1, () => 'x'.repeat(rng.int(256, 4096))],
  ];
  /** @type {readonly (readonly [number, () => unknown])[]} */
  const revisionTable = [
    [4, () => null],
    [3, () => rng.int(0, 8)],
    [2, () => fuzzNumeric(rng)],
    [1, () => ({ assets: rng.int(0, 8), actors: rng.int(0, 8) })],
  ];
  /** @type {readonly (readonly [number, () => number|null])[]} */
  const cancelTable = [[6, () => null], [2, () => 0], [1, () => 1], [1, () => rng.int(2, 40)]];
  return {
    consent: rng.weighted(consentTable)(),
    idempotencyKey: rng.weighted(keyTable)(),
    expectedRevision: rng.weighted(revisionTable)(),
    cancelAfterMs: rng.weighted(cancelTable)(),
  };
}
