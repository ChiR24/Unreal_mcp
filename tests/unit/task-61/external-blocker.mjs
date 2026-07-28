// @ts-check
// tests/unit/task-61/external-blocker.mjs
// Task 61 — the BLOCKED_EXTERNAL record for a UE minor that is not on this host.
//
// This project advertises UE 5.0-5.8. Four of those minors — 5.1, 5.2, 5.4, 5.6 —
// are not installed here, so four rows of that claim cannot be certified. The
// honest output is a blocker, not a quietly narrowed range, and not a skip.
//
// THE FAILURE MODE THIS FILE IS SHAPED AGAINST. Task 54 wired `npm audit` into CI
// at position 15 and never executed it, so a gate that would have failed was
// indistinguishable from one that passed. A blocker record is the same hazard in
// data form: the moment it carries a `pass`, an `ok` or a `verdict`, some later
// aggregator sums the wrong column and the missing minors evaporate into a green
// support matrix. So "no pass fields" is enforced mechanically, by a validator
// that REFUSES such a record, rather than by everyone remembering.
//
// WHY THE VALIDATOR IS A SEPARATE MODULE. This file builds records;
// external-blocker-validator.mjs refuses them. Neither imports the other. A
// builder that also approved its own output could only ever confirm its own
// assumptions — the same "compared the parser to itself" mistake Task 50 was
// written to stop.
//
// WHAT MAKES A RECORD FALSIFIABLE. Every one carries the command that detected the
// absence, the moment it ran, the digest of the detector's own source, and the
// digest of every `Engine/Build/Build.version` that WAS on the host at that
// moment. Install 5.1 and the host digest changes; edit the detector and the
// detector digest changes. Neither hash can be invented, because the validator
// recomputes both from the filesystem.
//
// VERSIONS COME FROM `Engine/Build/Build.version`, NEVER FROM A DIRECTORY NAME.
// `/data/UnrealEngine` carries no version in its name and contains 5.7.4, and
// `/data/UnrealEngine-5.8.0-preview-1` is tagged `5.8.0-release`. A record that
// trusted a folder name would file evidence under a minor nobody ran.

import { readFileSync } from 'node:fs';

import { treeDigestOf } from '../task-50/evidence-validator.mjs';
import { minorKeyOf } from '../task-52/engine-identity.mjs';

/** The only status a record of this kind may carry. */
export const BLOCKER_STATUS = 'BLOCKED_EXTERNAL';
/** The only severity. Not `warning`, not `info`: an uncertifiable advertised minor blocks the claim. */
export const BLOCKER_SEVERITY = 'BLOCKER';
/** Recorded so a reader knows the detector looked and got an answer, rather than declined to look. */
export const DETECTION_OUTCOME = 'ABSENCE_CONFIRMED';
/** Mirrors Task 52's `RESOLVE_REASONS.MINOR_NOT_INSTALLED` — the one absence this record describes. */
export const ABSENCE_REASON = 'MINOR_NOT_INSTALLED';

/** The tasks a missing minor blocks, from the plan's own dependency edges. */
export const BLOCKED_TASKS = Object.freeze([62, 63, 64]);

/**
 * Where the 5.0-5.8 promise is actually made. Cited by path so a reviewer can
 * check that the range being blocked is the range the product advertises, rather
 * than one this file invented to have something to block.
 */
export const ADVERTISED_RANGE = '5.0-5.8';

/**
 * `Engine/Build/Build.version` field names, spelled once. A record that named
 * them differently from the detector would describe an input that satisfies
 * nothing.
 */
export const BUILD_VERSION_FIELDS = Object.freeze(['MajorVersion', 'MinorVersion', 'PatchVersion']);

/** Relative paths a usable engine root must hold, in the detector's own words. */
export const REQUIRED_ENGINE_FILES = Object.freeze({
  buildVersion: 'Engine/Build/Build.version',
  versionHeader: 'Engine/Source/Runtime/Launch/Resources/Version.h',
  runUat: 'Engine/Build/BatchFiles/RunUAT.sh',
  editorCmd: 'Engine/Binaries/Linux/UnrealEditor-Cmd',
});

/**
 * Digest a set of `{path, sha256}` entries the way `sha256sum` prints them, so a
 * human can reproduce the number with one command. Deliberately the SAME function
 * Task 50 uses for its source tree: two digest conventions in one evidence file
 * is one convention too many.
 * @param {readonly {path: string, sha256: string}[]} entries
 */
export const digestOfFiles = (entries) => treeDigestOf(entries);

/**
 * Snapshot every engine root the inventory identified, as file digests.
 *
 * This is the falsifiable half of the record: it says what WAS on the host, so
 * "5.1 was not among them" is checkable rather than asserted. A root whose
 * `Build.version` could not be identified is carried too — an unreadable root is
 * not an absent minor, and collapsing the two would let a broken install hide
 * inside a gap.
 * @param {{ identities: readonly any[] }} inventory
 */
export function snapshotHostEngineTree(inventory) {
  const roots = [...inventory.identities]
    .map((identity) => ({
      root: identity.root,
      buildVersionFile: identity.sources.buildVersion.file,
      buildVersionSha256: identity.sources.buildVersion.sha256,
      versionString: identity.versionString,
      minorKey: identity.minorKey,
      // Task 52 spells a healthy root `OK`; that token is re-spelled here because
      // a bare "OK" sitting in a blocker is exactly the string a later reader —
      // or a grep — mistakes for a verdict about the record.
      identification: identity.reason === 'OK' ? 'IDENTIFIED' : identity.reason,
    }))
    .sort((left, right) => (left.root < right.root ? -1 : 1));
  const digestable = roots
    .filter((entry) => typeof entry.buildVersionSha256 === 'string')
    .map((entry) => ({ path: entry.buildVersionFile, sha256: entry.buildVersionSha256 }));
  return {
    roots,
    digest: digestOfFiles(digestable),
    digestedFileCount: digestable.length,
    digestConvention: 'sha256 over sorted "<sha256>  <Engine/Build/Build.version path>" lines, the shape sha256sum prints',
  };
}

/**
 * The detection context shared by every record in one run.
 *
 * `commandExitCode` is recorded, never interpreted as a verdict — and here the
 * honest value is NON-ZERO, because the detector exits 3 when an advertised minor
 * is missing. A record whose detector exited 0 would be describing a host where
 * nothing was missing.
 * @param {{ command: string, commandExitCode: number, reproducibleShellCommand: string,
 *   searchDirs: readonly string[], detectorTree: {files: readonly {path: string, sha256: string}[], sourceDigest: string},
 *   inventory: any, detectedAt?: string, now?: () => Date }} spec
 */
export function describeDetection(spec) {
  const now = spec.now ?? (() => new Date());
  return {
    command: spec.command,
    commandExitCode: spec.commandExitCode,
    commandExitCodeMeaning: 'the detector exits 3 when an advertised minor is absent; 0 would mean nothing was missing, so a blocker record can never carry it',
    reproducibleShellCommand: spec.reproducibleShellCommand,
    detectedAt: spec.detectedAt ?? now().toISOString(),
    searchDirs: [...spec.searchDirs],
    detectorTree: { files: [...spec.detectorTree.files], sourceDigest: spec.detectorTree.sourceDigest },
    hostEngineTree: snapshotHostEngineTree(spec.inventory),
    identifiedBy: `${REQUIRED_ENGINE_FILES.buildVersion}, corroborated by ${REQUIRED_ENGINE_FILES.versionHeader}; a directory name is never an input`,
    outcome: DETECTION_OUTCOME,
  };
}

/**
 * The nearest installed minors, each named with the reason it may not stand in.
 *
 * Every installed root is listed, not just the adjacent one. "5.3.2 is close to
 * 5.4" is the exact argument that would put a 5.3 binary under a 5.4 heading, and
 * an exhaustive list makes the refusal explicit for all six roots at once.
 * @param {{ minorKey: string, inventory: any }} spec
 */
function notSubstitutableBy(spec) {
  return [...spec.inventory.available]
    .map((entry) => ({
      root: entry.preferredRoot,
      versionString: entry.versionString,
      minorKey: entry.minorKey,
      reason: `${entry.versionString} is minor ${entry.minorKey}, not ${spec.minorKey}. Engine headers, module ABI and API availability differ per minor, so a package built here would be evidence about ${entry.minorKey} filed under ${spec.minorKey}.`,
    }))
    .sort((left, right) => (left.minorKey < right.minorKey ? -1 : 1));
}

/**
 * The exact operator action, expressed so a human can execute it and a machine
 * can tell when it is done.
 *
 * The `PatchVersion` constraint is `null` on purpose: any patch of the minor
 * satisfies detection, and naming one would invent a release. The tag convention
 * is quoted from a tag that really exists on this host rather than guessed.
 * @param {{ minorKey: string, minor: number, templateFile: string|null,
 *   templateSha256: string|null, observedTagExample: string|null, projectRelativeDetector: string }} spec
 */
function requiredOperatorInput(spec) {
  const engineRoot = `/data/UnrealEngine-${spec.minorKey}`;
  const tagHint = spec.observedTagExample === null
    ? 'Epic tags engine releases `<major>.<minor>.<patch>-release`'
    : `Epic tags engine releases \`<major>.<minor>.<patch>-release\` — the tag \`${spec.observedTagExample}\` is present on this host and is the shape to follow`;
  return {
    engineRoot,
    engineRootNamingNote: `the directory NAME is never read as identity — only ${REQUIRED_ENGINE_FILES.buildVersion} is. Any directory directly under a scanned search dir works; this name is recommended only so the folder does not contradict its contents.`,
    requiredFile: REQUIRED_ENGINE_FILES.buildVersion,
    requiredFileAbsolutePath: `${engineRoot}/${REQUIRED_ENGINE_FILES.buildVersion}`,
    requiredFields: { MajorVersion: 5, MinorVersion: spec.minor, PatchVersion: null },
    patchConstraint: `PatchVersion is unconstrained: any non-negative integer patch of ${spec.minorKey} satisfies detection. No specific patch is named here because naming one would invent a release.`,
    structuralTemplateFrom: spec.templateFile,
    structuralTemplateSha256: spec.templateSha256,
    structuralTemplateNote: spec.templateFile === null
      ? 'no installed root was available to quote as a structural template'
      : `the required file has the same JSON shape as ${spec.templateFile}, which is a real file on this host; only Major/Minor/PatchVersion differ`,
    alsoRequired: [
      `${REQUIRED_ENGINE_FILES.versionHeader} must define ENGINE_MAJOR_VERSION 5, ENGINE_MINOR_VERSION ${spec.minor} and the same ENGINE_PATCH_VERSION. The detector reads both in-engine sources and refuses a root where they disagree, so the two must come from one install.`,
      `${REQUIRED_ENGINE_FILES.runUat} must exist, or the plugin cannot be packaged for this root.`,
      `${REQUIRED_ENGINE_FILES.editorCmd} must exist and be executable, or a plugin can be built here but no editor can be launched and nothing can be certified.`,
    ],
    handWrittenFileIsRefused: `writing ${REQUIRED_ENGINE_FILES.buildVersion} by hand does NOT satisfy this record. It would parse, and then fail on the ${REQUIRED_ENGINE_FILES.versionHeader} cross-check, the RunUAT requirement and the editor-binary requirement. The input is a genuine Epic installation of ${spec.minorKey}, not a file.`,
    obtainedFrom: [
      `an EpicGames/UnrealEngine checkout of a ${spec.minorKey} release tag (${tagHint}), followed by ./Setup.sh, ./GenerateProjectFiles.sh and make — this is what produces ${REQUIRED_ENGINE_FILES.editorCmd}`,
      `or an Epic Games Launcher installation of ${spec.minorKey}, placed or symlinked at a directory directly under a scanned search dir`,
    ],
    acceptanceCommand: `node ${spec.projectRelativeDetector} --search-dir /data --minor ${spec.minorKey}`,
    acceptanceCriterion: `the acceptance command must exit 0 and report ${spec.minorKey} as installed, buildable and runnable. While it exits 3, this record stands.`,
  };
}

/**
 * Build one BLOCKED_EXTERNAL record for one absent minor.
 *
 * Deliberately carries no `pass`, `ok`, `success`, `skip` or `verdict` field of
 * any kind, at any depth — see external-blocker-validator.mjs, which refuses a
 * record that does.
 * @param {{ minorKey: string, inventory: any, detection: ReturnType<typeof describeDetection>,
 *   projectRelativeDetector: string, advertisedBy?: readonly string[], observedTagExample?: string|null }} spec
 */
export function buildMissingMinorBlocker(spec) {
  const minor = Number(spec.minorKey.split('.')[1]);
  const resolution = spec.inventory.resolve(spec.minorKey);
  const template = [...spec.detection.hostEngineTree.roots]
    .find((entry) => typeof entry.buildVersionSha256 === 'string') ?? null;

  return {
    recordId: `blocked-external-unreal-engine-${spec.minorKey}`,
    status: BLOCKER_STATUS,
    severity: BLOCKER_SEVERITY,
    blocksTasks: [...BLOCKED_TASKS],
    blocksClaims: [
      `the UE ${spec.minorKey} row of the Task 62 engine/plugin/client compatibility matrix`,
      `BEST_IN_CLASS_VERIFIED in Task 63, which additionally requires all advertised UE minors`,
      `the advertised "UE ${ADVERTISED_RANGE}" support statement reconciled in Task 64`,
    ],
    subject: {
      kind: 'unreal-engine-minor',
      minorKey: spec.minorKey,
      advertisedRange: ADVERTISED_RANGE,
      advertisedBy: [...(spec.advertisedBy ?? [])],
    },
    detection: spec.detection,
    absence: {
      reason: ABSENCE_REASON,
      detail: resolution.detail,
      scannedRootCount: spec.detection.hostEngineTree.roots.length,
      rootsContainingSubjectMinor: spec.detection.hostEngineTree.roots
        .filter((entry) => entry.minorKey === spec.minorKey)
        .map((entry) => entry.root),
      notSubstitutableBy: notSubstitutableBy({ minorKey: spec.minorKey, inventory: spec.inventory }),
      substitutionRule: `no neighbouring minor is evidence for ${spec.minorKey}. Certification is per-minor and binaries are never reused across minors, so substituting one would file another engine's result under this heading.`,
    },
    requiredOperatorInput: requiredOperatorInput({
      minorKey: spec.minorKey,
      minor,
      templateFile: template === null ? null : template.buildVersionFile,
      templateSha256: template === null ? null : template.buildVersionSha256,
      observedTagExample: spec.observedTagExample ?? null,
      projectRelativeDetector: spec.projectRelativeDetector,
    }),
    remediation: {
      summary: `Install a genuine Unreal Engine ${spec.minorKey} at a directory directly under /data, build its editor, then re-run the acceptance command and re-run the Task 56-60 certification shape against it.`,
      steps: [
        `1. Obtain UE ${spec.minorKey} under your Epic licence: clone a \`${spec.minorKey}.<patch>-release\` tag of EpicGames/UnrealEngine into /data/UnrealEngine-${spec.minorKey}, or install ${spec.minorKey} with the Epic Games Launcher and place it there.`,
        `2. cd /data/UnrealEngine-${spec.minorKey} && ./Setup.sh && ./GenerateProjectFiles.sh && make — this is the step that produces ${REQUIRED_ENGINE_FILES.editorCmd}; without it the root can package a plugin but cannot host a certification.`,
        `3. Confirm ${REQUIRED_ENGINE_FILES.buildVersion} reports MajorVersion 5 and MinorVersion ${minor}, and that ${REQUIRED_ENGINE_FILES.versionHeader} agrees.`,
        `4. Re-run the acceptance command; while it exits 3 this record stands, and it must exit 0 before any ${spec.minorKey} row may be claimed.`,
        `5. Run the Task 56-60 certification shape against the new root — a present engine is a precondition for certification, never a substitute for it.`,
      ],
      performedBy: 'operator',
      notPerformedHere: 'Task 61 is a read-only filesystem inventory. Downloading or installing an engine is explicitly outside its scope, so nothing here attempts the remediation it describes.',
    },
    consequence: `Until the required input exists, UE ${spec.minorKey} cannot be built, launched, exercised or certified on this host, and no ${spec.minorKey} result may appear in any matrix, readiness record or public support claim. Narrowing the advertised ${ADVERTISED_RANGE} range to hide this gap is not an accepted resolution.`,
  };
}

/**
 * Read a real `Build.version` and report the minor it CONTAINS.
 *
 * Used by the validator to catch a record that points at the wrong minor's root.
 * Returns null when the file is absent or unreadable, which the caller must treat
 * as "could not look" rather than as agreement.
 * @param {string} buildVersionFile
 */
export function minorContainedAt(buildVersionFile) {
  try {
    const parsed = JSON.parse(readFileSync(buildVersionFile, 'utf8'));
    const [major, minor, patch] = BUILD_VERSION_FIELDS
      .map((key) => (typeof parsed?.[key] === 'number' ? Number(parsed[key]) : Number.NaN));
    if ([major, minor, patch].some((value) => !Number.isFinite(value))) return null;
    return minorKeyOf({ major, minor, patch });
  } catch {
    return null;
  }
}
