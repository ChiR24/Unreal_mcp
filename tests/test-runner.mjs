import fs from 'node:fs/promises';
import path from 'node:path';
import process from 'node:process';
import { fileURLToPath } from 'node:url';
import { performance } from 'node:perf_hooks';
import net from 'node:net';
import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { StdioClientTransport } from '@modelcontextprotocol/sdk/client/stdio.js';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const repoRoot = path.resolve(__dirname, '..');
const reportsDir = path.resolve(repoRoot, 'tests', 'reports');

const failureKeywords = [
  'error', 'fail', 'invalid', 'missing', 'not found', 'reject', 'warning'
];

const successKeywords = [
  'success', 'spawn', 'visible', 'applied', 'returns', 'plays', 'updates', 'created', 'saved'
];

// Tests always run against a real MCP server + Automation Bridge.

// Defaults for spawning the MCP server.
let serverCommand = process.env.UNREAL_MCP_SERVER_CMD ?? 'node';
let serverArgs = process.env.UNREAL_MCP_SERVER_ARGS ? process.env.UNREAL_MCP_SERVER_ARGS.split(',') : [path.join(repoRoot, 'dist', 'cli.js')];
const serverCwd = process.env.UNREAL_MCP_SERVER_CWD ?? repoRoot;
const serverEnv = Object.assign({}, process.env);

function formatResultLine(testCase, status, detail, durationMs) {
  const durationText = typeof durationMs === 'number' ? ` (${durationMs.toFixed(1)} ms)` : '';
  return `[${status.toUpperCase()}] ${testCase.scenario}${durationText}${detail ? ` => ${detail}` : ''}`;
}

async function persistResults(toolName, results) {
  await fs.mkdir(reportsDir, { recursive: true });
  const timestamp = new Date().toISOString().replace(/[:]/g, '-');
  const resultsPath = path.join(reportsDir, `${toolName}-test-results-${timestamp}.json`);
  const serializable = results.map((result) => ({
    scenario: result.scenario,
    toolName: result.toolName,
    arguments: result.arguments,
    status: result.status,
    durationMs: result.durationMs,
    detail: result.detail
  }));
  await fs.writeFile(resultsPath, JSON.stringify({ generatedAt: new Date().toISOString(), toolName, results: serializable }, null, 2));
  return resultsPath;
}

function summarize(toolName, results, resultsPath) {
  const totals = results.reduce((acc, result) => { acc.total += 1; acc[result.status] = (acc[result.status] ?? 0) + 1; return acc; }, { total: 0, passed: 0, failed: 0, skipped: 0 });
  console.log('\n' + '='.repeat(60));
  console.log(`${toolName} Test Summary`);
  console.log('='.repeat(60));
  console.log(`Total cases: ${totals.total}`);
  console.log(`✅ Passed: ${totals.passed ?? 0}`);
  console.log(`❌ Failed: ${totals.failed ?? 0}`);
  console.log(`⏭️  Skipped: ${totals.skipped ?? 0}`);
  if (totals.passed && totals.total > 0) console.log(`Pass rate: ${((totals.passed / totals.total) * 100).toFixed(1)}%`);
  console.log(`Results saved to: ${resultsPath}`);
  console.log('='.repeat(60));
}



// Mock response generator removed — tests always call the real server.

/**
 * Evaluates whether a test case passed based on expected outcome
 */
function evaluateExpectation(testCase, response) {
  const lowerExpected = testCase.expected.toLowerCase();
  const containsFailure = failureKeywords.some((word) => lowerExpected.includes(word));
  const containsSuccess = successKeywords.some((word) => lowerExpected.includes(word));

  const structuredSuccess = typeof response.structuredContent?.success === 'boolean'
    ? response.structuredContent.success
    : undefined;
  // Strict success evaluation to avoid false positives
  let actualSuccess;
  if (structuredSuccess === true) actualSuccess = true;
  else if (structuredSuccess === false) actualSuccess = false;
  else if (response.isError === true) actualSuccess = false;
  else actualSuccess = undefined;

  // Extract actual error/message from response
  let actualError = null;
  let actualMessage = null;
  if (response.structuredContent) {
    actualError = response.structuredContent.error;
    actualMessage = response.structuredContent.message;
  }

  // Handle NOT_IMPLEMENTED expectations
  if (lowerExpected === 'not_implemented') {
    const isNotImplemented = actualError === 'NOT_IMPLEMENTED' ||
      (actualMessage && actualMessage.toLowerCase().includes('not implemented'));
    if (isNotImplemented) {
      return { passed: true, reason: 'Feature not implemented as expected' };
    } else {
      return { passed: false, reason: `Expected NOT_IMPLEMENTED but got: ${actualMessage || actualError}` };
    }
  }

  // Handle "success or X" patterns (e.g., "success or skeleton not found" / "success or handled")
  if (lowerExpected.includes(' or ')) {
    const conditions = lowerExpected.split(' or ').map(c => c.trim());
    for (const condition of conditions) {
      if (successKeywords.some(kw => condition.includes(kw)) && actualSuccess === true) {
        return { passed: true, reason: JSON.stringify(response.structuredContent) };
      }
      const messageStr = (actualMessage || '').toString().toLowerCase();
      const errorStr = (actualError || '').toString().toLowerCase();
      if (condition === 'handled' && response.structuredContent && response.structuredContent.handled === true) {
        return { passed: true, reason: 'Handled gracefully' };
      }
      if (messageStr.includes(condition) || errorStr.includes(condition)) {
        return { passed: true, reason: `Expected condition met: ${condition}` };
      }
    }
    // If none of the OR conditions matched, it's a failure
    return { passed: false, reason: `None of the expected conditions matched: ${testCase.expected}` };
  }

  // CRITICAL: Check for Python syntax errors in message/error
  const pythonSyntaxErrors = [
    'SyntaxError',
    'invalid syntax',
    'unterminated string',
    'forgot a comma',
    'unexpected indent',
    'IndentationError',
    'NameError',
    'AttributeError: module',
    'Python execution failed'
  ];

  const messageStr = (actualMessage || '').toString();
  const errorStr = (actualError || '').toString();
  const combinedText = (messageStr + ' ' + errorStr).toLowerCase();

  const hasPythonError = pythonSyntaxErrors.some(errType =>
    combinedText.includes(errType.toLowerCase())
  );

  // Also flag common automation/plugin failure phrases
  const pluginFailureIndicators = ['does not match prefix', 'unknown', 'not implemented', 'unavailable', 'unsupported'];
  const hasPluginFailure = pluginFailureIndicators.some(term => combinedText.includes(term));

  // If expecting success but got Python error, test FAILS
  if (!containsFailure && hasPythonError) {
    return {
      passed: false,
      reason: `Expected success but got Python error: ${actualMessage || actualError}`
    };
  }

  if (!containsFailure && hasPluginFailure) {
    return {
      passed: false,
      reason: `Expected success but plugin reported failure: ${actualMessage || actualError}`
    };
  }

  // CRITICAL: Check if message says "failed" but success is true (FALSE POSITIVE)
  if (actualSuccess && (
    messageStr.toLowerCase().includes('failed') ||
    messageStr.toLowerCase().includes('python execution failed') ||
    errorStr.toLowerCase().includes('failed')
  )) {
    return {
      passed: false,
      reason: `False positive: success=true but message indicates failure: ${actualMessage}`
    };
  }


  // CRITICAL FIX: UE_NOT_CONNECTED errors should ALWAYS fail tests unless explicitly expected
  if (actualError === 'UE_NOT_CONNECTED') {
    const explicitlyExpectsDisconnection = lowerExpected.includes('not connected') ||
      lowerExpected.includes('ue_not_connected') ||
      lowerExpected.includes('disconnected');
    if (!explicitlyExpectsDisconnection) {
      return {
        passed: false,
        reason: `Test requires Unreal Engine connection, but got: ${actualError} - ${actualMessage}`
      };
    }
  }

  // For tests that expect specific error types, validate the actual error matches
  const expectedFailure = containsFailure && !containsSuccess;
  if (expectedFailure && !actualSuccess) {
    // Test expects failure and got failure - but verify it's the RIGHT kind of failure
    const lowerReason = actualMessage?.toLowerCase() || actualError?.toLowerCase() || '';

    // Check for specific error types (not just generic "error" keyword)
    const specificErrorTypes = ['not found', 'invalid', 'missing', 'already exists', 'does not exist'];
    const expectedErrorType = specificErrorTypes.find(type => lowerExpected.includes(type));
    const errorTypeMatch = expectedErrorType ? lowerReason.includes(expectedErrorType) :
      failureKeywords.some(keyword => lowerExpected.includes(keyword) && lowerReason.includes(keyword));

    // If expected outcome specifies an error type, actual error should match it
    if (lowerExpected.includes('not found') || lowerExpected.includes('invalid') ||
      lowerExpected.includes('missing') || lowerExpected.includes('already exists')) {
      const passed = errorTypeMatch;
      let reason;
      if (response.isError) {
        reason = response.content?.map((entry) => ('text' in entry ? entry.text : JSON.stringify(entry))).join('\n');
      } else if (response.structuredContent) {
        reason = JSON.stringify(response.structuredContent);
      } else {
        reason = 'No structured response returned';
      }
      return { passed, reason };
    }
  }

  // Default evaluation logic
  const passed = expectedFailure ? !actualSuccess : !!actualSuccess;
  let reason;
  if (response.isError) {
    reason = response.content?.map((entry) => ('text' in entry ? entry.text : JSON.stringify(entry))).join('\n');
  } else if (response.structuredContent) {
    reason = JSON.stringify(response.structuredContent);
  } else if (response.content?.length) {
    reason = response.content.map((entry) => ('text' in entry ? entry.text : JSON.stringify(entry))).join('\n');
  } else {
    reason = 'No structured response returned';
  }
  return { passed, reason };
}

/**
 * Main test runner function
 */
export async function runToolTests(toolName, testCases) {
  console.log(`Total test cases: ${testCases.length}`);
  console.log('='.repeat(60));
  console.log('');

  let transport;
  let client;
  const results = [];
  // callToolOnce is assigned after the MCP client is initialized. Declare here so
  // the test loop can call it regardless of block scoping rules.
  let callToolOnce;

  try {
    // Wait for the automation bridge ports to be available so the spawned MCP server
    // process can successfully connect to the editor plugin.
    const bridgeHost = process.env.MCP_AUTOMATION_WS_HOST ?? '127.0.0.1';
    const envPorts = process.env.MCP_AUTOMATION_WS_PORTS
      ? process.env.MCP_AUTOMATION_WS_PORTS.split(',').map((p) => parseInt(p.trim(), 10)).filter(Boolean)
      : [8090, 8091];
    const waitMs = parseInt(process.env.UNREAL_MCP_WAIT_PORT_MS ?? '5000', 10);

    async function waitForAnyPort(host, ports, timeoutMs = 10000) {
      const start = Date.now();
      while (Date.now() - start < timeoutMs) {
        for (const port of ports) {
          try {
            await new Promise((resolve, reject) => {
              const sock = new net.Socket();
              let settled = false;
              sock.setTimeout(1000);
              sock.once('connect', () => { settled = true; sock.destroy(); resolve(true); });
              sock.once('timeout', () => { if (!settled) { settled = true; sock.destroy(); reject(new Error('timeout')); } });
              sock.once('error', () => { if (!settled) { settled = true; sock.destroy(); reject(new Error('error')); } });
              sock.connect(port, host);
            });
            console.log(`✅ Automation bridge appears to be listening on ${host}:${port}`);
            return port;
          } catch {
            // ignore and try next port
          }
        }
        // Yield to the event loop once instead of sleeping.
        await new Promise((r) => setImmediate(r));
      }
      throw new Error(`Timed out waiting for automation bridge on ports: ${ports.join(',')}`);
    }

    try {
      await waitForAnyPort(bridgeHost, envPorts, waitMs);
    } catch (err) {
      console.warn('Automation bridge did not become available before tests started:', err.message);
    }

    // Decide whether to run the built server (dist/cli.js) or to run the
    // TypeScript source directly. Prefer the built dist when it is up-to-date
    // with the src tree. Fall back to running src with ts-node when dist is
    // missing or older than the src modification time to avoid running stale code.
    const distPath = path.join(repoRoot, 'dist', 'cli.js');
    const srcDir = path.join(repoRoot, 'src');

    async function getLatestMtime(dir) {
      let latest = 0;
      try {
        const entries = await fs.readdir(dir, { withFileTypes: true });
        for (const e of entries) {
          const full = path.join(dir, e.name);
          if (e.isDirectory()) {
            const child = await getLatestMtime(full);
            if (child > latest) latest = child;
          } else {
            try {
              const st = await fs.stat(full);
              const m = st.mtimeMs || 0;
              if (m > latest) latest = m;
            } catch (_) { }
          }
        }
      } catch (_) {
        // ignore
      }
      return latest;
    }

    // Choose how to launch the server. Prefer using the built `dist/` executable so
    // Node resolves ESM imports cleanly. If `dist/` is missing, attempt an automatic
    // `npm run build` so users that run live tests don't hit ts-node resolution errors.
    let useDist = false;
    let distExists = false;
    try {
      await fs.access(distPath);
      distExists = true;
    } catch (e) {
      distExists = false;
    }

    if (process.env.UNREAL_MCP_FORCE_DIST === '1') {
      useDist = true;
      console.log('Forcing use of dist build via UNREAL_MCP_FORCE_DIST=1');
    } else if (distExists) {
      try {
        const distStat = await fs.stat(distPath);
        const srcLatest = await getLatestMtime(srcDir);
        const srcIsNewer = srcLatest > (distStat.mtimeMs || 0);
        const autoBuildEnabled = process.env.UNREAL_MCP_AUTO_BUILD === '1';
        const autoBuildDisabled = process.env.UNREAL_MCP_NO_AUTO_BUILD === '1';
        if (srcIsNewer) {
          if (!autoBuildEnabled && !autoBuildDisabled) {
            console.log('Detected newer source files than dist; attempting automatic build to refresh dist/ (set UNREAL_MCP_NO_AUTO_BUILD=1 to disable)');
          }
          if (autoBuildEnabled || !autoBuildDisabled) {
            const { spawn } = await import('node:child_process');
            try {
              await new Promise((resolve, reject) => {
                const npmCmd = process.platform === 'win32' ? 'npm.cmd' : 'npm';
                const ps = spawn(npmCmd, ['run', 'build'], { cwd: repoRoot, stdio: 'inherit', shell: process.platform === 'win32' });
                ps.on('close', (code) => (code === 0 ? resolve() : reject(new Error(`Build failed with code ${code}`))));
                ps.on('error', (err) => reject(err));
              });
              console.log('Build succeeded — using dist/ for live tests');
              useDist = true;
            } catch (buildErr) {
              console.warn('Automatic build failed or could not stat files — falling back to TypeScript source for live tests:', String(buildErr));
              useDist = false;
            }
          } else {
            console.log('Detected newer source files than dist but automatic build is disabled.');
            console.log('Set UNREAL_MCP_AUTO_BUILD=1 to enable automatic builds, or run `npm run build` manually.');
            useDist = false;
          }
        } else {
          useDist = true;
          console.log('Using built dist for live tests');
        }
      } catch (buildErr) {
        console.warn('Automatic build failed or could not stat files — falling back to TypeScript source for live tests:', String(buildErr));
        useDist = false;
        console.log('Preferring TypeScript source for tests to pick up local changes (set UNREAL_MCP_FORCE_DIST=1 to force dist)');
      }
    } else {
      console.log('dist not found — attempting to run `npm run build` to produce dist/ for live tests');
      try {
        const { spawn } = await import('node:child_process');
        await new Promise((resolve, reject) => {
          const ps = spawn(process.platform === 'win32' ? 'npm.cmd' : 'npm', ['run', 'build'], { cwd: repoRoot, stdio: 'inherit' });
          ps.on('close', (code) => (code === 0 ? resolve() : reject(new Error(`Build failed with code ${code}`))));
          ps.on('error', (err) => reject(err));
        });
        useDist = true;
        console.log('Build succeeded — using dist/ for live tests');
      } catch (buildErr) {
        console.warn('Automatic build failed — falling back to running TypeScript source with ts-node-esm:', String(buildErr));
        useDist = false;
      }
    }

    if (!useDist) {
      serverCommand = process.env.UNREAL_MCP_SERVER_CMD ?? 'npx';
      serverArgs = ['ts-node-esm', path.join(repoRoot, 'src', 'cli.ts')];
    } else {
      serverCommand = process.env.UNREAL_MCP_SERVER_CMD ?? serverCommand;
      serverArgs = process.env.UNREAL_MCP_SERVER_ARGS?.split(',') ?? serverArgs;
    }

    transport = new StdioClientTransport({
      command: serverCommand,
      args: serverArgs,
      cwd: serverCwd,
      stderr: 'inherit',
      env: serverEnv
    });

    client = new Client({
      name: 'unreal-mcp-test-runner',
      version: '1.0.0'
    });

    await client.connect(transport);
    await client.listTools({});
    console.log('✅ Connected to Unreal MCP Server\n');

    // Single-attempt call helper (no retries). This forwards a timeoutMs
    // argument to the server so server-side automation calls use the same
    // timeout the test harness expects.
    callToolOnce = async function (callOptions, baseTimeoutMs) {
      const envDefault = Number(process.env.UNREAL_MCP_TEST_CALL_TIMEOUT_MS ?? '60000') || 60000;
      const perCall = Number(callOptions?.arguments?.timeoutMs) || undefined;
      const base = typeof baseTimeoutMs === 'number' && baseTimeoutMs > 0 ? baseTimeoutMs : (perCall || envDefault);
      const timeoutMs = base;
      try {
        console.log(`[CALL] ${callOptions.name} (timeout ${timeoutMs}ms)`);
        const outgoing = Object.assign({}, callOptions, { arguments: { ...(callOptions.arguments || {}), timeoutMs } });
        // Prefer instructing the MCP client to use a matching timeout if
        // the client library supports per-call options; fall back to the
        // plain call if not supported.
        let callPromise;
        try {
          // Correct parameter order: (params, resultSchema?, options)
          callPromise = client.callTool(outgoing, undefined, { timeout: timeoutMs });
        } catch (err) {
          // Fall back to calling the older signature where options might be second param
          try {
            callPromise = client.callTool(outgoing, { timeout: timeoutMs });
          } catch (inner) {
            try {
              callPromise = client.callTool(outgoing);
            } catch (inner2) {
              throw inner2 || inner || err;
            }
          }
        }

        const timed = Promise.race([
          callPromise,
          new Promise((_, rej) => setTimeout(() => rej(new Error(`Local test runner timeout after ${timeoutMs}ms`)), timeoutMs))
        ]);
        return await timed;
      } catch (e) {
        const msg = String(e?.message || e || '');
        if (msg.includes('Unknown blueprint action')) {
          return { structuredContent: { success: false, error: msg } };
        }
        throw e;
      }
    };

    // Run each test case
    for (let i = 0; i < testCases.length; i++) {
      const testCase = testCases[i];
      const testCaseTimeoutMs = Number(process.env.UNREAL_MCP_TEST_CASE_TIMEOUT_MS ?? testCase.arguments?.timeoutMs ?? '180000');
      const startTime = performance.now();

      try {
        const response = await callToolOnce({ name: testCase.toolName, arguments: testCase.arguments }, testCaseTimeoutMs);

        const endTime = performance.now();
        const durationMs = endTime - startTime;

        let structuredContent = response.structuredContent ?? null;
        if (structuredContent === null && response.content?.length) {
          for (const entry of response.content) {
            if (entry?.type !== 'text' || typeof entry.text !== 'string') continue;
            try { structuredContent = JSON.parse(entry.text); break; } catch { }
          }
        }
        const normalizedResponse = { ...response, structuredContent };

        const { passed, reason } = evaluateExpectation(testCase, normalizedResponse);
        let finalPassed = passed;
        let finalReason = reason;

        if (finalPassed && testCase.verify && normalizedResponse.structuredContent) {
          const blueprint = normalizedResponse.structuredContent.blueprint ?? normalizedResponse.structuredContent;
          if (testCase.verify.blueprintHasVariable && Array.isArray(testCase.verify.blueprintHasVariable)) {
            const missing = [];
            const vars = (blueprint && blueprint.variables) || [];
            for (const expectedVar of testCase.verify.blueprintHasVariable) {
              const found = vars.find(v => (v.name || v.VarName || v.varName || '').toString() === expectedVar);
              if (!found) missing.push(expectedVar);
            }
            if (missing.length > 0) { finalPassed = false; finalReason = `Verification failed: missing variable(s): ${missing.join(', ')}`; }
          }
          if (finalPassed && testCase.verify.blueprintHasFunction && Array.isArray(testCase.verify.blueprintHasFunction)) {
            const missing = [];
            const funcs = (blueprint && blueprint.functions) || [];
            for (const expectedF of testCase.verify.blueprintHasFunction) {
              const found = funcs.find(f => (f.name || f.Name || '').toString() === expectedF);
              if (!found) missing.push(expectedF);
            }
            if (missing.length > 0) { finalPassed = false; finalReason = `Verification failed: missing function(s): ${missing.join(', ')}`; }
          }
          if (finalPassed && testCase.verify.blueprintHasEvent && Array.isArray(testCase.verify.blueprintHasEvent)) {
            const missing = [];
            const evts = (blueprint && blueprint.events) || [];
            for (const expectedE of testCase.verify.blueprintHasEvent) {
              const found = evts.find(e => (e.name || e.EventName || '').toString() === expectedE || (e.eventType || '').toString() === expectedE);
              if (!found) missing.push(expectedE);
            }
            if (missing.length > 0) { finalPassed = false; finalReason = `Verification failed: missing event(s): ${missing.join(', ')}`; }
          }
        }
        const status = finalPassed ? 'passed' : 'failed';

        results.push({ ...testCase, status, durationMs, detail: finalPassed ? null : finalReason });
        console.log(formatResultLine(testCase, status, finalPassed ? null : finalReason, durationMs));

      } catch (error) {
        const endTime = performance.now();
        const durationMs = endTime - startTime;
        results.push({ ...testCase, status: 'failed', durationMs, detail: `Exception: ${error.message}` });
        console.log(formatResultLine(testCase, 'failed', error.message, durationMs));
      }
    }

  } catch (error) {
    console.error('\n❌ Failed to initialize MCP client:', error.message);
    process.exitCode = 1;
    return;
  } finally {
    if (transport) {
      try { await transport.close(); } catch { }
    }
  }

  const resultsPath = await persistResults(toolName.toLowerCase().replace(/ /g, '-'), results);
  summarize(toolName, results, resultsPath);

  const failCount = results.filter(r => r.status === 'failed').length;
  if (failCount > 0) process.exitCode = 1; else process.exitCode = 0;
  process.exit(process.exitCode || 0);
}
