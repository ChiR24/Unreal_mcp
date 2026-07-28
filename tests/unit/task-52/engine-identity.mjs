// @ts-check
// tests/unit/task-52/engine-identity.mjs
// Task 52 — what engine is actually at this path?
//
// The plan's rule is one line: infer versions only from `Engine/Build/Build.version`,
// never from folder names. This host is why that is a mechanical rule and not a
// style preference. Of its six engine roots:
//
//   /data/UnrealEngine                  contains 5.7.4   — the name says nothing
//   /data/UnrealEngine-5.0-branch       contains 5.0.3   — the name says nothing
//   /data/UnrealEngine-5.8.0-preview-1  contains 5.8.0, and its OWN git tag is
//                                       `5.8.0-release`  — the name is wrong
//
// Certification is per-minor and binaries are never reused across minors, so an
// identity that is off by one minor packages the wrong plugin, links the wrong
// engine headers, and then reports the result under the wrong version. Nothing
// downstream could tell.
//
// THREE SOURCES, ONE AUTHORITY. `Build.version` decides. `Version.h` is read
// independently and must AGREE — the engine writes both, so a disagreement means
// this root is a mixture of two trees and is unusable, not "probably fine".
// `git describe` is read third and only ever refines the CHANNEL (release vs
// preview), which neither of the other two records. The folder name is read last
// and is never an input: it exists here only so a contradiction can be REPORTED.

import { execFileSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import { accessSync, constants, existsSync, readFileSync } from 'node:fs';
import { basename, join } from 'node:path';

/** Closed refusal taxonomy for a root that cannot be certified against. */
export const IDENTITY_REASONS = Object.freeze({
  OK: 'OK',
  NO_BUILD_VERSION: 'NO_BUILD_VERSION',
  MALFORMED_BUILD_VERSION: 'MALFORMED_BUILD_VERSION',
  IDENTITY_CONFLICT: 'IDENTITY_CONFLICT',
});

/** Paths relative to an engine root. Named once so a probe cannot drift from a report. */
export const ENGINE_FILES = Object.freeze({
  buildVersion: 'Engine/Build/Build.version',
  versionHeader: 'Engine/Source/Runtime/Launch/Resources/Version.h',
  editorCmd: 'Engine/Binaries/Linux/UnrealEditor-Cmd',
  runUat: 'Engine/Build/BatchFiles/RunUAT.sh',
});

/** Real filesystem + git access. Injectable so every test above runs offline. */
export const realIo = Object.freeze({
  /** @param {string} path */
  readFile: (path) => readFileSync(path, 'utf8'),
  /** @param {string} path */
  exists: (path) => existsSync(path),
  /** @param {string} path */
  isExecutable: (path) => {
    try {
      accessSync(path, constants.X_OK);
      return true;
    } catch {
      return false;
    }
  },
  /** The engine's own tag, which is the only in-tree statement of release-vs-preview.
   * @param {string} root */
  describe: (root) => {
    try {
      return execFileSync('git', ['-C', root, 'describe', '--tags'], { encoding: 'utf8', timeout: 15_000 }).trim();
    } catch {
      return null;
    }
  },
});

/** @typedef {{ major: number, minor: number, patch: number }} Version */

/** @param {Version|null} version */
export const versionString = (version) => (version === null ? null : `${version.major}.${version.minor}.${version.patch}`);
/** @param {Version|null} version */
export const minorKeyOf = (version) => (version === null ? null : `${version.major}.${version.minor}`);
/** @param {Version|null} a @param {Version|null} b */
export const sameVersion = (a, b) => a !== null && b !== null
  && a.major === b.major && a.minor === b.minor && a.patch === b.patch;

/**
 * Parse `Engine/Build/Build.version`. THE authority.
 *
 * A file that parses but has no MajorVersion is refused rather than defaulted:
 * a partial identity would silently become "5.undefined", which sorts, prints
 * and compares like a real answer.
 * @param {string} text
 */
export function parseBuildVersion(text) {
  /** @type {any} */
  let parsed;
  try {
    parsed = JSON.parse(text);
  } catch {
    return { ok: false, reason: IDENTITY_REASONS.MALFORMED_BUILD_VERSION, detail: 'not valid JSON' };
  }
  const numbers = ['MajorVersion', 'MinorVersion', 'PatchVersion']
    .map((key) => (typeof parsed?.[key] === 'number' ? parsed[key] : null));
  if (numbers.some((value) => value === null)) {
    return {
      ok: false,
      reason: IDENTITY_REASONS.MALFORMED_BUILD_VERSION,
      detail: 'Major/Minor/PatchVersion must all be numbers; a partial identity is not an identity',
    };
  }
  return {
    ok: true,
    reason: IDENTITY_REASONS.OK,
    version: /** @type {Version} */ ({ major: numbers[0], minor: numbers[1], patch: numbers[2] }),
    branch: typeof parsed.BranchName === 'string' ? parsed.BranchName : null,
    changelist: typeof parsed.Changelist === 'number' ? parsed.Changelist : null,
    compatibleChangelist: typeof parsed.CompatibleChangelist === 'number' ? parsed.CompatibleChangelist : null,
    isLicenseeVersion: parsed.IsLicenseeVersion === 1,
    isPromotedBuild: parsed.IsPromotedBuild === 1,
  };
}

/**
 * Parse `Version.h`. The engine writes this from the same source as Build.version,
 * so it is a genuinely independent restatement of the same fact — which is what
 * makes a disagreement meaningful.
 * @param {string} text @returns {Version|null}
 */
export function parseVersionHeader(text) {
  const read = (/** @type {string} */ macro) => {
    const match = new RegExp(`^\\s*#define\\s+${macro}\\s+(\\d+)`, 'mu').exec(text);
    return match === null ? null : Number(match[1]);
  };
  const parts = [read('ENGINE_MAJOR_VERSION'), read('ENGINE_MINOR_VERSION'), read('ENGINE_PATCH_VERSION')];
  if (parts.some((value) => value === null)) return null;
  return /** @type {Version} */ ({ major: parts[0], minor: parts[1], patch: parts[2] });
}

/**
 * What a PATH claims about itself. Never an input to the identity — only ever
 * compared against it, so a wrong name is reported instead of believed.
 *
 * A two-component name like `UnrealEngine-5.0-branch` names a FAMILY, not a
 * version, and is deliberately not treated as a claim: manufacturing a claim of
 * "5.0.?" out of it would produce contradictions that say nothing.
 * @param {string} root
 */
export function folderVersionClaim(root) {
  const raw = basename(root);
  const match = /(\d+)\.(\d+)\.(\d+)(?:[-_.]?(preview[-_.]?\d+|ea|rc[-_.]?\d*|release))?/iu.exec(raw);
  if (match === null) return { raw, claim: null, label: null };
  return {
    raw,
    claim: /** @type {Version} */ ({ major: Number(match[1]), minor: Number(match[2]), patch: Number(match[3]) }),
    label: match[4] === undefined ? null : match[4].toLowerCase().replace(/[_.]/gu, '-'),
  };
}

/**
 * Read the channel out of `git describe`.
 *
 * An INEXACT describe (`5.7.0-preview-1-4009-g0bcfaff`) says only that the tag is
 * an ancestor 4009 commits back. That proves nothing about what the tree is now,
 * so the channel is reported UNPROVEN rather than inherited from the tag. This is
 * the difference between /data/UnrealEngine (unproven) and
 * /data/UnrealEngine-5.8.0-preview-1 (exactly `5.8.0-release`, so provably stable).
 * @param {string|null} describe @param {string|null} branch
 */
export function channelFromSources(describe, branch) {
  const exact = typeof describe === 'string' && /^\d+\.\d+\.\d+-[A-Za-z0-9-]+$/u.test(describe)
    && !/-\d+-g[0-9a-f]{7,}$/u.test(describe);
  if (exact) {
    const preview = /-preview-?(\d+)$/u.exec(/** @type {string} */ (describe));
    if (preview !== null) return { value: 'preview', preview: Number(preview[1]), provenBy: 'git-describe', tag: describe, exactTag: true };
    if (/-release$/u.test(/** @type {string} */ (describe))) return { value: 'stable', preview: null, provenBy: 'git-describe', tag: describe, exactTag: true };
  }
  if (typeof branch === 'string' && /^\+\+UE\d\+Release-/u.test(branch)) {
    return { value: 'stable', preview: null, provenBy: 'branch-name', tag: describe ?? null, exactTag: exact };
  }
  return { value: null, preview: null, provenBy: 'unproven', tag: describe ?? null, exactTag: exact };
}

/** @param {string} text */
const sha256 = (text) => createHash('sha256').update(text).digest('hex');

/**
 * Identify one engine root.
 * @param {{ root: string, io?: typeof realIo }} spec
 */
export function readEngineIdentity(spec) {
  const io = spec.io ?? realIo;
  const file = (/** @type {string} */ relative) => join(spec.root, relative);
  const folderName = folderVersionClaim(spec.root);
  /** @type {string[]} */
  const notes = [];

  const buildVersionFile = file(ENGINE_FILES.buildVersion);
  /** @type {string|null} */
  let buildVersionText = null;
  try {
    buildVersionText = io.exists(buildVersionFile) ? io.readFile(buildVersionFile) : null;
  } catch {
    buildVersionText = null;
  }

  const headerFile = file(ENGINE_FILES.versionHeader);
  let headerVersion = null;
  try {
    headerVersion = io.exists(headerFile) ? parseVersionHeader(io.readFile(headerFile)) : null;
  } catch {
    headerVersion = null;
  }

  const toolchain = {
    unrealEditorCmd: file(ENGINE_FILES.editorCmd),
    hasCompiledEditor: io.isExecutable(file(ENGINE_FILES.editorCmd)),
    runUat: file(ENGINE_FILES.runUat),
    hasRunUat: io.exists(file(ENGINE_FILES.runUat)),
  };

  /** @param {string} reason @param {string} detail
   * @param {{ headerAgrees?: boolean|null, buildVersion?: Version|null }} [seen] */
  const unusable = (reason, detail, seen = {}) => {
    notes.push(detail);
    return {
      root: spec.root, usable: false, reason, detail,
      version: null, versionString: null, minorKey: null,
      branch: null, changelist: null, compatibleChangelist: null,
      isLicenseeVersion: null, isPromotedBuild: null,
      channel: { value: null, preview: null, provenBy: 'unproven', tag: null, exactTag: false, folderLabel: folderName.label, folderLabelContradicted: false },
      sources: {
        buildVersion: { file: buildVersionFile, present: buildVersionText !== null, sha256: buildVersionText === null ? null : sha256(buildVersionText), version: seen.buildVersion ?? null },
        versionHeader: { file: headerFile, present: headerVersion !== null, version: headerVersion, agrees: seen.headerAgrees ?? null },
        gitDescribe: { raw: null, ...channelFromSources(null, null) },
      },
      folderName: { ...folderName, agrees: null },
      toolchain, buildable: false, runnable: false, notes,
    };
  };

  if (buildVersionText === null) {
    return unusable(IDENTITY_REASONS.NO_BUILD_VERSION,
      `${ENGINE_FILES.buildVersion} is absent. The folder name is NOT a fallback: "${folderName.raw}" is a label somebody typed, not something the engine wrote.`);
  }
  const parsed = parseBuildVersion(buildVersionText);
  if (parsed.ok !== true) {
    return unusable(IDENTITY_REASONS.MALFORMED_BUILD_VERSION, `${ENGINE_FILES.buildVersion} is unreadable: ${parsed.detail}`);
  }

  const version = /** @type {Version} */ (parsed.version);
  const headerAgrees = headerVersion === null ? null : sameVersion(headerVersion, version);
  if (headerAgrees === false) {
    return unusable(IDENTITY_REASONS.IDENTITY_CONFLICT,
      `Build.version says ${versionString(version)} but Version.h says ${versionString(headerVersion)}. `
      + 'Two in-engine sources disagree, so this root is a mixture of trees and cannot be certified.',
      { headerAgrees: false, buildVersion: version });
  }

  const describe = io.describe(spec.root);
  const channel = channelFromSources(describe, parsed.branch ?? null);
  const folderAgrees = folderName.claim === null ? null : sameVersion(folderName.claim, version);
  if (folderAgrees === false) {
    notes.push(`FOLDER NAME CONTRADICTS THE ENGINE: the path claims ${versionString(folderName.claim)} but `
      + `${ENGINE_FILES.buildVersion} says ${versionString(version)}. Filed under the version it CONTAINS.`);
  }
  const labelContradicted = folderName.label !== null && channel.value !== null
    && folderName.label !== (channel.value === 'preview' ? `preview-${channel.preview}` : 'release');
  if (labelContradicted) {
    notes.push(`FOLDER LABEL "${folderName.label}" is contradicted by the engine's own tag "${channel.tag}". `
      + 'The label is reported, never repeated as though it were read from the engine.');
  }
  if (!toolchain.hasCompiledEditor) {
    notes.push('No compiled UnrealEditor-Cmd: this root can BUILD a plugin but cannot RUN a certification.');
  }

  return {
    root: spec.root, usable: true, reason: IDENTITY_REASONS.OK, detail: null,
    version, versionString: versionString(version), minorKey: minorKeyOf(version),
    branch: parsed.branch ?? null,
    changelist: parsed.changelist ?? null,
    compatibleChangelist: parsed.compatibleChangelist ?? null,
    isLicenseeVersion: parsed.isLicenseeVersion === true,
    isPromotedBuild: parsed.isPromotedBuild === true,
    channel: { ...channel, folderLabel: folderName.label, folderLabelContradicted: labelContradicted },
    sources: {
      buildVersion: { file: buildVersionFile, present: true, sha256: sha256(buildVersionText), version },
      versionHeader: { file: headerFile, present: headerVersion !== null, version: headerVersion, agrees: headerAgrees },
      gitDescribe: { raw: describe, ...channel },
    },
    folderName: { ...folderName, agrees: folderAgrees },
    toolchain,
    buildable: toolchain.hasRunUat,
    runnable: toolchain.hasCompiledEditor && toolchain.hasRunUat,
    notes,
  };
}
