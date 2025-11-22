#!/usr/bin/env node
import { spawn } from 'node:child_process';

const tests = [
  'test:control_actor',
  'test:control_editor',
  'test:manage_level',
  'test:animation',
  'test:materials',
  'test:niagara',
  'test:landscape',
  'test:sequence',
  'test:system',
  'test:console_command',
  'test:inspect',
  'test:manage_asset',
  'test:blueprint'
];

const isWindows = process.platform === 'win32';
const npmExecPath = process.env.npm_execpath;
const npmCommand = isWindows ? 'npm.cmd' : 'npm';

function spawnNpm(cmd) {
  if (npmExecPath && npmExecPath.endsWith('.js')) {
    return spawn(process.execPath, [npmExecPath, 'run', cmd], {
      stdio: 'inherit',
      cwd: process.cwd(),
      env: process.env,
      windowsHide: true
    });
  }
  return spawn(npmCommand, ['run', cmd], {
    stdio: 'inherit',
    cwd: process.cwd(),
    env: process.env,
    windowsHide: true
  });
}

function runOne(cmd) {
  return new Promise((resolve) => {
    const startedAt = Date.now();
    const child = spawnNpm(cmd);

    let errorMsg = '';
    child.on('error', (err) => {
      errorMsg = `Failed to launch npm run ${cmd}: ${err.message}`;
    });

    child.on('exit', (code, signal) => {
      const durationMs = Date.now() - startedAt;
      if (signal) {
        resolve({ name: cmd, ok: false, code: null, signal, durationMs, error: `${cmd} terminated via signal ${signal}` });
      } else {
        resolve({ name: cmd, ok: code === 0, code, signal: null, durationMs, error: code === 0 ? null : (errorMsg || `${cmd} failed with code ${code}`) });
      }
    });

    const forwardSignal = (sig) => {
      if (child.killed) return;
      child.kill(sig);
    };

    process.once('SIGINT', forwardSignal);
    process.once('SIGTERM', forwardSignal);

    child.once('exit', () => {
      process.removeListener('SIGINT', forwardSignal);
      process.removeListener('SIGTERM', forwardSignal);
    });
  });
}

(async () => {
  const results = [];
  for (const t of tests) {
    const res = await runOne(t);
    results.push(res);
    if (!res.ok) {
      console.error(`\n${t} failed (code=${res.code ?? 'n/a'}, signal=${res.signal ?? 'n/a'}) after ${res.durationMs}ms`);
      if (res.error) console.error(res.error);
    }
  }

  // Summary
  const passed = results.filter(r => r.ok).length;
  const failed = results.length - passed;
  console.log('\n============================================================');
  console.log('Test Suite Summary');
  console.log('============================================================');
  for (const r of results) {
    const status = r.ok ? 'PASSED' : 'FAILED';
    console.log(`${status} - ${r.name} (${r.durationMs} ms)`);
  }
  console.log('------------------------------------------------------------');
  console.log(`Total: ${results.length}  Passed: ${passed}  Failed: ${failed}`);
  console.log('============================================================');

  process.exit(failed > 0 ? 1 : 0);
})().catch((err) => {
  console.error(err.message || err);
  process.exit(1);
});
