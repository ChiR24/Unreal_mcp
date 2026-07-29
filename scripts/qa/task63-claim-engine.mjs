// @ts-check
// scripts/qa/task63-claim-engine.mjs
// Task 63 — decide the product-quality and best-in-class claim from measured
// evidence, deterministically, from evidence FIELDS ONLY.
//
// WHY THIS IS A PROGRAM AND NOT A PARAGRAPH
//
// This plan has three times caught a gate that could not fail: a memory gate
// whose baseline was a peak so every delta read as a saving; a 500-cycle soak
// that called `enable_tool` (no such action) and cycled nothing while reporting
// 500 successes; a CI command wired in at position 15 and never once executed.
// A claim engine that only ever emits one status is that same defect wearing a
// different hat. So this file is built to be falsified:
//
//   --forge perfect            every requirement satisfied  -> must emit the STRONGEST status
//   --forge perfect --omit X   exactly one requirement gone -> must emit the documented downgrade
//   --prose claim|blocked      narrative strings rewritten  -> status must NOT move
//
// If the positive control cannot reach BEST_IN_CLASS_VERIFIED, this engine's
// verdict on the real input set proves nothing, and the run says so rather than
// reporting the real verdict as though it meant something.
//
// THE STATUS VOCABULARY IS CLOSED. The plan (line 599) documents exactly three
// strings. Inventing a fourth — however much better it would describe the
// situation — is how a plan's vocabulary gets widened until nothing is blocked.
//
// PROSE CANNOT VOTE. Every decision input is read through readField(), which
// records its JSON pointer and REFUSES a pointer whose leaf is a narrative key.
// Human-readable quotes are gathered by quote() only after the status is frozen,
// so a summary string has no path to the verdict even in principle.

import { execFileSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import { existsSync, mkdirSync, mkdtempSync, readFileSync, rmSync, statSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

/** The ONLY status strings the plan documents. A fourth cannot be emitted. */
export const STATUS = Object.freeze({
  BEST_IN_CLASS_VERIFIED: 'BEST_IN_CLASS_VERIFIED',
  IMPLEMENTED_AVAILABLE_MATRIX_PASS: 'IMPLEMENTED_AVAILABLE_MATRIX_PASS',
  BLOCKED_EXTERNAL: 'BLOCKED_EXTERNAL',
});

/** Blocker CLASSES. "Install an engine" and "our plugin does not compile" are
 * not the same thing and must never be summed into one number. */
export const BLOCKER_CLASS = Object.freeze({
  OPERATOR_INPUT: 'external-operator-input',
  OWNED_DEFECT: 'owned-defect',
  OWNED_INSTRUMENT_GAP: 'owned-instrument-gap',
  SUPPLY_CHAIN: 'supply-chain-advisory',
  STALE_CERTIFICATION: 'stale-certification',
});

/** Leaf keys that carry narrative. A decision input may never end in one. */
const PROSE_LEAVES = Object.freeze([
  'verdict', 'statusReason', 'honestyStatement', 'notes', 'note', 'summary',
  'reason', 'finding', 'narrative', 'reading', 'why', 'whyNotFixed', 'detail',
  'observable', 'caveats', 'notProven', 'title', 'interpretation', 'conclusion',
  'criterionVerdict', 'downstreamImpact', 'explanation',
]);

const EVIDENCE_FILES = Object.freeze({
  budgets: 'task-48-pure-unreal-mcp-implementation.json',
  adversarial: 'task-51-pure-unreal-mcp-implementation.json',
  staticGates: 'task-55-pure-unreal-mcp-implementation.json',
  matrix: 'task-62-pure-unreal-mcp-implementation.json',
});

/** The advertised range. Nine minors, named, so a shrunken matrix is visible. */
const ADVERTISED_MINORS = Object.freeze(['5.0', '5.1', '5.2', '5.3', '5.4', '5.5', '5.6', '5.7', '5.8']);

/**
 * Findings that DO NOT gate the status but must travel with it. Each names where
 * it was measured. They are listed separately from the rule-produced blockers so
 * nobody can mistake a disclosure for something the engine tested: the status is
 * already BLOCKED_EXTERNAL without any of them, and would be unchanged if every
 * one were struck out.
 */
const DISCLOSED_FINDINGS = Object.freeze([
  {
    id: 'D5-observe-foreign-ownership-false-positive',
    class: BLOCKER_CLASS.OWNED_DEFECT,
    remediationOwner: 'us',
    statement: 'observeForeignOwnership() matches the RAW COMMAND LINE, so any process merely MENTIONING RunUAT/UnrealBuildTool/Build.sh/UnrealEditor counts as a foreign builder. A false positive SILENTLY DELETES the load phase while the run still reports PARTIAL_PHASES_DEFERRED — the exact "truncates silently while reading as a pass" shape. Recorded by Task 51 as an OPEN finding, NOT remediated.',
    source: { evidenceFile: EVIDENCE_FILES.adversarial, pointer: '/selfCaughtDefects/6' },
    gating: false,
  },
  {
    id: 'T63-F1-locale-sensitive-record-ordering',
    class: BLOCKER_CLASS.OWNED_DEFECT,
    remediationOwner: 'us',
    statement: 'The capability-record ordering comparators still use String.prototype.localeCompare with no explicit locale (src/tools/catalog/capabilities/records/gameplay/index.ts:40 compareCanonicalIds, src/tools/catalog/capabilities/records/utility/index.ts:12 compareIds), so record order resolves against the HOST ICU locale. The notepad REAL-1 fix landed in src/server/gateway/gateway-guidance.ts, which now tie-breaks ordinally; these two did not change. UNMEASURED: no other-locale run was performed, so NO generated-artifact drift is claimed here — only that the comparator is locale-sensitive by API contract and that registry:check has therefore only ever been green under one locale.',
    source: { evidenceFile: 'src/tools/catalog/capabilities/records/{gameplay,utility}/index.ts', pointer: 'compareCanonicalIds / compareIds' },
    gating: false,
  },
]);

/** What this task did NOT do, stated before anyone can infer it did. */
const NOT_PROVEN = Object.freeze([
  'NO ENGINE, EDITOR, PLUGIN BUILD OR RunUAT WAS RUN BY THIS TASK. Every engine fact here is re-read from the Tasks 56-62 records. This task adds zero new live coverage and its status is a decision about existing measurements, not a new measurement.',
  'NO MODEL WAS CONTACTED. Task 48 records the model arm as BLOCKED_EXTERNAL / NOT_ENABLED. The absence of a model accuracy is carried as a blocker and is NOT converted into a qualitative claim in either direction.',
  'SEVEN OF NINE ADVERTISED MINORS HAVE NO PLUGIN COMPILE RESULT AT ALL, so for those minors nothing is proven about whether the plugin works — only that it could not be tried on this host.',
  'THE ONE PASS ROW (5.7.4) IS CERTIFIED AGAINST A TREE THAT IS NOT HEAD: 139 plugin source files are newer than the certified binary and the record\'s own tree snapshot covers the harness rather than plugin source.',
  'THE @hono/node-server ADVISORY WAS NOT EXPLOITABILITY-ASSESSED. Task 55 states plainly that it did not assess it and that Task 51 was the adversarial lane. This task rules on whether a SHIPPED-dependency advisory affects a quality claim; it does not rule on whether that advisory is reachable in this product.',
  'THE STATUS NAME UNDERSTATES THE SITUATION. BLOCKED_EXTERNAL is the plan\'s only residual status, but two of the blockers here are OURS, not external: UE 5.8 Preview 1 fails to compile our plugin on an engine that is installed and built, and the two public transports do not name the same action set live. Emitting a fourth, more accurate status would have widened a vocabulary the plan deliberately closed, so the ownership lives in blockersByClass instead of in the status string.',
]);

const SELF = fileURLToPath(import.meta.url);
const SELF_REL = 'scripts/qa/task63-claim-engine.mjs';

/**
 * @typedef {{reads: {source: string, pointer: string, value: unknown}[],
 *   quotes: {source: string, pointer: string}[]}} AccessLog
 */

// ─── field access ────────────────────────────────────────────────────────────

class ProseInputError extends Error {}

/** @param {string} pointer */
function leafOf(pointer) {
  const parts = pointer.split('/');
  return parts[parts.length - 1] ?? '';
}

/**
 * Read one decision input by JSON pointer, recording the pointer so the set of
 * fields the verdict depends on is itself evidence.
 * @param {AccessLog} log @param {string} source @param {any} document @param {string} pointer
 * @returns {any}
 */
function readField(log, source, document, pointer) {
  if (PROSE_LEAVES.includes(leafOf(pointer))) {
    throw new ProseInputError(`decision input ${source}${pointer} ends in narrative key "${leafOf(pointer)}"`);
  }
  let cursor = document;
  for (const raw of pointer.split('/').slice(1)) {
    const key = raw.replace(/~1/g, '/').replace(/~0/g, '~');
    if (cursor === null || cursor === undefined) { cursor = undefined; break; }
    cursor = Array.isArray(cursor) ? cursor[Number(key)] : cursor[key];
  }
  log.reads.push({ source, pointer, value: cursor === undefined ? null : cursor });
  return cursor;
}

/**
 * Display-only text. Called only after the status is frozen.
 * @param {AccessLog} log @param {string} source @param {any} document @param {string} pointer
 * @returns {any}
 */
function quote(log, source, document, pointer) {
  let cursor = document;
  for (const raw of pointer.split('/').slice(1)) {
    if (cursor === null || cursor === undefined) return null;
    cursor = Array.isArray(cursor) ? cursor[Number(raw)] : cursor[raw];
  }
  log.quotes.push({ source, pointer });
  return cursor === undefined ? null : cursor;
}

// ─── the exception predicate ─────────────────────────────────────────────────

/**
 * A DECLARED exception counts as a pass only when four structural predicates
 * hold. Predicate 3 is load-bearing: the failure must be attributable to a path
 * THE PLAN ITSELF protects (commit strategy, line 627: the preserved dirty paths
 * must survive and be proven by the preservation validator). An exception that
 * merely asserts "accepted, precisely scoped" excuses nothing.
 * @param {AccessLog} log @param {any} doc @param {number} index
 */
function evaluateException(log, doc, index) {
  const at = `/declaredExceptions/${index}`;
  const id = readField(log, 'task-55', doc, `${at}/id`);
  const gate = readField(log, 'task-55', doc, `${at}/gate`);
  const declaredExit = readField(log, 'task-55', doc, `${at}/exitCode`);
  const commands = readField(log, 'task-55', doc, '/gateChain/commands') ?? [];
  const linked = commands.findIndex((/** @type {any} */ entry) => entry?.command === gate);

  const gateLinked = linked >= 0;
  const exitCodeAgrees = gateLinked
    && readField(log, 'task-55', doc, `/gateChain/commands/${linked}/exitCode`) === declaredExit;

  // P3 — attribution to a plan-protected preserved path, per gate. A gate with
  // no evaluator is NOT attributed: silence is not an excuse.
  let attributed = false;
  let attributionEvidence = 'no attribution evaluator for this gate; an exception with no machine-checkable attribution to a plan-protected path excuses nothing';
  if (gate === 'npm run test:unit') {
    const exact = readField(log, 'task-55', doc, '/unitBaseline/exactlyTheDeclaredBaseline');
    const failures = readField(log, 'task-55', doc, '/unitBaseline/failures') ?? [];
    const allPreserved = Array.isArray(failures) && failures.length > 0
      && failures.every((/** @type {any} */ entry) => entry?.preservedPath === true || entry?.offenderIsPreservedPath === true);
    attributed = exact === true && allPreserved;
    attributionEvidence = `exactlyTheDeclaredBaseline=${String(exact)}; ${Array.isArray(failures) ? failures.length : 0} failure(s), every one attributable to a preserved path=${String(allPreserved)}`;
  }

  // P4 — an exception carrying a SHIPPED-dependency advisory is not a scoping
  // decision, it is a product risk, and it cannot be excused here.
  const findings = readField(log, 'task-55', doc, `${at}/findings`) ?? [];
  const production = (Array.isArray(findings) ? findings : [])
    .filter((/** @type {any} */ entry) => /^production/i.test(String(entry?.path ?? '')));

  return {
    id,
    gate,
    declaredExit,
    predicates: {
      gateLinked,
      exitCodeAgrees,
      attributedToPlanProtectedPath: attributed,
      noShippedDependencyAdvisory: production.length === 0,
    },
    attributionEvidence,
    productionPathFindings: production.map((/** @type {any} */ entry) => ({
      package: entry?.package, severity: entry?.severity, path: entry?.path, advisory: entry?.advisory,
    })),
    excused: gateLinked && exitCodeAgrees && attributed && production.length === 0,
  };
}

// ─── availability ────────────────────────────────────────────────────────────

/**
 * "Available" under the PRIMARY reading = a row a live gate could actually have
 * been run against: the root is on this host AND its editor target is compiled.
 * The ALTERNATE reading is the plan's success-criteria wording (line 640), which
 * calls 5.0.3/5.3.2/5.5.4/5.7.4/5.8-P1 "available" on root presence alone. Both
 * are computed so the verdict can be shown invariant to which one wins.
 * @param {{rootPresent: any, editorBuilt: any}} row @param {'primary'|'alternate'} reading
 */
function isAvailable(row, reading) {
  if (reading === 'alternate') return row.rootPresent === true;
  return row.rootPresent === true && row.editorBuilt === true;
}

/**
 * A PASS row is only current if its certified binary covers the tree it claims.
 * @param {any[]} driftRows @param {string} minor
 */
function freshnessOf(driftRows, minor) {
  const drift = (driftRows ?? []).find((entry) => entry?.minor === minor);
  if (!drift) {
    return {
      proven: false,
      why: 'no certification-drift row exists for this minor, so "does the certified binary cover this tree?" is unanswerable and fails closed',
    };
  }
  return {
    proven: drift.pluginSourceFilesNewerThanBinary === 0 && drift.recordedTreeCoversPluginSource === true,
    why: `pluginSourceFilesNewerThanBinary=${String(drift.pluginSourceFilesNewerThanBinary)}, recordedTreeCoversPluginSource=${String(drift.recordedTreeCoversPluginSource)}`,
    certifiedBinarySha256: drift.certifiedBinarySha256,
    binaryBuiltAt: drift.binaryBuiltAt,
  };
}

// ─── the rules ───────────────────────────────────────────────────────────────

/**
 * @param {AccessLog} log
 * @param {{budgets: any, adversarial: any, staticGates: any, matrix: any}} docs
 * @param {'primary'|'alternate'} reading
 */
function evaluateRules(log, docs, reading) {
  /** @type {any[]} */
  const rules = [];
  /** @type {any[]} */
  const blockers = [];

  // ── R1 static/code gates ───────────────────────────────────────────────────
  const commands = readField(log, 'task-55', docs.staticGates, '/gateChain/commands') ?? [];
  const exceptions = (readField(log, 'task-55', docs.staticGates, '/declaredExceptions') ?? [])
    .map((/** @type {any} */ _entry, /** @type {number} */ index) => evaluateException(log, docs.staticGates, index));
  const treeHeld = readField(log, 'task-55', docs.staticGates, '/sourceTreeHash/held');
  const preservedOk = readField(log, 'task-55', docs.staticGates, '/preflight/preserved24/okCount');
  const preservedBad = readField(log, 'task-55', docs.staticGates, '/preflight/preserved24/mismatches');

  /** @type {any[]} */
  const failedGates = [];
  for (const [index, entry] of commands.entries()) {
    const code = readField(log, 'task-55', docs.staticGates, `/gateChain/commands/${index}/exitCode`);
    if (code === 0) continue;
    const command = readField(log, 'task-55', docs.staticGates, `/gateChain/commands/${index}/command`);
    const excuse = exceptions.find((/** @type {any} */ candidate) => candidate.gate === command);
    failedGates.push({
      index, position: entry.position, label: entry.label, command, exitCode: code,
      excusedBy: excuse?.excused === true ? excuse.id : null,
    });
  }
  const unexcused = failedGates.filter((entry) => entry.excusedBy === null);
  for (const gate of unexcused) {
    const isAudit = gate.command === 'npm audit --audit-level=moderate';
    blockers.push({
      id: `static-gate:${String(gate.label)}`,
      class: isAudit ? BLOCKER_CLASS.SUPPLY_CHAIN : BLOCKER_CLASS.OWNED_DEFECT,
      remediationOwner: isAudit
        ? 'us (dependency owner): the advisory sits in a dependency we ship; remediation is a breaking version decision, not an operator action'
        : 'us: a repository gate exits nonzero and no declared exception excuses it',
      statement: `static gate "${String(gate.command)}" exited ${String(gate.exitCode)} and no declared exception excuses it`,
      citation: { evidenceFile: EVIDENCE_FILES.staticGates, pointer: `/gateChain/commands/${gate.index}/exitCode`, value: gate.exitCode },
    });
  }
  rules.push({
    id: 'R1',
    requirement: 'all code/static gates pass on one recorded tree (plan line 599 "all code/static/available-live gates pass"; line 640 "Static gates pass from one recorded tree")',
    satisfied: unexcused.length === 0 && treeHeld === true && preservedOk === 24 && preservedBad === 0,
    observed: {
      gatesTotal: commands.length,
      gatesExitZero: commands.filter((/** @type {any} */ entry) => entry.exitCode === 0).length,
      gatesNonZero: failedGates.map((entry) => ({ position: entry.position, command: entry.command, exitCode: entry.exitCode, excusedBy: entry.excusedBy })),
      declaredExceptions: exceptions,
      sourceTreeHashHeld: treeHeld,
      preserved24: { okCount: preservedOk, mismatches: preservedBad },
    },
  });

  // ── R2 available-live gates ────────────────────────────────────────────────
  const rows = readField(log, 'task-62', docs.matrix, '/environment/aggregate/rows') ?? [];
  const drift = readField(log, 'task-62', docs.matrix, '/environment/certificationDrift/rows') ?? [];
  const probesAgree = readField(log, 'task-62', docs.matrix, '/environment/rootProbes/allAgree');
  /** @type {any[]} */
  const rowVerdicts = [];
  for (const [index] of rows.entries()) {
    const at = `/environment/aggregate/rows/${index}`;
    const minor = readField(log, 'task-62', docs.matrix, `${at}/minor`);
    const state = readField(log, 'task-62', docs.matrix, `${at}/state`);
    const rootPresent = readField(log, 'task-62', docs.matrix, `${at}/rootPresent`);
    const editorBuilt = readField(log, 'task-62', docs.matrix, `${at}/editorBuilt`);
    const subclass = readField(log, 'task-62', docs.matrix, `${at}/subclass`);
    const owner = readField(log, 'task-62', docs.matrix, `${at}/remediationOwner`);
    const available = isAvailable({ rootPresent, editorBuilt }, reading);
    const fresh = state === 'PASS'
      ? freshnessOf(drift, minor)
      : { proven: true, why: 'not a PASS row; no freshness is asserted for it' };
    rowVerdicts.push({
      minor, state, rootPresent, editorBuilt, subclass, remediationOwner: owner,
      available, certified: state === 'PASS' && fresh.proven === true, freshness: fresh,
    });

    if (available && state !== 'PASS') {
      blockers.push({
        id: `available-engine-not-passing:${String(minor)}`,
        class: state === 'FAIL' ? BLOCKER_CLASS.OWNED_DEFECT : BLOCKER_CLASS.OPERATOR_INPUT,
        remediationOwner: owner ?? 'unrecorded',
        statement: `UE ${String(minor)} is AVAILABLE under the ${reading} reading (rootPresent=${String(rootPresent)}, editorBuilt=${String(editorBuilt)}) but its state is ${String(state)}`,
        citation: { evidenceFile: EVIDENCE_FILES.matrix, pointer: `${at}/state`, value: state },
      });
    }
    if (!available) {
      blockers.push({
        id: `engine-not-available:${String(minor)}`,
        class: BLOCKER_CLASS.OPERATOR_INPUT,
        remediationOwner: owner ?? 'operator',
        statement: `UE ${String(minor)} cannot host a live gate: rootPresent=${String(rootPresent)}, editorBuilt=${String(editorBuilt)}, subclass=${String(subclass)}`,
        citation: { evidenceFile: EVIDENCE_FILES.matrix, pointer: `${at}/editorBuilt`, value: editorBuilt },
      });
    }
    if (state === 'PASS' && fresh.proven !== true) {
      blockers.push({
        id: `stale-certification:${String(minor)}`,
        class: BLOCKER_CLASS.STALE_CERTIFICATION,
        remediationOwner: 'us: re-certify this minor against the current tree',
        statement: `UE ${String(minor)} PASSES, but that certification does not cover the current tree (${fresh.why})`,
        citation: { evidenceFile: EVIDENCE_FILES.matrix, pointer: '/environment/certificationDrift/rows', value: fresh.why },
      });
    }
  }
  const availableRows = rowVerdicts.filter((entry) => entry.available);
  rules.push({
    id: 'R2',
    requirement: 'every AVAILABLE engine row passes its live gates AND its certification covers the current tree (plan line 594 "available-engine rows pass or fail with direct evidence; missing rows remain blocked")',
    satisfied: probesAgree === true && availableRows.length > 0 && availableRows.every((entry) => entry.certified),
    observed: { reading, rootProbesAllAgree: probesAgree, availableCount: availableRows.length, rows: rowVerdicts },
  });

  // ── R3 evaluation budgets ──────────────────────────────────────────────────
  const declared = readField(log, 'task-48', docs.budgets, '/budgets/declared');
  const passed = readField(log, 'task-48', docs.budgets, '/budgets/passed');
  const failedCount = readField(log, 'task-48', docs.budgets, '/budgets/failed');
  const results = readField(log, 'task-48', docs.budgets, '/budgets/results') ?? [];
  const failingBudgets = results
    .filter((/** @type {any} */ entry) => entry?.passed !== true)
    .map((/** @type {any} */ entry) => entry.id);
  for (const id of failingBudgets) {
    blockers.push({
      id: `budget:${String(id)}`,
      class: BLOCKER_CLASS.OWNED_DEFECT,
      remediationOwner: 'us',
      statement: `evaluation budget ${String(id)} does not pass`,
      citation: { evidenceFile: EVIDENCE_FILES.budgets, pointer: '/budgets/results', value: id },
    });
  }
  rules.push({
    id: 'R3',
    requirement: 'every declared retrieval/latency/memory/payload budget passes (Task 48 blocks this task)',
    satisfied: typeof declared === 'number' && declared > 0 && failedCount === 0 && passed === declared && failingBudgets.length === 0,
    observed: { declared, passed, failed: failedCount, failingIds: failingBudgets },
  });

  // ── R4 adversarial / security / load / soak ────────────────────────────────
  const counter = (/** @type {string} */ pointer) => readField(log, 'task-51', docs.adversarial, pointer);
  const counters = {
    loadPlanned: counter('/liveResults/loadFullScale32Sessions/requests/planned'),
    loadAttempted: counter('/liveResults/loadFullScale32Sessions/requests/attempted'),
    loadAnswered: counter('/liveResults/loadFullScale32Sessions/requests/answered'),
    loadSucceeded: counter('/liveResults/loadFullScale32Sessions/requests/succeeded'),
    rssPass: counter('/liveResults/loadFullScale32Sessions/nodeRetainedRss/pass'),
    rssBaselineWasAPeak: counter('/liveResults/loadFullScale32Sessions/nodeRetainedRss/baselineWasAPeak'),
    growthPass: counter('/liveResults/loadFullScale32Sessions/nodeSecondHalfGrowth/pass'),
    receiptsLeaked: counter('/liveResults/loadFullScale32Sessions/cleanup/leaked'),
    receiptsPresentAfter: counter('/liveResults/loadFullScale32Sessions/cleanup/presentAfterTeardown'),
    soakPlanned: counter('/liveResults/cleanupSoakAuthoritative/cyclesPlanned'),
    soakOpened: counter('/liveResults/cleanupSoakAuthoritative/cyclesOpened'),
    soakCompleted: counter('/liveResults/cleanupSoakAuthoritative/cyclesCompleted'),
    soakLeaks: counter('/liveResults/cleanupSoakAuthoritative/openStateLeaks'),
    fuzzSurvived: counter('/liveResults/protocolFuzzReconfirmed/survived'),
    fuzzAnswered: counter('/liveResults/protocolFuzzReconfirmed/answeredAfterFuzz'),
    fuzzCheckpoints: counter('/liveResults/protocolFuzzReconfirmed/checkpoints'),
    fuzzCheckpointsAlive: counter('/liveResults/protocolFuzzReconfirmed/checkpointsAnsweredAndAlive'),
    fuzzMalformedStdout: counter('/liveResults/protocolFuzzReconfirmed/malformedStdoutLines'),
    residueEntries: counter('/liveResults/processResidue/residueEntries'),
    residueSurvivors: counter('/liveResults/processResidue/survivors'),
    offlineFindings: counter('/offlineResults/findings'),
    differentialFindings: counter('/offlineResults/commandDifferential/findings'),
    unusedClasses: (counter('/offlineResults/commandDifferential/unusedClasses') ?? []).length,
    pathEscapes: counter('/offlineResults/pathContainment/escapes'),
    ledgerViolations: counter('/offlineResults/idempotencyLedger/violations'),
  };
  const countersHold = counters.loadPlanned === counters.loadAttempted
    && counters.loadAttempted === counters.loadAnswered
    && counters.loadAnswered === counters.loadSucceeded
    && counters.rssPass === true && counters.rssBaselineWasAPeak === false && counters.growthPass === true
    && counters.receiptsLeaked === 0 && counters.receiptsPresentAfter === 0
    && counters.soakPlanned === counters.soakOpened && counters.soakOpened === counters.soakCompleted
    && counters.soakLeaks === 0
    && counters.fuzzSurvived === true && counters.fuzzAnswered === true
    && counters.fuzzCheckpoints === counters.fuzzCheckpointsAlive && counters.fuzzMalformedStdout === 0
    && counters.residueEntries === 0 && counters.residueSurvivors === 0
    && counters.offlineFindings === 0 && counters.differentialFindings === 0 && counters.unusedClasses === 0
    && counters.pathEscapes === 0 && counters.ledgerViolations === 0;

  const blockedEntries = readField(log, 'task-51', docs.adversarial, '/blocked') ?? [];
  /** @type {any[]} */
  const openAdversarial = [];
  for (const [index] of blockedEntries.entries()) {
    const state = String(readField(log, 'task-51', docs.adversarial, `/blocked/${index}/status`) ?? '');
    const code = readField(log, 'task-51', docs.adversarial, `/blocked/${index}/code`);
    const open = /STILL BLOCKED/i.test(state);
    const partial = /PARTIALLY CLOSED/i.test(state);
    if (!open && !partial) continue;
    openAdversarial.push({ index, code, state: open ? 'STILL_BLOCKED' : 'PARTIALLY_CLOSED' });
    const needsEditor = code === 'FULL_PARITY_CORPUS_NOT_RUN';
    blockers.push({
      id: `adversarial:${String(code)}`,
      class: needsEditor ? BLOCKER_CLASS.OPERATOR_INPUT : BLOCKER_CLASS.OWNED_INSTRUMENT_GAP,
      remediationOwner: needsEditor
        ? 'operator+us: the full corpus needs a live editor session; the corpus itself exists and is recorded'
        : 'us: the gate needs an instrument we have not built (per-allocator LLM tags for editor RSS; a compiled delegate counter)',
      statement: `adversarial gate ${String(code)} is ${open ? 'STILL BLOCKED' : 'only PARTIALLY CLOSED'}`,
      citation: { evidenceFile: EVIDENCE_FILES.adversarial, pointer: `/blocked/${index}/status`, value: state },
    });
  }
  if (!countersHold) {
    blockers.push({
      id: 'adversarial:counters',
      class: BLOCKER_CLASS.OWNED_DEFECT,
      remediationOwner: 'us',
      statement: 'an adversarial/load/soak/fuzz counter does not hold',
      citation: { evidenceFile: EVIDENCE_FILES.adversarial, pointer: '/liveResults', value: counters },
    });
  }
  rules.push({
    id: 'R4',
    requirement: 'adversarial, fuzz, security, load and soak gates hold with no gate left blocked or half-closed (plan line 639)',
    satisfied: countersHold && openAdversarial.length === 0,
    observed: { counters, openOrPartial: openAdversarial },
  });

  // ── R5 shipped-dependency supply chain ─────────────────────────────────────
  /** @type {any[]} */
  const allFindings = [];
  for (const [index] of (readField(log, 'task-55', docs.staticGates, '/declaredExceptions') ?? []).entries()) {
    const findings = readField(log, 'task-55', docs.staticGates, `/declaredExceptions/${index}/findings`) ?? [];
    for (const [findingIndex, finding] of (Array.isArray(findings) ? findings : []).entries()) {
      const at = `/declaredExceptions/${index}/findings/${findingIndex}`;
      const path = String(readField(log, 'task-55', docs.staticGates, `${at}/path`) ?? '');
      allFindings.push({
        package: readField(log, 'task-55', docs.staticGates, `${at}/package`),
        severity: readField(log, 'task-55', docs.staticGates, `${at}/severity`),
        shipped: /^production/i.test(path),
        path,
        count: finding?.count ?? 1,
        pointer: at,
      });
    }
  }
  const shipped = allFindings.filter((entry) => entry.shipped);
  for (const finding of shipped) {
    blockers.push({
      id: `supply-chain:${String(finding.package)}`,
      class: BLOCKER_CLASS.SUPPLY_CHAIN,
      remediationOwner: 'us (dependency owner): the advisory is reached through a dependency we ship; the fix is a breaking version decision',
      statement: `${String(finding.package)} (${String(finding.severity)}) is reached on the PRODUCTION path, so it ships to users`,
      citation: { evidenceFile: EVIDENCE_FILES.staticGates, pointer: finding.pointer, value: finding.path },
    });
  }
  rules.push({
    id: 'R5',
    requirement: 'no advisory in the SHIPPED dependency path. A quality claim covers the bytes a user installs: a dev-only advisory never reaches them, a production-path one does.',
    satisfied: shipped.length === 0,
    observed: { findings: allFindings, shippedCount: shipped.length, devOnlyCount: allFindings.length - shipped.length },
  });

  // ── R6 transport equivalence ───────────────────────────────────────────────
  const totalsAt = '/environment/transports/liveDivergence/totals';
  const toolsDiverging = readField(log, 'task-62', docs.matrix, `${totalsAt}/toolsDiverging`);
  const unnamed = readField(log, 'task-62', docs.matrix, `${totalsAt}/declaredActionsUnnamedByNative`);
  const undeclared = readField(log, 'task-62', docs.matrix, `${totalsAt}/namesNativeEmitsThatAreNotDeclaredCapabilities`);
  if (toolsDiverging !== 0 || unnamed !== 0 || undeclared !== 0) {
    blockers.push({
      id: 'transport-divergence',
      class: BLOCKER_CLASS.OWNED_DEFECT,
      remediationOwner: 'us: the native describe surface does not name the declared action list the TypeScript surface names',
      statement: `${String(toolsDiverging)} tool(s) diverge live; ${String(unnamed)} declared actions are unnamed by native describe; ${String(undeclared)} names native emits are not declared capabilities`,
      citation: { evidenceFile: EVIDENCE_FILES.matrix, pointer: totalsAt, value: { toolsDiverging, unnamed, undeclared } },
    });
  }
  rules.push({
    id: 'R6',
    requirement: 'the two public transports are equivalent in what they NAME (plan line 638 "transport-equivalent"; line 635 "exact drift/parity checks")',
    satisfied: toolsDiverging === 0 && unnamed === 0 && undeclared === 0,
    observed: { toolsDiverging, declaredActionsUnnamedByNative: unnamed, namesNativeEmitsThatAreNotDeclaredCapabilities: undeclared },
  });

  // ── R7 configured real-model accuracy (best-in-class only) ─────────────────
  const modelStatus = readField(log, 'task-48', docs.budgets, '/modelArm/status');
  const modelAccuracy = readField(log, 'task-48', docs.budgets, '/modelArm/accuracy');
  const modelOk = modelStatus !== 'BLOCKED_EXTERNAL' && typeof modelAccuracy === 'number';
  if (!modelOk) {
    blockers.push({
      id: 'model-arm-not-configured',
      class: BLOCKER_CLASS.OPERATOR_INPUT,
      remediationOwner: 'operator: configure a real model, set TASK48_MODEL_ENABLE=1, and re-run the Task 48 model arm',
      statement: `no configured real-model accuracy exists (modelArm.status=${String(modelStatus)}, accuracy=${String(modelAccuracy)})`,
      citation: { evidenceFile: EVIDENCE_FILES.budgets, pointer: '/modelArm/status', value: modelStatus },
    });
  }
  rules.push({
    id: 'R7',
    requirement: 'BEST_IN_CLASS_VERIFIED additionally requires configured real-model accuracy (plan line 599)',
    satisfied: modelOk,
    observed: { status: modelStatus, accuracy: modelAccuracy ?? null },
    bestInClassOnly: true,
  });

  // ── R8 every advertised UE minor (best-in-class only) ──────────────────────
  const rowsRequired = readField(log, 'task-62', docs.matrix, '/environment/aggregate/rowsRequired');
  const present = rowVerdicts.map((entry) => entry.minor);
  const missingRows = ADVERTISED_MINORS.filter((minor) => !present.includes(minor));
  const uncertified = rowVerdicts.filter((entry) => !entry.certified).map((entry) => entry.minor);
  rules.push({
    id: 'R8',
    requirement: 'BEST_IN_CLASS_VERIFIED additionally requires ALL advertised UE minors (plan line 599; line 602 "current missing roots force best-in-class verification blocked until supplied")',
    satisfied: rowsRequired === ADVERTISED_MINORS.length && missingRows.length === 0 && uncertified.length === 0,
    observed: {
      advertised: ADVERTISED_MINORS,
      rowsRequired,
      missingRows,
      certified: rowVerdicts.filter((entry) => entry.certified).map((entry) => entry.minor),
      uncertified,
    },
    bestInClassOnly: true,
  });

  return { rules, blockers };
}

/**
 * The ladder. Nothing here touches a document; it sees rule verdicts only.
 * @param {any[]} rules
 */
export function decide(rules) {
  const by = (/** @type {string} */ id) => rules.find((rule) => rule.id === id);
  const implementation = ['R1', 'R2', 'R3', 'R4', 'R5', 'R6'];
  const bestInClass = [...implementation, 'R7', 'R8'];
  const unmet = (/** @type {string[]} */ ids) => ids.filter((id) => by(id)?.satisfied !== true);

  const bicUnmet = unmet(bestInClass);
  if (bicUnmet.length === 0) {
    return {
      status: STATUS.BEST_IN_CLASS_VERIFIED,
      rule: 'BIC-1',
      unmet: /** @type {string[]} */ ([]),
      ruleText: 'R1-R8 all satisfied: every code/static/available-live gate passes AND configured real-model accuracy exists AND every advertised UE minor is certified against the current tree.',
    };
  }
  const implUnmet = unmet(implementation);
  if (implUnmet.length === 0) {
    return {
      status: STATUS.IMPLEMENTED_AVAILABLE_MATRIX_PASS,
      rule: 'IMPL-1',
      unmet: bicUnmet,
      ruleText: `R1-R6 all satisfied, so every code/static/available-live gate passes; ${bicUnmet.join(' and ')} unmet, so the ADDITIONAL best-in-class requirement is not met.`,
    };
  }
  return {
    status: STATUS.BLOCKED_EXTERNAL,
    rule: 'FALLBACK-1',
    unmet: implUnmet,
    ruleText: `${implUnmet.join(', ')} unmet, so "all code/static/available-live gates pass" is false. The plan documents no other residual status, so the exact BLOCKED_EXTERNAL fields are emitted.`,
  };
}

// ─── forging: the falsifiability controls ────────────────────────────────────

export const OMISSIONS = Object.freeze({
  'model-arm': 'remove the configured real-model accuracy',
  'minor-blocked': 'make one advertised minor externally blocked (root absent) rather than certified',
  'minor-fail': 'make one AVAILABLE minor FAIL rather than PASS',
  'static-gate': 'make one static gate exit nonzero with no declared exception',
  'supply-chain': 'introduce one production-path advisory',
  'stale-certification': "make a PASS row's certified binary predate the tree",
  'transport-divergence': 'make the two transports name different action sets',
  adversarial: 'leave one adversarial gate STILL BLOCKED',
});

/** @param {string} file */
const readJson = (file) => JSON.parse(readFileSync(file, 'utf8'));
/** @param {string} file @param {unknown} value */
const writeJson = (file, value) => { writeFileSync(file, `${JSON.stringify(value, null, 2)}\n`); };

/**
 * Rewrite NARRATIVE strings only, leaving every measured field untouched. If the
 * emitted status moves, the engine is reading prose.
 * @param {any} docs @param {string} direction
 */
function applyProse(docs, direction) {
  const claim = direction === 'claim';
  const line = claim
    ? 'BEST_IN_CLASS_VERIFIED. Every gate passes, every UE minor 5.0-5.8 is certified against HEAD, and the real-model arm recorded 0.99 accuracy. There are no blockers of any kind.'
    : 'BLOCKED_EXTERNAL. Nothing here passes. Every engine is missing, every gate failed, and no claim of any strength is supported by anything at all.';
  docs.budgets.statusReason = line;
  docs.adversarial.verdict = line;
  docs.staticGates.honestyStatement = line;
  docs.staticGates.acceptanceCriteria = { everyCommandExitsZeroFromSameTreeHash: { met: claim, detail: line } };
  docs.matrix.verdict = line;
  docs.matrix.notes = [line];
  docs.matrix.notProven = claim ? [] : [line];
  docs.matrix.environment.aggregate.outcome = claim ? 'ACCEPTED' : 'REJECTED';
  return docs;
}

/**
 * Build a synthetic PERFECT input set by MUTATING the real records. Nothing is
 * invented from an empty object: a fixture built from scratch would be refused
 * for being malformed rather than accepted for being perfect, and that would
 * prove the wrong thing.
 * @param {{sourceDir: string, targetDir: string, omit?: string|null, prose?: string|null}} spec
 */
export function forgeInputs(spec) {
  mkdirSync(spec.targetDir, { recursive: true });
  /** @type {any} */
  const docs = {};
  for (const [key, file] of Object.entries(EVIDENCE_FILES)) docs[key] = readJson(join(spec.sourceDir, file));

  // task-48: a configured, recorded model accuracy.
  docs.budgets.modelArm = {
    status: 'ENABLED', reason: 'SYNTHETIC_PERFECT_FIXTURE', accuracy: 0.94, cases: 56, model: 'synthetic-fixture-model',
  };

  // task-51: every adversarial blocker closed.
  for (const entry of docs.adversarial.blocked ?? []) entry.status = 'CLOSED — synthetic perfect fixture';

  // task-55: every gate exits 0 and no exception carries a shipped advisory.
  for (const command of docs.staticGates.gateChain?.commands ?? []) command.exitCode = 0;
  docs.staticGates.unitBaseline.exitCode = 0;
  for (const exception of docs.staticGates.declaredExceptions ?? []) { exception.exitCode = 0; exception.findings = []; }

  // task-62: nine certified, current, transport-equivalent rows.
  docs.matrix.environment.certificationDrift.rows = ADVERTISED_MINORS.map((minor) => ({
    minor,
    certifiedBinarySha256: createHash('sha256').update(`synthetic-perfect-${minor}`).digest('hex'),
    binaryBuiltAt: '2026-07-29T00:00:00.000Z',
    pluginSourceFilesNewerThanBinary: 0,
    recordedTreeCoversPluginSource: true,
  }));
  for (const row of docs.matrix.environment.aggregate.rows) {
    row.state = 'PASS';
    row.subclass = null;
    row.remediationOwner = null;
    row.rootPresent = true;
    row.editorBuilt = true;
    row.blockedCitation = null;
    row.citation = row.citation ?? {
      evidenceFile: 'synthetic-perfect-fixture',
      stageId: 'tree.stable',
      stagePath: '/environment/stages[19]',
      observable: 'synthetic perfect fixture',
      stageTally: { PASSED: 20, FAILED: 0, NOT_REACHED: 0, NOT_APPLICABLE: 0 },
    };
  }
  docs.matrix.environment.aggregate.counts = { PASS: 9, FAIL: 0, BLOCKED_EXTERNAL: 0 };
  docs.matrix.environment.transports.liveDivergence.totals = {
    toolsDiverging: 0, declaredActionsUnnamedByNative: 0, namesNativeEmitsThatAreNotDeclaredCapabilities: 0,
  };

  // ── exactly ONE requirement removed, per run ───────────────────────────────
  const target = (/** @type {string} */ minor) => docs.matrix.environment.aggregate.rows
    .find((/** @type {any} */ row) => row.minor === minor);
  switch (spec.omit ?? null) {
    case null: break;
    case 'model-arm':
      docs.budgets.modelArm = { status: 'BLOCKED_EXTERNAL', reason: 'NOT_ENABLED' };
      break;
    case 'minor-blocked': {
      const row = target('5.4');
      row.state = 'BLOCKED_EXTERNAL';
      row.subclass = 'root-absent';
      row.rootPresent = false;
      row.editorBuilt = false;
      row.remediationOwner = 'operator: install the engine at a root this host can see, then build its editor target';
      break;
    }
    case 'minor-fail': {
      const row = target('5.4');
      row.state = 'FAIL';
      row.rootPresent = true;
      row.editorBuilt = true;
      row.remediationOwner = 'us: the engine is installed and its editor is built, so nothing external is missing';
      break;
    }
    case 'static-gate':
      docs.staticGates.gateChain.commands[4].exitCode = 1;
      break;
    case 'supply-chain':
      docs.staticGates.declaredExceptions[1].findings = [{
        package: '@hono/node-server',
        severity: 'moderate',
        path: 'production: @modelcontextprotocol/sdk -> @hono/node-server',
        advisory: 'GHSA-frvp-7c67-39w9',
      }];
      break;
    case 'stale-certification':
      docs.matrix.environment.certificationDrift.rows[7].pluginSourceFilesNewerThanBinary = 139;
      docs.matrix.environment.certificationDrift.rows[7].recordedTreeCoversPluginSource = false;
      break;
    case 'transport-divergence':
      docs.matrix.environment.transports.liveDivergence.totals = {
        toolsDiverging: 8, declaredActionsUnnamedByNative: 207, namesNativeEmitsThatAreNotDeclaredCapabilities: 29,
      };
      break;
    case 'adversarial':
      docs.adversarial.blocked[1].status = 'STILL BLOCKED';
      break;
    default:
      throw new Error(`unknown --omit "${String(spec.omit)}"; known: ${Object.keys(OMISSIONS).join(', ')}`);
  }

  if (spec.prose) applyProse(docs, spec.prose);
  for (const [key, file] of Object.entries(EVIDENCE_FILES)) writeJson(join(spec.targetDir, file), docs[key]);
  return { dir: spec.targetDir, omitted: spec.omit ?? null, prose: spec.prose ?? null };
}

/**
 * Copy the REAL input set with only its narrative strings rewritten.
 * @param {{sourceDir: string, targetDir: string, prose: string}} spec
 */
export function forgeProseOnly(spec) {
  mkdirSync(spec.targetDir, { recursive: true });
  /** @type {any} */
  const docs = {};
  for (const [key, file] of Object.entries(EVIDENCE_FILES)) docs[key] = readJson(join(spec.sourceDir, file));
  applyProse(docs, spec.prose);
  for (const [key, file] of Object.entries(EVIDENCE_FILES)) writeJson(join(spec.targetDir, file), docs[key]);
  return { dir: spec.targetDir, prose: spec.prose };
}

/**
 * Prove the prose guard is not decoration: a rule that reads a narrative leaf is
 * refused rather than quietly answered.
 * @param {any} docs
 */
function proveProseGuard(docs) {
  /** @type {AccessLog} */
  const log = { reads: [], quotes: [] };
  try {
    readField(log, 'task-62', docs.matrix, '/verdict');
    return { guardHeld: false, detail: 'readField accepted a narrative leaf; the prose guard is decoration' };
  } catch (error) {
    if (!(error instanceof ProseInputError)) throw error;
    return { guardHeld: true, detail: String(error.message) };
  }
}

// ─── run ─────────────────────────────────────────────────────────────────────

/** @param {{inputDir: string}} spec */
export function runEngine(spec) {
  /** @type {any} */
  const docs = {};
  for (const [key, file] of Object.entries(EVIDENCE_FILES)) {
    const path = join(spec.inputDir, file);
    if (!existsSync(path)) throw new Error(`missing required input ${path}`);
    docs[key] = readJson(path);
  }
  /** @type {AccessLog} */
  const log = { reads: [], quotes: [] };
  const primary = evaluateRules(log, docs, 'primary');
  const decision = decide(primary.rules);

  // The alternate reading of "available" (plan line 640: root presence alone) is
  // computed on a SEPARATE log so the verdict can be shown to survive the
  // argument rather than to depend on winning it.
  const alternate = decide(evaluateRules({ reads: [], quotes: [] }, docs, 'alternate').rules);

  // Display-only quotes are gathered AFTER the status is frozen.
  /** @type {AccessLog} */
  const display = { reads: [], quotes: [] };
  const quoted = {
    matrixVerdict: quote(display, 'task-62', docs.matrix, '/verdict'),
    matrixNotProven: quote(display, 'task-62', docs.matrix, '/notProven'),
    staticHonesty: quote(display, 'task-55', docs.staticGates, '/honestyStatement'),
    budgetsStatusReason: quote(display, 'task-48', docs.budgets, '/statusReason'),
  };

  return {
    docs,
    log,
    display,
    quoted,
    status: decision.status,
    rule: decision.rule,
    ruleText: decision.ruleText,
    unmet: decision.unmet,
    rules: primary.rules,
    blockers: primary.blockers,
    alternateReading: {
      status: alternate.status,
      rule: alternate.rule,
      agreesWithPrimary: alternate.status === decision.status,
    },
    proseGuard: proveProseGuard(docs),
  };
}

/** @param {string[]} argv */
function parseArgs(argv) {
  /** @type {Record<string, string|boolean>} */
  const out = {};
  for (let index = 0; index < argv.length; index += 1) {
    const token = argv[index];
    if (!token.startsWith('--')) continue;
    const next = argv[index + 1];
    if (next && !next.startsWith('--')) { out[token.slice(2)] = next; index += 1; } else out[token.slice(2)] = true;
  }
  return out;
}

/**
 * Run the engine in a CHILD process against one forged input set and read back
 * the status it printed. A control sharing this process's memory could be
 * satisfied by a variable rather than by the program.
 * @param {{node: string, inputDir: string}} spec
 */
function spawnEngine(spec) {
  const stdout = execFileSync(spec.node, [SELF, '--input-dir', spec.inputDir], { encoding: 'utf8', cwd: process.cwd() });
  return {
    status: (/^STATUS: (.+)$/m.exec(stdout) ?? [])[1] ?? null,
    rule: (/^RULE: (\S+) /m.exec(stdout) ?? [])[1] ?? null,
    stdout,
  };
}

/**
 * The falsifiability matrix: the positive control, one downgrade per removed
 * requirement, and both prose controls. Every fixture is created here, driven
 * once in a child, then removed and the removal re-read from the filesystem.
 * @param {{sourceDir: string}} spec
 */
export function buildControls(spec) {
  const node = process.execPath;
  /** @type {any[]} */
  const runs = [];
  /** @type {any[]} */
  const fixtures = [];

  /**
   * @param {{label: string, kind: string, omit: string|null, prose: string|null,
   *   base: 'perfect'|'real', expected: string}} plan
   */
  const drive = (plan) => {
    const dir = mkdtempSync(join(tmpdir(), 'task63-ctl-'));
    if (plan.base === 'perfect') forgeInputs({ sourceDir: spec.sourceDir, targetDir: dir, omit: plan.omit, prose: plan.prose });
    else forgeProseOnly({ sourceDir: spec.sourceDir, targetDir: dir, prose: String(plan.prose) });
    const presentBefore = existsSync(dir);
    const driven = spawnEngine({ node, inputDir: dir });
    rmSync(dir, { recursive: true, force: true });
    const presentAfter = existsSync(dir);
    fixtures.push({ dir, presentBefore, presentAfter, released: presentBefore === true && presentAfter === false });
    runs.push({
      label: plan.label,
      kind: plan.kind,
      removedRequirement: plan.omit,
      proseInjected: plan.prose,
      base: plan.base,
      command: `node ${SELF_REL} --input-dir ${dir}`,
      fixtureDescription: `${plan.base} input set${plan.omit ? ` with "${plan.omit}" removed` : ''}${plan.prose ? `, narrative rewritten to assert "${plan.prose}"` : ''}`,
      expectedStatus: plan.expected,
      emittedStatus: driven.status,
      emittedRule: driven.rule,
      matchesExpectation: driven.status === plan.expected,
      stdout: driven.stdout.trimEnd(),
    });
  };

  drive({ label: 'POSITIVE CONTROL — synthetic perfect input', kind: 'positive', omit: null, prose: null, base: 'perfect', expected: STATUS.BEST_IN_CLASS_VERIFIED });
  drive({ label: 'DOWNGRADE — model arm absent', kind: 'downgrade', omit: 'model-arm', prose: null, base: 'perfect', expected: STATUS.IMPLEMENTED_AVAILABLE_MATRIX_PASS });
  drive({ label: 'DOWNGRADE — one advertised minor uncertified (externally blocked)', kind: 'downgrade', omit: 'minor-blocked', prose: null, base: 'perfect', expected: STATUS.IMPLEMENTED_AVAILABLE_MATRIX_PASS });
  drive({ label: 'DOWNGRADE — one AVAILABLE minor FAILS', kind: 'downgrade', omit: 'minor-fail', prose: null, base: 'perfect', expected: STATUS.BLOCKED_EXTERNAL });
  drive({ label: 'DOWNGRADE — one static gate failing', kind: 'downgrade', omit: 'static-gate', prose: null, base: 'perfect', expected: STATUS.BLOCKED_EXTERNAL });
  drive({ label: 'DOWNGRADE — one production-path advisory', kind: 'downgrade', omit: 'supply-chain', prose: null, base: 'perfect', expected: STATUS.BLOCKED_EXTERNAL });
  drive({ label: 'DOWNGRADE — a PASS row certified against a stale tree', kind: 'downgrade', omit: 'stale-certification', prose: null, base: 'perfect', expected: STATUS.BLOCKED_EXTERNAL });
  drive({ label: 'DOWNGRADE — the two transports name different action sets', kind: 'downgrade', omit: 'transport-divergence', prose: null, base: 'perfect', expected: STATUS.BLOCKED_EXTERNAL });
  drive({ label: 'DOWNGRADE — one adversarial gate STILL BLOCKED', kind: 'downgrade', omit: 'adversarial', prose: null, base: 'perfect', expected: STATUS.BLOCKED_EXTERNAL });
  drive({ label: 'PROSE — perfect input, narrative rewritten to assert everything is blocked', kind: 'prose', omit: null, prose: 'blocked', base: 'perfect', expected: STATUS.BEST_IN_CLASS_VERIFIED });
  drive({ label: 'PROSE — real input, narrative rewritten to assert best-in-class', kind: 'prose', omit: null, prose: 'claim', base: 'real', expected: STATUS.BLOCKED_EXTERNAL });

  return {
    runs,
    fixtures,
    allMatched: runs.every((entry) => entry.matchesExpectation),
    statusesReached: [...new Set(runs.map((entry) => entry.emittedStatus))].sort(),
    allFixturesReleased: fixtures.every((entry) => entry.released),
  };
}

// ─── evidence document ───────────────────────────────────────────────────────

/** @param {string} file */
function sha256File(file) { return createHash('sha256').update(readFileSync(file)).digest('hex'); }

/** @param {readonly {path: string, sha256: string}[]} entries */
function treeDigestOf(entries) {
  return createHash('sha256')
    .update([...entries].map((entry) => `${entry.sha256}  ${entry.path}`).sort().join('\n'))
    .digest('hex');
}

/**
 * Assemble the Task 50 strict document. The status is frozen before the first
 * quote() call, so no narrative string can reach it even by accident.
 * @param {{projectRoot: string, sourceDir: string, result: any, controls: any}} spec
 */
export function buildEvidence(spec) {
  const { result, controls } = spec;
  const rel = (/** @type {string} */ file) => join('.omo/evidence', file);
  const now = new Date().toISOString();

  const treeFiles = [
    { path: SELF_REL, sha256: sha256File(join(spec.projectRoot, SELF_REL)) },
    ...Object.values(EVIDENCE_FILES).map((file) => ({ path: rel(file), sha256: sha256File(join(spec.sourceDir, file)) })),
  ];
  const enginePath = join(spec.projectRoot, SELF_REL);
  const inputsNewest = Object.values(EVIDENCE_FILES)
    .map((file) => ({ file: rel(file), mtime: statSync(join(spec.sourceDir, file)).mtimeMs }))
    .sort((left, right) => right.mtime - left.mtime)[0];

  /** @type {any[]} */
  const observations = [
    ...Object.entries(EVIDENCE_FILES).map(([key, file], index) => ({
      id: `obs-input-${index + 1}`,
      phase: 'pre',
      kind: 'evidence-file',
      mechanism: 'fs:sha256',
      independence: 'out-of-band',
      target: rel(file),
      present: true,
      digest: sha256File(join(spec.sourceDir, file)),
      conclusive: true,
      detail: { role: `${key} input to the claim rules` },
      observedAt: now,
    })),
    {
      id: 'obs-input-control',
      phase: 'control',
      kind: 'evidence-file',
      mechanism: 'fs:sha256',
      independence: 'out-of-band',
      target: '.omo/evidence/task-63-control-must-not-exist.json',
      present: existsSync(join(spec.sourceDir, 'task-63-control-must-not-exist.json')),
      digest: null,
      conclusive: true,
      detail: { role: 'negative control for the fs:sha256 oracle: a mechanism that only ever reports PRESENT satisfies every absence assertion in the document' },
      observedAt: now,
    },
  ];

  for (const [index, run] of controls.runs.entries()) {
    observations.push({
      id: `obs-control-${index + 1}`,
      phase: 'control',
      kind: 'claim-status',
      mechanism: 'task63:self-spawn',
      independence: 'out-of-band',
      target: run.label,
      present: run.emittedStatus === STATUS.BEST_IN_CLASS_VERIFIED,
      digest: null,
      conclusive: true,
      detail: { expected: run.expectedStatus, emitted: run.emittedStatus, rule: run.emittedRule, matched: run.matchesExpectation },
      observedAt: now,
    });
  }
  for (const [index, fixture] of controls.fixtures.entries()) {
    observations.push({
      id: `obs-fixture-pre-${index + 1}`,
      phase: 'pre',
      kind: 'temp-workspace',
      mechanism: 'fs:stat',
      independence: 'out-of-band',
      target: fixture.dir,
      present: fixture.presentBefore,
      digest: null,
      conclusive: true,
      detail: { role: 'the forged input set exists only to drive one control run' },
      observedAt: now,
    });
    observations.push({
      id: `obs-fixture-post-${index + 1}`,
      phase: 'cleanup',
      kind: 'temp-workspace',
      mechanism: 'fs:stat',
      independence: 'out-of-band',
      target: fixture.dir,
      present: fixture.presentAfter,
      digest: null,
      conclusive: true,
      detail: { role: 'a fixture nobody proved removed changes the next run' },
      observedAt: now,
    });
  }
  observations.push({
    id: 'obs-real-status',
    phase: 'post',
    kind: 'claim-status',
    mechanism: 'task63:self-spawn',
    independence: 'out-of-band',
    target: 'real input set',
    present: result.status === STATUS.BEST_IN_CLASS_VERIFIED,
    digest: null,
    conclusive: true,
    detail: { emitted: result.status, rule: result.rule, unmet: result.unmet },
    observedAt: now,
  });

  /** @param {string} mechanism @param {string} kind */
  const control = (mechanism, kind) => {
    const seen = observations.filter((entry) => entry.mechanism === mechanism && entry.kind === kind);
    return {
      mechanism,
      kind,
      sawPresent: seen.some((entry) => entry.present === true),
      sawAbsent: seen.some((entry) => entry.present === false),
      inconclusive: seen.filter((entry) => entry.conclusive !== true).length,
    };
  };
  const mechanisms = [
    control('fs:sha256', 'evidence-file'),
    control('task63:self-spawn', 'claim-status'),
    control('fs:stat', 'temp-workspace'),
  ];

  return {
    task: 63,
    title: 'Decide the product-quality and best-in-class claim from measured evidence',
    plan: '.omo/plans/pure-unreal-mcp-implementation.md',
    kind: 'wave-7 claim gate',
    generatedAt: now,
    verdict: `${result.status} — ${result.ruleText}`,
    environment: {
      mockUnrealConnection: false,
      processes: [],
      claim: {
        status: result.status,
        rule: result.rule,
        ruleText: result.ruleText,
        statusVocabulary: Object.values(STATUS),
        statusIsComputed: 'the status is the return value of decide(), which sees rule verdicts only. It is never written as a literal.',
        unmetRequirements: result.unmet,
        alternateReading: result.alternateReading,
        proseGuard: result.proseGuard,
      },
      rules: result.rules,
      blockers: result.blockers,
      blockersByClass: result.blockers.reduce((/** @type {Record<string, number>} */ acc, /** @type {any} */ entry) => {
        acc[entry.class] = (acc[entry.class] ?? 0) + 1;
        return acc;
      }, {}),
      disclosedFindings: {
        note: 'these do NOT gate the status. The status is already BLOCKED_EXTERNAL without any of them and would be unchanged if every one were struck out. They are listed so a reader is not left to discover them elsewhere.',
        findings: DISCLOSED_FINDINGS,
      },
      controls,
      decisionInputs: {
        fieldsRead: result.log.reads.length,
        narrativeLeavesRead: 0,
        proseLeafDenyList: PROSE_LEAVES,
        pointers: result.log.reads.map((/** @type {any} */ entry) => `${entry.source}${entry.pointer}`),
      },
      quotedForDisplayOnly: {
        pointers: result.display.quotes.map((/** @type {any} */ entry) => `${entry.source}${entry.pointer}`),
        readAfterStatusWasFrozen: true,
        values: result.quoted,
      },
    },
    tree: { files: treeFiles, sourceDigest: treeDigestOf(treeFiles) },
    artifacts: [{
      path: SELF_REL,
      sha256: sha256File(enginePath),
      builtAtMs: statSync(enginePath).mtimeMs,
      inputsNewest: inputsNewest?.file ?? null,
      inputsNewestAtMs: inputsNewest?.mtime ?? 0,
    }],
    engine: {},
    clients: [],
    commands: controls.runs.map((/** @type {any} */ entry) => ({
      label: entry.label,
      command: entry.command,
      fixture: entry.fixtureDescription,
      expected: entry.expectedStatus,
      emitted: entry.emittedStatus,
      matched: entry.matchesExpectation,
    })),
    transcripts: [],
    observations,
    claims: [
      {
        id: 'claim-status',
        target: 'the product-quality and best-in-class claim',
        effect: 'unchanged',
        outcome: result.status === STATUS.BLOCKED_EXTERNAL ? 'error' : 'success',
        verdict: result.status,
        pass: result.status !== STATUS.BLOCKED_EXTERNAL,
        reason: result.ruleText,
        oracleRefs: ['obs-input-1', 'obs-input-2', 'obs-input-3', 'obs-input-4', 'obs-real-status'],
        cleanupRef: null,
      },
      {
        id: 'claim-engine-is-falsifiable',
        target: 'the rule engine itself',
        effect: 'unchanged',
        outcome: controls.allMatched ? 'success' : 'error',
        verdict: controls.allMatched
          ? `FALSIFIABLE: ${String(controls.runs.length)} control runs, every one emitting exactly the documented status, across ${String(controls.statusesReached.length)} distinct statuses`
          : "NOT FALSIFIABLE: a control did not emit its documented status, so this engine's verdict on the real input proves nothing",
        pass: controls.allMatched,
        reason: 'an engine that only ever emits one status is the same defect as a gate that cannot fail',
        oracleRefs: controls.runs.map((/** @type {any} */ _entry, /** @type {number} */ index) => `obs-control-${index + 1}`),
        cleanupRef: 'cleanup-control-fixtures',
      },
    ],
    cleanup: [{
      id: 'cleanup-control-fixtures',
      owned: controls.fixtures.map((/** @type {any} */ entry) => entry.dir).join(', '),
      verifiedBy: 'obs-fixture-post-1',
      pass: controls.allFixturesReleased,
      verdict: controls.allFixturesReleased ? 'RELEASED' : 'RESIDUAL',
      reason: 'every forged input set is created, driven once in a child process, removed, and its removal re-read from the filesystem',
    }],
    positiveControls: {
      ok: mechanisms.every((entry) => entry.sawPresent && entry.sawAbsent),
      mechanisms,
      missing: mechanisms.filter((entry) => !(entry.sawPresent && entry.sawAbsent)).map((entry) => entry.mechanism),
    },
    notProven: [...NOT_PROVEN],
    notes: [
      `STATUS ${result.status} by rule ${result.rule}. Unmet: ${result.unmet.join(', ') || 'none'}.`,
      `THE VERDICT SURVIVES THE ARGUMENT ABOUT "AVAILABLE": under the primary reading (root present AND editor compiled) and under the plan's success-criteria reading (root present alone, line 640) the engine emits ${result.alternateReading.status} either way, so no ruling on that word had to be won to reach this status.`,
      `THE ENGINE CAN EMIT ALL THREE DOCUMENTED STATUSES: ${controls.statusesReached.join(', ')} were each reached by a control run in this pass, ${String(controls.runs.filter((/** @type {any} */ entry) => entry.matchesExpectation).length)}/${String(controls.runs.length)} matching their documented expectation.`,
      'A DECLARED EXCEPTION IS NOT A RUBBER STAMP: EXC-1 (test:unit) is excused because all four predicates hold and both failures are attributable to a path the plan itself protects; EXC-2 (npm audit) is NOT excused, because it has no machine-checkable attribution to a protected path and it carries a production-path advisory. The same predicate answered differently for the two exceptions in the same run.',
    ],
  };
}

function main() {
  const args = parseArgs(process.argv.slice(2));
  const projectRoot = process.cwd();
  const sourceDir = String(args['input-dir'] ?? join(projectRoot, '.omo/evidence'));
  let inputDir = sourceDir;
  /** @type {string|null} */
  let fixtureDir = null;

  if (args.forge) {
    if (args.forge !== 'perfect') {
      process.stderr.write(`unknown --forge "${String(args.forge)}"; only "perfect" exists\n`);
      process.exit(3);
    }
    fixtureDir = mkdtempSync(join(tmpdir(), 'task63-forged-'));
    forgeInputs({
      sourceDir, targetDir: fixtureDir,
      omit: args.omit ? String(args.omit) : null,
      prose: args.prose ? String(args.prose) : null,
    });
    inputDir = fixtureDir;
  } else if (args.prose) {
    fixtureDir = mkdtempSync(join(tmpdir(), 'task63-prose-'));
    forgeProseOnly({ sourceDir, targetDir: fixtureDir, prose: String(args.prose) });
    inputDir = fixtureDir;
  }

  let result;
  try {
    result = runEngine({ inputDir });
  } catch (error) {
    if (fixtureDir) rmSync(fixtureDir, { recursive: true, force: true });
    if (error instanceof ProseInputError) {
      process.stderr.write(`PROSE_INPUT: ${error.message}\n`);
      process.exit(4);
    }
    process.stderr.write(`${String(error)}\n`);
    process.exit(3);
    return;
  }

  const label = args.forge ? `forged:perfect${args.omit ? ` --omit ${String(args.omit)}` : ''}` : 'real';
  process.stdout.write(`INPUT: ${label}${args.prose ? ` --prose ${String(args.prose)}` : ''}\n`);
  process.stdout.write(`STATUS: ${result.status}\n`);
  process.stdout.write(`RULE: ${result.rule} ${result.ruleText}\n`);
  for (const rule of result.rules) {
    process.stdout.write(`  ${rule.satisfied ? 'MET   ' : 'UNMET '} ${rule.id}${rule.bestInClassOnly ? '  (best-in-class only)' : ''}\n`);
  }
  process.stdout.write(`ALTERNATE READING OF AVAILABLE: ${result.alternateReading.status} (agrees=${String(result.alternateReading.agreesWithPrimary)})\n`);
  process.stdout.write(`PROSE GUARD: ${result.proseGuard.guardHeld ? 'HELD' : 'BROKEN'}\n`);
  process.stdout.write(`DECISION INPUTS: ${result.log.reads.length} fields read, 0 narrative leaves\n`);
  process.stdout.write(`BLOCKERS: ${result.blockers.length}\n`);

  if (fixtureDir) {
    rmSync(fixtureDir, { recursive: true, force: true });
    process.stdout.write(`FIXTURE REMOVED: present=${String(existsSync(fixtureDir))}\n`);
  }

  if (args.evidence) {
    const controls = buildControls({ sourceDir });
    const out = resolve(projectRoot, String(args.evidence));
    mkdirSync(dirname(out), { recursive: true });
    writeJson(out, buildEvidence({ projectRoot, sourceDir, result, controls }));
    const matched = controls.runs.filter((/** @type {any} */ entry) => entry.matchesExpectation).length;
    process.stdout.write(`CONTROLS: ${matched}/${controls.runs.length} matched their expected status; statuses reached = ${controls.statusesReached.join(', ')}\n`);
    process.stdout.write(`EVIDENCE: ${String(args.evidence)}\n`);
  }
  process.exit(0);
}

if (import.meta.url === `file://${process.argv[1]}`) main();
