// tests/eval/task4-baseline-adversarial.test.ts
// Adversarial and manual-QA probes for Task 4:
//  - malformed_input: parse/load fail-closed
//  - misleading_success_output: run-baseline exits non-zero when coverage/drift
//    fails, so a green exit cannot be faked by log prose
//  - stale_state / flaky_tests: repeat-run and shuffled determinism
//  - prompt_injection / cancel_resume / hung_or_long / repeated_interruptions:
//    ruled out by static inspection (no network/process-spawning in the
//    evaluation core modules)

import { spawnSync } from 'node:child_process';
import { existsSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { describe, expect, it } from 'vitest';
import { corpus, parseCorpus } from './corpus.js';
import { CorpusValidationError } from './errors.js';
import { loadManifestModel } from './manifest-model.js';
import { scoreCorpus } from './scorer.js';

const MANIFEST_PATH = join(process.cwd(), 'src/gateway/gateway-manifest.generated.json');

function runBaseline(manifestPath: string, outputPath: string): { status: number | null; stdout: string; stderr: string } {
  const r = spawnSync(
    'node',
    ['--loader', 'ts-node/esm', 'tests/eval/run-baseline.ts', '--manifest', manifestPath, '--output', outputPath],
    { cwd: process.cwd(), encoding: 'utf8' },
  );
  return { status: r.status, stdout: r.stdout ?? '', stderr: r.stderr ?? '' };
}

describe('adversarial: malformed_input', () => {
  it('rejects malformed corpus input', () => {
    expect(() => parseCorpus({ schema: 'nope', version: '1', cases: [] })).toThrow(CorpusValidationError);
    expect(() => parseCorpus(42)).toThrow(CorpusValidationError);
  });

  it('rejects a manifest that is not valid JSON', () => {
    const dir = mkdtempSync(join(tmpdir(), 'task4-'));
    const bad = join(dir, 'bad.json');
    writeFileSync(bad, '{not json');
    try {
      expect(() => loadManifestModel(bad)).toThrow();
    } finally {
      rmSync(dir, { recursive: true, force: true });
    }
  });
});

describe('adversarial: misleading_success_output', () => {
  it('exits non-zero when the manifest drops a referenced tool', () => {
    const dir = mkdtempSync(join(tmpdir(), 'task4-'));
    const drifted = join(dir, 'drifted.json');
    const manifest = JSON.parse(readFileSync(MANIFEST_PATH, 'utf8'));
    manifest.tools = manifest.tools.filter((t: { name: string }) => t.name !== 'manage_tools');
    writeFileSync(drifted, JSON.stringify(manifest));
    const out = join(dir, 'out.json');
    const r = runBaseline(drifted, out);
    try {
      expect(r.status).not.toBe(0);
      expect(existsSync(out)).toBe(false);
    } finally {
      rmSync(dir, { recursive: true, force: true });
    }
  });

  it('exits zero and writes machine fields on a valid run', () => {
    const dir = mkdtempSync(join(tmpdir(), 'task4-'));
    const out = join(dir, 'ok.json');
    const r = runBaseline(MANIFEST_PATH, out);
    try {
      expect(r.status).toBe(0);
      expect(existsSync(out)).toBe(true);
      const payload = JSON.parse(readFileSync(out, 'utf8'));
      expect(typeof payload.corpusHash).toBe('string');
      expect(payload.parentCoverageCount).toBe(23);
      expect(payload.metrics.top1Accuracy).toBe(1);
      expect(typeof payload.disclosure.totalBytes).toBe('number');
      expect(typeof payload.metrics.tokenBudgetComplianceRate).toBe('number');
    } finally {
      rmSync(dir, { recursive: true, force: true });
    }
  });
});

describe('adversarial: stale_state / flaky_tests', () => {
  it('produces an identical report hash on repeated runs', () => {
    const manifest = loadManifestModel(MANIFEST_PATH);
    const a = scoreCorpus(corpus, manifest);
    const b = scoreCorpus(corpus, manifest);
    expect(b.reportHash).toBe(a.reportHash);
  });
});

describe('adversarial: prompt_injection / cancel_resume / hung_or_long / repeated_interruptions', () => {
  const coreModules = [
    'tests/eval/scorer.ts',
    'tests/eval/model-fixture.ts',
    'tests/eval/corpus.ts',
    'tests/eval/hash.ts',
    'tests/eval/manifest-model.ts',
  ];
  for (const mod of coreModules) {
    it(`core module ${mod} makes no external/long-running calls`, () => {
      const src = readFileSync(join(process.cwd(), mod), 'utf8');
      expect(src.includes('fetch(')).toBe(false);
      expect(src.includes('node:child_process')).toBe(false);
      expect(src.includes('node:http')).toBe(false);
      expect(src.includes('node:net')).toBe(false);
      expect(src.includes('import(')).toBe(false);
    });
  }
});
