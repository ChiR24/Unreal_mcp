import fs from 'node:fs';
import path from 'node:path';
import { repoRoot, reportsDir } from './parameter-audit-context.mjs';
import { extractToolSchemas } from './parameter-audit-schema.mjs';
import {
  argumentSignature,
  captureTestSuites,
  groupCasesByTool,
  liveReportCases
} from './parameter-audit-suites.mjs';

function staticCasesFromSuites(suites) {
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
  return staticCases;
}

function summarizeToolCoverage(schema, casesByTool, parameterCoverageCasesByTool) {
  const toolCases = casesByTool.get(schema.name) ?? [];
  const toolParameterCoverageCases = parameterCoverageCasesByTool.get(schema.name) ?? [];
  const testedActions = new Set(toolCases.map((testCase) => testCase.action));
  const usedParameters = new Set(toolCases.flatMap((testCase) => testCase.parameters));
  const successfulParameters = new Set(toolParameterCoverageCases.flatMap((testCase) => testCase.parameters));
  const required = new Set(schema.required);
  const optionalParameters = schema.properties.filter((property) => !required.has(property) && property !== 'action');
  const declaredProperties = new Set(schema.properties);
  const declaredActions = new Set(schema.actions);
  const missingOptionalParameters = optionalParameters.filter((parameter) => !successfulParameters.has(parameter)).sort();

  return {
    tool: schema.name,
    declaredActions: schema.actions.length,
    testedActions: testedActions.size,
    missingActions: schema.actions.filter((action) => !testedActions.has(action)),
    extraActions: [...testedActions].filter((action) => !declaredActions.has(action)).sort(),
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
}

function summarizeTotals(tools, suites, staticCases, cases, parameterCoverageCases, live) {
  return tools.reduce(
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
}

export async function buildAudit(options) {
  const schemas = extractToolSchemas();
  const suites = await captureTestSuites();
  const staticCases = staticCasesFromSuites(suites);
  const live = options.staticOnly
    ? { cases: [], missingReports: [], failedCases: [], handledFailureCases: [], reports: [] }
    : liveReportCases(suites);
  const cases = options.staticOnly ? staticCases : live.cases;
  const parameterCoverageCases = options.staticOnly
    ? cases
    : cases.filter((testCase) => testCase.responseSuccess === true);
  const casesByTool = groupCasesByTool(cases);
  const parameterCoverageCasesByTool = groupCasesByTool(parameterCoverageCases);
  const tools = schemas.map((schema) => summarizeToolCoverage(schema, casesByTool, parameterCoverageCasesByTool));

  return {
    generatedAt: new Date().toISOString(),
    strict: options.strict,
    optionalStrict: options.optionalStrict,
    coverageBasis: options.staticOnly ? 'static-test-definitions' : 'successful-live-report-cases',
    missingLiveReports: live.missingReports,
    failedLiveCases: live.failedCases,
    handledFailureLiveCases: live.handledFailureCases,
    liveReports: live.reports,
    totals: summarizeTotals(tools, suites, staticCases, cases, parameterCoverageCases, live),
    tools
  };
}

export function writeReport(audit) {
  fs.mkdirSync(reportsDir, { recursive: true });
  const reportPath = path.join(reportsDir, `parameter-combination-audit-${audit.generatedAt.replace(/[:]/g, '-')}.json`);
  fs.writeFileSync(reportPath, JSON.stringify(audit, null, 2));
  return reportPath;
}

export function printSummary(audit, reportPath) {
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
