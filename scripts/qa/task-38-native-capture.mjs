#!/usr/bin/env node
// scripts/qa/task-38-native-capture.mjs
// Task 38 lane E — the OWNED runner that drives the executable native-protocol
// capture seam (Private/Tests/Task38NativeProtocolCaptureTests.cpp) and feeds its
// output through the parity harness. It is the exact interface the integration lane
// invokes once a UE5.7 plugin package/editor is serialized.
//
// Modes:
//   --plan     (default) validate inputs and PRINT the exact editor invocation +
//              capture dir + unique namespace/port; launches NOTHING. Reports
//              crossTransportParity BLOCKED-RED (no capture artifact yet).
//   --verify   read an existing capture dir, run ground-truth + schema + provenance
//              verification, and report per-domain parity. BLOCKED-RED when the
//              artifact is absent.
//   --execute  (integration lane only) spawn the editor with a BOUNDED timeout to
//              produce the artifact, then --verify. Refuses without --engine/--project
//              and a live editor; never run in this lane (no UBT/editor here).
//
// Inputs (flags; env fallbacks in parens):
//   --engine=<UE root>            (UNREAL_ENGINE_ROOT)
//   --project=<.uproject path>    (UE_PROJECT_PATH)
//   --endpoint=<http://h:p/mcp>   documented http-sse alternative (unused in-process)
//   --capture-dir=<dir>           default /tmp/opencode/<namespace> (owned scratch)
//   --namespace=<unique>          default task38-<time>-<pid>
//   --port=<n>                    unique MCP_NATIVE_PORT for the editor
//   --timeout-ms=<n>              bounded editor timeout (default 300000)
//   --max-age-ms=<n>              freshness window for --verify (default 86400000)
//   --capability-token=<token>    optional
//   --cleanup                     remove the owned capture dir after --execute
//   --out=<path>                  report path (must stay under .omo/evidence/task-38/harness-)
//   --json-only / --help
//
// SIZE_OK: single responsibility — orchestrate one native-capture run end to end.
// It is one indivisible narrative (plan -> execute -> verify) mirroring task-38-parity-qa.

import { spawnSync } from 'node:child_process';
import { execFileSync } from 'node:child_process';
import { existsSync, mkdirSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { dirname, isAbsolute, join, relative, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

import {
  checkFixture,
  compareCaptures,
  assertParityReady,
  loadEmittedArtifact,
  verifyGroundTruth,
  buildNativeCaptureBundle,
  verifyNativeCaptureProvenance,
  computePluginSourceHash,
  computePluginPackageHash,
} from '../../tests/unit/task-38/parity-harness.mjs';

const HERE = dirname(fileURLToPath(import.meta.url));
const REPO_ROOT = resolve(HERE, '../..');
const FIXTURES_DIR = join(REPO_ROOT, 'tests/fixtures/task-38');
const OWNED_SCRATCH = '/tmp/opencode';
const DEFAULT_OUT = join(REPO_ROOT, '.omo/evidence/task-38/harness-native-capture.json');
const CAPTURE_FILE = 'task38-native-capture.json';
const TRANSCRIPT_FILE = 'native-transcript.jsonl';
const DOMAIN_FIXTURES = {
  result: 'executable-ts-result.json',
  error: 'executable-ts-error.json',
  revision: 'executable-ts-revision.json',
  profile: 'executable-ts-profile.json',
  session: 'executable-ts-session.json',
  pointer: 'executable-ts-pointer.json',
};

// --- helpers ---------------------------------------------------------------

function gitHead() {
  try {
    return execFileSync('git', ['rev-parse', '--short', 'HEAD'], { cwd: REPO_ROOT, encoding: 'utf8' }).trim();
  } catch {
    return 'unknown';
  }
}

function parseArgs(argv) {
  const opts = {
    mode: 'plan', engine: process.env.UNREAL_ENGINE_ROOT ?? '', project: process.env.UE_PROJECT_PATH ?? '',
    endpoint: '', captureDir: '', namespace: '', port: 0, timeoutMs: 300000, maxAgeMs: 86400000,
    token: '', cleanup: false, out: '', jsonOnly: false, help: false, editorBin: '',
  };
  for (const arg of argv) {
    if (arg === '--help' || arg === '-h') opts.help = true;
    else if (arg === '--plan') opts.mode = 'plan';
    else if (arg === '--verify') opts.mode = 'verify';
    else if (arg === '--execute') opts.mode = 'execute';
    else if (arg === '--json-only') opts.jsonOnly = true;
    else if (arg === '--cleanup') opts.cleanup = true;
    else if (arg.startsWith('--engine=')) opts.engine = arg.slice('--engine='.length);
    else if (arg.startsWith('--project=')) opts.project = arg.slice('--project='.length);
    else if (arg.startsWith('--endpoint=')) opts.endpoint = arg.slice('--endpoint='.length);
    else if (arg.startsWith('--capture-dir=')) opts.captureDir = arg.slice('--capture-dir='.length);
    else if (arg.startsWith('--namespace=')) opts.namespace = arg.slice('--namespace='.length);
    else if (arg.startsWith('--port=')) opts.port = Number(arg.slice('--port='.length));
    else if (arg.startsWith('--timeout-ms=')) opts.timeoutMs = Number(arg.slice('--timeout-ms='.length));
    else if (arg.startsWith('--max-age-ms=')) opts.maxAgeMs = Number(arg.slice('--max-age-ms='.length));
    else if (arg.startsWith('--capability-token=')) opts.token = arg.slice('--capability-token='.length);
    else if (arg.startsWith('--editor-bin=')) opts.editorBin = arg.slice('--editor-bin='.length);
    else if (arg.startsWith('--out=')) opts.out = arg.slice('--out='.length);
  }
  if (!opts.namespace) opts.namespace = `task38-${Date.now().toString(36)}-${process.pid}`;
  if (!opts.captureDir) opts.captureDir = join(OWNED_SCRATCH, opts.namespace);
  if (!opts.port || Number.isNaN(opts.port)) opts.port = 38000 + (process.pid % 2000);
  return opts;
}

function editorInvocation(opts) {
  const bin = opts.editorBin || (opts.engine ? join(opts.engine, 'Engine/Binaries/Linux/UnrealEditor-Cmd') : '<engine>/Engine/Binaries/Linux/UnrealEditor-Cmd');
  const args = [
    opts.project || '<project.uproject>',
    '-ExecCmds=Automation RunTests McpAutomationBridge.Task38.NativeProtocolCapture; Quit',
    '-unattended', '-nop4', '-nosplash', '-nullrhi', '-log',
    `-abslog=${join(opts.captureDir, 'editor.log')}`,
  ];
  const env = { MCP_TASK38_CAPTURE_DIR: opts.captureDir, MCP_NATIVE_PORT: String(opts.port) };
  if (opts.token) env.MCP_CAPABILITY_TOKEN = opts.token;
  return { command: bin, args, env };
}

// Confine the capture dir to the owned scratch namespace unless the caller passed an
// explicit absolute dir (their responsibility). Never touch unrelated editors/projects.
function assertOwnedCaptureDir(captureDir) {
  const abs = isAbsolute(captureDir) ? captureDir : join(REPO_ROOT, captureDir);
  const owned = abs.startsWith(`${OWNED_SCRATCH}/task38-`) || abs.includes('/task38-');
  return { abs, owned };
}

// --- verification (used by --verify and after --execute) -------------------

function loadTsCaptures() {
  const out = {};
  for (const [domain, file] of Object.entries(DOMAIN_FIXTURES)) {
    out[domain] = checkFixture(JSON.parse(readFileSync(join(FIXTURES_DIR, file), 'utf8')));
  }
  return out;
}

function verifyCaptureDir(captureDir, opts) {
  const emittedPath = join(captureDir, CAPTURE_FILE);
  if (!existsSync(emittedPath)) {
    return { parity: 'BLOCKED-RED', reason: 'NATIVE_CAPTURE_ABSENT', detail: `no ${CAPTURE_FILE} in ${relative(REPO_ROOT, captureDir)}; run --execute against a built editor first`, domains: [] };
  }
  const raw = loadEmittedArtifact(emittedPath);
  const ground = verifyGroundTruth(raw);
  const bundle = buildNativeCaptureBundle(raw, { root: REPO_ROOT, transcriptRef: TRANSCRIPT_FILE });
  writeFileSync(join(captureDir, TRANSCRIPT_FILE), bundle.transcriptJsonl, 'utf8');
  const expectedSourceHash = computePluginSourceHash(REPO_ROOT);
  const expectedPackageHash = computePluginPackageHash(REPO_ROOT);
  const ts = loadTsCaptures();
  const now = Date.now();

  const domains = [];
  for (const rawNative of bundle.captures) {
    const domain = String(rawNative.domain);
    let entry = { domain, id: rawNative.id, schemaValid: false, provenanceOk: false, ready: false, mismatches: null, error: null };
    try {
      const native = checkFixture(rawNative);
      entry.schemaValid = true;
      const prov = verifyNativeCaptureProvenance(native, { captureRoot: captureDir, now, maxAgeMs: opts.maxAgeMs, expectedSourceHash, expectedPackageHash });
      entry.provenanceOk = prov.ok;
      entry.provenanceReason = prov.ok ? null : prov.reason;
      const tsCap = ts[domain];
      if (tsCap && prov.ok) {
        const gate = assertParityReady(tsCap, native);
        entry.ready = gate.ready === true;
        if (gate.ready) {
          const cmp = compareCaptures(tsCap, native);
          entry.mismatches = cmp.ready ? cmp.mismatches : null;
        }
      }
    } catch (e) {
      entry.error = e?.reason ?? e?.message ?? String(e);
    }
    domains.push(entry);
  }
  const allReady = domains.length > 0 && domains.every((d) => d.ready);
  const anyDrift = domains.some((d) => Array.isArray(d.mismatches) && d.mismatches.length > 0);
  const groundOk = ground.ok;
  const parity = !groundOk ? 'BLOCKED-RED' : (!allReady ? 'BLOCKED-RED' : (anyDrift ? 'VERIFIED-DIVERGENT' : 'VERIFIED-GREEN'));
  return { parity, groundTruth: ground, expectedSourceHash, expectedPackageHash, domains };
}

// --- execute (integration lane only; not run in this lane) -----------------

function executeEditor(opts, invocation) {
  mkdirSync(opts.captureDir, { recursive: true });
  const run = spawnSync(invocation.command, invocation.args, {
    cwd: REPO_ROOT, env: { ...process.env, ...invocation.env }, timeout: opts.timeoutMs, killSignal: 'SIGKILL', encoding: 'utf8',
  });
  const timedOut = run.error && /ETIMEDOUT|timed out/i.test(String(run.error?.message ?? '')) || run.signal === 'SIGKILL';
  return { status: run.status, signal: run.signal, timedOut: Boolean(timedOut), spawnError: run.error ? String(run.error.message) : null };
}

// --- report ----------------------------------------------------------------

function buildReport(opts) {
  const invocation = editorInvocation(opts);
  const { abs: captureAbs, owned } = assertOwnedCaptureDir(opts.captureDir);
  const base = {
    task: 38, lane: 'E', kind: 'native-capture-runner', mode: opts.mode,
    generatedAt: new Date().toISOString(), gitHead: gitHead(), node: process.version, packageVersion: '0.5.30',
    namespace: opts.namespace, captureDir: captureAbs, captureDirOwned: owned,
    port: opts.port, timeoutMs: opts.timeoutMs, maxAgeMs: opts.maxAgeMs,
    inputs: { engine: opts.engine || null, project: opts.project || null, endpoint: opts.endpoint || null, capabilityToken: Boolean(opts.token) },
    plannedInvocation: invocation,
  };

  if (opts.mode === 'plan') {
    return { ...base, status: 'PLANNED', crossTransportParity: 'BLOCKED-RED',
      completionClaim: 'REFUSED — no native execution performed; this plan launches nothing. Run --execute against a built UE editor to produce the capture.',
      limitations: [
        'This lane does NOT build the plugin or launch the editor. The invocation above is the exact command the integration lane runs.',
        'Until --execute produces a verified native-protocol capture, cross-transport parity stays BLOCKED-RED.',
      ] };
  }

  if (opts.mode === 'execute') {
    if (!opts.engine || !opts.project) {
      return { ...base, status: 'ERROR', crossTransportParity: 'BLOCKED-RED', error: '--execute requires --engine and --project (a live UE editor). Refusing to launch.' };
    }
    if (!owned) {
      return { ...base, status: 'ERROR', crossTransportParity: 'BLOCKED-RED', error: `capture dir "${captureAbs}" is outside the owned /tmp/opencode/task38- scratch; refusing to write.` };
    }
    const exec = executeEditor(opts, invocation);
    const status = exec.timedOut ? 'HUNG' : (exec.status === 0 ? 'EXECUTED' : 'ERROR');
    const verification = exec.timedOut ? null : verifyCaptureDir(captureAbs, opts);
    if (opts.cleanup) { try { rmSync(captureAbs, { recursive: true, force: true }); } catch { /* best effort */ } }
    return { ...base, status, editor: exec, crossTransportParity: verification?.parity ?? 'BLOCKED-RED', verification };
  }

  // verify
  const verification = verifyCaptureDir(captureAbs, opts);
  return { ...base, status: verification.parity === 'BLOCKED-RED' ? 'BLOCKED-RED' : 'VERIFIED', crossTransportParity: verification.parity, verification };
}

// --- CLI -------------------------------------------------------------------

function printHelp() {
  process.stdout.write(`task-38-native-capture — runner for the executable native-protocol capture seam

Usage: node scripts/qa/task-38-native-capture.mjs [--plan|--verify|--execute] [options]

  --plan (default)  print the exact editor invocation; launch nothing (BLOCKED-RED)
  --verify          verify an existing capture dir (ground-truth + provenance + parity)
  --execute         integration lane only: spawn the editor (bounded), then verify

Options: --engine= --project= --endpoint= --capture-dir= --namespace= --port=
         --timeout-ms= --max-age-ms= --capability-token= --cleanup --out= --json-only
`);
}

function main() {
  const opts = parseArgs(process.argv.slice(2));
  if (opts.help) { printHelp(); return 0; }
  const report = buildReport(opts);

  const outPath = opts.out ? resolve(REPO_ROOT, opts.out) : DEFAULT_OUT;
  const outRel = relative(REPO_ROOT, outPath);
  const confined = outRel.startsWith(join('.omo', 'evidence', 'task-38', 'harness-')) || outRel.includes('.omo/evidence/task-38/harness-');
  if (confined) {
    mkdirSync(dirname(outPath), { recursive: true });
    writeFileSync(outPath, `${JSON.stringify(report, null, 2)}\n`, 'utf8');
    report.artifact = outRel;
  } else {
    report.artifactRefused = `refused to write report outside owned evidence namespace: ${outRel}`;
  }

  if (!opts.jsonOnly) {
    process.stdout.write(`task-38-native-capture: mode=${report.mode} status=${report.status} crossTransportParity=${report.crossTransportParity}\n`);
    process.stdout.write(`  namespace=${report.namespace} captureDir=${report.captureDir} port=${report.port}\n`);
    if (report.plannedInvocation) process.stdout.write(`  invoke: ${report.plannedInvocation.command} ${report.plannedInvocation.args.join(' ')}\n`);
  }
  process.stdout.write(`${JSON.stringify(report, null, 2)}\n`);
  // Exit non-zero only on a hard runner error; BLOCKED-RED is data, not a failure.
  return report.status === 'ERROR' ? 1 : 0;
}

process.exit(main());
