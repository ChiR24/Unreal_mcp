import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

const readText = (rel: string): string => readFileSync(resolve(process.cwd(), rel), 'utf8');

// package.json is canonical. Every other source is compared against it, so a
// bump-version run (which rewrites package.json via `npm version`) resyncs this
// gate with no literal for the workflow to rewrite.
const CANONICAL = (JSON.parse(readText('package.json')) as { version: string }).version;

const UPLUGIN = 'plugins/McpAutomationBridge/McpAutomationBridge.uplugin';

// The four sources bump-version.yml rewrites. The TS server reads package.json
// and the native server reads the .uplugin VersionName at runtime, so there are
// no hardcoded fallbacks left to track.
const SOURCES: ReadonlyArray<{ file: string; extract: (text: string) => string[] }> = [
  { file: 'package.json', extract: (text) => [(JSON.parse(text) as { version: string }).version] },
  {
    file: 'package-lock.json',
    extract: (text) => {
      const json = JSON.parse(text) as { version: string; packages: Record<string, { version: string }> };
      return [json.version, json.packages['']?.version ?? ''];
    },
  },
  {
    file: 'server.json',
    extract: (text) => {
      const json = JSON.parse(text) as { version: string; packages: Array<{ version: string }> };
      return [json.version, ...json.packages.map((entry) => entry.version)];
    },
  },
  { file: UPLUGIN, extract: (text) => [(JSON.parse(text) as { VersionName: string }).VersionName] },
];

describe('version source consistency', () => {
  it('treats package.json as the canonical semver source', () => {
    expect(CANONICAL).toMatch(/^[0-9]+\.[0-9]+\.[0-9]+(?:-[0-9A-Za-z.-]+)?$/);
  });

  for (const source of SOURCES) {
    it(`keeps ${source.file} in sync with ${CANONICAL}`, () => {
      const versions = source.extract(readText(source.file));
      expect(versions.length, `no version extracted from ${source.file}`).toBeGreaterThan(0);
      for (const found of versions) {
        expect(found, `${source.file} reports ${found}, expected ${CANONICAL}`).toBe(CANONICAL);
      }
    });
  }

  it('keeps the .uplugin numeric Version in step with its VersionName', () => {
    // Unreal compares the integer Version; VersionName is display only.
    const { Version } = JSON.parse(readText(UPLUGIN)) as { Version: number };
    const [major = '0', minor = '0', patch = '0'] = CANONICAL.split('-')[0]?.split('.') ?? [];
    expect(Number.isInteger(Version)).toBe(true);
    expect(Version).toBe(Number(major) * 10000 + Number(minor) * 100 + Number(patch));
  });

  it('bump-version.yml rewrites every source', () => {
    const workflow = readText('.github/workflows/bump-version.yml');
    for (const source of SOURCES) {
      expect(workflow.includes(source.file), `bump-version.yml does not reference ${source.file}`).toBe(true);
    }
  });

  it('the plugin reports its VersionName in bridge_ack, under the name the server reads', () => {
    const ack = readText('plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Transport/Connection/McpConnectionManagerAuthority.cpp');
    expect(ack).toMatch(/SetStringField\(TEXT\("pluginVersion"\), Plugin->GetDescriptor\(\)\.VersionName\)/u);
    expect(readText('src/automation/message-schema.ts')).toContain('metadata?.pluginVersion');
  });
});
