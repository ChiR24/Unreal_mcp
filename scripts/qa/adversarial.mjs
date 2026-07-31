#!/usr/bin/env node
// @ts-check
// scripts/qa/adversarial.mjs
// Task 51 — the live half: 32 isolated sessions, 1,000 mixed requests, a 500-cycle
// cleanup soak, and the retained-RSS gates.
//
// OWNERSHIP IS CHECKED BEFORE ANYTHING IS STARTED, AND AGAIN AFTER.
// A concurrent lane owns the editors, the plugin builds, UBT, ports 3000/8090/8091
// and the /tmp/opencode certification projects. This script therefore:
//   * never launches an editor and never invokes Build.sh or UBT,
//   * binds nothing, and points its bridge clients at an OWNED port pair
//     (8190/8191) so they can never attach to the other lane's editor,
//   * refuses to run at all if it observes the other lane holding a resource,
//   * re-checks afterwards and records what it saw, so an operator can tell a
//     clean run from one that raced.
//
// It measures the Node surface honestly and says so: the editor-side gates
// (retained editor RSS, residual UObjects and delegates) require an editor this
// script is not allowed to start, and are reported as BLOCKED with the observable
// condition rather than estimated.
//
// The build under test is the BUILT CLI, and assertDistFresh (reached through the
// Task 49 stdio driver) refuses a stale dist/ before a single child is spawned.

import { execFileSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import { readFileSync, existsSync, statSync } from 'node:fs';
import { resolve } from 'node:path';

import { writeRedactedEvidence } from '../../tests/unit/live-drivers/live-resource-ledger.mjs';
import { checkDistFreshness } from '../../tests/unit/cross-transport/dist-freshness.mjs';
import { runLoad, OWNED_WS_PORTS, processAlive, summariseRss } from '../../tests/unit/adversarial/load-harness.mjs';
import { runSoak, runProcessResidueSoak } from '../../tests/unit/adversarial/soak-harness.mjs';
import { runProtocolFuzz } from '../../tests/unit/adversarial/protocol-fuzz-harness.mjs';
import { BUDGETS, RSS_LIMITS, SEEDS, TIME_LIMITS_MS } from '../../tests/unit/adversarial/fuzz-seeds.mjs';

const ROOT = resolve(new URL('../..', import.meta.url).pathname);

/** Ports the OTHER lane owns. Observing a listener on any of them is a blocker. */
const FOREIGN_PORTS = Object.freeze([3000, 8090, 8091]);

/** @param {string[]} argv */
function parseArgs(argv) {
  /** @type {{ out: string, sessions: number, requests: number, cycles: number, residueRounds: number, phases: string }} */
  const out = {
    out: '.omo/evidence/adversarial/adversarial-run.json',
    sessions: BUDGETS.loadSessions,
    requests: BUDGETS.loadRequests,
    cycles: BUDGETS.soakCycles,
    residueRounds: 12,
    /** Which phases to run. The 32-session load is the only CPU-heavy one, so it can
     * be deferred to a quiet window independently of the single-process phases. */
    phases: 'protocol,load,soak,residue',
  };
  for (let i = 0; i < argv.length; i += 1) {
    const [flag, inline] = argv[i].split('=');
    const value = inline ?? argv[i + 1];
    if (flag === '--out') { out.out = value; if (inline === undefined) i += 1; }
    if (flag === '--sessions') { out.sessions = Number(value); if (inline === undefined) i += 1; }
    if (flag === '--requests') { out.requests = Number(value); if (inline === undefined) i += 1; }
    if (flag === '--cycles') { out.cycles = Number(value); if (inline === undefined) i += 1; }
    if (flag === '--residue-rounds') { out.residueRounds = Number(value); if (inline === undefined) i += 1; }
    if (flag === '--phases') { out.phases = value; if (inline === undefined) i += 1; }
  }
  return out;
}

/** @param {string} command @param {string[]} args */
function capture(command, args) {
  try {
    return execFileSync(command, args, { encoding: 'utf8', timeout: 20_000 });
  } catch (error) {
    const shell = /** @type {{ stdout?: string }} */ (error);
    return typeof shell.stdout === 'string' ? shell.stdout : '';
  }
}

/**
 * What the other lane is holding, observed rather than assumed.
 * Reported as a list of concrete observations so a BLOCKED verdict names a
 * condition someone else can check.
 */
function observeForeignOwnership() {
  const processes = capture('ps', ['-eo', 'pid,lstart,cmd']);
  /** @type {string[]} */
  const held = [];
  for (const line of processes.split('\n')) {
    if (/UnrealEditor|RunUAT|UnrealBuildTool|Build\.sh/u.test(line) && !/\bgrep\b/u.test(line)) {
      held.push(`process: ${line.trim().slice(0, 200)}`);
    }
  }
  const listeners = capture('ss', ['-ltnp']);
  for (const port of FOREIGN_PORTS) {
    const pattern = new RegExp(`:${port}\\b`, 'u');
    for (const line of listeners.split('\n')) {
      if (pattern.test(line)) held.push(`listener on port ${port}: ${line.trim().slice(0, 200)}`);
    }
  }
  return { held, quiet: held.length === 0, sampledAt: new Date().toISOString() };
}

/** @param {string} relative */
function hashFile(relative) {
  const file = resolve(ROOT, relative);
  if (!existsSync(file)) return null;
  return {
    path: relative,
    sha256: createHash('sha256').update(readFileSync(file)).digest('hex'),
    builtAtMs: statSync(file).mtimeMs,
  };
}

async function main() {
  const options = parseArgs(process.argv.slice(2));
  const phases = new Set(options.phases.split(',').map((entry) => entry.trim()).filter(Boolean));
  const startedAt = Date.now();

  // ── preflight ───────────────────────────────────────────────────────────────
  const ownershipBefore = observeForeignOwnership();
  const freshness = checkDistFreshness(ROOT);

  /** @type {Record<string, unknown>} */
  const report = {
    task: 51,
    kind: 'task-51-adversarial-live',
    generatedAt: new Date().toISOString(),
    mandate: 'Node-surface load and cleanup soak with no editor, no build, and no foreign port touched.',
    ownership: {
      foreignResources: FOREIGN_PORTS,
      before: ownershipBefore,
      ownedWsPorts: OWNED_WS_PORTS,
      rationale: 'the bridge clients are pointed at an owned port pair so they cannot attach to the other lane editor even if one appears mid-run',
    },
    buildUnderTest: {
      entry: freshness.entry,
      fresh: freshness.fresh,
      newestInput: freshness.newestInput,
      // Recorded so the evidence validator can re-check that the artifact under
      // test was not behind its sources. Without both timestamps a STALE_PACKAGE
      // cannot be ruled out, which is why the validator refuses the pair's absence.
      newestInputMtimeMs: freshness.newestInputMtimeMs ?? null,
      entryMtimeMs: freshness.entryMtimeMs ?? null,
      artifact: hashFile('dist/cli.js'),
    },
    budgets: { sessions: options.sessions, requests: options.requests, cycles: options.cycles, residueRounds: options.residueRounds },
    phasesRequested: [...phases],
    seeds: { load: SEEDS.load, soak: SEEDS.soak },
    limits: { rss: RSS_LIMITS, time: TIME_LIMITS_MS },
  };

  // A foreign listener is a hard stop for EVERY phase: it means something is on a
  // port this run must never speak to. A foreign BUILD is different — it shares only
  // CPU. It therefore blocks the 32-session load, whose throughput and retained-RSS
  // deltas would be measured under contention (and which would meaningfully slow the
  // other lane), while the single-process phases, which cost about one core and
  // measure one process's own memory, may proceed.
  const foreignListener = ownershipBefore.held.some((entry) => entry.startsWith('listener on port'));
  const foreignBuild = ownershipBefore.held.some((entry) => entry.startsWith('process:'));
  if (foreignListener) {
    report.verdict = 'BLOCKED';
    report.blocker = {
      code: 'FOREIGN_LISTENER_HELD',
      detail: 'a listener is bound on one of ports 3000/8090/8091; no phase may run while a surface this lane does not own is live',
      observed: ownershipBefore.held,
    };
    writeRedactedEvidence(resolve(ROOT, options.out), report);
    process.stderr.write(`BLOCKED: ${JSON.stringify(ownershipBefore.held, null, 2)}\n`);
    return;
  }
  if (foreignBuild && phases.has('load')) {
    phases.delete('load');
    report.loadDeferred = {
      code: 'FOREIGN_BUILD_CONTENTION',
      detail: 'another lane is running RunUAT/UnrealBuildTool/clang; a 32-session load would be measured under CPU contention and would materially slow that build',
      observed: ownershipBefore.held.filter((entry) => entry.startsWith('process:')),
    };
  }
  if (!freshness.fresh) {
    report.verdict = 'BLOCKED';
    report.blocker = {
      code: 'STALE_DIST',
      detail: 'dist/ is behind src/; measuring it would measure the previous build, not this tree',
      observed: [`entry ${freshness.entry}`, `newest input ${String(freshness.newestInput)}`],
    };
    writeRedactedEvidence(resolve(ROOT, options.out), report);
    process.stderr.write('BLOCKED: stale dist/. Run `npm run build` first.\n');
    return;
  }

  // ── protocol fuzz: the malformed corpus, on the wire of the real server ─────
  /** @type {any} */
  let protocolFuzz = { started: false, sent: 0, survived: null, answeredAfterFuzz: null, checkpoints: [], malformedStdoutLines: 0, releasedIndependently: null };
  if (phases.has('protocol')) {
    process.stderr.write(`protocol fuzz: ${BUDGETS.protocolCases} generated frames plus raw malformations\n`);
    protocolFuzz = await runProtocolFuzz({
      frames: BUDGETS.protocolCases,
      seed: SEEDS.protocol,
      cwd: ROOT,
    });
    report.protocolFuzz = protocolFuzz;
  }

  // ── load ────────────────────────────────────────────────────────────────────
  /** @type {any} */
  let load = { started: 0, sessionsPlanned: 0, startFailures: [], outcomes: {}, sessions: [], receipts: { total: 0, released: 0, leaked: 0, receipts: [] }, requestsPlanned: 0, requestsAttempted: 0, requestsAnswered: 0, requestsSucceeded: 0, teardownObservations: [] };
  if (phases.has('load')) {
    process.stderr.write(`load: ${options.sessions} sessions / ${options.requests} requests\n`);
    load = await runLoad({
      sessions: options.sessions,
      requests: options.requests,
      seed: SEEDS.load,
      cwd: ROOT,
      onProgress: (done, total) => process.stderr.write(`  ${done}/${total}\n`),
    });
    report.load = load;
  }

  // ── soak ────────────────────────────────────────────────────────────────────
  /** @type {any} */
  let soak = { started: false, cyclesPlanned: 0, cyclesAttempted: 0, cyclesOpened: 0, cyclesCompleted: 0, openStateLeaks: 0, failures: [], samples: [] };
  if (phases.has('soak')) {
    process.stderr.write(`soak: ${options.cycles} cleanup cycles\n`);
    soak = await runSoak({
      cycles: options.cycles,
      seed: SEEDS.soak,
      cwd: ROOT,
      onProgress: (done, total) => process.stderr.write(`  ${done}/${total}\n`),
    });
    report.soak = soak;
  }

  // ── process residue ─────────────────────────────────────────────────────────
  /** @type {any} */
  let residue = { roundsPlanned: 0, roundsOpened: 0, roundsCompleted: 0, pids: [], residue: [], survivors: [] };
  if (phases.has('residue')) {
    process.stderr.write(`residue: ${options.residueRounds} spawn/close rounds\n`);
    residue = await runProcessResidueSoak({ rounds: options.residueRounds, cwd: ROOT });
    report.processResidue = residue;
  }

  // ── gates ───────────────────────────────────────────────────────────────────
  const rss = summariseRss(load.sessions);
  const worstRetained = rss.worstRetainedBytes;
  const survivors = [
    ...residue.survivors,
    ...load.sessions.map(/** @param {any} s */ (s) => s.pid).filter(/** @param {any} pid */ (pid) => processAlive(pid)),
  ];

  const gates = {
    protocolFuzzSurvived: {
      sent: protocolFuzz.sent,
      survived: protocolFuzz.survived,
      answeredAfterFuzz: protocolFuzz.answeredAfterFuzz,
      wedgedCheckpoints: (protocolFuzz.checkpoints ?? []).filter(/** @param {any} entry */ (entry) => !entry.answered || !entry.alive),
      // Survival alone is not enough: a wedged parser is alive and mute. The server
      // must still ANSWER a well-formed request after the whole corpus.
      pass: protocolFuzz.started === true
        && protocolFuzz.survived === true
        && protocolFuzz.answeredAfterFuzz === true
        && (protocolFuzz.checkpoints ?? []).every(/** @param {any} entry */ (entry) => entry.answered && entry.alive),
    },
    protocolFuzzStdoutPure: {
      malformedStdoutLines: protocolFuzz.malformedStdoutLines,
      pass: protocolFuzz.malformedStdoutLines === 0,
    },
    protocolFuzzProcessReleased: {
      released: protocolFuzz.releasedIndependently,
      pass: protocolFuzz.releasedIndependently === true,
    },
    // Measured against the POST-WARM-UP baseline. Kept because it is the plan's literal
    // wording, but it is reported INVALID rather than passed when that baseline turns out
    // to have been a peak — see summariseRss. A comparison that cannot fail is not a gate,
    // and scoring one as green is precisely the defect this suite was built to catch.
    nodeRetainedRss: {
      limitBytes: RSS_LIMITS.nodeRetainedBytes,
      worstBytes: worstRetained,
      baselineWasAPeak: rss.baselineWasAPeak,
      measured: `${rss.retainedCount} of ${load.sessions.length} sessions produced a retained-RSS delta`,
      whyInvalid: rss.baselineWasAPeak
        ? 'no session rose above its own post-warm-up baseline, so every delta is negative by construction and <= limit holds for any input, a real leak included; judged by nodeSecondHalfGrowth instead'
        : null,
      pass: rss.baselineWasAPeak
        ? 'INVALID_VACUOUS_BASELINE'
        : worstRetained !== null && worstRetained <= RSS_LIMITS.nodeRetainedBytes,
    },
    // The falsifiable form: growth from a STEADY-STATE trough taken mid-plan, after the
    // start-up transient has decayed. A leak is monotonic, so it still registers here;
    // the initial decay does not. This is the same shape the soak already uses.
    nodeSecondHalfGrowth: {
      limitBytes: RSS_LIMITS.nodeRetainedBytes,
      worstBytes: rss.worstGrowthBytes,
      measured: `${rss.growthCount} of ${load.sessions.length} sessions produced a steady-state growth figure`,
      pass: rss.worstGrowthBytes !== null
        && rss.growthCount === load.sessions.length
        && rss.worstGrowthBytes <= RSS_LIMITS.nodeRetainedBytes,
    },
    soakRetainedRss: {
      limitBytes: RSS_LIMITS.nodeRetainedBytes,
      retainedBytes: soak.retainedBytes ?? null,
      secondHalfGrowthBytes: soak.secondHalfGrowthBytes ?? null,
      pass: typeof soak.retainedBytes === 'number' && soak.retainedBytes <= RSS_LIMITS.nodeRetainedBytes,
    },
    zeroProcessResidue: { survivors, pass: survivors.length === 0 },
    // "No survivors" is vacuously true when no round ever spawned anything, so the
    // rounds that actually ran are gated alongside it.
    residueRoundsCompleted: {
      planned: options.residueRounds,
      opened: residue.roundsOpened,
      completed: residue.roundsCompleted,
      residue: residue.residue,
      pass: residue.roundsCompleted === options.residueRounds && residue.residue.length === 0,
    },
    allSessionsStarted: { started: load.started, expected: options.sessions, pass: load.started === options.sessions },
    // D1: the old gate compared requestsIssued against the plan length. The plan
    // loop cannot exit early, so that counter WAS the plan length and the gate was
    // a tautology — 1,000 timeouts scored exactly like 1,000 correct answers. It is
    // replaced by two gates that a failing run can actually move: ANSWERED (a frame
    // came back) and SUCCEEDED (the frame was the one the request kind demands).
    allRequestsAnswered: {
      answered: load.requestsAnswered,
      attempted: load.requestsAttempted,
      expected: options.requests,
      pass: load.requestsAttempted === options.requests && load.requestsAnswered === options.requests,
    },
    allRequestsSucceeded: {
      succeeded: load.requestsSucceeded,
      expected: options.requests,
      byOutcome: load.outcomes,
      pass: load.requestsSucceeded === options.requests,
    },
    // D3: the old gate read only `failures.length === 0`, so a soak that never
    // started — or one whose every cycle enabled nothing — passed with an empty
    // failures array and zero cycles. Completion is now counted per cycle and must
    // equal the plan, and a cycle that opened state it could not close is fatal.
    soakCyclesCompleted: {
      started: soak.started,
      completed: soak.cyclesCompleted ?? 0,
      opened: soak.cyclesOpened ?? 0,
      attempted: soak.cyclesAttempted ?? 0,
      expected: options.cycles,
      openStateLeaks: soak.openStateLeaks ?? 0,
      blocked: soak.blocked ?? null,
      failures: (soak.failures ?? []).slice(0, 10),
      failureCount: (soak.failures ?? []).length,
      pass: soak.started === true
        && (soak.cyclesCompleted ?? 0) === options.cycles
        && (soak.openStateLeaks ?? 0) === 0
        && (soak.failures ?? []).length === 0,
    },
    noMalformedStdout: {
      load: load.sessions.reduce(/** @param {number} sum @param {any} session */ (sum, session) => sum + session.malformedStdoutLines, 0),
      soak: soak.malformedStdoutLines ?? 0,
      pass: load.sessions.every(/** @param {any} session */ (session) => session.malformedStdoutLines === 0) && (soak.malformedStdoutLines ?? 0) === 0,
    },
    executeFailsClosedWithoutBridge: {
      unexpected: Object.entries(load.outcomes).filter(([key]) => key.endsWith(':UNEXPECTED')),
      pass: Object.keys(load.outcomes).every((key) => !key.endsWith(':UNEXPECTED')),
    },
    cleanupReceiptsVerified: {
      leaked: load.receipts.leaked,
      total: load.receipts.total,
      pass: load.receipts.leaked === 0,
    },
  };
  report.gates = gates;

  report.blockedClaims = [
    {
      claim: 'editor retained RSS <= 64 MiB after warm-up',
      code: 'EDITOR_OWNED_BY_ANOTHER_LANE',
      observable: 'the concurrent certification lane holds exclusive ownership of editors, plugin builds, UBT and ports 3000/8090/8091; this run launched no editor and therefore measured none',
    },
    {
      claim: 'zero residual UObjects and delegates after the soak',
      code: 'EDITOR_OWNED_BY_ANOTHER_LANE',
      observable: 'residual UObject and delegate counts are only observable from inside a running editor',
    },
    {
      claim: 'native /mcp 32-session load and live native accept/reject parity',
      code: 'EDITOR_OWNED_BY_ANOTHER_LANE',
      observable: 'the native transport is served by the plugin inside an editor on port 3000, which this run may not start or bind',
    },
  ];

  const ownershipAfter = observeForeignOwnership();
  /** @type {any} */ (report.ownership).after = ownershipAfter;
  /** @type {any} */ (report.ownership).raced = !ownershipAfter.quiet;

  // A phase that did not run is reported as SKIPPED and judged by nobody. Scoring a
  // skipped phase as a pass is how a deferred measurement becomes a claimed one.
  for (const [name, gate] of Object.entries(gates)) {
    const phase = name.startsWith('protocolFuzz') ? 'protocol'
      : name.startsWith('soak') ? 'soak'
        : (name === 'zeroProcessResidue' || name === 'residueRoundsCompleted') ? 'residue'
          : 'load';
    if (!phases.has(phase)) /** @type {any} */ (gate).pass = 'SKIPPED';
  }
  const failed = Object.entries(gates).filter(([, gate]) => /** @type {any} */ (gate).pass !== true && /** @type {any} */ (gate).pass !== 'SKIPPED');
  const skipped = Object.entries(gates).filter(([, gate]) => /** @type {any} */ (gate).pass === 'SKIPPED').map(([name]) => name);
  report.skippedGates = skipped;
  const failedGates = failed.map(([name]) => name);
  report.verdict = failed.length > 0
    ? 'FAIL'
    : skipped.length > 0
      ? 'PARTIAL_PHASES_DEFERRED'
      : 'PASS_WITH_BLOCKED_EDITOR_CLAIMS';
  report.failedGates = failedGates;
  report.elapsedMs = Date.now() - startedAt;

  const written = writeRedactedEvidence(resolve(ROOT, options.out), report);
  process.stderr.write(`${report.verdict}: ${written}\n`);
  if (failed.length > 0) process.stderr.write(`failed gates: ${failedGates.join(', ')}\n`);
}

main().catch((error) => {
  process.stderr.write(`adversarial failed: ${error instanceof Error ? error.stack : String(error)}\n`);
  process.exitCode = 1;
});
