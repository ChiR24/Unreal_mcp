// @ts-check
// tests/unit/adversarial/load-harness.mjs
// Task 51 — the 32-session / 1,000-request load driver and the retained-RSS gate.
//
// WHAT THIS MEASURES, AND WHAT IT DELIBERATELY DOES NOT TOUCH.
//
// Each session is a REAL built `node dist/cli.js` child, spawned through Task 49's
// StdioDriver (which refuses a stale dist/ before spawning anything). No mock mode:
// MOCK_UNREAL_CONNECTION is explicitly deleted from the child environment, because a
// mocked run is not evidence and a leftover export in the operator's shell would
// silently turn this into one.
//
// The bridge is pointed at a port range this harness OWNS (default 8190/8191) rather
// than the 8090/8091 pair the editor lane owns. That is not tidiness: without it, a
// load run would attach 32 clients to whatever editor happened to be listening,
// which is someone else's resource and someone else's measurement. With it, the
// gateway's read operations (search / describe / configure) are served entirely
// in-process from the generated registry — genuinely live code, no editor needed —
// and `execute` correctly answers NOT_CONNECTED, which is itself an assertable
// fail-closed outcome rather than a hole in the run.
//
// A REQUEST COUNTS WHEN IT IS ANSWERED, NOT WHEN IT IS SENT. The plan loop cannot
// exit early, so a counter incremented next to `callTool` is always exactly
// `spec.requests` and any gate against it restates the plan. Attempted, answered
// and succeeded are therefore three separate numbers, and only the last one — the
// verdict each kind actually requires — is what a gate should read.
//
// RETAINED RSS IS A DELTA, NOT A LEVEL. A freshly started Node process is still
// lazily compiling and its RSS climbs for reasons that have nothing to do with the
// workload. So the harness warms up first, records a baseline per process, runs the
// measured traffic, drains, and reports post-drain MINUS baseline. Comparing a raw
// level against 32 MiB would fail on a bigger V8 heap and pass on a leak.

import { readFileSync } from 'node:fs';

import { StdioDriver } from '../live-drivers/live-driver-stdio.mjs';
import { ResourceLedger } from '../live-drivers/live-resource-ledger.mjs';
import { observeProcess } from '../evidence-oracles/state-oracles.mjs';
import { streamFor } from './fuzz-random.mjs';

/** Ports this harness owns. Never 8090/8091 (editor lane) and never 3000 (native MCP). */
export const OWNED_WS_PORTS = '8190,8191';

/** @param {number} pid @returns {number|null} resident set size in bytes, or null if the pid is gone */
export function readRssBytes(pid) {
  try {
    const status = readFileSync(`/proc/${pid}/status`, 'utf8');
    const match = /^VmRSS:\s+(\d+)\s+kB$/mu.exec(status);
    return match === null ? null : Number(match[1]) * 1024;
  } catch {
    return null;
  }
}

/**
 * The MINIMUM resident size observed across a short settle window.
 *
 * A single instantaneous sample is not a baseline. Taken right after warm-up it
 * lands on a transient V8 peak, and the "retained" delta against a later trough
 * then comes out NEGATIVE — which sails through a `<= 32 MiB` gate while proving
 * nothing at all. That is exactly what the first pilot run of this harness did, and
 * it is why the trough, not the instant, is the number both ends compare.
 * @param {number} pid @param {{ windowMs?: number, samples?: number }} [options]
 * @returns {Promise<{ min: number|null, max: number|null, last: number|null, samples: number }>}
 */
export async function sampleSettledRss(pid, options = {}) {
  const count = options.samples ?? 6;
  const windowMs = options.windowMs ?? 3000;
  /** @type {number[]} */
  const readings = [];
  for (let i = 0; i < count; i += 1) {
    const value = readRssBytes(pid);
    if (value !== null) readings.push(value);
    if (i + 1 < count) await new Promise((settle) => { setTimeout(settle, Math.round(windowMs / count)); });
  }
  if (readings.length === 0) return { min: null, max: null, last: null, samples: 0 };
  return {
    min: Math.min(...readings),
    max: Math.max(...readings),
    last: readings[readings.length - 1],
    samples: readings.length,
  };
}

/** @param {number} pid */
export function processAlive(pid) {
  try {
    readFileSync(`/proc/${pid}/stat`, 'utf8');
    return true;
  } catch {
    return false;
  }
}

/**
 * The child environment. Built by subtraction as well as addition: anything that
 * could make the run non-live or point it at a resource another lane owns is
 * removed rather than merely overridden.
 * @param {NodeJS.ProcessEnv} base @param {{ wsPorts?: string }} [options]
 */
export function loadEnv(base, options = {}) {
  const env = { ...base };
  delete env.MOCK_UNREAL_CONNECTION;
  delete env.MCP_METRICS_PORT;
  env.MCP_AUTOMATION_WS_PORTS = options.wsPorts ?? OWNED_WS_PORTS;
  env.MCP_AUTOMATION_HOST = '127.0.0.1';
  env.MCP_AUTOMATION_ALLOW_NON_LOOPBACK = 'false';
  env.MCP_LOG_LEVEL = 'error';
  return env;
}

/**
 * The mixed request plan. Deterministic from the seed, so two runs issue the same
 * 1,000 requests in the same order and their RSS numbers are comparable.
 * @param {number|string} seed @param {number} count
 * @returns {Array<{ kind: string, args: Record<string, unknown> }>}
 */
export function buildRequestPlan(seed, count) {
  const rng = streamFor(seed, 'load-plan');
  /** @type {readonly (readonly [number, () => { kind: string, args: Record<string, unknown> }])[]} */
  const table = [
    [5, () => ({ kind: 'search', args: { operation: 'search', query: rng.pick(['asset', 'actor', 'level', 'material', 'niagara', 'sequence']) } })],
    [4, () => ({ kind: 'describe-tool', args: { operation: 'describe', tool: rng.pick(['manage_asset', 'control_actor', 'manage_level', 'inspect']) } })],
    [3, () => ({ kind: 'describe-catalog', args: { operation: 'describe' } })],
    [2, () => ({ kind: 'configure', args: { operation: 'configure', action: 'list_tools' } })],
    // Execute with no bridge must fail CLOSED, not hang and not succeed.
    [3, () => ({ kind: 'execute-unconnected', args: { operation: 'execute', tool: 'inspect', action: 'inspect_object', params: { objectPath: '/Game/MCPTest/DoesNotExist' } } })],
    // A refusal path that must never reach a bridge at all.
    [2, () => ({ kind: 'execute-unknown', args: { operation: 'execute', tool: 'no_such_tool', action: 'nope', params: {} } })],
  ];
  return Array.from({ length: count }, () => rng.weighted(table)());
}

/**
 * The ONE verdict each request kind must produce to count as a completed request.
 *
 * D1: the first version of this harness counted a request the moment it was SENT.
 * The plan loop has no early exit, so that counter was always exactly
 * `spec.requests` and the `allRequestsIssued` gate restated the plan's own length
 * instead of measuring the run. A server that timed out on all 1,000 requests, or
 * answered every `execute` with an unexpected SUCCESS, scored an identical pass.
 *
 * A request is only SUCCEEDED when the verdict its kind demands actually arrived,
 * so a timeout, an ERROR on a read, or an UNEXPECTED on a refusal path each move
 * the succeeded count away from the planned count and the gate fails.
 */
export const EXPECTED_VERDICT = Object.freeze({
  'search': 'OK',
  'describe-tool': 'OK',
  'describe-catalog': 'OK',
  'configure': 'OK',
  'execute-unconnected': 'FAILED_CLOSED',
  'execute-unknown': 'REFUSED',
});

/** @param {string} kind @param {string} verdict */
export function isExpectedVerdict(kind, verdict) {
  const expected = /** @type {Record<string, string>} */ (EXPECTED_VERDICT)[kind];
  // An unknown kind has no declared expectation, so nothing about it can be
  // scored as success. Defaulting to "true" here would let a new request kind
  // enter the plan and be counted before anyone decided what it should return.
  return expected !== undefined && verdict === expected;
}

/** Classify one response without trusting it. @param {any} frame @param {string} kind */
export function classifyResponse(frame, kind) {
  if (frame === null) return 'TIMEOUT';
  const text = JSON.stringify(frame);
  if (kind === 'execute-unconnected') {
    return /NOT_CONNECTED|UE_NOT_CONNECTED|DISPATCH_ERROR/u.test(text) ? 'FAILED_CLOSED' : 'UNEXPECTED';
  }
  if (kind === 'execute-unknown') {
    return /UNKNOWN_CAPABILITY|CAPABILITY_UNAVAILABLE|VALIDATION_ERROR|not found|Unknown/iu.test(text) ? 'REFUSED' : 'UNEXPECTED';
  }
  if (frame.error !== undefined) return 'ERROR';
  return 'OK';
}

/**
 * Reduce per-session RSS readings to the three numbers the gates need.
 *
 * Exported as a seam so the VACUITY RULE can be driven offline against synthetic
 * sessions. A gate whose arithmetic is only ever exercised by a 15-minute 32-child
 * run is a gate nobody can check, which is how the first unfalsifiable version
 * survived review.
 * @param {Array<Record<string, unknown>>} sessions
 */
export function summariseRss(sessions) {
  /** @param {string} key */
  const numbers = (key) => sessions
    .map((session) => session[key])
    .filter((value) => typeof value === 'number');
  const retained = numbers('retainedBytes');
  const growth = numbers('secondHalfGrowthBytes');
  const peaks = numbers('peakOverBaselineBytes');
  return {
    retainedCount: retained.length,
    worstRetainedBytes: retained.length === 0 ? null : Math.max(...retained),
    growthCount: growth.length,
    worstGrowthBytes: growth.length === 0 ? null : Math.max(...growth),
    // If NO session ever rose above its own baseline, that baseline was a peak and
    // every retained delta is negative by construction. `<= limit` is then true for
    // any input, including a real leak smaller than the decay, so the comparison is
    // not a measurement and must not be scored as one.
    baselineWasAPeak: peaks.length > 0 && peaks.every((value) => value <= 0),
  };
}

/**
 * Run the load.
 *
 * Returns counts and per-session RSS deltas; it asserts nothing, so the caller can
 * attach the numbers to evidence and apply the gate in one place.
 * @param {{ sessions: number, requests: number, seed: number|string, cwd?: string,
 *   env?: NodeJS.ProcessEnv, warmupPerSession?: number, requestTimeoutMs?: number,
 *   wsPorts?: string, onProgress?: (done: number, total: number) => void,
 *   driverFactory?: (options: Record<string, unknown>) => any,
 *   rssSettle?: { windowMs?: number, samples?: number }, drainMs?: number }} spec
 */
export async function runLoad(spec) {
  const cwd = spec.cwd ?? process.cwd();
  const env = loadEnv(spec.env ?? process.env, { wsPorts: spec.wsPorts });
  const ledger = new ResourceLedger();
  // The real driver by default. The seam exists so the counting rules below can be
  // driven against a scripted server offline; a harness whose own arithmetic is only
  // ever exercised by the 32-child live run is a harness nobody can check.
  const makeDriver = spec.driverFactory ?? ((options) => new StdioDriver(options));
  /** @type {Array<{ index: number, driver: any, pid: number, spawnObservation?: unknown,
   *   baselineRss?: number|null, baselineSettle?: unknown }>} */
  const live = [];
  /** @type {string[]} */
  const startFailures = [];

  for (let index = 0; index < spec.sessions; index += 1) {
    const driver = makeDriver({ cwd, env, clientName: `task51-load-${index}` });
    const started = await driver.start({ timeoutMs: 60_000 });
    const pid = typeof started.pid === 'number' ? started.pid : 0;
    if (!started.ok || pid === 0) {
      startFailures.push(`session ${index}: ${started.reason}`);
      await driver.close();
      continue;
    }
    // The independent post-spawn reading, taken WHILE the process is alive. It has
    // to happen now: after teardown the pid is gone and its start ticks are
    // unrecoverable, so a later record could never be re-checked and the evidence
    // validator would (correctly) refuse it as a bare pid.
    const spawned = observeProcess({ pid });
    live.push({ index, driver, pid, spawnObservation: spawned });
    ledger.register('process', `load-session-${index}`, { pid, role: 'stdio mcp server' },
      async () => { await driver.close(); },
      async () => {
        const released = !processAlive(pid);
        return { released, observed: released ? `/proc/${pid} is gone` : `/proc/${pid} still exists` };
      });
  }

  if (live.length === 0) {
    const receipts = await ledger.teardown();
    return {
      started: 0,
      sessionsPlanned: spec.sessions,
      startFailures,
      outcomes: {},
      sessions: [],
      receipts,
      requestsPlanned: spec.requests,
      requestsAttempted: 0,
      requestsAnswered: 0,
      requestsSucceeded: 0,
      teardownObservations: [],
    };
  }

  // ── warm-up: pay the lazy-compilation cost BEFORE the baseline is taken ──────
  const warmup = spec.warmupPerSession ?? 12;
  for (const session of live) {
    for (let i = 0; i < warmup; i += 1) {
      await session.driver.callTool({ operation: 'search', query: 'warmup' }, { timeoutMs: spec.requestTimeoutMs ?? 30_000 });
    }
  }
  // Let the warm-up peak subside, then take the SETTLED TROUGH as the baseline.
  for (const session of live) {
    const settled = await sampleSettledRss(session.pid, spec.rssSettle ?? {});
    Object.assign(session, { baselineRss: settled.min, baselineSettle: settled });
  }

  // ── measured traffic, round-robin so every session carries a share ───────────
  const plan = buildRequestPlan(spec.seed, spec.requests);
  /** @type {Record<string, number>} */
  const outcomes = {};
  // Three counts, deliberately not one. ATTEMPTED is what this loop did; ANSWERED
  // is what came back at all; SUCCEEDED is what came back in the shape its kind
  // requires. Collapsing them into a single "issued" is what made the old gate
  // impossible to fail.
  let attempted = 0;
  let answered = 0;
  let succeeded = 0;
  // D4: at 32-session scale the post-warm-up baseline is a PEAK, not a floor. Thirty-two
  // children compete for pages while each carries only ~1/32 of the traffic, so across the
  // measured phase the OS reclaims far more than the workload adds and every retained delta
  // comes out negative — the same unfalsifiable shape the settled-trough fix was meant to
  // kill, merely relocated from one process to thirty-two. A negative delta cannot fail a
  // `<= 32 MiB` gate, so a real leak smaller than the decay would pass.
  //
  // The fix is the one the soak already uses: take a SECOND trough at steady state, once the
  // start-up transient has decayed, and measure growth from there. A leak is monotonic, so it
  // still shows up in the second half; the initial decay does not.
  const midpoint = Math.floor(plan.length / 2);
  for (const [position, request] of plan.entries()) {
    if (position === midpoint) {
      for (const held of live) {
        const mid = await sampleSettledRss(held.pid, spec.rssSettle ?? {});
        Object.assign(held, { midRss: mid.min, midSettle: mid });
      }
    }
    const session = live[position % live.length];
    attempted += 1;
    const response = await session.driver.callTool(request.args, { timeoutMs: spec.requestTimeoutMs ?? 30_000 });
    const verdict = classifyResponse(response.response, request.kind);
    const key = `${request.kind}:${verdict}`;
    outcomes[key] = (outcomes[key] ?? 0) + 1;
    if (response.response !== null) answered += 1;
    if (isExpectedVerdict(request.kind, verdict)) succeeded += 1;
    if (position % 100 === 99) spec.onProgress?.(succeeded, plan.length);
  }

  // ── drain, then measure the settled trough again, the SAME way ──────────────
  await new Promise((settle) => { setTimeout(settle, spec.drainMs ?? 1500); });
  /** @type {Array<Record<string, unknown>>} */
  const sessions = [];
  for (const session of live) {
    const held = /** @type {any} */ (session);
    const baseline = /** @type {number|null} */ (held.baselineRss);
    const after = await sampleSettledRss(session.pid, spec.rssSettle ?? {});
    sessions.push({
      index: session.index,
      pid: session.pid,
      spawnObservation: held.spawnObservation ?? null,
      baselineRssBytes: baseline,
      baselineSettle: held.baselineSettle ?? null,
      /** The steady-state trough, taken mid-plan. This is the end a leak is measured FROM. */
      midRssBytes: typeof held.midRss === 'number' ? held.midRss : null,
      midSettle: held.midSettle ?? null,
      finalRssBytes: after.min,
      finalSettle: after,
      /** Growth across the second half of the plan. Monotonic for a leak, ~flat otherwise. */
      secondHalfGrowthBytes: typeof held.midRss !== 'number' || after.min === null
        ? null
        : after.min - held.midRss,
      // Both ends are settled troughs, so this is a like-for-like comparison.
      retainedBytes: baseline === null || after.min === null ? null : after.min - baseline,
      /** Proof the workload actually moved memory; a run where it did not would make
       * the retained figure vacuous rather than good news. */
      peakOverBaselineBytes: baseline === null || after.max === null ? null : after.max - baseline,
      notifications: session.driver.notifications.length,
      malformedStdoutLines: session.driver.decoder.malformed,
    });
  }

  const receipts = await ledger.teardown();
  // Read every pid again through the same independent mechanism AFTER teardown.
  // The ledger receipt is the claim; this is the proof.
  const teardownObservations = live.map((session) => ({
    index: session.index,
    pid: session.pid,
    observation: observeProcess({ pid: session.pid }),
  }));
  return {
    started: live.length,
    sessionsPlanned: spec.sessions,
    startFailures,
    outcomes,
    sessions,
    receipts,
    requestsPlanned: plan.length,
    requestsAttempted: attempted,
    requestsAnswered: answered,
    requestsSucceeded: succeeded,
    teardownObservations,
  };
}
