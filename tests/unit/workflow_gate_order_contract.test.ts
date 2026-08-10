import { readdirSync, readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';
import { load } from 'js-yaml';

const WORKFLOW_DIR = resolve(process.cwd(), '.github/workflows');
const CI_WORKFLOW = resolve(WORKFLOW_DIR, 'ci.yml');
const PACKAGE_JSON = resolve(process.cwd(), 'package.json');

const FULL_SHA_RE = /@[0-9a-f]{40}$/;
const GATE_SCRIPT = 'scripts/ci/unreal-job-gate.mjs';

interface StepLike {
  name?: unknown;
  uses?: unknown;
  env?: unknown;
  run?: unknown;
}

interface JobLike {
  'runs-on'?: unknown;
  if?: unknown;
  env?: unknown;
  steps?: unknown;
}

interface WorkflowLike {
  jobs?: Record<string, JobLike>;
}

function parse(path: string): WorkflowLike {
  return (load(readFileSync(path, 'utf8')) ?? {}) as WorkflowLike;
}

function steps(job: JobLike | undefined): StepLike[] {
  return Array.isArray(job?.steps) ? (job?.steps as StepLike[]) : [];
}

function runTexts(job: JobLike | undefined): string[] {
  return steps(job).map((step) => (typeof step.run === 'string' ? step.run : ''));
}

function indexOfCommand(commands: string[], needle: string): number {
  return commands.findIndex((text) => text.includes(needle));
}

function workflowFiles(): string[] {
  return readdirSync(WORKFLOW_DIR)
    .filter((name) => name.endsWith('.yml') || name.endsWith('.yaml'))
    .map((name) => resolve(WORKFLOW_DIR, name));
}

function collectUses(node: unknown, acc: string[]): void {
  if (node === null || typeof node !== 'object') {
    return;
  }
  if (Array.isArray(node)) {
    for (const item of node) {
      collectUses(item, acc);
    }
    return;
  }
  for (const [key, value] of Object.entries(node)) {
    if (key === 'uses' && typeof value === 'string') {
      acc.push(value);
    } else {
      collectUses(value, acc);
    }
  }
}

function collectSecretConditions(node: unknown, acc: string[]): void {
  if (node === null || typeof node !== 'object') {
    return;
  }
  if (Array.isArray(node)) {
    for (const item of node) {
      collectSecretConditions(item, acc);
    }
    return;
  }
  for (const [key, value] of Object.entries(node)) {
    if (key === 'if' && typeof value === 'string' && value.includes('secrets.')) {
      acc.push(value);
    }
    collectSecretConditions(value, acc);
  }
}

// The deterministic gate order. The first three are the pre-existing
// lint/type/unit sequence; everything after them was staged by this task and
// must stay after them.
const DETERMINISTIC_GATES: ReadonlyArray<{ id: string; cmd: string }> = [
  { id: 'eslint', cmd: 'npx eslint . --max-warnings=0' },
  { id: 'type-check', cmd: 'npm run type-check' },
  { id: 'unit', cmd: 'npm run test:unit' },
  { id: 'registry-drift', cmd: 'npm run registry:check' },
  { id: 'normalization-drift', cmd: 'npm run normalization:check' },
  { id: 'manifest-drift', cmd: 'npm run manifest:check' },
  { id: 'command-policy', cmd: 'npm run policy:check' },
  { id: 'exact-parity', cmd: 'npm run test:params' },
  { id: 'migration', cmd: 'npm run migration:check' },
  { id: 'primitive-runtime', cmd: 'npm run primitives:check' },
  { id: 'security', cmd: 'npm run security:check' },
  { id: 'eval-budgets', cmd: 'npm run eval:check' },
  { id: 'version', cmd: 'npm run version:check' },
  { id: 'workflow', cmd: 'npm run workflow:check' },
  { id: 'audit-runtime', cmd: 'npm audit --omit=dev --audit-level=high' },
  { id: 'audit-full', cmd: 'npm audit --audit-level=moderate' },
];

describe('deterministic CI gate order (ci.yml lint job)', () => {
  const doc = parse(CI_WORKFLOW);
  const lintJob = doc.jobs?.['lint'];
  const commands = runTexts(lintJob);

  it('runs every deterministic gate', () => {
    const missing = DETERMINISTIC_GATES.filter((gate) => indexOfCommand(commands, gate.cmd) < 0).map(
      (gate) => gate.id,
    );
    expect(missing).toEqual([]);
  });

  it('keeps the new gates after the existing lint/type/unit sequence', () => {
    const positions = DETERMINISTIC_GATES.map((gate) => indexOfCommand(commands, gate.cmd));
    for (let index = 1; index < positions.length; index += 1) {
      expect(
        positions[index],
        `gate "${DETERMINISTIC_GATES[index].id}" must run after "${DETERMINISTIC_GATES[index - 1].id}"`,
      ).toBeGreaterThan(positions[index - 1]);
    }
  });

  it('resolves every `npm run` gate to a real package script', () => {
    const scripts = (JSON.parse(readFileSync(PACKAGE_JSON, 'utf8')) as {
      scripts?: Record<string, string>;
    }).scripts ?? {};

    const unresolved = DETERMINISTIC_GATES.filter((gate) => gate.cmd.startsWith('npm run '))
      .map((gate) => gate.cmd.slice('npm run '.length))
      .filter((script) => scripts[script] === undefined);

    expect(unresolved).toEqual([]);
  });

  it('gates the canonical registry, which the gateway manifest check does not cover', () => {
    // The manifest gate and the registry gate look alike but guard different
    // artifacts: the manifest covers the gateway manifest, the registry covers
    // the capability records, the TS facades and the generated native shards.
    expect(indexOfCommand(commands, 'npm run registry:check')).toBeGreaterThanOrEqual(0);
    expect(indexOfCommand(commands, 'npm run manifest:check')).toBeGreaterThanOrEqual(0);
  });

  it('runs the evaluation budgets, which the unit include globs never reach', () => {
    expect(indexOfCommand(commands, 'npm run eval:check')).toBeGreaterThanOrEqual(0);
  });
});

describe('hosted CI never requires Unreal', () => {
  const doc = parse(CI_WORKFLOW);

  it('keeps Unreal-dependent commands out of every non-opt-in job', () => {
    const unrealCommands = ['package-plugin.sh', 'npm test'];
    const offenders: string[] = [];

    for (const [jobId, job] of Object.entries(doc.jobs ?? {})) {
      const condition = typeof job.if === 'string' ? job.if : '';
      const isOptIn = condition.includes('vars.UNREAL_');
      if (isOptIn) {
        continue;
      }
      for (const text of runTexts(job)) {
        for (const command of unrealCommands) {
          // `npm test` must not be confused with `npm test:unit`-style names.
          const hit = command === 'npm test' ? /(^|\s)npm test(\s|$)/.test(text) : text.includes(command);
          if (hit) {
            offenders.push(`${jobId}: ${command}`);
          }
        }
      }
    }

    expect(offenders).toEqual([]);
  });

  it('keeps the Node 20.19 / current matrix intact', () => {
    const matrixJob = doc.jobs?.['test-matrix'] as { strategy?: { matrix?: Record<string, unknown> } } | undefined;
    expect(matrixJob?.strategy?.matrix?.['node-version']).toEqual(['20.19.x', '26.x']);
  });

  it('does not publish artifacts or releases from CI', () => {
    const uses: string[] = [];
    collectUses(doc, uses);
    const publishing = uses.filter((value) => /upload-artifact|action-gh-release|publish/i.test(value));
    expect(publishing).toEqual([]);
  });
});

describe('optional Unreal jobs announce rather than silently skip', () => {
  const doc = parse(CI_WORKFLOW);
  const jobs = Object.entries(doc.jobs ?? {});

  const optInJobs = jobs.filter(([, job]) => typeof job.if === 'string' && job.if.includes('vars.UNREAL_'));

  it('defines at least one opt-in Unreal job', () => {
    expect(optInJobs.length).toBeGreaterThan(0);
  });

  it('gates each opt-in job on a repository variable, never a secret', () => {
    for (const [jobId, job] of optInJobs) {
      const condition = job.if as string;
      expect(condition, `${jobId} must be gated by a repository variable`).toContain('vars.UNREAL_');
      expect(condition, `${jobId} must not reference a secret in its condition`).not.toContain('secrets.');
    }
  });

  it('preflights each opt-in job before it does any real work', () => {
    for (const [jobId, job] of optInJobs) {
      const commands = runTexts(job);
      const preflightIndex = commands.findIndex(
        (text) => text.includes(GATE_SCRIPT) && text.includes('--mode=preflight'),
      );
      expect(preflightIndex, `${jobId} must preflight the Unreal gate`).toBeGreaterThanOrEqual(0);

      // Nothing expensive or Unreal-touching may precede the preflight.
      const earlierWork = commands
        .slice(0, preflightIndex)
        .filter((text) => text.trim() !== '');
      expect(earlierWork, `${jobId} must preflight before any other run step`).toEqual([]);
    }
  });

  it('always runs a status job that reports the skip out loud', () => {
    const statusJob = doc.jobs?.['unreal-optional-status'];
    expect(statusJob, 'ci.yml must define an unreal-optional-status job').toBeDefined();

    // If the announcer could itself be skipped, the skip would be invisible
    // again and the whole arrangement would be decorative.
    expect(statusJob?.if, 'the status job must not be conditional').toBeUndefined();

    const announceCommands = runTexts(statusJob).filter(
      (text) => text.includes(GATE_SCRIPT) && text.includes('--mode=announce'),
    );
    expect(announceCommands.length).toBeGreaterThanOrEqual(optInJobs.length);
  });

  it('names every opt-in job in the status report', () => {
    const statusJob = doc.jobs?.['unreal-optional-status'];
    const announcedJobs = steps(statusJob)
      .map((step) => (step.env as Record<string, unknown> | undefined)?.['UNREAL_GATE_JOB'])
      .filter((value): value is string => typeof value === 'string');

    for (const [jobId] of optInJobs) {
      expect(announcedJobs, `${jobId} must appear in the status report`).toContain(jobId);
    }
  });

  it('supplies the engine root through env only', () => {
    for (const [jobId, job] of jobs) {
      for (const step of steps(job)) {
        const env = (step.env ?? {}) as Record<string, unknown>;
        for (const [key, value] of Object.entries(env)) {
          if (typeof value === 'string' && value.includes('secrets.UNREAL_ENGINE_ROOT')) {
            expect(key, `${jobId} must bind the engine root to an env var`).toBeTruthy();
          }
        }
      }
    }
  });
});

describe('workflow security invariants hold across every workflow file', () => {
  const files = workflowFiles();

  it('finds the workflow directory', () => {
    expect(files.length).toBeGreaterThan(0);
  });

  it('pins every action to a full 40-character commit SHA', () => {
    const unpinned: string[] = [];
    for (const file of files) {
      const uses: string[] = [];
      collectUses(parse(file), uses);
      for (const value of uses) {
        // Local composite actions (`./.github/...`) are part of this repo.
        if (value.startsWith('./')) {
          continue;
        }
        if (!FULL_SHA_RE.test(value)) {
          unpinned.push(`${file}: ${value}`);
        }
      }
    }
    expect(unpinned).toEqual([]);
  });

  it('never references a secret from any `if:` condition', () => {
    const offenders: string[] = [];
    for (const file of files) {
      const conditions: string[] = [];
      collectSecretConditions(parse(file), conditions);
      for (const condition of conditions) {
        offenders.push(`${file}: ${condition}`);
      }
    }
    expect(offenders).toEqual([]);
  });
});
