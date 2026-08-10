import { describe, expect, it } from 'vitest';

import { handleUnrealGatewayCall, type GatewayContext } from '../../src/server/tool-registry-gateway.js';
import { gatewayContext } from './tools/support/gateway-context-fixture.js';

function context(): GatewayContext {
  return gatewayContext(
    {
      isConnected: () => true,
      sendAutomationRequest: async () => ({ success: true }),
      isCapabilityTokenConfigured: async () => false,
      getAuthority: () => ({ scopes: ['admin'] })
    },
    'describe-execute-parity'
  );
}

const PARENTS = [
  'manage_audio',
  'manage_networking',
  'inspect',
  'system_control',
  'manage_level',
  'control_editor'
] as const;

async function listedActions(tool: string): Promise<readonly string[]> {
  const seen: string[] = [];
  for (let offset = 0; offset < 400; offset += 50) {
    const page = (await handleUnrealGatewayCall(
      { operation: 'describe', tool, limit: 50, offset },
      context()
    )) as { actions?: Array<string | { action?: string; name?: string }>; hasMore?: boolean };
    const rows = page.actions ?? [];
    for (const row of rows) {
      const name = typeof row === 'string' ? row : row.action ?? row.name;
      if (name !== undefined) seen.push(name);
    }
    if (page.hasMore !== true || rows.length === 0) break;
  }
  return seen;
}

describe('every action describe advertises can be reached by execute', () => {
  it.each(PARENTS)('%s advertises no action that execute rejects as unknown', async (tool) => {
    const actions = await listedActions(tool);
    expect(actions.length, `describe listed no actions for ${tool}`).toBeGreaterThan(0);

    const unreachable: string[] = [];
    for (const action of actions) {
      const result = (await handleUnrealGatewayCall(
        { operation: 'execute', tool, action, params: {} },
        context()
      )) as { errorCode?: string };
      // Any other outcome is fine: a missing required parameter, a refusal, or a
      // success all prove the route resolved. Only UNKNOWN_ACTION means describe
      // pointed at something execute cannot address at all.
      if (result.errorCode === 'UNKNOWN_ACTION') unreachable.push(`${tool}.${action}`);
    }

    expect(
      unreachable,
      `describe advertised ${unreachable.length}/${actions.length} actions on ${tool} that execute rejects as `
        + 'UNKNOWN_ACTION, so progressive discovery dead-ends'
    ).toEqual([]);
  });
});
