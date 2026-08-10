// @ts-check
// tests/unit/cross-transport/matrix-dimensions.mjs
// Task 46 gate - the GENERATED cross-transport matrix.
//
// The plan asks for "generated cross-transport matrices", not a hand-written
// list of calls. Hand-writing the cases would reintroduce the exact defect this
// gate exists to catch: a human picks the capability whose behavior they already
// believe, and the matrix confirms the belief. So every case that needs a real
// capability RESOLVES one out of the canonical registry by CONTRACT PREDICATE
// (policy.consent === 'explicit', behavior.supportsPreview === false, ...) and
// takes the lexicographically first match. Deterministic, contract-derived, and
// it moves when the contract moves.
//
// Each case is transport-neutral on purpose: it describes the gateway call to
// make, never how to frame it. The stdio driver and the native /mcp driver each
// turn the same case into their own framing, and both feed the result through
// the single projection in matrix-projection.mjs.

import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { MATRIX_DIMENSIONS } from '../mcp-primitives/parity-harness-schema.mjs';

/**
 * @typedef {{ id: string, policy: { requiredScope: string, consent: string }, behavior: { effect: string, supportsPreview: boolean, idempotency: string, longRunning: boolean }, cost: { latency: string, resources: string }, routing: { parentTool: string, dispatchAction: string }, schemas: { input: { required?: string[] } } }} CapabilityRecord
 * @typedef {{ id: string, dimension: string, scenario: string, capabilityId: string|null, call: Record<string, unknown>, followUp: Record<string, unknown>|null, extractor: string, repeat: number }} MatrixCase
 */

/** Gateway execute arguments in the shape BOTH transports actually accept:
 * `tool` + `action`, never a dotted capability id, and `expectedRevisions` /
 * `idempotencyKey` / `preview` under `options`. Building them here (from the
 * record's own routing block) is what stops a case from being unrunnable on one
 * transport because its argument names were guessed. */
/** @param {CapabilityRecord} record @param {Record<string, unknown>} [params] @param {Record<string, unknown>|null} [options] */
export function executeArgs(record, params = {}, options = null) {
  /** @type {Record<string, unknown>} */
  const args = {
    operation: 'execute',
    tool: record.routing.parentTool,
    action: record.routing.dispatchAction,
    params,
  };
  if (options) args.options = options;
  return args;
}

const REGISTRY_PATH = 'src/tools/catalog/capabilities/generated/canonical-registry.generated.json';

/** @returns {CapabilityRecord[]} */
export function loadRecords(root = process.cwd()) {
  const raw = readFileSync(resolve(root, REGISTRY_PATH), 'utf8');
  const parsed = /** @type {{ records: CapabilityRecord[] }} */ (JSON.parse(raw));
  return [...parsed.records].sort((a, b) => a.id.localeCompare(b.id));
}

/**
 * First record satisfying `predicate`, by id order. Throws rather than returning
 * a fallback: a matrix that silently drops a case because the contract stopped
 * containing that shape would report parity over fewer dimensions than it claims.
 * @param {CapabilityRecord[]} records
 * @param {(record: CapabilityRecord) => boolean} predicate
 * @param {string} description
 * @returns {CapabilityRecord}
 */
export function pick(records, predicate, description) {
  const found = records.find(predicate);
  if (!found) throw new Error(`Task 46 matrix cannot be generated: no capability satisfies ${description}`);
  return found;
}

/**
 * A case must reach the gate it is named after. A capability with required
 * input parameters is refused by SCHEMA VALIDATION before consent, preview,
 * revision or idempotency is ever consulted — so the cell would compare two
 * MISSING_REQUIRED_PARAMETER refusals and report parity for a dimension it
 * never exercised. Every capability the matrix picks therefore has an empty
 * `required` list, and that is a property of the CONTRACT, not of this file.
 * @param {CapabilityRecord} record
 */
const callableWithNoParams = (record) =>
  Array.isArray(record.schemas?.input?.required) === false ||
  (record.schemas?.input?.required ?? []).length === 0;

/**
 * Read-only, genuinely CHEAP, and present on every build: the anchor for cases
 * that need a capability which does not mutate the editor.
 *
 * The cost class is load-bearing, not decoration. A read whose payload is large
 * (a report generator, a whole-project asset search) hits the response-size
 * policy before anything else, and every dimension anchored on it then compares
 * two size refusals instead of the gate it was named after. Requiring the
 * contract's own `instant`/`low` cost class keeps the anchor small — and keeps
 * the choice derived from the record rather than hand-picked.
 * @param {CapabilityRecord} record
 */
const isCheapRead = (record) =>
  record.policy.requiredScope === 'read' &&
  record.behavior.effect === 'read' &&
  record.policy.consent === 'none' &&
  record.cost.latency === 'instant' &&
  record.cost.resources === 'low' &&
  callableWithNoParams(record);

/**
 * Expand the matrix. Returns one case per (dimension, scenario); every dimension
 * in MATRIX_DIMENSIONS is covered, and the function asserts that before
 * returning, so a dimension cannot be dropped by editing this file.
 * @param {CapabilityRecord[]} records @returns {MatrixCase[]}
 */
export function generateMatrix(records) {
  const read = pick(records, isCheapRead, 'a cheap read-scope capability');
  const consented = pick(records, (r) => r.policy.consent === 'explicit' && callableWithNoParams(r), "policy.consent === 'explicit' with no required parameters");
  const noPreview = pick(records, (r) => r.behavior.supportsPreview === false && r.behavior.effect !== 'read' && callableWithNoParams(r), 'a mutating capability with supportsPreview false and no required parameters');
  const destructive = pick(records, (r) => r.policy.requiredScope === 'destructive', "policy.requiredScope === 'destructive'");
  const costly = pick(records, (r) => r.cost.latency === 'long-running', "cost.latency === 'long-running'");

  /** @type {MatrixCase[]} */
  const cases = [
    {
      id: 'receipt/read-success', dimension: 'receipt', extractor: 'receipt', repeat: 1,
      scenario: 'a successful read returns a canonical receipt',
      capabilityId: read.id, call: executeArgs(read), followUp: null,
    },
    {
      id: 'error/unknown-capability', dimension: 'error', extractor: 'error', repeat: 1,
      scenario: 'an unknown capability is a guided typed refusal',
      capabilityId: null, followUp: null,
      call: { operation: 'execute', tool: 'no_such_tool', action: 'no_such_action', params: {} },
    },
    {
      id: 'scope/declared-scope-is-discoverable', dimension: 'scope', extractor: 'policy', repeat: 1,
      scenario: 'a destructive capability declares the same scope and consent on both transports',
      capabilityId: destructive.id, followUp: null,
      call: { operation: 'describe', tool: destructive.routing.parentTool, action: destructive.routing.dispatchAction },
    },
    {
      id: 'consent/missing-consent-is-refused', dimension: 'consent', extractor: 'error', repeat: 1,
      scenario: 'a consent-gated capability without a consent grant is refused before dispatch',
      capabilityId: consented.id, call: executeArgs(consented), followUp: null,
    },
    {
      id: 'idempotency/same-key-different-params', dimension: 'idempotency', extractor: 'idempotency', repeat: 1,
      scenario: 'one idempotency key reused with different parameters',
      capabilityId: read.id,
      call: executeArgs(read, {}, { idempotencyKey: 'task46-matrix-key' }),
      followUp: executeArgs(read, { task46Divergent: true }, { idempotencyKey: 'task46-matrix-key' }),
    },
    {
      id: 'revision/stale-precondition-is-refused', dimension: 'revision', extractor: 'error', repeat: 1,
      scenario: 'a stale expected revision is refused before dispatch',
      capabilityId: read.id, followUp: null,
      call: executeArgs(read, {}, { expectedRevisions: { selection: 1 } }),
    },
    {
      id: 'preview/unsupported-preview-is-refused', dimension: 'preview', extractor: 'error', repeat: 1,
      scenario: 'options.preview on a capability with no dry-run path is refused, not silently ignored',
      capabilityId: noPreview.id, followUp: null,
      call: executeArgs(noPreview, {}, { preview: true }),
    },
    {
      id: 'progress/token-is-never-invented', dimension: 'progress', extractor: 'progress', repeat: 1,
      scenario: 'a client progress token is preserved and no internal id is substituted',
      capabilityId: read.id, call: executeArgs(read), followUp: null,
    },
    {
      id: 'task/checkpoint-is-session-scoped', dimension: 'task', extractor: 'task', repeat: 1,
      scenario: 'a task-augmented read-only checkpoint is session scoped and terminal',
      capabilityId: null, followUp: null,
      call: { operation: 'search', query: 'spawn actor' },
    },
    {
      id: 'cancellation/late-cancel-settles-once', dimension: 'cancellation', extractor: 'queue', repeat: 1,
      scenario: 'a cancelled request yields at most one terminal result',
      capabilityId: read.id, call: executeArgs(read), followUp: null,
    },
    {
      id: 'cache/catalog-revision-is-stable', dimension: 'cache', extractor: 'cache', repeat: 1,
      scenario: 'repeated discovery reports one stable catalog revision',
      capabilityId: read.id,
      call: { operation: 'describe', tool: read.routing.parentTool, action: read.routing.dispatchAction },
      followUp: { operation: 'describe', tool: read.routing.parentTool, action: read.routing.dispatchAction },
    },
    {
      id: 'queue/concurrent-reads-all-terminate', dimension: 'queue', extractor: 'queue', repeat: 8,
      scenario: 'concurrent requests each produce exactly one terminal result',
      capabilityId: read.id, call: executeArgs(read), followUp: null,
    },
    {
      id: 'reconnect/unknown-session-is-refused', dimension: 'reconnect', extractor: 'session', repeat: 1,
      scenario: 'an unknown session is refused rather than silently re-created',
      capabilityId: null, followUp: null,
      call: { operation: 'search', query: 'reconnect probe' },
    },
    {
      id: 'timeout/cost-class-is-declared-identically', dimension: 'timeout', extractor: 'cost', repeat: 1,
      scenario: 'a long-running capability declares the same cost class on both transports',
      capabilityId: costly.id, followUp: null,
      call: { operation: 'describe', tool: costly.routing.parentTool, action: costly.routing.dispatchAction },
    },
    {
      id: 'shutdown/teardown-leaves-no-usable-session', dimension: 'shutdown', extractor: 'session', repeat: 1,
      scenario: 'after teardown the session no longer answers',
      capabilityId: null, followUp: null,
      call: { operation: 'search', query: 'shutdown probe' },
    },
  ];


  const covered = new Set(cases.map((matrixCase) => matrixCase.dimension));
  const missing = MATRIX_DIMENSIONS.filter((dimension) => !covered.has(dimension));
  if (missing.length > 0) {
    throw new Error(`Task 46 matrix is incomplete: no case covers {${missing.join(', ')}}`);
  }
  return cases;
}
