#!/usr/bin/env node
// Task 50 — the LIVE oracle + evidence probe.
//
// Task 49 built the corpus and the drivers. This adds the missing half: proof
// that does not come from the thing being proven. Every verdict below is decided
// by bytes on disk, entries in /proc, or a socket this script opened itself —
// never by the response that claimed the work was done.
//
// WHY THAT MATTERS, concretely. Task 49's first live run reported
// `cleanupClean: true` while two materials sat in the project's Content tree.
// Every delete had answered INVALID_ARGUMENT and the harness believed them.
// A `find` caught it. This script IS that find, made systematic.
//
// TRANSPORT CHOICE, and the reason for it. Mutations and the destructive cleanup
// run on NATIVE /mcp. Task 49's finding F2 established that `asset.delete_asset`
// is unreachable over stdio — the TypeScript gateway validates the consent grant
// against the alias record while the plugin normalises to canonical `asset.delete`,
// so no client-supplied grant satisfies both layers. A stdio-driven destructive
// fixture would therefore fail to clean up and leave residue in somebody's project.
// stdio is still exercised, as a READ-ONLY CROSS-TRANSPORT corroborator, which is
// the role it can play honestly.
//
// PRECONDITIONS (all, or the run is BLOCKED rather than scored):
//   - a live editor launched with -unattended. Without it Map_Check blocks the
//     game thread on a prompt headless mode cannot present, the queue never
//     drains, and the suite truncates silently while reading as a pass.
//   - native /mcp on 127.0.0.1:3000 and the WS bridge on 127.0.0.1:8090,8091
//   - MCP_QA_TOKEN exported when the plugin requires a capability token
//   - a FRESH dist/ — the stdio driver refuses a stale build rather than rebuild
//
// MOCK MODE IS NOT LIVE EVIDENCE. MOCK_UNREAL_CONNECTION is refused outright.
//
// Run: node scripts/qa/task50-oracle-evidence.mjs --out <file> [--project <dir>]

import { execFileSync } from 'node:child_process';
import { statSync } from 'node:fs';

import { NativeDriver } from '../../tests/unit/task-49/live-driver-native.mjs';
import { StdioDriver } from '../../tests/unit/task-49/live-driver-stdio.mjs';
import { legacyArgs, receiptOf } from '../../tests/unit/task-49/live-corpus-runner.mjs';
import { ResourceLedger, readCapabilityToken } from '../../tests/unit/task-49/live-resource-ledger.mjs';
import { loadRecords } from '../../tests/unit/task-46/matrix-dimensions.mjs';
import { checkDistFreshness } from '../../tests/unit/task-46/dist-freshness.mjs';

import {
  crossTransportObservation,
  observeAssetPackage,
  observeHttpSession,
  observeListener,
  observeProcess,
  observeTree,
  walkFiles,
} from '../../tests/unit/task-50/state-oracles.mjs';
import { VERDICTS, forgedSuccessClaim, judgeClaim, judgeCleanup } from '../../tests/unit/task-50/oracle-judgement.mjs';
import { FixtureNamespace, findInterruptedRuns, reclaimInterruptedRun, residualContent } from '../../tests/unit/task-50/fixture-namespace.mjs';
import { EvidenceAggregator, identifyEngine, recordCommand } from '../../tests/unit/task-50/evidence-aggregator.mjs';
import { describeRejections, validateEvidence } from '../../tests/unit/task-50/evidence-validator.mjs';

const argOf = (name, fallback) => {
  const index = process.argv.indexOf(name);
  return index >= 0 ? process.argv[index + 1] : fallback;
};

const OUT = argOf('--out', '.omo/evidence/task-50/live-oracle-run.json');
const PROJECT_ROOT = argOf('--project', '/data/Game/MCPtest');
const ENGINE_ROOT = argOf('--engine', '/data/UnrealEngine');
const SETTLE_MS = Number(argOf('--settle', '2500'));

const log = (line) => { process.stderr.write(`${line}\n`); };
const wait = (ms) => new Promise((settle) => { setTimeout(settle, ms); });

/** Find the running editor without assuming anything about who started it. */
function findEditorPid() {
  try {
    const out = execFileSync('pgrep', ['-f', 'UnrealEditor.*MCPtest'], { encoding: 'utf8' });
    const pid = Number(String(out).trim().split('\n')[0]);
    return Number.isFinite(pid) ? pid : null;
  } catch {
    return null;
  }
}

/** One gateway call, recorded verbatim into the transcript. */
async function call(driver, aggregator, args, timeoutMs = 60_000) {
  const started = Date.now();
  const observed = await driver.callTool(args, { timeoutMs });
  const receipt = receiptOf(observed.response);
  const transcriptRef = aggregator.addTranscript({
    transport: driver.kind, request: args, response: receipt ?? observed.response, ms: Date.now() - started,
  });
  const failed = observed.response?.error !== undefined
    || observed.response?.result?.isError === true
    || receipt?.success === false
    || typeof receipt?.errorCode === 'string';
  return { receipt, transcriptRef, outcome: failed ? 'error' : 'success', raw: observed };
}

/**
 * Poll an out-of-band observation until it reads the wanted polarity, or the
 * window closes. The asset registry writes the package asynchronously, so a
 * single immediate read would report a real creation absent.
 */
async function settleObservation(read, wanted, attempts = 6, intervalMs = SETTLE_MS) {
  let last = read();
  for (let attempt = 1; attempt < attempts && last.present !== wanted; attempt += 1) {
    await wait(intervalMs);
    last = read();
  }
  return last;
}

async function main() {
  if (process.env.MOCK_UNREAL_CONNECTION === 'true') {
    log('REFUSING TO RUN: MOCK_UNREAL_CONNECTION=true.\n  A mocked run is not live evidence and must never be recorded as one.');
    process.exitCode = 2;
    return;
  }

  const credentials = readCapabilityToken();
  const records = loadRecords();
  const recordOf = (id) => records.find((entry) => entry.id === id || entry.canonicalId === id);

  const aggregator = new EvidenceAggregator({
    task: 50,
    title: 'Build independent Unreal state oracles, isolated fixtures and evidence validation',
    plan: '.omo/plans/pure-unreal-mcp-implementation.md',
  });
  aggregator.addNote(`capability token source: ${credentials.source}`);

  const namespace = new FixtureNamespace({ projectRoot: PROJECT_ROOT });
  const ledger = new ResourceLedger();
  /** @type {Array<Record<string, unknown>>} */
  const results = [];
  /** @type {string[]} */
  const blocked = [];

  // ── interrupted-run recovery, BEFORE anything else claims a namespace ───────
  const interrupted = findInterruptedRuns({});
  const reclaimed = interrupted.reclaimable.map((manifest) => reclaimInterruptedRun(manifest));
  aggregator.addNote(`interruption scan: ${interrupted.scanned} manifest(s), ${interrupted.reclaimable.length} reclaimable, `
    + `${interrupted.active.length} left alone as another run's, ${interrupted.unreadable.length} unrecognised and untouched`);

  const manifest = namespace.open();
  log(`run ${namespace.runId}\n  content ${namespace.gameRoot}\n  temp    ${namespace.tempRoot}`);

  // ── what actually ran: tree, artifacts, engine, processes ──────────────────
  aggregator.recordTree([
    'tests/unit/task-50/state-oracles.mjs',
    'tests/unit/task-50/oracle-judgement.mjs',
    'tests/unit/task-50/fixture-namespace.mjs',
    'tests/unit/task-50/evidence-validator.mjs',
    'tests/unit/task-50/evidence-aggregator.mjs',
    'scripts/qa/task50-oracle-evidence.mjs',
  ]);
  const freshness = checkDistFreshness(process.cwd());
  aggregator.recordArtifact({
    path: 'dist/cli.js',
    inputsNewest: freshness.newestInput,
    inputsNewestAtMs: freshness.newestInputMtimeMs,
  });
  // The PLUGIN binary the editor actually loaded.
  //
  // Its real inputs are the SYNCED source tree inside the project, not the repo
  // working tree — those are two different facts and collapsing them would make
  // STALE_PACKAGE mean two things. STALE_PACKAGE is reserved for "the binary is
  // older than the sources it was compiled from", which is a broken build. "The
  // repo advanced after the sync" is recorded separately, because it bounds what
  // the live observations are evidence OF. Task 49 had to put exactly this in its
  // notProven; here it is measured instead of remembered.
  const newestIn = (root) => walkFiles(root)
    .map((file) => ({ file, mtimeMs: statSync(file).mtimeMs }))
    .sort((a, b) => b.mtimeMs - a.mtimeMs)[0] ?? null;
  const syncedSource = newestIn(`${PROJECT_ROOT}/Plugins/McpAutomationBridge/Source`);
  const repoSource = newestIn(`${process.cwd()}/plugins/McpAutomationBridge/Source`);
  aggregator.recordArtifact({
    path: `${PROJECT_ROOT}/Plugins/McpAutomationBridge/Binaries/Linux/libUnrealEditor-McpAutomationBridge.so`,
    inputsNewest: syncedSource?.file ?? null,
    inputsNewestAtMs: syncedSource?.mtimeMs ?? null,
  });
  const syncBehindMs = (repoSource?.mtimeMs ?? 0) - (syncedSource?.mtimeMs ?? 0);
  aggregator.document.environment.pluginSyncDivergence = {
    syncedNewest: syncedSource?.file ?? null,
    syncedNewestAt: syncedSource === null ? null : new Date(syncedSource.mtimeMs).toISOString(),
    repoNewest: repoSource?.file.replace(`${process.cwd()}/`, '') ?? null,
    repoNewestAt: repoSource === null ? null : new Date(repoSource.mtimeMs).toISOString(),
    repoAheadBySeconds: Math.round(syncBehindMs / 1000),
  };
  if (syncBehindMs > 0) {
    aggregator.addNotProven('PLUGIN behavior of the CURRENT repo working tree. The editor loaded a binary compiled from the '
      + `project's SYNCED plugin source, which is ${Math.round(syncBehindMs / 60000)} minute(s) behind the repo tree because another `
      + 'lane owns the plugin build. Every plugin-side observation below is of the synced build, not of the working tree.');
  }
  aggregator.document.engine = identifyEngine({ engineRoot: ENGINE_ROOT, projectPath: `${PROJECT_ROOT}/MCPtest.uproject` });

  const editorPid = findEditorPid();
  if (editorPid === null) blocked.push('no UnrealEditor process for MCPtest was found; a run with the editor down is BLOCKED, never a pass');
  else aggregator.recordProcess({ pid: editorPid, role: 'unreal-editor (NOT owned by this run; observed only)' });

  try {
    // ── native transport ─────────────────────────────────────────────────────
    const native = new NativeDriver();
    native.kind = 'native';
    const opened = await native.initialize();
    aggregator.recordClient({
      id: 'native', transport: 'http-sse', host: `${native.host}:${native.port}`,
      ready: opened.ok, negotiatedVersion: opened.negotiatedVersion,
      tokenPresented: opened.tokenPresented, sessionId: opened.ok ? String(opened.sessionId) : null,
    });
    if (!opened.ok) {
      blocked.push(`native /mcp did not initialize (status ${opened.status})`);
      throw new Error('native transport unavailable; nothing below can be observed');
    }
    ledger.register('session', String(opened.sessionId), { transport: 'native' },
      () => native.close(),
      async () => {
        const seen = await observeHttpSession({
          host: native.host, port: native.port, sessionId: String(opened.sessionId),
          token: native.token, protocolVersion: native.negotiatedVersion ?? undefined,
        });
        return { released: seen.present === false, observed: `independent POST on that session id: ${JSON.stringify(seen.detail)}` };
      });

    // ── stdio transport, read-only corroborator ──────────────────────────────
    const stdio = new StdioDriver();
    stdio.kind = 'stdio';
    const started = await stdio.start();
    aggregator.recordClient({
      id: 'stdio', transport: 'jsonrpc-stdio', ready: started.ok, pid: started.pid,
      buildUnderTest: stdio.buildUnderTest, negotiatedVersion: started.negotiatedVersion,
      tokenPresented: started.tokenPresented, role: 'read-only cross-transport corroborator',
    });
    let stdioReady = false;
    if (started.ok) {
      aggregator.recordProcess({ pid: started.pid, role: 'node dist/cli.js (spawned and owned by this run)' });
      ledger.register('process', `node-dist-cli-${started.pid}`, { pid: started.pid },
        () => stdio.close(),
        async () => {
          const seen = observeProcess({ pid: started.pid });
          return { released: seen.present === false, observed: `/proc/${started.pid}: ${seen.present === false ? 'gone' : JSON.stringify(seen.detail)}` };
        });
      const bridge = await stdio.waitForBridge();
      stdioReady = bridge.ready;
      if (!bridge.ready) blocked.push('the WebSocket bridge never connected; cross-transport corroboration is unavailable');
    } else {
      blocked.push(`stdio server did not start (${started.reason})`);
    }

    // ── LISTENER control: present now, and one that is definitely absent ──────
    for (const port of [3000, 8090, 8091]) {
      aggregator.addObservation(observeListener({ port }), { phase: 'control' });
    }
    aggregator.addObservation(observeListener({ port: 65_531 }), { phase: 'control' });

    // ═══════════════════ SCENARIO 1 — a real, owned mutation ══════════════════
    const assetName = 'M_Task50Owned';
    const objectPath = `${namespace.gameRoot}/${assetName}`;
    namespace.declare('content', objectPath);

    const readAsset = () => observeAssetPackage({ projectRoot: PROJECT_ROOT, objectPath, expectName: assetName });
    const preState = readAsset();
    const preRef = aggregator.addObservation(preState, { phase: 'pre' });

    if (preState.present === true) {
      results.push({ scenario: 'owned-mutation', status: 'BLOCKED', detail: `${objectPath} already existed; a "present" reading could not be attributed to this run` });
    } else {
      const created = await call(native, aggregator, {
        operation: 'execute', capability: 'material.create_material',
        params: { name: assetName, path: namespace.gameRoot },
        consent: { capability: 'material.create_material', acknowledge: 'explicit' },
      });
      const postState = await settleObservation(readAsset, true);
      const postRef = aggregator.addObservation(postState, { phase: 'post' });

      /** Corroboration from the OTHER transport — never the sole proof. */
      let crossRef = null;
      if (stdioReady) {
        const listed = await call(stdio, aggregator, legacyArgs(recordOf('asset.list'), { path: namespace.gameRoot }, null));
        const assets = listed.receipt?.data?.assets ?? listed.receipt?.assets ?? null;
        crossRef = aggregator.addObservation(crossTransportObservation({
          target: objectPath, transport: 'stdio',
          present: Array.isArray(assets) ? assets.some((entry) => JSON.stringify(entry).includes(assetName)) : null,
          detail: { listedCount: Array.isArray(assets) ? assets.length : null },
        }), { phase: 'post' });
      }

      const verdict = judgeClaim({
        claim: { outcome: created.outcome, effect: 'created', target: objectPath },
        before: preState, after: postState,
        corroboration: crossRef === null ? [] : [aggregator.document.observations.find((entry) => entry.id === crossRef)],
      });
      results.push({ scenario: 'owned-mutation', status: verdict.pass ? 'PASS' : 'FAIL', verdict: verdict.verdict, reason: verdict.reason, transcriptRef: created.transcriptRef });
      aggregator.document.claims.push({
        id: 'claim-owned-mutation', target: objectPath, effect: 'created', outcome: created.outcome,
        verdict: verdict.verdict, pass: verdict.pass, reason: verdict.reason,
        oracleRefs: crossRef === null ? [preRef, postRef] : [preRef, postRef, crossRef],
        cleanupRef: 'cleanup-owned-namespace', transcriptRef: created.transcriptRef,
      });
      log(`${verdict.pass ? 'PASS' : 'FAIL'}  owned-mutation  ${verdict.verdict}`);
    }

    // ═══════════ SCENARIO 2 — the lie detector, against the LIVE tree ═════════
    // A response that CLAIMS it created something, handed to the same oracle that
    // just proved a real creation. If this does not fail, nothing above is worth
    // anything.
    const phantom = `${namespace.gameRoot}/M_Task50NeverCreated`;
    const phantomBefore = observeAssetPackage({ projectRoot: PROJECT_ROOT, objectPath: phantom });
    const phantomAfter = observeAssetPackage({ projectRoot: PROJECT_ROOT, objectPath: phantom });
    // NEGATIVE CONTROL for the cross-transport mechanism. Without a reading where
    // stdio reports ABSENT, its one PRESENT reading above proves only that it
    // answers — an oracle stuck on "yes" would look identical.
    if (stdioReady) {
      const emptyList = await call(stdio, aggregator, legacyArgs(recordOf('asset.list'), { path: namespace.gameRoot }, null));
      const listed = emptyList.receipt?.data?.assets ?? emptyList.receipt?.assets ?? null;
      aggregator.addObservation(crossTransportObservation({
        target: phantom, transport: 'stdio',
        present: Array.isArray(listed) ? listed.some((entry) => JSON.stringify(entry).includes('M_Task50NeverCreated')) : null,
        detail: { listedCount: Array.isArray(listed) ? listed.length : null, role: 'negative control' },
      }), { phase: 'control' });
    }
    const phantomBeforeRef = aggregator.addObservation(phantomBefore, { phase: 'pre' });
    const phantomAfterRef = aggregator.addObservation(phantomAfter, { phase: 'post' });
    const forged = judgeClaim({
      claim: forgedSuccessClaim({ target: phantom }),
      before: phantomBefore, after: phantomAfter,
    });
    const forgedCaught = forged.verdict === VERDICTS.FORGED_SUCCESS;
    results.push({
      scenario: 'forged-success-detector', status: forgedCaught ? 'PASS' : 'FAIL',
      verdict: forged.verdict,
      reason: forgedCaught
        ? 'a response claiming success for an object that was never created was REFUSED by the out-of-band oracle'
        : `the oracle did NOT catch a forged success (returned ${forged.verdict}); every other verdict in this file is unearned`,
    });
    aggregator.document.claims.push({
      id: 'claim-forged-success', target: phantom, effect: 'created', outcome: 'success',
      verdict: forged.verdict, pass: forgedCaught, reason: forged.reason,
      oracleRefs: [phantomBeforeRef, phantomAfterRef], cleanupRef: null, transcriptRef: null,
      note: 'DELIBERATE FORGERY: no request was sent. This is the suite proving it can catch a lie.',
    });
    log(`${forgedCaught ? 'PASS' : 'FAIL'}  forged-success-detector  ${forged.verdict}`);

    // ═════════ SCENARIO 3 — a REAL refusal really changed nothing ════════════
    const refusedName = 'M_Task50Refused';
    const refusedPath = `${namespace.gameRoot}/${refusedName}`;
    const refusedBefore = observeAssetPackage({ projectRoot: PROJECT_ROOT, objectPath: refusedPath });
    const refusedBeforeRef = aggregator.addObservation(refusedBefore, { phase: 'pre' });
    const refused = await call(native, aggregator, {
      operation: 'execute', capability: 'material.create_material',
      params: { name: refusedName, path: namespace.gameRoot },
    });
    await wait(SETTLE_MS);
    const refusedAfter = observeAssetPackage({ projectRoot: PROJECT_ROOT, objectPath: refusedPath });
    const refusedAfterRef = aggregator.addObservation(refusedAfter, { phase: 'post' });
    const refusedVerdict = judgeClaim({
      claim: { outcome: refused.outcome, effect: 'created', target: refusedPath },
      before: refusedBefore, after: refusedAfter,
    });
    results.push({
      scenario: 'consent-refusal-changed-nothing',
      status: refusedVerdict.pass && refused.outcome === 'error' ? 'PASS' : 'FAIL',
      verdict: refusedVerdict.verdict, observedOutcome: refused.outcome,
      errorCode: refused.receipt?.errorCode ?? null, reason: refusedVerdict.reason,
    });
    aggregator.document.claims.push({
      id: 'claim-refusal', target: refusedPath, effect: 'created', outcome: refused.outcome,
      verdict: refusedVerdict.verdict, pass: refusedVerdict.pass, reason: refusedVerdict.reason,
      oracleRefs: [refusedBeforeRef, refusedAfterRef], cleanupRef: null, transcriptRef: refused.transcriptRef,
    });
    log(`${refusedVerdict.pass ? 'PASS' : 'FAIL'}  consent-refusal  ${refusedVerdict.verdict} (${refused.outcome})`);

    // ═══════════ SCENARIO 4 — a read really is read-only, tree-wide ══════════
    const treeBefore = observeTree({ root: namespace.diskRoot, kind: 'namespace' });
    const treeBeforeRef = aggregator.addObservation(treeBefore, { phase: 'pre' });
    const read = await call(native, aggregator, { operation: 'execute', capability: 'asset.list', params: { path: namespace.gameRoot } });
    await wait(500);
    const treeAfter = observeTree({ root: namespace.diskRoot, kind: 'namespace' });
    const treeAfterRef = aggregator.addObservation(treeAfter, { phase: 'post' });
    const readVerdict = judgeClaim({
      claim: { outcome: read.outcome, effect: 'unchanged', target: namespace.gameRoot },
      before: treeBefore, after: treeAfter,
    });
    results.push({ scenario: 'read-is-read-only', status: readVerdict.pass ? 'PASS' : 'FAIL', verdict: readVerdict.verdict, reason: readVerdict.reason });
    aggregator.document.claims.push({
      id: 'claim-read-only', target: namespace.gameRoot, effect: 'unchanged', outcome: read.outcome,
      verdict: readVerdict.verdict, pass: readVerdict.pass, reason: readVerdict.reason,
      oracleRefs: [treeBeforeRef, treeAfterRef], cleanupRef: null, transcriptRef: read.transcriptRef,
    });
    log(`${readVerdict.pass ? 'PASS' : 'FAIL'}  read-is-read-only  ${readVerdict.verdict}`);

    // ═══════════════ CLEANUP — through the editor, verified by disk ══════════
    const leftovers = residualContent(namespace);
    for (const leftover of leftovers) {
      const guard = namespace.owns('content', leftover.objectPath);
      if (!guard.owned) {
        log(`SKIP delete of ${leftover.objectPath}: ${guard.reason} — not ours to remove`);
        continue;
      }
      await call(native, aggregator, {
        operation: 'execute', capability: 'asset.delete_asset',
        params: { paths: [leftover.objectPath] },
        consent: { capability: 'asset.delete_asset', acknowledge: 'elevated' },
      });
    }
    await wait(SETTLE_MS);
    const afterCleanup = observeTree({ root: namespace.diskRoot, kind: 'namespace' });
    const cleanupRef = aggregator.addObservation(afterCleanup, { phase: 'cleanup' });
    const cleanupVerdict = judgeCleanup({
      baseline: namespace.baseline, afterCleanup, owned: namespace.gameRoot,
    });
    aggregator.document.cleanup.push({
      id: 'cleanup-owned-namespace', owned: namespace.gameRoot, verifiedBy: cleanupRef,
      pass: cleanupVerdict.pass, verdict: cleanupVerdict.verdict, reason: cleanupVerdict.reason,
      deletedThroughEditor: leftovers.map((entry) => entry.objectPath),
      residualAfter: residualContent(namespace).map((entry) => entry.objectPath),
    });
    results.push({ scenario: 'cleanup-restores-pre-state', status: cleanupVerdict.pass ? 'PASS' : 'FAIL', verdict: cleanupVerdict.verdict, reason: cleanupVerdict.reason });
    log(`${cleanupVerdict.pass ? 'PASS' : 'FAIL'}  cleanup  ${cleanupVerdict.verdict}`);
  } catch (error) {
    blocked.push(`probe aborted: ${error instanceof Error ? error.message : String(error)}`);
    log(String(error instanceof Error ? error.stack : error));
  } finally {
    const teardown = await ledger.teardown();
    const closed = namespace.close();
    aggregator.document.environment.teardown = teardown;
    aggregator.document.environment.namespace = { ...manifest, closed };
    aggregator.document.environment.interruptionRecovery = {
      scanned: interrupted.scanned,
      reclaimed,
      leftAloneAsActive: interrupted.active.map((entry) => entry.runId),
      unrecognisedAndUntouched: interrupted.unreadable,
    };
    aggregator.document.environment.results = results;
    aggregator.document.environment.blocked = blocked;

    // Post-run process census. Never a kill list: Task 49 found six unrelated
    // `node dist/cli.js` processes on this host and correctly left them alone.
    aggregator.document.environment.processCensus = recordCommand({ file: 'pgrep', args: ['-af', 'node dist/cli.js'] });

    const passes = results.filter((entry) => entry.status === 'PASS').length;
    const fails = results.filter((entry) => entry.status !== 'PASS').length;
    const verdict = blocked.length > 0
      ? `BLOCKED — ${blocked.join(' | ')}`
      : `${passes} PASS / ${fails} FAIL over ${results.length} independently-observed scenarios`;
    const document = aggregator.finalize(verdict);

    const validation = validateEvidence(document, { projectRoot: process.cwd() });
    document.environment.selfValidation = validation;
    const written = aggregator.write(OUT);

    log(`\n${verdict}`);
    log(`teardown: ${teardown.released}/${teardown.total} released, ${teardown.leaked} leaked`);
    log(`namespace restored: ${closed.contentRestored}; temp released: ${closed.tempReleased}`);
    log(describeRejections(validation));
    log(`wrote ${written}`);
    if (!validation.valid || fails > 0 || blocked.length > 0) process.exitCode = 1;
  }
}

main().catch((error) => {
  const named = error?.name === 'StaleBuildRefusal';
  log(named ? error.message : String(error?.stack ?? error));
  process.exitCode = 1;
});
