// @ts-check
// tests/unit/live-drivers/live-corpus.mjs
// Task 49 — THE transport-neutral live scenario corpus.
//
// One corpus, two drivers. Nothing in this file knows about HTTP, SSE, stdin or
// WebSockets: a scenario declares WHAT to ask and HOW the answer will be proven,
// and live-driver-native.mjs / live-driver-stdio.mjs decide how to put it on a
// wire. That is the whole point — the eight probes in scripts/qa/ each re-invented
// both halves, so a semantic difference between two of them was indistinguishable
// from two authors reading a response differently.
//
// Every scenario here passes live-corpus-schema.mjs, which means each one:
//   - names a capability and parameters that exist in the canonical registry
//   - carries a narrow expectation (no `success|error`), with an exact typed code
//     when the primary intent is `error`
//   - if it mutates, is proven by an INDEPENDENT read and removes what it made
//   - owns a unique namespace and a unique `/Game/MCPTest/...` content path
//
// TWO STRUCTURAL HABITS FROM THIS PLAN ARE ENCODED DELIBERATELY:
//
// 1. POSITIVE CONTROL FOR EVERY NEGATIVE. `execute.canonical-mutation` proves the
//    asset.list oracle can read `present`. Without it, `execute.consent-refused`
//    (which proves `absent`) would pass identically against a blind oracle that
//    can never see anything — and so would the whole suite against a server that
//    refuses everything.
//
// 2. BOTH POLARITIES, AS IN TASK 43. A refusal is proven by absence AND a real
//    mutation is proven by presence, using the SAME oracle capability, so neither
//    reading can be explained away by the oracle being broken in one direction.

import { validateCorpus, indexRecords } from './live-corpus-schema.mjs';
import { loadRecords } from '../cross-transport/matrix-dimensions.mjs';

/** Every scenario's content lives under a per-scenario child of this root. */
const ROOT = '/Game/MCPTest';

/** The plugin set every `manage_asset`/`material` capability declares. */
const ASSET_PLUGINS = Object.freeze(['EditorScriptingUtilities']);

/** Both transports unless a scenario documents why it is single-sided. */
const BOTH = Object.freeze(['stdio', 'native']);

/**
 * The raw corpus. Exported unvalidated so a test can prove the VALIDATOR accepts
 * it, rather than the corpus being trusted because it was written by the same
 * hand as the validator.
 * @type {readonly Record<string, unknown>[]}
 */
export const RAW_SCENARIOS = Object.freeze([
  // ───────────────────────────── gateway primitive: search ─────────────────────
  {
    namespace: 'task49.search.keyword',
    title: 'search returns ranked capabilities for a keyword without dumping schemas',
    primitive: 'search',
    ownedPath: `${ROOT}/task49-search-keyword`,
    discovery: { query: 'list assets in a content folder', limit: 5 },
    expected: 'success',
    timeoutTier: 'instant',
    requires: { unrealMin: '5.0.0', plugins: [], editorStates: ['edit'], clients: BOTH },
  },

  // ──────────────────────────── gateway primitive: describe ────────────────────
  {
    namespace: 'task49.describe.tool-summary',
    title: 'describe drills one level to a tool summary',
    primitive: 'describe',
    ownedPath: `${ROOT}/task49-describe-tool`,
    discovery: { tool: 'manage_asset' },
    expected: 'success',
    timeoutTier: 'instant',
    requires: { unrealMin: '5.0.0', plugins: [], editorStates: ['edit'], clients: BOTH },
  },
  {
    namespace: 'task49.describe.action-params',
    title: 'describe drills to the parameter catalogue for one action',
    primitive: 'describe',
    ownedPath: `${ROOT}/task49-describe-action`,
    discovery: { tool: 'manage_asset', action: 'list' },
    expected: 'success',
    timeoutTier: 'instant',
    requires: { unrealMin: '5.0.0', plugins: [], editorStates: ['edit'], clients: BOTH },
  },
  {
    // The wire-proven error case: Task 46's `error` dimension observed UNKNOWN_TOOL
    // on BOTH transports for exactly this shape. An unknown TOOL is expressible
    // here where an unknown CAPABILITY is not — the schema refuses to let a
    // scenario name a capability the registry does not have.
    namespace: 'task49.describe.unknown-tool',
    title: 'describe of a tool that does not exist is refused UNKNOWN_TOOL with guidance',
    primitive: 'describe',
    ownedPath: `${ROOT}/task49-describe-unknown`,
    discovery: { tool: 'not_a_real_tool' },
    expected: 'error',
    expectedErrorCode: 'UNKNOWN_TOOL',
    timeoutTier: 'instant',
    requires: { unrealMin: '5.0.0', plugins: [], editorStates: ['edit'], clients: BOTH },
  },

  // ─────────────────────────── gateway primitive: configure ────────────────────
  {
    namespace: 'task49.configure.get-status',
    title: 'configure wraps manage_tools and reports dynamic tool state',
    primitive: 'configure',
    ownedPath: `${ROOT}/task49-configure-status`,
    discovery: { action: 'get_status' },
    expected: 'success',
    timeoutTier: 'instant',
    requires: { unrealMin: '5.0.0', plugins: [], editorStates: ['edit'], clients: BOTH },
  },

  // ───────────────────── execute: CANONICAL form, read-only ────────────────────
  {
    namespace: 'task49.execute.canonical-read',
    title: 'canonical execute form { capability } performs a read',
    primitive: 'execute',
    form: 'canonical',
    capability: 'asset.list',
    ownedPath: `${ROOT}/task49-canonical-read`,
    request: { params: { path: '/Game' } },
    // `not found` is a legal narrow alternative under the project grammar: a
    // project whose /Game is empty is a valid editor, not a failed read.
    expected: 'success|not found',
    timeoutTier: 'interactive',
    requires: { unrealMin: '5.0.0', plugins: ASSET_PLUGINS, editorStates: ['edit'], clients: BOTH },
  },

  // ─────────────────────── execute: LEGACY form, read-only ─────────────────────
  {
    namespace: 'task49.execute.legacy-read',
    title: 'legacy execute form { tool, action } performs the SAME read',
    primitive: 'execute',
    form: 'legacy',
    capability: 'asset.list',
    ownedPath: `${ROOT}/task49-legacy-read`,
    request: { params: { path: '/Game' } },
    expected: 'success|not found',
    timeoutTier: 'interactive',
    requires: { unrealMin: '5.0.0', plugins: ASSET_PLUGINS, editorStates: ['edit'], clients: BOTH },
  },

  // ───────── execute: CANONICAL form, real mutation — the POSITIVE CONTROL ─────
  {
    namespace: 'task49.execute.canonical-mutation',
    title: 'canonical execute really creates an asset, proven present by an independent list',
    primitive: 'execute',
    form: 'canonical',
    capability: 'material.create_material',
    ownedPath: `${ROOT}/task49-canonical-mutation`,
    request: {
      params: { name: 'M_Task49Canonical', path: `${ROOT}/task49-canonical-mutation` },
      consent: { capability: 'material.create_material', acknowledge: 'explicit' },
    },
    expected: 'success|already exists',
    timeoutTier: 'interactive',
    oracle: {
      capability: 'asset.list',
      params: { path: `${ROOT}/task49-canonical-mutation` },
      expect: 'present',
      needle: 'M_Task49Canonical',
      attempts: 5,
      intervalMs: 1500,
    },
    cleanup: [{
      capability: 'asset.delete',
      params: { paths: [`${ROOT}/task49-canonical-mutation/M_Task49Canonical`] },
      consent: { capability: 'asset.delete', acknowledge: 'elevated' },
      tolerateFailure: true,
    }],
    requires: { unrealMin: '5.0.0', plugins: ASSET_PLUGINS, editorStates: ['edit'], clients: BOTH },
  },

  // ─────────── execute: LEGACY form, real mutation — same semantics ────────────
  {
    namespace: 'task49.execute.legacy-mutation',
    title: 'legacy execute really creates an asset with the same semantics as canonical',
    primitive: 'execute',
    form: 'legacy',
    capability: 'material.create_material',
    ownedPath: `${ROOT}/task49-legacy-mutation`,
    request: {
      params: { name: 'M_Task49Legacy', path: `${ROOT}/task49-legacy-mutation` },
      consent: { capability: 'material.create_material', acknowledge: 'explicit' },
    },
    expected: 'success|already exists',
    timeoutTier: 'interactive',
    oracle: {
      capability: 'asset.list',
      params: { path: `${ROOT}/task49-legacy-mutation` },
      expect: 'present',
      needle: 'M_Task49Legacy',
      attempts: 5,
      intervalMs: 1500,
    },
    cleanup: [{
      capability: 'asset.delete',
      params: { paths: [`${ROOT}/task49-legacy-mutation/M_Task49Legacy`] },
      consent: { capability: 'asset.delete', acknowledge: 'elevated' },
      tolerateFailure: true,
    }],
    requires: { unrealMin: '5.0.0', plugins: ASSET_PLUGINS, editorStates: ['edit'], clients: BOTH },
  },

  // ──────── execute: consent refusal — the NEGATIVE, proven in the ABSENT polarity ───
  {
    // Task 46 observed CONSENT_REQUIRED on both transports for a consent-gated
    // capability called with no grant. Here the refusal is not believed from the
    // response: the oracle must independently see that NOTHING was created. The
    // cleanup step still exists and tolerates failure — if the refusal ever stops
    // holding, this is what stops the leak becoming permanent.
    namespace: 'task49.execute.consent-refused',
    title: 'a consent-gated mutation with no grant is refused AND creates nothing',
    primitive: 'execute',
    form: 'canonical',
    capability: 'material.create_material',
    ownedPath: `${ROOT}/task49-consent-refused`,
    request: {
      params: { name: 'M_Task49ConsentRefused', path: `${ROOT}/task49-consent-refused` },
    },
    expected: 'error',
    expectedErrorCode: 'CONSENT_REQUIRED',
    timeoutTier: 'interactive',
    oracle: {
      capability: 'asset.list',
      params: { path: `${ROOT}/task49-consent-refused` },
      expect: 'absent',
      needle: 'M_Task49ConsentRefused',
      attempts: 5,
      intervalMs: 1500,
    },
    cleanup: [{
      capability: 'asset.delete',
      params: { paths: [`${ROOT}/task49-consent-refused/M_Task49ConsentRefused`] },
      consent: { capability: 'asset.delete', acknowledge: 'elevated' },
      tolerateFailure: true,
    }],
    requires: { unrealMin: '5.0.0', plugins: ASSET_PLUGINS, editorStates: ['edit'], clients: BOTH },
  },

  // ─────────────────────────────── progress ────────────────────────────────────
  {
    // What is actually falsifiable here is NOT "progress was emitted" — a fast
    // read may legitimately emit none. It is that the server never INVENTS a
    // progress token the client did not supply, which is the contract a client
    // relies on to correlate. Task 46 gated the same property.
    namespace: 'task49.progress.token-not-invented',
    title: 'a client-supplied progressToken is honoured and no foreign token is invented',
    primitive: 'execute',
    form: 'canonical',
    capability: 'asset.list',
    ownedPath: `${ROOT}/task49-progress`,
    request: { params: { path: '/Game' } },
    protocol: { kind: 'progress', progressToken: 'task49-progress-token' },
    expected: 'success|not found',
    timeoutTier: 'interactive',
    requires: { unrealMin: '5.0.0', plugins: ASSET_PLUGINS, editorStates: ['edit'], clients: BOTH },
  },

  // ───────────────────────────────── tasks ─────────────────────────────────────
  {
    namespace: 'task49.task.search-checkpoint',
    title: 'a task-augmented search settles and its stored result is retrievable',
    primitive: 'search',
    ownedPath: `${ROOT}/task49-task-checkpoint`,
    discovery: { query: 'list assets', limit: 3 },
    protocol: { kind: 'task', taskTtlMs: 60_000 },
    expected: 'success',
    timeoutTier: 'interactive',
    requires: { unrealMin: '5.0.0', plugins: [], editorStates: ['edit'], clients: BOTH },
  },
  {
    // The matching NEGATIVE. Mutating operations are deliberately not offered as
    // tasks, because cancelling a task cannot recall work already dispatched to
    // the game thread. Scoped to stdio: the refusal text and code are the
    // TypeScript seam's (task-checkpoint.ts); whether the native surface refuses
    // identically is Task 46's parity question, not this corpus's, and claiming
    // it here without having watched it would be exactly the overclaim this plan
    // keeps catching.
    namespace: 'task49.task.execute-refused',
    title: 'a task-augmented EXECUTE is refused as not checkpointable',
    primitive: 'execute',
    form: 'canonical',
    capability: 'asset.list',
    ownedPath: `${ROOT}/task49-task-refused`,
    request: { params: { path: '/Game' } },
    protocol: { kind: 'task', taskTtlMs: 60_000 },
    expected: 'error',
    expectedErrorCode: 'TASK_CHECKPOINT_NOT_AVAILABLE',
    timeoutTier: 'interactive',
    requires: { unrealMin: '5.0.0', plugins: ASSET_PLUGINS, editorStates: ['edit'], clients: ['stdio'] },
  },

  // ─────────────────────────────── cancellation ────────────────────────────────
  {
    // Cancellation is ADVISORY: work already handed to the game thread cannot be
    // recalled, so "the operation stopped" is not a checkable claim. What IS
    // checkable, and what a client actually depends on, is that sending
    // notifications/cancelled never wedges the connection. The runner proves it
    // with an independent follow-up call on the SAME session after the cancel.
    namespace: 'task49.cancel.does-not-wedge',
    title: 'notifications/cancelled for an in-flight request leaves the transport usable',
    primitive: 'execute',
    form: 'canonical',
    capability: 'asset.list',
    ownedPath: `${ROOT}/task49-cancel`,
    request: { params: { path: '/Game' } },
    protocol: { kind: 'cancel', cancelAfterMs: 50 },
    expected: 'success',
    timeoutTier: 'interactive',
    requires: { unrealMin: '5.0.0', plugins: ASSET_PLUGINS, editorStates: ['edit'], clients: BOTH },
  },
]);

/** Build the validated corpus against the canonical registry. Throws CorpusRejection. */
export function buildCorpus(root = process.cwd()) {
  const index = indexRecords(loadRecords(root));
  return validateCorpus(RAW_SCENARIOS, { index });
}

/**
 * Coverage the corpus claims. Asserted by a test, so a scenario deleted to make a
 * run green also deletes the claim — the suite cannot silently narrow itself.
 */
export const REQUIRED_COVERAGE = Object.freeze({
  primitives: Object.freeze(['search', 'describe', 'execute', 'configure']),
  protocolKinds: Object.freeze(['progress', 'task', 'cancel']),
  executeForms: Object.freeze(['canonical', 'legacy']),
  oraclePolarities: Object.freeze(['present', 'absent']),
});
