// @ts-check
// tests/unit/engine-certification/certification-verdict.mjs
// Task 52 — the verdicts a certification cannot read off a return code.
//
// Most of the chain announces its own failure: UBT exits non-zero, packaging
// produces no zip, the Result table comes up short. Three do not, and all three
// were met on this lane rather than imagined:
//
//   THE EDITOR THAT NEVER ANSWERS. "the native port never bound" is one sentence
//   for two different defects — an editor that died on a fatal error thirty
//   seconds in, and an editor that is alive and merely slower than the timeout.
//   The remedies differ, and the evidence differs more: a run that reports
//   "crashed" while the process is still running has also just reported a kill it
//   never performed, and the next run inherits a live editor on a port the report
//   swears is free.
//
//   THE CLEANUP THAT AGREES WITH ITSELF. The first live run of this lane scored
//   port release from its own connect() probe while separately recording an
//   independent /proc/net/tcp reading of the same port — and never compared them.
//   A teardown graded by the mechanism that performed it is precisely the shape
//   that wrote `cleanupClean: true` over two leaked materials in an earlier wave.
//   Here two independent readings must AGREE before anything is called released,
//   and "the reading was inconclusive" is never a release.
//
//   THE TREE THAT MOVED. This worktree is shared with other lanes. A run whose
//   sources changed between its first stage and its last certified no single
//   tree, and the shape it fails in downstream ("STALE_PACKAGE, dist/cli.js is
//   856s behind its inputs") reads like this lane's bug rather than a concurrent
//   edit. Naming it while the run is still going is the difference.
//
// Every judgement here is PURE: it takes readings and returns a verdict. Nothing
// in this file kills, deletes, or touches anything — which is what lets the
// failure injections that exercise it run offline, on every commit, instead of
// once by hand on the day someone remembers.

import { createHash } from 'node:crypto';
import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { observeProcess } from '../evidence-oracles/state-oracles.mjs';

// ───────────────────────────── editor liveness ───────────────────────────────

export const EDITOR_LIVENESS = Object.freeze({
  READY: 'READY',
  NEVER_LAUNCHED: 'NEVER_LAUNCHED',
  CRASHED: 'CRASHED',
  HUNG: 'HUNG',
  PORT_NOT_OURS: 'PORT_NOT_OURS',
});

/**
 * Lines Unreal writes when it DIES, in the order a crash prints them.
 *
 * Deliberately absent: `Ensure condition failed`. An ensure prints a full
 * callstack and the editor carries on — the 5.7 run on this lane printed one and
 * then completed all 84 automation tests. Reading it as a crash would fail a
 * green run, and a suite that cries wolf gets ignored.
 */
export const CRASH_MARKERS = Object.freeze([
  { id: 'CRITICAL_ERROR', pattern: /=== Critical error: ===[^\n]*/u },
  { id: 'FATAL_ERROR', pattern: /Fatal error:[^\n]*/u },
  { id: 'ASSERTION_FAILED', pattern: /Assertion failed:[^\n]*/u },
  { id: 'SIGNAL', pattern: /(?:[Cc]aught signal|SIGSEGV|SIGABRT|SIGBUS|Segmentation fault)[^\n]*/u },
  { id: 'UNHANDLED_EXCEPTION', pattern: /Unhandled Exception:[^\n]*/u },
  { id: 'OUT_OF_MEMORY', pattern: /Ran out of memory allocating[^\n]*/u },
]);

/**
 * Find the first crash marker in a log, reading the TAIL — a process that died
 * printed its reason on the way out, and this lane's live log is truncated from
 * the front once it passes 64 MiB.
 * @param {string} text @param {{ tailBytes?: number }} [options]
 */
export function extractCrashSignature(text, options = {}) {
  const tail = String(text ?? '').slice(-(options.tailBytes ?? 256 * 1024));
  for (const marker of CRASH_MARKERS) {
    const hit = marker.pattern.exec(tail);
    if (hit !== null) return { crashed: true, marker: marker.id, excerpt: hit[0].trim().slice(0, 300) };
  }
  return { crashed: false, marker: null, excerpt: null };
}

/**
 * Why is (or is not) the editor answering?
 *
 * `portReady` alone cannot decide this. A port that answers while THIS run's pid
 * is gone is the failure that looks most like success: something is listening,
 * every driver would score against it, and it is not the editor this run built.
 * @param {{ pid: number|null|undefined, portReady: boolean, logText?: string,
 *   procRoot?: string, observe?: typeof observeProcess }} spec
 */
export function judgeEditorLiveness(spec) {
  const crash = extractCrashSignature(spec.logText ?? '');
  if (spec.pid === null || spec.pid === undefined) {
    return {
      verdict: EDITOR_LIVENESS.NEVER_LAUNCHED, ok: false, alive: false, crash, observation: null,
      detail: 'no editor pid was ever recorded, so the launch itself failed and nothing below it ran against an editor',
    };
  }
  const observation = (spec.observe ?? observeProcess)({ pid: spec.pid, procRoot: spec.procRoot });
  const alive = observation.present === true;
  if (spec.portReady === true) {
    if (alive) {
      return {
        verdict: EDITOR_LIVENESS.READY, ok: true, alive, crash, observation,
        detail: `pid ${spec.pid} is running and answered on its own port`,
      };
    }
    return {
      verdict: EDITOR_LIVENESS.PORT_NOT_OURS, ok: false, alive, crash, observation,
      detail: `something answers on the claimed port but pid ${spec.pid} — the editor THIS run launched — is gone; `
        + 'whatever is listening was not started here and scoring against it would certify another process',
    };
  }
  if (!alive) {
    return {
      verdict: EDITOR_LIVENESS.CRASHED, ok: false, alive, crash, observation,
      detail: crash.crashed
        ? `the editor died before binding its port: [${crash.marker}] ${crash.excerpt}`
        : `pid ${spec.pid} exited before binding its port and its log names no crash marker; the exit is real either way`,
    };
  }
  return {
    verdict: EDITOR_LIVENESS.HUNG, ok: false, alive, crash, observation,
    detail: `pid ${spec.pid} is still running but never bound its port — a hang, not a crash, so cleanup still has an editor to end`,
  };
}

// ────────────────────────────── cleanup agreement ────────────────────────────

export const CLEANUP_AGREEMENT = Object.freeze({
  AGREED_RELEASED: 'AGREED_RELEASED',
  AGREED_LEAKED: 'AGREED_LEAKED',
  DISAGREEMENT: 'DISAGREEMENT',
  INCONCLUSIVE: 'INCONCLUSIVE',
  REAPING: 'REAPING',
});

/**
 * Compare what the teardown BELIEVES against what an independent reading SEES.
 *
 * The two must come from different mechanisms — a connect() probe against
 * /proc/net/tcp, an rm receipt against a directory walk — or this function is
 * comparing a claim to its own echo.
 * @param {{ resource: string, claimedReleased: boolean, claimedBy: string,
 *   observation: { present: boolean|null, conclusive?: boolean, mechanism?: string,
 *     detail?: unknown }|null }} spec
 */
export function judgeCleanupRelease(spec) {
  const seen = spec.observation;
  const mechanism = seen?.mechanism ?? 'no independent reading';
  const row = { resource: spec.resource, claimedReleased: spec.claimedReleased, claimedBy: spec.claimedBy, mechanism };
  if (seen === null || seen === undefined || seen.conclusive === false || seen.present === null) {
    return {
      ...row, verdict: CLEANUP_AGREEMENT.INCONCLUSIVE, ok: false,
      reason: `${mechanism} could not say whether ${spec.resource} is still there`
        + `${seen?.detail === undefined ? '' : ` (${JSON.stringify(seen.detail).slice(0, 160)})`}; `
        + '"we could not look" is not "it is gone"',
    };
  }
  const stillThere = seen.present === true;
  if (spec.claimedReleased && !stillThere) {
    return { ...row, verdict: CLEANUP_AGREEMENT.AGREED_RELEASED, ok: true, reason: `${spec.claimedBy} released ${spec.resource} and ${mechanism} independently finds it gone` };
  }
  if (!spec.claimedReleased && stillThere) {
    return { ...row, verdict: CLEANUP_AGREEMENT.AGREED_LEAKED, ok: false, reason: `${spec.resource} LEAKED: ${spec.claimedBy} could not release it and ${mechanism} still sees it` };
  }
  return {
    ...row, verdict: CLEANUP_AGREEMENT.DISAGREEMENT, ok: false,
    reason: spec.claimedReleased
      ? `${spec.claimedBy} reported ${spec.resource} released but ${mechanism} still sees it — the teardown graded itself and was wrong`
      : `${spec.claimedBy} reported ${spec.resource} NOT released but ${mechanism} cannot find it; the two readings contradict, so neither can be reported as the outcome`,
  };
}

/**
 * A process is released when a signal-0 probe and procfs agree it is gone.
 *
 * The zombie branch exists because the two mechanisms genuinely differ there and
 * only one of them is right: `kill(pid, 0)` succeeds for a zombie (the pid is
 * still in the table) while procfs reports it not-present (state Z). A zombie
 * runs no code, holds no port and vanishes the moment its parent reaps it, so
 * calling that a leak would fail clean runs on a scheduling window — but it is
 * still recorded, because a zombie that never gets reaped is a real leak.
 * @param {{ pid: number, procRoot?: string, kill?: (pid: number, signal: number) => void,
 *   observe?: typeof observeProcess, resource?: string }} spec
 */
export function judgeProcessRelease(spec) {
  const resource = spec.resource ?? `pid ${spec.pid}`;
  const send = spec.kill ?? ((pid, signal) => { process.kill(pid, signal); });
  /** @type {boolean|null} */
  let signalSaysAlive = null;
  /** @type {string} */
  let signalDetail = '';
  try {
    send(spec.pid, 0);
    signalSaysAlive = true;
    signalDetail = 'kill(pid, 0) succeeded';
  } catch (error) {
    const code = /** @type {{ code?: string }} */ (error)?.code ?? String(error);
    // ESRCH: no such process. EPERM: it exists and belongs to someone else —
    // which is a liveness answer too, and a loud one on a pid this run spawned.
    signalSaysAlive = code === 'EPERM' ? true : code === 'ESRCH' ? false : null;
    signalDetail = `kill(pid, 0) threw ${code}`;
  }
  const observation = (spec.observe ?? observeProcess)({ pid: spec.pid, procRoot: spec.procRoot });
  const zombie = observation.detail?.zombie === true;
  if (zombie && signalSaysAlive === true) {
    return {
      resource, claimedReleased: true, claimedBy: 'kill:signal-0', mechanism: observation.mechanism,
      verdict: CLEANUP_AGREEMENT.REAPING, ok: true, observation,
      reason: `${resource} is a zombie: the pid is still in the process table but it runs no code and holds no port, `
        + 'and it disappears when its parent reaps it',
    };
  }
  return {
    ...judgeCleanupRelease({
      resource, claimedReleased: signalSaysAlive === false, claimedBy: `kill:signal-0 (${signalDetail})`, observation,
    }),
    observation,
  };
}

/** @param {{ rows: ReadonlyArray<{ ok: boolean, verdict: string, resource: string, reason: string }> }} spec */
export function judgeCleanupAgreement(spec) {
  const rows = [...spec.rows];
  const bad = rows.filter((row) => row.ok !== true);
  return {
    ok: bad.length === 0,
    checked: rows.length,
    rows,
    disagreements: rows.filter((row) => row.verdict === CLEANUP_AGREEMENT.DISAGREEMENT),
    leaked: rows.filter((row) => row.verdict === CLEANUP_AGREEMENT.AGREED_LEAKED),
    inconclusive: rows.filter((row) => row.verdict === CLEANUP_AGREEMENT.INCONCLUSIVE),
    detail: bad.length === 0
      ? `${rows.length} owned resource(s): two independent readings agree each one is gone`
      : bad.map((row) => `${row.verdict} ${row.resource}: ${row.reason}`).join(' | '),
  };
}

// ─────────────────────────────── tree stability ──────────────────────────────

/**
 * Did the sources this evidence names change while the run was going?
 *
 * The evidence validator asks the same question afterwards, and answering it
 * DURING the run is the point: a certification that took ninety minutes on a
 * shared worktree can name the stage it was in when another lane regenerated a
 * file, instead of surfacing as an unexplained stale-hash rejection at the end.
 * @param {{ recorded: ReadonlyArray<{ path: string, sha256: string }>, projectRoot: string,
 *   stage?: string, hash?: (file: string) => string|null }} spec
 */
export function judgeTreeStability(spec) {
  const digest = spec.hash ?? ((file) => {
    try {
      return createHash('sha256').update(readFileSync(file)).digest('hex');
    } catch {
      return null;
    }
  });
  const moved = [];
  for (const entry of spec.recorded) {
    const now = digest(join(spec.projectRoot, entry.path));
    if (now !== entry.sha256) moved.push({ path: entry.path, recorded: entry.sha256, now });
  }
  return {
    stable: moved.length === 0,
    checked: spec.recorded.length,
    stage: spec.stage ?? null,
    moved,
    detail: moved.length === 0
      ? `${spec.recorded.length} recorded source file(s) are byte-identical to what this run started from`
      : `${moved.length} recorded source file(s) changed under this run${spec.stage === undefined ? '' : ` by ${spec.stage}`}: `
        + `${moved.map((entry) => `${entry.path}${entry.now === null ? ' (now unreadable)' : ''}`).slice(0, 5).join(', ')}`
        + ' — this run no longer certifies one tree',
  };
}
