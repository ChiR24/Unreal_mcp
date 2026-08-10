import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';
import { load } from 'js-yaml';

const CI_WORKFLOW = resolve(process.cwd(), '.github/workflows/ci.yml');
const RELEASE_WORKFLOW = resolve(process.cwd(), '.github/workflows/release.yml');

// A full commit SHA pin looks like `owner/repo@<40 hex chars>` (a trailing
// `# comment` is stripped by the YAML parser, so the value ends at the SHA).
const FULL_SHA_RE = /@[0-9a-f]{40}$/;

const ENGINE_ROOT_SECRET = 'secrets.UNREAL_ENGINE_ROOT';
const GATE_VARIABLE = 'vars.UNREAL_ENGINE_ROOT';

interface JobLike {
  if?: unknown;
  env?: unknown;
  steps?: unknown;
}

interface WorkflowLike {
  jobs?: Record<string, JobLike>;
}

function parseWorkflow(path: string): WorkflowLike {
  const raw = readFileSync(path, 'utf8');
  return load(raw) as WorkflowLike;
}

// Recursively walk the parsed document, collecting every `uses:` string and
// recording any string value containing the engine-root secret together with
// whether that value sits under an `env:` mapping (the only allowed location).
interface WalkResult {
  usesValues: string[];
  secretOutsideEnv: string[];
  secretInEnv: boolean;
}

function walk(node: unknown, parentKey: string | null, inEnv: boolean, acc: WalkResult): void {
  if (node === null || node === undefined) {
    return;
  }
  if (typeof node === 'string') {
    if (parentKey === 'uses') {
      acc.usesValues.push(node);
    }
    if (node.includes(ENGINE_ROOT_SECRET) && !inEnv) {
      acc.secretOutsideEnv.push(node);
    }
    if (node.includes(ENGINE_ROOT_SECRET) && inEnv) {
      acc.secretInEnv = true;
    }
    return;
  }
  if (Array.isArray(node)) {
    for (const item of node) {
      walk(item, parentKey, inEnv, acc);
    }
    return;
  }
  if (typeof node === 'object') {
    for (const [key, value] of Object.entries(node)) {
      // A direct `if:` condition must never reference a secret context.
      if (key === 'if' && typeof value === 'string' && value.includes('secrets.')) {
        acc.secretOutsideEnv.push(`if: ${value}`);
      }
      walk(value, key, inEnv || key === 'env', acc);
    }
    return;
  }
}

function analyse(path: string): WalkResult {
  const doc = parseWorkflow(path);
  const result: WalkResult = { usesValues: [], secretOutsideEnv: [], secretInEnv: false };
  walk(doc, null, false, result);
  return result;
}

describe('Unreal job gate contract (ci.yml / release.yml)', () => {
  const ci = analyse(CI_WORKFLOW);
  const release = analyse(RELEASE_WORKFLOW);

  it('replaces the invalid secret-level job `if` with a repository variable gate', () => {
    const doc = parseWorkflow(CI_WORKFLOW);
    const packagingJob = doc.jobs?.['package-plugin'];
    expect(packagingJob).toBeDefined();

    const jobIf = typeof packagingJob?.if === 'string' ? packagingJob.if : '';
    // The gate must be driven by the documented repository variable, not a secret.
    expect(jobIf).toContain(GATE_VARIABLE);
    expect(jobIf).not.toContain('secrets.');
  });

  it('keeps the engine-root secret out of every job-level `if`', () => {
    // No `if` anywhere may reference a secret context (job or step).
    expect(ci.secretOutsideEnv).toEqual([]);
    expect(release.secretOutsideEnv).toEqual([]);
  });

  it('supplies the engine root only through job/step `env`', () => {
    // The only place the secret may appear is an `env:` mapping.
    const anySecretUsage = [...ci.secretOutsideEnv, ...release.secretOutsideEnv];
    expect(anySecretUsage).toEqual([]);
    expect(ci.secretInEnv).toBe(true);
  });

  it('pins every `uses:` action to a full 40-character commit SHA', () => {
    const ciUnpinned = ci.usesValues.filter((u) => !FULL_SHA_RE.test(u));
    const releaseUnpinned = release.usesValues.filter((u) => !FULL_SHA_RE.test(u));

    expect(ciUnpinned).toEqual([]);
    expect(releaseUnpinned).toEqual([]);

    // Both workflow files must contain at least one SHA-pinned action.
    expect(ci.usesValues.length).toBeGreaterThan(0);
    expect(release.usesValues.length).toBeGreaterThan(0);

    const allPinned = [...ci.usesValues, ...release.usesValues].every((u) => FULL_SHA_RE.test(u));
    expect(allPinned).toBe(true);
  });

  it('does not invent a self-hosted runner label', () => {
    const doc = parseWorkflow(CI_WORKFLOW);
    for (const job of Object.values(doc.jobs ?? {})) {
      // `runs-on` is intentionally a hosted label, never a custom runner group.
      expect(JSON.stringify(job)).not.toMatch(/runs-on:\s*\[.*self-hosted.*\]/);
    }
  });
});
