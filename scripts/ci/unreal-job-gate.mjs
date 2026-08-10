#!/usr/bin/env node
//
// Decide, for one optional Unreal-dependent CI job, whether it must run, must
// be skipped, or must fail loudly.
//
// Why this exists as an executable instead of a `if:` expression: a job that
// silently skips looks exactly like a job that passed. A grey "Skipped" badge
// carries no statement about what was or was not verified, and required-check
// logic frequently treats it as satisfied. So the decision is made here, in
// something that can be run locally and tested in both directions.
//
// Exactly three verdicts, never blurred:
//
//   skip  The opt-in switch is OFF, so Unreal genuinely is not available.
//         Exit 0 -- hosted CI must stay green without an engine -- but say
//         out loud that NOTHING was verified.
//   fail  The opt-in switch is ON but the configuration cannot work. Exit 1.
//         Turning a job on and then having it quietly skip is precisely the
//         false green this gate exists to prevent, so a misconfigured job is
//         never allowed to degrade into a skip.
//   run   The opt-in switch is ON and the configuration is usable.
//
// Secret handling: the engine root arrives through env, never through a
// job-level `if:` (a secret in an `if:` is readable in the workflow file and
// does not function as a gate anyway). Its VALUE is never printed -- only
// whether it was supplied, and which validation rejected it.

import { existsSync, statSync, appendFileSync } from 'node:fs';
import { isAbsolute, join } from 'node:path';

// GitHub-hosted runner labels. A live Unreal job that lands on one of these
// has no engine and no persistent workspace, so running it there would be a
// misconfiguration rather than a capability.
const HOSTED_RUNNER_LABELS = new Set([
  'ubuntu-latest',
  'ubuntu-24.04',
  'ubuntu-22.04',
  'ubuntu-20.04',
  'windows-latest',
  'windows-2025',
  'windows-2022',
  'windows-2019',
  'macos-latest',
  'macos-15',
  'macos-14',
  'macos-13',
]);

const VERDICT_SKIP = 'skip';
const VERDICT_RUN = 'run';
const VERDICT_FAIL = 'fail';

const MODE_ANNOUNCE = 'announce';
const MODE_PREFLIGHT = 'preflight';

const KIND_PACKAGE = 'package';
const KIND_LIVE = 'live';

function parseArgs(argv) {
  const args = { mode: MODE_ANNOUNCE };
  for (const raw of argv) {
    if (raw.startsWith('--mode=')) {
      args.mode = raw.slice('--mode='.length);
    }
  }
  if (args.mode !== MODE_ANNOUNCE && args.mode !== MODE_PREFLIGHT) {
    throw new Error(`unknown --mode: ${args.mode} (expected ${MODE_ANNOUNCE} or ${MODE_PREFLIGHT})`);
  }
  return args;
}

function env(name) {
  const value = process.env[name];
  return typeof value === 'string' ? value.trim() : '';
}

// The switch is deliberately "non-empty means on", byte-for-byte the same
// rule as the `if: vars.X != ''` job condition. If the two disagreed, a job
// could start and then talk itself back into a skip.
function isEnabled(value) {
  return value !== '';
}

function validateEngineRoot(root, mode) {
  if (root === '') {
    return 'ENGINE_ROOT_MISSING';
  }
  if (!isAbsolute(root)) {
    return 'ENGINE_ROOT_NOT_ABSOLUTE';
  }
  // Filesystem checks only mean something on the machine that owns the
  // engine. The announce job runs on a hosted runner and must not pretend to
  // know whether a self-hosted path exists.
  if (mode !== MODE_PREFLIGHT) {
    return null;
  }
  if (!existsSync(root) || !statSync(root).isDirectory()) {
    return 'ENGINE_ROOT_NOT_FOUND';
  }
  const runUatPosix = join(root, 'Engine', 'Build', 'BatchFiles', 'RunUAT.sh');
  const runUatWindows = join(root, 'Engine', 'Build', 'BatchFiles', 'RunUAT.bat');
  if (!existsSync(runUatPosix) && !existsSync(runUatWindows)) {
    return 'ENGINE_RUNUAT_MISSING';
  }
  if (!existsSync(join(root, 'Engine', 'Build', 'Build.version'))) {
    return 'ENGINE_BUILD_VERSION_MISSING';
  }
  return null;
}

function validateRunnerLabel(label) {
  if (label === '') {
    return 'RUNNER_LABEL_MISSING';
  }
  if (HOSTED_RUNNER_LABELS.has(label.toLowerCase())) {
    return 'RUNNER_LABEL_HOSTED';
  }
  return null;
}

export function decide(inputs) {
  const {
    mode = MODE_ANNOUNCE,
    kind = KIND_PACKAGE,
    enabled = '',
    engineRoot = '',
    runnerLabel = '',
  } = inputs;

  if (!isEnabled(enabled)) {
    // A running job can never honestly report "skipped": its `if:` already
    // decided it was enabled. Reaching preflight with the switch off means the
    // env plumbing is wrong, and a wrong skip is the defect being guarded.
    if (mode === MODE_PREFLIGHT) {
      return { verdict: VERDICT_FAIL, reason: 'OPT_IN_DISABLED_IN_RUNNING_JOB' };
    }
    return { verdict: VERDICT_SKIP, reason: 'OPT_IN_DISABLED' };
  }

  if (kind === KIND_LIVE) {
    const labelReason = validateRunnerLabel(runnerLabel);
    if (labelReason !== null) {
      return { verdict: VERDICT_FAIL, reason: labelReason };
    }
  }

  const rootReason = validateEngineRoot(engineRoot, mode);
  if (rootReason !== null) {
    return { verdict: VERDICT_FAIL, reason: rootReason };
  }

  return { verdict: VERDICT_RUN, reason: 'CONFIGURED' };
}

// Each reason is a complete sentence, so the rendered line reads correctly
// whichever verdict it is attached to.
const REASON_TEXT = {
  OPT_IN_DISABLED:
    'its opt-in repository variable is unset, so no Unreal Engine is available to this workflow',
  OPT_IN_DISABLED_IN_RUNNING_JOB:
    'the job started with its opt-in switch reading as unset, and the gate refuses to report a skip from inside a running job',
  ENGINE_ROOT_MISSING:
    'the job is switched ON but no engine root was supplied (set the UNREAL_ENGINE_ROOT secret, not just the variable)',
  ENGINE_ROOT_NOT_ABSOLUTE:
    'the job is switched ON but the supplied engine root is not an absolute path',
  ENGINE_ROOT_NOT_FOUND:
    'the job is switched ON but the supplied engine root does not exist on this runner',
  ENGINE_RUNUAT_MISSING:
    'the job is switched ON but the supplied engine root has no Engine/Build/BatchFiles/RunUAT script, so it is not an engine root',
  ENGINE_BUILD_VERSION_MISSING:
    'the job is switched ON but the supplied engine root has no Engine/Build/Build.version, so its version cannot be identified',
  RUNNER_LABEL_MISSING:
    'live Unreal jobs are switched ON but no runner label variable was supplied',
  RUNNER_LABEL_HOSTED:
    'live Unreal jobs are switched ON but resolved to a GitHub-hosted runner, which has no engine',
  CONFIGURED: 'its opt-in switch is on and its configuration validates',
};

function render(job, mode, result, engineRootProvided) {
  const lines = [];
  lines.push(
    `UNREAL_GATE verdict=${result.verdict} job=${job} mode=${mode} reason=${result.reason} engineRootProvided=${engineRootProvided}`,
  );
  const why = REASON_TEXT[result.reason] ?? result.reason;
  if (result.verdict === VERDICT_SKIP) {
    lines.push(`SKIPPED: "${job}" did not run because ${why}.`);
    lines.push('SKIPPED: this is NOT a pass -- nothing about Unreal was verified by this run.');
  } else if (result.verdict === VERDICT_FAIL) {
    lines.push(`FAILED: "${job}" cannot run because ${why}.`);
    lines.push('FAILED: an enabled job must not degrade into a skip, so this run is red on purpose.');
  } else {
    lines.push(`RUN: "${job}" will run because ${why}.`);
  }
  return lines;
}

function publish(name, value) {
  const target = env(name);
  if (target === '') {
    return;
  }
  try {
    appendFileSync(target, value, 'utf8');
  } catch {
    // A missing summary/output file must never turn a green gate red; the
    // verdict has already been written to stdout, which is the real record.
  }
}

function main() {
  const args = parseArgs(process.argv.slice(2));
  const job = env('UNREAL_GATE_JOB') || 'unreal-optional';
  const kind = env('UNREAL_GATE_KIND') || KIND_PACKAGE;
  const enabled = env('UNREAL_GATE_ENABLED');
  const engineRoot = env('UNREAL_ENGINE_ROOT');
  const runnerLabel = env('UNREAL_GATE_RUNNER_LABEL');

  const result = decide({ mode: args.mode, kind, enabled, engineRoot, runnerLabel });
  const lines = render(job, args.mode, result, engineRoot !== '');
  const text = `${lines.join('\n')}\n`;

  if (result.verdict === VERDICT_FAIL) {
    process.stderr.write(text);
  } else {
    process.stdout.write(text);
  }

  // Never emit the engine root itself -- only the verdict and the reason.
  publish('GITHUB_STEP_SUMMARY', `${lines.map((line) => `- ${line}`).join('\n')}\n`);
  publish('GITHUB_OUTPUT', `verdict=${result.verdict}\nreason=${result.reason}\n`);

  process.exitCode = result.verdict === VERDICT_FAIL ? 1 : 0;
}

const invokedDirectly = process.argv[1] !== undefined
  && import.meta.url === new URL(`file://${process.argv[1]}`).href;

if (invokedDirectly) {
  try {
    main();
  } catch (error) {
    const message = error instanceof Error ? error.message : 'unknown error';
    process.stderr.write(`UNREAL_GATE verdict=fail reason=GATE_ERROR\nFAILED: ${message}\n`);
    process.exit(1);
  }
}
