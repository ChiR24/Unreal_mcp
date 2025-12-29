#!/usr/bin/env node
import { spawn } from 'node:child_process';

const tests = [
  'test:control_actor',
  'test:control_editor',
  'test:manage_level', // crashed 1
  'test:materials', // 1
  'test:niagara', // 1
  'test:animation',
  'test:landscape',
  'test:sequence', // crashed 1
  'test:system',
  'test:console_command',
  'test:inspect', // 3
  'test:manage_asset', // 1
  'test:blueprint', // 1
  'test:blueprint_graph', // 2
  'test:audio', // crashed 1
  'test:behavior_tree', // 1
  'test:lighting',
  'test:performance',
  'test:input',
  'test:asset_graph', // 2
  'test:graphql',
  'test:wasm:all',
  'test:no-inline-python',
  'test:plugin-handshake',
  'test:asset_advanced',
  'test:world_partition' // 2
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

    process.once('SIGTERM', forwardSignal);

    child.once('exit', () => {
      process.removeListener('SIGINT', forwardSignal);
      process.removeListener('SIGTERM', forwardSignal);
    });
  });
}

(async () => {
  // Pass 0: Build once
  console.log('\n============================================================');
  console.log('Running Pre-Test Build');
  console.log('============================================================');

  // Use runOne to execute the build command
  // We can't use npm run build directly if it's not in the 'tests' array, so let's just trigger it.
  const buildRes = await runOne('build');
  if (!buildRes.ok) {
    console.error(`\nBuild failed (code=${buildRes.code ?? 'n/a'}). Aborting tests.`);
    process.exit(1);
  }
  console.log('Build completed successfully. Disabling auto-build for tests.');

  // Set the environment variable to disable auto-build for subsequent tests
  process.env.UNREAL_MCP_NO_AUTO_BUILD = '1';
  // Force use of the just-built dist folder to prevent falling back to ts-node
  // (which can happen if build:wasm updates timestamps in src/ after build:core)
  process.env.UNREAL_MCP_FORCE_DIST = '1';

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
