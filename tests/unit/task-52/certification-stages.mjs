// @ts-check
// tests/unit/task-52/certification-stages.mjs
// Task 52 — the individual stages of a disposable certification, each one able to
// FAIL LOUDLY on its own terms.
//
// A certification is a chain of expensive, slow, destructive steps. The failure
// mode this file is built against is the one where a stage half-works and the
// chain carries on: the package silently produces yesterday's zip, the build
// reuses a binary from another minor, the editor never binds its port, the
// automation run truncates after four tests. Every one of those reads as a green
// run downstream unless the stage itself refuses.
//
// So each stage returns a RECORD, never a boolean, and the two facts that
// actually distinguish a real run from a truncated one — started == completed,
// and a Result table with rows in it — are computed here rather than inferred by
// a caller from an exit code. A fully green Unreal automation run can exit
// non-zero; judging by exit code alone marks it failed, and judging by "it
// printed something" marks a truncated run passed.

import { execFileSync, spawn } from 'node:child_process';
import { createHash } from 'node:crypto';
import { existsSync, mkdirSync, readFileSync, readdirSync, statSync, writeFileSync } from 'node:fs';
import { connect } from 'node:net';
import { dirname, join } from 'node:path';

/** @param {string} file */
export function sha256File(file) {
  return createHash('sha256').update(readFileSync(file)).digest('hex');
}

/**
 * Run a command, recording it verbatim. The exit code is RECORDED, never treated
 * as the verdict — that judgement belongs to the stage that knows what the
 * command means.
 * @param {{ file: string, args: readonly string[], cwd?: string, env?: NodeJS.ProcessEnv,
 *   timeoutMs?: number, logFile?: string, tail?: number }} spec
 */
export function runCommand(spec) {
  const startedAt = Date.now();
  /** @type {Record<string, unknown>} */
  const record = {
    cmd: [spec.file, ...spec.args].join(' '),
    cwd: spec.cwd ?? process.cwd(),
    startedAt: new Date(startedAt).toISOString(),
  };
  let stdout = '';
  let stderr = '';
  try {
    stdout = String(execFileSync(spec.file, [...spec.args], {
      cwd: spec.cwd, env: spec.env, encoding: 'utf8',
      timeout: spec.timeoutMs ?? 3_600_000, maxBuffer: 256 * 1024 * 1024,
    }));
    record.exitCode = 0;
  } catch (error) {
    const failure = /** @type {any} */ (error);
    record.exitCode = failure.status ?? null;
    record.signal = failure.signal ?? null;
    stdout = String(failure.stdout ?? '');
    stderr = String(failure.stderr ?? failure.message ?? '');
  }
  record.ms = Date.now() - startedAt;
  record.stdoutTail = stdout.slice(-(spec.tail ?? 4000));
  if (stderr.length > 0) record.stderrTail = stderr.slice(-(spec.tail ?? 4000));
  if (spec.logFile !== undefined) {
    mkdirSync(dirname(spec.logFile), { recursive: true });
    writeFileSync(spec.logFile, `${stdout}\n----- stderr -----\n${stderr}`);
    record.logFile = spec.logFile;
  }
  return { record, stdout, stderr };
}

/**
 * Package the CURRENT plugin tree for one engine, into this run's own output dir.
 *
 * The archive is hashed here rather than trusted from the manifest the packaging
 * script writes, so the recorded digest is of the bytes that exist now.
 * @param {{ repoRoot: string, engineRoot: string, outDir: string, timeoutMs?: number, logFile?: string }} spec
 */
export function packagePlugin(spec) {
  mkdirSync(spec.outDir, { recursive: true });
  const { record, stdout } = runCommand({
    file: join(spec.repoRoot, 'scripts/package-plugin.sh'),
    // -WaitForUATMutex: AutomationTool is a single instance per engine. Without
    // this an overlapping build anywhere on the host fails this stage instantly
    // with "a conflicting instance is already running" — which is true, points at
    // the wrong run, and turns a scheduling overlap into a fake certification
    // failure. Waiting is the correct coordination; tearing down the other build
    // is not, because it may not be ours.
    args: [spec.engineRoot, spec.outDir, '-WaitForUATMutex'],
    cwd: spec.repoRoot,
    timeoutMs: spec.timeoutMs ?? 5_400_000,
    logFile: spec.logFile,
  });
  const produced = existsSync(spec.outDir) ? readdirSync(spec.outDir) : [];
  const archive = produced.find((entry) => entry.endsWith('.zip')) ?? null;
  const manifest = produced.find((entry) => entry.endsWith('.manifest.json')) ?? null;
  return {
    ok: record.exitCode === 0 && archive !== null,
    command: record,
    outDir: spec.outDir,
    archive: archive === null ? null : join(spec.outDir, archive),
    archiveSha256: archive === null ? null : sha256File(join(spec.outDir, archive)),
    archiveBytes: archive === null ? null : statSync(join(spec.outDir, archive)).size,
    manifest: manifest === null ? null : JSON.parse(readFileSync(join(spec.outDir, manifest), 'utf8')),
    detail: archive === null ? `no .zip appeared in ${spec.outDir}; packaging produced ${produced.join(', ') || 'nothing'}` : null,
    stdoutTail: stdout.slice(-2000),
  };
}

/**
 * Write the generated project and unpack the freshly-packaged plugin into it.
 * @param {{ projectDir: string, name: string, files: Record<string, string>, archive: string }} spec
 */
export function materializeProject(spec) {
  for (const [relative, contents] of Object.entries(spec.files)) {
    const target = join(spec.projectDir, relative);
    mkdirSync(dirname(target), { recursive: true });
    writeFileSync(target, contents);
  }
  const pluginsDir = join(spec.projectDir, 'Plugins');
  mkdirSync(pluginsDir, { recursive: true });
  const { record } = runCommand({ file: 'unzip', args: ['-q', '-o', spec.archive, '-d', pluginsDir], timeoutMs: 600_000 });
  const pluginRoot = join(pluginsDir, 'McpAutomationBridge');
  return {
    ok: record.exitCode === 0 && existsSync(join(pluginRoot, 'McpAutomationBridge.uplugin')),
    command: record,
    projectDir: spec.projectDir,
    projectFile: join(spec.projectDir, `${spec.name}.uproject`),
    pluginRoot,
    detail: existsSync(pluginRoot) ? null : `${pluginRoot} does not exist after unpacking ${spec.archive}`,
  };
}

/**
 * Compile the project's editor target with this engine's own UBT.
 * @param {{ engineRoot: string, projectFile: string, target: string, logFile?: string, timeoutMs?: number }} spec
 */
export function buildEditorTarget(spec) {
  const build = join(spec.engineRoot, 'Engine/Build/BatchFiles/Linux/Build.sh');
  const { record, stdout, stderr } = runCommand({
    file: build,
    args: [spec.target, 'Linux', 'Development', `-project=${spec.projectFile}`, '-waitmutex'],
    timeoutMs: spec.timeoutMs ?? 7_200_000,
    logFile: spec.logFile,
  });
  const text = `${stdout}\n${stderr}`;
  return {
    ok: record.exitCode === 0,
    command: record,
    buildTool: build,
    errors: [...text.matchAll(/^.*\berror\b.*$/gimu)].map((match) => match[0].trim()).slice(0, 20),
    detail: record.exitCode === 0 ? null : `UBT exited ${String(record.exitCode)}`,
  };
}

/**
 * Is the compiled plugin binary NEWER than the plugin sources it came from?
 *
 * This is the stale-binary gate. A run that links yesterday's .so and then
 * certifies today's tree is the single most convincing way to certify nothing at
 * all, and nothing downstream of the build can see it.
 * @param {{ binary: string, sourceRoot: string }} spec
 */
export function judgeBinaryFreshness(spec) {
  if (!existsSync(spec.binary)) {
    return { fresh: false, reason: 'MISSING_BINARY', binary: spec.binary, builtAtMs: null, newestInput: null, newestInputMtimeMs: null, staleByMs: null };
  }
  /** @type {{ file: string, mtimeMs: number }|null} */
  let newest = null;
  /** @param {string} dir */
  const walk = (dir) => {
    for (const entry of readdirSync(dir, { withFileTypes: true })) {
      const path = join(dir, entry.name);
      if (entry.isDirectory()) {
        walk(path);
        continue;
      }
      if (!/\.(?:h|cpp|inl|cs|uplugin)$/u.test(entry.name)) continue;
      const mtimeMs = statSync(path).mtimeMs;
      if (newest === null || mtimeMs > newest.mtimeMs) newest = { file: path, mtimeMs };
    }
  };
  if (existsSync(spec.sourceRoot)) walk(spec.sourceRoot);
  const builtAtMs = statSync(spec.binary).mtimeMs;
  const newestInputMtimeMs = newest === null ? null : /** @type {{mtimeMs:number}} */ (newest).mtimeMs;
  const staleByMs = newestInputMtimeMs === null ? 0 : newestInputMtimeMs - builtAtMs;
  return {
    fresh: staleByMs <= 0,
    reason: staleByMs <= 0 ? 'FRESH' : 'STALE_BINARY',
    binary: spec.binary,
    binarySha256: sha256File(spec.binary),
    builtAtMs,
    newestInput: newest === null ? null : /** @type {{file:string}} */ (newest).file,
    newestInputMtimeMs,
    staleByMs,
  };
}

/** The Result= values Unreal writes for a test that did not fail. */
export const PASSING_RESULTS = new Set(['Success', 'Passed']);

/**
 * Read the Unreal automation Result table out of a run's log.
 *
 * `startedEqualsCompleted` is the load-bearing field. Unreal happily stops
 * scheduling after an ensure or a hang and the log still ends tidily; a suite
 * that started 214 tests and completed 4 is a FAILED run that looks like a fast
 * one, and only this comparison catches it.
 * @param {string} text
 */
export function parseAutomationLog(text) {
  const started = [...text.matchAll(/Test Started\.\s*Name=\{([^}]*)\}/gu)].map((match) => match[1]);
  const completed = [...text.matchAll(/Test Completed\.\s*Result=\{(\w+)\}\s*Name=\{([^}]*)\}/gu)]
    .map((match) => ({ result: String(match[1]), name: String(match[2]) }));
  /** @type {Record<string, number>} */
  const tally = {};
  for (const row of completed) tally[row.result] = (tally[row.result] ?? 0) + 1;
  // Unreal writes Result={Success} for a passing test; the automation UI shows
  // "Passed". Treating only "Passed" as a pass reported all 84 green tests as
  // failures on the first live run — a suite that cries wolf gets ignored, which
  // costs more than the bug it was watching for.
  const summary = /(\d+)\s+Test\(?s\)?\s+(?:Completed|Run)/iu.exec(text);
  return {
    startedCount: started.length,
    completedCount: completed.length,
    startedEqualsCompleted: started.length === completed.length && started.length > 0,
    tally,
    failed: completed.filter((row) => !PASSING_RESULTS.has(row.result)).map((row) => row.name),
    resultTable: completed,
    engineSummaryLine: summary === null ? null : summary[0],
    sawQueueEmpty: /Automation Test Queue Empty|Tests Completed In/iu.test(text),
  };
}

/** Is anything listening on this loopback port right now? @param {number} port */
export function portAnswers(port, timeoutMs = 1000) {
  return new Promise((settle) => {
    const socket = connect({ host: '127.0.0.1', port });
    const finish = (/** @type {boolean} */ answered) => {
      socket.destroy();
      settle(answered);
    };
    socket.setTimeout(timeoutMs, () => finish(false));
    socket.on('connect', () => finish(true));
    socket.on('error', () => finish(false));
  });
}

/**
 * Wait for a port to start answering, or give up and say so.
 * @param {{ port: number, timeoutMs?: number, intervalMs?: number }} spec
 */
export async function waitForPort(spec) {
  const deadline = Date.now() + (spec.timeoutMs ?? 300_000);
  let attempts = 0;
  while (Date.now() < deadline) {
    attempts += 1;
    if (await portAnswers(spec.port)) return { ready: true, attempts, waitedMs: null };
    await new Promise((settle) => { setTimeout(settle, spec.intervalMs ?? 2000); });
  }
  return { ready: false, attempts, waitedMs: spec.timeoutMs ?? 300_000 };
}

/**
 * Launch an editor in its OWN session, as its own session leader.
 *
 * A new session is not tidiness. Cleanup has to end THIS editor and only this
 * editor; a signal aimed at a process group this run does not own could reach an
 * editor another lane is using, and there is no undo for that.
 *
 * `detached: true` is setsid(2) — Node calls it in the forked child before exec,
 * so the EDITOR becomes the session and group leader and `child.pid` names it.
 * The first live run instead spawned the `setsid` BINARY with the editor as its
 * argument. setsid forks, the parent exits immediately, and the editor is
 * re-parented with a pid nobody recorded: `process.kill(-child.pid)` then
 * signalled an empty group, cleanup reported the editor gone because its pid was
 * gone, and a real editor survived the run against a deleted project. Same
 * syscall, one fewer hop, and the pid in the evidence is the pid that matters.
 * @param {{ engineRoot: string, projectFile: string, args: readonly string[],
 *   logFile: string, env?: NodeJS.ProcessEnv }} spec
 */
export function launchEditor(spec) {
  const editor = join(spec.engineRoot, 'Engine/Binaries/Linux/UnrealEditor-Cmd');
  mkdirSync(dirname(spec.logFile), { recursive: true });
  const child = spawn(editor, [spec.projectFile, ...spec.args], {
    detached: true, stdio: ['ignore', 'pipe', 'pipe'], env: spec.env ?? process.env,
  });
  let log = '';
  const collect = (/** @type {Buffer} */ chunk) => {
    log += String(chunk);
    if (log.length > 64 * 1024 * 1024) log = log.slice(-32 * 1024 * 1024);
  };
  child.stdout?.on('data', collect);
  child.stderr?.on('data', collect);
  return {
    child,
    pid: child.pid ?? null,
    command: [editor, spec.projectFile, ...spec.args].join(' '),
    logFile: spec.logFile,
    text: () => log,
    flush: () => {
      writeFileSync(spec.logFile, log);
      return spec.logFile;
    },
  };
}
