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
// CRITICAL: Include both singular and plural forms for flexible matching
const failureKeywords = ['failed', 'error', 'exception', 'invalid', 'not found', 'not_found', 'missing', 'timed out', 'timeout', 'unsupported', 'unknown', 'traversal', 'blocked', 'denied', 'forbidden', 'security', 'violation', 'object_not_found', 'actor_not_found', 'actors not found', 'not exist'];
const successKeywords = ['success', 'created', 'updated', 'deleted', 'completed', 'done', 'ok', 'skipped', 'handled', 'not_implemented'];

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
  const actualSuccess = structuredSuccess ?? !(response.isError || response.structuredContent?.isError);

  // CRITICAL: If response explicitly indicates an error (isError: true or structuredContent.success: false
  // or structuredContent.isError: true) and the PRIMARY expectation is success (not just a fallback alternative),
  // FAIL immediately. This prevents false positives where tests like "success|handled|error" pass even when
  // the engine returns success: false.
  if ((response.isError === true || response.structuredContent?.isError === true || structuredSuccess === false) && !primaryExpectsFailure) {
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
  // Also check top-level message field (MCP errors may not have structuredContent)
  if (!actualMessage && response.message) {
    actualMessage = response.message;
  }
  if (!actualError && response.error) {
    actualError = response.error;
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

  // CRITICAL FIX: Detect infrastructure errors that should FAIL tests even if
  // structuredContent.success is true or the expectation allows success as fallback.
  // This prevents false positives where tests like "error|success|handled" pass
  // even when the engine returns NO_NAVMESH, NOT_FOUND, NO_COMPONENT, etc.
  // Note: 'actor_not_found' is a valid error for negative tests - do NOT add it here
  // as that would fail tests that expect actor_not_found as the error.
  const infrastructureErrorCodes = [
    'no_navmesh', 'no_nav_sys', 'no_world', 'no_component', 'no_smart_link',
    'create_failed', 'spawn_failed', 'invalid_bp', 'cdo_failed', 
    'level_already_exists', 'physics_failed', 'function_not_found'
  ];
  const hasInfrastructureError = infrastructureErrorCodes.some(code => 
    errorStr === code || errorStr.includes(code) || messageStr.includes(code)
  );
  
  if (hasInfrastructureError && !primaryExpectsFailure) {
    return {
      passed: false,
      reason: `Infrastructure error detected but test expected success (primary intent: ${primaryCondition}): ${actualError || actualMessage}`
    };
  }

  // CRITICAL FIX: Detect crash/connection loss in error responses that should FAIL tests
  // unless explicitly expected. This prevents false positives where tests like "error|notfound"
  // pass on crash because "error" matches any error message.
  // CRITICAL FIX: Also detect "not connected" messages that indicate bridge disconnection
  // FIX: Only check for crash indicators if response is NOT a success (success: true)
  // This prevents false positives where "Node disconnection partial" (a valid success message)
  // incorrectly matches "disconnect" as a substring.
  const crashIndicators = ['1006', 'econnreset', 'socket hang up', 'bridge disconnected', 'ue_not_connected', 'automation bridge not connected'];
  // Word-boundary checks for ambiguous terms
  const hasDisconnect = /\bdisconnect(ed)?\b/i.test(errorStr) || /\bdisconnect(ed)?\b/i.test(messageStr);
  const hasConnectionLost = /\bconnection lost\b/i.test(errorStr) || /\bconnection lost\b/i.test(messageStr);
  const hasNotConnected = /\bnot connected\b/i.test(errorStr) || /\bnot connected\b/i.test(messageStr);
  
  const hasCrashIndicator = (crashIndicators.some(ind => 
    errorStr.includes(ind) || messageStr.includes(ind) || combined.includes(ind)
  )) || hasDisconnect || hasConnectionLost || hasNotConnected;
  
  // Only fail on crash indicators if response is NOT successful
  // "Node disconnection partial" is a SUCCESS message, not a crash indicator
  const responseSuccess = structuredSuccess === true;
  const isActuallyCrash = hasCrashIndicator && !responseSuccess;
  
  const explicitlyExpectsCrash = lowerExpected.includes('disconnect') || 
    lowerExpected.includes('crash') || 
    lowerExpected.includes('connection lost') ||
    lowerExpected.includes('ue_not_connected') ||
    lowerExpected.includes('not connected');
  
  if (isActuallyCrash && !explicitlyExpectsCrash) {
    return {
      passed: false,
      reason: `Crash/connection loss detected but test did not expect it: ${actualError || actualMessage}`
    };
  }

  // CRITICAL FIX: Detect timeout in structured responses that should FAIL tests
  // unless "timeout" is the PRIMARY expectation. This prevents false positives where
  // tests like "error" or "error|timeout" pass on timeout when the timeout is an
  // infrastructure failure, not a validation error.
  const hasTimeout = combined.includes('timeout') || combined.includes('timed out');
  const explicitlyExpectsTimeout = primaryCondition === 'timeout' || primaryCondition.includes('timeout');
  
  if (hasTimeout && !explicitlyExpectsTimeout) {
    return {
      passed: false,
      reason: `Timeout detected (infrastructure failure) but test did not expect timeout as primary condition (expected: ${primaryCondition}): ${actualError || actualMessage}`
    };
  }

  // CRITICAL FIX: Detect attachment failure for add_*_volume actions.
  // When a volume is created but attachment fails (e.g., static volume to movable actor),
  // the test should FAIL because the requested attachment did not succeed.
  const attachmentSucceeded = response.structuredContent?.attachmentSucceeded;
  if (attachmentSucceeded === false && primaryExpectsSuccess && !lowerExpected.includes('attachment failed')) {
    return {
      passed: false,
      reason: `Attachment failed for volume operation: ${actualMessage}. Volume was created but could not be attached to target actor.`
    };
  }

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
      // (e.g. "Request timed out") satisfy conditions where "timeout" is
      // the PRIMARY expected outcome (not just an alternative).
      // This prevents false positives where "error|timeout" passes on timeout
      // when the primary expectation is actually "error" (validation failure).
      if ((condition === 'timeout' || condition.includes('timeout')) && primaryCondition === condition) {
        if (combined.includes('timeout') || combined.includes('timed out')) {
          return { passed: true, reason: `Expected timeout condition met: ${condition}` };
        }
      }

      // CRITICAL FIX: Only match failure keywords in response if PRIMARY intent expects failure
      // This prevents false positives where "success|handled|object_not_found" passes when
      // response is success=false with "object_not_found" error - primary was "success"!
      const conditionIsFailure = failureKeywords.some((kw) => condition.includes(kw));
      const conditionIsSuccess = successKeywords.some((kw) => condition.includes(kw));
      
      // CRITICAL FIX: If condition is "error" and response.isError is true, match it
      // This handles cases where the error message doesn't contain the word "error" but
      // isError: true clearly indicates an error state.
      if (condition === 'error' && (response.isError === true || response.structuredContent?.isError === true)) {
        return { passed: true, reason: `Expected condition met: isError=true` };
      }
      
      // Only allow substring match if:
      // 1. Condition is a success keyword AND actualSuccess is true, OR
      // 2. Condition is a failure keyword AND primary expects failure, OR
      // 3. Condition is neither success nor failure keyword (neutral like "handled")
      if (combined.includes(condition)) {
        if (conditionIsSuccess && actualSuccess === true) {
          return { passed: true, reason: `Expected condition met: ${condition}` };
        }
        if (conditionIsFailure && primaryExpectsFailure) {
          return { passed: true, reason: `Expected condition met: ${condition}` };
        }
        if (!conditionIsSuccess && !conditionIsFailure) {
          // Neutral keyword (like "handled") - but still require that actualSuccess matches primary intent
          if ((primaryExpectsSuccess && actualSuccess === true) || (primaryExpectsFailure && actualSuccess === false)) {
            return { passed: true, reason: `Expected condition met: ${condition}` };
          }
        }
      }
    }
    // If none of the OR/pipe conditions matched, it's a failure
    return { passed: false, reason: `None of the expected conditions matched: ${expectedCondition}` };
  }

  // Also flag common automation/plugin failure phrases
  // NOTE: "not_implemented" is acceptable when test expects it (added to successKeywords)
  const pluginFailureIndicators = ['does not match prefix', 'unknown', 'unavailable', 'unsupported'];
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
    // Include security-related keywords for path traversal / injection tests
    // Include engine-specific error codes (OBJECT_NOT_FOUND, ACTOR_NOT_FOUND)
    // Support both lowercase (for readability) and uppercase (engine output) variants
    // CRITICAL FIX: Include plural forms like 'actors not found' to match 'actor_not_found'
    // The engine may return "Actors not found" (plural) for batch operations
    const specificErrorTypes = ['not found', 'not_found', 'invalid', 'missing', 'already exists', 'does not exist', 'sc_disabled', 'security', 'blocked', 'violation', 'object_not_found', 'actor_not_found', 'actors not found', 'CLASS_NOT_FOUND', 'ACTOR_NOT_FOUND', 'OBJECT_NOT_FOUND', 'COMPONENT_NOT_FOUND', 'PHYSICS_FAILED', 'FUNCTION_NOT_FOUND', 'NOT_FOUND', 'PROPERTY_NOT_FOUND'];
    const expectedErrorType = specificErrorTypes.find(type => lowerExpected.includes(type.toLowerCase()));
    // Normalize underscores/spaces for comparison (engine may return NOT_FOUND or "not found")
    const normalizedReason = lowerReason.replace(/_/g, ' ');
    const normalizedExpected = expectedErrorType ? expectedErrorType.toLowerCase().replace(/_/g, ' ') : '';
    let errorTypeMatch = expectedErrorType ? 
      (normalizedReason.includes(normalizedExpected) || lowerReason.includes(expectedErrorType.toLowerCase()) || lowerReason.includes(expectedErrorType.toUpperCase())) :
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
    if (lowerExpected.includes('not found') || lowerExpected.includes('not_found') || lowerExpected.includes('invalid') ||
      lowerExpected.includes('missing') || lowerExpected.includes('already exists') || 
      lowerExpected.includes('sc_disabled') || lowerExpected.includes('security') ||
      lowerExpected.includes('blocked') || lowerExpected.includes('violation') ||
      lowerExpected.includes('object_not_found') || lowerExpected.includes('actor_not_found') ||
      lowerExpected.includes('actors not found') ||  // CRITICAL FIX: Handle plural form
      lowerExpected.includes('class_not_found') || lowerExpected.includes('component_not_found') ||
      lowerExpected.includes('physics_failed') || lowerExpected.includes('function_not_found') ||
      lowerExpected.includes('property_not_found')) {
      
      // CRITICAL FIX: Special handling for actor_not_found vs "actors not found" (plural)
      // The engine may return "Actors not found" for batch operations even for single actor
      if ((lowerExpected.includes('actor_not_found') || lowerExpected.includes('actors not found')) &&
          (normalizedReason.includes('actors not found') || normalizedReason.includes('actor not found'))) {
        return { passed: true, reason: `Expected error type matched (actor/actors not found): ${actualMessage || actualError}` };
      }
      
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
    // NOTE: This MUST be defined before the setup code below uses it.
    // CRITICAL: We use TWO different timeouts:
    // 1. Server-side timeout (progress-extended) - passed via arguments.timeoutMs
    // 2. Client SDK timeout (fixed) - should be LONGER to allow progress extension
    callToolOnce = async function (callOptions, baseTimeoutMs) {
      const envDefault = Number(process.env.UNREAL_MCP_TEST_CALL_TIMEOUT_MS ?? '60000') || 60000;
      const perCall = Number(callOptions?.arguments?.timeoutMs) || undefined;
      const base = typeof baseTimeoutMs === 'number' && baseTimeoutMs > 0 ? baseTimeoutMs : (perCall || envDefault);
      const serverTimeoutMs = base;  // Server-side timeout (can be extended by progress)
      
      // Client SDK timeout should be LONGER than server timeout to allow progress extension
      // Use 5 minutes (ABSOLUTE_MAX_TIMEOUT_MS) to give progress updates time to extend server timeout
      const clientTimeoutMs = Number(process.env.UNREAL_MCP_TEST_CLIENT_TIMEOUT_MS ?? '300000') || 300000;
      
      try {
        console.log(`[CALL] ${callOptions.name} (server timeout: ${serverTimeoutMs}ms, client timeout: ${clientTimeoutMs}ms)`);
        const outgoing = Object.assign({}, callOptions, { arguments: { ...(callOptions.arguments || {}), timeoutMs: serverTimeoutMs } });
        // Prefer instructing the MCP client to use a matching timeout if
        // the client library supports per-call options; fall back to the
        // plain call if not supported.
        // CRITICAL: Use LONG client timeout to allow server-side progress extension
        let callPromise;
        try {
          // Correct parameter order: (params, resultSchema?, options)
          callPromise = client.callTool(outgoing, undefined, { timeout: clientTimeoutMs });
        } catch (err) {
          // Fall back to calling the older signature where options might be second param
          try {
            callPromise = client.callTool(outgoing, { timeout: clientTimeoutMs });
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
          timeoutId = setTimeout(() => rej(new Error(`Local test runner timeout after ${clientTimeoutMs}ms`)), clientTimeoutMs);
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

    // Retry helper with exponential backoff for setup operations
    // Helps handle transient failures like Intel GPU driver crashes
    async function callWithRetry(callOptions, options = {}) {
      const {
        maxRetries = 3,
        baseDelayMs = 1000,
        maxDelayMs = 10000,
        timeoutMs = 15000,
        operationName = callOptions.name
      } = options;
      
      let lastError = null;
      for (let attempt = 1; attempt <= maxRetries; attempt++) {
        try {
          const result = await callToolOnce(callOptions, timeoutMs);
          // Check for success in structured response
          if (result?.structuredContent?.success === false) {
            throw new Error(result.structuredContent.error || result.structuredContent.message || 'Operation failed');
          }
          if (attempt > 1) {
            console.log(`  ✅ ${operationName} succeeded on attempt ${attempt}/${maxRetries}`);
          }
          return result;
        } catch (err) {
          lastError = err;
          const errorMsg = String(err?.message || err);
          const isDriverCrash = errorMsg.toLowerCase().includes('disconnect') ||
                                errorMsg.toLowerCase().includes('1006') ||
                                errorMsg.toLowerCase().includes('crash') ||
                                errorMsg.toLowerCase().includes('exception');
          
          if (attempt < maxRetries) {
            const delay = Math.min(baseDelayMs * Math.pow(2, attempt - 1), maxDelayMs);
            const reason = isDriverCrash ? 'driver instability detected' : 'transient failure';
            console.warn(`  ⚠️  ${operationName} failed (attempt ${attempt}/${maxRetries}, ${reason}), retrying in ${delay}ms...`);
            await new Promise(r => setTimeout(r, delay));
          }
        }
      }
      throw lastError;
    }

    // === CLEANUP: Delete existing test assets from previous runs ===
    console.log('🧹 Cleaning up existing test assets...');
    try {
      // Delete test levels
      await callToolOnce({ name: 'manage_level', arguments: { action: 'unload', levelName: 'MainLevel' } }, 10000).catch(() => {});
      await callToolOnce({ name: 'manage_level', arguments: { action: 'unload', levelName: 'TestLevel' } }, 10000).catch(() => {});
      
      // Delete geometry actors
      await callToolOnce({ name: 'control_actor', arguments: { action: 'delete', actorName: 'TestBox' } }, 5000).catch(() => {});
      await callToolOnce({ name: 'control_actor', arguments: { action: 'delete', actorName: 'TestSphere' } }, 5000).catch(() => {});
      await callToolOnce({ name: 'control_actor', arguments: { action: 'delete', actorName: 'TestCylinder' } }, 5000).catch(() => {});
      await callToolOnce({ name: 'control_actor', arguments: { action: 'delete', actorName: 'TestActor' } }, 5000).catch(() => {});
      await callToolOnce({ name: 'control_actor', arguments: { action: 'delete', actorName: 'ParentActor' } }, 5000).catch(() => {});
      await callToolOnce({ name: 'control_actor', arguments: { action: 'delete', actorName: 'ChildActor' } }, 5000).catch(() => {});
      await callToolOnce({ name: 'control_actor', arguments: { action: 'delete', actorName: 'NavTestActor' } }, 5000).catch(() => {});
      await callToolOnce({ name: 'control_actor', arguments: { action: 'delete', actorName: 'TestSpline' } }, 5000).catch(() => {});
      await callToolOnce({ name: 'control_actor', arguments: { action: 'delete', actorName: 'TestRoad' } }, 5000).catch(() => {});
      await callToolOnce({ name: 'control_actor', arguments: { action: 'delete', actorName: 'SplineControlPoints' } }, 5000).catch(() => {});
      await callToolOnce({ name: 'control_actor', arguments: { action: 'delete', actorName: 'NavLinkProxy_Test' } }, 5000).catch(() => {});
      await callToolOnce({ name: 'control_actor', arguments: { action: 'delete', actorName: 'SmartNavLink_Test' } }, 5000).catch(() => {});
      
      // Delete test assets (blueprints, materials)
      await callToolOnce({ name: 'manage_asset', arguments: { action: 'delete_asset', assetPath: '/Game/MCPTest/BP_Test' } }, 10000).catch(() => {});
      await callToolOnce({ name: 'manage_asset', arguments: { action: 'delete_asset', assetPath: '/Game/MCPTest/SplineBP' } }, 10000).catch(() => {});
      await callToolOnce({ name: 'manage_asset', arguments: { action: 'delete_asset', assetPath: '/Game/MCPTest/TestMaterial' } }, 10000).catch(() => {});
      await callToolOnce({ name: 'manage_asset', arguments: { action: 'delete_asset', assetPath: '/Game/MCPTest/Parent' } }, 10000).catch(() => {});
      await callToolOnce({ name: 'manage_asset', arguments: { action: 'delete_asset', assetPath: '/Game/MCPTest/M_Test' } }, 10000).catch(() => {});
      await callToolOnce({ name: 'manage_asset', arguments: { action: 'delete_asset', assetPath: '/Game/MCPTest/ConvertedMesh' } }, 10000).catch(() => {});
      await callToolOnce({ name: 'manage_asset', arguments: { action: 'delete_asset', assetPath: '/Game/MCPTest/TestLandscape' } }, 10000).catch(() => {});
      // Delete asset test artifacts
      await callToolOnce({ name: 'manage_asset', arguments: { action: 'delete_asset', assetPath: '/Game/MCPTest/TestAsset' } }, 10000).catch(() => {});
      await callToolOnce({ name: 'manage_asset', arguments: { action: 'delete_asset', assetPath: '/Game/MCPTest/TestMesh' } }, 10000).catch(() => {});
      await callToolOnce({ name: 'manage_asset', arguments: { action: 'delete_asset', assetPath: '/Game/MCPTest/TestMat' } }, 10000).catch(() => {});
      await callToolOnce({ name: 'manage_asset', arguments: { action: 'delete_asset', assetPath: '/Game/MCPTest/TestInstance' } }, 10000).catch(() => {});
      
      // Delete foliage types
      await callToolOnce({ name: 'manage_asset', arguments: { action: 'delete_asset', assetPath: '/Game/Foliage/Grass' } }, 10000).catch(() => {});
      await callToolOnce({ name: 'manage_asset', arguments: { action: 'delete_asset', assetPath: '/Game/Foliage/Tree' } }, 10000).catch(() => {});
      await callToolOnce({ name: 'manage_asset', arguments: { action: 'delete_asset', assetPath: '/Game/Foliage/Bush' } }, 10000).catch(() => {});
      
      // Delete NavMeshBoundsVolume
      await callToolOnce({ name: 'control_actor', arguments: { action: 'delete', actorName: 'NavMeshBoundsVolume' } }, 5000).catch(() => {});
      
      console.log('✅ Cleanup complete\n');
    } catch (cleanupErr) {
      console.warn('⚠️  Cleanup had issues:', cleanupErr?.message || cleanupErr);
    }

    // Setup test assets before running tests
    console.log('🔧 Setting up test assets...');
    try {
      // Create test folder
      await callToolOnce({
        name: 'manage_asset',
        arguments: { action: 'create_folder', path: '/Game/MCPTest' }
      }, 10000).catch(() => { /* Folder may already exist */ });

      // Spawn TestActor with a mesh for physics tests (apply_force requires a mesh)
      // Use /Engine/EngineMeshes/Cube (exists in UE 5.3-5.7) instead of /Engine/BasicShapes/Cube (doesn't exist)
      // Use retry logic for critical actors that many tests depend on
      await callWithRetry({
        name: 'control_actor',
        arguments: {
          action: 'spawn',
          classPath: '/Script/Engine.StaticMeshActor',
          actorName: 'TestActor',
          meshPath: '/Engine/EngineMeshes/Cube',
          location: { x: 0, y: 0, z: 0 },
          rotation: { pitch: 0, yaw: 0, roll: 0 },
          scale: { x: 1, y: 1, z: 1 }
        }
      }, { maxRetries: 3, timeoutMs: 20000, operationName: 'spawn TestActor' }).catch(err => console.warn('⚠️  TestActor may already exist:', err?.message || err));

      // Enable physics on TestActor for apply_force tests
      // Set collision to QueryAndPhysics and enable SimulatePhysics on the StaticMeshComponent
      await callToolOnce({
        name: 'control_actor',
        arguments: {
          action: 'set_component_property',
          actorName: 'TestActor',
          componentName: 'StaticMeshComponent0',
          properties: { 
            'CollisionEnabled': 'QueryAndPhysics',
            'SimulatePhysics': true,
            'bSimulatePhysics': true 
          }
        }
      }, 10000).catch(err => console.warn('⚠️  Physics setup for TestActor failed:', err?.message || err));

      // Spawn ParentActor for attach/detach tests
      await callToolOnce({
        name: 'control_actor',
        arguments: {
          action: 'spawn',
          classPath: '/Script/Engine.StaticMeshActor',
          actorName: 'ParentActor',
          meshPath: '/Engine/EngineMeshes/Cube',
          location: { x: 200, y: 0, z: 0 }
        }
      }, 15000).catch(err => console.warn('⚠️  ParentActor may already exist:', err?.message || err));

      // Spawn ChildActor for attach/detach tests
      await callToolOnce({
        name: 'control_actor',
        arguments: {
          action: 'spawn',
          classPath: '/Script/Engine.StaticMeshActor',
          actorName: 'ChildActor',
          meshPath: '/Engine/EngineMeshes/Cube',
          location: { x: 300, y: 0, z: 0 }
        }
      }, 15000).catch(err => console.warn('⚠️  ChildActor may already exist:', err?.message || err));

      // Create Test Blueprint (critical for many tests - use retry)
      await callWithRetry({
        name: 'manage_blueprint',
        arguments: {
          action: 'create',
          name: 'BP_Test',
          path: '/Game/MCPTest',
          parentClass: 'Actor'
        }
      }, { maxRetries: 3, timeoutMs: 20000, operationName: 'create BP_Test blueprint' }).catch(err => console.warn('⚠️  BP_Test may already exist:', err?.message || err));

      // Create Test Material
      await callToolOnce({
        name: 'manage_asset',
        arguments: {
          action: 'create_material',
          name: 'TestMaterial',
          path: '/Game/MCPTest'
        }
      }, 15000).catch(err => console.warn('⚠️  TestMaterial may already exist:', err?.message || err));

      // Create Parent Material for create_material_instance tests
      await callToolOnce({
        name: 'manage_asset',
        arguments: {
          action: 'create_material',
          name: 'Parent',
          path: '/Game/MCPTest'
        }
      }, 15000).catch(err => console.warn('⚠️  Parent material may already exist:', err?.message || err));

      // Create M_Test Material for material authoring tests
      await callToolOnce({
        name: 'manage_asset',
        arguments: {
          action: 'create_material',
          name: 'M_Test',
          path: '/Game/MCPTest'
        }
      }, 15000).catch(err => console.warn('⚠️  M_Test material may already exist:', err?.message || err));

      // === Asset Test Setup for manage_asset tests ===
      // Create TestMat material for material graph/stats tests
      await callToolOnce({
        name: 'manage_asset',
        arguments: {
          action: 'create_material',
          name: 'TestMat',
          path: '/Game/MCPTest'
        }
      }, 15000).catch(err => console.warn('⚠️  TestMat material may already exist:', err?.message || err));

      // Create TestInstance material instance for reset_instance_parameters tests
      await callToolOnce({
        name: 'manage_asset',
        arguments: {
          action: 'create_material_instance',
          name: 'TestInstance',
          parentMaterial: '/Game/MCPTest/Parent',
          path: '/Game/MCPTest'
        }
      }, 15000).catch(err => console.warn('⚠️  TestInstance material instance may already exist:', err?.message || err));

      // Create TestAsset by duplicating an engine cube mesh
      // This is needed for duplicate/rename/move/get_dependencies/validate etc. tests
      // Use /Engine/EngineMeshes/Cube (exists in UE 5.3-5.7) instead of /Engine/BasicShapes/Cube (doesn't exist)
      await callToolOnce({
        name: 'manage_asset',
        arguments: {
          action: 'duplicate',
          sourcePath: '/Engine/EngineMeshes/Cube',
          destinationPath: '/Game/MCPTest/TestAsset'
        }
      }, 15000).catch(err => console.warn('⚠️  TestAsset may already exist:', err?.message || err));

      // Create TestMesh for generate_lods and nanite_rebuild_mesh tests
      await callToolOnce({
        name: 'manage_asset',
        arguments: {
          action: 'duplicate',
          sourcePath: '/Engine/EngineMeshes/Cube',
          destinationPath: '/Game/MCPTest/TestMesh'
        }
      }, 15000).catch(err => console.warn('⚠️  TestMesh may already exist:', err?.message || err));
      // === End Asset Test Setup ===

      // === Foliage Setup for build_environment tests ===
      // Create Foliage folder
      await callToolOnce({
        name: 'manage_asset',
        arguments: { action: 'create_folder', path: '/Game/Foliage' }
      }, 10000).catch(() => { /* Folder may already exist */ });

      // Create Grass foliage type using add_foliage_type action
      await callToolOnce({
        name: 'build_environment',
        arguments: {
          action: 'add_foliage_type',
          name: 'Grass',
          meshPath: '/Engine/BasicShapes/Sphere',
          density: 100
        }
      }, 15000).catch(err => console.warn('⚠️  Grass foliage type may already exist:', err?.message || err));

      // Create Tree foliage type
      await callToolOnce({
        name: 'build_environment',
        arguments: {
          action: 'add_foliage_type',
          name: 'Tree',
          meshPath: '/Engine/BasicShapes/Sphere',
          density: 50
        }
      }, 15000).catch(err => console.warn('⚠️  Tree foliage type may already exist:', err?.message || err));

      // Create Bush foliage type for procedural foliage tests
      await callToolOnce({
        name: 'build_environment',
        arguments: {
          action: 'add_foliage_type',
          name: 'Bush',
          meshPath: '/Engine/BasicShapes/Sphere',
          density: 75
        }
      }, 15000).catch(err => console.warn('⚠️  Bush foliage type may already exist:', err?.message || err));
      // === End Foliage Setup ===

      // === Geometry Setup for manage_geometry tests ===
      // Create TestBox for geometry manipulation tests
      await callToolOnce({
        name: 'manage_geometry',
        arguments: { action: 'create_box', name: 'TestBox', width: 100, height: 100, depth: 100 }
      }, 15000).catch(err => console.warn('⚠️  TestBox may already exist:', err?.message || err));

      // Create TestSphere for boolean operation tests
      await callToolOnce({
        name: 'manage_geometry',
        arguments: { action: 'create_sphere', name: 'TestSphere', radius: 50, segments: 16 }
      }, 15000).catch(err => console.warn('⚠️  TestSphere may already exist:', err?.message || err));

      // Create TestCylinder for additional geometry tests
      await callToolOnce({
        name: 'manage_geometry',
        arguments: { action: 'create_cylinder', name: 'TestCylinder', radius: 50, height: 100, segments: 16 }
      }, 15000).catch(err => console.warn('⚠️  TestCylinder may already exist:', err?.message || err));
      // === End Geometry Setup ===

      // === Navigation Setup for manage_navigation tests ===
      // Create NavMeshBoundsVolume so RecastNavMesh can be generated
      await callToolOnce({
        name: 'manage_volumes',
        arguments: {
          action: 'create_nav_mesh_bounds_volume',
          volumeName: 'TestNavMeshBounds',
          location: { x: 0, y: 0, z: 0 },
          extent: { x: 2000, y: 2000, z: 500 }
        }
      }, 15000).catch(err => console.warn('⚠️  TestNavMeshBounds may already exist:', err?.message || err));

      // Trigger navigation rebuild to generate RecastNavMesh
      await callToolOnce({
        name: 'manage_navigation',
        arguments: { action: 'rebuild_navigation' }
      }, 30000).catch(err => console.warn('⚠️  Navigation rebuild may have failed:', err?.message || err));

      // Create BP_Test blueprint for create_nav_modifier_component tests
      await callToolOnce({
        name: 'manage_blueprint',
        arguments: { action: 'create_blueprint', name: 'BP_Test', path: '/Game/MCPTest' }
      }, 15000).catch(err => console.warn('⚠️  BP_Test may already exist:', err?.message || err));

      // Add NavModifier component to BP_Test blueprint
      await callToolOnce({
        name: 'manage_navigation',
        arguments: {
          action: 'create_nav_modifier_component',
          blueprintPath: '/Game/MCPTest/BP_Test',
          componentName: 'NavModifier',
          areaClass: '/Script/NavigationSystem.NavArea_Obstacle'
        }
      }, 15000).catch(err => console.warn('⚠️  NavModifier component may already exist:', err?.message || err));

      // Spawn actor from BP_Test for set_nav_area_class tests
      // Use control_actor spawn_blueprint to spawn actor from blueprint
      await callToolOnce({
        name: 'control_actor',
        arguments: { action: 'spawn_blueprint', blueprintPath: '/Game/MCPTest/BP_Test', actorName: 'NavTestActor', location: [0, 0, 100] }
      }, 15000).catch(err => console.warn('⚠️  NavTestActor may already exist:', err?.message || err));
      // === End Navigation Setup ===

      // === Spline Setup for manage_splines tests ===
      // Create TestSpline for spline manipulation tests
      await callToolOnce({
        name: 'manage_splines',
        arguments: { action: 'create_spline_actor', actorName: 'TestSpline', location: {x:0,y:0,z:0} }
      }, 15000).catch(err => console.warn('⚠️  TestSpline may already exist:', err?.message || err));

      // Create template spline actors for specialized spline tests
      await callToolOnce({
        name: 'manage_splines',
        arguments: { action: 'create_road_spline', actorName: 'TestRoad', location: {x:500,y:0,z:0} }
      }, 15000).catch(err => console.warn('⚠️  TestRoad may already exist:', err?.message || err));

      await callToolOnce({
        name: 'manage_splines',
        arguments: { action: 'create_river_spline', actorName: 'TestRiver', location: {x:1000,y:0,z:0} }
      }, 15000).catch(err => console.warn('⚠️  TestRiver may already exist:', err?.message || err));

      await callToolOnce({
        name: 'manage_splines',
        arguments: { action: 'create_fence_spline', actorName: 'TestFence', location: {x:1500,y:0,z:0} }
      }, 15000).catch(err => console.warn('⚠️  TestFence may already exist:', err?.message || err));

      await callToolOnce({
        name: 'manage_splines',
        arguments: { action: 'create_wall_spline', actorName: 'TestWall', location: {x:2000,y:0,z:0} }
      }, 15000).catch(err => console.warn('⚠️  TestWall may already exist:', err?.message || err));

      await callToolOnce({
        name: 'manage_splines',
        arguments: { action: 'create_cable_spline', actorName: 'TestCable', location: {x:2500,y:0,z:100} }
      }, 15000).catch(err => console.warn('⚠️  TestCable may already exist:', err?.message || err));

      await callToolOnce({
        name: 'manage_splines',
        arguments: { action: 'create_pipe_spline', actorName: 'TestPipe', location: {x:3000,y:0,z:0} }
      }, 15000).catch(err => console.warn('⚠️  TestPipe may already exist:', err?.message || err));

      // Create SplineBP blueprint for create_spline_mesh_component tests
      // Note: create_blueprint creates directly under path, so /Game/ creates /Game/SplineBP
      await callToolOnce({
        name: 'manage_blueprint',
        arguments: { action: 'create_blueprint', name: 'SplineBP', path: '/Game' }
      }, 15000).catch(err => console.warn('⚠️  SplineBP may already exist:', err?.message || err));
      // === End Spline Setup ===

      // === Level Structure Setup for manage_level_structure tests ===
      // Create TestLevel for level blueprint and level instance tests
      // Use retry logic for level creation due to Intel GPU driver instability
      await callWithRetry({
        name: 'manage_level_structure',
        arguments: { 
          action: 'create_level', 
          levelName: 'TestLevel', 
          levelPath: '/Game/MCPTest',
          bCreateWorldPartition: false,
          save: true
        }
      }, { maxRetries: 5, timeoutMs: 20000, operationName: 'create TestLevel' }).catch(err => console.warn('⚠️  TestLevel creation failed after retries:', err?.message || err));

      // Create MainLevel for streaming/sublevel tests
      await callWithRetry({
        name: 'manage_level_structure',
        arguments: { 
          action: 'create_level', 
          levelName: 'MainLevel', 
          levelPath: '/Game/MCPTest',
          bCreateWorldPartition: false,
          save: true
        }
      }, { maxRetries: 5, timeoutMs: 20000, operationName: 'create MainLevel' }).catch(err => console.warn('⚠️  MainLevel creation failed after retries:', err?.message || err));

      // Create DataLayers folder for data layer tests
      await callToolOnce({
        name: 'manage_asset',
        arguments: { action: 'create_folder', path: '/Game/MCPTest/DataLayers' }
      }, 10000).catch(() => { /* Folder may already exist */ });

      // Note: TestLayer creation requires World Partition enabled on the level
      // and the DataLayerEditorSubsystem to be available. We create it per-test
      // with unique names in the test file itself.
      // === End Level Structure Setup ===

      console.log('✅ Test assets setup complete\n');
    } catch (setupErr) {
      console.warn('⚠️  Test asset setup had issues (tests may fail if assets missing):', setupErr?.message || setupErr);
    }

    // Helper function to reset geometry for manage_geometry tests
    // This prevents polygon explosion from accumulating across tests
    let geometryResetCounter = 0;
    async function resetGeometryActors() {
      try {
        // Delete existing geometry actors
        await callToolOnce({
          name: 'control_actor',
          arguments: { action: 'delete', actorName: 'TestBox' }
        }, 5000).catch(() => { /* ignore if doesn't exist */ });
        
        await callToolOnce({
          name: 'control_actor',
          arguments: { action: 'delete', actorName: 'TestSphere' }
        }, 5000).catch(() => { /* ignore if doesn't exist */ });
        
        // Recreate fresh geometry
        await callToolOnce({
          name: 'manage_geometry',
          arguments: { action: 'create_box', name: 'TestBox', width: 100, height: 100, depth: 100 }
        }, 10000);
        
        await callToolOnce({
          name: 'manage_geometry',
          arguments: { action: 'create_sphere', name: 'TestSphere', radius: 50, segments: 16 }
        }, 10000);
      } catch (err) {
        console.warn('⚠️  Geometry reset failed:', err?.message || err);
      }
    }

    // Run each test case
    // Rate limit: 600 req/min = 10 req/sec, so add 100ms delay between tests
    const TEST_THROTTLE_MS = Number(process.env.UNREAL_MCP_TEST_THROTTLE_MS ?? 100);
    
    for (let i = 0; i < testCases.length; i++) {
      const testCase = testCases[i];
      // CRITICAL FIX: Check testCase.timeoutMs FIRST (test case level), then arguments.timeoutMs
      // Test files put timeoutMs at test case level, not inside arguments
      const testCaseTimeoutMs = Number(process.env.UNREAL_MCP_TEST_CASE_TIMEOUT_MS ?? testCase.timeoutMs ?? testCase.arguments?.timeoutMs ?? '5000');
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
        let { passed, reason } = evaluateExpectation(testCase, normalizedResponse);

        // CRITICAL FIX: For performance tests (tests with timeoutMs), if the response
        // has success=false AND the PRIMARY expectation is success, the test should FAIL.
        // However, if the PRIMARY expectation is failure (error/not found/etc), then
        // success=false is the EXPECTED outcome and the test should PASS.
        // This fixes negative test cases (like delete_object with non-existent actor)
        // where success=false is the correct expected result.
        const isPerformanceTest = testCase.arguments?.timeoutMs !== undefined || 
                                  testCase.scenario?.includes('performance');
        const responseSuccess = normalizedResponse?.structuredContent?.success;
        
        // Check PRIMARY intent - if the test EXPECTS failure, success=false is correct
        const lowerExpected = (testCase.expected || '').toString().toLowerCase();
        const primaryCondition = lowerExpected.split('|')[0].split(' or ')[0].trim();
        const primaryExpectsFailure = failureKeywords.some((word) => primaryCondition.includes(word));
        
        // Only fail performance test if:
        // 1. It's a performance test AND
        // 2. Response shows success=false AND
        // 3. PRIMARY expectation is NOT failure (test expected success)
        if (isPerformanceTest && responseSuccess === false && !primaryExpectsFailure) {
          passed = false;
          const errorMsg = normalizedResponse?.structuredContent?.error || 
                          normalizedResponse?.structuredContent?.message || 
                          'Operation failed during performance test';
          reason = `Performance test failed: Operation returned success=false. Error: ${errorMsg}`;
        }

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

        // CRITICAL: Detect crash/connection loss indicators that should ALWAYS fail tests
        // unless the test explicitly expects disconnection. This prevents false positives
        // where tests like "error|not found" pass on crash/connection loss because
        // "error" matches any error message.
        const crashIndicators = ['disconnect', '1006', 'econnreset', 'socket hang up', 'connection lost', 'bridge disconnected'];
        const isCrashError = crashIndicators.some(ind => lowerError.includes(ind));
        const explicitlyExpectsCrash = lowerExpected.includes('disconnect') || 
          lowerExpected.includes('crash') || 
          lowerExpected.includes('connection lost') ||
          lowerExpected.includes('ue_not_connected');
        
        if (isCrashError && !explicitlyExpectsCrash) {
          console.log(`[FAILED] ${testCase.scenario} (${durationMs.toFixed(1)} ms) => CRASH/CONNECTION LOSS: ${errorMessage}`);
          results.push({
            scenario: testCase.scenario,
            toolName: testCase.toolName,
            arguments: testCase.arguments,
            status: 'failed',
            durationMs,
            detail: `Infrastructure failure (crash/disconnection): ${errorMessage}`
          });
          continue;
        }

        // Determine PRIMARY intent from expected string
        // Only pass timeout tests when "timeout" is the PRIMARY expectation,
        // not just an alternative in the expected string.
        const primaryCondition = lowerExpected.split('|')[0].split(' or ')[0].trim();
        const primaryExpectsTimeout = primaryCondition === 'timeout' || primaryCondition.includes('timeout');

        // If the test's PRIMARY expectation is a timeout, then an MCP/client timeout
        // should be treated as the expected outcome. Accept both "timeout" and "timed out"
        // phrasing from different MCP client implementations.
        // 
        // CRITICAL: This fixes the bug where tests like "error|timeout|success" would pass
        // on timeout even though the PRIMARY expectation is "error" (validation failure).
        // A timeout in such cases is an infrastructure failure, not a validation success.
        if (primaryExpectsTimeout && (lowerError.includes('timeout') || lowerError.includes('timed out'))) {
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
      
      // Throttle to avoid rate limiting (600 req/min = 10 req/sec)
      if (TEST_THROTTLE_MS > 0 && i < testCases.length - 1) {
        await new Promise(resolve => setTimeout(resolve, TEST_THROTTLE_MS));
      }
      
      // GEOMETRY RESET: Reset geometry actors between manage_geometry tests to prevent
      // polygon explosion from accumulating. Destructive operations (subdivide, bevel, shell,
      // etc.) can create millions of triangles, eventually causing OOM crashes.
      // We reset every N destructive geometry tests to balance performance vs memory safety.
      const GEOMETRY_RESET_INTERVAL = 5; // Reset every 5 destructive geometry tests (reduced from 10)
      
      // High-impact operations that cause exponential triangle growth - ALWAYS reset before these
      const HIGH_IMPACT_OPS = ['poke', 'subdivide', 'triangulate', 'array_radial', 'array_linear'];
      
      const isGeometryTest = testCase.toolName === 'manage_geometry';
      const testAction = testCase.arguments?.action || '';
      const isDestructiveGeometryOp = isGeometryTest && [
        'subdivide', 'extrude', 'inset', 'outset', 'bevel', 'offset_faces', 'shell', 'chamfer',
        'boolean_union', 'boolean_subtract', 'boolean_intersection', 'remesh_uniform', 'poke',
        'array_linear', 'array_radial', 'cylindrify', 'spherify', 'bend', 'twist', 'taper',
        'noise_deform', 'smooth', 'relax', 'stretch', 'triangulate'
      ].some(op => testAction.includes(op));
      
      // Always reset BEFORE high-impact operations to prevent POLYGON_LIMIT_EXCEEDED
      const isHighImpactOp = isGeometryTest && HIGH_IMPACT_OPS.some(op => testAction.includes(op));
      if (isHighImpactOp) {
        console.log('  🔄 Resetting geometry before high-impact operation: ' + testAction);
        await resetGeometryActors();
      } else if (isDestructiveGeometryOp) {
        geometryResetCounter++;
        if (geometryResetCounter % GEOMETRY_RESET_INTERVAL === 0) {
          console.log('  🔄 Resetting geometry actors to prevent polygon accumulation...');
          await resetGeometryActors();
        }
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

