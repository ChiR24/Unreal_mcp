#!/usr/bin/env node
// Task 60 — compose the UE 5.8 certification record.
//
// The run this document reports did not reach a verdict on the product. It
// stopped at stage 3 of 20 because the plugin does not COMPILE against the 5.8
// engine, and the honest shape of that is not "certification failed" — it is a
// blocked certification whose seventeen unreached stages are recorded as
// unmeasured rather than as failures.
//
// THE DISTINCTION THIS FILE EXISTS TO HOLD:
//
//   a stage that RAN and said no is a result. `package.plugin` is one: RunUAT
//   compiled 45 translation units against 5.8 headers and clang refused.
//
//   a stage that never ran has NO result, in either direction. `drivers.stdio`
//   is not "failing on 5.8" — nobody has ever run it there. Writing it as a
//   failure would overstate what is known just as badly as writing it as a pass,
//   and the aggregate in Task 62 has to be able to tell the two apart.
//
// WHY NO FIX IS APPLIED HERE. The compile break is real and its remedy is known
// (Epic ships a documented opt-out for the larger half of it). Applying it would
// edit plugin C++ that Task 52's 20/20 certification of 5.7.4 and Task 55's
// 18-gate release-candidate chain were both recorded against. That trade —
// a 5.8 row bought with the invalidation of the two strongest existing records —
// is the orchestrator's to make, not this lane's. So the fix is SPECIFIED, in
// full, and deliberately left unapplied.
//
// Run: node scripts/qa/task60-evidence.mjs [--certification FILE] [--out FILE]

import { execFileSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import { existsSync, readFileSync, statSync } from 'node:fs';
import { createServer } from 'node:net';

import { buildEngineInventory, formatInventoryTable } from '../../tests/unit/task-52/engine-inventory.mjs';
import { readEngineIdentity } from '../../tests/unit/task-52/engine-identity.mjs';
import {
  buildProfileMatrix, collectNativeGates, defineProfile, evaluateNativeFeatures,
} from '../../tests/unit/task-52/profile-matrix.mjs';
import { surveyOwnedParent, findOrphanedProcesses } from '../../tests/unit/task-52/disposable-project.mjs';
import { EvidenceAggregator } from '../../tests/unit/task-50/evidence-aggregator.mjs';
import { describeRejections, validateEvidence } from '../../tests/unit/task-50/evidence-validator.mjs';
import { observeListener, observeProcess, observeTree } from '../../tests/unit/task-50/state-oracles.mjs';

const argOf = (name, fallback) => {
  const index = process.argv.indexOf(name);
  return index >= 0 ? process.argv[index + 1] : fallback;
};

const REPO = process.cwd();
const ENGINE_ROOT = '/data/UnrealEngine-5.8.0-preview-1';
const CERTIFICATION = argOf('--certification', '.omo/evidence/task-60/certify-5.8-preview-1-run1.json');
const COMPILE_LOG = '.omo/evidence/task-60/ubt-compile-errors-5.8.0.log';
const OUT = argOf('--out', '.omo/evidence/task-60-pure-unreal-mcp-implementation.json');
const PLUGIN_ROOT = `${REPO}/plugins/McpAutomationBridge`;
/** The tree every one of Task 55's 18 release-candidate gates is cross-linked to. */
const TASK_55_TREE_HASH = '15ca4845052e885957db77b92ac974b8c3b471b8ca224a2a513ecc79ca1aeec0';
const log = (line) => { process.stderr.write(`${line}\n`); };

const TREE = [
  'tests/unit/task-52/engine-identity.mjs',
  'tests/unit/task-52/engine-inventory.mjs',
  'tests/unit/task-52/profile-matrix.mjs',
  'tests/unit/task-52/disposable-project.mjs',
  'tests/unit/task-52/certification-stages.mjs',
  'scripts/qa/task52-certify-engine.mjs',
  'scripts/qa/task60-capability-probe.mjs',
  'scripts/qa/task60-evidence.mjs',
];

/**
 * Re-measure the tree the way Task 55 defined it, so "the hash moved" is a
 * reading rather than a recollection: sha256 over sorted "<sha256>  <path>" of
 * git-tracked + untracked-not-ignored files, excluding .omo/ (evidence is written
 * there) and the gitignored build outputs.
 */
function measureSourceTreeHash() {
  const listed = (args) => {
    try {
      return execFileSync('git', ['-C', REPO, ...args], { encoding: 'utf8', timeout: 120_000, maxBuffer: 64 * 1024 * 1024 })
        .split('\n').filter((line) => line.length > 0);
    } catch {
      return [];
    }
  };
  const paths = [...new Set([...listed(['ls-files']), ...listed(['ls-files', '--others', '--exclude-standard'])])]
    .filter((path) => !path.startsWith('.omo/'))
    .sort();
  const lines = [];
  let bytes = 0;
  for (const path of paths) {
    let contents;
    try {
      contents = readFileSync(`${REPO}/${path}`);
    } catch {
      continue; // a directory entry or a file removed mid-scan is not an input
    }
    bytes += contents.length;
    lines.push(`${createHash('sha256').update(contents).digest('hex')}  ${path}`);
  }
  lines.sort();
  const measuredNow = createHash('sha256').update(`${lines.join('\n')}\n`).digest('hex');
  return {
    measuredNow,
    measuredFileCount: lines.length,
    measuredTotalBytes: bytes,
    matchesTask55: measuredNow === TASK_55_TREE_HASH,
  };
}

/** The 20 stages Task 52 defines, in order, with what this run actually did. */
const STAGE_ORDER = [
  'inventory.resolve', 'workspace.open', 'package.plugin', 'project.materialize',
  'build.editorTarget', 'build.binaryFresh', 'ports.stillFree',
  'automation.startedEqualsCompleted', 'automation.noFailures',
  'editor.nativeListening', 'editor.bridgeListening', 'editor.alive',
  'dist.fresh', 'drivers.native', 'drivers.stdio', 'drivers.corpusSubset',
  'cleanup.editor', 'cleanup.workspace', 'cleanup.agrees', 'tree.stable',
];

const NOT_REACHED = 'the run stopped at package.plugin; this stage was never attempted and no result '
  + 'may be inferred for it in either direction';

const aggregator = new EvidenceAggregator({
  task: 60,
  title: 'Certify the disposable package and live corpus on UE 5.8 Preview 1',
  plan: '.omo/plans/pure-unreal-mcp-implementation.md',
  kind: 'wave-7 engine lane',
});
aggregator.recordTree(TREE);

// ── 1. WHAT ENGINE IS ACTUALLY AT THAT PATH ─────────────────────────────────
// Read, not assumed, and reported with its own contradiction intact. The plan
// calls this root "5.8 Preview 1" and instructs that evidence never call it
// stable. The engine disagrees with the folder: its own tag is an EXACT
// `5.8.0-release`. Neither label is repeated here as though the other had not
// been read — both are recorded, and the run claims only what all sources share.
const identity = readEngineIdentity({ root: ENGINE_ROOT });
const inventory = buildEngineInventory({ searchDirs: ['/data'] });
let headCommit = null;
try {
  headCommit = execFileSync('git', ['-C', ENGINE_ROOT, 'rev-parse', 'HEAD'], { encoding: 'utf8', timeout: 15_000 }).trim();
} catch { headCommit = null; }

aggregator.document.engine = {
  root: ENGINE_ROOT,
  versionString: identity.versionString,
  resolvedFrom: 'Engine/Build/Build.version',
  buildVersionSha256: identity.sources.buildVersion.sha256,
  versionHeaderAgrees: identity.sources.versionHeader.agrees,
  branch: identity.branch,
  changelist: identity.changelist,
  compatibleChangelist: identity.compatibleChangelist,
  isPromotedBuild: identity.isPromotedBuild,
  isLicenseeVersion: identity.isLicenseeVersion,
  gitDescribe: identity.sources.gitDescribe.raw,
  gitHeadCommit: headCommit,
  hasCompiledEditor: identity.toolchain.hasCompiledEditor,

  // The label question, answered without picking a side the sources do not support.
  channelLabelling: {
    folderLabel: identity.channel.folderLabel,
    engineTag: identity.channel.tag,
    engineTagIsExact: identity.channel.exactTag,
    folderLabelContradicted: identity.channel.folderLabelContradicted,
    isPromotedStableBuild: false,
    statedAs: 'UE 5.8.0 at a path labelled "preview-1", whose own git tag is exactly "5.8.0-release", '
      + 'built from source with Changelist 0 and IsPromotedBuild 0.',
    notClaimed: [
      'NOT claimed to be a promoted stable 5.8 release: Changelist is 0 and IsPromotedBuild is 0, '
        + 'which is a source build from a git checkout, not an Epic promoted binary release.',
      'NOT claimed to be Preview 1 on the engine\'s own authority: the folder says preview-1 and the '
        + 'engine\'s tag contradicts it. The label is the operator\'s, and is reported as the operator\'s.',
      'NOTHING here is generalised to a final/stable 5.8 release.',
    ],
    whyItMatters: 'The records declare max = 5.8.0 channel "preview" preview 1, but compareEngineVersions '
      + 'compares only major/minor/patch and ignores channel, so a 5.8.0-release tree and a 5.8.0-preview-1 '
      + 'tree are indistinguishable to the availability model. The distinction survives only in this field.',
  },
};
aggregator.document.environment.engineInventory = {
  table: formatInventoryTable(inventory),
  runnableRoots: inventory.certifiable.map((entry) => ({ minor: entry.minorKey, root: entry.preferredRoot })),
  folderNameContradictions: inventory.folderNameContradictions,
};

// ── 2. THE RUN ──────────────────────────────────────────────────────────────
const certification = existsSync(CERTIFICATION) ? JSON.parse(readFileSync(CERTIFICATION, 'utf8')) : null;
const ran = new Map((certification?.environment?.stages ?? []).map((entry) => [entry.name, entry]));

aggregator.document.environment.stages = STAGE_ORDER.map((name) => {
  const entry = ran.get(name);
  if (entry !== undefined) {
    return { stage: name, outcome: entry.ok === true ? 'PASSED' : 'FAILED', detail: entry.detail };
  }
  if (name === 'cleanup.editor') {
    return {
      stage: name,
      outcome: 'NOT_APPLICABLE',
      detail: 'no editor was ever launched, so there was no editor process to release. Recorded as '
        + 'not-applicable rather than passed: a cleanup that had nothing to clean is not evidence that '
        + 'editor teardown works on this engine.',
    };
  }
  return { stage: name, outcome: 'NOT_REACHED', detail: NOT_REACHED };
});
const tally = aggregator.document.environment.stages.reduce((counts, row) => (
  { ...counts, [row.outcome]: (counts[row.outcome] ?? 0) + 1 }
), /** @type {Record<string, number>} */ ({}));
aggregator.document.environment.stageTally = tally;

if (certification !== null) {
  aggregator.document.commands = certification.commands ?? [];
  aggregator.document.observations = certification.observations ?? [];
  aggregator.document.cleanup = certification.cleanup ?? [];
  aggregator.document.environment.cleanupAgreement = certification.environment?.cleanupAgreement ?? null;
  aggregator.document.environment.workspace = certification.environment?.workspace ?? null;
  aggregator.document.environment.treeStability = certification.environment?.treeStability ?? null;
}

// ── 3. WHY IT STOPPED ───────────────────────────────────────────────────────
// Two independent UE 5.8 API changes, told apart because their remedies differ.
const compileLogText = existsSync(`${REPO}/${COMPILE_LOG}`) ? readFileSync(`${REPO}/${COMPILE_LOG}`, 'utf8') : '';
const errorLines = compileLogText.split('\n').filter((line) => / error: /u.test(line));
const affectedFiles = [...new Set(errorLines
  .map((line) => /Source\/McpAutomationBridge\/[^:]+/u.exec(line))
  .filter((match) => match !== null)
  .map((match) => String(match[0])))].sort();

aggregator.document.environment.compatibility = {
  verdict: 'PLUGIN_DOES_NOT_COMPILE_AGAINST_UE_5_8',
  stage: 'package.plugin',
  tool: 'RunUAT BuildPlugin -> UnrealBuildTool',
  exitCode: 6,
  unrealBuildToolResult: 'Failed (OtherCompilationError)',
  errorsEmitted: errorLines.length,
  errorsTruncated: true,
  truncationNote: 'clang stopped at its -ferror-limit ("too many errors emitted, stopping now"), and the '
    + 'failing translation units aborted the rest of the module. The emitted count is a FLOOR, not a total: '
    + 'files that never compiled may hold further breaks that only a fixed tree can reveal.',
  filesWithErrors: affectedFiles.length,
  affectedFiles,
  preservedLog: COMPILE_LOG,

  rootCauses: [
    {
      id: 'BREAK-1',
      title: 'FJsonObject::Values changed its key type in UE 5.8',
      errorsAttributed: 29,
      was: 'UE 5.7.4  Engine/Source/Runtime/Json/Public/Dom/JsonObject.h: TMap<FString, TSharedPtr<FJsonValue>> Values;',
      now: 'UE 5.8.0  same header: TMap<FStringType, TSharedPtr<FJsonValue>> Values, where the active branch '
        + 'declares `using FStringType = UE::FSharedString;` (UE::TSharedString<char16_t>).',
      symptoms: [
        '-Werror,-Wrange-loop-construct: `for (const TPair<FString, TSharedPtr<FJsonValue>>& P : Obj->Values)` '
          + 'now binds a temporary, because the element type is TTuple<UE::TSharedString<char16_t>, ...>.',
        "no member named 'Equals' in 'UE::TSharedString<char16_t>'",
        "no matching member function for call to 'GetKeys' / 'Find' / 'Add'",
        'no viable overloaded operator[] for TMap<FStringType, TSharedPtr<FJsonValue>>',
      ],
      epicGuidance: 'JsonObject.h documents the transition and ships an opt-out verbatim: "To temporarily go '
        + 'back to the old FString interface, define UE_JSONOBJECT_LEGACY_STRING_KEYS=1 in your build." The '
        + 'same comment warns the flag "will be removed" in a future release.',
      minimalFix: 'ONE line in plugins/McpAutomationBridge/Source/McpAutomationBridge/McpAutomationBridge.Build.cs, '
        + 'gated to UE >= 5.8: PublicDefinitions.Add("UE_JSONOBJECT_LEGACY_STRING_KEYS=1");',
      minimalFixIsStopgap: true,
      durableFix: 'Migrate the ~20 listed files to FStringType/FStringView keys. That is the only fix that '
        + 'survives the flag being removed, and it is a real refactor of the native JSON surface, not a define.',
    },
    {
      id: 'BREAK-2',
      title: 'UUserDefinedEnum::SetEnums gained three parameters in UE 5.8',
      errorsAttributed: 6,
      was: 'SetEnums(TArray<TPair<FName,int64>>& InNames, ECppForm InCppForm)',
      now: 'SetEnums(TArray<TPair<FName,int64>>& InNames, ECppForm InCppForm, UEnum::EUnderlyingType InUnderlyingType, '
        + 'EEnumFlags InFlags, EAddMaxKeyIfMissing bAddMaxKeyIfMissing)  [Engine/Classes/Engine/UserDefinedEnum.h:70]',
      symptoms: ['error: too few arguments to function call, expected 5, have 2'],
      epicGuidance: 'None. There is no opt-out for this one.',
      minimalFix: 'Version-gate the 6 call sites (`Enum->SetEnums(Names, Enum->GetCppForm())`) in '
        + 'Private/Domains/AssetWorkflow/Enums/LifecycleEnums.cpp (1) and Private/Domains/AssetWorkflow/Enums/Values.cpp (5) '
        + 'behind #if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 8, passing the three new arguments.',
      minimalFixIsStopgap: false,
      durableFix: 'Same as the minimal fix; the added parameters have no pre-5.8 equivalent, so a gate is the fix.',
    },
  ],

  fixApplied: false,
  whyNotApplied: 'The plan states that any compatibility fix returns to source and invalidates affected later '
    + 'evidence, and this task was instructed to stop and report rather than make that trade unilaterally. '
    + 'No plugin source, build script or generated artifact was modified by this task.',
  wouldNotFixBySpeculation: 'Neither remedy above has been compiled. They are derived from the exact diagnostics '
    + 'and the engine headers, and they address every error clang emitted before it gave up — but because that '
    + 'list is truncated, nobody may claim they are SUFFICIENT until a tree carrying them builds.',
};

// ── 4. WHAT A FIX WOULD COST ────────────────────────────────────────────────
aggregator.document.environment.evidenceAtRisk = {
  note: 'Recorded so the trade can be priced before it is made, not discovered afterwards.',
  wouldChange: [
    'plugins/McpAutomationBridge/Source/McpAutomationBridge/McpAutomationBridge.Build.cs (BREAK-1)',
    'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/AssetWorkflow/Enums/LifecycleEnums.cpp (BREAK-2)',
    'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/AssetWorkflow/Enums/Values.cpp (BREAK-2)',
    'plus ~20 files under Private/{MCP,Core/Security,Domains,Foundation,Tests} if the durable BREAK-1 fix is taken',
  ],
  noneArePreserved: 'None of these paths is among the 24 preserved paths, so a fix would not disturb the '
    + 'preserved dirty work. Verified against .omo/evidence/preservation/preserved-24-baseline.sha256.',
  invalidates: [
    {
      evidence: '.omo/evidence/task-52-pure-unreal-mcp-implementation.json (and .omo/evidence/task-52/certify-5.7-run2.json)',
      what: 'the 20/20 live certification of UE 5.7.4',
      why: 'its binaryFreshness, package sha256 and preserved archive identify a plugin built from the CURRENT '
        + 'plugin sources. Editing plugin C++ means the certified bytes no longer correspond to the tree, and '
        + 'the 5.7.4 certification would have to be re-run to say anything about the fixed tree.',
    },
    {
      evidence: '.omo/evidence/task-55-pure-unreal-mcp-implementation.json',
      what: 'the 18-gate release-candidate baseline',
      why: 'all 18 gates are cross-linked to sourceTreeHash 15ca4845052e885957db77b92ac974b8c3b471b8ca224a2a513ecc79ca1aeec0 '
        + '("everyGateCrossLinksThisHash": true), defined over git-tracked + untracked-not-ignored files outside .omo/. '
        + 'Touching plugin source moves that hash and unlinks every gate from the tree it was proven on.',
    },
    {
      evidence: 'plugin source-contract tests (tests/unit/plugin/*contracts.test.ts)',
      what: 'the 250 pure-line-per-file ceiling and <=25-files-per-folder gates',
      why: 'the durable BREAK-1 fix edits ~20 files and could push one over the pure-line ceiling; two plugin '
        + 'folders are already at exactly 25 files, so any new file must go in a subdirectory.',
    },
  ],
  alreadyMoved: {
    task55Hash: '15ca4845052e885957db77b92ac974b8c3b471b8ca224a2a513ecc79ca1aeec0',
    task55FileCount: 2818,
    ...measureSourceTreeHash(),
    note: 'Stated because Task 55\'s hash had already moved BEFORE this lane ran, through ADDITIVE QA tooling '
      + 'from sibling wave-7 lanes (tests/unit/task-56, task-57, task-61, scripts/qa/task5{6,1}-*). That is a '
      + 'different kind of movement from a product source edit and the two must not be conflated: no product '
      + 'source under src/ or plugins/ has changed, which is why the 5.7.4 certification still describes the '
      + 'plugin bytes in this tree.',
    thisTasksOwnAdditions: [
      'scripts/qa/task60-capability-probe.mjs',
      'scripts/qa/task60-evidence.mjs',
      '(both additive QA tooling; neither is product source nor a generated artifact)',
    ],
  },
};

// ── 5. CAPABILITY GAPS — offline expectation, and an honest hole where the
//       runtime census should be ────────────────────────────────────────────
const records = JSON.parse(readFileSync(
  `${REPO}/src/tools/catalog/capabilities/generated/canonical-registry.generated.json`, 'utf8',
)).records;
const allPlugins = [...new Set(records.flatMap((entry) => entry.availability?.requiredPlugins ?? []))].sort();
const nativeGates = collectNativeGates({ pluginRoot: PLUGIN_ROOT });
const matrix = buildProfileMatrix({
  records,
  profiles: [
    defineProfile({ id: 'ue5.8-all-plugins-edit-native', engine: { major: 5, minor: 8, patch: 0 }, plugins: allPlugins }),
    defineProfile({ id: 'ue5.8-no-optional-plugins-edit-native', engine: { major: 5, minor: 8, patch: 0 }, plugins: [] }),
    defineProfile({ id: 'ue5.7-all-plugins-edit-native', engine: { major: 5, minor: 7, patch: 4 }, plugins: allPlugins }),
  ],
  nativeGates,
});
const nativeFor58 = evaluateNativeFeatures(
  defineProfile({ id: '5.8', engine: { major: 5, minor: 8, patch: 0 }, plugins: allPlugins }), nativeGates,
);

aggregator.document.environment.capabilityGaps = {
  measuredAtRuntime: false,
  whyNot: 'No 5.8 editor was ever launched, because the plugin did not compile. Runtime capability discovery '
    + 'was prepared (scripts/qa/task60-capability-probe.mjs) and never run against 5.8. Every figure below is '
    + 'the OFFLINE expectation computed from the records and the plugin sources; none of it is a live reading, '
    + 'and none of it may be reported as one.',
  probeReady: 'scripts/qa/task60-capability-probe.mjs',
  offlineExpectation: {
    capabilityRecords: records.length,
    engineVersionGatesOn58: {
      belowMin: matrix.rows[0].byGate.ENGINE_BELOW_MIN ?? 0,
      aboveMax: matrix.rows[0].byGate.ENGINE_ABOVE_MAX ?? 0,
      finding: 'ZERO capabilities are withheld by engine version on 5.8.0. 1334 of 1335 records declare '
        + 'min 5.0.0 and one declares min 5.7.0, so every 5.1+/5.3+/5.7+ gate is open. All 1335 declare '
        + 'max 5.8.0, so every record sits exactly AT its declared upper boundary — satisfied, not exceeded.',
    },
    boundaryNote: 'Every record declares max {major:5,minor:8,patch:0,channel:"preview",preview:1}. '
      + 'compareEngineVersions ignores channel/preview, so this 5.8.0-release tree compares EQUAL to the '
      + 'declared preview-1 ceiling and is not gated. If the project intends the ceiling to mean '
      + '"preview 1 specifically", the availability model cannot currently express that.',
    rows: matrix.rows.map(({ unavailableIds, ...row }) => ({ ...row, unavailableSample: unavailableIds.slice(0, 5) })),
    nativeGateCensus: {
      conditionsFound: nativeGates.conditions.length,
      distinctConditions: nativeGates.distinctConditions.length,
      compiledOn58: nativeFor58.compiledCount,
      excludedOn58: nativeFor58.excludedCount,
      undecidedOn58: nativeFor58.undecidedCount,
    },
    plugingatedExpectation: 'The disposable project enables no OPTIONAL UE plugins of its own, so the '
      + 'plugin-gated records (EditorScriptingUtilities 349, LevelSequenceEditor 81, GeometryScripting 76, '
      + 'UMG 65, Niagara 55, PCG 30, ...) would have been reconciled against whatever the engine enables by '
      + 'default — which is exactly the reconciliation that can only be done live, and was not.',
    editorStateExpectation: '12 records declare pie/simulate only and must be withheld in an `edit` session; '
      + '1280 declare edit, 25 edit+pie, 18 edit+pie+simulate. Unverified on 5.8.',
  },
  reconciliation: 'NOT PERFORMED. A reconciliation needs two sides and only the offline side exists for 5.8. '
    + 'No capability is claimed available on 5.8, and no capability is claimed withheld on 5.8.',
};

// ── 6. RESIDUE, at the moment of writing ────────────────────────────────────
const survey = surveyOwnedParent();
const strays = findOrphanedProcesses();
aggregator.document.environment.residueAtWrite = {
  ownedParent: survey.parent,
  liveRuns: survey.runs,
  strayProcesses: strays,
  clean: survey.runs.filter((entry) => entry.ownerAlive !== true).length === 0 && strays.length === 0,
};

// ── 7. ARTIFACTS the reader can re-hash, and CONTROLS proving the oracles see ─
for (const path of [COMPILE_LOG, CERTIFICATION]) {
  if (!existsSync(`${REPO}/${path}`)) continue;
  aggregator.recordArtifact({
    path,
    inputsNewest: 'plugins/McpAutomationBridge/Source/McpAutomationBridge',
    inputsNewestAtMs: statSync(`${REPO}/plugins/McpAutomationBridge/Source/McpAutomationBridge`).mtimeMs,
  });
}
// Both polarities on EVERY mechanism: an oracle that only ever reports "absent"
// satisfies every absence claim in this document without being able to see.
//
// The port oracle needs this most, and is the one a blocked run cannot supply by
// accident. Because no editor ever bound a port, `procfs:net-tcp` recorded four
// absent readings and no present one — and those four absent readings are exactly
// what underwrites "the three owned ports were released". So a REAL listener is
// opened here and read through the same oracle, which is the only thing that
// distinguishes "the ports are gone" from "this reader cannot see ports at all".
aggregator.addObservation(observeTree({ root: REPO, kind: 'owned-workspace' }), { phase: 'control', id: 'obs-tree-control-present' });
aggregator.addObservation(observeProcess({ pid: process.pid }), { phase: 'control', id: 'obs-process-control-present' });
aggregator.addObservation(observeProcess({ pid: 4_194_303 }), { phase: 'control', id: 'obs-process-control-absent' });

const controlPort = await new Promise((settle, fail) => {
  const server = createServer();
  server.on('error', fail);
  server.listen(0, '127.0.0.1', () => {
    const address = server.address();
    settle({ port: typeof address === 'object' && address !== null ? address.port : 0, server });
  });
});
aggregator.addObservation(observeListener({ port: controlPort.port }), { phase: 'control', id: 'obs-listener-control-present' });
await new Promise((settle) => { controlPort.server.close(() => settle(null)); });
aggregator.document.environment.portOracleControl = {
  port: controlPort.port,
  note: 'A real loopback listener was opened, read through procfs:net-tcp, and closed. This proves the oracle '
    + 'that reported the three owned ports absent is capable of reporting a port present.',
};

// ── 8. WHAT THIS DOCUMENT DOES NOT PROVE ────────────────────────────────────
for (const entry of [
  'LIVE CERTIFICATION ON UE 5.8: no 5.8 editor was launched. build.editorTarget, the C++ automation suite, '
    + 'both transports, the corpus subset, the security/runtime subset and editor cleanup are all UNMEASURED '
    + 'on this engine. 13 of 20 stages were never attempted and one (cleanup.editor) had nothing to act on.',
  'PLUGIN COMPATIBILITY WITH UE 5.8 IS DISPROVEN ONLY AT COMPILE TIME. This document proves the plugin does '
    + 'not build against 5.8.0 headers today. It says nothing about whether a fixed plugin would LOAD, bind '
    + 'its transports or behave correctly there — those questions are downstream of a compile that has not happened.',
  'THE PROPOSED FIXES ARE UNVERIFIED. They were derived from the diagnostics and the engine headers and were '
    + 'deliberately not applied. Because clang truncated its error list, they are necessary but not provably sufficient.',
  'NO CAPABILITY DELTA FOR 5.8 IS CLAIMED IN EITHER DIRECTION. Nothing is asserted available and nothing is '
    + 'asserted withheld on this engine, because no capability was ever queried from a running 5.8 editor.',
  'THIS IS NOT A STATEMENT ABOUT A FINAL/STABLE UE 5.8 RELEASE, nor about the tree the folder name calls '
    + '"preview-1" being Preview 1. See engine.channelLabelling for what the sources actually support.',
]) aggregator.addNotProven(entry);

aggregator.document.environment.blocked = [
  'UE 5.8 certification is BLOCKED at package.plugin: the McpAutomationBridge plugin does not compile against '
  + 'UE 5.8.0 headers. Two independent engine API changes (FJsonObject key type; UUserDefinedEnum::SetEnums '
  + 'signature) produce at least 35 errors across at least 20 files. A source fix is specified and NOT applied, '
  + 'because it would invalidate Task 52\'s 5.7.4 certification and Task 55\'s 18-gate baseline.',
];

const verdict = 'BLOCKED — UE 5.8.0 is installed at /data/UnrealEngine-5.8.0-preview-1, is correctly identified '
  + 'from Engine/Build/Build.version (corroborated by Version.h) and has a compiled UnrealEditor-Cmd, so the '
  + 'orchestrator resolved it and opened an owned workspace on unique ports. Certification then stopped at stage '
  + '3 of 20: RunUAT BuildPlugin failed (exit 6, OtherCompilationError) because the plugin does not compile '
  + 'against UE 5.8 headers. 5 stages passed, 1 failed, 13 were never reached and 1 was not applicable. All four '
  + 'owned resources were released and independently verified. No source fix was applied: the remedy is specified '
  + 'in environment.compatibility.rootCauses and its cost in environment.evidenceAtRisk, for the orchestrator to '
  + 'decide. Nothing here is claimed about a stable 5.8 release.';

const document = aggregator.finalize(verdict);
const validation = validateEvidence(document, { projectRoot: REPO });
document.environment.selfValidation = validation;
const written = aggregator.write(OUT);

log(describeRejections(validation));
log(`stage tally: ${JSON.stringify(tally)}`);
log(`residue at write: clean=${String(document.environment.residueAtWrite.clean)}`);
log(`wrote ${written}`);
if (!validation.valid) process.exitCode = 1;
