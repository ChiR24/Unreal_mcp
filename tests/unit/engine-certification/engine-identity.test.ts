// tests/unit/engine-certification/engine-identity.test.ts
// Task 52 — the identity of an engine root is decided by the ENGINE, never by the path.
//
// This host is the reason the rule is mechanical rather than advisory. Three of
// its six engine roots have a folder name that does not describe what is inside:
//
//   /data/UnrealEngine                 holds 5.7.4  (the name says nothing at all)
//   /data/UnrealEngine-5.0-branch      holds 5.0.3  (the name says nothing at all)
//   /data/UnrealEngine-5.8.0-preview-1 holds 5.8.0, and its own git tag is
//                                      `5.8.0-release` — the name is simply wrong
//
// A certification run that picked "the 5.8 preview" by globbing folder names
// would package a plugin for, launch, and certify an engine that is not the one
// named in the report. Every assertion below exists to make that impossible.

import { describe, expect, it } from 'vitest';

import {
  IDENTITY_REASONS,
  folderVersionClaim,
  parseBuildVersion,
  parseVersionHeader,
  readEngineIdentity,
} from './engine-identity.mjs';

/** A Build.version exactly as the engine writes it. */
const buildVersion = (major: number, minor: number, patch: number, extra: Record<string, unknown> = {}) =>
  JSON.stringify({
    MajorVersion: major, MinorVersion: minor, PatchVersion: patch,
    Changelist: 0, CompatibleChangelist: 0, IsLicenseeVersion: 0, IsPromotedBuild: 0,
    BranchName: 'UE5', ...extra,
  });

/** A Version.h exactly as the engine writes it. */
const versionHeader = (major: number, minor: number, patch: number) => [
  '#pragma once',
  `#define ENGINE_MAJOR_VERSION\t${major}`,
  `#define ENGINE_MINOR_VERSION\t${minor}`,
  `#define ENGINE_PATCH_VERSION\t${patch}`,
].join('\n');

const BUILD = 'Engine/Build/Build.version';
const HEADER = 'Engine/Source/Runtime/Launch/Resources/Version.h';
const EDITOR = 'Engine/Binaries/Linux/UnrealEditor-Cmd';
const RUNUAT = 'Engine/Build/BatchFiles/RunUAT.sh';

/** An offline engine root: a map of relative path to contents. */
function fakeIo(files: Record<string, string>, options: { executable?: string[]; describe?: string | null } = {}) {
  const executable = new Set(options.executable ?? []);
  return {
    readFile: (path: string) => {
      const key = Object.keys(files).find((entry) => path.endsWith(entry));
      if (key === undefined) throw new Error(`ENOENT ${path}`);
      return files[key] as string;
    },
    exists: (path: string) => Object.keys(files).some((entry) => path.endsWith(entry)),
    isExecutable: (path: string) => [...executable].some((entry) => path.endsWith(entry)),
    describe: () => options.describe ?? null,
  };
}

describe('parseBuildVersion', () => {
  it('reads the version the engine wrote', () => {
    const parsed = parseBuildVersion(buildVersion(5, 7, 4, { CompatibleChangelist: 47_537_391 }));
    expect(parsed.ok).toBe(true);
    expect(parsed.version).toEqual({ major: 5, minor: 7, patch: 4 });
    expect(parsed.compatibleChangelist).toBe(47_537_391);
  });

  it('refuses a file that is not JSON rather than guessing', () => {
    expect(parseBuildVersion('not json').reason).toBe(IDENTITY_REASONS.MALFORMED_BUILD_VERSION);
  });

  it('refuses a JSON file with no MajorVersion, because a partial identity is not an identity', () => {
    expect(parseBuildVersion(JSON.stringify({ MinorVersion: 7 })).reason)
      .toBe(IDENTITY_REASONS.MALFORMED_BUILD_VERSION);
  });
});

describe('parseVersionHeader', () => {
  it('reads the SECOND, independent in-engine statement of the same fact', () => {
    expect(parseVersionHeader(versionHeader(5, 8, 0))).toEqual({ major: 5, minor: 8, patch: 0 });
  });

  it('returns null when the header does not define the macros', () => {
    expect(parseVersionHeader('#pragma once')).toBeNull();
  });
});

describe('folderVersionClaim', () => {
  it('extracts what a path CLAIMS, which is used only to detect a contradiction', () => {
    expect(folderVersionClaim('/data/UnrealEngine-5.8.0-preview-1').claim).toEqual({ major: 5, minor: 8, patch: 0 });
    expect(folderVersionClaim('/data/UnrealEngine-5.8.0-preview-1').label).toBe('preview-1');
  });

  it('reports no claim for a path that names no version', () => {
    expect(folderVersionClaim('/data/UnrealEngine').claim).toBeNull();
    expect(folderVersionClaim('/data/UnrealEngine-5.0-branch').claim).toBeNull();
  });
});

describe('readEngineIdentity', () => {
  it('identifies /data/UnrealEngine as 5.7.4 even though the path names no version', () => {
    const identity = readEngineIdentity({
      root: '/data/UnrealEngine',
      io: fakeIo({ [BUILD]: buildVersion(5, 7, 4), [HEADER]: versionHeader(5, 7, 4), [EDITOR]: '', [RUNUAT]: '' },
        { executable: [EDITOR], describe: '5.7.0-preview-1-4009-g0bcfaffa52e9' }),
    });
    expect(identity.usable).toBe(true);
    expect(identity.versionString).toBe('5.7.4');
    expect(identity.minorKey).toBe('5.7');
    expect(identity.folderName.claim).toBeNull();
    expect(identity.sources.versionHeader.agrees).toBe(true);
  });

  it('records that /data/UnrealEngine-5.8.0-preview-1 calls itself 5.8.0-release', () => {
    const identity = readEngineIdentity({
      root: '/data/UnrealEngine-5.8.0-preview-1',
      io: fakeIo({ [BUILD]: buildVersion(5, 8, 0), [HEADER]: versionHeader(5, 8, 0), [EDITOR]: '', [RUNUAT]: '' },
        { executable: [EDITOR], describe: '5.8.0-release' }),
    });
    expect(identity.versionString).toBe('5.8.0');
    // The folder says preview-1. The engine's own tag says release. The label is
    // therefore reported as UNPROVEN rather than repeated as though it were read
    // from the engine.
    expect(identity.folderName.label).toBe('preview-1');
    expect(identity.channel.value).toBe('stable');
    expect(identity.channel.provenBy).toBe('git-describe');
    expect(identity.channel.folderLabelContradicted).toBe(true);
  });

  it('files a root under the version it CONTAINS, not the version its name claims', () => {
    const identity = readEngineIdentity({
      root: '/engines/UnrealEngine-5.4.0',
      io: fakeIo({ [BUILD]: buildVersion(5, 3, 2), [HEADER]: versionHeader(5, 3, 2), [RUNUAT]: '' }),
    });
    expect(identity.usable).toBe(true);
    expect(identity.minorKey).toBe('5.3');
    expect(identity.folderName.agrees).toBe(false);
    expect(identity.notes.join(' ')).toContain('5.4.0');
  });

  it('is UNUSABLE without Build.version and never falls back to the folder name', () => {
    const identity = readEngineIdentity({
      root: '/engines/UnrealEngine-5.6.1',
      io: fakeIo({ [RUNUAT]: '' }),
    });
    expect(identity.usable).toBe(false);
    expect(identity.reason).toBe(IDENTITY_REASONS.NO_BUILD_VERSION);
    expect(identity.version).toBeNull();
    expect(identity.minorKey).toBeNull();
  });

  it('is UNUSABLE when its two in-engine sources disagree', () => {
    const identity = readEngineIdentity({
      root: '/engines/mixed',
      io: fakeIo({ [BUILD]: buildVersion(5, 5, 4), [HEADER]: versionHeader(5, 3, 2), [RUNUAT]: '' }),
    });
    expect(identity.usable).toBe(false);
    expect(identity.reason).toBe(IDENTITY_REASONS.IDENTITY_CONFLICT);
    expect(identity.sources.versionHeader.agrees).toBe(false);
  });

  it('separates "can build a plugin" from "can run an editor"', () => {
    const sourceOnly = readEngineIdentity({
      root: '/data/UnrealEngine-5.5.4',
      io: fakeIo({ [BUILD]: buildVersion(5, 5, 4), [HEADER]: versionHeader(5, 5, 4), [RUNUAT]: '' },
        { describe: '5.5.4-release' }),
    });
    expect(sourceOnly.buildable).toBe(true);
    expect(sourceOnly.runnable).toBe(false);
    expect(sourceOnly.toolchain.hasCompiledEditor).toBe(false);
  });
});
