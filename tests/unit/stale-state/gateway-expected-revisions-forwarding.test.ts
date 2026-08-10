import { afterEach, describe, expect, it, vi } from 'vitest';

import { getGatewayExpectedRevisions } from '../../../src/automation/gateway-expected-revisions-context.js';
import type { GatewayContext } from '../../../src/server/tool-registry-gateway.js';
import { handleUnrealGatewayCall } from '../../../src/server/tool-registry-gateway.js';
import type { ExpectedRevisions } from '../../../src/tools/catalog/capabilities/semantic/execution-options.js';
import { dynamicToolManager } from '../../../src/tools/dynamic/dynamic-tool-manager.js';
import type { ITools } from '../../../src/types/tools/tool-interfaces.js';
import { Logger } from '../../../src/utils/logging/logger.js';

const observed: Array<ExpectedRevisions | undefined> = [];

vi.mock('../../../src/tools/orchestration/consolidated-tool-handlers.js', () => ({
  handleConsolidatedToolCall: vi.fn(async () => {
    observed.push(getGatewayExpectedRevisions());
    return { success: true, message: 'ok' };
  })
}));

function context(): GatewayContext {
  const tools: ITools = {
    systemTools: {
      executeConsoleCommand: async () => ({ success: true }),
      getProjectSettings: async () => ({})
    },
    assetResources: { list: async () => ({}) }
  };
  return {
    tools,
    logger: new Logger('task-42-expected-revisions', 'error'),
    elicitationTimeoutMs: 0,
    ensureConnected: async () => true
  };
}

afterEach(() => {
  observed.length = 0;
  dynamicToolManager.reset();
  vi.clearAllMocks();
});

describe('gateway expected-revisions context', () => {
  it('activates validated pins only while dispatching the selected capability', async () => {
    const result = await handleUnrealGatewayCall({
      operation: 'execute',
      capability: 'asset.list',
      params: {},
      options: { expectedRevisions: { selection: 7, package: 11 } }
    }, context());

    expect(result.success).toBe(true);
    expect(observed).toEqual([{ selection: 7, package: 11 }]);
  });

  it('does not leak pins into a later unpinned gateway dispatch', async () => {
    await handleUnrealGatewayCall({
      operation: 'execute',
      capability: 'asset.list',
      params: {},
      options: { expectedRevisions: { level: 3 } }
    }, context());
    await handleUnrealGatewayCall({
      operation: 'execute',
      capability: 'asset.list',
      params: {}
    }, context());

    expect(observed).toEqual([{ level: 3 }, undefined]);
  });
});
