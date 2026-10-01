// A refused path says why, a mounted content root is accepted wherever a folder is, and a reply keeps a registered
// mount readable. Wiring contracts only: the behaviour is pinned in-editor by
// McpAutomationBridge.Transport.ContentRoots.PathAcceptance and .ReplyRedaction.
//
// query_asset lookup=search packagePaths ["/Game","/MoverExamples","/MoverTests"] answered "contains traversal
// sequences" for a root whose plugin was simply not enabled: about thirty handlers worded every refusal as
// traversal and never asked SanitizeProjectRelativePath what it had refused, and the reply hid which path it was.

import { readdirSync, readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const PRIVATE = join('plugins', 'McpAutomationBridge', 'Source', 'McpAutomationBridge', 'Private');
const SECURITY = join(PRIVATE, 'Foundation', 'BridgeHelpers', 'Security');
const CORE = join(PRIVATE, 'Core', 'Subsystem');

/** Block and line comments removed, so no assertion can be satisfied (or tripped) by prose. */
const strip = (text: string): string => text.replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');
const code = (...segments: readonly string[]): string => strip(readFileSync(join(...segments), 'utf8'));

const walk = (dir: string): string[] =>
  readdirSync(dir, { withFileTypes: true }).flatMap((entry) => {
    if (entry.isDirectory()) return entry.name === 'Generated' ? [] : walk(join(dir, entry.name));
    return /\.(?:cpp|h)$/u.test(entry.name) ? [join(dir, entry.name)] : [];
  });

const sources = walk(PRIVATE).map((file) => ({ file, body: code(file) }));

describe('a mounted content root is a folder like /Game is', () => {
  const paths = code(SECURITY, 'McpAutomationBridgeHelpersProjectPaths.h');

  it('accepts a bare root, a trailing slash and an object path under a registered mount', () => {
    const probe = paths.slice(paths.indexOf('McpIsMountedContentPath('));
    expect(probe).toContain('FPackageName::ObjectPathToPackageName(CleanPath)');
    // The root is looked up as a mount, and only a path BELOW it is validated as a package name: the engine's
    // validator refuses a bare root shorter than four characters, which /Game and /Engine never are.
    expect(probe).toContain('FPackageName::MountPointExists(Root + TEXT("/"))');
    expect(probe).toContain('bBareRoot ||');
  });

  it('does not hand the whole path to the package-name validator any more', () => {
    expect(paths).not.toMatch(/IsValidLongPackageName\(CleanPath/u);
  });

  it('keeps the reason a refusal had, so a caller can say it', () => {
    expect(paths).toMatch(/enum class EMcpPathRejection : uint8 \{[^}]*NotAMountedRoot,[^}]*InvalidName,/u);
    expect(paths).toContain('OutReason = bRootMounted ? EMcpPathRejection::InvalidName : EMcpPathRejection::NotAMountedRoot;');
    // Asking again, to word a message, must not log the refusal twice.
    expect(paths).toContain('bool bLogRefusal)');
  });
});

describe('a refusal is worded in one place', () => {
  const refusal = code(SECURITY, 'McpAutomationBridgeHelpersProjectPathsRefusal.h');

  it('defines the formatter once, in the helper header chain', () => {
    const definers = sources.filter(({ body }) => /\bMcpDescribePathRejection\s*\([^;{]*\)\s*\{/u.test(body)).map(({ file }) => file);
    expect(definers).toEqual([join(SECURITY, 'McpAutomationBridgeHelpersProjectPathsRefusal.h')]);
    expect(code(SECURITY, 'McpAutomationBridgeHelpersProjectPaths.h')).toContain(
      '#include "Foundation/BridgeHelpers/Security/McpAutomationBridgeHelpersProjectPathsRefusal.h"',
    );
  });

  it('asks the helper again without logging, and calls only a real ".." segment traversal', () => {
    expect(refusal).toMatch(/McpClassifyProjectPath\(RawPath, &Reason, &Detail, &Normalized,\s+false\)/u);
    expect(refusal).toContain("the path contains a '..' traversal segment.");
    expect(refusal.match(/traversal/gu)?.length).toBe(1);
  });

  it('says which root is not mounted and lists the roots that are', () => {
    expect(refusal).toContain('is not a mounted content root');
    expect(refusal).toContain('Mounted roots: %s"');
    // No full stop after the list: the reply redactor reads a root followed by '.' as part of a path.
    expect(refusal).not.toContain('Mounted roots: %s.');
    expect(refusal).toMatch(/FPackageName::QueryRootContentPaths\(Roots,\s+true,/u);
  });
});

describe('the handlers report what SanitizeProjectRelativePath said', () => {
  // The words that blamed traversal for every refusal, and the generic ones that named nothing. A real '..' check
  // says so in its own words (the helper's Traversal case), and a name or a file path is not a content path.
  const LEGACY_WORDING: readonly RegExp[] = [
    /contains traversal sequences/u,
    /traversal\/security violation/u,
    /path traversal or invalid characters/u,
    /contains traversal or invalid characters/u,
    /traversal or invalid roots/u,
    /contains path traversal \(\.\.\) or invalid characters/u,
    /contains path traversal \(\.\.\), double slashes/u,
    /Invalid or unsafe (?!output path|project-relative file path|file path)/u,
    /Invalid (?:asset|source|destination) path"/u,
  ];

  it('no handler words a refused content path as traversal, or as nothing at all', () => {
    const offenders = sources.flatMap(({ file, body }) =>
      LEGACY_WORDING.filter((pattern) => pattern.test(body)).map((pattern) => `${file}: ${pattern.source}`));
    expect(offenders).toEqual([]);
  });

  it('every handler that refuses a content path words it with the shared helper', () => {
    const users = sources.filter(({ body }) => /\bMcpPathRefusalMessage\s*\(/u.test(body) && !body.includes('static inline FString McpPathRefusalMessage'));
    // About a hundred refusal sites went over; a handler that stops using it shows up as a drop here.
    expect(users.length).toBeGreaterThan(70);
    const calls = users.reduce((count, { body }) => count + (body.match(/\bMcpPathRefusalMessage\s*\(/gu)?.length ?? 0), 0);
    expect(calls).toBeGreaterThan(100);
  });

  it('query_asset search names the field and the reason for a package path it refuses', () => {
    const search = code(PRIVATE, 'Domains', 'AssetQuery', 'McpAutomationBridge_AssetQuerySearch.cpp');
    expect(search).toContain('McpPathRefusalMessage(TEXT("package path"), RawPath)');
    expect(search).toContain('McpPathRefusalMessage(TEXT("path"), SinglePath)');
    expect(search).not.toContain('traversal');
  });
});

describe('a reply keeps a registered content mount readable', () => {
  const sanitization = code(CORE, 'McpAutomationBridgeSubsystemResponseSanitization.h');
  const contentPaths = code(CORE, 'McpAutomationBridgeSubsystemContentPaths.h');

  it('asks the engine whether the first segment is a registered mount, and never trusts a host root', () => {
    const mount = contentPaths.slice(contentPaths.indexOf('IsRegisteredContentMountAt('));
    expect(mount).toContain('FPackageName::MountPointExists(TEXT("/") + Name + TEXT("/"))');
    // A host root is not a mount whatever is registered under that name.
    expect(mount).toContain('!McpAssetPathCanonical::IsHostFilesystemRootSegment(Name)');
  });

  it('keeps the five engine roots and adds the registered mounts to them', () => {
    const allowed = sanitization.slice(sanitization.indexOf('IsAllowedUnrealMountPath('));
    for (const root of ['/Game', '/Engine', '/Script', '/Temp', '/Niagara']) {
      expect(allowed).toContain(`IsAllowedUnrealMountPrefixAt(Value, Index, TEXT("${root}"))`);
    }
    expect(allowed).toContain('(bNameMounts && IsRegisteredContentMountAt(Value, Index))');
  });

  it('still hides a host path of every shape, and the root nothing has mounted', () => {
    const redact = sanitization.slice(sanitization.indexOf('inline FString RedactFilesystemPathsForResponse'));
    expect(redact).toContain('const bool bWindowsPath');
    expect(redact).toContain('const bool bUncPath');
    expect(redact).toContain('FString(TEXT("[path redacted]"))');
    expect(redact).toContain('IsAllowedUnrealMountPath(Input, Index, bNameMounts)');
  });

  it('asks the engine only where a reply is built, never inline in the log device', () => {
    // The log device runs on whichever thread logged, which can be the one holding the engine's mount lock.
    expect(code(PRIVATE, 'Domains', 'Log', 'McpAutomationBridge_LogHandlers.cpp')).toMatch(
      /SanitizeEngineErrorForResponse\(FString\(V\),\s+false\)\.Left\(2048\)/u,
    );
    // Nothing else passes the flag: every other caller sanitizes a finished message, on the thread that answers.
    const flagged = sources
      .filter(({ file, body }) => /SanitizeEngineErrorForResponse\((?:[^(),]|\([^()]*\))+,\s*(?:false|true)\)/u.test(body)
        && !file.endsWith('ResponseSanitization.h') && !file.startsWith(join(PRIVATE, 'Tests')))
      .map(({ file }) => file);
    expect(flagged).toEqual([join(PRIVATE, 'Domains', 'Log', 'McpAutomationBridge_LogHandlers.cpp')]);
  });
});
