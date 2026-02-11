import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { StdioClientTransport } from '@modelcontextprotocol/sdk/client/stdio.js';
import path from 'node:path';
import fs from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import net from 'node:net';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const repoRoot = path.resolve(__dirname, '..');
const reportsDir = path.join(__dirname, 'reports');

// Common failure keywords to check against
const failureKeywords = ['failed', 'error', 'exception', 'invalid', 'not found', 'missing', 'timed out', 'timeout', 'unsupported', 'unknown'];
const successKeywords = ['success', 'created', 'updated', 'deleted', 'completed', 'done', 'ok'];

// Defaults for spawning the MCP server.
let serverCommand = process.env.UNREAL_MCP_SERVER_CMD ?? 'node';
let serverArgs = process.env.UNREAL_MCP_SERVER_ARGS ? process.env.UNREAL_MCP_SERVER_ARGS.split(',') : [path.join(repoRoot, 'dist', 'cli.js')];
const serverCwd = process.env.UNREAL_MCP_SERVER_CWD ?? repoRoot;
const serverEnv = Object.assign({}, process.env);

const DEFAULT_RESPONSE_LOG_MAX_CHARS = 6000; // default max chars
const RESPONSE_LOGGING_ENABLED = process.env.UNREAL_MCP_TEST_LOG_RESPONSES !== '0';

function clampString(value, maxChars) {
  if (typeof value !== 'string') return '';
  if (value.length <= maxChars) return value;
  return value.slice(0, maxChars) + `\n... (truncated, ${value.length - maxChars} chars omitted)`;
}

function tryParseJson(text) {
  if (typeof text !== 'string') return null;
  try {
    return JSON.parse(text);
  } catch {
    return null;
  }
}

export function normalizeMcpResponse(response) {
  const normalized = {
    isError: Boolean(response?.isError),
    structuredContent: response?.structuredContent ?? null,
    contentText: '',
    content: response?.content ?? undefined
  };

  if (normalized.structuredContent === null && Array.isArray(response?.content)) {
    for (const entry of response.content) {
      if (entry?.type !== 'text' || typeof entry.text !== 'string') continue;
      const parsed = tryParseJson(entry.text);
      if (parsed !== null) {
        normalized.structuredContent = parsed;
        break;
      }
    }
  }

  if (Array.isArray(response?.content) && response.content.length > 0) {
    normalized.contentText = response.content
      .map((entry) => (entry && typeof entry.text === 'string' ? entry.text : ''))
      .filter((text) => text.length > 0)
      .join('\n');
  }

  return normalized;
}

function logMcpResponse(toolName, normalizedResponse) {
  const maxChars = Number(process.env.UNREAL_MCP_TEST_RESPONSE_MAX_CHARS ?? DEFAULT_RESPONSE_LOG_MAX_CHARS);
  const payload = {
    isError: normalizedResponse.isError,
    structuredContent: normalizedResponse.structuredContent,
    contentText: normalizedResponse.contentText,
    content: normalizedResponse.content
  };
  const json = JSON.stringify(payload, null, 2);
  console.log(`[MCP RESPONSE] ${toolName}:`);
  console.log(clampString(json, Number.isFinite(maxChars) && maxChars > 0 ? maxChars : DEFAULT_RESPONSE_LOG_MAX_CHARS));
}

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

/**
 * Evaluates whether a test case passed based on expected outcome
 */
function evaluateExpectation(testCase, response) {
  const expectation = testCase.expected;

  // Normalize expected into a comparable form. If expected is an object
  // (e.g. {condition: 'success|error', errorPattern: 'SC_DISABLED'}), then
  // we extract the condition string as the primary expectation string.
  const expectedCondition = (typeof expectation === 'object' && expectation !== null && expectation.condition)
    ? expectation.condition
    : (typeof expectation === 'string' ? expectation : String(expectation));

  const lowerExpected = expectedCondition.toLowerCase();

  // Determine failure/success intent from condition keywords
  const containsFailure = failureKeywords.some((word) => lowerExpected.includes(word));
  const containsSuccess = successKeywords.some((word) => lowerExpected.includes(word));

  // CRITICAL FIX: Determine PRIMARY intent (first condition in pipe-separated list)
  // Tests like "success|error" should have PRIMARY intent of success, meaning
  // if we get success=false, it should FAIL even though "error" is in the alternatives.
  const primaryCondition = lowerExpected.split('|')[0].split(' or ')[0].trim();
  const primaryExpectsSuccess = successKeywords.some((word) => primaryCondition.includes(word));
  const primaryExpectsFailure = failureKeywords.some((word) => primaryCondition.includes(word));

  const structuredSuccess = typeof response.structuredContent?.success === 'boolean'
    ? response.structuredContent.success
    : undefined;
  const actualSuccess = structuredSuccess ?? !response.isError;

  // CRITICAL: If response explicitly indicates an error (isError: true or structuredContent.success: false)
  // and the PRIMARY expectation is success (not just a fallback alternative), FAIL immediately.
  // This prevents false positives where tests like "success|handled|error" pass even when
  // the engine returns success: false.
  if ((response.isError === true || structuredSuccess === false) && !primaryExpectsFailure) {
    const errorReason = response.structuredContent?.error || response.structuredContent?.message || 'Unknown error';
    return {
      passed: false,
      reason: `Response indicates error but test expected success (primary intent: ${primaryCondition}): ${errorReason}`
    };
  }

  // Extract actual error/message from response
  let actualError = null;
  let actualMessage = null;
  if (response.structuredContent) {
    actualError = response.structuredContent.error;
    actualMessage = response.structuredContent.message;
  }

  // Also extract flattened plain-text content for matching when structured
  // fields are missing or when MCP errors (e.g. timeouts) are only reported
  // via the textual content array.
  let contentText = '';
  if (Array.isArray(response.content) && response.content.length > 0) {
    contentText = response.content
      .map((entry) => (entry && typeof entry.text === 'string' ? entry.text : ''))
      .filter((t) => t.length > 0)
      .join('\n');
  }

  // Helper to get effective actual strings for matching
  const messageStr = (actualMessage || '').toString().toLowerCase();
  const errorStr = (actualError || '').toString().toLowerCase();
  const contentStr = contentText.toString().toLowerCase();
  const combined = `${messageStr} ${errorStr} ${contentStr}`;

  // If expectation is an object with specific pattern constraints, apply them
  if (typeof expectation === 'object' && expectation !== null) {
    // If actual outcome was success, check successPattern
    if (actualSuccess && expectation.successPattern) {
      const pattern = expectation.successPattern.toLowerCase();
      if (combined.includes(pattern)) {
        return { passed: true, reason: `Success pattern matched: ${expectation.successPattern}` };
      }
    }
    // If actual outcome was error/failure, check errorPattern
    if (!actualSuccess && expectation.errorPattern) {
      const pattern = expectation.errorPattern.toLowerCase();
      if (combined.includes(pattern)) {
        return { passed: true, reason: `Error pattern matched: ${expectation.errorPattern}` };
      }
    }
  }

  // Handle multi-condition expectations using "or" or pipe separators
  // e.g., "success or LOAD_FAILED" or "success|no_instances|load_failed"
  if (lowerExpected.includes(' or ') || lowerExpected.includes('|')) {
    const separator = lowerExpected.includes(' or ') ? ' or ' : '|';
    const conditions = lowerExpected.split(separator).map((c) => c.trim()).filter(Boolean);
    for (const condition of conditions) {
      if (successKeywords.some((kw) => condition.includes(kw)) && actualSuccess === true) {
        return { passed: true, reason: JSON.stringify(response.structuredContent) };
      }
      if (condition === 'handled' && response.structuredContent && response.structuredContent.handled === true) {
        return { passed: true, reason: 'Handled gracefully' };
      }

      // Special-case timeout expectations so that MCP transport timeouts
      // (e.g. "Request timed out") satisfy conditions containing "timeout".
      if (condition === 'timeout' || condition.includes('timeout')) {
        if (combined.includes('timeout') || combined.includes('timed out')) {
          return { passed: true, reason: `Expected timeout condition met: ${condition}` };
        }
      }

      if (combined.includes(condition)) {
        return { passed: true, reason: `Expected condition met: ${condition}` };
      }
    }
    // If none of the OR/pipe conditions matched, it's a failure
    return { passed: false, reason: `None of the expected conditions matched: ${expectedCondition}` };
  }

  // Also flag common automation/plugin failure phrases
  const pluginFailureIndicators = ['does not match prefix', 'unknown', 'not implemented', 'unavailable', 'unsupported'];
  const hasPluginFailure = pluginFailureIndicators.some(term => combined.includes(term));

  if (!containsFailure && hasPluginFailure) {
    return {
      passed: false,
      reason: `Expected success but plugin reported failure: ${actualMessage || actualError}`
    };
  }

  // CRITICAL: Check if message says "failed" but success is true (FALSE POSITIVE)
  if (actualSuccess && (
    messageStr.includes('failed') ||
    messageStr.includes('python execution failed') ||
    errorStr.includes('failed')
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
    const lowerReason = actualMessage?.toLowerCase() || actualError?.toLowerCase() || contentStr || '';

    // Check for specific error types (not just generic "error" keyword)
    const specificErrorTypes = ['not found', 'invalid', 'missing', 'already exists', 'does not exist', 'sc_disabled'];
    const expectedErrorType = specificErrorTypes.find(type => lowerExpected.includes(type));
    let errorTypeMatch = expectedErrorType ? lowerReason.includes(expectedErrorType) :
      failureKeywords.some(keyword => lowerExpected.includes(keyword) && lowerReason.includes(keyword));

    // Also check detail field if main error check failed (handles wrapped exceptions)
    if (!errorTypeMatch && response.detail && typeof response.detail === 'string') {
      const lowerDetail = response.detail.toLowerCase();
      if (expectedErrorType) {
        if (lowerDetail.includes(expectedErrorType)) errorTypeMatch = true;
      } else {
        // If no specific error type, just check if detail contains expected string
        if (lowerDetail.includes(lowerExpected)) errorTypeMatch = true;
      }
    }

    // If expected outcome specifies an error type, actual error should match it
    if (lowerExpected.includes('not found') || lowerExpected.includes('invalid') ||
      lowerExpected.includes('missing') || lowerExpected.includes('already exists') || lowerExpected.includes('sc_disabled')) {
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
    const waitMs = 10000; // Hardcoded increased timeout

    console.log(`Waiting up to ${waitMs}ms for automation bridge on ${bridgeHost}:${envPorts.join(',')}`);

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
                const ps = process.platform === 'win32'
                  ? spawn(`${npmCmd} run build`, { cwd: repoRoot, stdio: 'inherit', shell: true })
                  : spawn(npmCmd, ['run', 'build'], { cwd: repoRoot, stdio: 'inherit' });
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

        let timeoutId;
        const timeoutPromise = new Promise((_, rej) => {
          timeoutId = setTimeout(() => rej(new Error(`Local test runner timeout after ${timeoutMs}ms`)), timeoutMs);
          if (timeoutId && typeof timeoutId.unref === 'function') {
            timeoutId.unref();
          }
        });
        try {
          const timed = Promise.race([
            callPromise,
            timeoutPromise
          ]);
          return await timed;
        } finally {
          if (timeoutId) {
            clearTimeout(timeoutId);
          }
        }
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
        // Log test start to Unreal Engine console
        const cleanScenario = (testCase.scenario || 'Unknown Test').replace(/"/g, "'");
        await callToolOnce({
          name: 'system_control',
          arguments: { action: 'console_command', command: `Log "---- STARTING TEST: ${cleanScenario} ----"` }
        }, 5000).catch(() => { });
      } catch (e) { /* ignore */ }

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
        if (RESPONSE_LOGGING_ENABLED) {
          logMcpResponse(testCase.toolName + " :: " + testCase.scenario, normalizeMcpResponse(normalizedResponse));
        }
        const { passed, reason } = evaluateExpectation(testCase, normalizedResponse);

        if (!passed) {
          console.log(`[FAILED] ${testCase.scenario} (${durationMs.toFixed(1)} ms) => ${reason}`);
          results.push({
            scenario: testCase.scenario,
            toolName: testCase.toolName,
            arguments: testCase.arguments,
            status: 'failed',
            durationMs,
            detail: reason,
            response: normalizedResponse
          });
        } else {
          console.log(`[PASSED] ${testCase.scenario} (${durationMs.toFixed(1)} ms)`);
          results.push({
            scenario: testCase.scenario,
            toolName: testCase.toolName,
            arguments: testCase.arguments,
            status: 'passed',
            durationMs,
            detail: reason
          });
        }

      } catch (error) {
        const endTime = performance.now();
        const durationMs = endTime - startTime;
        const errorMessage = String(error?.message || error || '');
        const lowerExpected = (testCase.expected || '').toString().toLowerCase();
        const lowerError = errorMessage.toLowerCase();

        // If the test explicitly expects a timeout (e.g. "timeout|error"), then
        // an MCP/client timeout should be treated as the expected outcome rather
        // than as a hard harness failure. Accept both "timeout" and "timed out"
        // phrasing from different MCP client implementations.
        if (lowerExpected.includes('timeout') && (lowerError.includes('timeout') || lowerError.includes('timed out'))) {
          console.log(`[PASSED] ${testCase.scenario} (${durationMs.toFixed(1)} ms)`);
          results.push({
            scenario: testCase.scenario,
            toolName: testCase.toolName,
            arguments: testCase.arguments,
            status: 'passed',
            durationMs,
            detail: errorMessage
          });
          continue;
        }

        console.log(`[FAILED] ${testCase.scenario} (${durationMs.toFixed(1)} ms) => Error: ${errorMessage}`);
        results.push({
          scenario: testCase.scenario,
          toolName: testCase.toolName,
          arguments: testCase.arguments,
          status: 'failed',
          durationMs,
          detail: errorMessage
        });
      }
    }

    const resultsPath = await persistResults(toolName, results);
    summarize(toolName, results, resultsPath);

    const hasFailures = results.some((result) => result.status === 'failed');
    process.exitCode = hasFailures ? 1 : 0;

  } catch (error) {
    console.error('Test runner failed:', error);
    process.exit(1);
  } finally {
    if (client) {
      try {
        await client.close();
      } catch {
        // ignore
      }
    }
    if (transport) {
      try {
        await transport.close();
      } catch {
        // ignore
      }
    }
  }
}

export class TestRunner {
  constructor(suiteName) {
    this.suiteName = suiteName || 'Test Suite';
    this.steps = [];
  }

  addStep(name, fn) {
    this.steps.push({ name, fn });
  }

  async run() {
    if (this.steps.length === 0) {
      console.warn(`No steps registered for ${this.suiteName}`);
      return;
    }

    console.log('\n' + '='.repeat(60));
    console.log(`${this.suiteName}`);
    console.log('='.repeat(60));
    console.log(`Total steps: ${this.steps.length}`);
    console.log('');

    let transport;
    let client;
    const results = [];

    try {
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
            }
          }
          await new Promise((r) => setImmediate(r));
        }
        throw new Error(`Timed out waiting for automation bridge on ports: ${ports.join(',')}`);
      }

      try {
        await waitForAnyPort(bridgeHost, envPorts, waitMs);
      } catch (err) {
        console.warn('Automation bridge did not become available before tests started:', err.message);
      }

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
        }
        return latest;
      }

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
        name: 'unreal-mcp-step-runner',
        version: '1.0.0'
      });

      await client.connect(transport);
      await client.listTools({});
      console.log('✅ Connected to Unreal MCP Server\n');

      const callToolOnce = async function (callOptions, baseTimeoutMs) {
        const envDefault = Number(process.env.UNREAL_MCP_TEST_CALL_TIMEOUT_MS ?? '60000') || 60000;
        const perCall = Number(callOptions?.arguments?.timeoutMs) || undefined;
        const base = typeof baseTimeoutMs === 'number' && baseTimeoutMs > 0 ? baseTimeoutMs : (perCall || envDefault);
        const timeoutMs = base;
        try {
          console.log(`[CALL] ${callOptions.name} (timeout ${timeoutMs}ms)`);
          const outgoing = Object.assign({}, callOptions, { arguments: { ...(callOptions.arguments || {}), timeoutMs } });
          let callPromise;
          try {
            callPromise = client.callTool(outgoing, undefined, { timeout: timeoutMs });
          } catch (err) {
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

          let timeoutId;
          const timeoutPromise = new Promise((_, rej) => {
            timeoutId = setTimeout(() => rej(new Error(`Local test runner timeout after ${timeoutMs}ms`)), timeoutMs);
            if (timeoutId && typeof timeoutId.unref === 'function') {
              timeoutId.unref();
            }
          });
          try {
            const timed = Promise.race([
              callPromise,
              timeoutPromise
            ]);
            return await timed;
          } finally {
            if (timeoutId) {
              clearTimeout(timeoutId);
            }
          }
        } catch (e) {
          const msg = String(e?.message || e || '');
          if (msg.includes('Unknown blueprint action')) {
            return { structuredContent: { success: false, error: msg } };
          }
          throw e;
        }
      };

      const tools = {
        async executeTool(toolName, args, options = {}) {
          const timeoutMs = typeof options.timeoutMs === 'number' ? options.timeoutMs : undefined;
          const response = await callToolOnce({ name: toolName, arguments: args }, timeoutMs);
          let structuredContent = response.structuredContent ?? null;
          if (structuredContent === null && response.content?.length) {
            for (const entry of response.content) {
              if (entry?.type !== 'text' || typeof entry.text !== 'string') continue;
              try {
                structuredContent = JSON.parse(entry.text);
                break;
              } catch {
              }
            }
          }

          if (structuredContent && typeof structuredContent === 'object') {
            return structuredContent;
          }

          return {
            success: !response.isError,
            message: undefined,
            error: undefined
          };
        }
      };

      for (const step of this.steps) {
        const startTime = performance.now();

        try {
          // Log step start to Unreal Engine console
          const cleanName = (step.name || 'Unknown Step').replace(/"/g, "'");
          await callToolOnce({
            name: 'system_control',
            arguments: { action: 'console_command', command: `Log "---- STARTING STEP: ${cleanName} ----"` }
          }, 5000).catch(() => { });
        } catch (e) { /* ignore */ }

        try {
          const ok = await step.fn(tools);
          const durationMs = performance.now() - startTime;
          const status = ok ? 'passed' : 'failed';
          console.log(formatResultLine({ scenario: step.name }, status, ok ? '' : 'Step returned false', durationMs));
          results.push({
            scenario: step.name,
            toolName: null,
            arguments: null,
            status,
            durationMs,
            detail: ok ? undefined : 'Step returned false'
          });
        } catch (err) {
          const durationMs = performance.now() - startTime;
          const detail = err?.message || String(err);
          console.log(formatResultLine({ scenario: step.name }, 'failed', detail, durationMs));
          results.push({
            scenario: step.name,
            toolName: null,
            arguments: null,
            status: 'failed',
            durationMs,
            detail
          });
        }
      }

      const resultsPath = await persistResults(this.suiteName, results);
      summarize(this.suiteName, results, resultsPath);

      const hasFailures = results.some((result) => result.status === 'failed');
      process.exitCode = hasFailures ? 1 : 0;
    } catch (error) {
      console.error('Step-based test runner failed:', error);
      process.exit(1);
    } finally {
      if (client) {
        try {
          await client.close();
        } catch {
        }
      }
      if (transport) {
        try {
          await transport.close();
        } catch {
        }
      }
    }
  }
}

