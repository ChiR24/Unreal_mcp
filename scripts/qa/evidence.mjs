#!/usr/bin/env node
// Task 56 — the UE 5.0 certification record.
//
// This task was to run Task 52's disposable certification against
// /data/UnrealEngine-5.0.3: package the plugin, generate a project, build it,
// launch an owned editor, run the corpus, discover capabilities at runtime and
// clean up. None of that happened, and this document says so in the only way that
// is useful to the next person: by proving exactly WHERE it stopped and exactly
// WHAT would let it continue.
//
// THE FINDING, and why the distinction is the whole point:
//
//   Both 5.0 roots on this host are UNBUILT ENGINE SOURCE TREES. Neither has a
//   compiled UnrealEditor-Cmd, so Task 52's orchestrator refuses at stage 1 —
//   inventory.resolve — before it packages anything.
//
//   A packaging attempt was made anyway, to learn whether the PLUGIN compiles on
//   5.0. It never got that far: RunUAT failed compiling the ENGINE's own
//   UnrealBuildTool/AutomationTool pair, which target netcoreapp3.1 while this host
//   offers only a .NET 9 SDK and the trees carry neither prebuilt assemblies nor
//   the bundled SDK that SetupDotnet.sh falls back to.
//
//   So NO PLUGIN SOURCE WAS COMPILED. This document is therefore not evidence that
//   the plugin fails on 5.0, and it must never be read as such — nor as evidence
//   that it succeeds. No compatibility fix is proposed, no source is touched, and
//   no earlier evidence (Task 52's 5.7.4 certification, Task 55's baseline) is
//   disturbed.
//
// Run: node scripts/qa/evidence.mjs [--out FILE]

import { copyFileSync, existsSync, mkdirSync, readFileSync, readdirSync, statSync } from 'node:fs';
import { dirname, join, relative } from 'node:path';

import { EvidenceAggregator, identifyEngine, recordCommand } from '../../tests/unit/evidence-oracles/evidence-aggregator.mjs';
import { describeRejections, validateEvidence } from '../../tests/unit/evidence-oracles/evidence-validator.mjs';
import { observeProcess, observeTree } from '../../tests/unit/evidence-oracles/state-oracles.mjs';
import { buildEngineInventory, formatInventoryTable } from '../../tests/unit/engine-certification/engine-inventory.mjs';
import { readEngineIdentity } from '../../tests/unit/engine-certification/engine-identity.mjs';
import { judgeCleanupRelease } from '../../tests/unit/engine-certification/certification-verdict.mjs';
import { evaluateCapability, defineProfile } from '../../tests/unit/engine-certification/profile-matrix.mjs';
import { judgeOwnership } from '../../tests/unit/engine-certification/disposable-project.mjs';
import {
  buildUnbuiltRootBlocker, diagnoseBuildToolchain, judgeCertificationReadiness, probeEngineRoot,
} from '../../tests/unit/engine-readiness/engine-readiness.mjs';

const argOf = (name, fallback) => {
  const index = process.argv.indexOf(name);
  return index >= 0 ? process.argv[index + 1] : fallback;
};

const REPO = process.cwd();
const OUT = argOf('--out', '.omo/evidence/task-56-pure-unreal-mcp-implementation.json');
const EVIDENCE_DIR = join(REPO, '.omo/evidence/task-56');
/** The root this task was told to certify. Named explicitly; never inferred from a glob. */
const SUBJECT_ROOT = '/data/UnrealEngine-5.0.3';
/** The scratch tree the packaging probe owned, and the only thing this task must clean. */
const PROBE_ROOT = '/tmp/opencode/task56-compile-probe';
const log = (line) => { process.stderr.write(`${line}\n`); };

const TREE = [
  'tests/unit/engine-readiness/engine-readiness.mjs',
  'tests/unit/engine-readiness/engine-readiness.test.ts',
  'scripts/qa/evidence.mjs',
  'tests/unit/engine-certification/engine-identity.mjs',
  'tests/unit/engine-certification/engine-inventory.mjs',
  'tests/unit/engine-certification/certification-stages.mjs',
  'scripts/qa/certify-engine.mjs',
];

/** Newest mtime under a directory, for the "what were this artifact's inputs?" field. */
function newestUnder(root, pattern) {
  let newest = null;
  const walk = (dir) => {
    for (const entry of readdirSync(dir, { withFileTypes: true })) {
      const path = join(dir, entry.name);
      if (entry.isDirectory()) {
        walk(path);
        continue;
      }
      if (!pattern.test(entry.name)) continue;
      const mtimeMs = statSync(path).mtimeMs;
      if (newest === null || mtimeMs > newest.mtimeMs) newest = { file: path, mtimeMs };
    }
  };
  if (existsSync(root)) walk(root);
  return newest;
}

/** Every .NET SDK major version this host offers. The input the engine's C# build lands on. */
function hostDotnetSdkMajors() {
  const record = recordCommand({ file: 'dotnet', args: ['--list-sdks'], timeoutMs: 60_000 });
  const majors = [...String(record.stdoutTail ?? '').matchAll(/^(\d+)\.\d+\.\d+\s/gmu)].map((match) => Number(match[1]));
  return { record, majors: [...new Set(majors)].sort((left, right) => left - right) };
}

const aggregator = new EvidenceAggregator({
  task: 56,
  title: 'Certify the disposable package and live corpus on UE 5.0.3',
  plan: '.omo/plans/pure-unreal-mcp-implementation.md',
  kind: 'wave-7 engine lane',
});
aggregator.recordTree(TREE);
mkdirSync(EVIDENCE_DIR, { recursive: true });

// ── 1. WHAT IS ACTUALLY ON THIS HOST ────────────────────────────────────────
const inventory = buildEngineInventory({ searchDirs: ['/data'] });
const resolution = inventory.resolve('5.0');
const subjectIdentity = readEngineIdentity({ root: SUBJECT_ROOT });
aggregator.document.engine = {
  ...identifyEngine({ engineRoot: SUBJECT_ROOT, projectPath: '(none — no project was generated)' }),
  resolvedFrom: 'Engine/Build/Build.version',
  versionHeaderAgrees: subjectIdentity.sources.versionHeader.agrees,
  gitDescribe: subjectIdentity.sources.gitDescribe.raw,
  channel: subjectIdentity.channel,
  folderNameAgrees: subjectIdentity.folderName.agrees,
  buildable: subjectIdentity.buildable,
  runnable: subjectIdentity.runnable,
};

const sdks = hostDotnetSdkMajors();
aggregator.addCommand(sdks.record);

// Every root that CONTAINS 5.0, judged independently. The task names 5.0.3, and
// the inventory prefers a different root for the same minor — so both are probed
// and reported rather than letting one stand in for the other.
const rootReports = inventory.identities
  .filter((identity) => identity.minorKey === '5.0')
  .map((identity) => {
    const probe = probeEngineRoot({ root: identity.root });
    const toolchain = diagnoseBuildToolchain({ probe, sdkMajorVersions: sdks.majors });
    return {
      identity,
      probe,
      toolchain,
      readiness: judgeCertificationReadiness({ identity, probe, toolchain }),
      observedState: {
        linuxBinariesDirPresent: probe.files.linuxBinaries.present,
        editorCmdPresent: probe.files.editorCmd.present,
        editorExePresent: probe.files.editorExe.present,
        prebuiltUnrealBuildTool: probe.hasPrebuiltUbt,
        prebuiltAutomationTool: probe.hasPrebuiltUat,
        bundledDotnetSdk: probe.hasBundledDotnet,
        uatTargetFramework: probe.uatTargetFramework,
      },
    };
  })
  .sort((left, right) => (left.readiness.root < right.readiness.root ? -1 : 1));

// ── 2. THE PACKAGING PROBE, and what it proves about WHOSE code failed ──────
// Preserved as an artifact because it is the only record of how far the attempt
// got, and "it failed" without the failing line is exactly the unfalsifiable
// shape this project's evidence contract exists to refuse.
const probeLogSource = join(PROBE_ROOT, 'probe.log');
const probeLogKept = join(EVIDENCE_DIR, 'compile-probe-5.0.3.log');
let probeLogText = null;
if (existsSync(probeLogSource)) {
  copyFileSync(probeLogSource, probeLogKept);
  probeLogText = readFileSync(probeLogKept, 'utf8');
}
const pluginNewest = newestUnder(join(REPO, 'plugins/McpAutomationBridge'), /\.(?:h|cpp|inl|cs|uplugin)$/u);
if (probeLogText !== null) {
  aggregator.recordArtifact({
    path: relative(REPO, probeLogKept),
    inputsNewest: pluginNewest === null ? null : relative(REPO, pluginNewest.file),
    inputsNewestAtMs: pluginNewest === null ? null : pluginNewest.mtimeMs,
  });
}
aggregator.recordArtifact({
  path: '.omo/evidence/engine-readiness/certify-5.0-attempt.json',
  inputsNewest: 'scripts/qa/certify-engine.mjs',
  inputsNewestAtMs: statSync(join(REPO, 'scripts/qa/certify-engine.mjs')).mtimeMs,
});

/** The failure signature, read out of the log rather than asserted. */
const probeAnalysis = probeLogText === null ? null : {
  logFile: relative(REPO, probeLogKept),
  reachedPluginCompile: /McpAutomationBridge[^\n]*\.cpp/u.test(probeLogText),
  engineToolchainError: (/error NETSDK\d+[^\n]*/u.exec(probeLogText) ?? [null])[0],
  failedComponent: /AutomationTool\.csproj|UnrealBuildTool\.csproj/u.test(probeLogText)
    ? 'the ENGINE\'s own C# build tools (UnrealBuildTool/AutomationTool)'
    : null,
  finalError: (/RunUAT ERROR:[^\n]*/u.exec(probeLogText) ?? [null])[0],
  interpretation: 'the packaging attempt failed inside the engine\'s C# toolchain. No plugin translation unit was '
    + 'compiled, so this run is evidence about the ENGINE INSTALLATION, not about the plugin\'s 5.0 compatibility.',
};

// ── 3. THE HARNESS'S OWN REFUSAL ────────────────────────────────────────────
const attemptFile = join(EVIDENCE_DIR, 'certify-5.0-attempt.json');
const attempt = existsSync(attemptFile) ? JSON.parse(readFileSync(attemptFile, 'utf8')) : null;
const attemptStages = attempt?.environment?.stages ?? [];

/** Task 52's twenty stages, and which of them this run reached. */
const TASK52_STAGES = Object.freeze([
  'inventory.resolve', 'workspace.open', 'package.plugin', 'project.materialize',
  'build.editorTarget', 'build.binaryFresh', 'ports.stillFree',
  'automation.startedEqualsCompleted', 'automation.noFailures',
  'editor.nativeListening', 'editor.bridgeListening', 'editor.alive', 'dist.fresh',
  'drivers.native', 'drivers.stdio', 'drivers.corpusSubset',
  'cleanup.editor', 'cleanup.workspace', 'cleanup.agrees', 'tree.stable',
]);
const reached = new Map(attemptStages.map((entry) => [entry.name, entry]));
const stageTable = TASK52_STAGES.map((name) => {
  const row = reached.get(name);
  if (row !== undefined) return { stage: name, outcome: row.ok === true ? 'PASSED' : 'FAILED', detail: row.detail ?? null };
  return {
    stage: name, outcome: 'NOT_REACHED',
    detail: 'the run stopped at inventory.resolve; this stage was never attempted and no result may be inferred for it',
  };
});

// ── 4. THE CAPABILITY GAPS THAT COULD BE DECIDED WITHOUT AN EDITOR ──────────
// Task 56 asks for gaps discovered at RUNTIME and reconciled against the records.
// No editor ran, so the runtime half is impossible and is declared not proven.
// What IS decidable is the records half, sharpened by one real filesystem reading:
// which of the plugins the records require actually SHIP in the 5.0.3 tree. That
// is measured here, not assumed, and it is labelled a prediction throughout.
const records = JSON.parse(readFileSync(
  join(REPO, 'src/tools/catalog/capabilities/generated/canonical-registry.generated.json'), 'utf8',
)).records;
const requiredPlugins = [...new Set(records.flatMap((entry) => entry.availability?.requiredPlugins ?? []))].sort();
// A dependency the records call a "plugin" is satisfied by a shipped `.uplugin`
// OR by an engine MODULE of that name. Both spellings appear in the records: UMG,
// RigVM and BehaviorTreeEditor are modules on 5.0 (inside Engine/Source and inside
// the ControlRig plugin), not standalone plugins. Matching `<name>.uplugin` alone
// reported all three absent, which would have inflated the predicted gap with
// three findings that are artefacts of the search, not properties of the engine.
// The match is case-insensitive for the same reason: the records say `MetaSound`
// and the engine ships `Metasound.uplugin`.
const pluginPresence = requiredPlugins.map((plugin) => {
  const asPlugin = findDescriptor(join(SUBJECT_ROOT, 'Engine'), `${plugin}.uplugin`.toLowerCase());
  const asModule = asPlugin !== null ? null : findDescriptor(join(SUBJECT_ROOT, 'Engine'), `${plugin}.build.cs`.toLowerCase());
  const found = asPlugin ?? asModule;
  return {
    plugin,
    shipsWithEngine: found !== null,
    satisfiedBy: asPlugin !== null ? 'uplugin' : asModule !== null ? 'engine-module' : null,
    descriptor: found === null ? null : relative(SUBJECT_ROOT, found),
  };
});

function findDescriptor(root, needleLower, depth = 0) {
  if (depth > 7) return null;
  let entries = [];
  try {
    entries = readdirSync(root, { withFileTypes: true });
  } catch {
    return null;
  }
  for (const entry of entries) {
    if (entry.isFile() && entry.name.toLowerCase() === needleLower) return join(root, entry.name);
  }
  for (const entry of entries) {
    if (!entry.isDirectory() || entry.isSymbolicLink()) continue;
    if (entry.name === 'Intermediate' || entry.name === 'Binaries' || entry.name === 'DerivedDataCache') continue;
    const found = findDescriptor(join(root, entry.name), needleLower, depth + 1);
    if (found !== null) return found;
  }
  return null;
}

const availablePlugins = pluginPresence.filter((entry) => entry.shipsWithEngine).map((entry) => entry.plugin);
const profile = defineProfile({
  id: 'ue5.0.3-plugins-actually-shipped-edit-native',
  engine: { major: 5, minor: 0, patch: 3 },
  plugins: availablePlugins,
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
  whyNotRuntime: 'reconciling declared availability against runtime behaviour requires executing capabilities against a '
    + 'live 5.0.3 editor. No editor exists on this host for that minor, so every runtime cell is unmeasured. These '
    + 'numbers are what the records PREDICT, and they are not a certification result.',
  profile: profile.id,
  totalCapabilities: verdicts.length,
  predictedAvailable: verdicts.filter((verdict) => verdict.available).length,
  predictedWithheld: verdicts.filter((verdict) => !verdict.available).length,
  byGate,
  withheldByEngineFloor,
  engineFloorNote: `exactly ${withheldByEngineFloor.length} capability record declares availability.unreal.min above `
    + '5.0.3; every other record declares min 5.0.0, so the engine-version tier is far narrower than the plugin tier.',
  requiredPluginsMissingFromEngine: pluginPresence.filter((entry) => !entry.shipsWithEngine),
  requiredPluginsSatisfiedByEngineModule: pluginPresence.filter((entry) => entry.satisfiedBy === 'engine-module'),
  pluginPresenceMeasuredBy: `a case-insensitive filesystem scan under ${SUBJECT_ROOT}/Engine for <name>.uplugin, `
    + 'falling back to <name>.Build.cs — a records dependency is satisfied by a shipped plugin OR by an engine module '
    + 'of that name, and several (UMG, RigVM, BehaviorTreeEditor) are modules rather than plugins on 5.0',
  reconciliationOutstanding: 'each predicted-withheld capability must still be shown WITHHELD at runtime, and each '
    + 'predicted-available capability shown AVAILABLE, before any 5.0 capability claim may be made.',
};

// ── 5. OWNED STATE, RELEASED AND PROVEN RELEASED ────────────────────────────
const probePresent = observeTree({ root: PROBE_ROOT, kind: 'owned-workspace' });
aggregator.addObservation(probePresent, { phase: 'pre', id: 'obs-probe-workspace-present' });
const ownership = judgeOwnership({ ownedRoot: PROBE_ROOT, path: PROBE_ROOT });
let removal = { removed: false, reason: 'NOT_ATTEMPTED', detail: 'ownership guard refused' };
if (ownership.owned) {
  const { rmSync } = await import('node:fs');
  try {
    rmSync(PROBE_ROOT, { recursive: true, force: true });
    removal = { removed: !existsSync(PROBE_ROOT), reason: existsSync(PROBE_ROOT) ? 'RESIDUE' : 'REMOVED', detail: `${PROBE_ROOT} removed` };
  } catch (error) {
    removal = { removed: false, reason: 'REMOVE_FAILED', detail: String(error) };
  }
}
const probeGone = observeTree({ root: PROBE_ROOT, kind: 'owned-workspace' });
const probeGoneRef = aggregator.addObservation(probeGone, { phase: 'cleanup', id: 'obs-probe-workspace-gone' });
const release = judgeCleanupRelease({
  resource: PROBE_ROOT,
  claimedReleased: removal.removed === true,
  claimedBy: `rm receipt (${removal.reason})`,
  observation: probeGone,
});
aggregator.document.cleanup.push({
  id: 'cleanup-owned-compile-probe',
  owned: PROBE_ROOT,
  verifiedBy: probeGoneRef,
  pass: release.ok,
  verdict: release.verdict,
  reason: `${removal.detail}; ${release.reason}`,
});

// Controls, one per mechanism this document reads with, each in BOTH polarities.
// auditPositiveControls requires every mechanism to have seen present AND absent;
// a mechanism offered in one polarity only would make the audit fail, which is the
// correct behaviour and the reason these are paired deliberately rather than
// sprinkled.
aggregator.addObservation(observeTree({ root: REPO, kind: 'owned-workspace' }), { phase: 'control', id: 'obs-tree-control-present' });
aggregator.addObservation(observeProcess({ pid: process.pid }), { phase: 'control', id: 'obs-process-control-present' });
aggregator.addObservation(observeProcess({ pid: 4_194_303 }), { phase: 'control', id: 'obs-process-control-absent' });

// ── 6. WHAT THIS DOCUMENT DOES NOT CLAIM ────────────────────────────────────
aggregator.document.environment.subjectRoot = SUBJECT_ROOT;
aggregator.document.environment.engineInventory = {
  table: formatInventoryTable(inventory),
  resolveFiveZero: { reason: resolution.reason, root: resolution.root, detail: resolution.detail },
  duplicates: inventory.duplicates,
  missing: inventory.missing,
};
aggregator.document.environment.hostDotnetSdkMajors = sdks.majors;
aggregator.document.environment.packagingProbe = probeAnalysis;
aggregator.document.environment.stages = stageTable;
aggregator.document.environment.stagesReached = stageTable.filter((row) => row.outcome !== 'NOT_REACHED').length;
aggregator.document.environment.blocker = buildUnbuiltRootBlocker({
  minorKey: '5.0',
  versionString: subjectIdentity.versionString,
  advertisedBy: ['README.md "Unreal Engine 5.0-5.8"', 'plugins/McpAutomationBridge/McpAutomationBridge.uplugin Description'],
  rootReports,
  detection: {
    command: 'node scripts/qa/certify-engine.mjs --engine-version 5.0',
    commandOutcome: attempt?.verdict ?? 'no attempt document was found',
    reproducibleShellCommand: `test -x ${SUBJECT_ROOT}/Engine/Binaries/Linux/UnrealEditor-Cmd`,
    detectedAt: new Date().toISOString(),
    identifiedBy: 'Engine/Build/Build.version, corroborated by Version.h; a directory name is never an input',
  },
});

for (const entry of [
  'LIVE CERTIFICATION: no UE 5.0 editor was launched, so build.editorTarget, the C++ automation suite, both transports, '
    + 'the corpus subset, the security/runtime subset and editor cleanup are all UNMEASURED on this minor.',
  'PLUGIN COMPATIBILITY WITH UE 5.0 IS UNKNOWN AND UNCLAIMED IN BOTH DIRECTIONS. The packaging probe failed inside the '
    + "engine's own netcoreapp3.1 C# toolchain before any plugin source was compiled, so this document is not evidence "
    + 'that the plugin builds on 5.0, and not evidence that it does not.',
  'RUNTIME CAPABILITY GAPS: not discovered. environment.capabilityGapPrediction is derived from the capability records '
    + 'plus a filesystem reading of which required plugins ship in the 5.0.3 tree. No capability was executed, so no '
    + 'declared gap has been confirmed WITHHELD and no declared availability has been confirmed AVAILABLE.',
  'NO SOURCE CHANGE WAS MADE OR PROPOSED for 5.0 compatibility, so no earlier evidence is invalidated: Task 52\'s UE '
    + '5.7.4 certification and Task 55\'s release-candidate baseline stand untouched by this task.',
]) aggregator.addNotProven(entry);

aggregator.addNote('The subject root named by the task is /data/UnrealEngine-5.0.3. Task 52\'s inventory independently '
  + 'prefers /data/UnrealEngine-5.0-branch for minor 5.0 (same 5.0.3 version, tie broken by path). Both were probed; '
  + 'neither can host a certification, so the choice does not change the outcome.');

const verdict = `BLOCKED_EXTERNAL — UE 5.0.3 is installed and correctly identified at ${SUBJECT_ROOT}, but no 5.0 root on `
  + 'this host has a compiled UnrealEditor-Cmd, so Task 52\'s orchestrator refuses at stage 1 of 20 (inventory.resolve) '
  + 'and 19 stages were never attempted. A separate packaging probe failed inside the engine\'s own netcoreapp3.1 C# '
  + 'toolchain, so no plugin source was compiled and no 5.0 compatibility conclusion is drawn in either direction.';

const document = aggregator.finalize(verdict);
const validation = validateEvidence(document, { projectRoot: REPO });
document.environment.selfValidation = validation;
const written = aggregator.write(OUT);

log(describeRejections(validation));
log(`cleanup: ${release.verdict} — ${release.reason}`);
log(`wrote ${written}`);
if (!validation.valid) process.exitCode = 1;
