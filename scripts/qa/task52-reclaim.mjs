#!/usr/bin/env node
// Task 52 — reclaim runs this lane abandoned.
//
// An orchestrator that dies mid-package leaves two things behind: a workspace
// under /tmp/opencode and, worse, a live RunUAT holding the engine-wide
// AutomationTool mutex. The next certification then fails at its first stage
// with "a conflicting instance of AutomationTool is already running", which is
// true, unhelpful, and points at the wrong run.
//
// This reclaims ONLY runs whose manifest owner process is really gone. A run
// whose owner is alive is somebody's work in progress and is left strictly
// alone — "it looked stale" is the reasoning that ends with a colleague's build
// torn down mid-flight.
//
// Run: node scripts/qa/task52-reclaim.mjs [--dry-run]

import { findOrphanedProcesses, reclaimOrphanedRun, surveyOwnedParent, sweepOrphanedProcesses } from '../../tests/unit/task-52/disposable-project.mjs';

const DRY = process.argv.includes('--dry-run');
const log = (line) => { process.stderr.write(`${line}\n`); };

const survey = surveyOwnedParent();
log(`${survey.scanned} task-52 run(s) under ${survey.parent}`);
const results = [];
for (const run of survey.runs) {
  if (run.ownerAlive === true) {
    log(`LEAVE    ${run.runId}  owner pid ${run.ownerPid} is alive — this is somebody's run in progress`);
    results.push({ runId: run.runId, action: 'left-alone', reason: 'OWNER_STILL_ALIVE' });
    continue;
  }
  if (DRY) {
    log(`WOULD RECLAIM ${run.runId}  owner pid ${run.ownerPid} is gone`);
    results.push({ runId: run.runId, action: 'would-reclaim' });
    continue;
  }
  const outcome = await reclaimOrphanedRun({ root: String(run.root) });
  log(`${outcome.reclaimed ? 'RECLAIMED' : 'FAILED   '} ${run.runId}  ${outcome.reason}  stopped ${outcome.stopped.length} process(es)`);
  for (const entry of outcome.stopped) log(`    stopped ${entry.pid}  ${entry.command.slice(0, 140)}`);
  results.push({ runId: run.runId, action: outcome.reclaimed ? 'reclaimed' : 'failed', ...outcome });
}
// A crashed orchestrator can leave the harder residue: the workspace already
// removed, and an editor still running against the deleted project, holding its
// ports. A directory scan cannot see that one, so the path in /proc is the proof.
const strays = findOrphanedProcesses();
for (const stray of strays) log(`STRAY    pid ${stray.pid} still running against the removed ${stray.workspace}`);
const sweep = DRY ? { stopped: strays, remaining: strays, clean: strays.length === 0 } : await sweepOrphanedProcesses();
if (!DRY && strays.length > 0) log(`${sweep.clean ? 'SWEPT    ' : 'FAILED   '} ${sweep.stopped.length} stray process(es); ${sweep.remaining.length} remain`);

process.stdout.write(`${JSON.stringify({ parent: survey.parent, results, strays: sweep }, null, 2)}\n`);
if (results.some((entry) => entry.action === 'failed') || sweep.clean !== true) process.exitCode = 1;
