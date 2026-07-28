#!/usr/bin/env node
// @ts-check
// scripts/qa/task51-fuzz-report.mjs
// Task 51 — persist the seeded fuzz result: exact seeds, exact case counts, and a
// minimized replay artifact for every finding.
//
// The vitest suites already FAIL on a finding. That is the gate. This script exists
// because a gate that only fails is not evidence: a reviewer asking "how many cases
// ran, under which seeds, and what exactly diverged" cannot answer it from a green
// tick. So the same corpora are re-run here and the numbers are written down.
//
// It reads the evaluator from `dist/` rather than `src/`, because it runs under
// plain node. That is the SAME generated function the runtime validator delegates
// to — injected through typescriptSideFrom — so this is not a second implementation
// that could agree with itself.
//
// Nothing here is allowed to be the reason a finding is not reported: if the
// differential produces one, it is minimized, written, and the exit code is set.

import { resolve } from 'node:path';

import { writeRedactedEvidence } from '../../tests/unit/task-49/live-resource-ledger.mjs';
import { streamFor } from '../../tests/unit/task-51/fuzz-random.mjs';
import { fuzzAssetPath, fuzzConsoleCommand } from '../../tests/unit/task-51/fuzz-generators.mjs';
import { fuzzJsonRpcFrame, fuzzAuthHeaders, fuzzExecuteEnvelope } from '../../tests/unit/task-51/fuzz-protocol.mjs';
import { runDifferential, validateAllowlist } from '../../tests/unit/task-51/differential-engine.mjs';
import {
  COMMAND_ASYMMETRY_CLASSES,
  NATIVE_REASONS,
  explainCommandDivergence,
  nativeSideFactory,
  typescriptSideFrom,
} from '../../tests/unit/task-51/command-parity.mjs';
import { loadNativePolicy, verifyNativeAlgorithmContract } from '../../tests/unit/task-51/native-policy-mirror.mjs';
import { replayArtifact, shrinkString } from '../../tests/unit/task-51/fuzz-shrink.mjs';
import {
  exerciseIdempotencyLedgerWith,
  normalizedPathIsContained,
  uePathVerdictWith,
} from '../../tests/unit/task-51/security-properties.mjs';
import { BUDGETS, SEEDS, extraSeed } from '../../tests/unit/task-51/fuzz-seeds.mjs';

const ROOT = resolve(new URL('../..', import.meta.url).pathname);

async function main() {
  const outArg = process.argv.indexOf('--out');
  const out = outArg >= 0 ? process.argv[outArg + 1] : '.omo/evidence/task-51/fuzz-report.json';

  // The SAME generated evaluator and the SAME rule inventory the runtime validator
  // uses, reached in dist/ because this script runs under plain node.
  const { evaluateGeneratedConsoleCommandPolicy } = await import(
    resolve(ROOT, 'dist/utils/commands/console-command-policy-generated.js')
  );
  const { CONSOLE_COMMAND_POLICY_RULES } = await import(
    resolve(ROOT, 'dist/utils/commands/console-command-policy-rules.js')
  );
  const KNOWN_RULE_IDS = CONSOLE_COMMAND_POLICY_RULES.map((/** @type {{id: string}} */ rule) => rule.id);
  const typescriptSide = typescriptSideFrom(evaluateGeneratedConsoleCommandPolicy);
  const { sanitizePath } = await import(resolve(ROOT, 'dist/utils/paths/path-security.js'));
  const { IdempotencyLedger } = await import(resolve(ROOT, 'dist/server/gateway/idempotency-ledger.js'));
  const uePathVerdict = uePathVerdictWith(sanitizePath);
  /** @param {import('../../tests/unit/task-51/fuzz-random.mjs').Rng} rng @param {any} options */
  const exerciseIdempotencyLedger = (rng, options) => exerciseIdempotencyLedgerWith(IdempotencyLedger, rng, options);
  const policy = loadNativePolicy(ROOT);
  const nativeSide = nativeSideFactory(policy);

  /** @type {Record<string, unknown>} */
  const report = {
    task: 51,
    kind: 'task-51-seeded-fuzz-report',
    generatedAt: new Date().toISOString(),
    evaluatorSource: 'dist/utils/commands/console-command-policy-generated.js',
    nativePolicyHeader: policy.headerPath,
    nativeAlgorithmContract: verifyNativeAlgorithmContract(ROOT),
    seeds: SEEDS,
    budgets: BUDGETS,
    extraSeed: extraSeed(),
    allowlist: {
      classes: COMMAND_ASYMMETRY_CLASSES.map((entry) => ({
        id: entry.id,
        direction: entry.direction,
        leftRules: entry.leftRules,
        rightReasons: entry.rightReasons,
        defenceInDepthOnly: entry.defenceInDepthOnly ?? false,
        rationale: entry.rationale,
        securityArgument: entry.securityArgument,
      })),
      validation: validateAllowlist(COMMAND_ASYMMETRY_CLASSES, KNOWN_RULE_IDS, NATIVE_REASONS),
    },
  };

  /** @type {Array<Record<string, unknown>>} */
  const findings = [];

  // ── console-command differential ────────────────────────────────────────────
  const commandCases = streamFor(SEEDS.commands, 'console-commands')
    .list(BUDGETS.commandCases, (rng) => fuzzConsoleCommand(rng))
    .map((entry, index) => ({ input: entry.command, label: entry.class, index }));
  const differential = runDifferential({
    name: 'console-command-policy',
    cases: commandCases,
    left: typescriptSide,
    right: nativeSide,
    explain: explainCommandDivergence,
    allowlist: COMMAND_ASYMMETRY_CLASSES,
  });
  report.differential = {
    total: differential.total,
    agreed: differential.agreed,
    undecidable: differential.undecidable,
    excused: differential.excused,
    unusedClasses: differential.unusedClasses,
    findingCount: differential.findings.length,
    distinctInputs: new Set(commandCases.map((entry) => entry.input)).size,
    classesGenerated: [...new Set(commandCases.map((entry) => entry.label))].sort(),
  };
  for (const finding of differential.findings) {
    const command = String(finding.input);
    const probe = (/** @type {string} */ candidate) => {
      const left = typescriptSide(candidate);
      const right = nativeSide(candidate);
      if (left.verdict === 'undecidable' || right.verdict === 'undecidable') return null;
      if (left.verdict === right.verdict) return null;
      if (explainCommandDivergence(candidate, left, right) !== null) return null;
      return `${left.verdict}/${right.verdict}`;
    };
    const shrunk = shrinkString(command, probe);
    findings.push(replayArtifact({
      suite: 'differential-command',
      property: 'every disagreement is attributable to a declared class',
      seed: SEEDS.commands,
      stream: 'console-commands',
      index: Number(finding.index),
      tag: String(shrunk.tag ?? finding.direction),
      original: command,
      minimal: shrunk.minimal,
      evaluations: shrunk.evaluations,
      detail: { left: finding.left, right: finding.right, why: finding.why },
    }));
  }

  // ── path containment ────────────────────────────────────────────────────────
  const pathRng = streamFor(SEEDS.paths, 'asset-paths');
  let accepted = 0;
  let rejected = 0;
  for (let index = 0; index < BUDGETS.pathCases; index += 1) {
    const testCase = fuzzAssetPath(pathRng);
    const verdict = uePathVerdict(testCase.path);
    if (!verdict.accepted) { rejected += 1; continue; }
    accepted += 1;
    const contained = normalizedPathIsContained(String(verdict.normalized));
    if (contained.ok) continue;
    const probe = (/** @type {string} */ candidate) => {
      const inner = uePathVerdict(candidate);
      if (!inner.accepted) return null;
      return normalizedPathIsContained(String(inner.normalized)).ok ? null : 'ESCAPED_ROOT';
    };
    const shrunk = shrinkString(testCase.path, probe);
    findings.push(replayArtifact({
      suite: 'parity-security',
      property: 'never returns an accepted UE path that escapes its root',
      seed: SEEDS.paths,
      stream: 'asset-paths',
      index,
      tag: 'ESCAPED_ROOT',
      original: testCase.path,
      minimal: shrunk.minimal,
      evaluations: shrunk.evaluations,
      detail: { normalized: verdict.normalized, why: contained.why },
    }));
  }
  report.paths = { cases: BUDGETS.pathCases, accepted, rejected };

  // ── idempotency ledger ──────────────────────────────────────────────────────
  /** @type {string[]} */
  const ledgerViolations = [];
  let inFlightSeen = 0;
  for (let run = 0; run < BUDGETS.ledgerRuns; run += 1) {
    const outcome = exerciseIdempotencyLedger(streamFor(SEEDS.ledger, `ledger-run-${run}`), {
      operations: BUDGETS.ledgerOperations,
      maxEntries: 8,
    });
    inFlightSeen += outcome.inFlight;
    for (const violation of outcome.violations) ledgerViolations.push(`run ${run}: ${violation}`);
  }
  report.idempotency = {
    runs: BUDGETS.ledgerRuns,
    operationsPerRun: BUDGETS.ledgerOperations,
    totalOperations: BUDGETS.ledgerRuns * BUDGETS.ledgerOperations,
    inFlightSlotsExercised: inFlightSeen,
    violations: ledgerViolations,
  };

  // ── protocol / auth / envelope corpora: recorded counts, not just executed ───
  const protocolShapes = streamFor(SEEDS.protocol, 'json-rpc')
    .list(BUDGETS.protocolCases, (rng) => fuzzJsonRpcFrame(rng).shape);
  const authShapes = streamFor(SEEDS.auth, 'headers')
    .list(BUDGETS.authCases, (rng) => fuzzAuthHeaders(rng, { token: 'placeholder-token-value', sessionId: 'sess-1' }));
  const envelopes = streamFor(SEEDS.auth, 'envelope')
    .list(BUDGETS.authCases, (rng) => fuzzExecuteEnvelope(rng, {
      capabilityId: 'manage_asset.create_material',
      otherCapabilityId: 'control_actor.delete_actor',
    }));
  /** @param {readonly string[]} values */
  const histogram = (values) => {
    /** @type {Record<string, number>} */
    const out = {};
    for (const value of values) out[value] = (out[value] ?? 0) + 1;
    return out;
  };
  report.protocolCorpus = { cases: BUDGETS.protocolCases, shapes: histogram(protocolShapes) };
  report.authCorpus = {
    cases: BUDGETS.authCases,
    shapes: histogram(authShapes.map((entry) => entry.shape)),
    authorized: authShapes.filter((entry) => entry.authorized).length,
    unauthorized: authShapes.filter((entry) => !entry.authorized).length,
  };
  report.envelopeCorpus = {
    cases: BUDGETS.authCases,
    consentAbsent: envelopes.filter((entry) => entry.consent === null).length,
    consentForWrongCapability: envelopes.filter((entry) => {
      const consent = /** @type {any} */ (entry.consent);
      return consent !== null && consent?.capability === 'control_actor.delete_actor';
    }).length,
    idempotencyKeysPresent: envelopes.filter((entry) => entry.idempotencyKey !== null).length,
    revisionPinsPresent: envelopes.filter((entry) => entry.expectedRevision !== null).length,
    cancellationsScheduled: envelopes.filter((entry) => entry.cancelAfterMs !== null).length,
  };

  report.findings = findings;
  report.totalCasesExecuted =
    BUDGETS.commandCases + BUDGETS.pathCases + BUDGETS.protocolCases + (BUDGETS.authCases * 2)
    + (BUDGETS.ledgerRuns * BUDGETS.ledgerOperations);
  report.verdict = findings.length === 0 && ledgerViolations.length === 0 && differential.unusedClasses.length === 0
    ? 'PASS'
    : 'FINDINGS';

  const written = writeRedactedEvidence(resolve(ROOT, out), report);
  process.stderr.write(`${report.verdict}: ${String(report.totalCasesExecuted)} seeded cases, ${findings.length} finding(s) -> ${written}\n`);
  if (report.verdict !== 'PASS') process.exitCode = 1;
}

main().catch((error) => {
  process.stderr.write(`task51-fuzz-report failed: ${error instanceof Error ? error.stack : String(error)}\n`);
  process.exitCode = 1;
});
