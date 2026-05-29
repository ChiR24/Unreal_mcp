#!/usr/bin/env node

import path from 'node:path';
import fs from 'node:fs';
import { createRequire } from 'node:module';
import { fileURLToPath } from 'node:url';
import ts from 'typescript';

const require = createRequire(import.meta.url);
const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const AsyncFunction = Object.getPrototypeOf(async function () {}).constructor;
const repoRoot = path.resolve(__dirname, '..');
const schemaPath = path.join(repoRoot, 'src/tools/consolidated-tool-definitions.ts');
const testsRoot = path.join(repoRoot, 'tests/mcp-tools');
const integrationSuitePath = path.join(repoRoot, 'tests/integration.mjs');
const reportsDir = path.join(repoRoot, 'tests/reports');
const args = process.argv.slice(2);
const allowedFlags = new Set(['--strict', '--static', '--optional-strict', '--coverage-strict', '--help', '-h']);
const unknownFlags = args.filter((arg) => arg.startsWith('-') && !allowedFlags.has(arg));

if (args.includes('--help') || args.includes('-h')) {
  console.log(`Usage: node tests/parameter-combination-audit.mjs [options]

Options:
  --static             Audit static test definitions instead of live reports.
  --strict             Fail on schema/action drift and undeclared test parameters.
  --optional-strict    Also fail on unreferenced optional parameter coverage debt.
  --coverage-strict    Alias for --optional-strict.
  --help, -h           Show this help text.`);
  process.exit(0);
}

if (unknownFlags.length > 0) {
  console.error(`Unknown option(s): ${unknownFlags.join(', ')}`);
  console.error('Run with --help for usage.');
  process.exit(2);
}

const strict = args.includes('--strict');
const staticOnly = args.includes('--static');
const optionalStrict = args.includes('--optional-strict') || args.includes('--coverage-strict');
const fakeSuccessIndicators = ['not implemented', 'unsupported', 'stub', 'no-op', 'noop', 'placeholder'];

function expectedCondition(expectation) {
  if (expectation && typeof expectation === 'object') {
    if (typeof expectation.condition === 'string') return expectation.condition;

    const conditions = [];
    if (typeof expectation.successPattern === 'string') {
      conditions.push('success', expectation.successPattern);
    }
    if (typeof expectation.errorPattern === 'string') {
      conditions.push('error', expectation.errorPattern);
    }
    if (conditions.length > 0) return conditions.join('|');

    try {
      return JSON.stringify(expectation);
    } catch {
      return String(expectation);
    }
  }
  return typeof expectation === 'string' ? expectation : String(expectation ?? '');
}

function caseKey(suiteName, toolName, args, scenario) {
  return [suiteName, toolName ?? '', args?.action ?? '', argumentSignature(args), scenario ?? ''].join('\u001f');
}

function propertyName(node) {
  if (ts.isIdentifier(node.name) || ts.isStringLiteral(node.name) || ts.isNumericLiteral(node.name)) {
    return node.name.text;
  }
  return undefined;
}

function getProperty(objectLiteral, name) {
  for (const property of objectLiteral.properties) {
    if (!ts.isPropertyAssignment(property)) continue;
    if (propertyName(property) === name) return property.initializer;
  }
  return undefined;
}

function stringArrayFromArrayLiteral(arrayLiteral, constants) {
  const values = [];
  for (const element of arrayLiteral.elements) {
    if (ts.isStringLiteral(element)) {
      values.push(element.text);
      continue;
    }
    if (ts.isSpreadElement(element) && ts.isIdentifier(element.expression)) {
      values.push(...(constants.get(element.expression.text) ?? []));
    }
  }
  return [...new Set(values)].sort();
}

function extractActionConstants(sourceFile) {
  const constants = new Map();
  for (const statement of sourceFile.statements) {
    if (!ts.isVariableStatement(statement)) continue;
    for (const declaration of statement.declarationList.declarations) {
      if (!ts.isIdentifier(declaration.name) || !declaration.name.text.endsWith('_ACTIONS')) continue;
      if (!declaration.initializer) continue;
      const initializer = ts.isAsExpression(declaration.initializer) ? declaration.initializer.expression : declaration.initializer;
      if (!ts.isArrayLiteralExpression(initializer)) continue;
      constants.set(declaration.name.text, stringArrayFromArrayLiteral(initializer, constants));
    }
  }
  return constants;
}

function extractToolSchemas() {
  const source = fs.readFileSync(schemaPath, 'utf8');
  const sourceFile = ts.createSourceFile(schemaPath, source, ts.ScriptTarget.Latest, true, ts.ScriptKind.TS);
  const constants = extractActionConstants(sourceFile);
  const tools = [];

  for (const statement of sourceFile.statements) {
    if (!ts.isVariableStatement(statement)) continue;
    for (const declaration of statement.declarationList.declarations) {
      if (!ts.isIdentifier(declaration.name) || declaration.name.text !== 'consolidatedToolDefinitions') continue;
      if (!declaration.initializer || !ts.isArrayLiteralExpression(declaration.initializer)) continue;

      for (const element of declaration.initializer.elements) {
        if (!ts.isObjectLiteralExpression(element)) continue;
        const nameNode = getProperty(element, 'name');
        if (!nameNode || !ts.isStringLiteral(nameNode)) continue;
        const inputSchema = getProperty(element, 'inputSchema');
        if (!inputSchema || !ts.isObjectLiteralExpression(inputSchema)) continue;
        const properties = getProperty(inputSchema, 'properties');
        if (!properties || !ts.isObjectLiteralExpression(properties)) continue;

        const action = getProperty(properties, 'action');
        const actionEnum = action && ts.isObjectLiteralExpression(action) ? getProperty(action, 'enum') : undefined;
        const required = getProperty(inputSchema, 'required');

        tools.push({
          name: nameNode.text,
          actions: actionEnum && ts.isArrayLiteralExpression(actionEnum) ? stringArrayFromArrayLiteral(actionEnum, constants) : [],
          properties: properties.properties
            .filter(ts.isPropertyAssignment)
            .map(propertyName)
            .filter((name) => typeof name === 'string')
            .sort(),
          required: required && ts.isArrayLiteralExpression(required) ? stringArrayFromArrayLiteral(required, constants) : []
        });
      }
    }
  }

  return tools;
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
  return require(specifier);
}

async function captureTestSuites() {
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

function argumentSignature(args) {
  return Object.keys(args ?? {})
    .filter((key) => key !== 'action')
    .sort()
    .join('+');
}

function latestReportForSuite(suiteName) {
  if (!fs.existsSync(reportsDir)) return undefined;
  const prefix = `${suiteName}-test-results-`;
  const candidates = fs.readdirSync(reportsDir)
    .filter((entry) => entry.startsWith(prefix) && entry.endsWith('.json'))
    .map((entry) => path.join(reportsDir, entry));
  let latest;
  for (const candidate of candidates) {
    try {
      const report = JSON.parse(fs.readFileSync(candidate, 'utf8'));
      if (!latest || String(report.generatedAt ?? '') > String(latest.report.generatedAt ?? '')) {
        latest = { path: candidate, report };
      }
    } catch {
      // Malformed historical reports cannot prove coverage.
    }
  }
  return latest;
}

function liveReportCases(suites) {
  const suiteNames = [...new Set(suites.map((suite) => suite.name))].sort();
  const staticCasesByKey = new Map();
  for (const suite of suites) {
    for (const testCase of suite.cases ?? []) {
      const args = testCase.arguments ?? {};
      if (!testCase.toolName || typeof args.action !== 'string') continue;
      staticCasesByKey.set(caseKey(suite.name, testCase.toolName, args, testCase.scenario), testCase);
    }
  }
  const missingReports = [];
  const failedCases = [];
  const handledFailureCases = [];
  const reports = [];
  const cases = [];

  for (const suiteName of suiteNames) {
    const latest = latestReportForSuite(suiteName);
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
      const staticCase = staticCasesByKey.get(caseKey(suiteName, result.toolName, args, result.scenario));
      const expected = result.expected ?? staticCase?.expected;

      const hasOutcomeFields = typeof result.responseSuccess === 'boolean' || typeof result.responseIsError === 'boolean';
      if (!hasOutcomeFields) {
        failedCases.push({
          suite: suiteName,
          scenario: result.scenario,
          status: 'passed-without-response-outcome',
          expected: expectedCondition(expected)
        });
        continue;
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
        continue;
      }

      if (responseSucceeded) {
        const successText = [result.detail, result.responseMessage, result.responseError]
          .filter((value) => typeof value === 'string')
          .join('\n')
          .toLowerCase();
        if (fakeSuccessIndicators.some((indicator) => successText.includes(indicator))) {
          failedCases.push({
            suite: suiteName,
            scenario: result.scenario,
            status: 'passed-with-fake-success-indicator'
          });
          continue;
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
        filePath: path.relative(repoRoot, latest.path),
        scenario: result.scenario,
        toolName: result.toolName,
        action: args.action,
        parameters: Object.keys(args).filter((key) => key !== 'action').sort(),
        signature: argumentSignature(args),
        responseSuccess: result.responseSuccess
      });
    }
  }

  return { cases, missingReports, failedCases, handledFailureCases, reports };
}

async function buildAudit() {
  const schemas = extractToolSchemas();
  const suites = await captureTestSuites();
  const staticCases = [];

  for (const suite of suites) {
    for (const testCase of suite.cases) {
      const args = testCase.arguments ?? {};
      if (!testCase.toolName || typeof args.action !== 'string') continue;
      staticCases.push({
        suite: suite.name,
        filePath: suite.filePath,
        scenario: testCase.scenario,
        toolName: testCase.toolName,
        action: args.action,
        expected: testCase.expected,
        parameters: Object.keys(args).filter((key) => key !== 'action').sort(),
        signature: argumentSignature(args)
      });
    }
  }

  const live = staticOnly
    ? { cases: [], missingReports: [], failedCases: [], handledFailureCases: [], reports: [] }
    : liveReportCases(suites);
  const cases = staticOnly ? staticCases : live.cases;
  const parameterCoverageCases = staticOnly
    ? cases
    : cases.filter((testCase) => testCase.responseSuccess === true);

  const tools = schemas.map((schema) => {
    const toolCases = cases.filter((testCase) => testCase.toolName === schema.name);
    const toolParameterCoverageCases = parameterCoverageCases.filter((testCase) => testCase.toolName === schema.name);
    const testedActions = new Set(toolCases.map((testCase) => testCase.action));
    const usedParameters = new Set(toolCases.flatMap((testCase) => testCase.parameters));
    const successfulParameters = new Set(toolParameterCoverageCases.flatMap((testCase) => testCase.parameters));
    const required = new Set(schema.required);
    const optionalParameters = schema.properties.filter((property) => !required.has(property) && property !== 'action');
    const declaredProperties = new Set(schema.properties);
    const missingOptionalParameters = optionalParameters.filter((parameter) => !successfulParameters.has(parameter)).sort();

    return {
      tool: schema.name,
      declaredActions: schema.actions.length,
      testedActions: testedActions.size,
      missingActions: schema.actions.filter((action) => !testedActions.has(action)),
      extraActions: [...testedActions].filter((action) => !schema.actions.includes(action)).sort(),
      optionalParameters: optionalParameters.length,
      coveredOptionalParameters: optionalParameters.filter((parameter) => successfulParameters.has(parameter)).length,
      missingOptionalParameters,
      failureOnlyOptionalParameters: missingOptionalParameters.filter((parameter) => usedParameters.has(parameter)),
      extraParameters: [...usedParameters].filter((parameter) => !declaredProperties.has(parameter)).sort(),
      actions: schema.actions.map((action) => {
        const actionCases = toolCases.filter((testCase) => testCase.action === action);
        return {
          action,
          caseCount: actionCases.length,
          parameterCombinations: [...new Set(actionCases.map((testCase) => testCase.signature))].sort()
        };
      })
    };
  });

  const totals = tools.reduce(
    (acc, tool) => {
      acc.declaredActions += tool.declaredActions;
      acc.testedActions += tool.testedActions;
      acc.missingActions += tool.missingActions.length;
      acc.extraActions += tool.extraActions.length;
      acc.optionalParameters += tool.optionalParameters;
      acc.coveredOptionalParameters += tool.coveredOptionalParameters;
      acc.missingOptionalParameters += tool.missingOptionalParameters.length;
      acc.failureOnlyOptionalParameters += tool.failureOnlyOptionalParameters.length;
      acc.extraParameters += tool.extraParameters.length;
      return acc;
    },
    {
      declaredActions: 0,
      testedActions: 0,
      missingActions: 0,
      extraActions: 0,
      optionalParameters: 0,
      coveredOptionalParameters: 0,
      missingOptionalParameters: 0,
      failureOnlyOptionalParameters: 0,
      extraParameters: 0,
      testSuites: suites.length,
      staticTestCases: staticCases.length,
      coveredTestCases: cases.length,
      parameterCoverageTestCases: parameterCoverageCases.length,
      missingLiveReports: live.missingReports.length,
      failedLiveCases: live.failedCases.length,
      handledFailureLiveCases: live.handledFailureCases.length
    }
  );

  return {
    generatedAt: new Date().toISOString(),
    strict,
    optionalStrict,
    coverageBasis: staticOnly ? 'static-test-definitions' : 'successful-live-report-cases',
    missingLiveReports: live.missingReports,
    failedLiveCases: live.failedCases,
    handledFailureLiveCases: live.handledFailureCases,
    liveReports: live.reports,
    totals,
    tools
  };
}

function writeReport(audit) {
  fs.mkdirSync(reportsDir, { recursive: true });
  const reportPath = path.join(reportsDir, `parameter-combination-audit-${audit.generatedAt.replace(/[:]/g, '-')}.json`);
  fs.writeFileSync(reportPath, JSON.stringify(audit, null, 2));
  return reportPath;
}

function printSummary(audit, reportPath) {
  console.log('Parameter Combination Audit');
  console.log(`Coverage basis: ${audit.coverageBasis}`);
  console.log(`Suites: ${audit.totals.testSuites}`);
  console.log(`Static test cases: ${audit.totals.staticTestCases}`);
  console.log(`Covered test cases: ${audit.totals.coveredTestCases}`);
  console.log(`Parameter coverage test cases: ${audit.totals.parameterCoverageTestCases}`);
  console.log(`Missing live reports: ${audit.totals.missingLiveReports}`);
  console.log(`Failed live cases: ${audit.totals.failedLiveCases}`);
  console.log(`Handled live failure cases: ${audit.totals.handledFailureLiveCases}`);
  console.log(`Actions: ${audit.totals.testedActions}/${audit.totals.declaredActions} covered`);
  console.log(`Optional parameters: ${audit.totals.coveredOptionalParameters}/${audit.totals.optionalParameters} referenced by parameter coverage tests`);
  console.log(`Unreferenced optional parameters: ${audit.totals.missingOptionalParameters}`);
  console.log(`Failure-only optional parameters: ${audit.totals.failureOnlyOptionalParameters}`);
  console.log(`Extra test parameters not declared in schema: ${audit.totals.extraParameters}`);
  console.log(`Report: ${path.relative(repoRoot, reportPath)}`);

  const toolsWithGaps = audit.tools.filter(
    (tool) => tool.missingActions.length > 0 || tool.missingOptionalParameters.length > 0 || tool.extraParameters.length > 0
  );
  if (toolsWithGaps.length > 0) {
    console.log('Tools with gaps:');
    for (const tool of toolsWithGaps) {
      console.log(`- ${tool.tool}: missingActions=${tool.missingActions.length}, missingOptionalParameters=${tool.missingOptionalParameters.length}, failureOnlyOptionalParameters=${tool.failureOnlyOptionalParameters.length}, extraParameters=${tool.extraParameters.length}`);
    }
  }
}

const audit = await buildAudit();
const reportPath = writeReport(audit);
printSummary(audit, reportPath);

if (audit.totals.missingActions > 0 || audit.totals.extraActions > 0) {
  process.exitCode = 1;
}

if (!staticOnly && (audit.totals.missingLiveReports > 0 || audit.totals.failedLiveCases > 0)) {
  process.exitCode = 1;
}

if (strict && audit.totals.extraParameters > 0) {
  process.exitCode = 1;
}

if (optionalStrict && audit.totals.missingOptionalParameters > 0) {
  process.exitCode = 1;
}
