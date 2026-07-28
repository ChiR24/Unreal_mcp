#!/usr/bin/env node
// Tasks 57 and 58 — the UE 5.3.2 and UE 5.5.4 certification records.
//
// Each task was to run Task 52's disposable certification against its own engine
// root: package the plugin, generate a project, build it, launch an owned editor,
// run the corpus over both transports, discover capabilities at runtime and clean
// up. Neither happened, and these documents say so the only way that is useful to
// the next person: by proving exactly WHERE the run stopped and exactly WHAT
// would let it continue.
//
// THE FINDING, and the distinction the whole record turns on:
//
//   Both roots are INSTALLED and correctly identified from their own
//   Engine/Build/Build.version. Neither has a compiled
//   Engine/Binaries/Linux/UnrealEditor-Cmd, so Task 52's orchestrator refuses at
//   stage 1 of 20 — inventory.resolve — before it packages anything.
//
//   That is a THIRD blocker class. Task 61 records minors that are ABSENT
//   (install the engine). Task 56 records a 5.0 root that is present and cannot
//   even compile its own C# build tools. These two are present, carry compiled
//   UnrealBuildTool/AutomationTool assemblies, and have a PARTIALLY BUILT editor:
//   the module manifest declares more modules than the tree has libraries for and
//   no target receipt was ever written. Remediation is to finish compiling the
//   editor target — hours of machine time, a host-capacity decision.
//
//   NO PLUGIN SOURCE WAS COMPILED and no packaging was attempted: a concurrent
//   lane owns UBT, RunUAT, editors and engine ports exclusively, and a stage-1
//   refusal needs none of them. So neither document is evidence that the plugin
//   builds on its minor, and neither is evidence that it does not. Nineteen
//   stages are recorded NOT_REACHED — neither pass nor fail — for exactly that
//   reason.
//
// This command is READ-ONLY with respect to the engines: it stats and reads
// files. It installs nothing, compiles nothing, launches no editor, claims no
// port and modifies no plugin or engine source.
//
// Run: node scripts/qa/task57-58-evidence.mjs --task 57 [--out FILE]

import { existsSync, readFileSync, readdirSync, statSync } from 'node:fs';
import { join, relative } from 'node:path';

import { EvidenceAggregator, identifyEngine, recordCommand } from '../../tests/unit/task-50/evidence-aggregator.mjs';
import { describeRejections, validateEvidence } from '../../tests/unit/task-50/evidence-validator.mjs';
import { observeProcess, observeTree } from '../../tests/unit/task-50/state-oracles.mjs';
import { buildEngineInventory, formatInventoryTable } from '../../tests/unit/task-52/engine-inventory.mjs';
import { readEngineIdentity } from '../../tests/unit/task-52/engine-identity.mjs';
import { defineProfile, evaluateCapability } from '../../tests/unit/task-52/profile-matrix.mjs';
import { diagnoseBuildToolchain, judgeCertificationReadiness, probeEngineRoot } from '../../tests/unit/task-56/engine-readiness.mjs';
import {
  READINESS_FILES, STAGE_OUTCOMES, STAGE_OUTCOME_CONTRACT,
  buildPresentButUnbuiltBlocker, buildStageTable, probeEditorBuildProgress, summarizeStageTable,
} from '../../tests/unit/task-57/unbuilt-engine-blocker.mjs';

const argOf = (name, fallback) => {
  const index = process.argv.indexOf(name);
  return index >= 0 ? process.argv[index + 1] : fallback;
};

const REPO = process.cwd();
const log = (line) => { process.stderr.write(`${line}\n`); };

/**
 * The two subjects, named explicitly. A root is never inferred from a glob and
 * never from a folder name — the name is only ever compared against what the
 * engine says about itself.
 */
const SUBJECTS = Object.freeze({
  57: {
    task: 57,
    minorKey: '5.3',
    title: 'Certify the disposable package and live corpus on UE 5.3.2',
    subjectRoot: '/data/UnrealEngine-5.3.2',
    evidenceDir: '.omo/evidence/task-57',
    attempt: '.omo/evidence/task-57/certify-5.3-attempt.json',
    attemptLog: '.omo/evidence/task-57/certify-5.3-attempt.log',
    out: '.omo/evidence/task-57-pure-unreal-mcp-implementation.json',
  },
  58: {
    task: 58,
    minorKey: '5.5',
    title: 'Certify the disposable package and live corpus on UE 5.5.4',
    subjectRoot: '/data/UnrealEngine-5.5.4',
    evidenceDir: '.omo/evidence/task-58',
    attempt: '.omo/evidence/task-58/certify-5.5-attempt.json',
    attemptLog: '.omo/evidence/task-58/certify-5.5-attempt.log',
    out: '.omo/evidence/task-58-pure-unreal-mcp-implementation.json',
  },
});

const TREE = [
  'tests/unit/task-57/unbuilt-engine-blocker.mjs',
  'tests/unit/task-57/unbuilt-engine-blocker.test.ts',
  'scripts/qa/task57-58-evidence.mjs',
  'tests/unit/task-56/engine-readiness.mjs',
  'tests/unit/task-52/engine-identity.mjs',
  'tests/unit/task-52/engine-inventory.mjs',
  'tests/unit/task-52/certification-stages.mjs',
  'scripts/qa/task52-certify-engine.mjs',
];

const subject = SUBJECTS[argOf('--task', '')];
if (subject === undefined) {
  log('usage: node scripts/qa/task57-58-evidence.mjs --task 57|58 [--out FILE]');
  process.exit(2);
}
const OUT = argOf('--out', subject.out);
const EVIDENCE_DIR = join(REPO, subject.evidenceDir);

/** Every .NET SDK major this host offers — the input `diagnoseBuildToolchain` rules on. */
function hostDotnetSdkMajors() {
  const record = recordCommand({ file: 'dotnet', args: ['--list-sdks'], timeoutMs: 60_000 });
  const majors = [...String(record.stdoutTail ?? '').matchAll(/^(\d+)\.\d+\.\d+\s/gmu)].map((match) => Number(match[1]));
  return { record, majors: [...new Set(majors)].sort((left, right) => left - right) };
}

/**
 * One walk of an engine tree, indexing every plugin and module descriptor by
 * lowercased filename.
 *
 * Task 56 searched per dependency, which is up to forty walks of an 86GB tree for
 * an answer one walk already contains. The lookup rule is otherwise identical and
 * deliberately so: a records dependency is satisfied by a shipped `<name>.uplugin`
 * OR by an engine module `<name>.Build.cs`, because several of them (UMG, RigVM,
 * BehaviorTreeEditor) are modules rather than plugins, and the match is
 * case-insensitive because the records say `MetaSound` while the engine ships
 * `Metasound.uplugin`.
 */
function indexDescriptors(engineDir, maxDepth = 7) {
  /** @type {Map<string, string>} */
  const index = new Map();
  const skip = new Set(['Intermediate', 'Binaries', 'DerivedDataCache', 'Saved', '.git']);
  const walk = (dir, depth) => {
    if (depth > maxDepth) return;
    let entries = [];
    try {
      entries = readdirSync(dir, { withFileTypes: true });
    } catch {
      return;
    }
    for (const entry of entries) {
      if (entry.isFile()) {
        const lower = entry.name.toLowerCase();
        if ((lower.endsWith('.uplugin') || lower.endsWith('.build.cs')) && !index.has(lower)) {
          index.set(lower, join(dir, entry.name));
        }
        continue;
      }
      if (entry.isDirectory() && !entry.isSymbolicLink() && !skip.has(entry.name)) walk(join(dir, entry.name), depth + 1);
    }
  };
  walk(engineDir, 0);
  return index;
}

const aggregator = new EvidenceAggregator({
  task: subject.task,
  title: subject.title,
  plan: '.omo/plans/pure-unreal-mcp-implementation.md',
  kind: 'wave-7 engine lane',
});
aggregator.recordTree(TREE);

// ── 1. WHAT IS ACTUALLY AT THIS PATH ────────────────────────────────────────
const inventory = buildEngineInventory({ searchDirs: ['/data'] });
const resolution = inventory.resolve(subject.minorKey);
const subjectIdentity = readEngineIdentity({ root: subject.subjectRoot });
aggregator.document.engine = {
  ...identifyEngine({ engineRoot: subject.subjectRoot, projectPath: '(none — no project was generated)' }),
  resolvedFrom: 'Engine/Build/Build.version',
  versionHeaderAgrees: subjectIdentity.sources.versionHeader.agrees,
  versionHeaderFile: subjectIdentity.sources.versionHeader.file,
  gitDescribe: subjectIdentity.sources.gitDescribe.raw,
  channel: subjectIdentity.channel,
  folderNameAgrees: subjectIdentity.folderName.agrees,
  buildable: subjectIdentity.buildable,
  runnable: subjectIdentity.runnable,
};

const sdks = hostDotnetSdkMajors();
aggregator.addCommand(sdks.record);

// Every root that CONTAINS this minor, judged independently. On this host each of
// 5.3 and 5.5 resolves to exactly one root, so there is none of the two-roots
// ambiguity Task 56 had to disambiguate for 5.0 — but the set is computed, not
// assumed, so a second root appearing later is reported rather than ignored.
const rootReports = inventory.identities
  .filter((identity) => identity.minorKey === subject.minorKey)
  .map((identity) => {
    const probe = probeEngineRoot({ root: identity.root });
    const toolchain = diagnoseBuildToolchain({ probe, sdkMajorVersions: sdks.majors });
    return {
      identity,
      probe,
      toolchain,
      progress: probeEditorBuildProgress({ root: identity.root }),
      readiness: judgeCertificationReadiness({ identity, probe, toolchain }),
      observedState: {
        linuxBinariesDirPresent: probe.files.linuxBinaries.present,
        editorCmdPresent: probe.files.editorCmd.present,
        editorExePresent: probe.files.editorExe.present,
        prebuiltUnrealBuildTool: probe.hasPrebuiltUbt,
        prebuiltAutomationTool: probe.hasPrebuiltUat,
        bundledDotnetSdk: probe.hasBundledDotnet,
        ubtTargetFramework: probe.ubtTargetFramework,
        uatTargetFramework: probe.uatTargetFramework,
      },
    };
  })
  .sort((left, right) => (left.readiness.root < right.readiness.root ? -1 : 1));

// ── 2. WHAT A COMPLETED EDITOR BUILD LOOKS LIKE ON THIS HOST ────────────────
// So "the build is incomplete" is a COMPARISON against a measurement rather than
// an assertion about UnrealBuildTool's behaviour. The reference root is a
// different minor and is loudly labelled as evidence about NOTHING else.
const referenceMinor = inventory.available
  .filter((entry) => entry.runnable && entry.minorKey !== subject.minorKey)
  .sort((left, right) => (left.minorKey < right.minorKey ? -1 : 1))[0] ?? null;
const completedBuildReference = referenceMinor === null ? null : {
  purpose: 'a completed UnrealEditor build measured on this host, so "incomplete" below is a comparison rather than '
    + 'an assertion about how UnrealBuildTool behaves',
  notEvidenceFor: `this reference root contains UE ${referenceMinor.versionString}, a DIFFERENT minor. It is not `
    + `evidence about ${subject.minorKey} in any direction: engine headers, module ABI and API availability differ per `
    + 'minor, and no binary, package or project is ever reused across minors.',
  root: referenceMinor.preferredRoot,
  versionString: referenceMinor.versionString,
  ...probeEditorBuildProgress({ root: referenceMinor.preferredRoot }),
};

// ── 3. THE HARNESS'S OWN REFUSAL, AND THE THREE-STATE STAGE TABLE ───────────
const attemptFile = join(REPO, subject.attempt);
const attempt = existsSync(attemptFile) ? JSON.parse(readFileSync(attemptFile, 'utf8')) : null;
const attemptStages = attempt?.environment?.stages ?? [];
const stoppedAt = attemptStages.find((entry) => entry.ok !== true)?.name ?? 'inventory.resolve';
const stageTable = buildStageTable({ attemptStages, stoppedAt });

// Artifacts must outlive the run and be re-checkable, so the attempt document and
// the transcript the orchestrator printed are both recorded by hash. Their inputs
// are the orchestrator's OWN recorded tree, read out of the attempt rather than
// guessed at.
const attemptInputs = (attempt?.tree?.files ?? [])
  .map((entry) => ({ path: entry.path, mtimeMs: existsSync(join(REPO, entry.path)) ? statSync(join(REPO, entry.path)).mtimeMs : 0 }))
  .sort((left, right) => right.mtimeMs - left.mtimeMs)[0] ?? null;
for (const path of [subject.attempt, subject.attemptLog]) {
  if (!existsSync(join(REPO, path))) continue;
  aggregator.recordArtifact({
    path,
    inputsNewest: attemptInputs === null ? null : attemptInputs.path,
    inputsNewestAtMs: attemptInputs === null ? null : attemptInputs.mtimeMs,
  });
}

// ── 4. THE CAPABILITY GAPS THAT CAN BE DECIDED WITHOUT AN EDITOR ────────────
// The task asks for gaps discovered at RUNTIME and reconciled against the
// records. No editor ran, so the runtime half is impossible and is declared not
// proven. What IS decidable is the records half, sharpened by one real filesystem
// reading of this engine: which of the plugins the records require actually ship
// in THIS tree. That is measured, not assumed, and labelled a prediction
// throughout.
const records = JSON.parse(readFileSync(
  join(REPO, 'src/tools/catalog/capabilities/generated/canonical-registry.generated.json'), 'utf8',
)).records;
const requiredPlugins = [...new Set(records.flatMap((entry) => entry.availability?.requiredPlugins ?? []))].sort();
const descriptors = indexDescriptors(join(subject.subjectRoot, 'Engine'));
const pluginPresence = requiredPlugins.map((plugin) => {
  const asPlugin = descriptors.get(`${plugin.toLowerCase()}.uplugin`) ?? null;
  const asModule = asPlugin !== null ? null : descriptors.get(`${plugin.toLowerCase()}.build.cs`) ?? null;
  const found = asPlugin ?? asModule;
  return {
    plugin,
    shipsWithEngine: found !== null,
    satisfiedBy: asPlugin !== null ? 'uplugin' : asModule !== null ? 'engine-module' : null,
    descriptor: found === null ? null : relative(subject.subjectRoot, found),
  };
});

const version = subjectIdentity.version;
const profile = defineProfile({
  id: `ue${subjectIdentity.versionString}-plugins-actually-shipped-edit-native`,
  engine: { major: version.major, minor: version.minor, patch: version.patch },
  plugins: pluginPresence.filter((entry) => entry.shipsWithEngine).map((entry) => entry.plugin),
});
const verdicts = records.map((record) => evaluateCapability(record, profile));
const byGate = {};
for (const verdict of verdicts) {
  for (const gate of verdict.gates) byGate[gate.code] = (byGate[gate.code] ?? 0) + 1;
}
const withheldByEngineFloor = verdicts
  .filter((verdict) => verdict.gates.some((gate) => gate.code === 'ENGINE_BELOW_MIN'))
  .map((verdict) => verdict.id);

aggregator.document.environment.capabilityGapPrediction = {
  status: 'PREDICTED_FROM_RECORDS_NOT_DISCOVERED_AT_RUNTIME',
  whyNotRuntime: 'reconciling declared availability against runtime behaviour requires executing capabilities against '
    + `a live ${subjectIdentity.versionString} editor. No editor binary exists at this root, so every runtime cell is `
    + 'unmeasured. These numbers are what the records PREDICT, and they are not a certification result.',
  profile: profile.id,
  totalCapabilities: verdicts.length,
  predictedAvailable: verdicts.filter((verdict) => verdict.available).length,
  predictedWithheld: verdicts.filter((verdict) => !verdict.available).length,
  byGate,
  withheldByEngineFloor,
  engineFloorNote: `${withheldByEngineFloor.length} capability record(s) declare availability.unreal.min above `
    + `${subjectIdentity.versionString}; the engine-version tier is far narrower than the plugin tier.`,
  requiredPluginsMissingFromEngine: pluginPresence.filter((entry) => !entry.shipsWithEngine),
  requiredPluginsSatisfiedByEngineModule: pluginPresence.filter((entry) => entry.satisfiedBy === 'engine-module'),
  pluginPresenceMeasuredBy: `one case-insensitive index of every <name>.uplugin and <name>.Build.cs under `
    + `${subject.subjectRoot}/Engine — a records dependency is satisfied by a shipped plugin OR by an engine module of `
    + 'that name, and several (UMG, RigVM, BehaviorTreeEditor) are modules rather than plugins',
  descriptorsIndexed: descriptors.size,
  reconciliationOutstanding: 'each predicted-withheld capability must still be shown WITHHELD at runtime, and each '
    + 'predicted-available capability shown AVAILABLE, before any capability claim may be made for this minor.',
};

// ── 5. OBSERVATIONS, IN BOTH POLARITIES ─────────────────────────────────────
// The absent reading IS the finding: the same filesystem reader that finds this
// run's recorded attempt artifacts present finds the editor binary the run needed
// absent. A mechanism that only ever answered one way would satisfy every absence
// assertion in the document while detecting nothing, which is what the
// positive-control audit refuses.
aggregator.addObservation(
  observeTree({ root: join(subject.subjectRoot, READINESS_FILES.editorCmd), kind: 'certification-input' }),
  { phase: 'pre', id: 'obs-editor-binary-absent' },
);
aggregator.addObservation(
  observeTree({ root: EVIDENCE_DIR, kind: 'certification-input' }),
  { phase: 'control', id: 'obs-attempt-artifacts-present' },
);
aggregator.addObservation(observeProcess({ pid: process.pid }), { phase: 'control', id: 'obs-process-control-present' });
aggregator.addObservation(observeProcess({ pid: 4_194_303 }), { phase: 'control', id: 'obs-process-control-absent' });

// ── 6. THE RECORD ───────────────────────────────────────────────────────────
aggregator.document.environment.subjectRoot = subject.subjectRoot;
aggregator.document.environment.subjectMinor = subject.minorKey;
aggregator.document.environment.engineInventory = {
  table: formatInventoryTable(inventory),
  resolveSubjectMinor: { minorKey: subject.minorKey, reason: resolution.reason, root: resolution.root, detail: resolution.detail },
  rootsContainingSubjectMinor: rootReports.map((entry) => entry.readiness.root),
  duplicates: inventory.duplicates,
  missing: inventory.missing,
};
aggregator.document.environment.hostDotnetSdkMajors = sdks.majors;
aggregator.document.environment.completedBuildReference = completedBuildReference;
aggregator.document.environment.stageOutcomeContract = STAGE_OUTCOME_CONTRACT;
aggregator.document.environment.stages = stageTable;
aggregator.document.environment.stageSummary = summarizeStageTable(stageTable);
aggregator.document.environment.stagesReached = summarizeStageTable(stageTable).attempted;
aggregator.document.environment.attemptDocument = {
  path: subject.attempt,
  transcript: subject.attemptLog,
  refusedAt: stoppedAt,
  commandVerdict: attempt?.verdict ?? 'no attempt document was found',
  blocked: attempt?.environment?.blocked ?? [],
};
aggregator.document.environment.treeStability = attempt?.environment?.treeStability ?? null;
aggregator.document.environment.residue = {
  workspacesOpened: 0,
  portsClaimed: 0,
  processesSpawned: 0,
  detail: `the orchestrator refused at ${stoppedAt}, which runs before workspace.open, so no disposable workspace was `
    + 'created, no port was claimed and no editor process was spawned. There is nothing this run could have leaked.',
  attemptCleanup: attempt?.environment?.cleanup ?? null,
};
aggregator.document.environment.blocker = buildPresentButUnbuiltBlocker({
  task: subject.task,
  minorKey: subject.minorKey,
  versionString: subjectIdentity.versionString,
  advertisedBy: ['README.md "Unreal Engine 5.0-5.8"', 'plugins/McpAutomationBridge/McpAutomationBridge.uplugin Description'],
  rootReports,
  completedBuildReference,
  detection: {
    command: `node scripts/qa/task52-certify-engine.mjs --engine-version ${subject.minorKey}`,
    commandOutcome: attempt?.verdict ?? 'no attempt document was found',
    commandExitCode: 1,
    reproducibleShellCommand: `test -x ${join(subject.subjectRoot, READINESS_FILES.editorCmd)}`,
    detectedAt: new Date().toISOString(),
    identifiedBy: 'Engine/Build/Build.version, corroborated by Version.h and the engine\'s own git tag; a directory '
      + 'name is never an input',
  },
});

for (const entry of [
  `LIVE CERTIFICATION: no UE ${subjectIdentity.versionString} editor was launched, so build.editorTarget, the C++ `
    + 'automation suite, both transport surfaces, the corpus subset, the oracles, the security/runtime subset and '
    + 'editor cleanup are all UNMEASURED on this minor. Nineteen of Task 52\'s twenty stages are recorded NOT_REACHED, '
    + 'which is neither a pass nor a failure and must never be coerced to either.',
  `PLUGIN COMPATIBILITY WITH UE ${subject.minorKey} IS UNKNOWN AND UNCLAIMED IN BOTH DIRECTIONS. No plugin translation `
    + 'unit was compiled and no packaging was attempted: the orchestrator refused before package.plugin, and this lane '
    + 'holds no UBT or RunUAT resource. This document is not evidence that the plugin builds on this minor, and not '
    + 'evidence that it does not.',
  'RUNTIME CAPABILITY GAPS: not discovered. environment.capabilityGapPrediction is derived from the capability records '
    + 'plus a filesystem reading of which required plugins ship in this engine tree. No capability was executed, so no '
    + 'declared gap has been confirmed WITHHELD and no declared availability has been confirmed AVAILABLE.',
  'WHY THE EDITOR BUILD STOPPED: not established. The counts under editorBuildProgress are files on disk right now. '
    + 'They show a build that produced module libraries and never wrote a target receipt or an executable; they say '
    + 'nothing about whether it failed, was interrupted or was deliberately abandoned.',
  'NO SOURCE CHANGE WAS MADE OR PROPOSED for compatibility with this minor, so no earlier evidence is invalidated: '
    + "Task 52's UE 5.7.4 certification and Task 55's release-candidate baseline stand untouched. The orchestrator's "
    + 'own tree.stable stage re-hashed its ten recorded source files at the end of the attempt and found them '
    + 'byte-identical.',
]) aggregator.addNotProven(entry);

aggregator.addNote(`Exactly one root on this host contains ${subject.minorKey}, so there is no preferred-root ambiguity `
  + 'to disambiguate here; Task 56 had two roots for 5.0 and had to probe both. The set is computed from the inventory '
  + 'rather than assumed, so a second root appearing later would be reported.');
aggregator.addNote('The two fs:tree-digest readings are the positive control AND the finding: the same reader returns '
  + `present for ${subject.evidenceDir} (this run's recorded attempt artifacts) and absent for `
  + `${join(subject.subjectRoot, READINESS_FILES.editorCmd)} (the binary the certification needed). A reader that `
  + 'could only ever answer one way would satisfy every absence assertion in this document while detecting nothing.');
aggregator.addNote('This lane started no build and launched no editor, so it never contended with the concurrent lane '
  + 'that owns UBT, RunUAT, editors and engine ports. A stage-1 refusal costs none of those resources.');

const summary = summarizeStageTable(stageTable);
const verdict = `BLOCKED_EXTERNAL — UE ${subjectIdentity.versionString} is installed and correctly identified at `
  + `${subject.subjectRoot} (read from ${READINESS_FILES.buildVersion}, corroborated by Version.h and the tag `
  + `${subjectIdentity.sources.gitDescribe.raw}), but it has no compiled ${READINESS_FILES.editorCmd}, so Task 52's `
  + `orchestrator refuses at stage 1 of ${summary.total} (${stoppedAt}) and ${summary.notReached} stages were never `
  + 'attempted. The editor target is partially built — '
  + `${String(rootReports[0]?.progress?.moduleLibrariesInEngineBinaries)} of `
  + `${String(rootReports[0]?.progress?.declaredModules)} declared modules have libraries and no target receipt exists `
  + '— and the remediation is to finish compiling it, which is an operator decision. No plugin source was compiled, so '
  + `no UE ${subject.minorKey} compatibility conclusion is drawn in either direction.`;

const document = aggregator.finalize(verdict);
const validation = validateEvidence(document, { projectRoot: REPO });
document.environment.selfValidation = validation;
const written = aggregator.write(OUT);

log(`${subject.minorKey}: refused at ${stoppedAt}; ${summary.passed} passed / ${summary.failed} failed / ${summary.notReached} ${STAGE_OUTCOMES.NOT_REACHED}`);
log(describeRejections(validation));
log(`wrote ${written}`);
if (!validation.valid) process.exitCode = 1;
