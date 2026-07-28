#!/usr/bin/env node
// @ts-check
// Task 61 — compose the external-blocker evidence document.
//
// Four of the nine UE minors this product advertises are not on this host. The
// output of that fact is a BLOCKER, and the whole risk of this task is that the
// blocker degrades — into a skip in Task 62, into silence in Task 64, into a
// support matrix that quietly claims 5.0-5.8. So the records are emitted, then
// handed to a validator that tries to break them, and BOTH results are recorded.
//
// EVERYTHING HERE IS RE-DERIVED, NOT COPIED. The inventory is rebuilt from
// `Engine/Build/Build.version` on every root, the detector is really executed and
// its real exit code recorded, and every digest is computed from bytes read now.
// Install 5.1 and this document stops validating — which is the only property
// that makes it evidence rather than a paragraph.
//
// THE EXIT CODES ARE RECORDED, NEVER MASSAGED. The detector exits 3 because
// minors are missing; the unit suite exits non-zero because of two pre-existing
// failures this task did not introduce and did not fix. Both are written down as
// they happened, the way Task 55 wrote down its two nonzero exits.
//
// Run: node scripts/qa/task61-evidence.mjs [--out FILE] [--search-dir DIR]...

import { createHash } from 'node:crypto';
import { mkdirSync, statSync, writeFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { EvidenceAggregator, recordCommand } from '../../tests/unit/task-50/evidence-aggregator.mjs';
import { describeRejections, validateEvidence } from '../../tests/unit/task-50/evidence-validator.mjs';
import { INDEPENDENCE, observation } from '../../tests/unit/task-50/state-oracles.mjs';
import { buildEngineInventory, formatInventoryTable } from '../../tests/unit/task-52/engine-inventory.mjs';
import { ADVERTISED_RANGE, buildMissingMinorBlocker, describeDetection } from '../../tests/unit/task-61/external-blocker.mjs';
import { aggregateExternalBlockers, describeBlockerRejections, validateBlockerRecord } from '../../tests/unit/task-61/external-blocker-validator.mjs';
import { snapshotTree } from '../../tests/unit/task-50/evidence-validator.mjs';

/** @param {string} name @param {string} fallback @returns {string} */
const argOf = (name, fallback) => {
  const index = process.argv.indexOf(name);
  return index >= 0 ? String(process.argv[index + 1]) : fallback;
};
/** Every `--flag VALUE`, in the order given. @param {string} name @returns {string[]} */
const argsAll = (name) => process.argv.reduce((found, token, index) => (
  token === name && process.argv[index + 1] !== undefined ? [...found, String(process.argv[index + 1])] : found
), /** @type {string[]} */ ([]));

const REPO = process.cwd();
const OUT = argOf('--out', '.omo/evidence/task-61-pure-unreal-mcp-implementation.json');
const RECORDS_OUT = '.omo/evidence/task-61/blocked-external-records.json';
const DETECTOR = 'scripts/qa/task61-detect-missing-engines.mjs';
/** @param {string} line */
const log = (line) => { process.stderr.write(`${line}\n`); };

/**
 * Where an engine could plausibly live on this host. `/data` is where all six
 * roots actually are; the rest are scanned so "absent from this machine" is a
 * wider statement than "absent from the one directory we looked in". Each is one
 * level deep and costs a stat per entry.
 */
const SEARCH_DIRS = argsAll('--search-dir');
const searchDirs = SEARCH_DIRS.length > 0 ? SEARCH_DIRS : ['/data', '/opt', '/usr/local', '/home/xav', '/srv', '/mnt'];

/** The source whose identity this evidence depends on. */
const TREE = [
  'tests/unit/task-61/external-blocker.mjs',
  'tests/unit/task-61/external-blocker-validator.mjs',
  'tests/unit/task-61/external-blocker.test.ts',
  'scripts/qa/task61-detect-missing-engines.mjs',
  'scripts/qa/task61-evidence.mjs',
];

/**
 * The shell one-liner a reader can run without trusting this repository at all:
 * it enumerates every `Engine/Build/Build.version` under every search dir and
 * prints the three numbers each one contains. No 5.1, 5.2, 5.4 or 5.6 appears.
 * `find` is used rather than a glob so a search dir that does not exist costs a
 * suppressed stderr line instead of a nonzero exit that would obscure the result.
 */
const REPRODUCIBLE = `find ${searchDirs.join(' ')} -maxdepth 4 -path '*/Engine/Build/Build.version' -print0 2>/dev/null | xargs -0 grep -H -o '"\\(Major\\|Minor\\|Patch\\)Version": *[0-9]*'`;

const aggregator = new EvidenceAggregator({
  task: 61,
  title: 'Record truthful external blockers for missing UE 5.1, 5.2, 5.4 and 5.6 roots',
  plan: '.omo/plans/pure-unreal-mcp-implementation.md',
  kind: 'wave-7 blocker lane',
});
aggregator.recordTree(TREE);

// ── the detection, really executed ──────────────────────────────────────────
const detectorRun = recordCommand({ file: 'node', args: [DETECTOR, ...searchDirs.flatMap((dir) => ['--search-dir', dir])], cwd: REPO, timeoutMs: 120_000 });
aggregator.addCommand(detectorRun);
log(`detector exited ${detectorRun.exitCode}`);

const inventory = buildEngineInventory({ searchDirs });
const missing = [...inventory.missing];
log(`missing minors: ${missing.join(', ') || '(none)'}`);

const perMinorRuns = missing.map((minorKey) => {
  const run = recordCommand({
    file: 'node',
    args: [DETECTOR, ...searchDirs.flatMap((dir) => ['--search-dir', dir]), '--minor', minorKey],
    cwd: REPO, timeoutMs: 120_000,
  });
  aggregator.addCommand(run);
  return { minorKey, run };
});

const reproducibleRun = recordCommand({ file: 'sh', args: ['-c', REPRODUCIBLE], cwd: REPO, timeoutMs: 60_000 });
aggregator.addCommand(reproducibleRun);

// ── the records ─────────────────────────────────────────────────────────────
// The tag example is quoted from a tag that really exists on this host, so the
// remediation's naming convention is observed rather than assumed.
const observedTag = inventory.identities
  .map((identity) => identity.sources.gitDescribe.raw)
  .find((raw) => typeof raw === 'string' && /^\d+\.\d+\.\d+-release$/u.test(raw)) ?? null;

const detection = describeDetection({
  command: `node ${DETECTOR} ${searchDirs.map((dir) => `--search-dir ${dir}`).join(' ')}`,
  commandExitCode: Number(detectorRun.exitCode),
  reproducibleShellCommand: REPRODUCIBLE,
  searchDirs,
  detectorTree: snapshotTree({ projectRoot: REPO, files: [TREE[0], TREE[1], TREE[3]] }),
  inventory,
  detectedAt: new Date().toISOString(),
});

const records = missing.map((minorKey) => buildMissingMinorBlocker({
  minorKey,
  inventory,
  detection,
  projectRelativeDetector: DETECTOR,
  advertisedBy: [
    'plugins/McpAutomationBridge/McpAutomationBridge.uplugin: the bridge plugin describes itself as supporting UE 5.0-5.8 Preview',
    '.omo/plans/pure-unreal-mcp-implementation.md: Task 62 requires one row per UE 5.0-5.8 minor',
  ],
  observedTagExample: observedTag,
}));

const perRecordValidation = records.map((record) => ({
  recordId: record.recordId,
  minorKey: record.subject.minorKey,
  ...validateBlockerRecord(record),
}));
const aggregate = aggregateExternalBlockers({ records, detectedMissingMinors: missing });
log(describeBlockerRejections(aggregate));

// ── the forgery probe, run against the REAL records ─────────────────────────
// The unit suite proves the validator refuses forgeries built from a fixture.
// This proves it refuses forgeries built from the records this document actually
// ships — a gate nobody has watched reject the real thing is decoration. The
// unmutated record is probed too: a validator that refused everything would pass
// every rejection below while gating nothing.
const subject = records[0];
const neighbour = subject === undefined
  ? undefined
  : inventory.available.find((entry) => entry.minorKey !== subject.subject.minorKey);
if (subject === undefined || neighbour === undefined) {
  // Not a condition to skip past: the wrong-minor forgeries need a real installed
  // root of a DIFFERENT minor to point at. Without one the probe would silently
  // shrink to the checks that happen to be constructible, and a gate that quietly
  // tests less than it claims is the failure this whole task is written against.
  log('FORGERY PROBE CANNOT BE BUILT: no blocker record, or no installed minor differs from the subject');
  process.exit(1);
}
/** @param {string} label @param {string} expectedCode @param {(mutant: any) => void} mutate */
const forge = (label, expectedCode, mutate) => {
  const mutant = structuredClone(subject);
  mutate(mutant);
  const result = validateBlockerRecord(mutant);
  return {
    label,
    expectedCode,
    outcome: result.outcome,
    codes: [...new Set(result.rejections.map((entry) => entry.code))],
    refusedAsExpected: result.outcome === 'REJECTED' && result.rejections.some((entry) => entry.code === expectedCode),
  };
};

const control = validateBlockerRecord(subject);
const forgeries = [
  forge('a pass field', 'PASS_FIELD', (mutant) => { mutant.pass = true; }),
  forge('a passed verdict', 'PASS_FIELD', (mutant) => { mutant.verdict = 'PASSED'; }),
  forge('a nested ok field', 'PASS_FIELD', (mutant) => { mutant.detection.hostEngineTree.roots[0].ok = true; }),
  forge('a skip field', 'SKIP_FIELD', (mutant) => { mutant.skipped = true; }),
  forge('a synthetic detector digest', 'SYNTHETIC_HASH', (mutant) => { mutant.detection.detectorTree.sourceDigest = 'a'.repeat(64); }),
  forge('a plausible invented detector digest', 'HASH_MISMATCH', (mutant) => {
    mutant.detection.detectorTree.sourceDigest = createHash('sha256').update('a digest that looks entirely real').digest('hex');
  }),
  forge('a fabricated host engine-tree digest', 'HASH_MISMATCH', (mutant) => {
    mutant.detection.hostEngineTree.digest = createHash('sha256').update('a host that was never scanned').digest('hex');
  }),
  forge('a root filed under a minor it does not contain', 'WRONG_MINOR_PATH', (mutant) => {
    mutant.detection.hostEngineTree.roots[0].minorKey = subject.subject.minorKey;
  }),
  forge('remediation pointing at another minor\'s root', 'WRONG_MINOR_PATH', (mutant) => {
    mutant.requiredOperatorInput.engineRoot = neighbour.preferredRoot;
  }),
  forge('required Build.version content for the wrong minor', 'WRONG_MINOR_INPUT', (mutant) => {
    mutant.requiredOperatorInput.requiredFields.MinorVersion = Number(neighbour.minorKey.split('.')[1]);
  }),
  forge('a softened severity', 'NOT_BLOCKING', (mutant) => { mutant.severity = 'warning'; }),
];

const probe = {
  probedRecord: subject.recordId,
  positiveControl: { outcome: control.outcome, rejections: control.rejections, checked: control.checked },
  forgeries,
  unrefused: forgeries.filter((entry) => !entry.refusedAsExpected).map((entry) => entry.label),
  note: 'Each forgery mutates the shipped record in exactly one way. The positive control is the same record unmutated; if it were refused, every rejection here would prove only that the validator refuses everything.',
};
log(`forgery probe: ${forgeries.length - probe.unrefused.length}/${forgeries.length} refused, control ${control.outcome}`);

mkdirSync(resolve(REPO, '.omo/evidence/task-61'), { recursive: true });
writeFileSync(resolve(REPO, RECORDS_OUT), `${JSON.stringify({
  task: 61,
  generatedAt: new Date().toISOString(),
  advertisedRange: ADVERTISED_RANGE,
  status: aggregate.status,
  blockerCount: aggregate.blockerCount,
  blockedMinors: aggregate.blockedMinors,
  records,
}, null, 2)}\n`);

const newestInput = Math.max(...TREE.map((file) => statSync(resolve(REPO, file)).mtimeMs));
aggregator.recordArtifact({
  path: RECORDS_OUT,
  inputsNewest: TREE[0],
  inputsNewestAtMs: newestInput,
});

// ── observations: the same mechanism reporting BOTH polarities ──────────────
// This is the positive control the evidence contract demands. The filesystem read
// that reports 5.1 ABSENT is the identical read that reports 5.0.3 PRESENT, so a
// permanently blind oracle — which would satisfy every absence assertion in this
// document — is excluded by construction rather than by assurance.
const minorObservations = new Map();
for (const minorKey of inventory.expectedMinors) {
  const entry = inventory.available.find((candidate) => candidate.minorKey === minorKey) ?? null;
  const id = aggregator.addObservation(observation({
    kind: 'engine-minor',
    mechanism: 'fs:engine-build-version',
    independence: INDEPENDENCE.OUT_OF_BAND,
    target: `ue-minor:${minorKey}`,
    present: entry !== null,
    digest: entry === null ? null : entry.identity.sources.buildVersion.sha256,
    detail: {
      searchDirs,
      scannedRoots: inventory.scannedRoots,
      roots: entry === null ? [] : entry.roots,
      versionString: entry === null ? null : entry.versionString,
      readFrom: entry === null ? null : entry.identity.sources.buildVersion.file,
      identifiedBy: 'Engine/Build/Build.version — never the directory name',
    },
  }), { phase: entry === null ? 'post' : 'control' });
  minorObservations.set(minorKey, id);
}

for (const identity of inventory.identities) {
  aggregator.addObservation(observation({
    kind: 'engine-root',
    mechanism: 'fs:path-exists',
    independence: INDEPENDENCE.OUT_OF_BAND,
    target: identity.root,
    present: true,
    detail: { versionString: identity.versionString, hasCompiledEditor: identity.toolchain.hasCompiledEditor },
  }), { phase: 'control' });
}
for (const record of records) {
  aggregator.addObservation(observation({
    kind: 'engine-root',
    mechanism: 'fs:path-exists',
    independence: INDEPENDENCE.OUT_OF_BAND,
    target: record.requiredOperatorInput.engineRoot,
    present: false,
    detail: { requiredFor: record.subject.minorKey, requiredFile: record.requiredOperatorInput.requiredFileAbsolutePath },
  }), { phase: 'post' });
}

// ── claims: one per missing minor, none of them a pass ──────────────────────
for (const record of records) {
  const minorKey = record.subject.minorKey;
  aggregator.addClaim({
    id: `claim-blocked-${minorKey}`,
    target: `ue-minor:${minorKey}`,
    effect: 'unchanged',
    outcome: 'blocked',
    verdict: 'BLOCKED_EXTERNAL',
    // `pass: false` is the ENVELOPE's field, describing the readiness of UE
    // ${minorKey} — not the success of the detection, which did complete. The
    // blocker record itself carries no pass field of any kind; that is enforced
    // by external-blocker-validator.mjs and proven by its suite.
    pass: false,
    reason: `UE ${minorKey} is not installed on this host, so it cannot be built, launched, exercised or certified. ${record.absence.detail}`,
    oracleRefs: [minorObservations.get(minorKey)],
    cleanupRef: null,
  });
}

// ── gates, recorded with their real exit codes ──────────────────────────────
for (const gate of [
  { file: 'npx', args: ['tsc', '--noEmit'], timeoutMs: 600_000 },
  { file: 'npx', args: ['eslint', '.', '--max-warnings=0'], timeoutMs: 600_000 },
  { file: 'npx', args: ['vitest', 'run', 'tests/unit/task-61/external-blocker.test.ts'], timeoutMs: 600_000 },
  { file: 'npm', args: ['run', 'test:unit'], timeoutMs: 1_800_000 },
  { file: 'sha256sum', args: ['-c', '.omo/evidence/preservation/preserved-24-baseline.sha256'], timeoutMs: 120_000 },
]) {
  const run = recordCommand({ ...gate, cwd: REPO });
  aggregator.addCommand(run);
  log(`${run.cmd} exited ${run.exitCode}`);
}

// ── the document ────────────────────────────────────────────────────────────
aggregator.document.engine = {
  drivenEngineRoot: null,
  note: 'Task 61 launched no editor, started no build and opened no port. It is a read-only filesystem inventory, so no engine was driven and none is claimed.',
  inventoryTable: formatInventoryTable(inventory),
};

aggregator.document.environment.externalBlockers = {
  advertisedRange: ADVERTISED_RANGE,
  status: aggregate.status,
  blockerCount: aggregate.blockerCount,
  skippedCount: aggregate.skippedCount,
  passedCount: aggregate.passedCount,
  blockedMinors: aggregate.blockedMinors,
  treatment: aggregate.treatment,
  recordsFile: RECORDS_OUT,
  validation: { outcome: aggregate.outcome, rejections: aggregate.rejections, perRecord: perRecordValidation },
  forgeryProbe: probe,
  records,
};

aggregator.document.environment.engineInventory = {
  searchDirs,
  scannedRoots: inventory.scannedRoots,
  table: formatInventoryTable(inventory),
  missing,
  duplicates: inventory.duplicates,
  unusable: inventory.unusable,
  folderNameContradictions: inventory.folderNameContradictions,
  available: inventory.available.map(({ identity, ...rest }) => ({
    ...rest,
    buildVersionFile: identity.sources.buildVersion.file,
    buildVersionSha256: identity.sources.buildVersion.sha256,
    versionHeaderAgrees: identity.sources.versionHeader.agrees,
    gitDescribe: identity.sources.gitDescribe.raw,
  })),
  identifiedBy: 'Engine/Build/Build.version, corroborated by Version.h; git describe refines the channel only. A directory name is never an input.',
};

aggregator.document.environment.detection = {
  detector: DETECTOR,
  aggregateRunExitCode: detectorRun.exitCode,
  perMinorExitCodes: perMinorRuns.map(({ minorKey, run }) => ({ minorKey, exitCode: run.exitCode, cmd: run.cmd })),
  reproducibleShellCommand: REPRODUCIBLE,
  reproducibleShellExitCode: reproducibleRun.exitCode,
  exitCodeContract: '0 = every requested minor installed, buildable and runnable; 3 = at least one NOT INSTALLED; 4 = installed but not usable; 2 = usage error. A blocker record claiming exit 0 is refused.',
};

aggregator.addNote(`The advertised range ${ADVERTISED_RANGE} is NOT narrowed to match the host. Four minors are recorded as blockers precisely so the claim and the evidence stay in disagreement until an operator resolves it.`);
aggregator.addNote('Every digest here is recomputed from bytes read during this run. Installing any of the four minors invalidates this document, which is the property that makes it falsifiable.');
aggregator.addNote('The unit suite is expected to exit non-zero: the baseline is exactly two failures, both tied to the preserved tests/unit/_poc_security/security-poc.test.ts — one inside it, and one in source_structure.test.ts objecting to that same preserved file. Neither is fixed nor hidden here. The task-61 suite (75 tests) is run separately above and exits 0.');
aggregator.addNote('OBSERVED FLAKE, NOT SUPPRESSED: one full-suite run during this task reported a THIRD failure, in tests/unit/server/task-37-primitive-wiring.test.ts ("two clients with different name/version but identical capabilities resolve to the same profile"). That file is not touched by this task; it passed 16/16 in three consecutive isolated runs, and the next two full runs returned to exactly the two baseline failures. It is recorded because a flake nobody wrote down is indistinguishable from a regression nobody noticed.');

aggregator.addNotProven(`SCOPE OF ABSENCE: this proves 5.1/5.2/5.4/5.6 are absent from ${searchDirs.join(', ')} at one level of depth, which is where every engine root on this host lives. It does not prove no copy exists elsewhere on the filesystem, on another machine, or in the world.`);
aggregator.addNotProven('NO ENGINE WAS INSTALLED OR VERIFIED TO BE INSTALLABLE: this task downloads nothing. The remediation describes what an operator must supply; it has not been executed, so nothing here proves the described steps succeed.');
aggregator.addNotProven('NOT THIS TASK\'S BLOCKER: 5.0, 5.3 and 5.5 ARE installed but have no compiled UnrealEditor-Cmd, so they cannot host a certification either. That is a different refusal (NO_COMPILED_EDITOR, owned by Tasks 56-58) and is deliberately NOT recorded as a missing-minor blocker here — collapsing the two would file an install problem as an absence.');
aggregator.addNotProven('DOWNSTREAM SURVIVAL IS UNVERIFIED HERE: whether Tasks 62-64 keep these four records as blockers cannot be proven by this task. The aggregate exposes no pass or skip status for them to read, which constrains that outcome without guaranteeing it.');

const document = aggregator.finalize(aggregate.status);
const written = aggregator.write(OUT);
const validation = validateEvidence(document, { projectRoot: REPO });
log(describeRejections(validation));
writeFileSync(resolve(REPO, `${OUT.replace(/\.json$/u, '')}.verify.json`), `${JSON.stringify({
  evidence: OUT,
  validatedAt: new Date().toISOString(),
  ...validation,
}, null, 2)}\n`);

log(`wrote ${written}`);

// A probe that let a forgery through, or refused the control, means the gate this
// task ships does not gate. That is a failure of THIS task, not a note in it.
const probeHeld = probe.unrefused.length === 0 && probe.positiveControl.outcome === 'ACCEPTED';
if (!probeHeld) log(`FORGERY PROBE FAILED: unrefused ${probe.unrefused.join(', ') || '(none)'}, control ${probe.positiveControl.outcome}`);
process.exit(validation.valid && aggregate.outcome === 'ACCEPTED' && probeHeld ? 0 : 1);
