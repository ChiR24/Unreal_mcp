// ACTOR_NOT_FOUND came back from 26 handlers as a bare "Actor not found" with no
// next step, so a caller that typed "Bug1" for Bug_01 had nothing to try. The
// gateway knows the actorName it sent, and control_actor find by name answers a
// miss with the labels it resembles (similar), so both doors hand that call back.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';
import { describe, expect, it } from 'vitest';

import { Logger } from '../../../src/utils/logging/logger.js';
import type { GatewayContext } from '../../../src/server/tool-registry-gateway.js';
import type { ITools } from '../../../src/types/tools/tool-interfaces.js';
import { handleUnrealGatewayCall } from '../../../src/server/tool-registry-gateway.js';

function makeContext(handlerResult: unknown): GatewayContext {
  const tools = {
    automationBridge: {
      isConnected: () => true,
      sendAutomationRequest: async () => handlerResult
    }
  } as unknown as ITools;
  return { tools, logger: new Logger('actor-not-found', 'error'), ensureConnected: async () => true };
}

const getTransform = async (handlerResult: unknown): Promise<Record<string, unknown>> =>
  (await handleUnrealGatewayCall(
    { operation: 'execute', capability: 'control_actor.get_transform', params: { actorName: 'Bug1' } },
    makeContext(handlerResult)
  )) as Record<string, unknown>;

const transport = (file: string): string =>
  readFileSync(
    join('plugins', 'McpAutomationBridge', 'Source', 'McpAutomationBridge', 'Private', 'MCP', 'Transport', file),
    'utf8'
  ).replace(/\/\/[^\n]*/gu, ' ');

describe('ACTOR_NOT_FOUND hands back a find by the name that missed', () => {
  it('TS: the refusal carries control_actor find by name as its nextCall', async () => {
    const result = await getTransform({ success: false, error: 'ACTOR_NOT_FOUND', message: 'Actor not found' });

    expect(result.errorCode).toBe('ACTOR_NOT_FOUND');
    expect(result.nextCall).toEqual({
      operation: 'execute', tool: 'control_actor', action: 'find', params: { findBy: 'name', name: 'Bug1' }
    });
    expect(String((result.suggestions as string[])[0])).toContain("'Bug1'");
  });

  it('TS: any other handler code keeps its own guidance', async () => {
    const result = await getTransform({ success: false, error: 'COMPONENT_NOT_FOUND', message: 'Component missing' });

    expect(result.errorCode).toBe('COMPONENT_NOT_FOUND');
    expect(result.nextCall).toBeUndefined();
  });

  it('native: the completion keeps the call arguments and adds the same nextCall', () => {
    expect(transport('McpNativeTransportGatewayStream.cpp')).toContain('Conn->Arguments = Arguments;');
    const pending = transport('McpNativeTransportPendingRequests.cpp');
    // After the handler's own nextCall, so ACTOR_NOT_FOUND keeps the find call its suggestion names (as over stdio).
    expect(pending).toMatch(/McpBuildGatewayExecuteReceipt\([^;]*;\s*if \(!bSuccess\)\s*\{\s*AddHandlerNextCall\(ReportedResult, Result\);\s*\}\s*AddActorNotFoundGuidance\(ReportedResult, Conn->Arguments\);/u);
    for (const token of ['TEXT("ACTOR_NOT_FOUND")', 'TEXT("actorName")', 'TEXT("findBy"), TEXT("name")', 'TEXT("action"), TEXT("find")']) {
      expect(pending).toContain(token);
    }
  });
});
