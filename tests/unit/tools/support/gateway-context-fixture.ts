import { Logger } from '../../../../src/utils/logging/logger.js';
import type { GatewayContext } from '../../../../src/server/tool-registry-gateway.js';
import type { AutomationRequestBridge, ITools } from '../../../../src/types/tools/tool-interfaces.js';

// A FULLY TYPED `ITools` for gateway unit tests.
//
// The fixtures these replace used `{ automationBridge } as unknown as ITools`,
// which silently accepts any shape: a renamed bridge method, or a new required
// member on ITools, would only surface as a runtime failure inside a test that
// was already asserting something else. Building a real ITools here means both
// fail at type-check instead.

export function gatewayTools(automationBridge: AutomationRequestBridge): ITools {
  return {
    systemTools: {
      executeConsoleCommand: async () => ({ success: true }),
      getProjectSettings: async () => ({})
    },
    assetResources: { list: async () => ({}) },
    automationBridge
  };
}

export function gatewayContext(
  automationBridge: AutomationRequestBridge,
  loggerName: string
): GatewayContext {
  return {
    tools: gatewayTools(automationBridge),
    logger: new Logger(loggerName, 'error'),
    elicitationTimeoutMs: 0,
    ensureConnected: async () => true
  };
}
