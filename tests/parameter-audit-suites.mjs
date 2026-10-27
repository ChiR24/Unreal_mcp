import fs from 'node:fs';
import path from 'node:path';
import { expectedCondition } from './expectation-utils.mjs';
import { withFoldTwins } from './fold-twins.mjs';
import { pathToFileURL } from 'node:url';
import { integrationSuitePath, repoRoot, reportsDir, testsRoot } from './parameter-audit-context.mjs';

// The harness merges a nested `params` object into the call (toGatewayCall), so the
// audit reads the merged keys.
export function caseParameters(args) {
  const nested = args?.params !== null && typeof args?.params === 'object' && !Array.isArray(args.params) ? args.params : {};
  return Object.keys({ ...nested, ...args }).filter((key) => key !== 'action' && key !== 'params').sort();
}

export function argumentSignature(args) {
  return caseParameters(args).join('+');
}

function caseKey(suiteName, toolName, args, scenario) {
  return [suiteName, toolName ?? '', args?.action ?? '', argumentSignature(args), scenario ?? ''].join('\u001f');
}

export function groupCasesByTool(cases) {
  const groups = new Map();
  for (const testCase of cases) {
    const toolCases = groups.get(testCase.toolName);
    if (toolCases) {
      toolCases.push(testCase);
    } else {
      groups.set(testCase.toolName, [testCase]);
    }
  }
  return groups;
}

function walkMjsFiles(dir) {
  const files = [];
  for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
    const fullPath = path.join(dir, entry.name);
    if (entry.isDirectory()) files.push(...walkMjsFiles(fullPath));
    if (entry.isFile() && entry.name.endsWith('.mjs')) files.push(fullPath);
  }
  return files;
}

function testSuiteFiles() {
  const files = walkMjsFiles(testsRoot);
  if (fs.existsSync(integrationSuitePath)) files.push(integrationSuitePath);
  return files.sort();
}

export async function captureTestSuites() {
  const suites = [];
  const captured = (globalThis.__capturedToolSuites = []);
  try {
    for (const filePath of testSuiteFiles()) {
      const before = captured.length;
      await import(pathToFileURL(filePath).href);
      for (const suite of captured.slice(before)) {
        // The runner derives the same twins, so the audit counts the cases that run.
        suites.push({ filePath: path.relative(repoRoot, filePath), name: suite.name, cases: withFoldTwins(suite.cases ?? []) });
      }
    }
  } finally {
    delete globalThis.__capturedToolSuites;
  }
  return suites;
}

function latestReportsForSuites(suiteNames) {
  const latestReports = new Map();
  if (!fs.existsSync(reportsDir)) return latestReports;

  const suitePrefixes = suiteNames.map((suiteName) => ({ suiteName, prefix: `${suiteName}-test-results-` }));
  for (const entry of fs.readdirSync(reportsDir)) {
    if (!entry.endsWith('.json')) continue;
    const suite = suitePrefixes.find(({ prefix }) => entry.startsWith(prefix));
    if (!suite) continue;

    const candidate = path.join(reportsDir, entry);
    try {
      const report = JSON.parse(fs.readFileSync(candidate, 'utf8'));
      const latest = latestReports.get(suite.suiteName);
      if (!latest || String(report.generatedAt ?? '') > String(latest.report.generatedAt ?? '')) {
        latestReports.set(suite.suiteName, { path: candidate, report });
      }
    } catch {
      // Malformed historical reports cannot prove coverage.
    }
  }

  return latestReports;
}

function staticCasesByKey(suites) {
  const casesByKey = new Map();
  for (const suite of suites) {
    for (const testCase of suite.cases ?? []) {
      const args = testCase.arguments ?? {};
      if (!testCase.toolName || typeof args.action !== 'string') continue;
      casesByKey.set(caseKey(suite.name, testCase.toolName, args, testCase.scenario), testCase);
    }
  }
  return casesByKey;
}

function recordLiveCase(result, suiteName, staticCase, failedCases, handledFailureCases, cases, reportPath) {
  const args = result.arguments ?? {};
  const expected = result.expected ?? staticCase?.expected;
  const hasOutcomeFields = typeof result.responseSuccess === 'boolean' || typeof result.responseIsError === 'boolean';
  if (!hasOutcomeFields) {
    failedCases.push({ suite: suiteName, scenario: result.scenario, status: 'passed-without-response-outcome', expected: expectedCondition(expected) });
    return;
  }

  const responseSucceeded = result.responseSuccess === true && result.responseIsError !== true;
  const responseFailedAsExpected = result.responseSuccess === false || result.responseIsError === true;
  if (!responseSucceeded && !responseFailedAsExpected) {
    failedCases.push({
      suite: suiteName,
      scenario: result.scenario,
      status: 'passed-with-unsuccessful-response',
      responseSuccess: result.responseSuccess,
      responseIsError: result.responseIsError,
      responseError: result.responseError
    });
    return;
  }

  if (!responseSucceeded) {
    handledFailureCases.push({
      suite: suiteName,
      scenario: result.scenario,
      expected: expectedCondition(expected),
      responseError: result.responseError,
      responseMessage: result.responseMessage
    });
  }

  cases.push({
    suite: suiteName,
    filePath: path.relative(repoRoot, reportPath),
    scenario: result.scenario,
    toolName: result.toolName,
    action: args.action,
    parameters: caseParameters(args),
    signature: argumentSignature(args),
    responseSuccess: result.responseSuccess
  });
}

export function liveReportCases(suites) {
  const suiteNames = [...new Set(suites.map((suite) => suite.name))].sort();
  const latestReports = latestReportsForSuites(suiteNames);
  const casesByKey = staticCasesByKey(suites);
  const missingReports = [];
  const failedCases = [];
  const handledFailureCases = [];
  const reports = [];
  const cases = [];

  for (const suiteName of suiteNames) {
    const latest = latestReports.get(suiteName);
    if (!latest) {
      missingReports.push(suiteName);
      continue;
    }

    reports.push(path.relative(repoRoot, latest.path));
    for (const result of latest.report.results ?? []) {
      if (result.status !== 'passed') {
        failedCases.push({ suite: suiteName, scenario: result.scenario, status: result.status });
        continue;
      }
      const args = result.arguments ?? {};
      if (!result.toolName || typeof args.action !== 'string') continue;
      const staticCase = casesByKey.get(caseKey(suiteName, result.toolName, args, result.scenario));
      recordLiveCase(result, suiteName, staticCase, failedCases, handledFailureCases, cases, latest.path);
    }
  }

  return { cases, missingReports, failedCases, handledFailureCases, reports };
}
