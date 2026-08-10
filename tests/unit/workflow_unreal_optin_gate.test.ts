import { spawnSync } from 'node:child_process';
import { mkdtempSync, mkdirSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join, resolve } from 'node:path';

import { afterAll, beforeAll, describe, expect, it } from 'vitest';

// The gate is executed, not string-matched. A workflow contract that only
// reads YAML can assert that a skip is *configured*; it cannot show that the
// skip happens for the right reason, and it cannot tell a job that skips
// because Unreal is absent from a job that skips unconditionally.
const GATE = resolve(process.cwd(), 'scripts/ci/unreal-job-gate.mjs');

interface GateInput {
  readonly mode: 'announce' | 'preflight';
  readonly enabled?: string;
  readonly kind?: 'package' | 'live';
  readonly engineRoot?: string;
  readonly runnerLabel?: string;
  readonly job?: string;
}

interface GateResult {
  readonly status: number;
  readonly stdout: string;
  readonly stderr: string;
  readonly output: string;
  readonly verdict: string;
  readonly reason: string;
}

function runGate(input: GateInput): GateResult {
  // Start from a copy with every gate variable stripped, so a value leaking in
  // from the developer's shell cannot quietly decide the verdict.
  const env: Record<string, string> = {};
  for (const [key, value] of Object.entries(process.env)) {
    if (value !== undefined && !key.startsWith('UNREAL_') && key !== 'GITHUB_STEP_SUMMARY' && key !== 'GITHUB_OUTPUT') {
      env[key] = value;
    }
  }
  env.UNREAL_GATE_JOB = input.job ?? 'test-job';
  env.UNREAL_GATE_KIND = input.kind ?? 'package';
  env.UNREAL_GATE_ENABLED = input.enabled ?? '';
  env.UNREAL_ENGINE_ROOT = input.engineRoot ?? '';
  env.UNREAL_GATE_RUNNER_LABEL = input.runnerLabel ?? '';

  const proc = spawnSync(process.execPath, [GATE, `--mode=${input.mode}`], {
    env,
    encoding: 'utf8',
  });

  const stdout = proc.stdout ?? '';
  const stderr = proc.stderr ?? '';
  const output = `${stdout}${stderr}`;
  const verdict = /verdict=([a-z]+)/.exec(output)?.[1] ?? '';
  const reason = /reason=([A-Z_]+)/.exec(output)?.[1] ?? '';

  return { status: proc.status ?? -1, stdout, stderr, output, verdict, reason };
}

// A directory that looks exactly like an engine root to the gate.
let validEngineRoot = '';
// An existing directory that is not an engine root.
let notAnEngineRoot = '';
// An engine root missing only Build.version.
let engineRootWithoutVersion = '';
let scratch = '';

beforeAll(() => {
  scratch = mkdtempSync(join(tmpdir(), 'mcp-ci-gate-'));

  validEngineRoot = join(scratch, 'engine');
  mkdirSync(join(validEngineRoot, 'Engine', 'Build', 'BatchFiles'), { recursive: true });
  writeFileSync(join(validEngineRoot, 'Engine', 'Build', 'BatchFiles', 'RunUAT.sh'), '#!/bin/sh\n');
  writeFileSync(
    join(validEngineRoot, 'Engine', 'Build', 'Build.version'),
    JSON.stringify({ MajorVersion: 5, MinorVersion: 7, PatchVersion: 4 }),
  );

  notAnEngineRoot = join(scratch, 'plain-dir');
  mkdirSync(notAnEngineRoot, { recursive: true });

  engineRootWithoutVersion = join(scratch, 'engine-no-version');
  mkdirSync(join(engineRootWithoutVersion, 'Engine', 'Build', 'BatchFiles'), { recursive: true });
  writeFileSync(join(engineRootWithoutVersion, 'Engine', 'Build', 'BatchFiles', 'RunUAT.sh'), '#!/bin/sh\n');
});

afterAll(() => {
  if (scratch !== '') {
    rmSync(scratch, { recursive: true, force: true });
  }
});

describe('unavailable Unreal skips truthfully', () => {
  it('skips with exit 0 when the opt-in switch is unset', () => {
    const result = runGate({ mode: 'announce', enabled: '' });

    expect(result.status).toBe(0);
    expect(result.verdict).toBe('skip');
    expect(result.reason).toBe('OPT_IN_DISABLED');
  });

  it('says out loud that a skip verified nothing', () => {
    const result = runGate({ mode: 'announce', enabled: '', job: 'package-plugin' });

    // The whole point of the criterion: a reader must be able to tell this
    // apart from a pass. A bare exit code cannot.
    expect(result.stdout).toContain('SKIPPED');
    expect(result.stdout).toContain('package-plugin');
    expect(result.stdout).toContain('NOT a pass');
    expect(result.stdout).toContain('nothing about Unreal was verified');
  });

  it('keeps hosted CI green when no engine is available', () => {
    for (const kind of ['package', 'live'] as const) {
      const result = runGate({ mode: 'announce', kind, enabled: '' });
      expect(result.status, `${kind} must not fail hosted CI`).toBe(0);
    }
  });
});

describe('enabled but misconfigured fails loudly', () => {
  it('fails when the switch is on but no engine root was supplied', () => {
    const result = runGate({ mode: 'announce', enabled: 'yes', engineRoot: '' });

    expect(result.status).toBe(1);
    expect(result.verdict).toBe('fail');
    expect(result.reason).toBe('ENGINE_ROOT_MISSING');
    expect(result.stderr).toContain('FAILED');
  });

  it('fails when the engine root is not absolute', () => {
    const result = runGate({ mode: 'announce', enabled: 'yes', engineRoot: 'relative/engine' });

    expect(result.status).toBe(1);
    expect(result.reason).toBe('ENGINE_ROOT_NOT_ABSOLUTE');
  });

  it('fails when the engine root does not exist on the runner', () => {
    const result = runGate({
      mode: 'preflight',
      enabled: 'yes',
      engineRoot: join(scratch, 'definitely-absent'),
    });

    expect(result.status).toBe(1);
    expect(result.reason).toBe('ENGINE_ROOT_NOT_FOUND');
  });

  it('fails when the path exists but is not an engine root', () => {
    const result = runGate({ mode: 'preflight', enabled: 'yes', engineRoot: notAnEngineRoot });

    expect(result.status).toBe(1);
    expect(result.reason).toBe('ENGINE_RUNUAT_MISSING');
  });

  it('fails when the engine root cannot be version-identified', () => {
    const result = runGate({ mode: 'preflight', enabled: 'yes', engineRoot: engineRootWithoutVersion });

    expect(result.status).toBe(1);
    expect(result.reason).toBe('ENGINE_BUILD_VERSION_MISSING');
  });

  it('fails when live tests are enabled without a runner label', () => {
    const result = runGate({
      mode: 'announce',
      kind: 'live',
      enabled: 'yes',
      engineRoot: validEngineRoot,
      runnerLabel: '',
    });

    expect(result.status).toBe(1);
    expect(result.reason).toBe('RUNNER_LABEL_MISSING');
  });

  it('fails when live tests resolve to a GitHub-hosted runner', () => {
    for (const label of ['ubuntu-latest', 'windows-latest', 'macos-latest']) {
      const result = runGate({
        mode: 'announce',
        kind: 'live',
        enabled: 'yes',
        engineRoot: validEngineRoot,
        runnerLabel: label,
      });

      expect(result.status, `${label} must not be accepted as an Unreal runner`).toBe(1);
      expect(result.reason).toBe('RUNNER_LABEL_HOSTED');
    }
  });

  it('refuses to report a skip from inside an already-running job', () => {
    // The job's `if:` already decided it was enabled. Reaching preflight with
    // the switch off means the plumbing is broken, and a broken gate that
    // reports "skipped" is the false green this whole gate exists to stop.
    const result = runGate({ mode: 'preflight', enabled: '' });

    expect(result.status).toBe(1);
    expect(result.verdict).toBe('fail');
    expect(result.reason).toBe('OPT_IN_DISABLED_IN_RUNNING_JOB');
  });
});

describe('the two directions are actually distinguished', () => {
  it('reaches the run verdict for a valid enabled configuration', () => {
    // The mutation guard. A gate hardcoded to skip, or one that skips whenever
    // anything is imperfect, cannot produce this result -- so this single
    // assertion is what makes the skip tests above mean something.
    const result = runGate({ mode: 'preflight', enabled: 'yes', engineRoot: validEngineRoot });

    expect(result.status).toBe(0);
    expect(result.verdict).toBe('run');
    expect(result.reason).toBe('CONFIGURED');
    expect(result.stdout).not.toContain('SKIPPED');
  });

  it('never reports a skip once the switch is on', () => {
    const enabledConfigurations: readonly GateInput[] = [
      { mode: 'announce', enabled: 'yes', engineRoot: '' },
      { mode: 'announce', enabled: 'yes', engineRoot: 'relative/engine' },
      { mode: 'preflight', enabled: 'yes', engineRoot: notAnEngineRoot },
      { mode: 'preflight', enabled: 'yes', engineRoot: engineRootWithoutVersion },
      { mode: 'preflight', enabled: 'yes', engineRoot: validEngineRoot },
      { mode: 'announce', kind: 'live', enabled: 'yes', engineRoot: validEngineRoot, runnerLabel: '' },
      {
        mode: 'announce',
        kind: 'live',
        enabled: 'yes',
        engineRoot: validEngineRoot,
        runnerLabel: 'ubuntu-latest',
      },
    ];

    for (const configuration of enabledConfigurations) {
      const result = runGate(configuration);
      expect(result.verdict, `enabled configuration must not skip: ${JSON.stringify(configuration)}`)
        .not.toBe('skip');
    }
  });

  it('maps each verdict onto the exit code a workflow reads', () => {
    expect(runGate({ mode: 'announce', enabled: '' }).status).toBe(0);
    expect(runGate({ mode: 'preflight', enabled: 'yes', engineRoot: validEngineRoot }).status).toBe(0);
    expect(runGate({ mode: 'preflight', enabled: 'yes', engineRoot: notAnEngineRoot }).status).toBe(1);
  });
});

describe('the gate never discloses the engine root', () => {
  it('reports only whether a root was supplied, never its value', () => {
    const secretish = join(scratch, 'engine');
    const result = runGate({ mode: 'preflight', enabled: 'yes', engineRoot: secretish });

    expect(result.verdict).toBe('run');
    expect(result.output).toContain('engineRootProvided=true');
    expect(result.output).not.toContain(secretish);
  });

  it('does not echo the root even when it is rejected', () => {
    const rejected = join(scratch, 'plain-dir');
    const result = runGate({ mode: 'preflight', enabled: 'yes', engineRoot: rejected });

    expect(result.verdict).toBe('fail');
    expect(result.output).not.toContain(rejected);
  });
});
