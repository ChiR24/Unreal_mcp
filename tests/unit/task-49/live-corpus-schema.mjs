// @ts-check
// tests/unit/task-49/live-corpus-schema.mjs
// Task 49 — the STRICT schema for the ONE transport-neutral live scenario corpus.
//
// This repository accumulated eight bespoke live probes (task40-security-matrix,
// task41-idempotency-probe, task42-stale-state-probe, task43-preview-compensation,
// task46-cross-transport-matrix, task-38-native-capture, task-38-parity-qa,
// task-37-stdio-qa). Each re-derived its own framing, its own timeouts and its own
// idea of what counts as proof. This module exists so the ninth one is never
// written: a scenario is DATA, validated here, and the two drivers are the only
// code that knows about a wire.
//
// The schema is deliberately hostile. A scenario is accepted ONLY when it is
// executable, bounded, independently checkable and self-cleaning; every looser
// shape is refused with a closed, machine-checkable reason. In particular it
// refuses the four ways a live suite historically told itself a comfortable lie:
//
//   1. a capability or parameter that does not exist   -> the case never ran
//   2. a broad expectation mask (`success|error`)      -> the case cannot fail
//   3. a mutation judged by its own response           -> a forged success passes
//   4. a mutation with no cleanup                      -> the next run inherits it
//
// It is plain ESM (.mjs) on purpose, exactly like tests/unit/task-38/parity-harness-*
// and tests/unit/task-46/*: the Vitest suites and the plain-node QA runner import
// the SAME accept/reject logic, so no surface can be validated more loosely than
// another. It reuses the parity harness primitives rather than re-deriving them.

import { isPlainObject, SCHEMA_DUMP_KEYS } from '../task-38/parity-harness-schema.mjs';

/**
 * Closed refusal taxonomy. Every rejection this schema can emit is one of these,
 * so a test asserts on `.reason` and never on message text.
 * @typedef {'UNKNOWN_CAPABILITY'|'UNKNOWN_PARAM'|'BROAD_EXPECTATION'|'MISSING_ORACLE'
 *   |'MISSING_CLEANUP'|'DEPENDENT_ORACLE'|'MUTATING_ORACLE'|'DUPLICATE_NAMESPACE'
 *   |'UNOWNED_TARGET'|'TIMEOUT_TIER_MISMATCH'|'REQUIREMENT_UNDERDECLARED'
 *   |'MISSING_ERROR_CODE'|'SCHEMA_DUMP'|'UNKNOWN_FIELD'|'MALFORMED'} CorpusRejectionReason
 */

/** @type {Readonly<Record<CorpusRejectionReason, CorpusRejectionReason>>} */
export const CORPUS_REASONS = Object.freeze({
  UNKNOWN_CAPABILITY: 'UNKNOWN_CAPABILITY',
  UNKNOWN_PARAM: 'UNKNOWN_PARAM',
  BROAD_EXPECTATION: 'BROAD_EXPECTATION',
  MISSING_ORACLE: 'MISSING_ORACLE',
  MISSING_CLEANUP: 'MISSING_CLEANUP',
  DEPENDENT_ORACLE: 'DEPENDENT_ORACLE',
  MUTATING_ORACLE: 'MUTATING_ORACLE',
  DUPLICATE_NAMESPACE: 'DUPLICATE_NAMESPACE',
  UNOWNED_TARGET: 'UNOWNED_TARGET',
  TIMEOUT_TIER_MISMATCH: 'TIMEOUT_TIER_MISMATCH',
  REQUIREMENT_UNDERDECLARED: 'REQUIREMENT_UNDERDECLARED',
  MISSING_ERROR_CODE: 'MISSING_ERROR_CODE',
  SCHEMA_DUMP: 'SCHEMA_DUMP',
  UNKNOWN_FIELD: 'UNKNOWN_FIELD',
  MALFORMED: 'MALFORMED',
});

/** A precise, typed corpus refusal carrying the closed reason and a JSON pointer. */
export class CorpusRejection extends Error {
  /** @param {CorpusRejectionReason} reason @param {string} pointer @param {string} detail */
  constructor(reason, pointer, detail) {
    super(`${reason} at ${pointer || '/'}: ${detail}`);
    this.name = 'CorpusRejection';
    /** @type {CorpusRejectionReason} */
    this.reason = reason;
    this.pointer = pointer || '/';
    this.detail = detail;
  }
}

/** @param {CorpusRejectionReason} reason @param {string} pointer @param {string} detail @returns {never} */
export function refuse(reason, pointer, detail) {
  throw new CorpusRejection(reason, pointer, detail);
}

/** The gateway operations a scenario may exercise. Closed: the four the gateway has. */
export const PRIMITIVES = Object.freeze(['search', 'describe', 'execute', 'configure']);

/** The MCP-level augmentations a scenario may layer on top of its gateway call. */
export const PROTOCOL_KINDS = Object.freeze(['progress', 'task', 'cancel']);

/** The two execute request forms. Neither wins by precedence; both must be covered. */
export const EXECUTE_FORMS = Object.freeze(['canonical', 'legacy']);

/** The transports a scenario may declare it can run on. */
export const CLIENTS = Object.freeze(['stdio', 'native']);

/** Timeout tiers, mirroring `cost.latency` in the canonical registry. */
export const TIMEOUT_TIERS = Object.freeze(['instant', 'interactive', 'long-running']);

/**
 * Per-tier client deadlines. A tier is not decoration: it is the deadline the
 * driver arms, so a scenario cannot quietly buy itself an unbounded wait to hide
 * a queue stall. Measured against UE 5.7.4 in the Task 42/43 probes: a cold
 * asset.list is ~4s, warm ~60ms; a queued destructive delete needs real headroom.
 * @type {Readonly<Record<string, number>>}
 */
export const TIER_TIMEOUT_MS = Object.freeze({
  instant: 30_000,
  interactive: 90_000,
  'long-running': 240_000,
});

/** The ONLY content root a Task 49 scenario may mutate. Everything else is unowned. */
export const OWNED_ROOT = '/Game/MCPTest';

/** Any string that addresses engine content. Used to catch a mutation aimed outside OWNED_ROOT. */
const CONTENT_PATH = /^\/(?:Game|Engine|Script|Temp|Niagara)\b/u;

const NAMESPACE_PATTERN = /^task49\.[a-z0-9]+(?:[.-][a-z0-9]+)*$/u;

/** The three primary intents. The FIRST token of an expectation must be one of these. */
export const PRIMARY_INTENTS = Object.freeze(['success', 'error', 'timeout']);

/** Wildcard-ish tokens that make an expectation unfalsifiable regardless of position. */
const WILDCARD_TOKENS = Object.freeze(['*', 'any', 'anything', '.*', '~']);

const SCENARIO_FIELDS = Object.freeze([
  'namespace', 'title', 'primitive', 'form', 'capability', 'discovery', 'request',
  'protocol', 'expected', 'expectedErrorCode', 'setup', 'oracle', 'cleanup',
  'requires', 'timeoutTier', 'ownedPath',
]);
const REQUEST_FIELDS = Object.freeze(['params', 'options', 'consent']);
const DISCOVERY_FIELDS = Object.freeze(['query', 'tool', 'action', 'param', 'domain', 'family', 'limit']);
const PROTOCOL_FIELDS = Object.freeze(['kind', 'progressToken', 'taskTtlMs', 'cancelAfterMs']);
const STEP_FIELDS = Object.freeze(['capability', 'params', 'consent', 'tolerateFailure']);
const ORACLE_FIELDS = Object.freeze(['capability', 'params', 'expect', 'needle', 'attempts', 'intervalMs']);
const REQUIRES_FIELDS = Object.freeze(['unrealMin', 'plugins', 'editorStates', 'clients']);

/**
 * Closed-object guard raising a corpus-taxonomy UNKNOWN_FIELD. The parity harness
 * has an equivalent, but it throws a HarnessRejection; a corpus consumer must be
 * able to catch exactly one error type, so the guard is restated over the shared
 * `isPlainObject` primitive rather than the shared thrower.
 * @param {unknown} value @param {readonly string[]} allowed @param {string} pointer
 * @returns {Record<string, unknown>}
 */
export function closed(value, allowed, pointer) {
  if (!isPlainObject(value)) refuse(CORPUS_REASONS.MALFORMED, pointer, 'expected a plain object');
  const object = /** @type {Record<string, unknown>} */ (value);
  for (const key of Object.keys(object)) {
    if (!allowed.includes(key)) {
      refuse(CORPUS_REASONS.UNKNOWN_FIELD, `${pointer}/${key}`, `field "${key}" is not in the closed schema {${allowed.join(', ')}}`);
    }
  }
  return object;
}

/** @param {unknown} value @param {string} pointer @param {string} what @returns {string} */
function requireString(value, pointer, what) {
  if (typeof value !== 'string' || value.length === 0) refuse(CORPUS_REASONS.MALFORMED, pointer, `${what} must be a non-empty string`);
  return /** @type {string} */ (value);
}

/** @param {unknown} value @param {readonly string[]} allowed @param {string} pointer @returns {string} */
function requireEnum(value, allowed, pointer) {
  const text = requireString(value, pointer, 'value');
  if (!allowed.includes(text)) refuse(CORPUS_REASONS.MALFORMED, pointer, `"${text}" is not one of {${allowed.join(', ')}}`);
  return text;
}

/**
 * Parse and police an expectation string against the project grammar
 * (tests/AGENTS.md): split on `|` or ` or `; the FIRST token is the primary intent
 * and must be success/error/timeout.
 *
 * Two masks are refused because both make a case unfalsifiable:
 *   `success|error`  — every outcome satisfies it
 *   `error|timeout`  — a hang is laundered into a pass; a timeout is admissible
 *                      ONLY as the primary condition, never as a fallback
 * The single rule that covers both: a primary-intent token may appear only first.
 *
 * @param {unknown} raw @param {string} pointer
 * @returns {{ intent: string, alternatives: string[], text: string }}
 */
export function parseExpectation(raw, pointer) {
  const text = requireString(raw, pointer, 'expected');
  const tokens = (text.includes(' or ') ? text.split(' or ') : text.split('|'))
    .map((token) => token.trim())
    .filter((token) => token.length > 0);
  if (tokens.length === 0) refuse(CORPUS_REASONS.BROAD_EXPECTATION, pointer, 'expectation is empty');

  const intent = /** @type {string} */ (tokens[0]).toLowerCase();
  if (!PRIMARY_INTENTS.includes(intent)) {
    refuse(CORPUS_REASONS.BROAD_EXPECTATION, pointer,
      `the first token must be the primary intent (${PRIMARY_INTENTS.join('/')}), got "${tokens[0]}"`);
  }

  const alternatives = tokens.slice(1);
  for (const alternative of alternatives) {
    const lowered = alternative.toLowerCase();
    if (PRIMARY_INTENTS.includes(lowered)) {
      refuse(CORPUS_REASONS.BROAD_EXPECTATION, pointer,
        `"${alternative}" is a primary intent and may appear only first; "${text}" is a broad mask that no outcome can fail`);
    }
    if (WILDCARD_TOKENS.some((token) => lowered === token || lowered.includes(token))) {
      refuse(CORPUS_REASONS.BROAD_EXPECTATION, pointer, `wildcard alternative "${alternative}" makes "${text}" unfalsifiable`);
    }
  }
  return { intent, alternatives, text };
}

/** @param {{ major: number, minor: number, patch: number }} version */
const versionTuple = (version) => [version.major, version.minor, version.patch];

/** @param {string} text @param {string} pointer @returns {number[]} */
function parseVersion(text, pointer) {
  const parts = text.split('.').map((part) => Number(part));
  if (parts.length !== 3 || parts.some((part) => !Number.isInteger(part) || part < 0)) {
    refuse(CORPUS_REASONS.MALFORMED, pointer, `"${text}" is not a major.minor.patch version`);
  }
  return parts;
}

/** @param {number[]} left @param {number[]} right */
function compareVersions(left, right) {
  for (let i = 0; i < 3; i += 1) {
    const diff = (left[i] ?? 0) - (right[i] ?? 0);
    if (diff !== 0) return diff;
  }
  return 0;
}

/**
 * Build the id -> record index the validator resolves capabilities against.
 * @param {readonly {id: string}[]} records
 * @returns {Map<string, any>}
 */
export function indexRecords(records) {
  return new Map(records.map((record) => [record.id, record]));
}

/**
 * Resolve a capability id or refuse. This is the gate that stops a corpus from
 * silently containing a case that can never run: an id no registry knows is not
 * a failing test, it is an absent one.
 * @param {Map<string, any>} index @param {unknown} raw @param {string} pointer
 */
function resolveCapability(index, raw, pointer) {
  const id = requireString(raw, pointer, 'capability');
  const record = index.get(id);
  if (record === undefined) {
    refuse(CORPUS_REASONS.UNKNOWN_CAPABILITY, pointer, `"${id}" is not in the canonical registry`);
  }
  return record;
}

/**
 * Every param key must be declared by the capability's own input schema, and no
 * key may be a schema-dump key. A typo'd param is otherwise dropped or defaulted
 * by the server and the case proves something other than what it says.
 * @param {any} record @param {unknown} raw @param {string} pointer
 * @returns {Record<string, unknown>}
 */
function validateParams(record, raw, pointer) {
  if (raw === undefined) return {};
  if (!isPlainObject(raw)) refuse(CORPUS_REASONS.MALFORMED, pointer, 'params must be a plain object');
  const params = /** @type {Record<string, unknown>} */ (raw);
  const declared = Object.keys(record?.schemas?.input?.properties ?? {});
  for (const key of Object.keys(params)) {
    if (SCHEMA_DUMP_KEYS.includes(key.toLowerCase())) {
      refuse(CORPUS_REASONS.SCHEMA_DUMP, `${pointer}/${key}`, `"${key}" leaks a schema dump into a live request`);
    }
    if (!declared.includes(key)) {
      refuse(CORPUS_REASONS.UNKNOWN_PARAM, `${pointer}/${key}`,
        `"${key}" is not a parameter of ${record.id}; declared: {${declared.join(', ')}}`);
    }
  }
  return params;
}

/**
 * Every engine path a step touches must live under the scenario's owned prefix.
 * A live corpus that can address `/Game/` at large is one typo away from deleting
 * a real project asset, and no amount of cleanup discipline undoes that.
 * @param {Record<string, unknown>} params @param {string} ownedPath @param {string} pointer
 */
function assertOwnedTargets(params, ownedPath, pointer) {
  for (const [key, value] of Object.entries(params)) {
    const candidates = typeof value === 'string' ? [value] : (Array.isArray(value) ? value : []);
    for (const candidate of candidates) {
      if (typeof candidate !== 'string' || !CONTENT_PATH.test(candidate)) continue;
      if (candidate !== ownedPath && !candidate.startsWith(`${ownedPath}/`)) {
        refuse(CORPUS_REASONS.UNOWNED_TARGET, `${pointer}/${key}`,
          `"${candidate}" is outside the scenario's owned namespace "${ownedPath}"`);
      }
    }
  }
}

/**
 * @param {Map<string, any>} index @param {unknown} raw @param {string} pointer
 * @param {string} ownedPath @param {boolean} enforceOwnership
 */
function validateStep(index, raw, pointer, ownedPath, enforceOwnership) {
  const step = closed(raw, STEP_FIELDS, pointer);
  const record = resolveCapability(index, step.capability, `${pointer}/capability`);
  const params = validateParams(record, step.params, `${pointer}/params`);
  if (enforceOwnership) assertOwnedTargets(params, ownedPath, `${pointer}/params`);
  const consent = step.consent === undefined ? null : closed(step.consent, ['capability', 'acknowledge'], `${pointer}/consent`);
  return {
    capability: record.id,
    params,
    consent,
    tolerateFailure: step.tolerateFailure === true,
    record,
  };
}

/**
 * The INDEPENDENT ORACLE. This is the heart of the corpus and the strictest part
 * of the schema.
 *
 * Task 42 proved a refusal with five independent `asset.list` reads; Task 43
 * proved absence in BOTH polarities. The rules below are what make that standard
 * structural rather than a habit:
 *   - the oracle may not be the capability under test (DEPENDENT_ORACLE) — a
 *     mutation checked by re-running the mutation proves nothing
 *   - the oracle must be a `read` capability (MUTATING_ORACLE) — an observation
 *     that changes the thing observed is not an observation
 *   - it declares a polarity (`present`/`absent`) and a needle, so the negative
 *     direction is a first-class outcome and not the absence of a positive one
 * @param {Map<string, any>} index @param {unknown} raw @param {string} pointer
 * @param {string|null} capabilityUnderTest @param {string} ownedPath
 */
function validateOracle(index, raw, pointer, capabilityUnderTest, ownedPath) {
  const oracle = closed(raw, ORACLE_FIELDS, pointer);
  const record = resolveCapability(index, oracle.capability, `${pointer}/capability`);
  if (capabilityUnderTest !== null && record.id === capabilityUnderTest) {
    refuse(CORPUS_REASONS.DEPENDENT_ORACLE, `${pointer}/capability`,
      `the oracle re-runs "${record.id}", the capability under test; a mutation must be verified by a SEPARATE read`);
  }
  if (record.behavior?.effect !== 'read') {
    refuse(CORPUS_REASONS.MUTATING_ORACLE, `${pointer}/capability`,
      `oracle "${record.id}" has effect "${record.behavior?.effect}"; an oracle must be a read capability`);
  }
  const params = validateParams(record, oracle.params, `${pointer}/params`);
  assertOwnedTargets(params, ownedPath, `${pointer}/params`);
  const expect = requireEnum(oracle.expect, ['present', 'absent'], `${pointer}/expect`);
  const needle = requireString(oracle.needle, `${pointer}/needle`, 'needle');
  const attempts = oracle.attempts === undefined ? 5 : oracle.attempts;
  if (!Number.isInteger(attempts) || Number(attempts) < 1 || Number(attempts) > 20) {
    refuse(CORPUS_REASONS.MALFORMED, `${pointer}/attempts`, 'attempts must be an integer in [1, 20]');
  }
  const intervalMs = oracle.intervalMs === undefined ? 1500 : oracle.intervalMs;
  if (!Number.isInteger(intervalMs) || Number(intervalMs) < 0 || Number(intervalMs) > 10_000) {
    refuse(CORPUS_REASONS.MALFORMED, `${pointer}/intervalMs`, 'intervalMs must be an integer in [0, 10000]');
  }
  return { capability: record.id, params, expect, needle, attempts: Number(attempts), intervalMs: Number(intervalMs), record };
}

/**
 * Engine/plugin/client requirements. Validated as a SUPERSET of what the registry
 * declares: a scenario may be more conservative than the contract, never less. An
 * under-declared requirement is how a suite claims coverage on an engine where the
 * capability could not have run.
 * @param {unknown} raw @param {string} pointer @param {any|null} record
 */
function validateRequires(raw, pointer, record) {
  const requires = closed(raw, REQUIRES_FIELDS, pointer);
  const unrealMin = requireString(requires.unrealMin, `${pointer}/unrealMin`, 'unrealMin');
  const declaredVersion = parseVersion(unrealMin, `${pointer}/unrealMin`);
  const plugins = Array.isArray(requires.plugins) ? requires.plugins.map(String) : null;
  if (plugins === null) refuse(CORPUS_REASONS.MALFORMED, `${pointer}/plugins`, 'plugins must be an array');
  const editorStates = Array.isArray(requires.editorStates) ? requires.editorStates.map(String) : null;
  if (editorStates === null) refuse(CORPUS_REASONS.MALFORMED, `${pointer}/editorStates`, 'editorStates must be an array');
  const clients = Array.isArray(requires.clients) ? requires.clients.map(String) : null;
  if (clients === null || clients.length === 0) refuse(CORPUS_REASONS.MALFORMED, `${pointer}/clients`, 'clients must be a non-empty array');
  for (const client of /** @type {string[]} */ (clients)) requireEnum(client, CLIENTS, `${pointer}/clients`);

  if (record !== null) {
    const contractMin = record.availability?.unreal?.min;
    if (contractMin && compareVersions(declaredVersion, versionTuple(contractMin)) < 0) {
      refuse(CORPUS_REASONS.REQUIREMENT_UNDERDECLARED, `${pointer}/unrealMin`,
        `${record.id} needs UE >= ${versionTuple(contractMin).join('.')}, scenario declares ${unrealMin}`);
    }
    for (const plugin of record.availability?.requiredPlugins ?? []) {
      if (!(/** @type {string[]} */ (plugins)).includes(plugin)) {
        refuse(CORPUS_REASONS.REQUIREMENT_UNDERDECLARED, `${pointer}/plugins`,
          `${record.id} needs plugin "${plugin}", which the scenario does not declare`);
      }
    }
  }
  return {
    unrealMin,
    plugins: /** @type {string[]} */ (plugins),
    editorStates: /** @type {string[]} */ (editorStates),
    clients: /** @type {string[]} */ (clients),
  };
}

/** @param {unknown} raw @param {string} pointer */
function validateProtocol(raw, pointer) {
  if (raw === null || raw === undefined) return null;
  const protocol = closed(raw, PROTOCOL_FIELDS, pointer);
  const kind = requireEnum(protocol.kind, PROTOCOL_KINDS, `${pointer}/kind`);
  return {
    kind,
    progressToken: typeof protocol.progressToken === 'string' ? protocol.progressToken : null,
    taskTtlMs: typeof protocol.taskTtlMs === 'number' ? protocol.taskTtlMs : null,
    cancelAfterMs: typeof protocol.cancelAfterMs === 'number' ? protocol.cancelAfterMs : null,
  };
}

/**
 * Validate ONE scenario into a normalized, executable shape. After this returns,
 * a driver holds a value it never re-validates.
 * @param {unknown} raw
 * @param {{ index: Map<string, any>, pointer?: string }} context
 */
export function validateScenario(raw, context) {
  const pointer = context.pointer ?? '/';
  const index = context.index;
  const scenario = closed(raw, SCENARIO_FIELDS, pointer);

  const namespace = requireString(scenario.namespace, `${pointer}/namespace`, 'namespace');
  if (!NAMESPACE_PATTERN.test(namespace)) {
    refuse(CORPUS_REASONS.MALFORMED, `${pointer}/namespace`,
      `"${namespace}" must match ${String(NAMESPACE_PATTERN)} so every scenario owns a distinct, greppable slice`);
  }
  const title = requireString(scenario.title, `${pointer}/title`, 'title');
  const primitive = requireEnum(scenario.primitive, PRIMITIVES, `${pointer}/primitive`);

  const ownedPath = requireString(scenario.ownedPath, `${pointer}/ownedPath`, 'ownedPath');
  if (!ownedPath.startsWith(`${OWNED_ROOT}/`)) {
    refuse(CORPUS_REASONS.UNOWNED_TARGET, `${pointer}/ownedPath`,
      `ownedPath "${ownedPath}" must live under ${OWNED_ROOT}/`);
  }

  /** @type {any|null} */
  let record = null;
  let form = null;
  if (primitive === 'execute') {
    record = resolveCapability(index, scenario.capability, `${pointer}/capability`);
    form = requireEnum(scenario.form, EXECUTE_FORMS, `${pointer}/form`);
  } else {
    if (scenario.capability !== undefined && scenario.capability !== null) {
      refuse(CORPUS_REASONS.MALFORMED, `${pointer}/capability`, `only an execute scenario names a capability; "${primitive}" must not`);
    }
    if (scenario.form !== undefined && scenario.form !== null) {
      refuse(CORPUS_REASONS.MALFORMED, `${pointer}/form`, `only an execute scenario has a request form; "${primitive}" must not`);
    }
  }

  const request = closed(scenario.request ?? {}, REQUEST_FIELDS, `${pointer}/request`);
  const params = record === null
    ? (isPlainObject(request.params) ? /** @type {Record<string, unknown>} */ (request.params) : {})
    : validateParams(record, request.params, `${pointer}/request/params`);
  const options = request.options === undefined ? null
    : closed(request.options, ['preview', 'idempotencyKey', 'expectedRevisions', 'timeoutMs', 'dryRun'], `${pointer}/request/options`);
  const consent = request.consent === undefined ? null
    : closed(request.consent, ['capability', 'acknowledge'], `${pointer}/request/consent`);

  const discovery = scenario.discovery === undefined ? null
    : closed(scenario.discovery, DISCOVERY_FIELDS, `${pointer}/discovery`);

  const protocol = validateProtocol(scenario.protocol, `${pointer}/protocol`);

  const expectation = parseExpectation(scenario.expected, `${pointer}/expected`);
  const expectedErrorCode = scenario.expectedErrorCode === undefined || scenario.expectedErrorCode === null
    ? null
    : requireString(scenario.expectedErrorCode, `${pointer}/expectedErrorCode`, 'expectedErrorCode');
  if (expectation.intent === 'error' && expectedErrorCode === null) {
    // "error" alone would pass on ANY refusal, including one caused by the probe
    // misconfiguring itself. Naming the code is what makes the case falsifiable.
    refuse(CORPUS_REASONS.MISSING_ERROR_CODE, `${pointer}/expectedErrorCode`,
      'an error-primary scenario must name the exact typed error code it expects');
  }
  if (expectedErrorCode !== null && !/^[A-Z][A-Z0-9_]*$/u.test(expectedErrorCode)) {
    refuse(CORPUS_REASONS.MALFORMED, `${pointer}/expectedErrorCode`,
      `"${expectedErrorCode}" is not an UPPER_SNAKE typed code`);
  }

  const timeoutTier = requireEnum(scenario.timeoutTier, TIMEOUT_TIERS, `${pointer}/timeoutTier`);
  if (record !== null && record.cost?.latency !== timeoutTier) {
    refuse(CORPUS_REASONS.TIMEOUT_TIER_MISMATCH, `${pointer}/timeoutTier`,
      `${record.id} is contract latency "${record.cost?.latency}" but the scenario declares tier "${timeoutTier}"; ` +
      'a tier is the armed deadline, so a mismatch either hides a stall or invents a flake');
  }

  const setupRaw = scenario.setup === undefined ? [] : scenario.setup;
  if (!Array.isArray(setupRaw)) refuse(CORPUS_REASONS.MALFORMED, `${pointer}/setup`, 'setup must be an array');
  const setup = setupRaw.map((step, i) => validateStep(index, step, `${pointer}/setup/${i}`, ownedPath, true));

  const cleanupRaw = scenario.cleanup === undefined ? [] : scenario.cleanup;
  if (!Array.isArray(cleanupRaw)) refuse(CORPUS_REASONS.MALFORMED, `${pointer}/cleanup`, 'cleanup must be an array');
  const cleanup = cleanupRaw.map((step, i) => validateStep(index, step, `${pointer}/cleanup/${i}`, ownedPath, true));

  // A scenario MUTATES when the capability under test writes, or when any setup
  // step writes. Setup counts: Task 46's own drain test asserted a container
  // drained to zero when it had never been populated — green, and proving nothing.
  // Seeded state is state, and it has to be observed and removed like any other.
  const effect = record?.behavior?.effect ?? 'read';
  const setupMutates = setup.some((step) => step.record.behavior?.effect !== 'read');
  const mutates = effect !== 'read' || setupMutates;

  if (scenario.oracle !== undefined && scenario.oracle !== null) {
    if (!isPlainObject(scenario.oracle)) refuse(CORPUS_REASONS.MALFORMED, `${pointer}/oracle`, 'oracle must be a plain object');
  }
  const hasOracle = scenario.oracle !== undefined && scenario.oracle !== null;
  if (mutates && !hasOracle) {
    refuse(CORPUS_REASONS.MISSING_ORACLE, `${pointer}/oracle`,
      `this scenario mutates (effect="${effect}"${setupMutates ? ', setup writes' : ''}) but declares no independent oracle; ` +
      'the mutating call\'s own response is not proof');
  }
  const oracle = hasOracle ? validateOracle(index, scenario.oracle, `${pointer}/oracle`, record?.id ?? null, ownedPath) : null;

  // Ownership is enforced on the request only for a MUTATING scenario. A read may
  // legitimately address `/Game` at large — an oracle listing the parent folder is
  // exactly how Task 42 proved absence — but nothing may WRITE outside its own
  // namespace, because a typo'd destructive path is not recoverable by cleanup.
  if (effect !== 'read') assertOwnedTargets(params, ownedPath, `${pointer}/request/params`);

  if (mutates && cleanup.length === 0) {
    refuse(CORPUS_REASONS.MISSING_CLEANUP, `${pointer}/cleanup`,
      'a mutating scenario must declare cleanup; cleanup is an acceptance criterion, not hygiene');
  }

  const requires = validateRequires(scenario.requires, `${pointer}/requires`, record);

  return Object.freeze({
    namespace, title, primitive, form, capability: record?.id ?? null, record,
    discovery, params, options, consent, protocol,
    expected: expectation, expectedErrorCode,
    setup, oracle, cleanup, requires, timeoutTier,
    timeoutMs: TIER_TIMEOUT_MS[timeoutTier] ?? TIER_TIMEOUT_MS.interactive,
    ownedPath, mutates,
  });
}

/** @typedef {ReturnType<typeof validateScenario>} Scenario */

/**
 * Validate a whole corpus. Namespace uniqueness is enforced HERE rather than per
 * scenario, because two scenarios sharing a namespace share an owned content path:
 * one's cleanup deletes the other's fixture and the loser reads as a flake.
 * @param {readonly unknown[]} rawScenarios
 * @param {{ index: Map<string, any> }} context
 * @returns {Scenario[]}
 */
export function validateCorpus(rawScenarios, context) {
  if (!Array.isArray(rawScenarios)) refuse(CORPUS_REASONS.MALFORMED, '/', 'a corpus must be an array of scenarios');
  /** @type {Map<string, number>} */
  const seenNamespaces = new Map();
  /** @type {Map<string, number>} */
  const seenPaths = new Map();
  /** @type {Scenario[]} */
  const scenarios = [];
  rawScenarios.forEach((raw, i) => {
    const pointer = `/${i}`;
    const scenario = validateScenario(raw, { index: context.index, pointer });
    const priorNamespace = seenNamespaces.get(scenario.namespace);
    if (priorNamespace !== undefined) {
      refuse(CORPUS_REASONS.DUPLICATE_NAMESPACE, `${pointer}/namespace`,
        `namespace "${scenario.namespace}" is already used by scenario ${priorNamespace}`);
    }
    seenNamespaces.set(scenario.namespace, i);
    const priorPath = seenPaths.get(scenario.ownedPath);
    if (priorPath !== undefined) {
      refuse(CORPUS_REASONS.DUPLICATE_NAMESPACE, `${pointer}/ownedPath`,
        `ownedPath "${scenario.ownedPath}" is already owned by scenario ${priorPath}; two scenarios cannot own one content path`);
    }
    seenPaths.set(scenario.ownedPath, i);
    scenarios.push(scenario);
  });
  return scenarios;
}
