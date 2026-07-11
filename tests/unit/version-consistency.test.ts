import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

const readText = (rel: string): string =>
  readFileSync(resolve(process.cwd(), rel), 'utf8');

const readJson = <T>(rel: string): T =>
  JSON.parse(readText(rel)) as T;

// Canonical version source: package.json. Every other version-bearing file is
// compared against it, so a `bump-version` run (which rewrites package.json via
// `npm version`) automatically resyncs this gate — there is no hardcoded
// literal for the workflow to rewrite, which was the latent desync between
// bump-version.yml and this test.
const CANONICAL = readJson<{ version: string }>('package.json').version;

describe('version source consistency', () => {
  it('treats package.json as the canonical semver source', () => {
    expect(CANONICAL).toMatch(/^[0-9]+\.[0-9]+\.[0-9]+$/);
  });

  it('keeps server.json versions in sync (top-level + npm package)', () => {
    const server = readJson<{ version: string; packages: Array<{ version: string }> }>(
      'server.json',
    );
    expect(server.version).toBe(CANONICAL);
    expect(server.packages[0].version).toBe(CANONICAL);
  });

  it('keeps the plugin .uplugin VersionName in sync', () => {
    const uplugin = readJson<{ VersionName: string }>(
      'plugins/McpAutomationBridge/McpAutomationBridge.uplugin',
    );
    expect(uplugin.VersionName).toBe(CANONICAL);
  });

  it('keeps the server-factory.ts SERVER_VERSION fallback in sync', () => {
    const source = readText('src/server/server-factory.ts');
    const match = source.match(
      /const SERVER_VERSION =[\s\S]*?:\s*'([0-9]+\.[0-9]+\.[0-9]+)';/,
    );
    expect(match, 'SERVER_VERSION fallback literal not found').not.toBeNull();
    expect(match?.[1]).toBe(CANONICAL);
  });

  it('agrees across all four version sources (derived from package.json)', () => {
    const pkg = readJson<{ version: string }>('package.json').version;
    const server = readJson<{ version: string; packages: Array<{ version: string }> }>(
      'server.json',
    );
    const uplugin = readJson<{ VersionName: string }>(
      'plugins/McpAutomationBridge/McpAutomationBridge.uplugin',
    ).VersionName;
    const source = readText('src/server/server-factory.ts');
    const factory = source.match(
      /const SERVER_VERSION =[\s\S]*?:\s*'([0-9]+\.[0-9]+\.[0-9]+)';/,
    )?.[1];

    const all = [pkg, server.version, server.packages[0].version, uplugin, factory];
    for (const v of all) {
      expect(v).toBe(CANONICAL);
    }
  });
});
