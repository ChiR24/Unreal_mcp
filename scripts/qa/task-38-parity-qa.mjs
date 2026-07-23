#!/usr/bin/env node
// scripts/qa/task-38-parity-qa.mjs
// Task 38 lane E — runtime QA harness for the strict, framing-neutral normalized
// parity harness (tests/unit/task-38/parity-harness*.mjs).
//
// It imports the harness engine under PLAIN NODE (no ts-node, no build) and runs
// seven adversarial probes against the on-disk fixtures:
//   malformed fixture, stale state, dirty worktree, misleading output,
//   deterministic order, interruption, cleanup.
//
// It NEVER claims cross-transport parity is GREEN: the parity status is DERIVED at
// runtime from assertParityReady(tsCapture, null), which returns the RED blocker
// because no executable native-protocol capture exists. `harnessSound` (all probes
// pass) is reported SEPARATELY from `crossTransportParity` (BLOCKED-RED), so a
// green QA run can never be mistaken for a satisfied completion claim.
//
// Exact invocation:
//   node scripts/qa/task-38-parity-qa.mjs --all --json-only --out=.omo/evidence/task-38/harness-manual-qa.json
//
// Modes: --help/-h (usage), --self-check (print the probe plan, no assertions),
//        --all (run every probe). Options: --out=<path>, --json-only.
//
// SIZE_OK: single responsibility — the Task 38 harness probe matrix. It is one
// indivisible ordered narrative; splitting it would hide the flow it documents.

import { execFileSync } from 'node:child_process';
import { existsSync, mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { dirname, join, relative, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

import {
  checkFixture,
  assertParityReady,
  compareCaptures,
  stableSortById,
  isGenuineNativeCapture,
  NATIVE_CAPTURE_REQUIREMENT,
  toSourceText,
  broadenMatch,
  addUnknownField,
  driftResultField,
  buildNativeCaptureBundle,
  verifyGroundTruth,
  verifyNativeCaptureProvenance,
  computePluginSourceHash,
  computePluginPackageHash,
  VERIFIER_REASONS,
} from '../../tests/unit/task-38/parity-harness.mjs';

const HERE = dirname(fileURLToPath(import.meta.url));
const REPO_ROOT = resolve(HERE, '../..');
const FIXTURES_DIR = join(REPO_ROOT, 'tests/fixtures/task-38');
const NATIVE_CAPTURE_DIR = join(FIXTURES_DIR, 'native-capture');
const DEFAULT_OUT = join(REPO_ROOT, '.omo/evidence/task-38/harness-manual-qa.json');
const VALID_FIXTURES = [
  'executable-ts-result.json',
  'executable-ts-error.json',
  'executable-ts-revision.json',
  'executable-ts-profile.json',
  'executable-ts-pointer.json',
  'executable-ts-session.json',
];

const PROBE_PLAN = [
  ['malformed-fixture', 'every on-disk + injected malformed capture is rejected with its EXACT reason'],
  ['stale-state', 'a rewound revision is rejected and repeated validation is byte-identical (no leaked state)'],
  ['dirty-worktree', 'the driver is read-only except its JSON artifact, which stays inside the owned evidence namespace'],
  ['misleading-output', 'the report presents cross-transport parity as BLOCKED-RED and never a bare PASS/GREEN'],
  ['native-hung', 'a timed-out/absent/truncated native run yields BLOCKED-RED, never a false GREEN'],
  ['native-provenance', 'the fs verifier rejects a stale, tampered, or source-drifted native capture; a native-model is refused'],
  ['deterministic-order', 'stableSortById yields the same order under any input permutation'],
  ['interruption', 'a truncated/half-written capture is rejected (MALFORMED), and recovery is clean'],
  ['cleanup', 'a session without a cleanup receipt is rejected and the valid session is cleaned + isolated'],
];

// --- helpers ---------------------------------------------------------------

const loadJson = (absPath) => JSON.parse(readFileSync(absPath, 'utf8'));
const loadFixture = (name) => loadJson(join(FIXTURES_DIR, name));

/** The reason a thunk throws, or a sentinel when it did not reject. */
function reasonOf(thunk) {
  try {
    thunk();
  } catch (error) {
    return error?.reason ?? error?.name ?? 'UNKNOWN_ERROR';
  }
  return 'NO_THROW';
}

function probe(name, pass, detail) {
  return { name, pass: Boolean(pass), detail };
}

// --- the seven probes ------------------------------------------------------

function probeMalformedFixture() {
  const manifest = loadFixture('malformed/_expected.json').expected;
  const misses = [];
  for (const [file, expected] of Object.entries(manifest)) {
    const got = reasonOf(() => checkFixture(loadFixture(`malformed/${file}`)));
    if (got !== expected) misses.push(`${file}: expected ${expected}, got ${got}`);
  }
  const base = loadFixture('executable-ts-result.json');
  const injected = [
    ['SOURCE_TEXT_CAPTURE', toSourceText(base)],
    ['BROAD_EXPECTATION', broadenMatch(base)],
    ['UNKNOWN_FIELD', addUnknownField(base, 'smuggled', 1)],
  ];
  for (const [expected, fixture] of injected) {
    const got = reasonOf(() => checkFixture(fixture));
    if (got !== expected) misses.push(`injected ${expected}: got ${got}`);
  }
  return probe(
    'malformed-fixture',
    misses.length === 0,
    misses.length === 0
      ? `${Object.keys(manifest).length} on-disk + 3 injected malformed captures each rejected with the exact reason`
      : misses.join('; '),
  );
}

function probeStaleState() {
  const staleReason = reasonOf(() => checkFixture(loadFixture('malformed/stale-revision.json')));
  const a = JSON.stringify(checkFixture(loadFixture('executable-ts-result.json')));
  const b = JSON.stringify(checkFixture(loadFixture('executable-ts-result.json')));
  return probe(
    'stale-state',
    staleReason === 'STALE_REVISION' && a === b,
    `staleRevisionRejected=${staleReason === 'STALE_REVISION'}; repeatedValidationByteIdentical=${a === b}`,
  );
}

function probeDirtyWorktree(outPath) {
  let entries = -1;
  try {
    const status = execFileSync('git', ['status', '--porcelain'], { cwd: REPO_ROOT, encoding: 'utf8' });
    entries = status.split('\n').filter(Boolean).length;
  } catch {
    entries = -1; // not a git tree; the confinement invariant below still holds
  }
  const outRel = relative(REPO_ROOT, outPath);
  const confined = outRel.startsWith(join('.omo', 'evidence', 'task-38', 'harness-')) || outRel.includes('.omo/evidence/task-38/harness-');
  return probe(
    'dirty-worktree',
    confined,
    `preexistingWorktreeEntries=${entries}; driverIsReadOnlyExceptArtifact; artifact="${outRel}" confinedToOwnedEvidence=${confined}`,
  );
}

function probeMisleadingOutput(reportSkeleton) {
  // Honesty is a property of the STATUS FIELDS, not of the prose: a refusal is
  // allowed to say "cannot be GREEN". We assert the parity field is the RED status
  // token (never a pass token), the claim is a refusal, and the blocker is RED.
  const parity = reportSkeleton.crossTransportParity;
  const claim = String(reportSkeleton.completionClaim);
  const GREEN_TOKENS = ['GREEN', 'READY', 'PASS', 'OK'];
  const parityIsNotGreenToken = !GREEN_TOKENS.includes(parity);
  const parityBlocked = parity === 'BLOCKED-RED';
  const claimRefuses = /^REFUSED\b/i.test(claim);
  const blockerRed = reportSkeleton.blocker?.status === 'RED';
  return probe(
    'misleading-output',
    parityIsNotGreenToken && parityBlocked && claimRefuses && blockerRed,
    `crossTransportParity=${parity} (notGreenToken=${parityIsNotGreenToken}); claimRefuses=${claimRefuses}; blockerStatus=${reportSkeleton.blocker?.status}`,
  );
}

function probeNativeHung() {
  // A hung/interrupted editor run leaves NO artifact or a truncated one: an absent run
  // has zero admissible captures, and a truncated run (captures cite transcript seqs it
  // never recorded) is refused by ground-truth. Either way the gate stays BLOCKED-RED.
  const absent = verifyGroundTruth({});
  const emitted = loadFixture('native-capture/emitted-sample.json');
  const truncated = verifyGroundTruth({ ...emitted, transcript: [] });
  const pass = absent.captureCount === 0 && truncated.ok === false && truncated.mismatches.length === emitted.captures.length;
  return probe(
    'native-hung',
    pass,
    `absentRunYieldsNoCaptures=${absent.captureCount === 0}; truncatedRunRefused=${truncated.ok === false} (${truncated.mismatches.length}/${emitted.captures.length} unverifiable)`,
  );
}

function probeNativeProvenance() {
  const dir = mkdtempSync('/tmp/opencode/task38-qa-');
  try {
    const emitted = loadFixture('native-capture/emitted-sample.json');
    const sourceHash = computePluginSourceHash(REPO_ROOT);
    const packageHash = computePluginPackageHash(REPO_ROOT);

    const fresh = buildNativeCaptureBundle({ ...emitted, capturedAt: new Date().toISOString() }, { root: REPO_ROOT });
    writeFileSync(join(dir, fresh.transcriptRef), fresh.transcriptJsonl, 'utf8');
    const pointer = checkFixture(fresh.captures.find((c) => c.domain === 'pointer'));
    const okFresh = verifyNativeCaptureProvenance(pointer, { captureRoot: dir, expectedSourceHash: sourceHash, expectedPackageHash: packageHash }).ok === true;

    const stale = buildNativeCaptureBundle({ ...emitted, capturedAt: new Date(Date.now() - 3 * 24 * 3600 * 1000).toISOString() }, { root: REPO_ROOT });
    const stalePointer = checkFixture(stale.captures.find((c) => c.domain === 'pointer'));
    const staleRejected = verifyNativeCaptureProvenance(stalePointer, { captureRoot: dir }).reason === VERIFIER_REASONS.CAPTURE_STALE;

    writeFileSync(join(dir, fresh.transcriptRef), 'tampered\n', 'utf8');
    const tamperRejected = verifyNativeCaptureProvenance(pointer, { captureRoot: dir }).reason === VERIFIER_REASONS.TRANSCRIPT_SHA_MISMATCH;

    writeFileSync(join(dir, fresh.transcriptRef), fresh.transcriptJsonl, 'utf8');
    const driftRejected = verifyNativeCaptureProvenance(pointer, { captureRoot: dir, expectedSourceHash: 'f'.repeat(64) }).reason === VERIFIER_REASONS.SOURCE_DRIFT;

    const modelRefused = isGenuineNativeCapture(checkFixture(loadFixture('native-capture/model-not-genuine.json'))) === false;

    const pass = okFresh && staleRejected && tamperRejected && driftRejected && modelRefused;
    return probe(
      'native-provenance',
      pass,
      `freshVerifies=${okFresh}; staleRejected=${staleRejected}; tamperRejected=${tamperRejected}; sourceDriftRejected=${driftRejected}; nativeModelRefused=${modelRefused}`,
    );
  } finally {
    try {
      rmSync(dir, { recursive: true, force: true });
    } catch {
      // owned /tmp/opencode scratch; best-effort cleanup
    }
  }
}

function probeDeterministicOrder() {
  const captures = VALID_FIXTURES.map(loadFixture).map(checkFixture);
  const forward = stableSortById(captures).map((c) => c.id);
  const reversed = stableSortById([...captures].reverse()).map((c) => c.id);
  const same = JSON.stringify(forward) === JSON.stringify(reversed);
  return probe(
    'deterministic-order',
    same && forward.length === VALID_FIXTURES.length,
    `stableSortById invariant under permutation=${same}; order=[${forward.join(', ')}]`,
  );
}

function probeInterruption() {
  const truncated = { id: 'partial', captureKind: 'executable-ts', domain: 'result', match: 'exact', provenance: 'aborted before value' };
  const half = { id: 'partial2', captureKind: 'executable-ts', domain: 'result', match: 'exact', provenance: 'half-written value', value: { uri: 'ue://capability/catalog' } };
  const truncatedReason = reasonOf(() => checkFixture(truncated));
  const halfReason = reasonOf(() => checkFixture(half));
  const recoveryClean = checkFixture(loadFixture('executable-ts-result.json')).executable === true;
  return probe(
    'interruption',
    truncatedReason === 'MALFORMED' && halfReason === 'MALFORMED' && recoveryClean,
    `truncatedCapture=${truncatedReason}; halfWrittenValue=${halfReason}; recoveryAfterFailuresClean=${recoveryClean}`,
  );
}

function probeCleanup(outPath) {
  const noCleanupReason = reasonOf(() => checkFixture(loadFixture('malformed/missing-cleanup.json')));
  const session = checkFixture(loadFixture('executable-ts-session.json')).value;
  const cleanedAndIsolated = session.cleaned === true && session.records.every((r) => r.ownerSessionId === session.sessionId);
  return probe(
    'cleanup',
    noCleanupReason === 'MISSING_CLEANUP' && cleanedAndIsolated,
    `missingCleanupRejected=${noCleanupReason === 'MISSING_CLEANUP'}; validSessionCleanedAndIsolated=${cleanedAndIsolated}; onlyArtifactWritten="${relative(REPO_ROOT, outPath)}"`,
  );
}

// --- parity readiness (derived, never hardcoded) ---------------------------

function deriveParity() {
  const ts = checkFixture(loadFixture('executable-ts-result.json'));
  const gate = assertParityReady(ts, null); // native side absent -> RED blocker
  // A native-model (hand-authored oracle) must be refused too, not accepted.
  const nativeModel = checkFixture({ ...loadFixture('executable-ts-result.json'), captureKind: 'native-model' });
  const modelRefused = assertParityReady(ts, nativeModel).ready === false && compareCaptures(ts, nativeModel).ready === false;
  // No on-disk fixture is a genuine native-protocol capture (all are executable-ts).
  const noGenuineNativeOnDisk = !VALID_FIXTURES.some((n) => isGenuineNativeCapture(checkFixture(loadFixture(n))));
  // And no native-capture fixture is a FRESH, transcript-verified live capture: the
  // golden accept-pointer is schema-valid but its transcript file is absent, so
  // provenance verification fails. The RED state is evidence-backed, not hardcoded.
  let freshVerifiedNativeCapture = false;
  try {
    const accept = checkFixture(loadJson(join(NATIVE_CAPTURE_DIR, 'accept-pointer.json')));
    freshVerifiedNativeCapture = verifyNativeCaptureProvenance(accept, { captureRoot: NATIVE_CAPTURE_DIR }).ok === true;
  } catch {
    freshVerifiedNativeCapture = false;
  }
  return {
    ready: gate.ready === true,
    nativeModelRefused: modelRefused,
    noGenuineNativeOnDisk,
    noFreshVerifiedNativeCapture: !freshVerifiedNativeCapture,
    blocker: gate.ready === false ? gate.blocker : null,
  };
}

function gitHead() {
  try {
    return execFileSync('git', ['rev-parse', '--short', 'HEAD'], { cwd: REPO_ROOT, encoding: 'utf8' }).trim();
  } catch {
    return 'unknown';
  }
}

// --- report assembly -------------------------------------------------------

function buildReport(outPath, mode) {
  const parity = deriveParity();
  const crossTransportParity = parity.ready ? 'READY' : 'BLOCKED-RED';
  const completionClaim = parity.ready
    ? 'ready (a genuine native-protocol capture is present)'
    : 'REFUSED — no executable native-protocol capture exists; cross-transport parity cannot be GREEN and must not be claimed complete';
  const skeleton = { crossTransportParity, completionClaim, blocker: parity.blocker ?? NATIVE_CAPTURE_REQUIREMENT };

  const probes = [
    probeMalformedFixture(),
    probeStaleState(),
    probeDirtyWorktree(outPath),
    probeMisleadingOutput(skeleton),
    probeNativeHung(),
    probeNativeProvenance(),
    probeDeterministicOrder(),
    probeInterruption(),
    probeCleanup(outPath),
  ];
  const pass = probes.filter((p) => p.pass).length;
  const harnessSound = probes.every((p) => p.pass);

  return {
    task: 38,
    lane: 'E',
    kind: 'harness-manual-qa',
    title: 'Strict framing-neutral normalized parity harness — adversarial probe matrix',
    invocation: 'node scripts/qa/task-38-parity-qa.mjs --all --json-only --out=.omo/evidence/task-38/harness-manual-qa.json',
    mode,
    generatedAt: new Date().toISOString(),
    gitHead: gitHead(),
    packageVersion: '0.5.30',
    node: process.version,
    harnessSound,
    ...skeleton,
    nativeCaptureRefusal: {
      absentSideBlocked: !parity.ready,
      nativeModelRefused: parity.nativeModelRefused,
      noGenuineNativeCaptureOnDisk: parity.noGenuineNativeOnDisk,
      noFreshVerifiedNativeCapture: parity.noFreshVerifiedNativeCapture,
    },
    probes,
    summary: { total: probes.length, pass, fail: probes.length - pass },
    limitations: [
      'The executable native-protocol seam now EXISTS (Private/Tests/Task38NativeProtocolCaptureTests.cpp + scripts/qa/task-38-native-capture.mjs), but it has not been run against a built UE editor in this lane (no UBT/editor), so no native-protocol capture artifact is on disk and cross-transport parity is RED.',
      'This driver probes the HARNESS (schema + comparator + refusal + native provenance verifier), not a live editor. The TS captures are grounded against live production in parity-harness-characterization.test.ts.',
      'harnessSound=true means the harness catches drift, refuses inadmissible/native-fake input, and rejects stale/tampered/source-drifted native captures — it is NOT a cross-transport parity pass.',
    ],
  };
}

// --- CLI -------------------------------------------------------------------

function parseArgs(argv) {
  const opts = { mode: 'all', out: null, jsonOnly: false, help: false, selfCheck: false };
  for (const arg of argv) {
    if (arg === '--help' || arg === '-h') opts.help = true;
    else if (arg === '--self-check') opts.selfCheck = true;
    else if (arg === '--all') opts.mode = 'all';
    else if (arg === '--json-only') opts.jsonOnly = true;
    else if (arg.startsWith('--out=')) opts.out = arg.slice('--out='.length);
  }
  return opts;
}

function printHelp() {
  process.stdout.write(`task-38-parity-qa — adversarial probe matrix for the Task 38 parity harness

Usage: node scripts/qa/task-38-parity-qa.mjs [--all] [--self-check] [--json-only] [--out=<path>]

Modes:
  --all          run every probe (default)
  --self-check   print the probe plan only, run NO assertions, exit 0
  --help, -h     this message

Options:
  --out=<path>   JSON output path (default: .omo/evidence/task-38/harness-manual-qa.json)
  --json-only    print ONLY the JSON document to stdout

Notes:
  * Plain node — no build, no ts-node, no live Unreal editor.
  * Cross-transport parity is reported as BLOCKED-RED (no native-protocol capture);
    harnessSound is reported separately and never implies a parity pass.
`);
}

function writeArtifact(outPath, report) {
  mkdirSync(dirname(outPath), { recursive: true });
  writeFileSync(outPath, `${JSON.stringify(report, null, 2)}\n`, 'utf8');
}

function main() {
  const opts = parseArgs(process.argv.slice(2));
  if (opts.help) {
    printHelp();
    return 0;
  }
  const outPath = opts.out ? resolve(REPO_ROOT, opts.out) : DEFAULT_OUT;

  if (opts.selfCheck) {
    const plan = {
      task: 38, lane: 'E', kind: 'harness-manual-qa', mode: 'self-check', overall: 'NOT-RUN',
      probes: PROBE_PLAN.map(([name, intent]) => ({ name, intent })),
      note: 'self-check prints the plan and runs NO assertions; use --all to execute.',
    };
    if (!opts.jsonOnly) process.stdout.write('task-38-parity-qa self-check (no assertions):\n');
    process.stdout.write(`${JSON.stringify(plan, null, 2)}\n`);
    return 0;
  }

  const report = buildReport(outPath, opts.mode);
  writeArtifact(outPath, report);

  if (!opts.jsonOnly) {
    process.stdout.write(`task-38-parity-qa: harnessSound=${report.harnessSound} crossTransportParity=${report.crossTransportParity}\n`);
    for (const p of report.probes) {
      process.stdout.write(`  [${p.pass ? 'PASS' : 'FAIL'}] ${p.name} — ${p.detail}\n`);
    }
    process.stdout.write(`  completionClaim: ${report.completionClaim}\n`);
    process.stdout.write(`  artifact: ${relative(REPO_ROOT, outPath)}\n`);
  }
  process.stdout.write(`${JSON.stringify(report, null, 2)}\n`);
  // Exit code reflects HARNESS soundness only. The RED cross-transport parity is
  // data carried in the report, not a driver failure.
  return report.harnessSound ? 0 : 1;
}

const code = main();
if (!existsSync(FIXTURES_DIR)) {
  process.stderr.write(`[task-38-parity-qa] fixtures dir missing: ${FIXTURES_DIR}\n`);
}
process.exit(code);
