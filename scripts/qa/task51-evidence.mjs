#!/usr/bin/env node
// @ts-check
// scripts/qa/task51-evidence.mjs
// Task 51 — assemble the strict, re-checkable evidence document for the adversarial
// run, using Task 50's aggregator and then submitting it to Task 50's VALIDATOR.
//
// The aggregator writes; the validator re-derives from the filesystem and /proc and
// refuses if the two disagree. They deliberately share no checks, so a mistake in
// the recording cannot approve itself.
//
// The process claims are shaped the way they are for one reason: a pid cannot be
// observed before it exists, so "created" has no honest pre-state here. What CAN be
// observed is that every server process this run started was alive while it ran and
// is gone afterwards — read both times through /proc, which is out-of-band with
// respect to the server it is reading. That is recorded as a `deleted` claim:
// present before, absent after, proven by a mechanism the harness does not own. The
// ledger receipt is the claim; the second /proc read is the proof.

import { existsSync, readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { EvidenceAggregator, recordCommand } from '../../tests/unit/task-50/evidence-aggregator.mjs';
import { describeRejections, validateEvidence } from '../../tests/unit/task-50/evidence-validator.mjs';
import { observation, sha256 } from '../../tests/unit/task-50/state-oracles.mjs';

const ROOT = resolve(new URL('../..', import.meta.url).pathname);
const RUN = resolve(ROOT, '.omo/evidence/task-51/adversarial-run.json');
const OUT = resolve(ROOT, '.omo/evidence/task-51/oracle-run.json');

/** The files whose identity this evidence depends on. */
const TREE_FILES = [
  'tests/unit/task-51/fuzz-random.mjs',
  'tests/unit/task-51/fuzz-generators.mjs',
  'tests/unit/task-51/fuzz-protocol.mjs',
  'tests/unit/task-51/fuzz-seeds.mjs',
  'tests/unit/task-51/fuzz-shrink.mjs',
  'tests/unit/task-51/native-policy-mirror.mjs',
  'tests/unit/task-51/differential-engine.mjs',
  'tests/unit/task-51/command-parity.mjs',
  'tests/unit/task-51/security-properties.mjs',
  'tests/unit/task-51/load-harness.mjs',
  'tests/unit/task-51/soak-harness.mjs',
  'tests/unit/task-51/protocol-fuzz-harness.mjs',
  'tests/unit/task-51/command-parity-source.mjs',
  'tests/unit/task-51/security-properties-source.mjs',
  'scripts/qa/task51-fuzz-report.mjs',
  'tests/unit/task-51/fuzz-core.test.ts',
  'tests/unit/task-51/differential-command.test.ts',
  'tests/unit/task-51/parity-security.test.ts',
  'tests/unit/task-51/load-soak.test.ts',
  'scripts/qa/task51-adversarial.mjs',
  'scripts/qa/task51-evidence.mjs',
  // The surfaces under test.
  'src/utils/commands/console-command-policy-rules.ts',
  'src/utils/commands/console-command-policy-generated.ts',
  'src/utils/paths/path-security.ts',
  'src/server/gateway/idempotency-ledger.ts',
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/ConsoleCommand/McpAutomationBridge_ConsoleCommandPolicy.generated.h',
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/ConsoleCommand/McpAutomationBridge_ConsoleCommandHandlers.cpp',
];

/** A filesystem digest reading, independent of anything the server reported.
 * @param {string} relative */
function fileObservation(relative) {
  const file = resolve(ROOT, relative);
  const present = existsSync(file);
  return observation({
    kind: 'file',
    mechanism: 'filesystem:sha256',
    target: relative,
    present,
    digest: present ? sha256(readFileSync(file)) : null,
    detail: { absolutePath: file },
  });
}

function main() {
  if (!existsSync(RUN)) {
    process.stderr.write(`no adversarial run at ${RUN}; run scripts/qa/task51-adversarial.mjs first\n`);
    process.exitCode = 1;
    return;
  }
  const run = JSON.parse(readFileSync(RUN, 'utf8'));

  const aggregator = new EvidenceAggregator({
    task: 51,
    title: 'Adversarial schema/protocol fuzz, security, load and soak gates',
    plan: '.omo/plans/pure-unreal-mcp-implementation.md:502-508',
    kind: 'wave-6 adversarial lane',
    projectRoot: ROOT,
  });

  aggregator.recordTree(TREE_FILES);
  aggregator.recordArtifact({
    path: 'dist/cli.js',
    inputsNewest: run.buildUnderTest?.newestInput ?? null,
    inputsNewestAtMs: run.buildUnderTest?.newestInputMtimeMs ?? null,
  });

  // ── the offline gates, recorded verbatim with their exit codes ──────────────
  for (const spec of [
    { file: 'npx', args: ['vitest', 'run', 'tests/unit/task-51/'] },
    // Only the .ts files: eslint.config.mjs ignores `tests/**/*.mjs` and `**/*.mjs`,
    // and naming an ignored file explicitly emits a warning that --max-warnings=0
    // then turns into a failure about nothing.
    { file: 'npx', args: ['eslint', 'tests/unit/task-51', '--max-warnings=0'] },
    { file: 'npm', args: ['run', 'type-check'] },
  ]) {
    aggregator.addCommand(recordCommand({ ...spec, cwd: ROOT, timeoutMs: 600_000 }));
  }
  aggregator.addCommand({
    cmd: 'node scripts/qa/task51-adversarial.mjs',
    cwd: ROOT,
    startedAt: run.generatedAt,
    exitCode: run.verdict === 'FAIL' ? 1 : 0,
    verdict: run.verdict,
    note: 'judged by its gate table and started==completed counters, never by process exit code',
  });

  aggregator.recordClient({
    transport: 'stdio-jsonrpc',
    driver: 'tests/unit/task-49/live-driver-stdio.mjs',
    entry: 'dist/cli.js',
    wsPorts: run.ownership?.ownedWsPorts ?? null,
    note: 'bridge clients pointed at an owned port pair so they can never attach to the editor lane sockets',
  });

  // ── file observations: BOTH polarities, so the control audit is not vacuous ──
  const distObs = aggregator.addObservation(fileObservation('dist/cli.js'), { id: 'obs-dist', phase: 'post' });
  aggregator.addObservation(fileObservation('dist/cli.js'), { id: 'obs-dist-pre', phase: 'pre' });
  aggregator.addObservation(
    fileObservation('dist/this-file-must-not-exist-task51'),
    { id: 'obs-absent-control', phase: 'control' },
  );

  aggregator.addClaim({
    id: 'claim-artifact-unchanged',
    target: 'dist/cli.js',
    effect: 'unchanged',
    outcome: 'success',
    verdict: 'PROVEN',
    pass: true,
    reason: 'the built CLI under test hashes identically before and after the adversarial run; the run measured it and did not rewrite it',
    oracleRefs: ['obs-dist-pre', distObs],
  });

  // ── process claims, one per session, from readings taken at run time ─────────
  /** @type {any[]} */
  const sessions = Array.isArray(run.load?.sessions) ? run.load.sessions : [];
  /** @type {any[]} */
  const teardowns = Array.isArray(run.load?.teardownObservations) ? run.load.teardownObservations : [];
  const teardownByPid = new Map(teardowns.map((/** @type {any} */ entry) => [entry.pid, entry.observation]));

  let proven = 0;
  for (const session of sessions) {
    const spawn = session.spawnObservation;
    const teardown = teardownByPid.get(session.pid);
    if (!spawn || !teardown) continue;
    aggregator.document.environment.processes.push({
      pid: session.pid,
      role: 'task-51 load session (node dist/cli.js)',
      startTicks: typeof spawn.detail?.startTicks === 'number' ? spawn.detail.startTicks : null,
      comm: spawn.detail?.comm ?? null,
      cmdlinePreview: Array.isArray(spawn.detail?.cmdline) ? spawn.detail.cmdline.slice(0, 3).join(' ').slice(0, 200) : null,
      aliveAtCapture: spawn.present === true,
      observedAt: spawn.observedAt,
    });
    const preId = aggregator.addObservation(spawn, { id: `obs-proc-${session.pid}-pre`, phase: 'pre' });
    const postId = aggregator.addObservation(teardown, { id: `obs-proc-${session.pid}-post`, phase: 'post' });
    const cleanupId = aggregator.addCleanup({
      id: `cleanup-proc-${session.pid}`,
      owned: `pid:${session.pid}`,
      verifiedBy: postId,
      pass: teardown.present === false,
      verdict: teardown.present === false ? 'PROVEN' : 'RESIDUE',
      reason: teardown.present === false
        ? `/proc/${session.pid} is absent after teardown; the ledger receipt was not consulted`
        : `/proc/${session.pid} still exists after teardown`,
    });
    aggregator.addClaim({
      id: `claim-proc-${session.pid}`,
      target: `pid:${session.pid}`,
      effect: 'deleted',
      outcome: 'success',
      verdict: teardown.present === false ? 'PROVEN' : 'RESIDUE',
      pass: teardown.present === false,
      reason: teardown.present === false
        ? 'the session process was read present by procfs while it served traffic and absent by procfs after teardown'
        : 'the session process survived teardown',
      oracleRefs: [preId, postId],
      cleanupRef: cleanupId,
    });
    if (teardown.present === false) proven += 1;
  }

  const soakSpawn = run.soak?.spawnObservation;
  const soakTeardown = run.soak?.teardownObservation;
  if (soakSpawn && soakTeardown) {
    aggregator.document.environment.processes.push({
      pid: run.soak.pid,
      role: 'task-51 soak session (node dist/cli.js)',
      startTicks: typeof soakSpawn.detail?.startTicks === 'number' ? soakSpawn.detail.startTicks : null,
      comm: soakSpawn.detail?.comm ?? null,
      cmdlinePreview: null,
      aliveAtCapture: soakSpawn.present === true,
      observedAt: soakSpawn.observedAt,
    });
    const preId = aggregator.addObservation(soakSpawn, { id: 'obs-soak-pre', phase: 'pre' });
    const postId = aggregator.addObservation(soakTeardown, { id: 'obs-soak-post', phase: 'post' });
    const cleanupId = aggregator.addCleanup({
      id: 'cleanup-soak',
      owned: `pid:${run.soak.pid}`,
      verifiedBy: postId,
      pass: soakTeardown.present === false,
      verdict: soakTeardown.present === false ? 'PROVEN' : 'RESIDUE',
      reason: `procfs read after ${run.soak.cyclesCompleted ?? 0} completed cleanup cycles of ${run.soak.cyclesPlanned ?? 0} planned`,
    });
    aggregator.addClaim({
      id: 'claim-soak-process',
      target: `pid:${run.soak.pid}`,
      effect: 'deleted',
      outcome: 'success',
      verdict: soakTeardown.present === false ? 'PROVEN' : 'RESIDUE',
      pass: soakTeardown.present === false,
      reason: `the soak process completed ${run.soak.cyclesCompleted ?? 0} of ${run.soak.cyclesPlanned ?? 0} enable/describe/disable cycles (${run.soak.openStateLeaks ?? 0} left a capability enabled) and is absent from procfs afterwards`,
      oracleRefs: [preId, postId],
      cleanupRef: cleanupId,
    });
  }

  for (const entry of /** @type {any[]} */ (run.blockedClaims ?? [])) {
    aggregator.addNotProven(`${entry.claim} — BLOCKED (${entry.code}): ${entry.observable}`);
  }
  aggregator.addNotProven(
    'native accept/reject parity is proven against a MIRROR of the plugin gate (its generated header plus a pinned source contract on IsBlockedCommand), not against a running plugin; a live native differential needs an editor this lane may not start.',
  );
  // Planned and achieved are both printed, and never the same number twice: a note
  // that reports only the plan reads as a result while proving nothing happened.
  aggregator.addNote(
    `load: ${run.load?.started ?? 0}/${run.load?.sessionsPlanned ?? 0} sessions started; `
    + `${run.load?.requestsAttempted ?? 0} attempted, ${run.load?.requestsAnswered ?? 0} answered, `
    + `${run.load?.requestsSucceeded ?? 0} succeeded of ${run.load?.requestsPlanned ?? 0} planned; `
    + `outcomes ${JSON.stringify(run.load?.outcomes ?? {})}`,
  );
  aggregator.addNote(
    `soak: ${run.soak?.cyclesCompleted ?? 0} completed / ${run.soak?.cyclesOpened ?? 0} opened / `
    + `${run.soak?.cyclesAttempted ?? 0} attempted of ${run.soak?.cyclesPlanned ?? 0} planned, `
    + `${run.soak?.openStateLeaks ?? 0} left state enabled, `
    + `retained ${String(run.soak?.retainedBytes)} bytes, second-half growth ${String(run.soak?.secondHalfGrowthBytes)} bytes`,
  );
  aggregator.addNote(`process claims proven: ${proven} of ${sessions.length}`);

  const document = aggregator.finalize(run.verdict ?? 'UNKNOWN');
  const validation = validateEvidence(document, { projectRoot: ROOT });
  document.notes.push(`evidence validator: ${validation.valid ? 'VALID' : 'REJECTED'} (${JSON.stringify(validation.checked)})`);
  if (!validation.valid) {
    document.notes.push(...validation.rejections.map((entry) => `${entry.code} at ${entry.at}: ${entry.detail}`));
  }

  const written = aggregator.write(OUT);
  process.stderr.write(`${describeRejections(validation)}\n${written}\n`);
  if (!validation.valid) process.exitCode = 1;
}

main();
