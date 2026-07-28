// @ts-check
// tests/unit/task-57/unbuilt-engine-blocker.mjs
// Tasks 57 and 58 — an engine that is INSTALLED, correctly identified, and whose
// editor target was never finished building.
//
// One module, two records: /data/UnrealEngine-5.3.2 and /data/UnrealEngine-5.5.4
// are the same finding against two roots, and each task still emits its own
// evidence file. Nothing here is shared with either subject's engine — the roots
// are read independently and never stand in for one another.
//
// THREE BLOCKER CLASSES NOW EXIST IN THIS PLAN, AND TASK 62 SUMS THEM. Merging
// any two of them destroys the only information the operator needs:
//
//   ABSENT               Task 61. The minor is not on this host at all
//                        (MINOR_NOT_INSTALLED). Remediation: obtain and install
//                        the engine. Nothing on disk to work with.
//   PRESENT_BUT_UNBUILT  THIS FILE. The tree is on disk, states its own version
//                        in Engine/Build/Build.version, and has no
//                        Engine/Binaries/Linux/UnrealEditor-Cmd
//                        (NO_COMPILED_EDITOR). Remediation: compile the engine's
//                        editor target — hours of machine time and tens of GB,
//                        i.e. a host-capacity decision, never a code change.
//   PRESENT_UNBUILDABLE  Task 56's UE 5.0 shape, which additionally could not
//                        compile the engine's own C# build tools on this host.
//
// The three differ in what an operator must DO, so this module refuses to emit a
// remediation it did not measure: the .NET step Task 56's record requires is
// derived from THAT root's toolchain, and a root whose UnrealBuildTool and
// AutomationTool are already compiled must not inherit it.
//
// WHAT THIS MODULE NEVER DOES: install, download, compile or launch anything. It
// reads the filesystem and reports. The remediation it describes is an operator
// action, exactly as in Tasks 56 and 61.

import { join } from 'node:path';

import { existsSync, readFileSync, readdirSync } from 'node:fs';

import {
  BLOCKER_SEVERITY, BLOCKER_STATUS, BLOCKED_TASKS, READINESS, READINESS_FILES,
} from '../task-56/engine-readiness.mjs';

export { BLOCKER_SEVERITY, BLOCKER_STATUS, BLOCKED_TASKS, READINESS, READINESS_FILES };

/**
 * The blocker classes, spelled so a record can never be filed under the wrong one.
 *
 * `ABSENT` is quoted here rather than imported from Task 61 for the same reason
 * Task 56 quotes the status vocabulary: the two lanes must AGREE, and a shared
 * import would make agreement automatic and therefore unverified. The test file
 * cross-checks this constant against Task 61's own `ABSENCE_REASON`.
 */
export const BLOCKER_CLASSES = Object.freeze({
  ABSENT: 'ABSENT',
  PRESENT_BUT_UNBUILT: 'PRESENT_BUT_UNBUILT',
});

/** The resolve reason that puts a root in each class. */
export const CLASS_REASONS = Object.freeze({
  [BLOCKER_CLASSES.ABSENT]: 'MINOR_NOT_INSTALLED',
  [BLOCKER_CLASSES.PRESENT_BUT_UNBUILT]: READINESS.NO_COMPILED_EDITOR,
});

// ─────────────────────────── the three-state stage table ─────────────────────

/**
 * Task 52's twenty certification stages, in the order its orchestrator runs them.
 * The list is frozen here so a record cannot silently describe nineteen stages
 * and call the missing one covered.
 */
export const TASK52_STAGES = Object.freeze([
  'inventory.resolve', 'workspace.open', 'package.plugin', 'project.materialize',
  'build.editorTarget', 'build.binaryFresh', 'ports.stillFree',
  'automation.startedEqualsCompleted', 'automation.noFailures',
  'editor.nativeListening', 'editor.bridgeListening', 'editor.alive', 'dist.fresh',
  'drivers.native', 'drivers.stdio', 'drivers.corpusSubset',
  'cleanup.editor', 'cleanup.workspace', 'cleanup.agrees', 'tree.stable',
]);

/**
 * THREE outcomes, not two. This is the single most important property of a
 * blocked certification record.
 *
 * A stage the run never reached is neither a pass nor a failure. Recorded as
 * FAILED it asserts the plugin was tried and broke, which is a defamation of code
 * nobody compiled; recorded as PASSED it is simply a lie. NOT_REACHED is the only
 * honest value, and `STAGE_OUTCOME_TO_BOOLEAN` maps it to `null` so an aggregator
 * that wants a boolean gets an explicit absence rather than a default.
 */
export const STAGE_OUTCOMES = Object.freeze({
  PASSED: 'PASSED',
  FAILED: 'FAILED',
  NOT_REACHED: 'NOT_REACHED',
});

/** The tri-state, machine-readable. `null` is a value here, never a missing key. */
export const STAGE_OUTCOME_TO_BOOLEAN = Object.freeze({
  [STAGE_OUTCOMES.PASSED]: true,
  [STAGE_OUTCOMES.FAILED]: false,
  [STAGE_OUTCOMES.NOT_REACHED]: null,
});

/**
 * The contract an aggregator must honour, carried in the evidence itself so it
 * does not depend on somebody having read this file.
 */
export const STAGE_OUTCOME_CONTRACT = Object.freeze({
  outcomes: Object.freeze({ ...STAGE_OUTCOME_TO_BOOLEAN }),
  rule: 'NOT_REACHED is neither a pass nor a failure and maps to null. Coercing it to false would assert that '
    + 'the plugin was exercised and failed on this engine; coercing it to true would be a fabrication. An '
    + 'aggregation that cannot represent null must exclude these stages, never default them.',
});

/**
 * The sentence every unattempted stage carries, so the reason is attached to the
 * stage rather than living in a summary somebody may not read.
 * @param {string} stoppedAt
 */
export const notReachedDetail = (stoppedAt) => `the run stopped at ${stoppedAt}; this stage was never attempted `
  + 'and no result may be inferred for it';

/**
 * Fold the orchestrator's own stage list into the full twenty-row table.
 *
 * The attempt document is the input; stages it does not mention were not
 * attempted. Nothing is inferred in either direction for those.
 * @param {{ attemptStages?: readonly {name: string, ok?: boolean, detail?: string|null}[],
 *   stoppedAt: string, stages?: readonly string[] }} spec
 */
export function buildStageTable(spec) {
  const attempted = new Map((spec.attemptStages ?? []).map((row) => [row.name, row]));
  return (spec.stages ?? TASK52_STAGES).map((stage) => {
    const row = attempted.get(stage);
    if (row === undefined) {
      return { stage, outcome: STAGE_OUTCOMES.NOT_REACHED, detail: notReachedDetail(spec.stoppedAt) };
    }
    return {
      stage,
      outcome: row.ok === true ? STAGE_OUTCOMES.PASSED : STAGE_OUTCOMES.FAILED,
      detail: row.detail ?? null,
    };
  });
}

/** @param {readonly {outcome: string}[]} table */
export function summarizeStageTable(table) {
  const count = (/** @type {string} */ outcome) => table.filter((row) => row.outcome === outcome).length;
  return {
    total: table.length,
    passed: count(STAGE_OUTCOMES.PASSED),
    failed: count(STAGE_OUTCOMES.FAILED),
    notReached: count(STAGE_OUTCOMES.NOT_REACHED),
    attempted: table.length - count(STAGE_OUTCOMES.NOT_REACHED),
  };
}

// ─────────────────────── how far did the editor build get? ───────────────────

/** The files that record what an editor build produced, relative to the engine root. */
export const EDITOR_BUILD_FILES = Object.freeze({
  modulesManifest: 'Engine/Binaries/Linux/UnrealEditor.modules',
  versionManifest: 'Engine/Binaries/Linux/UnrealEditor.version',
  targetReceipt: 'Engine/Binaries/Linux/UnrealEditor.target',
  binariesDir: 'Engine/Binaries/Linux',
  intermediateBuild: 'Engine/Intermediate/Build/Linux/UnrealEditor',
  generateProjectFiles: 'GenerateProjectFiles.sh',
  setup: 'Setup.sh',
  makefile: 'Makefile',
  buildScript: 'Engine/Build/BatchFiles/Linux/Build.sh',
  sourceDistribution: 'Engine/Build/SourceDistribution.txt',
  installedBuild: 'Engine/Build/InstalledBuild.txt',
});

/** How far the editor target got. Never a pass/fail — a description. */
export const BUILD_PROGRESS = Object.freeze({
  EDITOR_PRESENT: 'EDITOR_PRESENT',
  PARTIAL_MODULE_BUILD_NO_LINKED_EDITOR: 'PARTIAL_MODULE_BUILD_NO_LINKED_EDITOR',
  NO_BUILD_ARTIFACTS: 'NO_BUILD_ARTIFACTS',
  UNMEASURED: 'UNMEASURED',
});

/** Real filesystem access, injectable so the tests run with no engine present. */
export const progressIo = Object.freeze({
  /** @param {string} path */
  exists: (path) => existsSync(path),
  /** @param {string} path */
  readFile: (path) => readFileSync(path, 'utf8'),
  /** @param {string} path @returns {string[]|null} */
  listDir: (path) => {
    try {
      return readdirSync(path);
    } catch {
      return null;
    }
  },
});

/** A module library of the editor target, on Linux. */
const MODULE_LIBRARY = /^libUnrealEditor-.+\.so$/u;

/**
 * Measure what an editor build left behind.
 *
 * WHY THIS EXISTS. "no compiled UnrealEditor-Cmd" is true of a tree nobody ever
 * built and of a tree whose build ran for hours and stopped one link short. The
 * remediation is the same command either way, but the operator's estimate of what
 * it costs is not, and a record that cannot tell them apart invites "just re-run
 * it, it's probably quick".
 *
 * Every number here is a count of files that are on disk right now. Nothing is
 * inferred about WHY the build stopped — this module has no evidence about that
 * and does not pretend to.
 * @param {{ root: string, io?: typeof progressIo }} spec
 */
export function probeEditorBuildProgress(spec) {
  const io = spec.io ?? progressIo;
  const at = (/** @type {string} */ relative) => join(spec.root, relative);
  /** @type {Record<string, boolean>} */
  const files = {};
  for (const [name, relative] of Object.entries(EDITOR_BUILD_FILES)) files[name] = io.exists(at(relative));

  /** @type {number|null} */
  let declaredModules = null;
  /** @type {string|null} */
  let buildId = null;
  if (files.modulesManifest) {
    try {
      const parsed = JSON.parse(io.readFile(at(EDITOR_BUILD_FILES.modulesManifest)));
      declaredModules = typeof parsed?.Modules === 'object' && parsed.Modules !== null ? Object.keys(parsed.Modules).length : null;
      buildId = typeof parsed?.BuildId === 'string' ? parsed.BuildId : null;
    } catch {
      declaredModules = null;
    }
  }

  const entries = io.listDir(at(EDITOR_BUILD_FILES.binariesDir));
  const moduleLibraries = entries === null ? null : entries.filter((name) => MODULE_LIBRARY.test(name)).length;
  const editorExecutable = io.exists(at(READINESS_FILES.editorCmd));

  const state = editorExecutable
    ? BUILD_PROGRESS.EDITOR_PRESENT
    : moduleLibraries === null
      ? BUILD_PROGRESS.UNMEASURED
      : moduleLibraries > 0
        ? BUILD_PROGRESS.PARTIAL_MODULE_BUILD_NO_LINKED_EDITOR
        : BUILD_PROGRESS.NO_BUILD_ARTIFACTS;

  return {
    root: spec.root,
    state,
    editorExecutablePresent: editorExecutable,
    targetReceiptPresent: files.targetReceipt,
    modulesManifestPresent: files.modulesManifest,
    buildId,
    declaredModules,
    moduleLibrariesInEngineBinaries: moduleLibraries,
    declaredModulesWithoutLibrary: declaredModules === null || moduleLibraries === null
      ? null
      : Math.max(0, declaredModules - moduleLibraries),
    intermediateBuildPresent: files.intermediateBuild,
    sourceDistribution: files.sourceDistribution,
    installedBuild: files.installedBuild,
    makefilePresent: files.makefile,
    generateProjectFilesPresent: files.generateProjectFiles,
    setupPresent: files.setup,
    buildScriptPresent: files.buildScript,
    measuredBy: 'a directory listing of Engine/Binaries/Linux compared against the module list '
      + `${EDITOR_BUILD_FILES.modulesManifest} declares, plus the presence of the target receipt `
      + `${EDITOR_BUILD_FILES.targetReceipt}. Counts are files on disk; no reason for the build's state is inferred.`,
  };
}

/**
 * Describe the remediation for one root FROM WHAT WAS MEASURED THERE.
 *
 * A toolchain step is emitted only when this root's own toolchain diagnosis
 * demands it. Task 56's UE 5.0 record needs a .NET Core 3.1 SDK because that
 * root's UnrealBuildTool and AutomationTool are uncompiled and target
 * netcoreapp3.1; a root whose assemblies are already built must not inherit that
 * instruction, because following it would be an hour spent on a non-problem.
 * @param {{ root: string, versionString: string, toolchain: any, progress: any }} spec
 */
export function remediationStepsFor(spec) {
  const editorCmd = join(spec.root, READINESS_FILES.editorCmd);
  /** @type {string[]} */
  const steps = [];
  if (spec.toolchain?.bootstrappable === true) {
    steps.push(`1. No .NET work is required at ${spec.root}: its build toolchain diagnoses as `
      + `${String(spec.toolchain.reason)} (${String(spec.toolchain.detail)}). Do NOT apply the .NET SDK step from `
      + "Task 56's UE 5.0 record here; that step is specific to a root whose UnrealBuildTool and AutomationTool "
      + 'are themselves uncompiled.');
  } else {
    steps.push(`1. Repair this root's build toolchain first: ${String(spec.toolchain?.reason ?? 'UNDECIDABLE')} — `
      + `${String(spec.toolchain?.detail ?? 'no diagnosis was recorded')}. UnrealBuildTool must be able to run before `
      + 'any engine target can be compiled.');
  }
  steps.push(`2. cd ${spec.root} && ./${EDITOR_BUILD_FILES.setup} — fetch the engine's binary dependencies. This tree `
    + `${spec.progress?.sourceDistribution === true ? `carries ${EDITOR_BUILD_FILES.sourceDistribution}, so it is a source distribution and Setup.sh is the supported entry point` : 'was not observed to carry a source-distribution marker, so confirm the distribution kind before running Setup.sh'}`
    + '. If the dependencies are already present the step is a no-op; it is listed because the next one depends on it.');
  steps.push(`3. cd ${spec.root} && ./${EDITOR_BUILD_FILES.generateProjectFiles} — generate the build files. `
    + `${spec.progress?.makefilePresent === true ? 'A Makefile is already present at this root.' : `No Makefile is present at ${join(spec.root, EDITOR_BUILD_FILES.makefile)} right now, so this step is required before make can be used.`}`);
  steps.push(`4. cd ${spec.root} && make UnrealEditor  (equivalently: ${join(spec.root, EDITOR_BUILD_FILES.buildScript)} `
    + 'UnrealEditor Linux Development) — THIS is the step that links '
    + `${editorCmd}. ${describeProgress(spec.progress)}`);
  steps.push(`5. Confirm the build completed: test -x ${editorCmd} && test -f ${join(spec.root, EDITOR_BUILD_FILES.targetReceipt)}. `
    + 'UnrealBuildTool writes the target receipt when the editor target finishes, so the receipt and the executable '
    + 'appearing together is what distinguishes a completed build from the current state.');
  return steps;
}

/** One sentence about how far the build got, from counts only. @param {any} progress */
function describeProgress(progress) {
  if (progress?.state === BUILD_PROGRESS.PARTIAL_MODULE_BUILD_NO_LINKED_EDITOR) {
    return `As measured now, ${String(progress.moduleLibrariesInEngineBinaries)} of the `
      + `${String(progress.declaredModules)} modules this root's own ${EDITOR_BUILD_FILES.modulesManifest} declares `
      + `already have a library in ${EDITOR_BUILD_FILES.binariesDir}, and the target receipt is absent — so this is a `
      + 'resumed build of the remaining modules plus the final link, not a build from scratch. It is still hours of '
      + 'compilation and tens of gigabytes of disk on this host.';
  }
  if (progress?.state === BUILD_PROGRESS.NO_BUILD_ARTIFACTS) {
    return 'No editor module libraries were found at this root, so this is a full editor build from an unbuilt tree: '
      + 'a multi-hour compile requiring tens of gigabytes of additional disk.';
  }
  return 'The build state of this root could not be measured, so no estimate of the remaining work is offered.';
}

/**
 * Build the BLOCKED_EXTERNAL record for an advertised minor that is installed and
 * whose editor was never finished building.
 *
 * Carries no `pass`, `ok`, `success`, `skip`, `verdict` or `result` field at any
 * depth, for the reason Tasks 56 and 61 both state: the moment a blocker can be
 * read as a result, some aggregator sums the wrong column and an uncertified
 * minor turns green.
 * @param {{ task: number, minorKey: string, versionString: string, rootReports: readonly any[],
 *   detection: Record<string, unknown>, advertisedRange?: string, advertisedBy?: readonly string[],
 *   completedBuildReference?: Record<string, unknown>|null }} spec
 */
export function buildPresentButUnbuiltBlocker(spec) {
  const advertisedRange = spec.advertisedRange ?? '5.0-5.8';
  const blocking = spec.rootReports.filter((entry) => entry.readiness.ready !== true);
  const primary = spec.rootReports[0] ?? null;
  return {
    recordId: `blocked-external-unreal-engine-${spec.minorKey}-not-built`,
    status: BLOCKER_STATUS,
    severity: BLOCKER_SEVERITY,
    blockerClass: BLOCKER_CLASSES.PRESENT_BUT_UNBUILT,
    blockerClassNote: `${BLOCKER_CLASSES.PRESENT_BUT_UNBUILT} is NOT ${BLOCKER_CLASSES.ABSENT}. Task 61 records minors `
      + `that are not on this host (${CLASS_REASONS[BLOCKER_CLASSES.ABSENT]}) and its remediation is to install an `
      + `engine. This record's minor IS installed (${CLASS_REASONS[BLOCKER_CLASSES.PRESENT_BUT_UNBUILT]}) and its `
      + 'remediation is to compile an editor target that already has a source tree. An aggregation that merges the '
      + 'two classes reports a provisioning gap the operator has already closed, or hides one they have not.',
    blocksTasks: [...BLOCKED_TASKS],
    blocksClaims: [
      `the UE ${spec.minorKey} row of the Task 62 engine/plugin/client compatibility matrix`,
      'BEST_IN_CLASS_VERIFIED in Task 63, which additionally requires all advertised UE minors',
      `the advertised "UE ${advertisedRange}" support statement reconciled in Task 64`,
    ],
    subject: {
      kind: 'unreal-engine-minor',
      minorKey: spec.minorKey,
      versionString: spec.versionString,
      advertisedRange,
      advertisedBy: [...(spec.advertisedBy ?? [])],
      presentOnHost: true,
      presenceNote: 'this minor IS installed and identifies itself correctly from its own '
        + `${READINESS_FILES.buildVersion}. It is blocked because no root can launch an editor, which is a different `
        + 'finding from an absent minor and a different finding from a plugin defect.',
    },
    detection: spec.detection,
    unreadiness: {
      reason: READINESS.NO_COMPILED_EDITOR,
      rootsExamined: spec.rootReports.length,
      rootsAbleToHostCertification: spec.rootReports.length - blocking.length,
      roots: spec.rootReports.map((entry) => ({
        root: entry.readiness.root,
        versionString: entry.identity?.versionString ?? null,
        buildVersionFile: entry.identity?.sources?.buildVersion?.file ?? null,
        buildVersionSha256: entry.identity?.sources?.buildVersion?.sha256 ?? null,
        versionHeaderAgrees: entry.identity?.sources?.versionHeader?.agrees ?? null,
        gitDescribe: entry.identity?.sources?.gitDescribe?.raw ?? null,
        reason: entry.readiness.reason,
        detail: entry.readiness.detail,
        blockingConstraint: entry.readiness.blockingConstraint,
        toolchain: {
          reason: entry.toolchain.reason,
          detail: entry.toolchain.detail,
          bootstrappable: entry.toolchain.bootstrappable,
        },
        observedState: entry.observedState ?? null,
        editorBuildProgress: entry.progress ?? null,
      })),
      completedBuildReference: spec.completedBuildReference ?? null,
      notAPluginDefect: 'No plugin source was compiled during this task, and no packaging was attempted. The '
        + 'orchestrator refused at inventory.resolve, which runs before package.plugin, and this lane owns no UBT or '
        + 'RunUAT resource with which to probe further. Nothing in this record is evidence that the MCP Automation '
        + 'Bridge plugin builds on this minor, and nothing in it is evidence that it does not.',
      notSubstitutable: `no other installed minor is evidence for ${spec.minorKey}: engine headers, module ABI and API `
        + 'availability differ per minor, and binaries, packages and projects are never reused across minors. A '
        + 'certification that passes on a neighbouring minor says nothing about this one.',
    },
    requiredOperatorInput: {
      summary: `a completed UnrealEditor build of UE ${spec.versionString} at the root already on this host`,
      requiredFile: READINESS_FILES.editorCmd,
      requiredFileAbsolutePaths: blocking.map((entry) => join(entry.readiness.root, READINESS_FILES.editorCmd)),
      alsoRequired: [
        `${READINESS_FILES.editorCmd} must exist and be executable — it is the binary the certification launches.`,
        `${EDITOR_BUILD_FILES.targetReceipt} must be written by the same build. UnrealBuildTool emits the target `
          + 'receipt when the editor target completes, and this root carries the module manifest without it.',
        `Every module ${EDITOR_BUILD_FILES.modulesManifest} declares must have its library in `
          + `${EDITOR_BUILD_FILES.binariesDir}; a partially linked set loads no editor.`,
      ],
      handWrittenFileIsRefused: `creating ${READINESS_FILES.editorCmd} by hand does NOT satisfy this record: the `
        + 'certification launches it, waits for its ports and reads its automation log. The input is a real completed '
        + 'engine build.',
      acceptanceCommand: `node scripts/qa/task52-certify-engine.mjs --engine-version ${spec.minorKey}`,
      acceptanceCriterion: 'the acceptance command must get past its inventory.resolve stage; while that stage reports '
        + `${READINESS.NO_COMPILED_EDITOR}, this record stands.`,
    },
    remediation: {
      summary: `Compile the UnrealEditor target of UE ${spec.versionString} at ${primary === null ? 'the affected root' : primary.readiness.root}, then re-run the Task ${spec.task} certification.`,
      steps: primary === null ? [] : remediationStepsFor({
        root: primary.readiness.root,
        versionString: spec.versionString,
        toolchain: primary.toolchain,
        progress: primary.progress,
      }),
      thenReRun: [
        `6. node scripts/qa/task52-certify-engine.mjs --engine-version ${spec.minorKey} — confirm it advances past `
          + 'inventory.resolve.',
        `7. Re-run the full Task ${spec.task} certification against the now-runnable root.`,
      ],
      performedBy: 'operator',
      notPerformedHere: `Task ${spec.task} is a certification lane. Compiling an engine editor is engine provisioning `
        + '— the same class of action Task 61 places outside its scope, and a multi-hour exclusive claim on a host '
        + 'whose build resources another lane currently owns. Nothing here attempts the remediation it describes.',
      estimatedScale: 'a UE editor build is hours of compilation and tens of gigabytes of additional disk. It is a '
        + 'host-capacity decision, not a code change, and no certification run may absorb it silently.',
    },
    consequence: `Until an editor binary exists, UE ${spec.versionString} cannot be launched, exercised or certified on `
      + `this host. No ${spec.minorKey} capability, transport, security, performance or cleanup result may appear in `
      + `any matrix, readiness record or public support claim, and the advertised ${advertisedRange} range may not be `
      + 'narrowed to hide it.',
  };
}
