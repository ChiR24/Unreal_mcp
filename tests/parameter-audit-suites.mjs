import fs from 'node:fs';
import path from 'node:path';
import { expectedCondition } from './expectation-utils.mjs';
import {
  AsyncFunction,
  integrationSuitePath,
  repoRoot,
  reportsDir,
  requireFromAudit,
  testsRoot
} from './parameter-audit-context.mjs';

const fakeSuccessIndicators = ['not implemented', 'unsupported', 'stub', 'no-op', 'noop', 'placeholder'];

export function argumentSignature(args) {
  return Object.keys(args ?? {})
    .filter((key) => key !== 'action')
    .sort()
    .join('+');
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

function auditRequire(specifier) {
  if (specifier === 'node:fs') {
    return {
      mkdirSync() {},
      writeFileSync() {},
      existsSync: fs.existsSync,
      readFileSync: fs.readFileSync,
      readdirSync: fs.readdirSync,
      statSync: fs.statSync
    };
  }
  return requireFromAudit(specifier);
}

export async function captureTestSuites() {
  const suites = [];
  for (const filePath of testSuiteFiles()) {
    let code = fs.readFileSync(filePath, 'utf8').replace(/^#!.*\n/, '');
    code = code.replace(
      /import \{ runToolTests \} from ['"](?:\.\.\/\.\.\/test-runner|\.\/test-runner)\.mjs['"];?/g,
      'const runToolTests = (name, cases) => { __captured.push({ name, cases }); };'
    );
    code = code.replace(/import fs from ['"]node:fs['"];?/g, "const fs = require('node:fs');");
    code = code.replace(/import path from ['"]node:path['"];?/g, "const path = require('node:path');");

    const captured = [];
    await AsyncFunction('require', '__captured', 'process', 'console', 'Date', code)(
      auditRequire,
      captured,
      process,
      { log() {}, warn() {}, error() {} },
      Date
    );

    for (const suite of captured) {
      suites.push({ filePath: path.relative(repoRoot, filePath), name: suite.name, cases: suite.cases ?? [] });
    }
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

  if (responseSucceeded) {
    const successText = [result.detail, result.responseMessage, result.responseError]
      .filter((value) => typeof value === 'string')
      .join('\n')
      .toLowerCase();
    if (fakeSuccessIndicators.some((indicator) => successText.includes(indicator))) {
      failedCases.push({ suite: suiteName, scenario: result.scenario, status: 'passed-with-fake-success-indicator' });
      return;
    }
  } else {
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
    parameters: Object.keys(args).filter((key) => key !== 'action').sort(),
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
