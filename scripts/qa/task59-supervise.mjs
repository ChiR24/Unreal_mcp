#!/usr/bin/env node
// Task 59 — run the shared certification orchestrator against UE 5.7.4 and take
// the runtime capability census INSIDE the window where the editor is alive.
//
// WHY THIS EXISTS AT ALL. The orchestrator (`task52-certify-engine.mjs`) launches
// exactly one editor, drives it, and then kills it as a cleanup STAGE that must
// pass. There is no point after the run at which a capability probe could reach
// that editor, and no point before it at which the editor exists. A probe that
// wanted a live 5.7.4 surface therefore had two honest options: launch a second
// editor (a second package, a second UBT build, and a second thing to prove was
// cleaned up), or run beside the first one. This runs beside it.
//
// WHAT THIS DELIBERATELY DOES NOT DO. It does not modify the orchestrator or any
// `tests/unit/task-52/*.mjs` helper. Those files' hashes are recorded inside the
// evidence documents of Tasks 52, 56, 57, 58 and 60; editing one to add a probe
// hook would retroactively invalidate five records whose runs can never be
// repeated, to save writing this file. The orchestrator is driven exactly as
// Task 52 drives it and is read only through its stderr.
//
// THE WINDOW, and why the probe fires where it does. The orchestrator prints one
// PASS line per stage. `editor.nativeListening` is the first moment the native
// /mcp surface answers, and the next thing the orchestrator does is `npm run
// build` for the stdio driver — a quiet, editor-idle stretch of tens of seconds.
// Firing there means the census is taken while nothing else is driving the
// editor, and it is finished before the corpus subset starts. The probe is
// read-only plus refused-execute against targets that do not exist, and it opens
// its own session which it deletes, so the worst case it can impose on the run it
// is watching is a few seconds of game-thread queue.
//
// Everything is spawned detached (setsid), because this supervisor outlives the
// shell that starts it and a plain background job does not.
//
// Run: node scripts/qa/task59-supervise.mjs [--engine-version 5.7] [--outdir DIR]

import { spawn } from 'node:child_process';
import { appendFileSync, mkdirSync, writeFileSync } from 'node:fs';

const argOf = (name, fallback) => {
  const index = process.argv.indexOf(name);
  return index >= 0 ? process.argv[index + 1] : fallback;
};

const REPO = process.cwd();
const MINOR = argOf('--engine-version', '5.7');
const OUTDIR = argOf('--outdir', '.omo/evidence/task-59');
const CERT_OUT = `${OUTDIR}/certify-5.7.4.json`;
const PROBE_OUT = `${OUTDIR}/capability-probe.json`;
const CERT_LOG = `${REPO}/${OUTDIR}/certify-5.7.4.log`;
const PROBE_LOG = `${REPO}/${OUTDIR}/capability-probe.log`;
const RECEIPT = `${REPO}/${OUTDIR}/supervisor.json`;

mkdirSync(`${REPO}/${OUTDIR}`, { recursive: true });
writeFileSync(CERT_LOG, '');
writeFileSync(PROBE_LOG, '');

const log = (line) => { process.stderr.write(`[supervise] ${line}\n`); };

/** @type {Record<string, unknown>} */
const receipt = {
  task: 59,
  startedAt: new Date().toISOString(),
  supervisorPid: process.pid,
  engineVersionRequested: MINOR,
  certification: { out: CERT_OUT, log: CERT_LOG, pid: null, exitCode: null },
  probe: {
    out: PROBE_OUT, log: PROBE_LOG, pid: null, exitCode: null,
    firedAt: null, firedAfterStage: null, ports: null, workspaceRoot: null, skipped: null,
  },
  stageLines: /** @type {string[]} */ ([]),
};
const saveReceipt = () => { writeFileSync(RECEIPT, `${JSON.stringify(receipt, null, 2)}\n`); };
saveReceipt();

// ── the certification, driven exactly as Task 52 drives it ──────────────────
const certification = spawn(process.execPath, [
  'scripts/qa/task52-certify-engine.mjs', '--engine-version', MINOR, '--out', CERT_OUT,
], { cwd: REPO, detached: true, stdio: ['ignore', 'pipe', 'pipe'] });
receipt.certification.pid = certification.pid ?? null;
log(`certification pid ${String(certification.pid)} -> ${CERT_OUT}`);
saveReceipt();

let buffered = '';
let probeFired = false;
/** @type {Record<string, number>|null} */
let ports = null;
/** @type {string|null} */
let workspaceRoot = null;

/**
 * The workspace root and its ports are read from the orchestrator's own
 * `workspace.open` line rather than allocated or guessed here. Guessing a port
 * would let the probe address an editor this run did not launch, which is the
 * single failure mode that looks most like success; guessing the workspace would
 * make it read another run's compiled definitions.
 */
function readWorkspaceLine(line) {
  const match = /workspace\.open\s+(\S+)\s+ports\s+(\{.*\})\s*$/u.exec(line.trim());
  if (match === null) return null;
  try {
    return { root: match[1], ports: JSON.parse(match[2]) };
  } catch {
    return null;
  }
}

function fireProbe(afterStage) {
  if (probeFired) return;
  probeFired = true;
  receipt.probe.firedAt = new Date().toISOString();
  receipt.probe.firedAfterStage = afterStage;
  receipt.probe.ports = ports;
  receipt.probe.workspaceRoot = workspaceRoot;
  if (ports === null || typeof ports.native !== 'number') {
    receipt.probe.skipped = 'PORTS_NEVER_ANNOUNCED: the orchestrator did not print a parseable workspace.open ports line, '
      + 'so no port could be addressed without guessing one';
    log(String(receipt.probe.skipped));
    saveReceipt();
    return;
  }
  const probe = spawn(process.execPath, [
    'scripts/qa/task59-capability-probe.mjs',
    '--native-port', String(ports.native),
    '--out', PROBE_OUT,
    ...(workspaceRoot === null ? [] : ['--project-dir', `${workspaceRoot}/project`]),
  ], { cwd: REPO, detached: true, stdio: ['ignore', 'pipe', 'pipe'] });
  receipt.probe.pid = probe.pid ?? null;
  log(`probe pid ${String(probe.pid)} on native port ${String(ports.native)} (after ${afterStage})`);
  const collect = (chunk) => { appendFileSync(PROBE_LOG, String(chunk)); };
  probe.stdout?.on('data', collect);
  probe.stderr?.on('data', collect);
  probe.on('exit', (code) => {
    receipt.probe.exitCode = code;
    log(`probe exited ${String(code)}`);
    saveReceipt();
  });
  saveReceipt();
}

const onOutput = (chunk) => {
  const text = String(chunk);
  appendFileSync(CERT_LOG, text);
  buffered += text;
  const lines = buffered.split('\n');
  buffered = lines.pop() ?? '';
  for (const line of lines) {
    if (/^(?:PASS|FAIL)\s+/u.test(line)) {
      receipt.stageLines.push(line);
      log(line);
    }
    if (ports === null && line.includes('workspace.open')) {
      const opened = readWorkspaceLine(line);
      if (opened !== null) {
        ports = opened.ports;
        workspaceRoot = opened.root;
        receipt.probe.ports = ports;
        receipt.probe.workspaceRoot = workspaceRoot;
        log(`owned workspace ${workspaceRoot} ports ${JSON.stringify(ports)}`);
        saveReceipt();
      }
    }
    // The first stage at which the native surface is proven to answer. Waiting
    // for a later stage would risk landing inside the corpus; firing earlier
    // would address a port that has not bound yet.
    if (line.startsWith('PASS  editor.nativeListening')) fireProbe('editor.nativeListening');
    // If the editor never binds, the probe cannot run and must be recorded as
    // NOT ATTEMPTED with the reason. An unreached probe that leaves no row reads
    // downstream as a probe that found nothing wrong.
    if (line.startsWith('FAIL  editor.nativeListening') || line.startsWith('FAIL  editor.alive')) {
      if (!probeFired) {
        probeFired = true;
        receipt.probe.skipped = `NOT_ATTEMPTED: ${line.trim()} — the native surface never became addressable, `
          + 'so no capability census was taken and none is inferred';
        log(String(receipt.probe.skipped));
        saveReceipt();
      }
    }
  }
};

certification.stdout?.on('data', onOutput);
certification.stderr?.on('data', onOutput);
certification.on('exit', (code) => {
  receipt.certification.exitCode = code;
  receipt.finishedAt = new Date().toISOString();
  if (!probeFired) {
    receipt.probe.skipped = `NOT_ATTEMPTED: the certification exited ${String(code)} before editor.nativeListening passed`;
  }
  log(`certification exited ${String(code)}`);
  saveReceipt();
  // The probe may still be draining; give it a bounded chance to finish and
  // record its own exit before this supervisor's receipt is final.
  setTimeout(() => {
    receipt.finishedAt = new Date().toISOString();
    saveReceipt();
    process.exit(0);
  }, 20_000);
});
