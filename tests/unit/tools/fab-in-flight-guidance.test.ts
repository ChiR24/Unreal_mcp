// The Fab add turns an add away when the queue is full. QUEUE_FULL named nothing, so a caller
// could not tell which listing was in the way or how to find out when it would finish. The handler now
// returns the call that reads the running import, and both doors hand it back as the refusal's nextCall.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';
import { describe, expect, it } from 'vitest';

import { Logger } from '../../../src/utils/logging/logger.js';
import type { GatewayContext } from '../../../src/server/tool-registry-gateway.js';
import type { ITools } from '../../../src/types/tools/tool-interfaces.js';
import { handleUnrealGatewayCall } from '../../../src/server/tool-registry-gateway.js';
import { handlerNextCall } from '../../../src/server/gateway/gateway-execute-dispatch.js';

const STATUS_CALL = {
  operation: 'execute',
  tool: 'manage_asset',
  action: 'query_marketplace',
  params: { lookup: 'fab_import_status', operationId: 'fab-3f9a1c2e40' }
};

function makeContext(handlerResult: unknown): GatewayContext {
  const tools = {
    automationBridge: { isConnected: () => true, sendAutomationRequest: async () => handlerResult }
  } as unknown as ITools;
  return { tools, logger: new Logger('fab-in-flight', 'error'), ensureConnected: async () => true };
}

const addListing = async (handlerResult: unknown): Promise<Record<string, unknown>> =>
  (await handleUnrealGatewayCall(
    {
      operation: 'execute',
      capability: 'asset.import_marketplace_asset',
      params: { marketplace: 'fab_listing', listingId: 'abc123' },
      consent: { capability: 'asset.import_marketplace_asset', acknowledge: 'explicit' }
    },
    makeContext(handlerResult)
  )) as Record<string, unknown>;

describe('a refusal that names the call which settles it', () => {
  it('TS: passes the handler nextCall through from the bridge frame', async () => {
    const result = await addListing({
      type: 'automation_response',
      success: false,
      error: 'QUEUE_FULL',
      message: 'Fab is still importing listing xyz',
      result: { listingId: 'abc123', operationId: 'fab-3f9a1c2e40', nextCall: STATUS_CALL }
    });

    expect(result.errorCode).toBe('QUEUE_FULL');
    expect(result.nextCall).toEqual(STATUS_CALL);
  });

  it('TS: reads it from a flat handler result too', async () => {
    const result = await addListing({ success: false, error: 'QUEUE_FULL', message: 'busy', nextCall: STATUS_CALL });

    expect(result.nextCall).toEqual(STATUS_CALL);
  });

  it('TS: drops anything that is not an executable call', () => {
    expect(handlerNextCall({ nextCall: { operation: 'delete', tool: 'a', action: 'b', params: {} } })).toEqual({});
    expect(handlerNextCall({ nextCall: { operation: 'execute', tool: 'a' } })).toEqual({});
    expect(handlerNextCall({ nextCall: { operation: 'execute', tool: 'a', action: 'b', params: 'x' } })).toEqual({});
    expect(handlerNextCall('not a record')).toEqual({});
    expect(handlerNextCall({ result: { nextCall: STATUS_CALL } })).toEqual({ nextCall: STATUS_CALL });
  });

  it('TS: a refusal without one keeps its own guidance', async () => {
    const result = await addListing({ success: false, error: 'INVALID_LISTING_ID', message: 'bad id' });

    expect(result.errorCode).toBe('INVALID_LISTING_ID');
    expect(result.nextCall).toBeUndefined();
  });

  it('native: the completion copies an executable nextCall out of the handler result on a failure', () => {
    const pending = readFileSync(
      join('plugins', 'McpAutomationBridge', 'Source', 'McpAutomationBridge', 'Private', 'MCP', 'Transport', 'McpNativeTransportPendingRequests.cpp'),
      'utf8'
    ).replace(/\/\/[^\n]*/gu, ' ');
    expect(pending).toMatch(/if \(!bSuccess\)\s*\{\s*AddHandlerNextCall\(ReportedResult, Result\);/u);
    for (const token of ['TEXT("nextCall")', 'Operation != TEXT("execute")', 'TEXT("tool")', 'TEXT("action")', 'TEXT("params")']) {
      expect(pending).toContain(token);
    }
  });
});
