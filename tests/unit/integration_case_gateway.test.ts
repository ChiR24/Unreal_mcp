/**
 * tests/unit/integration_case_gateway.test.ts
 *
 * Every integration case runs here through the real stdio gateway, with the bridge stubbed, so a
 * case the gateway would refuse before Unreal sees it fails offline. Records change (a parameter
 * renamed, an alias dropped, a type corrected) while the cases that drive them live in .mjs suites
 * nothing type-checks: 104 success cases had drifted that way (`path` for `savePath`, `functionPath`
 * for `assetPath`, parameters the handler never read), and a live run reported them as editor
 * failures. An error case refused for an undeclared parameter never reached the check it was
 * written for, so it passed for the wrong reason.
 */

import { afterEach, describe, expect, it } from 'vitest';
import { Logger } from '../../src/utils/logging/logger.js';
import type { GatewayContext } from '../../src/server/tool-registry-gateway.js';
import { handleUnrealGatewayCall } from '../../src/server/tool-registry-gateway.js';
import { dynamicToolManager } from '../../src/tools/dynamic/dynamic-tool-manager.js';
import type { ITools } from '../../src/types/tools/tool-interfaces.js';
import { expectedCondition, splitExpectedConditions } from '../expectation-utils.mjs';
import { captureTestSuites } from '../parameter-audit-suites.mjs';
import { toGatewayCall } from '../test-runner.mjs';

// Refusals that mean the case, not the editor, is wrong.
const SCHEMA_REFUSALS = new Set([
  'UNKNOWN_TOOL', 'UNKNOWN_ACTION', 'UNKNOWN_CAPABILITY', 'UNDECLARED_PARAMETER',
  'MISSING_REQUIRED_PARAMETER', 'MISSING_REQUIRED_ONEOF', 'INVALID_PARAMETER_TYPE', 'INVALID_PARAMETER_VALUE',
]);
// A `${captured:...}` value is filled in by the runner from an earlier reply, so its type cannot be checked here.
const VALUE_REFUSALS = new Set(['INVALID_PARAMETER_TYPE', 'INVALID_PARAMETER_VALUE']);

function stubbedBridge(): GatewayContext {
  const tools = {
    automationBridge: { isConnected: () => true, sendAutomationRequest: async () => ({ success: true, message: 'ok' }) },
  } as unknown as ITools;
  return { tools, logger: new Logger('integration-case-gateway', 'error'), ensureConnected: async () => true };
}

afterEach(() => {
  dynamicToolManager.reset();
});

describe('integration cases against the gateway contract', () => {
  it('sends only declared parameters, and every required one a success case needs', async () => {
    const refused: string[] = [];
    for (const suite of await captureTestSuites()) {
      for (const testCase of suite.cases ?? []) {
        if (!testCase.toolName || testCase.toolName === 'unreal') continue;
        const expected = expectedCondition(testCase.expected);
        const intent = splitExpectedConditions(expected)[0];
        const args = testCase.arguments ?? {};
        const reply = (await handleUnrealGatewayCall(toGatewayCall(testCase.toolName, args).arguments, stubbedBridge())) as Record<string, unknown>;
        dynamicToolManager.reset();
        const code = String(reply.errorCode ?? '');
        const wrong = intent === 'error'
          ? code === 'UNDECLARED_PARAMETER' && !/undeclared/i.test(`${expected} ${String(testCase.scenario)}`)
          : SCHEMA_REFUSALS.has(code) && !(VALUE_REFUSALS.has(code) && JSON.stringify(args).includes('${captured:'));
        if (wrong) refused.push(`${suite.filePath} | ${String(testCase.scenario)}: ${code} ${String(reply.message ?? '')}`);
      }
    }
    expect(refused).toEqual([]);
  }, 60_000);
});
