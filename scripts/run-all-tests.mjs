#!/usr/bin/env node
import { spawn } from 'node:child_process';

const tests = [
  'test:control_actor', // Pass1 // Pass2
  'test:control_editor', // Pass1 // Pass2
  'test:manage_level', // Pass
  'test:animation', // Pass
  'test:materials', // Pass
  'test:niagara', // Pass
  'test:landscape', // Pass
  'test:sequence', // Pass
  'test:system', // Pass
  'test:console_command', // Pass
  'test:inspect', // Pass
  'test:manage_asset', // Pass
  'test:blueprint', // Pass1 // Pass2
  'test:blueprint_graph', // Pass
  'test:graphql', // Pass
  'test:wasm:all', // Pass
  'test:no-inline-python',
  'test:plugin-handshake', // Pass
  'test:asset_advanced', // Pass
  'test:render', // Pass
  'test:world_partition' // Pass
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
