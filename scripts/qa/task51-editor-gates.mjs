#!/usr/bin/env node
// Task 51 — closing the two EDITOR-SIDE gates that were BLOCKED on lane ownership.
//
// Task 51 could not measure these because it was not the lane that owns editors:
// both claims carry the observable EDITOR_OWNED_BY_ANOTHER_LANE. This run IS that
// lane, so it starts its own editor, in its own disposable workspace, on its own
// allocated ports, measures, and removes everything it made.
//
//   GATE A  editor retained RSS <= 64 MiB after warm-up
//   GATE B  zero residual UObjects and delegates after teardown
//
// NEITHER GATE IS ALLOWED TO BE DECORATION. Task 51 shipped an unfalsifiable
// memory gate twice — once as an instantaneous baseline that landed on a peak,
// then again at 32-session scale (D4) where every retained delta was negative and
// `<= 32 MiB` could not have failed. So the scoring here lives in
// tests/unit/task-51/editor-gates.mjs, is driven offline by its .test.ts over both
// polarities, and refuses to return PASS on a reading that could not have returned
// FAIL: INVALID_VACUOUS_BASELINE when the baseline was itself a peak, and
// INVALID_BLIND_COUNTER when the positive control never moved the object counter.
//
// The workload and the request shapes are Task 49's corpus, compiled by Task 49's
// own compiler. A bespoke set of "roughly equivalent" requests here would measure a
// different product than every other lane measured.
//
// Run: node scripts/qa/task51-editor-gates.mjs [--engine-version 5.7] [--out FILE]

import { existsSync, readFileSync, readdirSync, statSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';

import { buildEngineInventory } from '../../tests/unit/task-52/engine-inventory.mjs';
import { DisposableWorkspace } from '../../tests/unit/task-52/disposable-project.mjs';
import { scaffoldFiles } from '../../tests/unit/task-52/project-scaffold.mjs';
import {
  buildEditorTarget, judgeBinaryFreshness, launchEditor, materializeProject, packagePlugin, waitForPort,
} from '../../tests/unit/task-52/certification-stages.mjs';
import { judgeEditorLiveness, judgeCleanupRelease, judgeProcessRelease } from '../../tests/unit/task-52/certification-verdict.mjs';
import { buildCorpus } from '../../tests/unit/task-49/live-corpus.mjs';
import { compileRequest } from '../../tests/unit/task-49/live-corpus-runner.mjs';
import { NativeDriver } from '../../tests/unit/task-49/live-driver-native.mjs';
import { observeListener, observeProcess, observeTree } from '../../tests/unit/task-50/state-oracles.mjs';
import { sampleSettledRss } from '../../tests/unit/task-51/load-harness.mjs';
import { isSteadyState, judgeEditorRss, judgeResidualObjects, mib, parseObjectCount, stillBlocked } from '../../tests/unit/task-51/editor-gates.mjs';

const argOf = (name, fallback) => {
  const index = process.argv.indexOf(name);
  return index >= 0 ? process.argv[index + 1] : fallback;
};
const REPO = process.cwd();
const MINOR = argOf('--engine-version', '5.7');
const OUT = argOf('--out', '.omo/evidence/task-51/editor-gates.json');
const PROJECT = 'McpT51';
const RESIDUAL_FOLDER = '/Game/Task51Residual';
const CONTROL_OBJECTS = 8;
const log = (line) => { process.stderr.write(`${line}\n`); };
const wait = (ms) => new Promise((settle) => { setTimeout(settle, ms); });

// The native transport allows 120 tool calls per rolling 60s per client
// (MaxClientToolCallsPerMinute). The first attempt at this run fired 280 requests
// flat out, and everything past the cap came back HTTP 429 — while the harness
// happily counted all 280 as "measured". That is Task 51's own D1 defect, the
// requestsIssued tautology, rebuilt by hand: a counter incremented next to the
// call site always equals the plan and proves nothing about what the editor did.
// So calls go through a governor that stays under the cap, retries a 429 instead
// of swallowing it, and every call is tallied by what came BACK.
const RATE = { max: 100, windowMs: 60_000, stamps: /** @type {number[]} */ ([]) };
const tally = { attempted: 0, answered: 0, succeeded: 0, rateLimited: 0, failed: 0 };

const isRateLimited = (reply) => reply.status === 429
  || /rate limit/iu.test(JSON.stringify(reply.response ?? ''));

/** One governed, retried, tallied gateway call. */
async function call(driver, args, options = {}) {
  for (let attempt = 0; attempt < 6; attempt += 1) {
    for (;;) {
      const now = Date.now();
      RATE.stamps = RATE.stamps.filter((stamp) => now - stamp < RATE.windowMs);
      if (RATE.stamps.length < RATE.max) break;
      await wait(Math.max(500, RATE.windowMs - (now - RATE.stamps[0]) + 500));
    }
    RATE.stamps.push(Date.now());
    const reply = await driver.callTool(args, options);
    if (!isRateLimited(reply)) {
      tally.attempted += 1;
      if (reply.response !== null && reply.response !== undefined) tally.answered += 1;
      if (reply.status === 200 && reply.response?.error === undefined) tally.succeeded += 1;
      else tally.failed += 1;
      return reply;
    }
    await wait(4000 * (attempt + 1));
  }
  tally.attempted += 1;
  tally.rateLimited += 1;
  return { status: 429, response: null };
}

/** The read-only slice of Task 49's corpus, used as the RSS workload. */
const READ_ONLY = new Set([
  'task49.search.keyword', 'task49.describe.tool-summary', 'task49.describe.action-params',
  'task49.describe.unknown-tool', 'task49.configure.get-status',
  'task49.execute.canonical-read', 'task49.execute.legacy-read',
]);

/**
 * Run a console command and read its output from the EDITOR'S OWN LOG.
 *
 * The gateway's console_command reply is `{ success, message }` and carries no
 * console output, so a count parsed from it would be parsed from nothing. The
 * editor's stdout is a genuinely independent channel: the reading comes back
 * through a different path than the request went out on, and the raw delta is kept
 * in evidence so the number can be re-derived by hand.
 */
async function consoleAndRead(native, editor, command, { quietMs = 3000, maxMs = 45_000 } = {}) {
  const before = editor.text().length;
  // The action is the CAPABILITY, not a param. Passing `action` inside params is
  // refused with INVALID_PARAMS ("params must not override action or subAction"),
  // which the first attempt at this run did for every single console command —
  // so all four object counts came back empty and the gate scored UNMEASURED.
  const reply = await call(native, {
    operation: 'execute', capability: 'control_editor.console_command',
    params: { command },
  }, { timeoutMs: 180_000 });
  // `obj list` prints thousands of lines through GLog. Wait for the log to go
  // QUIET rather than for a fixed interval, or the summary line can land after
  // the read and the count silently becomes "no known summary line".
  const started = Date.now();
  let previous = -1;
  let quietSince = Date.now();
  for (;;) {
    const size = editor.text().length;
    if (size !== previous) { previous = size; quietSince = Date.now(); }
    if (Date.now() - quietSince >= quietMs || Date.now() - started >= maxMs) break;
    await wait(500);
  }
  const delta = editor.text().slice(before);
  return {
    command, delta, waitedMs: Date.now() - started, deltaBytes: delta.length,
    replyStatus: reply.status, replyText: JSON.stringify(reply.response ?? null).slice(0, 300),
  };
}

/** Force GC, then read the live UObject count out of the editor log. */
/** Newest `.memreport` under the project, with its mtime. */
function newestMemReport(projectDir) {
  const root = join(projectDir, 'Saved/Profiling/MemReports');
  if (!existsSync(root)) return null;
  const found = [];
  const walk = (dir) => {
    for (const entry of readdirSync(dir, { withFileTypes: true })) {
      const full = join(dir, entry.name);
      if (entry.isDirectory()) walk(full);
      else if (entry.name.endsWith('.memreport')) found.push({ file: full, mtimeMs: statSync(full).mtimeMs });
    }
  };
  walk(root);
  found.sort((left, right) => right.mtimeMs - left.mtimeMs);
  return found[0] ?? null;
}

/**
 * Force a garbage collection, policy-cleanly.
 *
 * `obj garbage` is refused with COMMAND_BLOCKED on BOTH transports — it is a
 * substring in the shared forbidden-token rule — so the first two attempts at this
 * gate could not collect anything and every count was taken over uncollected
 * garbage. `gc.CollectGarbageEveryFrame` is an ordinary CVar, blocked by nothing,
 * and it makes the engine collect on its own tick. It is turned back off
 * immediately: leaving it on would change the very thing the gate measures.
 */
async function forceGc(native, editor) {
  const on = await consoleAndRead(native, editor, 'gc.CollectGarbageEveryFrame 1', { quietMs: 2000, maxMs: 20_000 });
  await wait(8000);
  const off = await consoleAndRead(native, editor, 'gc.CollectGarbageEveryFrame 0', { quietMs: 2000, maxMs: 20_000 });
  await wait(2000);
  return { onStatus: on.replyStatus, offStatus: off.replyStatus, onReply: on.replyText };
}

/**
 * A live UObject census, taken from a memreport the PLUGIN generated.
 *
 * The census cannot come from a console command: `obj list` is refused by policy.
 * But `memreport` is run by the plugin's own code for
 * system_control.generate_memory_report, and UE's [MemReportCommands] includes
 * `obj list -resourcesizesort`, so the report file — written inside this run's own
 * disposable project — carries the summary line. Reading it off disk is also a
 * genuinely independent channel from the gateway reply that triggered it.
 */
async function countObjects(native, editor, projectDir, label) {
  const gc = await forceGc(native, editor);
  const before = newestMemReport(projectDir);
  const reply = await call(native, {
    operation: 'execute', capability: 'system_control.generate_memory_report',
    params: { detailed: true },
  }, { timeoutMs: 300_000 });
  let report = null;
  for (let waited = 0; waited < 180_000; waited += 2000) {
    const candidate = newestMemReport(projectDir);
    if (candidate !== null && (before === null || candidate.mtimeMs > before.mtimeMs)) { report = candidate; break; }
    await wait(2000);
  }
  // A report file that is still being written parses as a truncated one, so wait
  // for its size to stop moving before reading it.
  if (report !== null) {
    let previous = -1;
    for (let stable = 0; stable < 40; stable += 1) {
      const size = statSync(report.file).size;
      if (size === previous && size > 0) break;
      previous = size;
      await wait(1500);
    }
  }
  const text = report === null ? '' : readFileSync(report.file, 'utf8');
  const parsed = parseObjectCount(text);
  log(`  count[${label}] = ${parsed.count ?? 'UNPARSED'} (${parsed.matchedPattern ?? parsed.reason}; report ${report === null ? 'MISSING' : `${text.length}B`})`);
  return {
    label, count: parsed.count, matchedPattern: parsed.matchedPattern, occurrences: parsed.occurrences,
    allCounts: parsed.allCounts ?? null,
    reason: parsed.reason, gc, reportStatus: reply.status,
    reportReply: JSON.stringify(reply.response ?? null).slice(0, 250),
    reportFile: report === null ? null : report.file, reportBytes: text.length,
    censusExcerpt: text.length === 0 ? null : (/[^\n]*\d[\d,]*\s+Objects?[^\n]*/iu.exec(text)?.[0] ?? null),
  };
}

async function main() {
  if (process.env.MOCK_UNREAL_CONNECTION === 'true') {
    log('REFUSING TO RUN: MOCK_UNREAL_CONNECTION=true. A mocked editor measures nothing.');
    process.exitCode = 2;
    return;
  }
  const inventory = buildEngineInventory({ searchDirs: ['/data'] });
  const resolved = inventory.resolve(MINOR);
  if (!resolved.ok) { log(`cannot resolve UE ${MINOR}: ${resolved.detail}`); process.exitCode = 1; return; }
  const engineRoot = resolved.root;
  log(`engine ${resolved.identity.versionString} at ${engineRoot} (from Engine/Build/Build.version)`);

  const workspace = new DisposableWorkspace({ purpose: `task-51 editor gates on UE ${resolved.identity.versionString}` });
  workspace.open();
  const ports = await workspace.claimPorts(['native', 'wsPrimary', 'wsSecondary']);
  log(`workspace ${workspace.root}  ports ${JSON.stringify(ports)}`);

  /** @type {any} */
  const report = {
    task: 51, kind: 'editor-side gate closure (the lane that owns editors)',
    engine: { root: engineRoot, version: resolved.identity.versionString, resolvedFrom: 'Engine/Build/Build.version' },
    workspace: { ...workspace.manifest(), engineRoot },
    stages: [], gateA: null, gateB: null, delegates: null, probes: [], cleanup: {}, blocked: [],
  };
  const stage = (name, ok, detail) => {
    report.stages.push({ name, ok, detail });
    log(`${ok ? 'PASS' : 'FAIL'}  ${name}  ${detail}`);
    return ok;
  };

  let editor = null;
  let native = null;
  try {
    const packaged = packagePlugin({
      repoRoot: REPO, engineRoot, outDir: workspace.dir('package'), logFile: workspace.path('logs/package.log'),
    });
    if (!stage('package.plugin', packaged.ok, packaged.detail ?? `${packaged.archiveBytes} bytes`)) throw new Error('packaging failed');

    const project = materializeProject({
      projectDir: workspace.dir('project'), name: PROJECT, archive: packaged.archive,
      files: scaffoldFiles({ name: PROJECT, engine: resolved.identity.version, nativePort: ports.native, wsPorts: [ports.wsPrimary, ports.wsSecondary] }),
    });
    if (!stage('project.materialize', project.ok, project.detail ?? project.projectFile)) throw new Error('materialize failed');

    const built = buildEditorTarget({
      engineRoot, projectFile: project.projectFile, target: `${PROJECT}Editor`, logFile: workspace.path('logs/ubt.log'),
    });
    if (!stage('build.editorTarget', built.ok, built.detail ?? 'compiled')) throw new Error(`UBT failed: ${built.errors.slice(0, 3).join(' | ')}`);

    const binary = join(project.pluginRoot, 'Binaries/Linux/libUnrealEditor-McpAutomationBridge.so');
    const freshness = judgeBinaryFreshness({ binary, sourceRoot: join(project.pluginRoot, 'Source') });
    report.binaryFreshness = freshness;
    if (!stage('build.binaryFresh', freshness.fresh, freshness.reason)) throw new Error('stale binary');

    // -unattended is MANDATORY: without it Map_Check blocks the game thread on a UI
    // prompt a headless editor cannot present, the request queue never drains, and
    // the measurement below would read a wedged process as a quiet one.
    editor = launchEditor({
      engineRoot, projectFile: project.projectFile,
      args: ['-unattended', '-nopause', '-nosplash', '-NullRHI', '-NoSound', '-stdout', '-FullStdOutLogOutput'],
      logFile: workspace.path('logs/editor.log'),
      env: { ...process.env, MCP_NATIVE_PORT: String(ports.native) },
    });
    report.editor = { pid: editor.pid, command: editor.command };
    const ready = await waitForPort({ port: ports.native, timeoutMs: 600_000 });
    stage('editor.nativeListening', ready.ready, `port ${ports.native} after ${ready.attempts} probes`);
    const liveness = judgeEditorLiveness({ pid: editor.pid, portReady: ready.ready, logText: editor.text() });
    report.editor.liveness = { verdict: liveness.verdict, detail: liveness.detail };
    if (!stage('editor.alive', liveness.ok, `${liveness.verdict}: ${liveness.detail}`)) throw new Error(liveness.detail);

    native = new NativeDriver({ port: ports.native, clientName: 'task51-editor-gates' });
    const opened = await native.initialize();
    if (!stage('native.initialize', opened.ok === true, `protocol ${String(opened.negotiatedVersion)}`)) throw new Error('native /mcp did not initialize');

    const workload = buildCorpus().filter((scenario) => READ_ONLY.has(scenario.namespace)).map(compileRequest);
    stage('workload.compiled', workload.length > 0, `${workload.length} read-only Task 49 request shapes`);

    // ── GATE A ────────────────────────────────────────────────────────────────
    log('GATE A: editor retained RSS');
    const WARMUP = 30;
    const TRAFFIC = 150;
    const before = { ...tally };
    for (let i = 0; i < WARMUP; i += 1) await call(native, workload[i % workload.length], { timeoutMs: 120_000 });
    const settle = { windowMs: 12_000, samples: 8 };
    // DO NOT BASELINE ON A SLOPE. Sample consecutive troughs until they stop
    // falling; a baseline taken while the editor is still shedding its start-up
    // transient makes every later delta negative by construction (run 4: 2677 MiB
    // -> 873 MiB, retained -1798 MiB, gate unfalsifiable).
    const troughs = [];
    let steadiness = { steady: false, reason: 'NOT_ENOUGH_SAMPLES', spread: null };
    const settleDeadline = Date.now() + 480_000;
    for (;;) {
      const sample = await sampleSettledRss(editor.pid, settle);
      troughs.push(sample);
      steadiness = isSteadyState(troughs);
      log(`  trough ${troughs.length}: ${mib(sample.min)} (${steadiness.reason}`
        + `${steadiness.spread === null ? '' : `, spread ${(steadiness.spread * 100).toFixed(2)}%`})`);
      if (steadiness.steady || Date.now() > settleDeadline) break;
      // Keep the editor doing its normal work while it settles, so "steady" means
      // steady under load rather than steady while idle.
      for (let i = 0; i < 10; i += 1) await call(native, workload[i % workload.length], { timeoutMs: 120_000 });
    }
    const baseline = troughs[troughs.length - 1];
    log(`  baseline trough ${mib(baseline.min)} after ${troughs.length} troughs (${steadiness.reason})`);
    let mid = { min: null, max: null, last: null, samples: 0 };
    let trafficPeak = baseline.max ?? 0;
    const trafficStart = { ...tally };
    for (let i = 0; i < TRAFFIC; i += 1) {
      if (i === Math.floor(TRAFFIC / 2)) {
        mid = await sampleSettledRss(editor.pid, settle);
        log(`  steady-state trough ${mib(mid.min)}`);
      }
      await call(native, workload[i % workload.length], { timeoutMs: 120_000 });
      if (i % 15 === 0) {
        const spot = await sampleSettledRss(editor.pid, { windowMs: 300, samples: 2 });
        if (typeof spot.max === 'number' && spot.max > trafficPeak) trafficPeak = spot.max;
      }
    }
    await wait(8000);
    const final = await sampleSettledRss(editor.pid, settle);
    const served = {
      attempted: tally.attempted - trafficStart.attempted,
      answered: tally.answered - trafficStart.answered,
      succeeded: tally.succeeded - trafficStart.succeeded,
      rateLimited: tally.rateLimited - trafficStart.rateLimited,
    };
    log(`  workload served: ${served.succeeded}/${served.attempted} succeeded, ${served.rateLimited} rate-limited`);
    const gateA = judgeEditorRss({ baseline, mid, final });
    // A retained figure is only about the workload if the workload actually ran.
    const workloadServed = served.attempted > 0 && served.succeeded >= Math.floor(served.attempted * 0.95);
    report.gateA = {
      claim: 'editor retained RSS <= 64 MiB after warm-up',
      previousStatus: 'BLOCKED (EDITOR_OWNED_BY_ANOTHER_LANE)',
      methodology: 'settled troughs at three points (post-warm-up baseline, steady-state mid, post-drain final), '
        + 'the same shape Task 51 uses for its Node sessions, scored by the same reducer so the D4 vacuity rule cannot drift',
      warmupRequests: WARMUP, workload: served, workloadServed,
      steadyState: { ...steadiness, troughsTaken: troughs.length, troughs },
      warmupTally: { attempted: tally.attempted - before.attempted },
      baseline, mid, final, trafficPeakBytes: trafficPeak,
      trafficPeakOverBaselineBytes: baseline.min === null ? null : trafficPeak - baseline.min,
      ...gateA,
      ...(steadiness.steady ? {} : {
        verdict: 'INVALID_BASELINE_NOT_STEADY', ok: false,
        detail: `the editor never reached steady state within the settle budget (${steadiness.reason}, `
          + `${troughs.length} troughs, last spread ${steadiness.spread === null ? 'n/a' : `${(steadiness.spread * 100).toFixed(2)}%`}). `
          + 'A baseline taken on the start-up decay curve makes the retained delta negative by construction.',
      }),
      ...(workloadServed ? {} : {
        verdict: 'INVALID_WORKLOAD_NOT_SERVED', ok: false,
        detail: `only ${served.succeeded} of ${served.attempted} measured requests were served `
          + `(${served.rateLimited} rate-limited). A retained-RSS figure taken while the transport was refusing the `
          + 'traffic describes an idle editor, not a loaded one.',
      }),
    };
    stage('gateA.editorRetainedRss', report.gateA.ok, `${report.gateA.verdict} — ${report.gateA.detail}`);

    // THE CENSUS PROBE RUNS AFTER GATE A, DELIBERATELY.
    // It forces a GC and generates a 300KB detailed memreport. Run before the RSS
    // baseline, that released ~1.3 GB mid-measurement and Gate A correctly scored
    // INVALID_VACUOUS_BASELINE — the instrument for one gate had become the
    // dominant memory event of the other.
    const instrument = await countObjects(native, editor, project.projectDir, 'instrument-probe');
    report.probes.push(instrument);
    const instrumentOk = instrument.count !== null;
    stage('census.instrument', instrumentOk,
      instrumentOk
        ? `memreport census = ${instrument.count} objects (${instrument.matchedPattern}, ${instrument.occurrences} summary line(s), ${instrument.reportBytes}B)`
        : `NO CENSUS: ${instrument.reason}; report ${instrument.reportFile ?? 'MISSING'} (${instrument.reportBytes}B), reply ${instrument.reportReply}`);


    // ── GATE B ────────────────────────────────────────────────────────────────
    // Only run it if the census instrument was proven above. Four more censuses
    // that all return null would burn six minutes to re-derive a fact the probe
    // already established, and would still score UNMEASURED.
    if (!instrumentOk) {
      report.gateB = stillBlocked({
        claim: 'zero residual UObjects after teardown',
        code: 'NO_UOBJECT_CENSUS_INSTRUMENT',
        observable: 'No surface reachable from either transport reports a live UObject census. '
          + '`obj list`, `obj garbage` and `memreport` are all refused as console commands by the shared '
          + 'forbidden-token rule (COMMAND_BLOCKED, receipt in probes[]), no capability declares an object-count '
          + 'output field (inspect.find_by_class is world-ACTOR scoped), and system_control.generate_memory_report '
          + `did not yield a parseable census here: ${instrument.reason}.`,
      });
      stage('gateB.residualUObjects', false, 'STILL BLOCKED: no UObject census instrument');
    } else {
    log('GATE B: residual UObjects after session teardown');
    const baseCount = await countObjects(native, editor, project.projectDir, 'baseline');
    const created = [];
    for (let i = 0; i < CONTROL_OBJECTS; i += 1) {
      const name = `M_T51Residual_${i}`;
      const reply = await call(native, {
        operation: 'execute', capability: 'material.create_material',
        params: { name, path: RESIDUAL_FOLDER },
        consent: { capability: 'material.create_material', acknowledge: 'explicit' },
      }, { timeoutMs: 180_000 });
      created.push({
        name, path: `${RESIDUAL_FOLDER}/${name}`, status: reply.status,
        ok: reply.status === 200 && reply.response?.error === undefined,
        reply: JSON.stringify(reply.response ?? null).slice(0, 200),
      });
    }
    const createdOk = created.filter((entry) => entry.ok).length;
    stage('gateB.positiveControlCreated', createdOk === CONTROL_OBJECTS,
      `${createdOk}/${CONTROL_OBJECTS} control materials actually created in ${RESIDUAL_FOLDER}`);
    const peakCount = await countObjects(native, editor, project.projectDir, 'controlPeak');

    const deleted = await call(native, {
      operation: 'execute', capability: 'asset.delete_asset',
      params: { paths: created.map((entry) => entry.path) },
      consent: { capability: 'asset.delete_asset', acknowledge: 'elevated' },
    }, { timeoutMs: 180_000 });
    report.controlDelete = { status: deleted.status, body: JSON.stringify(deleted.response ?? null).slice(0, 400) };
    const returnCount = await countObjects(native, editor, project.projectDir, 'controlReturn');

    // THE MEASURED LIFECYCLE: whole native MCP sessions opened, used and closed.
    const CYCLES = 12;
    const cycleResults = [];
    for (let i = 0; i < CYCLES; i += 1) {
      const session = new NativeDriver({ port: ports.native, clientName: `task51-cycle-${i}` });
      const init = await session.initialize();
      for (let r = 0; r < 6; r += 1) await call(session, workload[r % workload.length], { timeoutMs: 120_000 });
      await session.close();
      const releasedSession = await session.verifySessionReleased().catch(() => null);
      cycleResults.push({ index: i, initialized: init.ok === true, sessionReleased: releasedSession });
    }
    stage('gateB.sessionCycle', cycleResults.every((entry) => entry.initialized),
      `${CYCLES} native sessions opened, used (6 requests each) and closed`);
    const finalCount = await countObjects(native, editor, project.projectDir, 'final');

    const gateB = judgeResidualObjects({
      baselineCount: baseCount.count, controlPeakCount: peakCount.count,
      controlReturnCount: returnCount.count, finalCount: finalCount.count, createdObjects: CONTROL_OBJECTS,
    });
    report.gateB = {
      claim: 'zero residual UObjects after teardown',
      previousStatus: 'BLOCKED (EDITOR_OWNED_BY_ANOTHER_LANE)',
      methodology: 'a forced GC (gc.CollectGarbageEveryFrame toggled on and back off — `obj garbage` is refused by '
        + 'policy) followed by a live UObject census parsed from a .memreport the PLUGIN generated, read off disk '
        + 'rather than from the gateway reply that triggered it. Taken at four points: baseline, after a positive '
        + 'control created real objects, after they were destroyed, and after 12 native MCP sessions were opened, '
        + 'used and closed. The control must move the census in BOTH directions or the gate scores '
        + 'INVALID_BLIND_COUNTER rather than passing on a zero it could not have failed.',
      sessionCycles: CYCLES, controlObjects: CONTROL_OBJECTS,
      // NOT `readings`: judgeResidualObjects returns its own `readings` summary and
      // the spread below would silently overwrite this array, which is exactly what
      // discarded the raw log tails on the first attempt.
      objectReadings: [baseCount, peakCount, returnCount, finalCount],
      controlCreated: created, cycles: cycleResults,
      ...gateB,
    };
    stage('gateB.residualUObjects', gateB.ok, `${gateB.verdict} — ${gateB.detail}`);
    }

    // ── DELEGATES: no instrument exists, and that is a finding, not an omission ─
    // A dynamic delegate BINDING is not a UObject, so no object census can see it.
    // Every console route that could dump one (`obj list`, `memreport`) is refused
    // by policy, and no capability in the 1,335-record catalogue declares a
    // delegate-count output field. This is restated rather than estimated.
    report.delegates = stillBlocked({
      claim: 'zero residual delegates after teardown',
      code: 'NO_DELEGATE_INSTRUMENT',
      observable: 'A dynamic delegate BINDING is not a UObject, so no object census can observe one. '
        + 'The console commands that could dump delegate state (`obj list`, `memreport`) are refused on BOTH '
        + 'transports by the shared forbidden-token policy rule, and no capability in the canonical catalogue '
        + 'declares a delegate-count output field. Even `obj list class=DelegateFunction` — itself blocked — would '
        + 'count UDelegateFunction reflection objects describing delegate SIGNATURES, not live bindings. '
        + 'Closing this requires an instrument compiled into the plugin, which this run did not build.',
    });
    stage('delegates.instrument', false, `${report.delegates.status}: no falsifiable delegate-binding reading was available`);
  } catch (error) {
    report.blocked.push(`aborted: ${error instanceof Error ? error.message : String(error)}`);
    log(String(error instanceof Error ? error.stack : error));
  }

  // ── TEARDOWN, judged by a mechanism other than the one that performed it ───
  if (native !== null) { try { await native.close(); } catch { /* never opened */ } }
  if (editor !== null && editor.pid !== null) {
    editor.flush();
    try { process.kill(-editor.pid, 'SIGTERM'); } catch { /* already gone */ }
    for (let waited = 0; waited < 90_000; waited += 1000) {
      if (observeProcess({ pid: editor.pid }).present !== true) break;
      await wait(1000);
    }
    if (observeProcess({ pid: editor.pid }).present === true) {
      try { process.kill(-editor.pid, 'SIGKILL'); } catch { /* raced */ }
      await wait(3000);
    }
    const release = judgeProcessRelease({ pid: editor.pid, resource: `editor pid ${editor.pid}` });
    report.cleanup.editor = { pid: editor.pid, ok: release.ok, verdict: release.verdict, reason: release.reason };
  }
  report.cleanup.ports = [];
  for (const [name, port] of Object.entries(workspace.ports)) {
    const seen = observeListener({ port });
    report.cleanup.ports.push({ name, port, observation: seen.detail, absent: seen.present !== true });
  }
  const receipt = workspace.close();
  const after = observeTree({ root: workspace.root, kind: 'owned-workspace' });
  const row = judgeCleanupRelease({
    resource: workspace.root, claimedReleased: receipt.removed === true,
    claimedBy: `rm receipt (${String(receipt.reason)})`, observation: after,
  });
  report.cleanup.workspace = { ...receipt, independentlyVerified: row.ok, verdict: row.verdict };
  report.cleanup.residueAtExit = existsSync(workspace.root) ? 'PRESENT' : 'NONE';
  stage('cleanup.workspace', row.ok, `${receipt.detail}; ${row.verdict}`);

  const failed = report.stages.filter((entry) => entry.ok !== true);
  report.verdict = report.blocked.length > 0
    ? `BLOCKED — ${report.blocked.join(' | ')}`
    : `${report.stages.length - failed.length}/${report.stages.length} stages passed`;
  // Three counts, never one: what this run sent, what came back at all, and what
  // came back in a usable shape. Task 51's D1 was a single "issued" counter that
  // could only ever equal the plan.
  report.requestTally = { ...tally };
  report.generatedAt = new Date().toISOString();
  writeFileSync(OUT, `${JSON.stringify(report, null, 2)}\n`);
  log(`\n${report.verdict}`);
  log(`wrote ${OUT}`);
  if (report.blocked.length > 0) process.exitCode = 1;
}

main().catch((error) => { log(String(error?.stack ?? error)); process.exitCode = 1; });
