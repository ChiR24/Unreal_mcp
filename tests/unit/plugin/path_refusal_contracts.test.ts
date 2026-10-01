// A refused path says why, and a mounted content root is accepted wherever a folder is. Wiring contracts only: the
// behaviour is pinned in-editor by McpAutomationBridge.Transport.ContentRoots.PathAcceptance.
//
// query_asset lookup=search packagePaths ["/Game","/MoverExamples","/MoverTests"] answered "contains traversal
// sequences" for a root whose plugin was simply not enabled: about thirty handlers worded every refusal as
// traversal and never asked SanitizeProjectRelativePath what it had refused.

import { readdirSync, readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const PRIVATE = join('plugins', 'McpAutomationBridge', 'Source', 'McpAutomationBridge', 'Private');
const SECURITY = join(PRIVATE, 'Foundation', 'BridgeHelpers', 'Security');

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
    expect(probe).toMatch(/while \(Probe\.Len\(\) > 1 && Probe\.EndsWith\(TEXT\("\/"\)\)\)/u);
    // The root is looked up as a mount, and only a path BELOW it is validated as a package name: the engine's
    // validator refuses a bare root shorter than four characters, which /Game and /Engine never are.
    expect(probe).toContain('FPackageName::MountPointExists(Root + TEXT("/"))');
    expect(probe).toMatch(/bRootMounted && \(bBareRoot \|\| FPackageName::IsValidLongPackageName\(Probe, true, &OutDetail\)\)/u);
  });

  it('does not hand the whole path to the package-name validator any more', () => {
    expect(paths).not.toMatch(/IsValidLongPackageName\(CleanPath/u);
  });

  it('keeps the reason a refusal had, so a caller can say it', () => {
    expect(paths).toMatch(/enum class EMcpPathRejection : uint8 \{[^}]*NotAMountedRoot,[^}]*InvalidName,/u);
    expect(paths).toContain('OutReason = bRootMounted ? EMcpPathRejection::InvalidName : EMcpPathRejection::NotAMountedRoot;');
    // Asking again, to word a message, must not log the refusal twice.
    expect(paths).toContain('FString *OutNormalized, bool bLogRefusal)');
    expect(paths).toContain('return McpClassifyProjectPath(InPath, OutReason, OutDetail, nullptr, true);');
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

  it('names the field, what the caller sent and the helper own reason', () => {
    expect(refusal).toContain('McpPathRefusalMessage(const TCHAR *Label, const FString &RawPath)');
    expect(refusal).toMatch(/McpClassifyProjectPath\(RawPath, &Reason, &Detail, &Normalized,\s+false\)/u);
    expect(refusal).toContain("the path contains a '..' traversal segment.");
    expect(refusal).toContain('Invalid %s \'%s\': %s');
  });

  it('says which root is not mounted and lists the roots that are', () => {
    expect(refusal).toContain('is not a mounted content root');
    expect(refusal).toContain('Mounted roots: %s.');
    expect(refusal).toMatch(/FPackageName::QueryRootContentPaths\(Roots,\s+true,/u);
    // The root is named without its slash: a reply hides an unregistered path, and this is what survives.
    expect(refusal).toContain('Normalized.RightChop(1)');
  });
});
