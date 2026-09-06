// tests/unit/gateway-manifest-pilot-check.test.ts
// Pilot --check no-write contracts and removed-record detection.
// Every test induces a REAL failure or proves a REAL contract.

import { existsSync, mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join, resolve } from 'node:path';
import { afterEach, beforeEach, describe, expect, it } from 'vitest';
import { pilotJson, pilotTsText } from '../../scripts/gateway-manifest/pilot.js';
import { runPilotCheck } from '../../scripts/generate-gateway-manifest.js';
import { secondCapabilitySource, validCapabilitySource } from '../../src/tools/catalog/capabilities/capability-record.test-support.js';
import { createCapabilityRecord } from '../../src/tools/catalog/capabilities/index.js';

function makeRecord(id: string) {
  const source = validCapabilitySource();
  return createCapabilityRecord({ ...source, id });
}

function makeSecondRecord() {
  return createCapabilityRecord(secondCapabilitySource());
}

function makeTempDir(): string {
  return mkdtempSync(join(tmpdir(), 'gw-pilot-check-'));
}

describe('gateway-manifest pilot --check never writes', () => {
  let savedCatalogPath: string | undefined;
  let savedOutputDir: string | undefined;
  let savedExitCode: number | string | null | undefined;

  beforeEach(() => {
    savedCatalogPath = process.env.MCP_PILOT_CATALOG_PATH;
    savedOutputDir = process.env.MCP_PILOT_OUTPUT_DIR;
    savedExitCode = process.exitCode;
    process.exitCode = undefined;
  });

  afterEach(() => {
    if (savedCatalogPath === undefined) delete process.env.MCP_PILOT_CATALOG_PATH;
    else process.env.MCP_PILOT_CATALOG_PATH = savedCatalogPath;
    if (savedOutputDir === undefined) delete process.env.MCP_PILOT_OUTPUT_DIR;
    else process.env.MCP_PILOT_OUTPUT_DIR = savedOutputDir;
    process.exitCode = savedExitCode;
  });

  it('reports stale outputs as drift WITHOUT writing (files unchanged)', () => {
    const dir = makeTempDir();
    try {
      const records = [makeRecord('asset.delete'), makeSecondRecord()];
      const catalogPath = join(dir, 'catalog.json');
      writeFileSync(catalogPath, JSON.stringify(records));
      process.env.MCP_PILOT_CATALOG_PATH = catalogPath;

      const outputDir = join(dir, 'pilot-out');
      mkdirSync(outputDir, { recursive: true });
      process.env.MCP_PILOT_OUTPUT_DIR = outputDir;

      const staleJson = 'STALE-JSON';
      const staleTs = 'STALE-TS';
      writeFileSync(join(outputDir, 'pilot-manifest.json'), staleJson);
      writeFileSync(join(outputDir, 'pilot-manifest.ts'), staleTs);

      const result = runPilotCheck(resolve(process.cwd()));

      expect(result.kind).toBe('drift');
      expect(process.exitCode).toBe(1);

      expect(readFileSync(join(outputDir, 'pilot-manifest.json'), 'utf8')).toBe(staleJson);
      expect(readFileSync(join(outputDir, 'pilot-manifest.ts'), 'utf8')).toBe(staleTs);
    } finally {
      rmSync(dir, { recursive: true, force: true });
    }
  });

  it('reports missing outputs as drift WITHOUT creating any files', () => {
    const dir = makeTempDir();
    try {
      const records = [makeRecord('asset.delete'), makeSecondRecord()];
      const catalogPath = join(dir, 'catalog.json');
      writeFileSync(catalogPath, JSON.stringify(records));
      process.env.MCP_PILOT_CATALOG_PATH = catalogPath;

      const outputDir = join(dir, 'pilot-out');
      mkdirSync(outputDir, { recursive: true });
      process.env.MCP_PILOT_OUTPUT_DIR = outputDir;

      const result = runPilotCheck(resolve(process.cwd()));

      expect(result.kind).toBe('drift');
      expect(process.exitCode).toBe(1);
      if (result.kind === 'drift') {
        expect(result.entries.every((e) => e.kind === 'missing')).toBe(true);
      }

      expect(existsSync(join(outputDir, 'pilot-manifest.json'))).toBe(false);
      expect(existsSync(join(outputDir, 'pilot-manifest.ts'))).toBe(false);
    } finally {
      rmSync(dir, { recursive: true, force: true });
    }
  });

  it('reports no drift and writes nothing when outputs are up to date', () => {
    const dir = makeTempDir();
    try {
      const records = [makeRecord('asset.delete'), makeSecondRecord()];
      const catalogPath = join(dir, 'catalog.json');
      writeFileSync(catalogPath, JSON.stringify(records));
      process.env.MCP_PILOT_CATALOG_PATH = catalogPath;

      const outputDir = join(dir, 'pilot-out');
      mkdirSync(outputDir, { recursive: true });
      process.env.MCP_PILOT_OUTPUT_DIR = outputDir;

      const expectedJson = pilotJson(records);
      const expectedTs = pilotTsText(records);
      writeFileSync(join(outputDir, 'pilot-manifest.json'), expectedJson);
      writeFileSync(join(outputDir, 'pilot-manifest.ts'), expectedTs);

      const result = runPilotCheck(resolve(process.cwd()));

      // No drift: exit code not set to 1.
      expect(result.kind).toBe('up_to_date');
      expect(process.exitCode).toBeUndefined();

      expect(readFileSync(join(outputDir, 'pilot-manifest.json'), 'utf8')).toBe(expectedJson);
      expect(readFileSync(join(outputDir, 'pilot-manifest.ts'), 'utf8')).toBe(expectedTs);
    } finally {
      rmSync(dir, { recursive: true, force: true });
    }
  });

  it('surfaces the exact removed canonical ID and leaves prior outputs byte-identical', () => {
    const dir = makeTempDir();
    try {
      // Setup: write valid pilot JSON/TS for two records.
      const records = [makeRecord('asset.delete'), makeSecondRecord()];
      const catalogPath = join(dir, 'catalog.json');
      writeFileSync(catalogPath, JSON.stringify(records));
      process.env.MCP_PILOT_CATALOG_PATH = catalogPath;

      const outputDir = join(dir, 'pilot-out');
      mkdirSync(outputDir, { recursive: true });
      process.env.MCP_PILOT_OUTPUT_DIR = outputDir;

      const priorJson = pilotJson(records);
      const priorTs = pilotTsText(records);
      writeFileSync(join(outputDir, 'pilot-manifest.json'), priorJson);
      writeFileSync(join(outputDir, 'pilot-manifest.ts'), priorTs);

      // Rewrite canonical input with one record removed.
      const remainingRecords = [makeRecord('asset.delete')];
      writeFileSync(catalogPath, JSON.stringify(remainingRecords));

      // Call pilot check.
      const result = runPilotCheck(resolve(process.cwd()));

      // The removed record ('actor.delete') must be surfaced by exact ID.
      expect(result.kind).toBe('validation_error');
      expect(process.exitCode).toBe(1);
      if (result.kind === 'validation_error') {
        const removedError = result.errors.find((e) => e.canonicalId === 'actor.delete');
        expect(removedError).toBeDefined();
        expect(removedError?.message).toContain('actor.delete');
      }

      // Both prior output files remain byte-identical (no writes).
      expect(readFileSync(join(outputDir, 'pilot-manifest.json'), 'utf8')).toBe(priorJson);
      expect(readFileSync(join(outputDir, 'pilot-manifest.ts'), 'utf8')).toBe(priorTs);
    } finally {
      rmSync(dir, { recursive: true, force: true });
    }
  });
});

describe('gateway-manifest pilot --check invalid prior pilot output warns and falls back to stale drift', () => {
  let savedCatalogPath: string | undefined;
  let savedOutputDir: string | undefined;
  let savedExitCode: number | string | null | undefined;
  let savedConsoleWarn: typeof console.warn;
  const warnings: string[] = [];

  beforeEach(() => {
    savedCatalogPath = process.env.MCP_PILOT_CATALOG_PATH;
    savedOutputDir = process.env.MCP_PILOT_OUTPUT_DIR;
    savedExitCode = process.exitCode;
    savedConsoleWarn = console.warn;
    process.exitCode = undefined;
    warnings.length = 0;
    console.warn = (msg: string) => { warnings.push(String(msg)); };
  });

  afterEach(() => {
    if (savedCatalogPath === undefined) delete process.env.MCP_PILOT_CATALOG_PATH;
    else process.env.MCP_PILOT_CATALOG_PATH = savedCatalogPath;
    if (savedOutputDir === undefined) delete process.env.MCP_PILOT_OUTPUT_DIR;
    else process.env.MCP_PILOT_OUTPUT_DIR = savedOutputDir;
    process.exitCode = savedExitCode;
    console.warn = savedConsoleWarn;
  });

  it('emits an actionable warning and falls back to stale drift when prior pilot JSON is invalid', () => {
    const dir = makeTempDir();
    try {
      const records = [makeRecord('asset.delete'), makeSecondRecord()];
      const catalogPath = join(dir, 'catalog.json');
      writeFileSync(catalogPath, JSON.stringify(records));
      process.env.MCP_PILOT_CATALOG_PATH = catalogPath;

      const outputDir = join(dir, 'pilot-out');
      mkdirSync(outputDir, { recursive: true });
      process.env.MCP_PILOT_OUTPUT_DIR = outputDir;

      const invalidJson = '{ this is not valid json';
      writeFileSync(join(outputDir, 'pilot-manifest.json'), invalidJson);
      const priorTs = 'PRIOR-TS';
      writeFileSync(join(outputDir, 'pilot-manifest.ts'), priorTs);

      const result = runPilotCheck(resolve(process.cwd()));

      expect(result.kind).toBe('drift');
      expect(warnings.length).toBeGreaterThan(0);
      expect(warnings.some((w) => w.includes('not valid JSON'))).toBe(true);
      expect(warnings.some((w) => w.includes('Falling back to ordinary stale-drift'))).toBe(true);

      expect(readFileSync(join(outputDir, 'pilot-manifest.json'), 'utf8')).toBe(invalidJson);
      expect(readFileSync(join(outputDir, 'pilot-manifest.ts'), 'utf8')).toBe(priorTs);
    } finally {
      rmSync(dir, { recursive: true, force: true });
    }
  });

  it('emits an actionable warning and falls back to stale drift when prior pilot JSON fails schema validation', () => {
    const dir = makeTempDir();
    try {
      const records = [makeRecord('asset.delete'), makeSecondRecord()];
      const catalogPath = join(dir, 'catalog.json');
      writeFileSync(catalogPath, JSON.stringify(records));
      process.env.MCP_PILOT_CATALOG_PATH = catalogPath;

      const outputDir = join(dir, 'pilot-out');
      mkdirSync(outputDir, { recursive: true });
      process.env.MCP_PILOT_OUTPUT_DIR = outputDir;

      const schemaInvalid = JSON.stringify({ version: 1, source: 'pilot:capabilityRecords', tools: 'not-an-array' });
      writeFileSync(join(outputDir, 'pilot-manifest.json'), schemaInvalid);
      const priorTs = 'PRIOR-TS';
      writeFileSync(join(outputDir, 'pilot-manifest.ts'), priorTs);

      const result = runPilotCheck(resolve(process.cwd()));

      expect(result.kind).toBe('drift');
      expect(warnings.some((w) => w.includes('failed schema validation'))).toBe(true);
      expect(warnings.some((w) => w.includes('Falling back to ordinary stale-drift'))).toBe(true);

      expect(readFileSync(join(outputDir, 'pilot-manifest.json'), 'utf8')).toBe(schemaInvalid);
      expect(readFileSync(join(outputDir, 'pilot-manifest.ts'), 'utf8')).toBe(priorTs);
    } finally {
      rmSync(dir, { recursive: true, force: true });
    }
  });
});
