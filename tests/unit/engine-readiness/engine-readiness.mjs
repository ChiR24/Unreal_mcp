// @ts-check
// tests/unit/engine-readiness/engine-readiness.mjs
// Task 56 — why a PRESENT engine root can still be unable to host a certification.
//
// Task 61 records the minors that are ABSENT from this host. This file records the
// other half of the same honesty problem, and it is the half that is easy to
// mistake for success: a root that is installed, correctly identified, carrying
// the right version in its own `Engine/Build/Build.version` — and completely
// unable to run anything.
//
// THE DISTINCTION THIS FILE EXISTS TO KEEP:
//
//   installed  the tree is on disk and identifies itself. Both 5.0 roots here are.
//   buildable  RunUAT is present, so packaging can be ATTEMPTED. Both roots are.
//   bootstrappable  RunUAT can actually compile itself on THIS host. Neither is:
//              UE 5.0's UnrealBuildTool/AutomationTool target `netcoreapp3.1`, the
//              trees carry no prebuilt UBT/UAT assemblies, and the bundled .NET SDK
//              that `SetupDotnet.sh` falls back to is not in the tree either — so
//              the build lands on whatever the host has, and a modern SDK refuses.
//   runnable   a compiled `UnrealEditor-Cmd` exists. Neither root has one.
//
// `buildable` is the trap. Task 52's inventory reports it from the PRESENCE of
// `RunUAT.sh`, which is correct for what it measures and says nothing about
// whether that script can produce a binary. A reader who saw "build: yes" and
// stopped would conclude the plugin failed to compile when packaging failed. It
// did not: the engine's own C# toolchain failed to compile, hundreds of steps
// before any plugin source was read. Those are different findings with different
// owners, and this module refuses to let the second read as the first.
//
// NOTHING HERE INSTALLS, DOWNLOADS OR BUILDS AN ENGINE. It reads the filesystem
// and reports. The remediation it describes is an operator action, exactly as in
// Task 61 — a present-but-unbuilt engine is a precondition somebody must supply,
// never something a certification lane may quietly manufacture for itself.

import { join } from 'node:path';

import { realIo } from '../engine-certification/engine-identity.mjs';
import { RESOLVE_REASONS } from '../engine-certification/engine-inventory.mjs';

/**
 * The BLOCKED_EXTERNAL vocabulary, spelled here rather than imported from Task 61.
 *
 * Not duplication for its own sake: the two lanes must AGREE, and a shared import
 * would make agreement automatic and therefore unverified. These constants are
 * cross-checked against Task 61's in engine-readiness.test.ts, so a divergence
 * fails a test instead of silently producing two dialects of "blocked" that a
 * later aggregator sums into one column.
 */
export const BLOCKER_STATUS = 'BLOCKED_EXTERNAL';
export const BLOCKER_SEVERITY = 'BLOCKER';

/** The tasks an uncertifiable advertised minor blocks, from the plan's dependency edges. */
export const BLOCKED_TASKS = Object.freeze([62, 63, 64]);

/** Paths that decide whether a root can build and run, relative to the engine root. */
export const READINESS_FILES = Object.freeze({
  buildVersion: 'Engine/Build/Build.version',
  versionHeader: 'Engine/Source/Runtime/Launch/Resources/Version.h',
  runUat: 'Engine/Build/BatchFiles/RunUAT.sh',
  editorCmd: 'Engine/Binaries/Linux/UnrealEditor-Cmd',
  editorExe: 'Engine/Binaries/Linux/UnrealEditor',
  linuxBinaries: 'Engine/Binaries/Linux',
  prebuiltUbt: 'Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll',
  prebuiltUat: 'Engine/Binaries/DotNET/AutomationTool/AutomationTool.dll',
  bundledDotnet: 'Engine/Binaries/ThirdParty/DotNet',
  ubtProject: 'Engine/Source/Programs/UnrealBuildTool/UnrealBuildTool.csproj',
  uatProject: 'Engine/Source/Programs/AutomationTool/AutomationTool.csproj',
});

/** Why a root that EXISTS still cannot host a certification. */
export const READINESS = Object.freeze({
  READY: 'READY',
  NO_COMPILED_EDITOR: RESOLVE_REASONS.NO_COMPILED_EDITOR,
  NO_BUILD_TOOLCHAIN: RESOLVE_REASONS.NO_BUILD_TOOLCHAIN,
  UNIDENTIFIED: 'UNIDENTIFIED',
});

/**
 * Whether a .NET SDK can build a given target framework.
 *
 * Deliberately narrow: this answers only the question the observed failure posed.
 * `netcoreapp3.1` is out of support from .NET 5 on, and building UE 5.0's UBT/UAT
 * pair with a modern SDK fails NETSDK1151 — AutomationTool is not self-contained
 * and references a UnrealBuildTool that is. Reporting `null` for a framework this
 * function has no evidence about is the point: an unknown is not a pass.
 * @param {{ targetFramework: string|null, sdkMajorVersions: readonly number[] }} spec
 */
export function judgeDotnetSupport(spec) {
  const framework = spec.targetFramework;
  if (typeof framework !== 'string' || !/^netcoreapp\d/u.test(framework)) {
    return { supported: null, reason: 'UNKNOWN_TARGET_FRAMEWORK', detail: `no ruling is recorded for target framework ${String(framework)}` };
  }
  const required = Number(/^netcoreapp(\d+)/u.exec(framework)?.[1] ?? Number.NaN);
  const usable = spec.sdkMajorVersions.filter((major) => major === required);
  if (usable.length > 0) {
    return { supported: true, reason: 'SDK_PRESENT', detail: `an SDK matching ${spec.targetFramework} is installed` };
  }
  return {
    supported: false,
    reason: 'SDK_TOO_NEW',
    detail: `${spec.targetFramework} needs a .NET Core ${required}.x SDK; this host offers only `
      + `${spec.sdkMajorVersions.length === 0 ? 'no' : spec.sdkMajorVersions.join('/')} `
      + 'major version(s), and building UE 5.0\'s non-self-contained AutomationTool against a '
      + 'self-contained UnrealBuildTool on a modern SDK fails NETSDK1151',
  };
}

/**
 * Read the target framework a UE C# tool project declares.
 * @param {{ root: string, relative: string, io?: typeof realIo }} spec
 */
export function readTargetFramework(spec) {
  const io = spec.io ?? realIo;
  const file = join(spec.root, spec.relative);
  try {
    if (!io.exists(file)) return null;
    return /<TargetFramework>([^<]+)<\/TargetFramework>/u.exec(io.readFile(file))?.[1]?.trim() ?? null;
  } catch {
    return null;
  }
}

/**
 * Probe one engine root for the files that decide build and run capability.
 * @param {{ root: string, io?: typeof realIo }} spec
 */
export function probeEngineRoot(spec) {
  const io = spec.io ?? realIo;
  const at = (/** @type {string} */ relative) => join(spec.root, relative);
  /** @type {Record<string, { path: string, present: boolean }>} */
  const files = {};
  for (const [name, relative] of Object.entries(READINESS_FILES)) {
    files[name] = { path: at(relative), present: io.exists(at(relative)) };
  }
  return {
    root: spec.root,
    files,
    hasCompiledEditor: io.isExecutable(at(READINESS_FILES.editorCmd)),
    hasRunUat: files.runUat.present,
    hasPrebuiltUbt: files.prebuiltUbt.present,
    hasPrebuiltUat: files.prebuiltUat.present,
    hasBundledDotnet: files.bundledDotnet.present,
    ubtTargetFramework: readTargetFramework({ root: spec.root, relative: READINESS_FILES.ubtProject, io }),
    uatTargetFramework: readTargetFramework({ root: spec.root, relative: READINESS_FILES.uatProject, io }),
  };
}

/**
 * Can this root's RunUAT actually produce a package on THIS host?
 *
 * Separate from "is RunUAT present" because that is the distinction the observed
 * failure turned on. A root can hold the script, be reported buildable, and still
 * have no way to compile the tool the script runs.
 * @param {{ probe: ReturnType<typeof probeEngineRoot>, sdkMajorVersions: readonly number[] }} spec
 */
export function diagnoseBuildToolchain(spec) {
  const { probe } = spec;
  if (!probe.hasRunUat) {
    return { bootstrappable: false, reason: 'NO_RUNUAT', detail: `${READINESS_FILES.runUat} is absent`, dotnet: null };
  }
  if (probe.hasPrebuiltUat && probe.hasPrebuiltUbt) {
    return { bootstrappable: true, reason: 'PREBUILT_ASSEMBLIES', detail: 'UnrealBuildTool and AutomationTool are already compiled in the tree', dotnet: null };
  }
  const dotnet = judgeDotnetSupport({ targetFramework: probe.uatTargetFramework, sdkMajorVersions: spec.sdkMajorVersions });
  if (dotnet.supported === true) {
    return { bootstrappable: true, reason: 'SDK_CAN_COMPILE', detail: dotnet.detail, dotnet };
  }
  const missing = [
    probe.hasPrebuiltUat ? null : READINESS_FILES.prebuiltUat,
    probe.hasPrebuiltUbt ? null : READINESS_FILES.prebuiltUbt,
    probe.hasBundledDotnet ? null : READINESS_FILES.bundledDotnet,
  ].filter((entry) => entry !== null);
  return {
    bootstrappable: dotnet.supported === null ? null : false,
    reason: dotnet.supported === null ? 'UNDECIDABLE' : 'CANNOT_COMPILE_TOOLCHAIN',
    detail: `${dotnet.detail}. The tree carries no fallback: ${missing.join(', ')} ${missing.length === 1 ? 'is' : 'are'} absent, `
      + 'so SetupDotnet.sh exports a bundled SDK directory that does not exist and the build lands on the host SDK.',
    missing,
    dotnet,
  };
}

/**
 * The readiness verdict for one root.
 *
 * `runnable` is decided FIRST and independently of the toolchain, because it is
 * the binding constraint: repairing the packaging toolchain on a root with no
 * compiled editor changes nothing about whether a certification can run there.
 * @param {{ identity: any, probe: ReturnType<typeof probeEngineRoot>, toolchain: ReturnType<typeof diagnoseBuildToolchain> }} spec
 */
export function judgeCertificationReadiness(spec) {
  if (spec.identity?.usable !== true) {
    return {
      ready: false, reason: READINESS.UNIDENTIFIED, root: spec.probe.root,
      detail: `${spec.probe.root} could not be identified: ${String(spec.identity?.detail ?? 'no identity was read')}`,
      blockingConstraint: READINESS_FILES.buildVersion,
    };
  }
  if (!spec.probe.hasCompiledEditor) {
    return {
      ready: false, reason: READINESS.NO_COMPILED_EDITOR, root: spec.probe.root,
      detail: `${spec.probe.root} identifies as ${spec.identity.versionString} but has no compiled ${READINESS_FILES.editorCmd}, `
        + 'so no editor can be launched and nothing can be certified against it. This is independent of packaging: '
        + `the toolchain is separately ${spec.toolchain.bootstrappable === true ? 'usable' : `unusable (${spec.toolchain.reason})`}, `
        + 'and repairing it would still leave no editor to run.',
      blockingConstraint: READINESS_FILES.editorCmd,
    };
  }
  if (spec.toolchain.bootstrappable !== true) {
    return {
      ready: false, reason: READINESS.NO_BUILD_TOOLCHAIN, root: spec.probe.root,
      detail: `${spec.probe.root} has an editor but cannot package a plugin: ${spec.toolchain.detail}`,
      blockingConstraint: READINESS_FILES.runUat,
    };
  }
  return { ready: true, reason: READINESS.READY, root: spec.probe.root, detail: null, blockingConstraint: null };
}

/**
 * Build the BLOCKED_EXTERNAL record for an advertised minor whose roots are all
 * present and none of which can host a certification.
 *
 * Carries no `pass`, `ok`, `success`, `skip` or `verdict` field at any depth, for
 * the reason Task 61 states: the moment a blocker can be read as a result, some
 * aggregator sums the wrong column and an uncertified minor turns green.
 * @param {{ minorKey: string, versionString: string, rootReports: readonly any[],
 *   detection: Record<string, unknown>, advertisedRange?: string, advertisedBy?: readonly string[] }} spec
 */
export function buildUnbuiltRootBlocker(spec) {
  const advertisedRange = spec.advertisedRange ?? '5.0-5.8';
  const blocking = spec.rootReports.filter((entry) => entry.readiness.ready !== true);
  return {
    recordId: `blocked-external-unreal-engine-${spec.minorKey}-not-built`,
    status: BLOCKER_STATUS,
    severity: BLOCKER_SEVERITY,
    blocksTasks: [...BLOCKED_TASKS],
    blocksClaims: [
      `the UE ${spec.minorKey} row of the Task 62 engine/plugin/client compatibility matrix`,
      `BEST_IN_CLASS_VERIFIED in Task 63, which additionally requires all advertised UE minors`,
      `the advertised "UE ${advertisedRange}" support statement reconciled in Task 64`,
    ],
    subject: {
      kind: 'unreal-engine-minor',
      minorKey: spec.minorKey,
      versionString: spec.versionString,
      advertisedRange,
      advertisedBy: [...(spec.advertisedBy ?? [])],
      presentOnHost: true,
      presenceNote: 'this minor IS installed and identifies itself correctly. It is blocked because no root can '
        + 'launch an editor, which is a different finding from an absent minor and from a plugin defect.',
    },
    detection: spec.detection,
    unreadiness: {
      reason: READINESS.NO_COMPILED_EDITOR,
      rootsExamined: spec.rootReports.length,
      rootsAbleToHostCertification: spec.rootReports.length - blocking.length,
      roots: spec.rootReports.map((entry) => ({
        root: entry.readiness.root,
        versionString: entry.identity?.versionString ?? null,
        buildVersionSha256: entry.identity?.sources?.buildVersion?.sha256 ?? null,
        reason: entry.readiness.reason,
        detail: entry.readiness.detail,
        blockingConstraint: entry.readiness.blockingConstraint,
        toolchain: { reason: entry.toolchain.reason, detail: entry.toolchain.detail, bootstrappable: entry.toolchain.bootstrappable },
        observedState: entry.observedState ?? null,
      })),
      notAPluginDefect: 'No plugin source was compiled during this task. The packaging attempt failed inside the '
        + "ENGINE's own C# toolchain (UnrealBuildTool/AutomationTool), before UnrealBuildTool was ever handed a "
        + 'plugin module, so nothing here is evidence for or against the plugin building on this minor.',
      notSubstitutable: `no other installed minor is evidence for ${spec.minorKey}: engine headers, module ABI and API `
        + 'availability differ per minor, and binaries are never reused across minors.',
    },
    requiredOperatorInput: {
      summary: `a completed editor build of UE ${spec.versionString} at one of the roots already on this host`,
      requiredFile: READINESS_FILES.editorCmd,
      requiredFileAbsolutePaths: blocking.map((entry) => join(entry.readiness.root, READINESS_FILES.editorCmd)),
      alsoRequired: [
        `${READINESS_FILES.editorCmd} must exist and be executable — it is the binary the certification launches.`,
        'Building it needs a .NET Core 3.1 SDK, or the engine\'s bundled SDK restored under '
          + `${READINESS_FILES.bundledDotnet}, because UE 5.0's UnrealBuildTool and AutomationTool target netcoreapp3.1 `
          + 'and will not compile on this host\'s modern SDK alone.',
      ],
      handWrittenFileIsRefused: `creating ${READINESS_FILES.editorCmd} by hand does NOT satisfy this record: the `
        + 'certification launches it and reads its automation log. The input is a real completed engine build.',
      acceptanceCommand: `certify UE ${spec.minorKey} end to end (certification runner is not bundled in this repository)`,
      acceptanceCriterion: `the acceptance command must get past its inventory.resolve stage; while that stage reports `
        + `${READINESS.NO_COMPILED_EDITOR}, this record stands.`,
    },
    remediation: {
      summary: `Complete an editor build for UE ${spec.versionString} on this host, then re-run the Task 56 certification.`,
      steps: [
        '1. Install a .NET Core 3.1 SDK, or restore the engine\'s bundled .NET SDK at '
          + `${READINESS_FILES.bundledDotnet}, so the engine's own netcoreapp3.1 build tools can compile.`,
        '2. In the chosen root: ./Setup.sh && ./GenerateProjectFiles.sh && make — this is the step that produces '
          + `${READINESS_FILES.editorCmd}. Without it the root can attempt packaging but cannot host a certification.`,
        `3. Re-run the acceptance command and confirm it advances past inventory.resolve.`,
        '4. Re-run the full Task 56 certification against the now-runnable root.',
      ],
      performedBy: 'operator',
      notPerformedHere: 'Task 56 is a certification lane. Building an engine editor is engine provisioning — the same '
        + 'class of action Task 61 places outside its scope — so nothing here attempts the remediation it describes.',
      estimatedScale: 'a full UE 5.0 editor build from an unbuilt source tree is a multi-hour compile requiring tens of '
        + 'gigabytes of additional disk; it is not a step a certification run may absorb silently.',
    },
    consequence: `Until an editor binary exists, UE ${spec.versionString} cannot be launched, exercised or certified on `
      + `this host. No ${spec.minorKey} capability, transport, security or cleanup result may appear in any matrix, `
      + `readiness record or public support claim, and the advertised ${advertisedRange} range may not be narrowed to hide it.`,
  };
}
