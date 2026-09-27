import { afterEach, describe, expect, it } from 'vitest';

import type { GatewayContext } from '../../../src/server/tool-registry-gateway.js';
import { handleUnrealGatewayCall } from '../../../src/server/tool-registry-gateway.js';
import { dynamicToolManager } from '../../../src/tools/dynamic/dynamic-tool-manager.js';
import { Logger } from '../../../src/utils/logging/logger.js';

const observed: unknown[] = [];

function context(): GatewayContext {
  return {
    tools: {
      automationBridge: {
        isConnected: () => true,
        sendAutomationRequest: async (_action, _payload, options) => {
          observed.push(options?.expectedRevisions);
          return { success: true, message: 'ok' };
        }
      }
    },
    logger: new Logger('expected-revisions-forwarding', 'error'),
    ensureConnected: async () => true
  };
}

afterEach(() => {
  observed.length = 0;
  dynamicToolManager.reset();
});

describe('gateway expected-revisions forwarding', () => {
  it('forwards validated pins with the dispatch of the selected capability', async () => {
    const result = await handleUnrealGatewayCall({
      operation: 'execute',
      capability: 'asset.list',
      params: {},
      options: { expectedRevisions: { selection: 7, package: 11 } }
    }, context());

    expect(result.success).toBe(true);
    expect(observed).toEqual([{ selection: 7, package: 11 }]);
  });

  it('does not carry pins into a later unpinned dispatch', async () => {
    await handleUnrealGatewayCall({ operation: 'execute', capability: 'asset.list', params: {}, options: { expectedRevisions: { level: 3 } } }, context());
    await handleUnrealGatewayCall({ operation: 'execute', capability: 'asset.list', params: {} }, context());

    expect(observed).toEqual([{ level: 3 }, undefined]);
  });
});
