import assert from 'node:assert/strict';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { StdioClientTransport } from '@modelcontextprotocol/sdk/client/stdio.js';

const repoRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..');
const transport = new StdioClientTransport({
  command: process.execPath,
  args: [path.join(repoRoot, 'dist', 'cli.js')],
  cwd: repoRoot,
  env: {
    ...process.env,
    UE_PROJECT_PATH: 'C:/Users/jeehoon/UnrealEngine_Projects/Missile_demo',
    UE_ENGINE_PATH: 'C:/Program Files/Epic Games/UE_5.7',
    UE_MCP_DEBUG_ALLOW_UNSAFE: 'true'
  }
});
const client = new Client({ name: 'unreal-mcp-debug-live-smoke', version: '1.0.0' }, { capabilities: {} });
let sessionId;

async function call(name, args, expectedSuccess = true) {
  const response = await client.callTool({ name, arguments: args });
  assert.equal(response.structuredContent?.success, expectedSuccess, JSON.stringify(response.structuredContent));
  assert.equal(response.isError === true, !expectedSuccess, JSON.stringify(response));
  return response.structuredContent;
}

async function waitForState(expected, timeoutMs = 20_000) {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    const status = await call('debug_session', { action: 'status', sessionId });
    if (status.session?.state === expected) return status.session;
    await new Promise((resolve) => setTimeout(resolve, 250));
  }
  throw new Error(`debug session ${sessionId} did not reach ${expected}`);
}

async function waitForTargetPid(timeoutMs = 20_000) {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    const status = await call('debug_session', { action: 'status', sessionId });
    if (Number.isInteger(status.session?.targetPid)) return status.session.targetPid;
    await new Promise((resolve) => setTimeout(resolve, 250));
  }
  throw new Error(`debug session ${sessionId} did not report a target PID`);
}

try {
  await client.connect(transport);
  const started = await call('debug_session', {
    action: 'start',
    mode: 'standalone_debug',
    arguments: ['-nosplash']
  });
  sessionId = started.session.sessionId;
  assert.equal(started.session.state, 'running');
  const targetPid = await waitForTargetPid();

  const breakpoint = await call('debug_breakpoint', {
    action: 'upsert',
    sessionId,
    kind: 'function',
    function: 'AMissileActor::SimTick'
  });
  assert.equal(breakpoint.result.breakpoints.length, 1);

  await call('debug_session', { action: 'pause', sessionId });
  await waitForState('stopped');

  const threads = await call('debug_inspect', { action: 'threads', sessionId });
  assert.ok(threads.result.threads.length > 0);
  const threadId = threads.result.threads[0].id;
  const stack = await call('debug_inspect', { action: 'stack', sessionId, threadId, levels: 20 });
  assert.ok(stack.result.stackFrames.length > 0);

  const rejected = await call('debug_inspect', {
    action: 'evaluate',
    sessionId,
    expression: 'Dt = 1.0'
  }, false);
  assert.equal(rejected.diagnostic.code, 'UNSAFE_EXPRESSION_REJECTED');

  const healthStarted = performance.now();
  const health = await client.readResource({ uri: 'ue://debug/health' });
  const healthLatencyMs = performance.now() - healthStarted;
  assert.ok(health.contents.length > 0);
  assert.ok(healthLatencyMs < 1000, `health latency was ${healthLatencyMs.toFixed(1)}ms`);

  await call('debug_session', { action: 'continue', sessionId });
  await waitForState('running');
  await call('debug_session', { action: 'pause', sessionId });
  await waitForState('stopped');
  const resumedThreads = await call('debug_inspect', { action: 'threads', sessionId });
  const resumedThreadId = resumedThreads.result.threads[0].id;
  await call('debug_session', { action: 'next', sessionId, threadId: resumedThreadId });
  await new Promise((resolve) => setTimeout(resolve, 250));

  const unsafeRejected = await call('debug_session', {
    action: 'stop',
    sessionId,
    terminate: true
  }, false);
  assert.equal(unsafeRejected.diagnostic.code, 'UNSAFE_PERMISSION_REQUIRED');

  const stopped = await call('debug_session', {
    action: 'stop',
    sessionId,
    terminate: true,
    unsafe: true
  });
  assert.equal(stopped.session.state, 'terminated');
  console.log(JSON.stringify({ sessionId, targetPid, healthLatencyMs }, null, 2));
  sessionId = undefined;
} finally {
  if (sessionId) {
    try {
      await client.callTool({
        name: 'debug_session',
        arguments: { action: 'stop', sessionId, terminate: true, unsafe: true }
      });
    } catch { /* best-effort cleanup */ }
  }
  await client.close();
}
