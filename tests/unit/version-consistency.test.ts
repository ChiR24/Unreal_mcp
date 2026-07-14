import { existsSync, mkdirSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

const readText = (rel: string): string =>
  readFileSync(resolve(process.cwd(), rel), 'utf8');

const readJson = <T>(rel: string): T => JSON.parse(readText(rel)) as T;

// Canonical version source: package.json. Every other version-bearing file is
// compared against it, so a `bump-version` run (which rewrites package.json via
// `npm version`) automatically resyncs this gate — there is no hardcoded
// literal for the workflow to rewrite, which was the latent desync between
// bump-version.yml and this test.
const CANONICAL = readJson<{ version: string }>('package.json').version;

// The complete, coordinated set of advertised version sources. The
// bump-version.yml workflow rewrites exactly this set, so the audit must
// enumerate the same set and report each source by its repository-relative path
// on failure. The extractor returns every version-bearing field a source
// advertises (some files carry the value in more than one place) so a partial
// drift cannot hide behind a second occurrence.
interface VersionSource {
  id: string;
  file: string;
  extract: (text: string) => string[];
}

const upluginVersionName = (text: string): string[] => {
  const json = JSON.parse(text) as { VersionName: string };
  return [json.VersionName];
};

const SERVER_FACTORY_FALLBACK = 'src/server/server-factory.ts';
const NATIVE_TRANSPORT_FALLBACK =
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Transport/McpNativeTransport.h';

const SOURCES: VersionSource[] = [
  {
    id: 'package.json',
    file: 'package.json',
    extract: (text) => [(JSON.parse(text) as { version: string }).version],
  },
  {
    id: 'package-lock.json',
    file: 'package-lock.json',
    extract: (text) => {
      const json = JSON.parse(text) as {
        version: string;
        packages: Record<string, { version: string }>;
      };
      return [json.version, json.packages[''].version];
    },
  },
  {
    id: 'server.json',
    file: 'server.json',
    extract: (text) => {
      const json = JSON.parse(text) as {
        version: string;
        packages: Array<{ version: string }>;
      };
      return [json.version, json.packages[0].version];
    },
  },
  {
    id: 'McpAutomationBridge.uplugin',
    file: 'plugins/McpAutomationBridge/McpAutomationBridge.uplugin',
    extract: upluginVersionName,
  },
  {
    id: 'UnrealAgent.uplugin',
    file: 'plugins/UnrealAgent/UnrealAgent.uplugin',
    extract: upluginVersionName,
  },
  {
    id: 'server-info.json',
    file: 'plugins/McpAutomationBridge/Resources/MCP/server-info.json',
    extract: (text) => [(JSON.parse(text) as { version: string }).version],
  },
  {
    id: 'server-factory.ts',
    file: SERVER_FACTORY_FALLBACK,
    extract: (text) => {
      const match = text.match(
        /const SERVER_VERSION =[\s\S]*?:\s*'([0-9]+\.[0-9]+\.[0-9]+)';/,
      );
      if (!match) {
        throw new Error(
          `SERVER_VERSION fallback literal not found in ${SERVER_FACTORY_FALLBACK}`,
        );
      }
      return [match[1]];
    },
  },
  {
    id: 'McpNativeTransport.h',
    file: NATIVE_TRANSPORT_FALLBACK,
    extract: (text) => {
      const match = text.match(
        /ServerVersion\s*=\s*TEXT\(\s*"([0-9]+\.[0-9]+\.[0-9]+)"\s*\)/,
      );
      if (!match) {
        throw new Error(
          `ServerVersion TEXT fallback literal not found in ${NATIVE_TRANSPORT_FALLBACK}`,
        );
      }
      return [match[1]];
    },
  },
];

const readVersions = (source: VersionSource, rel: string): string[] =>
  source.extract(readText(rel));

const EXPECTED_IDS = [
  'package.json',
  'package-lock.json',
  'server.json',
  'McpAutomationBridge.uplugin',
  'UnrealAgent.uplugin',
  'server-info.json',
  'server-factory.ts',
  'McpNativeTransport.h',
];

describe('version source consistency', () => {
  it('treats package.json as the canonical semver source', () => {
    expect(CANONICAL).toMatch(/^[0-9]+\.[0-9]+\.[0-9]+$/);
  });

  it('enumerates exactly the eight coordinated version sources', () => {
    expect(SOURCES.map((source) => source.id)).toEqual(EXPECTED_IDS);
  });

  // Table-driven: each source is audited independently so a drift in any single
  // source fails exactly that row and reports its repository-relative path.
  for (const source of SOURCES) {
    it(`keeps ${source.file} in sync with ${CANONICAL}`, () => {
      const versions = readVersions(source, source.file);
      expect(
        versions.length,
        `no version extracted from ${source.file}`,
      ).toBeGreaterThan(0);
      for (const found of versions) {
        expect(
          found,
          `${source.file} reports ${found}, expected ${CANONICAL}`,
        ).toBe(CANONICAL);
      }
    });
  }

  it('agrees across all eight version sources', () => {
    const all = SOURCES.flatMap((source) => readVersions(source, source.file));
    expect(all.length, 'no version sources extracted').toBeGreaterThan(0);
    expect(
      new Set(all).size,
      `version mismatch across sources: ${JSON.stringify(all)}`,
    ).toBe(1);
    expect(all[0]).toBe(CANONICAL);
  });

  it('detects a divergent fixture and names its path', () => {
    const fixtureRel = 'tests/unit/.tmp-fixture/McpAutomationBridge.uplugin';
    const fixtureDir = resolve(process.cwd(), 'tests/unit/.tmp-fixture');
    mkdirSync(fixtureDir, { recursive: true });
    try {
      const original = readText(
        'plugins/McpAutomationBridge/McpAutomationBridge.uplugin',
      );
      const mutated = original.replace(
        /"VersionName":\s*"[^"]+"/,
        '"VersionName": "0.5.31"',
      );
      writeFileSync(resolve(process.cwd(), fixtureRel), mutated, 'utf8');

      const source = SOURCES.find(
        (candidate) => candidate.id === 'McpAutomationBridge.uplugin',
      );
      expect(source, 'fixture source not found in SOURCES').toBeDefined();
      if (!source) return;

      const versions = readVersions(source, fixtureRel);
      const audit = (): void => {
        for (const found of versions) {
          expect(
            found,
            `${source.file} reports ${found}, expected ${CANONICAL}`,
          ).toBe(CANONICAL);
        }
      };
      expect(audit, `${source.file} should fail the audit`).toThrow();
      expect(versions).toContain('0.5.31');
    } finally {
      rmSync(fixtureDir, { recursive: true, force: true });
    }
    expect(existsSync(fixtureDir)).toBe(false);
  });

  it('bump-version.yml rewrites the identical eight-source set', () => {
    const workflow = readText('.github/workflows/bump-version.yml');
    for (const source of SOURCES) {
      expect(
        workflow.includes(source.file),
        `bump-version.yml does not reference ${source.file}`,
      ).toBe(true);
    }
  });
});
