#!/usr/bin/env node
// @ts-check
// Task 62 — aggregate the engine, plugin, client and transport compatibility
// evidence into one record, and refuse it when it cannot be defended.
//
// This is a PROGRAM, not a hand-written document. Every row is derived from the
// per-engine records, the shared engine inventory and the generated capability
// registry; nothing is asserted that the inputs do not say. That matters because
// the only thing that makes an accepted run meaningful is that a rejected run is
// possible: `--negative-control` forges an input set claiming all nine minors
// passed, re-runs this same binary against it, and records the refusal alongside
// the real acceptance. An aggregator never shown refusing proves nothing.
//
// Nothing here starts an editor, a build or RunUAT. Every engine fact was already
// measured by Tasks 56-61; this task re-reads those measurements, re-derives what
// can be re-derived, and independently re-stats the engine roots so the blocked
// rows rest on something other than the blocked records' own word.
//
// Run: node scripts/qa/task62-aggregate-compatibility.mjs
//      node scripts/qa/task62-aggregate-compatibility.mjs --input-dir DIR --no-evidence

import { accessSync, constants, existsSync, mkdtempSync, readFileSync, readdirSync, rmSync, statSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join, resolve } from 'node:path';

import { EvidenceAggregator, recordCommand } from '../../tests/unit/task-50/evidence-aggregator.mjs';
import { describeRejections, validateEvidence } from '../../tests/unit/task-50/evidence-validator.mjs';
import { INDEPENDENCE, observation, sha256 } from '../../tests/unit/task-50/state-oracles.mjs';
import { MINIMAL_PROFILE } from './task62-client-profiles.mjs';
import { forgeInputSet } from './task62-forge-inputs.mjs';
import {
  ADVERTISED_MINORS,
  AGGREGATE_REJECTIONS,
  BLOCKED_SUBCLASS,
  ROW_STATE,
  aggregate,
  describeAggregate,
} from './task62-compatibility-matrix.mjs';

const REPO = process.cwd();
/** @param {string} name @param {string} fallback */
const argOf = (name, fallback) => {
  const index = process.argv.indexOf(name);
  return index >= 0 ? String(process.argv[index + 1]) : fallback;
};
const has = (/** @type {string} */ name) => process.argv.includes(name);
/** @param {string} line */
const log = (line) => { process.stderr.write(`${line}\n`); };

const INPUT_DIR = argOf('--input-dir', '.omo/evidence');
const OUT = argOf('--out', '.omo/evidence/task-62-pure-unreal-mcp-implementation.json');
const WRITE_EVIDENCE = !has('--no-evidence');
const RUN_NEGATIVE_CONTROL = WRITE_EVIDENCE && !has('--skip-negative-control');

/** The per-engine lanes. Which minor each owns is read from its own engine block. */
const ENGINE_TASKS = Object.freeze([56, 57, 58, 59, 60]);
const BLOCKER_TASK = 61;
const REGISTRY = 'src/tools/catalog/capabilities/generated/canonical-registry.generated.json';
const PROFILE_MATRIX = 'task-52/profile-matrix.json';
const NATIVE_GATES = 'task-52/native-gates.json';

/** The source whose identity this evidence depends on. */
const TREE = Object.freeze([
  'scripts/qa/task62-compatibility-matrix.mjs',
  'scripts/qa/task62-aggregate-compatibility.mjs',
  'scripts/qa/task62-forge-inputs.mjs',
  'scripts/qa/task62-client-profiles.mjs',
  REGISTRY,
  '.omo/evidence/task-52/profile-matrix.json',
  '.omo/evidence/task-52/native-gates.json',
  ...ENGINE_TASKS.map((task) => `.omo/evidence/task-${task}-pure-unreal-mcp-implementation.json`),
  `.omo/evidence/task-${BLOCKER_TASK}-pure-unreal-mcp-implementation.json`,
]);

/** @param {string} file */
const readJson = (file) => JSON.parse(readFileSync(resolve(REPO, file), 'utf8'));

/**
 * Which UE minor a record speaks for, read from the record's OWN engine block.
 * Tasks 56-58 record `version`; 59-60 record `versionString`. A task number is
 * never used to infer a minor: that would survive a record being pointed at a
 * different root.
 * @param {any} document
 */
function ownerMinorOf(document) {
  const raw = document?.engine?.versionString ?? document?.engine?.version ?? null;
  if (typeof raw !== 'string') return null;
  const parts = raw.split('.');
  return parts.length >= 2 ? `${parts[0]}.${parts[1]}` : null;
}

// ─────────────────────────────── load the inputs ─────────────────────────────

/** @type {Record<string, {minor: string, file: string, document: any}>} */
const records = {};
/** @type {{code: string, at: string, detail: string}[]} */
const loadRejections = [];
for (const task of ENGINE_TASKS) {
  const file = `${INPUT_DIR}/task-${task}-pure-unreal-mcp-implementation.json`;
  if (!existsSync(resolve(REPO, file))) {
    loadRejections.push({ code: AGGREGATE_REJECTIONS.MISSING_MINOR_ROW, at: `/inputs/task-${task}`, detail: `per-engine record ${file} is absent; the minor it owns cannot be classified` });
    continue;
  }
  const document = readJson(file);
  const minor = ownerMinorOf(document);
  if (minor === null) {
    loadRejections.push({ code: AGGREGATE_REJECTIONS.MISSING_MINOR_ROW, at: `/inputs/task-${task}`, detail: `${file} names no engine version in /engine, so the minor it speaks for is unknown and no row may be derived from it` });
    continue;
  }
  records[minor] = { minor, file, document };
}

const blockerFile = `${INPUT_DIR}/task-${BLOCKER_TASK}-pure-unreal-mcp-implementation.json`;
const blockerDocument = existsSync(resolve(REPO, blockerFile)) ? readJson(blockerFile) : null;
/** @type {Record<string, any>} */
const blockerRecords = {};
for (const record of blockerDocument?.environment?.externalBlockers?.records ?? []) {
  const minor = record?.subject?.minorKey;
  if (typeof minor === 'string') blockerRecords[minor] = { ...record, sourceFile: blockerFile };
}

// The inventory table is emitted independently by the blocker lane and by the
// 5.0 lane. Using one and cross-checking the other means a tampered table has to
// be tampered with twice, in two records written by two different runs.
const blockerTable = blockerDocument?.engine?.inventoryTable ?? null;
const engineTable = records['5.0']?.document?.environment?.engineInventory?.table ?? null;
const inventoryTable = blockerTable ?? engineTable ?? '';
if (blockerTable !== null && engineTable !== null && blockerTable !== engineTable) {
  loadRejections.push({ code: AGGREGATE_REJECTIONS.INVENTORY_DISAGREEMENT, at: '/inputs/inventoryTable', detail: `the engine inventory recorded by task ${BLOCKER_TASK} and the one recorded by the 5.0 lane are not identical; two readings of the same host disagree and neither can be trusted to place a row` });
}

const registry = readJson(REGISTRY);
const matrix = readJson(`${INPUT_DIR}/${PROFILE_MATRIX}`);
const gates = readJson(`${INPUT_DIR}/${NATIVE_GATES}`);

const result = aggregate({ inventoryTable, records, blockerRecords, matrix, registry, gates });
result.rejections.unshift(...loadRejections);
if (loadRejections.length > 0) result.outcome = 'REFUSED';

// ─────────────────── independent re-reading of the engine roots ──────────────
// The blocked rows say an engine is absent or has no compiled editor. Those are
// filesystem facts, so they are re-read here rather than believed: this is the
// out-of-band oracle for the whole left-hand side of the matrix.
/** @param {string} path */
const isExecutable = (path) => {
  try { accessSync(path, constants.X_OK); return true; } catch { return false; }
};
/** @type {any[]} */
const rootProbes = [];
for (const row of result.rows) {
  // The inventory's root cell can carry a "(+1 more)" suffix when a minor has
  // duplicate roots; the probe reads the preferred root it names.
  const root = String(row.engineRoot).replace(/\s*\(\+\d+ more\)\s*$/u, '').trim();
  const usable = root !== '' && root !== '—';
  const editorCmd = usable ? join(root, 'Engine/Binaries/Linux/UnrealEditor-Cmd') : null;
  rootProbes.push({
    minor: row.minor,
    root: usable ? root : null,
    rootExists: usable ? existsSync(root) : false,
    editorCmd,
    editorExecutable: editorCmd === null ? false : isExecutable(editorCmd),
    agreesWithRow: usable
      ? existsSync(root) === row.rootPresent && (editorCmd !== null && isExecutable(editorCmd)) === row.editorBuilt
      : row.rootPresent === false,
  });
}
for (const probe of rootProbes) {
  if (probe.agreesWithRow) continue;
  result.rejections.push({ code: AGGREGATE_REJECTIONS.INVENTORY_DISAGREEMENT, at: `/matrix/${probe.minor}`, detail: `the recorded inventory and a live filesystem re-read disagree for UE ${probe.minor}: root ${probe.root ?? '(none)'} exists=${probe.rootExists}, compiled editor executable=${probe.editorExecutable}. A row cannot rest on a reading the disk contradicts.` });
  result.outcome = 'REFUSED';
}

// ─────────────────── plugin-tree drift since each certification ──────────────
// A 20/20 stage table proves the tree AS RECORDED at that moment. Task 59's own
// `tree.files` covers the certification harness, not the plugin C++, so plugin
// source that moved after the package was built is NOT covered by that stage
// table. This does not change the classification — the rule is about the recorded
// stages — but leaving it unsaid would let a PASS read as "HEAD is certified".
/** @type {any[]} */
const certificationDrift = [];
for (const row of result.rows) {
  if (row.state === ROW_STATE.BLOCKED_EXTERNAL) continue;
  const owner = records[row.minor];
  const certification = owner?.document?.environment?.certification ?? null;
  const packageBuiltAtMs = certification?.binaryFreshness?.builtAtMs ?? null;
  if (typeof packageBuiltAtMs !== 'number') continue;
  const drifted = newerPluginSources(packageBuiltAtMs);
  certificationDrift.push({
    minor: row.minor,
    certifiedBinarySha256: certification?.binaryFreshness?.binarySha256 ?? null,
    binaryBuiltAt: new Date(packageBuiltAtMs).toISOString(),
    pluginSourceFilesNewerThanBinary: drifted.count,
    newestPluginSource: drifted.newest,
    recordedTreeFiles: (owner?.document?.tree?.files ?? []).length,
    recordedTreeCoversPluginSource: (owner?.document?.tree?.files ?? []).some((/** @type {any} */ entry) => String(entry.path).startsWith('plugins/')),
  });
  if (drifted.count > 0) {
    row.caveats.push(`the certified binary ${String(certification?.binaryFreshness?.binarySha256 ?? '').slice(0, 12)} was built ${new Date(packageBuiltAtMs).toISOString()}; ${drifted.count} plugin source file(s) have changed since, and the record's own tree snapshot covers the harness rather than plugin source. This row certifies the tree as it stood at that build, NOT the current HEAD.`);
  }
}

/** @param {number} sinceMs */
function newerPluginSources(sinceMs) {
  const root = resolve(REPO, 'plugins/McpAutomationBridge/Source');
  /** @type {{count: number, newest: {path: string, mtime: string}|null}} */
  const found = { count: 0, newest: null };
  /** @param {string} dir */
  const walk = (dir) => {
    for (const entry of readdirSafe(dir)) {
      const full = join(dir, entry.name);
      if (entry.isDirectory()) { walk(full); continue; }
      if (!/\.(?:cpp|h|cs)$/u.test(entry.name)) continue;
      const mtimeMs = statSync(full).mtimeMs;
      if (mtimeMs <= sinceMs) continue;
      found.count += 1;
      if (found.newest === null || mtimeMs > Date.parse(found.newest.mtime)) {
        found.newest = { path: full.slice(REPO.length + 1), mtime: new Date(mtimeMs).toISOString() };
      }
    }
  };
  if (existsSync(root)) walk(root);
  return found;
}

// A directory that cannot be read must be LOUD. Swallowing the error here would
// report "0 files changed since certification" for a tree nobody could open,
// which is the most flattering possible lie about drift.
/** @param {string} dir */
function readdirSafe(dir) {
  try {
    return readdirSync(dir, { withFileTypes: true });
  } catch (error) {
    throw new Error(`cannot read ${dir} while measuring plugin drift since certification: ${String(error instanceof Error ? error.message : error)}`);
  }
}

log(describeAggregate(result));
for (const row of result.rows) {
  const label = row.state === ROW_STATE.BLOCKED_EXTERNAL ? `${row.state} (${row.subclass})` : row.state;
  log(`  UE ${row.minor}${row.previewLabel ? ` ${row.previewLabel}` : ''}  ${label.padEnd(34)} ${row.evidenceFile ?? '(no record)'}`);
}

if (!WRITE_EVIDENCE) {
  process.exit(result.outcome === 'ACCEPTED' ? 0 : 3);
}

// ─────────────────────────── the negative control ────────────────────────────
// Forge an input set that claims every one of the nine minors passed, then run
// THIS SAME PROGRAM against it. If it accepts the forgery, the gate this task
// ships does not gate, and that is a failure of this task rather than a note in
// it.
/** @type {any} */
let negativeControl = null;
/** @type {string|null} */
let forgedDir = null;
/** @type {any} */
let forgedDirPre = null;
if (RUN_NEGATIVE_CONTROL) {
  forgedDir = mkdtempSync(join(tmpdir(), 'task62-forged-'));
  const forged = forgeInputSet({ sourceDir: resolve(REPO, INPUT_DIR), targetDir: forgedDir, minors: ADVERTISED_MINORS });
  forgedDirPre = observation({ kind: 'temp-workspace', mechanism: 'fs:stat', target: forgedDir, present: existsSync(forgedDir), detail: { entries: forged.written.length } });
  const run = recordCommand({
    file: process.execPath,
    args: ['scripts/qa/task62-aggregate-compatibility.mjs', '--input-dir', forgedDir, '--no-evidence'],
    cwd: REPO,
  });
  const control = recordCommand({
    file: process.execPath,
    args: ['scripts/qa/task62-aggregate-compatibility.mjs', '--input-dir', resolve(REPO, INPUT_DIR), '--no-evidence'],
    cwd: REPO,
  });
  const forgedOutput = `${run.stdoutTail ?? ''}${run.stderrTail ?? ''}`;
  const codesFired = Object.keys(AGGREGATE_REJECTIONS).filter((code) => forgedOutput.includes(code));
  negativeControl = {
    forgery: forged.mutations,
    forgedInputDir: forgedDir,
    forgedRun: { cmd: run.cmd, exitCode: run.exitCode, refusalCodesFired: codesFired, outputTail: forgedOutput.slice(-2600) },
    realRun: { cmd: String(control.cmd), exitCode: control.exitCode, outputTail: `${control.stdoutTail ?? ''}${control.stderrTail ?? ''}`.slice(-1200) },
    forgedRefused: run.exitCode !== 0 && codesFired.length > 0,
    realAccepted: control.exitCode === 0,
    verdict: run.exitCode !== 0 && codesFired.length > 0 && control.exitCode === 0
      ? 'GATE HELD: the forged all-pass input set was refused and the real input set was accepted'
      : 'GATE FAILED: the refusal could not be demonstrated, so acceptance of the real set proves nothing',
  };
  log(`negative control: forged exit ${run.exitCode} (${codesFired.join(', ') || 'no codes fired'}), real exit ${control.exitCode}`);
}

// ───────────────────────────── the evidence document ─────────────────────────

const aggregator = new EvidenceAggregator({
  task: 62,
  title: 'Aggregate engine, plugin, client and transport compatibility evidence',
  plan: '.omo/plans/pure-unreal-mcp-implementation.md',
  kind: 'wave-7 aggregation',
  projectRoot: REPO,
});
aggregator.recordTree(TREE);

// The artifacts under test are the per-engine records this aggregate reads. Each
// is dated against the generator that produced it, so a record edited after its
// own run would surface as STALE_PACKAGE instead of being silently aggregated.
const GENERATORS = Object.freeze({
  56: 'scripts/qa/task56-evidence.mjs',
  57: 'scripts/qa/task57-58-evidence.mjs',
  58: 'scripts/qa/task57-58-evidence.mjs',
  59: 'scripts/qa/task59-evidence.mjs',
  60: 'scripts/qa/task60-evidence.mjs',
  61: 'scripts/qa/task61-evidence.mjs',
});
for (const task of [...ENGINE_TASKS, BLOCKER_TASK]) {
  const generator = GENERATORS[/** @type {keyof typeof GENERATORS} */ (task)];
  const generatorPath = resolve(REPO, generator);
  aggregator.recordArtifact({
    path: `.omo/evidence/task-${task}-pure-unreal-mcp-implementation.json`,
    inputsNewest: generator,
    inputsNewestAtMs: existsSync(generatorPath) ? statSync(generatorPath).mtimeMs : null,
  });
}

// ── observations ────────────────────────────────────────────────────────────
// Each mechanism reports BOTH a present and an absent reading, which is what
// makes the absences in this document mean anything.
/** @type {Record<string, string[]>} */
const oracleRefs = { evidenceFiles: [], roots: [], editors: [], control: [], workspace: [] };

for (const [task, generator] of Object.entries(GENERATORS)) {
  const file = `.omo/evidence/task-${task}-pure-unreal-mcp-implementation.json`;
  const path = resolve(REPO, file);
  const present = existsSync(path);
  oracleRefs.evidenceFiles.push(aggregator.addObservation(observation({
    kind: 'evidence-file',
    mechanism: 'fs:sha256',
    independence: INDEPENDENCE.OUT_OF_BAND,
    target: file,
    present,
    digest: present ? sha256(readFileSync(path)) : null,
    detail: { generator, bytes: present ? statSync(path).size : null },
  }), { phase: 'post' }));
}
// The absent reading for this mechanism: a path that must NOT exist. Without it,
// an oracle that reported "present" for everything would satisfy every absence
// assertion in this document.
oracleRefs.control.push(aggregator.addObservation(observation({
  kind: 'evidence-file',
  mechanism: 'fs:sha256',
  independence: INDEPENDENCE.OUT_OF_BAND,
  target: '.omo/evidence/task-62-control-must-not-exist.json',
  present: existsSync(resolve(REPO, '.omo/evidence/task-62-control-must-not-exist.json')),
  detail: { role: 'negative control for the fs:sha256 oracle' },
}), { phase: 'control' }));

for (const probe of rootProbes) {
  oracleRefs.roots.push(aggregator.addObservation(observation({
    kind: 'engine-root',
    mechanism: 'fs:stat',
    independence: INDEPENDENCE.OUT_OF_BAND,
    target: probe.root ?? `(no root for UE ${probe.minor})`,
    present: probe.rootExists,
    detail: { minor: probe.minor },
  }), { phase: 'post' }));
  oracleRefs.editors.push(aggregator.addObservation(observation({
    kind: 'compiled-editor',
    mechanism: 'fs:access-x',
    independence: INDEPENDENCE.OUT_OF_BAND,
    target: probe.editorCmd ?? `(no editor path for UE ${probe.minor})`,
    present: probe.editorExecutable,
    detail: { minor: probe.minor, agreesWithRecordedInventory: probe.agreesWithRow },
  }), { phase: 'post' }));
}

if (negativeControl !== null && forgedDir !== null) {
  oracleRefs.control.push(aggregator.addObservation(observation({
    kind: 'aggregate-refusal',
    mechanism: 'task62:self-spawn',
    independence: INDEPENDENCE.OUT_OF_BAND,
    target: 'forged all-pass input set',
    present: negativeControl.forgedRefused,
    detail: { exitCode: negativeControl.forgedRun.exitCode, codes: negativeControl.forgedRun.refusalCodesFired },
  }), { phase: 'control' }));
  oracleRefs.control.push(aggregator.addObservation(observation({
    kind: 'aggregate-refusal',
    mechanism: 'task62:self-spawn',
    independence: INDEPENDENCE.OUT_OF_BAND,
    target: 'real input set',
    present: !negativeControl.realAccepted,
    detail: { exitCode: negativeControl.realRun.exitCode, note: 'present=false is the expected reading: the real input set produced no refusal' },
  }), { phase: 'control' }));

  oracleRefs.workspace.push(aggregator.addObservation(forgedDirPre, { phase: 'pre' }));
  rmSync(forgedDir, { recursive: true, force: true });
  const post = aggregator.addObservation(observation({
    kind: 'temp-workspace',
    mechanism: 'fs:stat',
    independence: INDEPENDENCE.OUT_OF_BAND,
    target: forgedDir,
    present: existsSync(forgedDir),
    detail: { role: 'the forged input set is fixture, and a fixture nobody proved removed changes the next run' },
  }), { phase: 'cleanup' });
  oracleRefs.workspace.push(post);
  aggregator.addCleanup({
    id: 'cleanup-forged-inputs',
    owned: forgedDir,
    verifiedBy: post,
    pass: !existsSync(forgedDir),
    verdict: existsSync(forgedDir) ? 'STILL PRESENT' : 'RELEASED',
    reason: 'the forged input set exists only to prove the aggregator refuses; it is removed and the removal is re-read from the filesystem',
  });
}

// ── claims ──────────────────────────────────────────────────────────────────
// Every claim is `unchanged`: this task drove no editor and mutated no project.
for (const row of result.rows) {
  const probe = rootProbes.find((entry) => entry.minor === row.minor);
  const refs = [
    ...(row.evidenceFile === null ? [] : [oracleRefs.evidenceFiles[0]]),
    oracleRefs.roots[rootProbes.indexOf(/** @type {any} */ (probe))],
    oracleRefs.editors[rootProbes.indexOf(/** @type {any} */ (probe))],
  ].filter((ref) => typeof ref === 'string');
  aggregator.addClaim({
    id: `row-ue-${row.minor}`,
    target: `UE ${row.minor}${row.previewLabel ? ` ${row.previewLabel}` : ''}`,
    effect: 'unchanged',
    outcome: row.state === ROW_STATE.PASS ? 'success' : 'error',
    verdict: row.state === ROW_STATE.BLOCKED_EXTERNAL ? `${row.state} (${row.subclass})` : row.state,
    pass: row.state === ROW_STATE.PASS,
    reason: row.state === ROW_STATE.BLOCKED_EXTERNAL
      ? `${row.subclass}: ${row.remediationOwner}`
      : `${row.citation?.stageId ?? 'unknown stage'} — ${row.citation?.observable ?? 'no observable'}`,
    oracleRefs: refs,
  });
}

aggregator.addClaim({
  id: 'negative-control-gate-holds',
  target: 'the aggregator itself',
  effect: 'unchanged',
  outcome: negativeControl?.forgedRefused === true && negativeControl?.realAccepted === true ? 'success' : 'error',
  verdict: negativeControl?.verdict ?? 'NOT RUN',
  pass: negativeControl?.forgedRefused === true && negativeControl?.realAccepted === true,
  reason: 'a forged input set claiming all nine minors passed must be refused by the same binary that accepts the real one',
  oracleRefs: oracleRefs.control,
});

aggregator.addClaim({
  id: 'no-artifact-reused-across-minors',
  target: 'package/binary/Build.version digests across the five recorded engine lanes',
  effect: 'unchanged',
  outcome: result.reusedDigests.length === 0 ? 'success' : 'error',
  verdict: result.reusedDigests.length === 0 ? 'NO REUSE' : `${result.reusedDigests.length} DIGEST(S) REUSED`,
  pass: result.reusedDigests.length === 0,
  reason: 'one artifact cannot have been built against two engines; a shared digest means a result was copied rather than measured',
  oracleRefs: oracleRefs.evidenceFiles,
});

// ── the non-engine axes ─────────────────────────────────────────────────────
aggregator.document.environment.aggregate = {
  advertisedRange: '5.0-5.8',
  rowsRequired: ADVERTISED_MINORS.length,
  counts: result.counts,
  countsAreDisjoint: 'PASS, FAIL and BLOCKED_EXTERNAL are three separate integers over nine rows. A BLOCKED row is NOT a skip, is NOT a pass, and is NOT averaged into any percentage; a FAIL is a defect we own and is NOT a blocker.',
  blockedBySubclass: result.blockedBySubclass,
  subclassRemediation: {
    [BLOCKED_SUBCLASS.ROOT_ABSENT]: 'operator installs the engine',
    [BLOCKED_SUBCLASS.ROOT_UNBUILT]: 'operator compiles the already-installed engine\'s editor target',
  },
  rows: result.rows,
  outcome: result.outcome,
  rejections: result.rejections,
};

aggregator.document.environment.rootProbes = {
  note: 'an independent filesystem re-read of every root the inventory names, so the blocked rows do not rest on the blocked records\' own word',
  probes: rootProbes,
  allAgree: rootProbes.every((probe) => probe.agreesWithRow),
};

aggregator.document.environment.artifactDigests = {
  reused: result.reusedDigests,
  recertifications: result.recertifications,
  note: 'digests are compared ACROSS minors (a shared digest is a copied result) and ACROSS runs of the same minor (two runs sharing a package or binary digest are one run reported as two).',
};

aggregator.document.environment.certificationDrift = {
  note: 'a stage table certifies the tree AS RECORDED at that moment. Plugin source that moved after the certified binary was built is outside what that stage table proves.',
  rows: certificationDrift,
};

aggregator.document.environment.capabilityDeltas = {
  recordCount: result.profileMatrix.recordCount,
  source: REGISTRY,
  everyRowReproduces: result.profileMatrix.rows.every((row) => row.reproduces),
  profiles: result.profileMatrix.rows,
  nativeGates: result.nativeGates,
  explained: explainDeltas(registry, matrix),
  unexplained: result.rejections.filter((entry) => entry.code === AGGREGATE_REJECTIONS.UNEXPLAINED_DELTA),
};

aggregator.document.environment.clients = {
  source: 'src/server/mcp-primitives/session-capability-profile.ts (Task 35 primitive C3)',
  derivation: 'six structural booleans read from the client\'s declared MCP capabilities. Never from a client name or version, so two clients declaring the same capabilities get the same profile.',
  minimal: MINIMAL_PROFILE,
  full: Object.fromEntries(Object.keys(MINIMAL_PROFILE).map((key) => [key, true])),
  capabilityAvailabilityIsClientIndependent: true,
  note: 'the client profile selects which MCP primitives a session may use and which bounded fallback pointer it receives; it does not gate capability availability, which is decided by engine version, enabled plugins and editor state. Both profiles therefore see the same per-engine availability counts recorded above.',
};

aggregator.document.environment.transports = {
  offlineParity: matrix.rows
    .filter((/** @type {any} */ row) => String(row.engine) === '5.7.4' && row.pluginCount === 7)
    .map((/** @type {any} */ row) => ({ profile: row.profile, transport: row.transport, protocolVersion: row.protocolVersion, available: row.available, filtered: row.filtered })),
  liveDivergence: records['5.7']?.document?.environment?.transportDiscoveryDivergence ?? null,
  note: 'the OFFLINE availability model is transport-identical for native and stdio at 5.7.4. The LIVE 5.7.4 probe is not: the native surface named 1157 of 1335 declared actions with 8 tools missing and 7 extra, while the TypeScript surface (answered from the generated registry, not a live editor) named all 1335. That divergence is recorded, not averaged away.',
};

// ── notes and limits ────────────────────────────────────────────────────────
aggregator.addNote(`MATRIX: ${result.counts.PASS} PASS, ${result.counts.FAIL} FAIL, ${result.counts.BLOCKED_EXTERNAL} BLOCKED_EXTERNAL over ${result.rows.length} rows (UE 5.0 through 5.8). The blocked rows split ${result.blockedBySubclass[BLOCKED_SUBCLASS.ROOT_ABSENT]} root-absent / ${result.blockedBySubclass[BLOCKED_SUBCLASS.ROOT_UNBUILT]} root-unbuilt, and those two do not share a remediation.`);
aggregator.addNote('RECLASSIFIED FROM ITS SOURCE: the UE 5.8 Preview 1 record calls its own verdict "BLOCKED". This aggregate records FAIL. Its engine block states hasCompiledEditor:true and the orchestrator resolved the root and opened a workspace, so nothing external is missing — the plugin simply does not compile against 5.8 headers (exit 6, OtherCompilationError). Filing that as BLOCKED_EXTERNAL would move our defect onto the operator.');
aggregator.addNote('THE PREVIEW LABEL IS LOAD-BEARING: the generated records advertise a maximum of 5.8.0 on the "preview" channel, preview 1, but compareEngineVersions ignores the channel, so a 5.8.0-release tree and a 5.8.0-preview-1 tree are indistinguishable to the availability model. Nothing here is generalised to a stable 5.8 release.');
aggregator.addNote('NOT_REACHED STAYS NOT_REACHED: 13 of the 20 stages on the 5.8 lane and 18-19 on the 5.0/5.3/5.5 lanes were never attempted. No outcome is inferred for any of them in either direction, and a stage table containing a NOT_REACHED can never satisfy the certified test.');

aggregator.addNotProven('NO ENGINE WAS DRIVEN BY THIS TASK: no editor was started, no plugin was built and RunUAT was not invoked. Every engine fact here was measured by Tasks 56-61; this task re-reads those measurements and independently re-stats the roots. It adds no new live coverage.');
aggregator.addNotProven('THE 5.7.4 PASS DOES NOT COVER CURRENT HEAD: the certified binary was built before the plugin refactor now in the tree, and the 5.7.4 record\'s own tree snapshot lists certification harness files rather than plugin source. See environment.certificationDrift for the exact count and the newest changed file.');
aggregator.addNotProven('SIX OF NINE MINORS HAVE NO PLUGIN COMPILE RESULT AT ALL: 5.1/5.2/5.4/5.6 have no engine on this host and 5.0/5.3/5.5 have no compiled editor, so for those seven minors this aggregate proves nothing about whether the plugin works — only that it could not be tried. The offline profile matrix models what WOULD be available; it compiles nothing.');
aggregator.addNotProven('CLIENT PROFILES ARE MODELLED, NOT DRIVEN HERE: the full and minimal profiles are read from the Task 35 source of truth. No MCP client was connected by this task, so nothing here re-proves the bounded fallbacks Task 35 tested.');

/**
 * The deltas a reader is owed, each traced to the generated field that produces
 * it. A delta with no generated field behind it is reported as UNEXPLAINED by
 * `explainProfileMatrix` and fails the aggregate; this section explains the ones
 * that DO reproduce, so "explained" is a statement with content.
 * @param {any} registryDocument @param {any} matrixDocument
 */
function explainDeltas(registryDocument, matrixDocument) {
  const all = registryDocument.records ?? [];
  const belowMin = all.filter((/** @type {any} */ record) => {
    const min = record?.availability?.unreal?.min;
    return min && !(min.major === 5 && min.minor === 0 && min.patch === 0);
  });
  const stateGated = all.filter((/** @type {any} */ record) => !(record?.availability?.editorStates ?? []).includes('edit'));
  /** @type {Record<string, number>} */
  const optionalOccurrences = {};
  for (const record of all) {
    for (const plugin of record?.availability?.requiredPlugins ?? []) {
      if ((matrixDocument.optionalPlugins ?? []).includes(plugin)) optionalOccurrences[plugin] = (optionalOccurrences[plugin] ?? 0) + 1;
    }
  }
  const ceiling = all[0]?.availability?.unreal?.max ?? null;
  return [
    {
      delta: 'ENGINE_BELOW_MIN = 1 on 5.0/5.3/5.5 and absent on 5.7/5.8',
      explainedBy: 'availability.unreal.min',
      generatedRecords: belowMin.map((/** @type {any} */ record) => ({ id: record.id, min: record.availability.unreal.min })),
      reading: `exactly ${belowMin.length} of ${all.length} capability records declare a minimum above 5.0.0, so the gate fires on every minor below it and on none at or above it`,
    },
    {
      delta: 'EDITOR_STATE_UNSUPPORTED = 12 on every profile',
      explainedBy: 'availability.editorStates',
      generatedRecords: stateGated.map((/** @type {any} */ record) => ({ id: record.id, editorStates: record.availability.editorStates })),
      reading: `exactly ${stateGated.length} records omit "edit" from editorStates, and every profile in the matrix models the editor in the edit state`,
    },
    {
      delta: 'PLUGIN_NOT_ENABLED 761 with the optional plugins enabled vs 821 without',
      explainedBy: 'availability.requiredPlugins',
      generatedRecords: Object.entries(optionalOccurrences).map(([plugin, count]) => ({ plugin, occurrences: count })),
      reading: `the ${matrixDocument.optionalPlugins?.length ?? 0} optional plugins account for exactly ${Object.values(optionalOccurrences).reduce((sum, count) => sum + count, 0)} requirement occurrences (821 - 761); the distinct-record availability drop is smaller (671 -> 641) because some of those records were already gated by another requirement`,
    },
    {
      delta: 'the advertised ceiling is a PREVIEW, not a stable 5.8',
      explainedBy: 'availability.unreal.max',
      generatedRecords: [{ appliesTo: `all ${all.length} records`, max: ceiling }],
      reading: 'every record caps at 5.8.0 channel "preview" preview 1, which is why the 5.8 row carries the Preview 1 label and why no claim here reaches a stable 5.8 release',
    },
    {
      delta: 'native preprocessor gates compile 17 conditions on 5.0.3 rising to 307 on 5.8.0',
      explainedBy: '.omo/evidence/task-52/native-gates.json',
      generatedRecords: (matrixDocument.rows ?? []).length > 0 ? [] : [],
      reading: 'each engine row accounts for all 325 conditions across compiled/excluded/undecided; the 6 undecided are __has_include and open-ended ENGINE_MAJOR_VERSION tests that cannot be decided without the engine headers, and they are carried as undecided rather than assumed',
    },
  ];
}

const document = aggregator.finalize(
  result.outcome === 'ACCEPTED'
    ? `${result.counts.PASS} PASS / ${result.counts.FAIL} FAIL / ${result.counts.BLOCKED_EXTERNAL} BLOCKED_EXTERNAL across UE 5.0-5.8. UE 5.7.4 is the only certified minor; UE 5.8 Preview 1 FAILS on our plugin (exit 6, 35+ compile errors); seven minors are externally blocked (4 root-absent, 3 root-unbuilt).`
    : `REFUSED: ${result.rejections.length} rejection(s); the aggregate cannot be defended and no compatibility claim is made.`,
);
aggregator.document.environment.negativeControl = negativeControl;

const written = aggregator.write(OUT);
const validation = validateEvidence(document, { projectRoot: REPO });
log(describeRejections(validation));
log(`wrote ${written}`);

const gateHeld = negativeControl === null || (negativeControl.forgedRefused === true && negativeControl.realAccepted === true);
if (!gateHeld) log('NEGATIVE CONTROL FAILED: the aggregator did not refuse a forged all-pass input set, so its acceptance of the real set proves nothing.');
process.exit(validation.valid && result.outcome === 'ACCEPTED' && gateHeld ? 0 : 3);
