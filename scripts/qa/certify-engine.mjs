#!/usr/bin/env node
// Task 52 — the inventory-driven disposable certification orchestrator.
//
// One command certifies one engine minor, end to end, owning everything it
// touches and leaving nothing behind:
//
//   identify -> package -> generate project -> build -> prove the binary is fresh
//   -> claim ports -> run the C++ automation suite -> run both live drivers
//   -> prove the ports and processes are gone -> delete the workspace -> evidence
//
// WHAT THIS REFUSES TO DO, and why each refusal exists:
//
//   It never reads a version out of a folder name. `/data/UnrealEngine` holds
//   5.7.4 and `/data/UnrealEngine-5.8.0-preview-1` holds a tree whose own git tag
//   is `5.8.0-release`. A glob-driven orchestrator would package for, build
//   against and then report the wrong engine, and nothing downstream could tell.
//
//   It never reuses a binary across minors. The workspace is fresh per run, the
//   plugin is packaged per run, and the compiled .so must be newer than the
//   sources it came from or the run stops. A stale .so certifies yesterday.
//
//   It never touches anything outside its own /tmp/opencode workspace. Not
//   /data/Game/MCPtest, not an engine root, not another editor, not a listener it
//   did not open. Every removal goes through the ownership guard.
//
// Run: node scripts/qa/certify-engine.mjs --engine-version 5.7 [--out FILE] [--keep]

import { copyFileSync, existsSync, mkdirSync } from 'node:fs';
import { basename, dirname, join, relative } from 'node:path';

import { buildEngineInventory, formatInventoryTable } from '../../tests/unit/engine-certification/engine-inventory.mjs';
import { DisposableWorkspace, surveyOwnedParent } from '../../tests/unit/engine-certification/disposable-project.mjs';
import { scaffoldFiles } from '../../tests/unit/engine-certification/project-scaffold.mjs';
import {
  buildEditorTarget, judgeBinaryFreshness, launchEditor, materializeProject,
  packagePlugin, parseAutomationLog, portAnswers, runCommand, sha256File, waitForPort,
} from '../../tests/unit/engine-certification/certification-stages.mjs';
import { checkDistFreshness } from '../../tests/unit/cross-transport/dist-freshness.mjs';
import { runCertificationDrivers } from '../../tests/unit/engine-certification/certification-drivers.mjs';
import {
  judgeCleanupAgreement, judgeCleanupRelease, judgeEditorLiveness, judgeProcessRelease, judgeTreeStability,
} from '../../tests/unit/engine-certification/certification-verdict.mjs';
import { EvidenceAggregator, identifyEngine } from '../../tests/unit/evidence-oracles/evidence-aggregator.mjs';
import { describeRejections, validateEvidence } from '../../tests/unit/evidence-oracles/evidence-validator.mjs';
import { observeListener, observeProcess, observeTree } from '../../tests/unit/evidence-oracles/state-oracles.mjs';

const argOf = (name, fallback) => {
  const index = process.argv.indexOf(name);
  return index >= 0 ? process.argv[index + 1] : fallback;
};
const flag = (name) => process.argv.includes(name);

const REPO = process.cwd();
const MINOR = argOf('--engine-version', '5.7');
const OUT = argOf('--out', `.omo/evidence/engine-certification/certify-${MINOR}.json`);
const KEEP = flag('--keep');
const PROJECT = 'McpCert';
const log = (line) => { process.stderr.write(`${line}\n`); };

/** The files whose identity this evidence depends on. */
const TREE = [
  'tests/unit/engine-certification/engine-identity.mjs',
  'tests/unit/engine-certification/engine-inventory.mjs',
  'tests/unit/engine-certification/preprocessor-conditions.mjs',
  'tests/unit/engine-certification/profile-matrix.mjs',
  'tests/unit/engine-certification/disposable-project.mjs',
  'tests/unit/engine-certification/project-scaffold.mjs',
  'tests/unit/engine-certification/certification-stages.mjs',
  'tests/unit/engine-certification/certification-drivers.mjs',
  'tests/unit/engine-certification/certification-verdict.mjs',
  'scripts/qa/certify-engine.mjs',
];

async function main() {
  if (process.env.MOCK_UNREAL_CONNECTION === 'true') {
    log('REFUSING TO RUN: MOCK_UNREAL_CONNECTION=true. A mocked run is not a certification.');
    process.exitCode = 2;
    return;
  }

  const aggregator = new EvidenceAggregator({
    task: 52,
    title: 'Build capability simulation and disposable UE certification orchestration',
    plan: '.omo/plans/pure-unreal-mcp-implementation.md',
    kind: 'wave-6 version/certification lane',
  });
  aggregator.recordTree(TREE);
  const stages = [];
  const blocked = [];
  const stage = (name, ok, detail, extra = {}) => {
    stages.push({ name, ok, detail, ...extra });
    log(`${ok ? 'PASS' : 'FAIL'}  ${name}${detail === null ? '' : `  ${detail}`}`);
    return ok;
  };

  // ── 1. IDENTIFY. Nothing below may name an engine this stage did not resolve ──
  const inventory = buildEngineInventory({ searchDirs: ['/data'] });
  log(formatInventoryTable(inventory));
  aggregator.document.environment.engineInventory = {
    table: formatInventoryTable(inventory),
    available: inventory.available.map(({ identity: _identity, ...rest }) => rest),
    missing: inventory.missing,
    duplicates: inventory.duplicates,
    unusable: inventory.unusable,
    folderNameContradictions: inventory.folderNameContradictions,
  };
  const resolved = inventory.resolve(MINOR);
  stage('inventory.resolve', resolved.ok, resolved.detail ?? `${MINOR} -> ${resolved.root}`);
  if (!resolved.ok) {
    blocked.push(`cannot certify ${MINOR}: ${resolved.reason} — ${resolved.detail}`);
    return finish(aggregator, stages, blocked, null, null);
  }
  const engineRoot = resolved.root;
  const engine = resolved.identity.version;
  aggregator.document.engine = {
    ...identifyEngine({ engineRoot, projectPath: '(generated below)' }),
    resolvedFrom: 'Engine/Build/Build.version',
    versionHeaderAgrees: resolved.identity.sources.versionHeader.agrees,
    gitDescribe: resolved.identity.sources.gitDescribe.raw,
    channel: resolved.identity.channel,
    folderNameAgrees: resolved.identity.folderName.agrees,
  };

  // ── 2. OWN. One workspace, stamped, with its own ports ───────────────────────
  const workspace = new DisposableWorkspace({ purpose: `task-52 certification of UE ${resolved.identity.versionString}` });
  workspace.open();
  const ports = await workspace.claimPorts(['native', 'wsPrimary', 'wsSecondary']);
  aggregator.document.environment.workspace = { ...workspace.manifest(), engineRoot };
  aggregator.document.environment.ownedParentSurvey = surveyOwnedParent();
  stage('workspace.open', true, `${workspace.root}  ports ${JSON.stringify(ports)}`);

  let editor = null;
  try {
    // ── 3. PACKAGE, fresh, for THIS engine ────────────────────────────────────
    const packaged = packagePlugin({
      repoRoot: REPO, engineRoot,
      outDir: workspace.dir('package'),
      logFile: workspace.path('logs/package-plugin.log'),
    });
    aggregator.addCommand(packaged.command);
    if (!stage('package.plugin', packaged.ok, packaged.detail ?? `${packaged.archive} (${packaged.archiveBytes} bytes, sha ${String(packaged.archiveSha256).slice(0, 12)})`)) {
      blocked.push(`packaging failed for ${engineRoot}`);
      return finish(aggregator, stages, blocked, workspace, editor);
    }
    aggregator.document.environment.package = {
      archive: packaged.archive, sha256: packaged.archiveSha256,
      bytes: packaged.archiveBytes, manifest: packaged.manifest,
    };

    // ── 4. GENERATE a disposable C++ project and unpack the plugin into it ─────
    const project = materializeProject({
      projectDir: workspace.dir('project'), name: PROJECT, archive: packaged.archive,
      files: scaffoldFiles({ name: PROJECT, engine, nativePort: ports.native, wsPorts: [ports.wsPrimary, ports.wsSecondary] }),
    });
    aggregator.addCommand(project.command);
    if (!stage('project.materialize', project.ok, project.detail ?? project.projectFile)) {
      blocked.push('the disposable project could not be assembled');
      return finish(aggregator, stages, blocked, workspace, editor);
    }

    // ── 5. BUILD with THIS engine's UBT ───────────────────────────────────────
    const built = buildEditorTarget({
      engineRoot, projectFile: project.projectFile, target: `${PROJECT}Editor`,
      logFile: workspace.path('logs/ubt-build.log'),
    });
    aggregator.addCommand(built.command);
    if (!stage('build.editorTarget', built.ok, built.detail ?? 'compiled', { errors: built.errors.slice(0, 5) })) {
      blocked.push(`UBT failed: ${built.errors.slice(0, 3).join(' | ')}`);
      return finish(aggregator, stages, blocked, workspace, editor);
    }

    // ── 6. PROVE THE BINARY IS FRESH ─────────────────────────────────────────
    const binary = join(project.pluginRoot, 'Binaries/Linux/libUnrealEditor-McpAutomationBridge.so');
    const freshness = judgeBinaryFreshness({ binary, sourceRoot: join(project.pluginRoot, 'Source') });
    aggregator.document.environment.binaryFreshness = freshness;

    // ARTIFACTS MUST OUTLIVE THE RUN. The first live run recorded the .so and the
    // zip at their workspace paths, cleanup then deleted the workspace, and the
    // validator correctly refused the evidence: a hash of bytes that no longer
    // exist cannot be re-checked by anyone. A disposable certification therefore
    // preserves the small artifact that identifies WHAT was built, and records the
    // 70MB binary's digest as a measurement rather than as a re-checkable file.
    const keptArchive = join(REPO, '.omo/evidence/engine-certification/artifacts', `${workspace.runId}-${basename(packaged.archive)}`);
    mkdirSync(dirname(keptArchive), { recursive: true });
    copyFileSync(packaged.archive, keptArchive);
    aggregator.document.environment.package.preservedAt = relative(REPO, keptArchive);
    aggregator.document.environment.package.preservedSha256 = sha256File(keptArchive);
    aggregator.recordArtifact({
      path: relative(REPO, keptArchive),
      inputsNewest: freshness.newestInput, inputsNewestAtMs: freshness.newestInputMtimeMs,
    });

    if (!stage('build.binaryFresh', freshness.fresh, `${freshness.reason}; sha ${String(freshness.binarySha256).slice(0, 12)}`)) {
      blocked.push(`the compiled plugin is ${freshness.reason}; certifying it would certify the wrong sources`);
      return finish(aggregator, stages, blocked, workspace, editor);
    }

    // ── 7. PORT COLLISION CHECK, at the moment of use ────────────────────────
    const collision = await workspace.verifyPortsStillFree();
    aggregator.document.environment.portCheck = collision.rows;
    if (!stage('ports.stillFree', collision.collided.length === 0,
      collision.collided.length === 0 ? `${collision.rows.length} ports free` : `COLLISION on ${collision.collided.map((row) => `${row.name}:${row.port}`).join(', ')}`)) {
      blocked.push('a claimed port was taken between allocation and use');
      return finish(aggregator, stages, blocked, workspace, editor);
    }

    // ── 8. C++ AUTOMATION, in its own short-lived editor ─────────────────────
    const reportDir = workspace.dir('automation-report');
    const automation = runCommand({
      file: join(engineRoot, 'Engine/Binaries/Linux/UnrealEditor-Cmd'),
      args: [project.projectFile, '-unattended', '-nopause', '-nosplash', '-NullRHI', '-NoSound', '-stdout',
        '-FullStdOutLogOutput', '-ExecCmds=Automation RunTests McpAutomationBridge;Quit',
        '-TestExit=Automation Test Queue Empty', `-ReportExportPath=${reportDir}`],
      timeoutMs: 2_700_000,
      logFile: workspace.path('logs/automation.log'),
      tail: 8000,
    });
    aggregator.addCommand(automation.record);
    const results = parseAutomationLog(`${automation.stdout}\n${automation.stderr}`);
    aggregator.document.environment.automation = {
      ...results,
      resultTable: results.resultTable.slice(0, 400),
      exitCode: automation.record.exitCode,
      reportDir,
      reportPresent: existsSync(join(reportDir, 'index.json')),
    };
    stage('automation.startedEqualsCompleted', results.startedEqualsCompleted,
      `started ${results.startedCount}, completed ${results.completedCount}, tally ${JSON.stringify(results.tally)}`);
    stage('automation.noFailures', results.failed.length === 0,
      results.failed.length === 0 ? 'every completed test passed' : `${results.failed.length} failed: ${results.failed.slice(0, 5).join(', ')}`);

    // ── 9. LIVE EDITOR + BOTH DRIVERS ────────────────────────────────────────
    editor = launchEditor({
      engineRoot, projectFile: project.projectFile,
      args: ['-unattended', '-nopause', '-nosplash', '-NullRHI', '-NoSound', '-stdout', '-FullStdOutLogOutput'],
      logFile: workspace.path('logs/editor-live.log'),
      env: { ...process.env, MCP_NATIVE_PORT: String(ports.native) },
    });
    const editorProc = observeProcess({ pid: editor.pid });
    aggregator.recordProcess({ pid: editor.pid, role: 'UnrealEditor-Cmd (spawned, owned and killed by this run)' });
    aggregator.document.environment.editor = {
      pid: editor.pid, command: editor.command,
      startTicks: editorProc.detail.startTicks ?? null,
      startedAt: new Date().toISOString(),
    };
    const nativeReady = await waitForPort({ port: ports.native, timeoutMs: 600_000 });
    const bridgeReady = await waitForPort({ port: ports.wsPrimary, timeoutMs: 120_000 });
    stage('editor.nativeListening', nativeReady.ready, `port ${ports.native} after ${nativeReady.attempts} probes`);
    stage('editor.bridgeListening', bridgeReady.ready, `port ${ports.wsPrimary} after ${bridgeReady.attempts} probes`);
    for (const [name, port] of Object.entries(ports)) {
      aggregator.addObservation(observeListener({ port }), { phase: 'post', id: `obs-listener-${name}-up` });
    }

    // WHY THE PORT IS NOT THE QUESTION. "the native port never bound" covers an
    // editor that died on a fatal error and an editor that is merely slower than
    // the timeout — different defects, and reporting a crash for a process that
    // is still running also reports a kill that never happened. The inverse is
    // worse: a port that answers while THIS run's pid is gone belongs to
    // something this run did not start, and every driver below would score
    // against it happily. Both are decided here, from the pid and the log.
    const liveness = judgeEditorLiveness({ pid: editor.pid, portReady: nativeReady.ready, logText: editor.text() });
    aggregator.document.environment.editor.liveness = {
      verdict: liveness.verdict, detail: liveness.detail, alive: liveness.alive, crash: liveness.crash,
    };
    if (liveness.observation !== null) {
      aggregator.addObservation(liveness.observation, { phase: 'post', id: 'obs-editor-liveness' });
    }
    stage('editor.alive', liveness.ok, `${liveness.verdict}: ${liveness.detail}`);

    // ── 10. dist/ IMMEDIATELY BEFORE THE STDIO DRIVER USES IT ────────────────
    // The stdio driver spawns `node dist/cli.js` and refuses a build older than
    // src/ — correctly, because it would otherwise report that build's behavior
    // as the tree's. On a shared worktree the window matters: two runs were
    // refused after another lane regenerated console-command-policy.generated.ts
    // fourteen minutes INTO a certification, long after any build at launch. So
    // the rebuild happens here, seconds before use, and is a recorded command
    // rather than a silent one — the driver's refusal still stands guard.
    const rebuilt = runCommand({
      file: 'npm', args: ['run', 'build'], cwd: REPO,
      timeoutMs: 900_000, logFile: workspace.path('logs/npm-build.log'),
    });
    aggregator.addCommand(rebuilt.record);
    const distFreshness = checkDistFreshness(REPO);
    aggregator.recordArtifact({
      path: distFreshness.entry,
      inputsNewest: distFreshness.newestInput, inputsNewestAtMs: distFreshness.newestInputMtimeMs,
    });
    stage('dist.fresh', distFreshness.fresh,
      `${distFreshness.entry} vs newest input ${distFreshness.newestInput ?? 'none'} (${distFreshness.reason})`);

    if (liveness.ok) {
      const drivers = await runCertificationDrivers({ ports, repoRoot: REPO, aggregator, projectDir: project.projectDir });
      aggregator.document.environment.drivers = drivers;
      stage('drivers.native', drivers.native.ok, drivers.native.detail);
      stage('drivers.stdio', drivers.stdio.ok, drivers.stdio.detail);
      stage('drivers.corpusSubset', drivers.corpus.pass > 0 && drivers.corpus.fail === 0,
        `${drivers.corpus.pass} pass / ${drivers.corpus.fail} fail / ${drivers.corpus.blocked} blocked over ${drivers.corpus.total} cases on ${drivers.corpus.transports.join('+')}`);
    } else {
      blocked.push(`the driver stages could not run: ${liveness.verdict} — ${liveness.detail}`);
    }
  } catch (error) {
    blocked.push(`certification aborted: ${error instanceof Error ? error.message : String(error)}`);
    log(String(error instanceof Error ? error.stack : error));
  }
  return finish(aggregator, stages, blocked, workspace, editor);
}

/** Cleanup is a STAGE, verified out of band, and it runs whatever happened above. */
async function finish(aggregator, stages, blocked, workspace, editor) {
  const cleanup = { editor: null, ports: [], workspace: null };
  // Every owned resource, judged by TWO mechanisms. A teardown graded by the
  // mechanism that performed it is the shape that wrote `cleanupClean: true` over
  // two leaked materials in an earlier wave; the first live run of this lane
  // scored port release from its own connect() probe while separately recording
  // an independent /proc/net/tcp reading and never compared the two.
  const agreement = [];
  if (editor !== null && editor.pid !== null) {
    editor.flush();
    try {
      process.kill(-editor.pid, 'SIGTERM');
    } catch { /* already gone */ }
    for (let waited = 0; waited < 60_000; waited += 1000) {
      if (observeProcess({ pid: editor.pid }).present !== true) break;
      await new Promise((settle) => { setTimeout(settle, 1000); });
    }
    if (observeProcess({ pid: editor.pid }).present === true) {
      try {
        process.kill(-editor.pid, 'SIGKILL');
      } catch { /* raced with exit */ }
      await new Promise((settle) => { setTimeout(settle, 2000); });
    }
    // A signal-0 probe (syscall) and procfs (a file read) are genuinely different
    // readings of the same pid, and they disagree in exactly one interesting
    // place — an unreaped zombie — which the judge names rather than scores.
    const release = judgeProcessRelease({ pid: editor.pid, resource: `editor pid ${editor.pid}` });
    agreement.push(release);
    cleanup.editor = {
      pid: editor.pid, gone: release.ok, verdict: release.verdict,
      observed: JSON.stringify(release.observation.detail).slice(0, 300),
    };
    const editorRef = aggregator.addObservation(release.observation, { phase: 'cleanup', id: 'obs-editor-gone' });
    aggregator.document.cleanup.push({
      id: 'cleanup-owned-editor', owned: `pid ${editor.pid}`, verifiedBy: editorRef,
      pass: release.ok, verdict: release.verdict, reason: release.reason,
    });
    stages.push({ name: 'cleanup.editor', ok: release.ok, detail: release.reason });
  }
  if (workspace !== null) {
    for (const [name, port] of Object.entries(workspace.ports)) {
      const answering = await portAnswers(port);
      const seen = observeListener({ port });
      const ref = aggregator.addObservation(seen, { phase: 'cleanup', id: `obs-listener-${name}-down` });
      const row = judgeCleanupRelease({
        resource: `127.0.0.1:${port} (${name})`, claimedReleased: !answering,
        claimedBy: `connect() probe (${answering ? 'answered' : 'refused'})`, observation: seen,
      });
      agreement.push(row);
      cleanup.ports.push({ name, port, released: row.ok, verdict: row.verdict });
      aggregator.document.cleanup.push({
        id: `cleanup-port-${name}`, owned: `127.0.0.1:${port}`, verifiedBy: ref,
        pass: row.ok, verdict: row.verdict, reason: row.reason,
      });
    }
    const before = observeTree({ root: workspace.root, kind: 'owned-workspace' });
    aggregator.addObservation(before, { phase: 'pre', id: 'obs-workspace-present' });
    if (KEEP) {
      const kept = observeTree({ root: workspace.root, kind: 'owned-workspace' });
      const ref = aggregator.addObservation(kept, { phase: 'cleanup', id: 'obs-workspace-kept' });
      const row = judgeCleanupRelease({
        resource: workspace.root, claimedReleased: false, claimedBy: '--keep (no removal attempted)', observation: kept,
      });
      agreement.push(row);
      cleanup.workspace = { removed: false, reason: 'KEPT_BY_REQUEST', detail: `--keep was passed; ${workspace.root} is deliberately left in place` };
      aggregator.document.cleanup.push({
        id: 'cleanup-owned-workspace', owned: workspace.root, verifiedBy: ref,
        pass: false, verdict: 'KEPT_BY_REQUEST', reason: row.reason,
      });
      stages.push({ name: 'cleanup.workspace', ok: false, detail: 'KEPT (--keep): this run intentionally leaves residue and is NOT a clean certification' });
    } else {
      const receipt = workspace.close();
      const after = observeTree({ root: workspace.root, kind: 'owned-workspace' });
      const ref = aggregator.addObservation(after, { phase: 'cleanup', id: 'obs-workspace-gone' });
      const row = judgeCleanupRelease({
        resource: workspace.root, claimedReleased: receipt.removed === true,
        claimedBy: `rm receipt (${String(receipt.reason)})`, observation: after,
      });
      agreement.push(row);
      cleanup.workspace = receipt;
      aggregator.document.cleanup.push({
        id: 'cleanup-owned-workspace', owned: workspace.root, verifiedBy: ref,
        pass: row.ok, verdict: receipt.reason,
        reason: `${receipt.detail}; ${row.reason}`,
      });
      stages.push({ name: 'cleanup.workspace', ok: row.ok, detail: `${receipt.detail}; ${row.verdict}` });
    }
  }
  // ── THE TWO READINGS MUST AGREE ───────────────────────────────────────────
  if (agreement.length > 0) {
    const agreed = judgeCleanupAgreement({ rows: agreement });
    aggregator.document.environment.cleanupAgreement = agreed;
    stages.push({ name: 'cleanup.agrees', ok: agreed.ok, detail: agreed.detail });
  }

  // ── DID THE TREE MOVE UNDER THE RUN? ──────────────────────────────────────
  // The validator asks this afterwards; asking it here names the run's own stage
  // instead of surfacing as an unexplained stale-hash rejection. On a shared
  // worktree it is a real event: an earlier 5.7 run was refused because another
  // lane regenerated console-command-policy.generated.ts fourteen minutes into it.
  const stability = judgeTreeStability({
    recorded: aggregator.document.tree.files, projectRoot: REPO, stage: 'cleanup',
  });
  aggregator.document.environment.treeStability = stability;
  stages.push({ name: 'tree.stable', ok: stability.stable, detail: stability.detail });
  if (!stability.stable) {
    blocked.push(`the source tree changed while this run was going: ${stability.moved.map((entry) => entry.path).join(', ')}`);
  }

  aggregator.document.environment.cleanup = cleanup;
  aggregator.document.environment.stages = stages;
  aggregator.document.environment.blocked = blocked;
  // A control the positive-control audit can see: one port that is definitely
  // free, so "nothing answers" is proven to be a reading and not a blind oracle.
  aggregator.addObservation(observeListener({ port: 65_529 }), { phase: 'control', id: 'obs-listener-control-absent' });
  // The control MUST use the same oracle as the cleanup reading it underwrites.
  // A bespoke `proc:self` mechanism proved that some other reader works and left
  // `procfs:pid` with only absent readings — which is exactly the blind-oracle
  // shape the audit exists to reject, and it rejected it.
  aggregator.addObservation(observeProcess({ pid: process.pid }), { phase: 'control', id: 'obs-process-control-present' });

  const failed = stages.filter((entry) => entry.ok !== true);
  const verdict = blocked.length > 0
    ? `BLOCKED — ${blocked.join(' | ')}`
    : `${stages.length - failed.length}/${stages.length} certification stages passed for UE ${MINOR}`;
  const document = aggregator.finalize(verdict);
  const validation = validateEvidence(document, { projectRoot: REPO });
  document.environment.selfValidation = validation;
  const written = aggregator.write(OUT);
  log(`\n${verdict}`);
  log(describeRejections(validation));
  log(`wrote ${written}`);
  if (failed.length > 0 || blocked.length > 0) process.exitCode = 1;
}

main().catch((error) => {
  log(String(error?.stack ?? error));
  process.exitCode = 1;
});
