#!/usr/bin/env node
/**
 * Shared Test Runner for Unreal MCP Server Tools
 * Provides common test execution logic for all individual tool test files
 */

import fs from 'node:fs/promises';
import path from 'node:path';
import process from 'node:process';
import { fileURLToPath } from 'node:url';
import { performance } from 'node:perf_hooks';
import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { StdioClientTransport } from '@modelcontextprotocol/sdk/client/stdio.js';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const repoRoot = path.resolve(__dirname, '..');
const reportsDir = path.resolve(repoRoot, 'tests', 'reports');

const failureKeywords = ['error', 'fail', 'invalid', 'missing', 'not found', 'reject', 'warning'];
const successKeywords = ['success', 'spawn', 'visible', 'applied', 'returns', 'plays', 'updates', 'created', 'saved'];

const serverCommand = process.env.UNREAL_MCP_SERVER_CMD ?? 'node';
const serverArgs = process.env.UNREAL_MCP_SERVER_ARGS?.split(',') ?? [path.join(repoRoot, 'dist', 'cli.js')];
const serverCwd = process.env.UNREAL_MCP_SERVER_CWD ?? repoRoot;

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
  const actualSuccess = structuredSuccess ?? !response.isError;

  // Extract actual error/message from response
  let actualError = null;
  let actualMessage = null;
  if (response.structuredContent) {
    actualError = response.structuredContent.error;
    actualMessage = response.structuredContent.message;
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
    'Python fallback failed'
  ];
  
  const messageStr = (actualMessage || '').toString();
  const errorStr = (actualError || '').toString();
  const combinedText = (messageStr + ' ' + errorStr).toLowerCase();
  
  const hasPythonError = pythonSyntaxErrors.some(errType => 
    combinedText.includes(errType.toLowerCase())
  );

  // If expecting success but got Python error, test FAILS
  if (!containsFailure && hasPythonError) {
    return {
      passed: false,
      reason: `Expected success but got Python error: ${actualMessage || actualError}`
    };
  }

  // CRITICAL: Check if message says "failed" but success is true (FALSE POSITIVE)
  if (actualSuccess && (
    messageStr.toLowerCase().includes('failed') ||
    messageStr.toLowerCase().includes('python fallback failed') ||
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
 * Format a result line for console output
 */
function formatResultLine(testCase, status, detail, durationMs) {
  const durationText = typeof durationMs === 'number' ? ` (${durationMs.toFixed(1)} ms)` : '';
  return `[${status.toUpperCase()}] ${testCase.scenario}${durationText}${detail ? ` => ${detail}` : ''}`;
}

/**
 * Save test results to JSON report file
 */
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
  
  await fs.writeFile(resultsPath, JSON.stringify({
    generatedAt: new Date().toISOString(),
    toolName,
    results: serializable
  }, null, 2));
  
  return resultsPath;
}

/**
 * Print test summary statistics
 */
function summarize(toolName, results, resultsPath) {
  const totals = results.reduce((acc, result) => {
    acc.total += 1;
    acc[result.status] = (acc[result.status] ?? 0) + 1;
    return acc;
  }, { total: 0, passed: 0, failed: 0, skipped: 0 });

  console.log('\n' + '='.repeat(60));
  console.log(`${toolName} Test Summary`);
  console.log('='.repeat(60));
  console.log(`Total cases: ${totals.total}`);
  console.log(`✅ Passed: ${totals.passed ?? 0}`);
  console.log(`❌ Failed: ${totals.failed ?? 0}`);
  console.log(`⏭️  Skipped: ${totals.skipped ?? 0}`);
  
  if (totals.passed && totals.total > 0) {
    const passRate = ((totals.passed / totals.total) * 100).toFixed(1);
    console.log(`Pass rate: ${passRate}%`);
  }
  
  console.log(`Results saved to: ${resultsPath}`);
  console.log('='.repeat(60));
}

/**
 * Main test runner function
 */
export async function runToolTests(toolName, testCases) {
  console.log('='.repeat(60));
  console.log(`Starting ${toolName} Tests`);
  console.log(`Total test cases: ${testCases.length}`);
  console.log('='.repeat(60));
  console.log('');

  let transport;
  let client;
  const results = [];

  try {
    // Initialize MCP client
    transport = new StdioClientTransport({
      command: serverCommand,
      args: serverArgs,
      cwd: serverCwd,
      stderr: 'inherit'
    });

    client = new Client({
      name: 'unreal-mcp-test-runner',
      version: '1.0.0'
    });

    await client.connect(transport);
    await client.listTools({});
    
    console.log('✅ Connected to Unreal MCP Server\n');

    // Run each test case
    for (let i = 0; i < testCases.length; i++) {
      const testCase = testCases[i];
      const startTime = performance.now();

      try {
        const response = await client.callTool({
          name: testCase.toolName,
          arguments: testCase.arguments
        });

        const endTime = performance.now();
        const durationMs = endTime - startTime;

        // Parse structured response
        let structuredContent = null;
        if (response.content?.[0]?.text) {
          try {
            structuredContent = JSON.parse(response.content[0].text);
          } catch {
            structuredContent = null;
          }
        }

        const enrichedResponse = {
          ...response,
          structuredContent
        };

        // Evaluate test outcome
        const { passed, reason } = evaluateExpectation(testCase, enrichedResponse);
        const status = passed ? 'passed' : 'failed';

        results.push({
          ...testCase,
          status,
          durationMs,
          detail: reason
        });

        console.log(formatResultLine(testCase, status, passed ? null : reason, durationMs));

      } catch (error) {
        const endTime = performance.now();
        const durationMs = endTime - startTime;
        
        results.push({
          ...testCase,
          status: 'failed',
          durationMs,
          detail: `Exception: ${error.message}`
        });

        console.log(formatResultLine(testCase, 'failed', error.message, durationMs));
      }
    }

  } catch (error) {
    console.error('\n❌ Failed to initialize MCP client:', error.message);
    process.exitCode = 1;
    return;
  } finally {
    // Cleanup
    if (transport) {
      try {
        await transport.close();
      } catch {
        // Ignore cleanup errors
      }
    }
  }

  // Save and display results
  const resultsPath = await persistResults(toolName.toLowerCase().replace(/ /g, '-'), results);
  summarize(toolName, results, resultsPath);

  // Set exit code based on failures
  const failCount = results.filter(r => r.status === 'failed').length;
  if (failCount > 0) {
    process.exitCode = 1;
  }
}
