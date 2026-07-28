// @ts-check
// tests/unit/task-51/soak-harness.mjs
// Task 51 — the 500-cycle cleanup soak.
//
// A soak is not "the load test, for longer". It answers one question: after N
// complete create/use/destroy cycles, is the process holding anything it did not
// hold after the first one? So the shape is deliberate:
//
//   * ONE long-lived child, many cycles. Restarting the process between cycles
//     would reset exactly the accumulation the soak exists to find.
//   * A CYCLE IS A FULL LIFECYCLE, not a repeated read. Each cycle creates
//     session-scoped state (enable a capability), uses it, then destroys it
//     (disable it) — because state that is never destroyed cannot be leaked by a
//     faulty teardown, and a soak over pure reads would be green against a broken
//     one. THE CYCLE MUST PROVE IT OPENED SOMETHING: the first version called
//     `enable_tool`, which manage_tools answers with UNKNOWN_ACTION, and checked
//     only for a timeout — so it cycled nothing 500 times and reported no
//     failures.
//   * COMPLETED CYCLES ARE COUNTED, NOT PLANNED ONES. Returning `spec.cycles`
//     reports the argument the caller passed in.
//   * THE BASELINE IS TAKEN AFTER WARM-UP, and again at a fixed midpoint. Two
//     samples separate "V8 grew its heap once" from "every cycle costs bytes":
//     a leak shows up as a rising second half, a one-off shows up as a flat one.
//
// The process-residue half is separate and blunt: spawn real children, close them,
// and confirm through /proc that each one is actually gone. `close()` returning is
// the claim; an absent /proc entry is the proof.

import { StdioDriver } from '../task-49/live-driver-stdio.mjs';
import { ResourceLedger } from '../task-49/live-resource-ledger.mjs';
import { observeProcess } from '../task-50/state-oracles.mjs';
import { loadEnv, processAlive, readRssBytes, sampleSettledRss } from './load-harness.mjs';
import { streamFor } from './fuzz-random.mjs';

/** Capabilities cycled on and off. Read-only, non-destructive and NOT protected —
 * `manage_tools` and `inspect` cannot be disabled, so cycling either would make
 * every teardown a no-op that still reported success. */
export const SOAK_TOOLS = Object.freeze(['manage_geometry', 'manage_pcg', 'manage_audio', 'manage_ai']);

/**
 * The gateway envelope inside a `tools/call` frame, or null when the frame does not
 * carry one.
 *
 * `wrapResponse('unreal', …)` puts the gateway envelope in `structuredContent`, so
 * that is the primary read; the JSON text block is a fallback for a client profile
 * that receives content only. Returning null when NEITHER is present is the point:
 * an uninterpretable answer must not be able to score as a successful step.
 * @param {any} frame @returns {Record<string, any>|null}
 */
export function readGatewayEnvelope(frame) {
  if (frame === null || typeof frame !== 'object') return null;
  if (frame.error !== undefined) return null;
  const result = frame.result;
  if (result === null || typeof result !== 'object') return null;
  if (result.structuredContent !== null && typeof result.structuredContent === 'object') {
    return result.structuredContent;
  }
  const text = Array.isArray(result.content)
    ? result.content.find(/** @param {any} entry */ (entry) => entry && entry.type === 'text' && typeof entry.text === 'string')
    : undefined;
  if (text === undefined) return null;
  try {
    const parsed = JSON.parse(text.text);
    return parsed !== null && typeof parsed === 'object' ? parsed : null;
  } catch {
    return null;
  }
}

/**
 * Did one `configure` step actually change the capability's state?
 *
 * `success: true` is required, and `isError` must be absent — `enable_tool`
 * (singular) hits the manage_tools default branch and answers
 * `success:false / UNKNOWN_ACTION`, which the previous version of this file could
 * not see because it only looked for a TIMEOUT. When the action's own result list
 * is present the tool must appear in it too, so a name that silently landed in
 * `notFound` cannot be read as a state change.
 * @param {any} frame @param {string} tool @param {'enabled'|'disabled'} listKey
 * @returns {{ changed: boolean, listConfirmed: boolean, detail: string }}
 */
export function configureChangedState(frame, tool, listKey) {
  if (frame === null) return { changed: false, listConfirmed: false, detail: 'timed out' };
  if (frame.result?.isError === true) return { changed: false, listConfirmed: false, detail: 'isError' };
  const envelope = readGatewayEnvelope(frame);
  if (envelope === null) return { changed: false, listConfirmed: false, detail: 'no readable gateway envelope' };
  if (envelope.success !== true) {
    const inner = envelope.result;
    const code = inner?.errorCode ?? envelope.errorCode ?? 'unspecified';
    return { changed: false, listConfirmed: false, detail: `refused (${String(code)})` };
  }
  const list = envelope.result?.[listKey];
  if (!Array.isArray(list)) {
    // success:true with no list to check. Honest but weaker, and recorded as such.
    return { changed: true, listConfirmed: false, detail: `success without a ${listKey} list` };
  }
  if (!list.includes(tool)) {
    return { changed: false, listConfirmed: false, detail: `${tool} absent from ${listKey}` };
  }
  return { changed: true, listConfirmed: true, detail: `${tool} in ${listKey}` };
}

/**
 * One cycle: OPEN session state, use it, CLOSE it.
 *
 * D3, in two halves, both of which the first version got wrong:
 *
 *   * A CYCLE COUNTS ONLY IF IT OPENED. The old cycle called `enable_tool` /
 *     `disable_tool` — actions manage_tools does not have (they are `enable_tools`
 *     / `disable_tools`, taking `tools: []` under `params`) — so every cycle in the
 *     recorded 500-cycle run answered UNKNOWN_ACTION twice and cycled nothing. The
 *     cycle still returned ok because only a timeout was checked. A "cleanup soak"
 *     that never creates state cannot observe a faulty teardown.
 *
 *   * CLOSE EXACTLY WHAT WAS OPENED. If the middle step fails after `enable`
 *     succeeded, the capability is STILL enabled; returning early there would leave
 *     it enabled for every remaining cycle, making the harness itself the leak it
 *     exists to detect. Conversely, when `enable` did not open anything, `disable`
 *     is NOT sent — issuing it would fabricate a teardown for state that never
 *     existed and paint an UNKNOWN_ACTION run green.
 * @param {StdioDriver|any} driver @param {string} tool @param {number} timeoutMs
 * @returns {Promise<{ ok: boolean, opened: boolean, used: boolean, closed: boolean,
 *   listConfirmed: boolean, leakedOpenState: boolean, stage: string, detail: string }>}
 */
export async function cleanupCycle(driver, tool, timeoutMs) {
  const enable = await driver.callTool(
    { operation: 'configure', action: 'enable_tools', params: { tools: [tool] } }, { timeoutMs });
  const opened = configureChangedState(enable.response, tool, 'enabled');
  if (!opened.changed) {
    return {
      ok: false, opened: false, used: false, closed: false, listConfirmed: false,
      leakedOpenState: false, stage: 'enable',
      detail: `enable ${tool}: ${opened.detail}`,
    };
  }

  const use = await driver.callTool({ operation: 'describe', tool }, { timeoutMs });
  const used = use.response !== null && use.response.result?.isError !== true;

  // Unconditional, because `opened.changed` is true: whatever happened above, this
  // capability is enabled and must be put back.
  const disable = await driver.callTool(
    { operation: 'configure', action: 'disable_tools', params: { tools: [tool] } }, { timeoutMs });
  const closed = configureChangedState(disable.response, tool, 'disabled');

  const detail = closed.changed && used
    ? `${tool} enabled, described and disabled`
    : `${tool}: ${used ? 'described' : 'describe failed'}; close ${closed.detail}`;
  return {
    ok: used && closed.changed,
    opened: true,
    used,
    closed: closed.changed,
    listConfirmed: opened.listConfirmed && closed.listConfirmed,
    // Opened and not closed is the one outcome that contaminates every later
    // cycle, so it is named rather than folded into a generic failure.
    leakedOpenState: !closed.changed,
    stage: closed.changed ? (used ? 'complete' : 'describe') : 'disable',
    detail,
  };
}

/**
 * Run the cleanup soak over one live child.
 * @param {{ cycles: number, seed: number|string, cwd?: string, env?: NodeJS.ProcessEnv,
 *   timeoutMs?: number, warmupCycles?: number, onProgress?: (done: number, total: number) => void,
 *   driverFactory?: (options: Record<string, unknown>) => any,
 *   rssSettle?: { windowMs?: number, samples?: number }, drainMs?: number }} spec
 */
export async function runSoak(spec) {
  const cwd = spec.cwd ?? process.cwd();
  const env = loadEnv(spec.env ?? process.env);
  const timeoutMs = spec.timeoutMs ?? 30_000;
  const ledger = new ResourceLedger();
  const makeDriver = spec.driverFactory ?? ((options) => new StdioDriver(options));
  const driver = makeDriver({ cwd, env, clientName: 'task51-soak' });
  const started = await driver.start({ timeoutMs: 60_000 });
  const pid = typeof started.pid === 'number' ? started.pid : 0;
  if (!started.ok || pid === 0) {
    // A session that never initialized still spawned a child, so it is closed —
    // but it opened nothing, so it cycles nothing and completes nothing.
    await driver.close();
    return {
      started: false, reason: started.reason,
      cyclesPlanned: spec.cycles, cyclesAttempted: 0, cyclesOpened: 0, cyclesCompleted: 0,
      openStateLeaks: 0, failures: [], samples: [], receipts: null,
    };
  }
  const spawnObservation = observeProcess({ pid });
  ledger.register('process', 'soak-session', { pid, role: 'stdio mcp server' },
    async () => { await driver.close(); },
    async () => {
      const released = !processAlive(pid);
      return { released, observed: released ? `/proc/${pid} is gone` : `/proc/${pid} still exists` };
    });

  const rng = streamFor(spec.seed, 'soak-cycles');
  /** @type {string[]} */
  const failures = [];
  /** @type {Array<{ cycle: number, rssBytes: number|null }>} */
  const samples = [];

  const warmup = spec.warmupCycles ?? 10;
  let warmupOpened = 0;
  /** @type {string[]} */
  const warmupDetails = [];
  for (let i = 0; i < warmup; i += 1) {
    const outcome = await cleanupCycle(driver, SOAK_TOOLS[i % SOAK_TOOLS.length], timeoutMs);
    if (outcome.opened) warmupOpened += 1;
    else if (warmupDetails.length < 4) warmupDetails.push(outcome.detail);
  }
  if (warmupOpened === 0) {
    // Nothing this soak does creates state, so running 500 more cycles would only
    // repeat the same non-measurement 500 times. Stop and say so: the completed
    // count stays at zero, which is what the gate reads.
    const receipts = await ledger.teardown();
    return {
      started: true, reason: started.reason, pid,
      cyclesPlanned: spec.cycles, cyclesAttempted: warmup, cyclesOpened: 0, cyclesCompleted: 0,
      openStateLeaks: 0,
      failures: warmupDetails.map((detail, index) => `warm-up ${index}: ${detail}`),
      samples: [],
      blocked: {
        code: 'CYCLE_NEVER_OPENED',
        detail: 'no warm-up cycle enabled a capability, so the soak would cycle no state at all',
        observed: warmupDetails,
      },
      malformedStdoutLines: driver.decoder.malformed,
      receipts, spawnObservation, teardownObservation: observeProcess({ pid }),
    };
  }
  // Settled trough, for the same reason the load harness uses one: a baseline taken
  // at the warm-up peak makes every later reading look like a saving.
  const baselineSettle = await sampleSettledRss(pid, spec.rssSettle ?? {});
  const baselineRss = baselineSettle.min;

  let attempted = 0;
  let opened = 0;
  let completed = 0;
  let openStateLeaks = 0;
  for (let cycle = 0; cycle < spec.cycles; cycle += 1) {
    attempted += 1;
    const outcome = await cleanupCycle(driver, rng.pick(SOAK_TOOLS), timeoutMs);
    if (outcome.opened) opened += 1;
    // A cycle is completed only when it opened, used and closed. Counting the loop
    // index instead — which is what `cycles: spec.cycles` did — reports the plan.
    if (outcome.ok) completed += 1;
    else failures.push(`cycle ${cycle}: ${outcome.detail}`);
    if (outcome.leakedOpenState) openStateLeaks += 1;
    if (cycle % 25 === 24) {
      samples.push({ cycle: cycle + 1, rssBytes: readRssBytes(pid) });
      spec.onProgress?.(completed, spec.cycles);
    }
  }

  await new Promise((settle) => { setTimeout(settle, spec.drainMs ?? 1500); });
  const finalSettle = await sampleSettledRss(pid, spec.rssSettle ?? {});
  const finalRss = finalSettle.min;
  const midpoint = samples[Math.floor(samples.length / 2)]?.rssBytes ?? null;
  const receipts = await ledger.teardown();

  return {
    started: true,
    reason: started.reason,
    pid,
    cyclesPlanned: spec.cycles,
    cyclesAttempted: attempted,
    cyclesOpened: opened,
    cyclesCompleted: completed,
    /** Cycles that enabled a capability and could not put it back. Every later
     * cycle ran against contaminated state, so this is reported separately rather
     * than being one more line in `failures`. */
    openStateLeaks,
    failures,
    samples,
    baselineRssBytes: baselineRss,
    baselineSettle,
    midpointRssBytes: midpoint,
    finalRssBytes: finalRss,
    finalSettle,
    retainedBytes: baselineRss === null || finalRss === null ? null : finalRss - baselineRss,
    /** Second-half growth. A one-off heap expansion is flat here; a per-cycle leak is not. */
    secondHalfGrowthBytes: midpoint === null || finalRss === null ? null : finalRss - midpoint,
    malformedStdoutLines: driver.decoder.malformed,
    receipts,
    spawnObservation,
    teardownObservation: observeProcess({ pid }),
  };
}

/**
 * The process-residue half: spawn and close real children, then prove each is gone
 * by a mechanism the driver does not own.
 * @param {{ rounds: number, cwd?: string, env?: NodeJS.ProcessEnv,
 *   driverFactory?: (options: Record<string, unknown>) => any }} spec
 */
export async function runProcessResidueSoak(spec) {
  const cwd = spec.cwd ?? process.cwd();
  const env = loadEnv(spec.env ?? process.env);
  const makeDriver = spec.driverFactory ?? ((options) => new StdioDriver(options));
  /** @type {string[]} */
  const residue = [];
  /** @type {number[]} */
  const pids = [];
  let opened = 0;
  let completed = 0;

  for (let round = 0; round < spec.rounds; round += 1) {
    const driver = makeDriver({ cwd, env, clientName: `task51-residue-${round}` });
    const started = await driver.start({ timeoutMs: 60_000 });
    const pid = typeof started.pid === 'number' ? started.pid : 0;
    if (pid === 0) {
      // No pid means the spawn itself failed. The driver object still holds a child
      // handle and its stdio listeners, so it is closed anyway: "close only what was
      // opened" cuts both ways, and skipping close() here (as the first version did)
      // leaked a handle per failed round.
      await driver.close();
      residue.push(`round ${round}: no pid was ever assigned`);
      continue;
    }
    pids.push(pid);
    // A round that spawned but never completed `initialize` proves nothing about
    // session teardown; it is still torn down, but it is not counted as a round.
    if (started.ok !== true) {
      await driver.close();
      residue.push(`round ${round}: session never initialized (${String(started.reason)})`);
      if (processAlive(pid)) residue.push(`round ${round}: /proc/${pid} still exists after close()`);
      continue;
    }
    opened += 1;
    await driver.callTool({ operation: 'search', query: 'residue' }, { timeoutMs: 30_000 });
    await driver.close();
    // The driver's own view, and then the independent one.
    const own = driver.verifyChildReleased();
    if (!own.released) residue.push(`round ${round}: ${own.observed}`);
    if (processAlive(pid)) residue.push(`round ${round}: /proc/${pid} still exists after close()`);
    else if (own.released) completed += 1;
  }

  // A final sweep: nothing this function started may survive it.
  const survivors = pids.filter((pid) => processAlive(pid));
  return { roundsPlanned: spec.rounds, roundsOpened: opened, roundsCompleted: completed, pids, residue, survivors };
}
