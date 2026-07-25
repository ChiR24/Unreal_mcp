import { describe, expect, it } from 'vitest';

import { handleUnrealGatewayCall, type GatewayContext } from '../../../src/server/tool-registry-gateway.js';
import { gatewayContext } from './support/gateway-context-fixture.js';

// Task 40 Blocker 5 regression: the scope fail-fast is exercised END TO END
// through the real gateway entry point, not by calling the predicate directly.
// Deleting the checkScopeAuthorization call in gateway-execute.ts makes these
// fail, which is exactly what the predicate-only tests could not do.

interface DispatchLog {
  readonly calls: string[];
}

// The typed authorization payload lives on the receipt, mirroring the plugin's
// FMcpAuthorizationDecision fields, not at the envelope's top level.
function receiptError(result: Record<string, unknown>): Record<string, unknown> {
  const receipt = result.receipt as Record<string, unknown> | undefined;
  return (receipt?.error as Record<string, unknown> | undefined) ?? {};
}

function makeContext(scopes: readonly string[] | undefined, log: DispatchLog): GatewayContext {
  return gatewayContext(
    {
      isConnected: () => true,
      sendAutomationRequest: async (action: string) => {
        log.calls.push(action);
        return { success: true };
      },
      isCapabilityTokenConfigured: () => false,
      getAuthority: () => (scopes === undefined ? undefined : { scopes: [...scopes] })
    },
    'scope-enforcement'
  );
}

const destructiveCall = {
  operation: 'execute',
  tool: 'manage_asset',
  action: 'delete_asset',
  params: { assetPath: '/Game/MCPTest/Disposable' },
  consent: { capability: 'asset.delete_asset', acknowledge: 'elevated' }
} as const;

describe('Task 40 — scope enforcement through the real `unreal` gateway call', () => {
  it('refuses a destructive action for a read-only principal and never dispatches', async () => {
    const log: DispatchLog = { calls: [] };
    const result = await handleUnrealGatewayCall({ ...destructiveCall }, makeContext(['read'], log));

    expect(result.success).toBe(false);
    expect(result.errorCode).toBe('SCOPE_NOT_GRANTED');
    expect(receiptError(result).requiredScope).toBe('destructive');
    expect(log.calls, 'a refused call must never reach the bridge').toEqual([]);
  });

  it('positive control: the identical call succeeds for an admin principal', async () => {
    const log: DispatchLog = { calls: [] };
    const result = await handleUnrealGatewayCall({ ...destructiveCall }, makeContext(['admin'], log));

    expect(result.errorCode).toBeUndefined();
    expect(result.success).toBe(true);
    expect(log.calls.length).toBe(1);
  });

  it('write does not imply destructive (exact-set, not rank-based)', async () => {
    const log: DispatchLog = { calls: [] };
    const result = await handleUnrealGatewayCall(
      { ...destructiveCall },
      makeContext(['read', 'write'], log)
    );

    expect(result.errorCode).toBe('SCOPE_NOT_GRANTED');
    expect(receiptError(result).grantedScopes).toEqual(['read', 'write']);
    expect(log.calls).toEqual([]);
  });

  it('an unrecognised advertised scope grants nothing', async () => {
    const log: DispatchLog = { calls: [] };
    const result = await handleUnrealGatewayCall(
      { ...destructiveCall },
      makeContext(['superuser'], log)
    );

    expect(result.errorCode).toBe('SCOPE_NOT_GRANTED');
    expect(log.calls).toEqual([]);
  });

  it('a read-scoped principal may still run a read action (refusal is not blanket)', async () => {
    const log: DispatchLog = { calls: [] };
    const result = await handleUnrealGatewayCall(
      { operation: 'execute', tool: 'control_actor', action: 'find_by_class', params: { className: 'PointLight' } },
      makeContext(['read'], log)
    );

    expect(result.errorCode).not.toBe('SCOPE_NOT_GRANTED');
    expect(log.calls.length).toBe(1);
  });

  it('no refusal payload ever carries a token', async () => {
    const log: DispatchLog = { calls: [] };
    const result = await handleUnrealGatewayCall({ ...destructiveCall }, makeContext(['read'], log));
    expect(JSON.stringify(result).toLowerCase()).not.toContain('token');
  });
});
