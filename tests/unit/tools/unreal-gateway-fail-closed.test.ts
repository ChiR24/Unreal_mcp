import { describe, expect, it } from 'vitest';

import { Logger } from '../../../src/utils/logging/logger.js';
import type { ITools } from '../../../src/types/tools/tool-interfaces.js';
import { handleUnrealGatewayCall, type GatewayContext } from '../../../src/server/tool-registry-gateway.js';

function makeContext(tokenConfigured: boolean): GatewayContext {
  const tools = {
    systemTools: {
      executeConsoleCommand: async () => ({ success: false }),
      getProjectSettings: async () => ({})
    },
    assetResources: { list: async () => ({}) },
    automationBridge: {
      isConnected: () => false,
      sendAutomationRequest: async () => ({ success: true }),
      isCapabilityTokenConfigured: () => tokenConfigured
    }
  } as unknown as ITools;

  return {
    tools,
    logger: new Logger('fail-closed', 'error'),
    elicitationTimeoutMs: 0,
    ensureConnected: async () => false
  };
}

describe('Task 40 fail-closed offline default', () => {
  it('refuses the offline get_project_settings path when a capability token is configured', async () => {
    const result = await handleUnrealGatewayCall(
      { operation: 'execute', tool: 'system_control', action: 'get_project_settings', params: {} },
      makeContext(true)
    );
    expect(result.success).toBe(false);
    expect(result.errorCode).toBe('NOT_CONNECTED');
  });

  it('still allows the offline get_project_settings path when no token is configured', async () => {
    const result = await handleUnrealGatewayCall(
      { operation: 'execute', tool: 'system_control', action: 'get_project_settings', params: {} },
      makeContext(false)
    );
    expect(result.errorCode).not.toBe('NOT_CONNECTED');
  });
});
