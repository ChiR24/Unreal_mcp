import { describe, expect, it } from 'vitest';

import { Logger } from '../../../src/utils/logging/logger.js';
import type { ITools } from '../../../src/types/tools/tool-interfaces.js';
import { handleUnrealGatewayCall, type GatewayContext } from '../../../src/server/tool-registry-gateway.js';

function makeContext(): GatewayContext {
  const tools = {
    automationBridge: {
      isConnected: () => false,
      sendAutomationRequest: async () => ({ success: true }),
    }
  } as unknown as ITools;

  return {
    tools,
    logger: new Logger('fail-closed', 'error'),
    ensureConnected: async () => false
  };
}

describe('Task 40 fail-closed offline default', () => {
  it('refuses execute with NOT_CONNECTED while the editor is unreachable', async () => {
    const result = await handleUnrealGatewayCall(
      { operation: 'execute', tool: 'system_control', action: 'get_project_settings', params: {} },
      makeContext()
    );
    expect(result.success).toBe(false);
    expect(result.errorCode).toBe('NOT_CONNECTED');
  });
});
