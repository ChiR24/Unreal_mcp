import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';
import { load } from 'js-yaml';

const CI_WORKFLOW = resolve(process.cwd(), '.github/workflows/ci.yml');

// A full commit SHA pin looks like `owner/repo@<40 hex chars>` (a trailing
// `# comment` is stripped by the YAML parser, so the value ends at the SHA).
const FULL_SHA_RE = /@[0-9a-f]{40}$/;

interface StepLike {
  name?: unknown;
  uses?: unknown;
  with?: unknown;
  run?: unknown;
}

interface StrategyLike {
  matrix?: Record<string, unknown>;
}

interface JobLike {
  'runs-on'?: unknown;
  strategy?: StrategyLike;
  steps?: unknown;
}

interface WorkflowLike {
  jobs?: Record<string, JobLike>;
}

function parseWorkflow(path: string): WorkflowLike {
  const raw = readFileSync(path, 'utf8');
  const doc = load(raw);
  return (doc ?? {}) as WorkflowLike;
}

// The dedicated Node-version test matrix job (Todo 8).
const MATRIX_JOB = 'test-matrix';

// Gate commands that must appear, in this exact order, within the job's
// `run:` steps. Each is matched as a substring (run steps may be multiline).
const EXPECTED_GATES: ReadonlyArray<{ id: string; cmd: string }> = [
  { id: 'eslint', cmd: 'npx eslint . --max-warnings=0' },
  { id: 'type-check', cmd: 'npm run type-check' },
  { id: 'vitest', cmd: 'npx vitest run' },
  { id: 'build', cmd: 'npm run build' },
  { id: 'smoke', cmd: 'npm run test:smoke' },
];

// Recursively collect every `uses:` string value from the parsed document.
function collectUses(node: unknown, acc: string[]): void {
  if (node === null || node === undefined) {
    return;
  }
  if (typeof node === 'string') {
    return;
  }
  if (Array.isArray(node)) {
    for (const item of node) {
      collectUses(item, acc);
    }
    return;
  }
  if (typeof node === 'object') {
    for (const [key, value] of Object.entries(node)) {
      if (key === 'uses' && typeof value === 'string') {
        acc.push(value);
      } else {
        collectUses(value, acc);
      }
    }
  }
}

function getSteps(job: JobLike): StepLike[] {
  if (!Array.isArray(job.steps)) {
    return [];
  }
  return job.steps as StepLike[];
}

function runStepTexts(job: JobLike): string[] {
  return getSteps(job).map((step) => (typeof step.run === 'string' ? step.run : ''));
}

function firstStepIndexContaining(steps: string[], needle: string): number {
  return steps.findIndex((text) => text.includes(needle));
}

describe('CI test-matrix job contract (ci.yml)', () => {
  const doc = parseWorkflow(CI_WORKFLOW);
  const job = doc.jobs?.[MATRIX_JOB];

  it('defines a dedicated `test-matrix` job', () => {
    expect(job, `ci.yml must define a "${MATRIX_JOB}" job`).toBeDefined();
  });

  it('runs the test-matrix job on ubuntu-latest', () => {
    expect(job?.['runs-on']).toBe('ubuntu-latest');
  });

  it('uses a node-version matrix exactly ["20.19.x", "26.x"]', () => {
    const matrix = job?.strategy?.matrix;
    expect(matrix, 'test-matrix job must declare a strategy.matrix').toBeDefined();

    const nodeVersion = matrix?.['node-version'];
    expect(Array.isArray(nodeVersion), 'matrix.node-version must be a list').toBe(true);

    const versions = nodeVersion as unknown[];
    // A 20.19 entry and a current Node 26 entry must both be present.
    expect(versions).toContain('20.19.x');
    expect(versions).toContain('26.x');
    // And nothing else may be present.
    expect(versions).toEqual(['20.19.x', '26.x']);
  });

  it('caches npm via actions/setup-node', () => {
    const steps = getSteps(job ?? {});
    const setupStep = steps.find(
      (step) => typeof step.uses === 'string' && step.uses.includes('actions/setup-node'),
    );
    expect(setupStep, 'test-matrix must use actions/setup-node').toBeDefined();
    expect(setupStep?.with, 'setup-node step must declare `with:`').toBeDefined();

    const withMap = setupStep?.with as Record<string, unknown> | undefined;
    expect(withMap?.['cache'], 'setup-node must set cache: npm').toBe('npm');
  });

  it('installs dependencies with `npm ci`', () => {
    const steps = runStepTexts(job ?? {});
    const ciIndex = firstStepIndexContaining(steps, 'npm ci');
    expect(ciIndex, 'test-matrix must run `npm ci`').toBeGreaterThanOrEqual(0);
  });

  it('runs the gate commands in the exact required order', () => {
    const steps = runStepTexts(job ?? {});
    const positions = EXPECTED_GATES.map((gate) =>
      firstStepIndexContaining(steps, gate.cmd),
    );

    // Every gate must be present.
    positions.forEach((pos, i) => {
      expect(pos, `missing gate: ${EXPECTED_GATES[i].id} (${EXPECTED_GATES[i].cmd})`).toBeGreaterThanOrEqual(0);
    });

    // And they must appear strictly in order.
    for (let i = 1; i < positions.length; i++) {
      expect(
        positions[i],
        `gate "${EXPECTED_GATES[i].id}" must run after "${EXPECTED_GATES[i - 1].id}"`,
      ).toBeGreaterThan(positions[i - 1]);
    }
  });

  it('pins every `uses:` action to a full 40-character commit SHA', () => {
    const usesValues: string[] = [];
    collectUses(doc, usesValues);

    // The workflow must contain at least one action.
    expect(usesValues.length).toBeGreaterThan(0);

    const unpinned = usesValues.filter((u) => !FULL_SHA_RE.test(u));
    expect(unpinned, 'all `uses:` references must be full-SHA pinned').toEqual([]);
  });
});
