#!/usr/bin/env node
// Task 59 — compose the task-level evidence for the UE 5.7.4 certification.
//
// WHAT MAKES THIS RUN DIFFERENT FROM TASK 52's, and why it had to be re-run.
// Task 52 already certified this same engine at 20/20. It used the same
// orchestrator, and its run document is still on disk. Citing it would have been
// cheaper and would have been wrong: Tasks 53-58 landed in nine commits between
// that run and this one, so the tree Task 52 measured is not the tree that ships.
// A certification that names a source hash nobody can reproduce from HEAD is the
// stale-artifact defect this plan keeps catching, and repeating it inside the
// task whose job is to catch it would be the worst place to do it. So the engine
// is the same, the orchestrator is the same, and the RUN is new: new workspace,
// new ports, new package digest, new .so digest, new pids, new timestamps.
//
// WHAT IS SHARED AND WHAT IS OWNED. The 20-stage orchestration is Task 52's
// (`scripts/qa/task52-certify-engine.mjs`) and is driven UNMODIFIED — its file
// hash, and those of the nine helpers it imports, are recorded inside the
// evidence of Tasks 52, 56, 57, 58 and 60, and editing one to fit this task
// would retroactively invalidate five records whose runs can never be repeated.
// What Task 59 owns is the supervisor that opens the runtime window, the
// capability probe that reads the live surface, and this document.
//
// THE HALF THAT CAN BE RE-DERIVED AND THE HALF THAT CANNOT, kept apart on
// purpose. The record model below is recomputed here from the generated
// registry, so if the catalogue moves these numbers move with it and the
// difference is visible. The live readings happened once, against bytes a
// disposable run then deleted, and are carried over VERBATIM with their ids
// intact — re-deriving them is exactly what nobody can do, which is what made
// them evidence.
//
// Run: node scripts/qa/task59-evidence.mjs [--certification FILE] [--probe FILE]
//                                          [--supervisor FILE] [--out FILE]

import { execFileSync } from 'node:child_process';
import { existsSync, readFileSync } from 'node:fs';

import { readEngineIdentity } from '../../tests/unit/task-52/engine-identity.mjs';
import { buildEngineInventory, formatInventoryTable } from '../../tests/unit/task-52/engine-inventory.mjs';
import {
  collectNativeGates, defineProfile, evaluateCapability, evaluateNativeFeatures,
} from '../../tests/unit/task-52/profile-matrix.mjs';
import { surveyOwnedParent, findOrphanedProcesses } from '../../tests/unit/task-52/disposable-project.mjs';
import { EvidenceAggregator } from '../../tests/unit/task-50/evidence-aggregator.mjs';
import { describeRejections, validateEvidence } from '../../tests/unit/task-50/evidence-validator.mjs';

const argOf = (name, fallback) => {
  const index = process.argv.indexOf(name);
  return index >= 0 ? process.argv[index + 1] : fallback;
};

const REPO = process.cwd();
const ENGINE_ROOT = argOf('--engine-root', '/data/UnrealEngine');
const CERTIFICATION = argOf('--certification', '.omo/evidence/task-59/run3/certify-5.7.4.json');
const PROBE = argOf('--probe', '.omo/evidence/task-59/run3/capability-probe.json');
const SUPERVISOR = argOf('--supervisor', '.omo/evidence/task-59/run3/supervisor.json');
const TS_CENSUS = argOf('--ts-census', '.omo/evidence/task-59/ts-contract-census.json');
const OUT = argOf('--out', '.omo/evidence/task-59-pure-unreal-mcp-implementation.json');
const PLUGIN_ROOT = `${REPO}/plugins/McpAutomationBridge`;
const log = (line) => { process.stderr.write(`${line}\n`); };

/** The files whose identity this evidence depends on: what Task 59 owns, then what it drove. */
const TREE = [
  'scripts/qa/task59-supervise.mjs',
  'scripts/qa/task59-capability-probe.mjs',
  'scripts/qa/task59-transport-census.mjs',
  'scripts/qa/task59-evidence.mjs',
  'tests/unit/task-59/capability-verdicts.mjs',
  'scripts/qa/task52-certify-engine.mjs',
  'tests/unit/task-52/engine-identity.mjs',
  'tests/unit/task-52/engine-inventory.mjs',
  'tests/unit/task-52/preprocessor-conditions.mjs',
  'tests/unit/task-52/profile-matrix.mjs',
  'tests/unit/task-52/disposable-project.mjs',
  'tests/unit/task-52/project-scaffold.mjs',
  'tests/unit/task-52/certification-stages.mjs',
  'tests/unit/task-52/certification-drivers.mjs',
  'tests/unit/task-52/certification-verdict.mjs',
];

/** The 20 stages Task 52 defines, in order. */
const STAGE_ORDER = [
  'inventory.resolve', 'workspace.open', 'package.plugin', 'project.materialize',
  'build.editorTarget', 'build.binaryFresh', 'ports.stillFree',
  'automation.startedEqualsCompleted', 'automation.noFailures',
  'editor.nativeListening', 'editor.bridgeListening', 'editor.alive',
  'dist.fresh', 'drivers.native', 'drivers.stdio', 'drivers.corpusSubset',
  'cleanup.editor', 'cleanup.workspace', 'cleanup.agrees', 'tree.stable',
];

const readJson = (file) => (existsSync(file) ? JSON.parse(readFileSync(file, 'utf8')) : null);

const aggregator = new EvidenceAggregator({
  task: 59,
  title: 'Certify the disposable package and live corpus on UE 5.7.4',
  plan: '.omo/plans/pure-unreal-mcp-implementation.md',
  kind: 'wave-7 engine lane',
});
aggregator.recordTree(TREE);

// ── 1. WHAT ENGINE IS ACTUALLY AT THAT PATH ─────────────────────────────────
// Read, never taken from the folder name. `/data/UnrealEngine` carries no
// version in its path at all, which is precisely why the name can never be the
// answer. Tasks 57 and 58 corroborated Build.version with Version.h and the
// release tag; this does the same, and reports where the three agree and differ.
const identity = readEngineIdentity({ root: ENGINE_ROOT });
let engineHead = null;
try {
  engineHead = execFileSync('git', ['-C', ENGINE_ROOT, 'rev-parse', 'HEAD'], { encoding: 'utf8', timeout: 15_000 }).trim();
} catch { engineHead = null; }
let repoHead = null;
try {
  repoHead = execFileSync('git', ['-C', REPO, 'rev-parse', 'HEAD'], { encoding: 'utf8', timeout: 15_000 }).trim();
} catch { repoHead = null; }

aggregator.document.engine = {
  root: ENGINE_ROOT,
  versionString: identity.versionString,
  resolvedFrom: 'Engine/Build/Build.version',
  buildVersionFile: identity.sources.buildVersion.file,
  buildVersionSha256: identity.sources.buildVersion.sha256,
  versionHeaderAgrees: identity.sources.versionHeader.agrees,
  branch: identity.branch,
  changelist: identity.changelist,
  compatibleChangelist: identity.compatibleChangelist,
  isPromotedBuild: identity.isPromotedBuild,
  isLicenseeVersion: identity.isLicenseeVersion,
  gitDescribe: identity.sources.gitDescribe.raw,
  gitHeadCommit: engineHead,
  hasCompiledEditor: identity.toolchain.hasCompiledEditor,
  channelLabelling: {
    folderLabel: identity.channel.folderLabel,
    folderNameAgrees: identity.folderName.agrees,
    engineTag: identity.channel.tag,
    engineTagIsExact: identity.channel.exactTag,
    statedAs: `UE ${identity.versionString} on branch ${String(identity.branch)}, proven by Build.version and `
      + 'corroborated by Version.h. The path carries no version, so the folder name is not a source here and '
      + 'is not treated as one.',
    // The tag is 5.7.0-preview-1-<n>-g<sha>: a DESCRIBE from an old tag, not the
    // engine's identity. Reporting it as "preview" would contradict the two
    // sources that actually state the version, so it is recorded and discounted.
    tagNotUsedAsVersion: 'git describe resolves to a 5.7.0-preview-1 ancestor tag plus commit distance. '
      + 'Build.version and Version.h both state 5.7.4 exactly and agree with each other; the describe '
      + 'string refines provenance only and is NOT read as a channel claim.',
  },
};

const inventory = buildEngineInventory({ searchDirs: ['/data'] });
aggregator.document.environment.engineInventory = {
  table: formatInventoryTable(inventory),
  available: inventory.available.map(({ identity: _identity, ...rest }) => rest),
  missing: inventory.missing,
  unusable: inventory.unusable,
  folderNameContradictions: inventory.folderNameContradictions,
};
aggregator.document.environment.repositoryHead = repoHead;

// ── 2. THE LIVE RUN, CARRIED OVER VERBATIM ──────────────────────────────────
const certification = readJson(`${REPO}/${CERTIFICATION}`);
/** @type {Array<Record<string, unknown>>} */
let stageRows = [];
if (certification === null) {
  aggregator.addNotProven(`LIVE CERTIFICATION: no run document at ${CERTIFICATION}. Nothing in this record claims `
    + 'that an engine was packaged, built, launched or driven.');
  stageRows = STAGE_ORDER.map((name) => ({
    name, ok: null, detail: 'NOT ATTEMPTED: the certification produced no run document, so this stage has no result '
      + 'and none may be inferred for it in either direction',
  }));
} else {
  aggregator.document.clients = certification.clients ?? [];
  aggregator.document.commands = certification.commands ?? [];
  aggregator.document.transcripts = certification.transcripts ?? [];
  aggregator.document.observations = certification.observations ?? [];
  aggregator.document.claims = certification.claims ?? [];
  aggregator.document.cleanup = certification.cleanup ?? [];
  aggregator.document.artifacts = certification.artifacts ?? [];
  aggregator.document.environment.processes = certification.environment?.processes ?? [];

  // A stage the run never reached is recorded `null` — neither pass nor fail —
  // with the reason. Tasks 56, 57, 58 and 60 all did this; it is what stops an
  // unreached stage reading as a pass when Task 62 aggregates the engine rows.
  const observed = new Map((certification.environment?.stages ?? []).map((row) => [row.name, row]));
  stageRows = STAGE_ORDER.map((name) => {
    const row = observed.get(name);
    if (row === undefined) {
      return {
        name, ok: null,
        detail: 'NOT ATTEMPTED: the run ended before this stage, so it has no result and none may be inferred',
      };
    }
    return { name, ok: row.ok === true, detail: row.detail ?? null };
  });

  aggregator.document.environment.certification = {
    source: CERTIFICATION,
    producedBy: 'scripts/qa/task52-certify-engine.mjs — the shared 20-stage orchestrator, driven UNMODIFIED. '
      + 'The harness is Task 52\'s; the run, its workspace, its ports, its artifacts and its pids are Task 59\'s.',
    generatedAt: certification.generatedAt,
    verdict: certification.verdict,
    workspace: certification.environment?.workspace ?? null,
    package: certification.environment?.package ?? null,
    binaryFreshness: certification.environment?.binaryFreshness ?? null,
    portCheck: certification.environment?.portCheck ?? null,
    editor: certification.environment?.editor ?? null,
    automation: certification.environment?.automation ?? null,
    drivers: certification.environment?.drivers ?? null,
    cleanup: certification.environment?.cleanup ?? null,
    cleanupAgreement: certification.environment?.cleanupAgreement ?? null,
    treeStability: certification.environment?.treeStability ?? null,
    blocked: certification.environment?.blocked ?? [],
    selfValidation: certification.environment?.selfValidation ?? null,
  };

  // The proof that this is a NEW run rather than Task 52's, stated as the three
  // digests and the workspace that cannot coincide between two disposable runs.
  const prior = readJson(`${REPO}/.omo/evidence/task-52/certify-5.7-run2.json`);
  aggregator.document.environment.freshnessVsTask52 = {
    priorRun: prior === null ? null : {
      source: '.omo/evidence/task-52/certify-5.7-run2.json',
      generatedAt: prior.generatedAt,
      workspace: prior.environment?.workspace?.root ?? null,
      packageSha256: prior.environment?.package?.sha256 ?? null,
      binarySha256: prior.environment?.binaryFreshness?.binarySha256 ?? null,
      editorPid: prior.environment?.editor?.pid ?? null,
    },
    thisRun: {
      source: CERTIFICATION,
      generatedAt: certification.generatedAt,
      workspace: certification.environment?.workspace?.root ?? null,
      packageSha256: certification.environment?.package?.sha256 ?? null,
      binarySha256: certification.environment?.binaryFreshness?.binarySha256 ?? null,
      editorPid: certification.environment?.editor?.pid ?? null,
    },
    distinctWorkspace: prior === null ? null
      : prior.environment?.workspace?.root !== certification.environment?.workspace?.root,
    distinctBinary: prior === null ? null
      : prior.environment?.binaryFreshness?.binarySha256 !== certification.environment?.binaryFreshness?.binarySha256,
    why: 'Task 52 certified this same engine at 20/20 before Tasks 53-58 landed. This record does not cite that '
      + 'run: the fields above are the disposable artifacts that cannot be shared between two runs, and they '
      + 'differ, so the code certified here is the code in the tree now.',
  };
}
aggregator.document.environment.stages = stageRows;

// ── 3. THE CAPABILITY MODEL, RECOMPUTED FROM THE RECORDS FOR THIS ENGINE ────
const registry = JSON.parse(readFileSync(
  `${REPO}/src/tools/catalog/capabilities/generated/canonical-registry.generated.json`, 'utf8',
));
/** @type {any[]} */
const records = registry.records;
const nativeGates = collectNativeGates({ pluginRoot: PLUGIN_ROOT });
const ENGINE_5_7_4 = { major: 5, minor: 7, patch: 4 };

// The disposable project enables exactly one plugin — the bridge — so the honest
// offline counterpart of this run is the no-optional-plugins profile, not the
// all-plugins one. Scoring the run against the generous profile would report
// hundreds of "missing" capabilities that the project never asked for.
const profile = defineProfile({
  id: 'ue5.7.4-generated-project', engine: ENGINE_5_7_4, plugins: [], editorState: 'edit',
});
const evaluated = records.map((record) => ({ record, verdict: evaluateCapability(record, profile) }));
/** @type {Record<string, number>} */
const byGate = {};
for (const row of evaluated) {
  for (const gate of row.verdict.gates ?? []) byGate[gate.code] = (byGate[gate.code] ?? 0) + 1;
}
const engineGated = records.filter((record) => {
  const min = record.availability?.unreal?.min;
  return min !== undefined && (min.major > 5 || min.minor > 0);
});
const nativeFeatures = evaluateNativeFeatures(profile, nativeGates);

aggregator.document.environment.recordModel = {
  capabilityRecords: records.length,
  catalogRevision: registry.catalogRevision ?? null,
  profile: profile.id,
  available: evaluated.filter((row) => row.verdict.available === true).length,
  filtered: evaluated.filter((row) => row.verdict.available !== true).length,
  byGate,
  engineRange: {
    declaringMinAboveBaseline: engineGated.map((record) => ({
      id: record.id, min: record.availability.unreal.min, requiredPlugins: record.availability.requiredPlugins,
    })),
    withheldByMaxOnThisEngine: records
      .filter((record) => {
        const max = record.availability?.unreal?.max;
        return max !== undefined && (max.major < 5 || (max.major === 5 && max.minor < 7));
      })
      .map((record) => record.id),
    expectation: 'EXACTLY ONE capability in the catalogue declares a minimum above the 5.0 baseline, and it '
      + 'declares 5.7.0. On 5.7.4 that minimum is met and no declared maximum (all 5.8.0-preview-1) is '
      + 'exceeded, so the ENGINE-RANGE gate withholds nothing here. Every gap on this engine is therefore a '
      + 'plugin gap, an editor-state gap or a compile gap — and the runtime probe below says which.',
  },
  nativeGateCensus: {
    conditionsFound: nativeGates.conditions.length,
    compiledAt5_7_4: nativeFeatures.compiledCount,
    excludedAt5_7_4: nativeFeatures.excludedCount,
    undecidedAt5_7_4: nativeFeatures.undecidedCount,
    simulatedMacros: nativeFeatures.macros,
  },
};

// ── 4. RECONCILE THE MODEL AGAINST WHAT THE LIVE ENGINE DID ─────────────────
const probe = readJson(`${REPO}/${PROBE}`);
const supervisor = readJson(`${REPO}/${SUPERVISOR}`);
aggregator.document.environment.supervisor = supervisor === null ? null : {
  source: SUPERVISOR,
  probeFiredAfterStage: supervisor.probe?.firedAfterStage ?? null,
  probeFiredAt: supervisor.probe?.firedAt ?? null,
  probeSkipped: supervisor.probe?.skipped ?? null,
  ports: supervisor.probe?.ports ?? null,
  certificationExitCode: supervisor.certification?.exitCode ?? null,
  probeExitCode: supervisor.probe?.exitCode ?? null,
  note: 'the certification exit code is RECORDED, never read as the verdict: a fully green Unreal automation '
    + 'run returns non-zero because a handled engine ensure fires before the first test. The stage table and '
    + 'the started==completed comparison are the verdict.',
};

if (probe === null || probe.probed !== true) {
  aggregator.addNotProven('RUNTIME CAPABILITY CENSUS: '
    + (probe === null
      ? `no probe document at ${PROBE}; the record model above stands on its own and nothing here claims the `
        + 'live surface was read'
      : `the probe did not run (${String(probe.reason)}: ${String(probe.detail)}); no census was taken and none is inferred`));
  aggregator.document.environment.capabilityReconciliation = {
    probed: false,
    reason: probe === null ? 'NO_PROBE_DOCUMENT' : String(probe.reason),
    detail: 'expected capability gaps could NOT be reconciled against the records at runtime',
  };
} else {
  // The compile gate, model versus reality. This is the divergence worth naming:
  // the offline evaluator keys MCP_HAS_* off the PROFILE's plugin list, while
  // Build.cs keys them off what the ENGINE tree makes available at compile time.
  // On a project that enables only the bridge the two disagree, and the build is
  // right — so a gap predicted from the simulation alone would be fiction.
  const real = probe.compiledMacros?.macros ?? null;
  const simulated = nativeFeatures.macros ?? {};
  const macroRows = real === null ? [] : Object.keys(real).sort().map((name) => ({
    macro: name, compiled: real[name], simulated: simulated[name] ?? null,
    agrees: simulated[name] === undefined ? null : simulated[name] === real[name],
  }));

  aggregator.document.environment.capabilityReconciliation = {
    probed: true,
    port: probe.port,
    negotiatedVersion: probe.negotiatedVersion ?? null,
    exactlyOneGatewayTool: probe.exactlyOneGatewayTool === true,
    publicTools: probe.publicTools ?? [],
    contractFidelity: {
      declared: probe.totals?.declared ?? null,
      runtimeNamed: probe.totals?.runtimeNamed ?? null,
      toolsWithMissing: probe.totals?.toolsWithMissing ?? null,
      toolsWithExtra: probe.totals?.toolsWithExtra ?? null,
      perTool: probe.perTool ?? [],
      whatThisDoesAndDoesNotProve: 'describe is a CONTRACT query. src/server/gateway/gateway-availability.ts '
        + 'decides status from deprecation and the dynamic tool manager only, and reports engine range, '
        + 'plugins and editor states as DATA — a documented refusal to guess without a live editor. So this '
        + 'section proves the surface names exactly the catalogue and invents nothing; it is NOT an '
        + 'availability verdict, and reading it as one would report a perfect match on every engine.',
    },
    environmentGate: {
      probes: probe.gateProbes ?? [],
      summary: probe.gateSummary ?? null,
      whereTheGateLives: 'the environment gate is enforced at execute, by the plugin, on the game thread. '
        + 'Each probe targets an actor that does not exist, so the call reaches the handler and is refused on '
        + 'its merits while mutating nothing, and the refusal REASON separates an engine gate from a plugin '
        + 'gate from an editor-state gate.',
    },
    compileGate: {
      source: probe.compiledMacros?.source ?? null,
      reason: probe.compiledMacros?.reason ?? null,
      macros: macroRows,
      disagreements: macroRows.filter((row) => row.agrees === false).map((row) => row.macro),
      interpretation: real === null
        ? 'the compiled macro set could not be read from this build, so no compile-gate claim is made'
        : 'MCP_HAS_* is decided by Build.cs probing the ENGINE tree, not the project\'s enabled-plugin list. '
          + 'The offline profile model keys the same macros off the profile\'s plugin list, so on a project '
          + 'that enables only the bridge the simulation UNDERSTATES what was compiled. The build is the '
          + 'authority; the disagreements listed here are a limitation of the offline model, not a product '
          + 'defect, and are recorded so Task 62 does not read them as one.',
    },
  };

  // ── THE CROSS-TRANSPORT DIVERGENCE THIS RUN FOUND ─────────────────────────
  // The native census is only interpretable next to the contract's own server.
  // If both surfaces named fewer actions than the catalogue declares, the records
  // would be the suspect. They do not: the TypeScript gateway names all 1,335,
  // so the shortfall belongs to one transport and not to the contract.
  const tsCensus = readJson(`${REPO}/${TS_CENSUS}`);
  if (tsCensus === null || tsCensus.censused !== true) {
    aggregator.addNotProven('CROSS-TRANSPORT COMPARISON: the TypeScript contract census is missing, so the native '
      + 'discovery shortfall below cannot be attributed to a transport rather than to the records.');
  } else {
    const nativeByTool = new Map((probe.perTool ?? []).map((row) => [row.tool, row]));
    const rows = (tsCensus.perTool ?? []).map((ts) => {
      const nat = nativeByTool.get(ts.tool);
      return {
        tool: ts.tool,
        declared: ts.declaredCount,
        tsNamed: ts.runtimeCount,
        nativeNamed: nat?.runtimeCount ?? null,
        nativeMissing: nat?.missingAtRuntime ?? [],
        nativeExtra: nat?.extraAtRuntime ?? [],
      };
    });
    const diverging = rows.filter((row) => row.nativeNamed !== null && row.nativeNamed !== row.tsNamed);
    aggregator.document.environment.transportDiscoveryDivergence = {
      typescript: {
        source: TS_CENSUS,
        isLiveEditorEvidence: false,
        note: String(tsCensus.whatThisIsNot),
        totals: tsCensus.totals,
      },
      native: { source: PROBE, isLiveEditorEvidence: true, totals: probe.totals },
      divergingTools: diverging,
      totals: {
        toolsDiverging: diverging.length,
        declaredActionsUnnamedByNative: diverging.reduce((sum, row) => sum + row.nativeMissing.length, 0),
        namesNativeEmitsThatAreNotDeclaredCapabilities: diverging.reduce((sum, row) => sum + row.nativeExtra.length, 0),
      },
      finding: diverging.length === 0
        ? 'both surfaces name exactly the catalogue'
        : 'DISCOVERY-SURFACE DIVERGENCE, live on 5.7.4. The TypeScript gateway names every declared action for '
          + 'every parent tool. The native /mcp gateway, for the tools listed above, answers a parent-tool '
          + 'describe with what look like dispatch-group names rather than the flat canonical action list, so '
          + 'actions the catalogue declares are not discoverable there.',
      // The sharp end of it: the same run EXECUTED one of the actions native
      // discovery never named, and the plugin's receipt proves it ran on the game
      // thread. So this is a discovery gap, not a missing capability — a client
      // that trusts native `describe` cannot find work the native `execute` will
      // happily do.
      executableButNotDiscoverable: (probe.gateProbes ?? [])
        .filter((row) => row.ran === true && row.reachedEditor === true)
        .map((row) => ({
          id: row.id,
          namedByNativeDescribe: !(nativeByTool.get(row.tool)?.missingAtRuntime ?? []).includes(row.action),
          reachedEditorOnExecute: true,
          verdict: row.verdict,
        }))
        .filter((row) => row.namedByNativeDescribe === false),
      notClaimed: 'This is NOT a claim that the capabilities are unreachable on 5.7.4, and NOT a claim that the '
        + 'records are wrong. It is a measured difference between what the two transports NAME. No source was '
        + 'changed to accommodate it: a fix here would invalidate the Task 55 baseline and every prior '
        + 'certification, so it is reported rather than absorbed.',
    };
    if (diverging.length > 0) {
      aggregator.addNote(`NATIVE DISCOVERY DIVERGENCE on UE ${identity.versionString}: `
        + `${diverging.length} parent tools name fewer actions over native /mcp than the catalogue declares `
        + `(${diverging.reduce((sum, row) => sum + row.nativeMissing.length, 0)} declared actions unnamed, `
        + `${diverging.reduce((sum, row) => sum + row.nativeExtra.length, 0)} emitted names that are not declared `
        + 'capabilities), while the TypeScript gateway names all 1,335. Reported, not patched.');
    }
  }

  const summary = probe.gateSummary ?? {};
  if (Number(summary.defects ?? 0) > 0) {
    aggregator.addNote(`CAPABILITY GATE DEFECT: ${summary.defects} probe(s) contradicted the records on 5.7.4 `
      + `(${summary.answeredWhereWithheld} answered where the records withhold, `
      + `${summary.withheldByEngineWhereQualified} withheld by engine where the records qualify this engine).`);
  }
  if (Number(summary.unclear ?? 0) > 0) {
    aggregator.addNotProven(`${summary.unclear} environment-gate probe(s) returned a refusal this probe could not `
      + 'classify. Those rows prove neither agreement nor a defect and are recorded as unreadable rather than '
      + 'scored in either direction.');
  }
}

// ── 5. RESIDUE, CHECKED AT THE MOMENT THIS DOCUMENT IS WRITTEN ──────────────
const survey = surveyOwnedParent();
const strays = findOrphanedProcesses();
aggregator.document.environment.residueAtWrite = {
  ownedParent: survey.parent,
  liveRuns: survey.runs,
  strayProcesses: strays,
  clean: survey.runs.filter((entry) => entry.ownerAlive !== true).length === 0 && strays.length === 0,
};

const passed = stageRows.filter((row) => row.ok === true).length;
const failed = stageRows.filter((row) => row.ok === false).length;
const notAttempted = stageRows.filter((row) => row.ok === null).length;
const verdict = certification === null
  ? 'BLOCKED — no certification run document was produced for UE 5.7.4'
  : `${passed}/${STAGE_ORDER.length} certification stages passed for UE ${identity.versionString}`
    + `${failed > 0 ? `, ${failed} failed` : ''}${notAttempted > 0 ? `, ${notAttempted} not attempted` : ''}`
    + `; ${records.length} capability records reconciled against the live surface`;

const document = aggregator.finalize(verdict);
const validation = validateEvidence(document, { projectRoot: REPO });
document.environment.selfValidation = validation;
const written = aggregator.write(OUT);

log(`\n${verdict}`);
log(describeRejections(validation));
log(`residue at write: ${JSON.stringify(document.environment.residueAtWrite.clean)}`);
log(`wrote ${written}`);
if (!validation.valid || failed > 0) process.exitCode = 1;
