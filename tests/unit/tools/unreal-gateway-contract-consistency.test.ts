import { describe, expect, it } from 'vitest';

import { handleUnrealGatewayCall, type GatewayContext } from '../../../src/server/tool-registry-gateway.js';
import { describeGatewayCapability } from '../../../src/server/gateway/gateway-describe.js';
import { searchGatewayCapabilities } from '../../../src/server/gateway/gateway-search.js';
import { gatewayContext } from './support/gateway-context-fixture.js';

// Black-box certification pass 2 — gateway contract consistency.
//
// Every case here is a defect the certification sweep observed through the
// public `unreal` contract: an operation that accepted something it should have
// refused, silently rewrote an argument the caller supplied, or reported a
// number that did not describe what was actually returned. None of them need a
// live editor, because all four gateway operations resolve against the
// generated registry before any bridge call is attempted.

function makeContext(): GatewayContext {
  return gatewayContext(
    {
      isConnected: () => true,
      sendAutomationRequest: async () => ({ success: true }),
      isCapabilityTokenConfigured: async () => false,
      getAuthority: () => ({ scopes: ['Read', 'Write', 'Destructive', 'Admin'] })
    },
    'contract-consistency'
  );
}

describe('MCPBB-006 — configure applies the same admission checks as execute', () => {
  it('refuses a tool it cannot route to instead of ignoring the field', async () => {
    const result = await handleUnrealGatewayCall(
      { operation: 'configure', tool: 'not_a_tool', action: 'get_status' },
      makeContext()
    );
    expect(result.success).toBe(false);
    expect(result.errorCode).toBe('UNKNOWN_TOOL');
  });

  it('accepts the one tool configure does route to', async () => {
    const result = await handleUnrealGatewayCall(
      { operation: 'configure', tool: 'manage_tools', action: 'get_status' },
      makeContext()
    );
    expect(result.success).toBe(true);
  });

  it('refuses an action smuggled through params instead of silently preferring one', async () => {
    const result = await handleUnrealGatewayCall(
      { operation: 'configure', action: 'get_status', params: { action: 'list_tools' } },
      makeContext()
    );
    expect(result.success).toBe(false);
    expect(result.errorCode).toBe('INVALID_PARAMS');
  });

  it('refuses an unknown action with a typed code rather than a bare wrapped result', async () => {
    const result = await handleUnrealGatewayCall(
      { operation: 'configure', action: 'definitely_not_an_action' },
      makeContext()
    );
    expect(result.success).toBe(false);
    expect(result.errorCode).toBe('UNKNOWN_ACTION');
  });
});

describe('MCPBB-003 — every gateway refusal carries a machine-readable code', () => {
  it('a failed configure envelope names its own error code at the top level', async () => {
    const result = await handleUnrealGatewayCall(
      { operation: 'configure', action: 'definitely_not_an_action' },
      makeContext()
    );
    expect(result.success).toBe(false);
    expect(typeof result.errorCode).toBe('string');
    expect((result.errorCode as string).length).toBeGreaterThan(0);
  });
});

describe('MCPBB-007 — describe refuses a parameter selector it cannot resolve', () => {
  it('refuses `param` when no action narrows it to a single capability', () => {
    const result = describeGatewayCapability({ tool: 'manage_tools', param: 'action' });
    expect(result.success).toBe(false);
    expect(result.errorCode).toBe('MISSING_ACTION');
  });

  it('refuses `param` when nothing at all narrows it', () => {
    const result = describeGatewayCapability({ param: 'action' });
    expect(result.success).toBe(false);
    expect(result.errorCode).toBe('MISSING_ACTION');
  });

  // `action` itself is deliberately absent from every described schema
  // (MCPBB-001), so the reachability case has to name a parameter the capability
  // actually declares.
  it('still resolves `param` once an action is supplied', () => {
    const result = describeGatewayCapability({ tool: 'manage_asset', action: 'delete_asset', param: 'assetPath' });
    expect(result.success).toBe(true);
  });

  it('leaves the plain tool summary reachable', () => {
    const result = describeGatewayCapability({ tool: 'manage_tools' });
    expect(result.success).toBe(true);
    expect(result.scope).toBe('tool');
  });
});

describe('MCPBB-004 — pagination coercions are disclosed, not silent', () => {
  it('reports that limit=0 was raised to the minimum', () => {
    const result = searchGatewayCapabilities({ query: 'actor', limit: 0 });
    const coercions = result.coercions as Array<Record<string, unknown>> | undefined;
    expect(coercions, 'limit=0 was clamped with no disclosure').toBeDefined();
    const limitCoercion = (coercions ?? []).find((entry) => entry.parameter === 'limit');
    expect(limitCoercion).toBeDefined();
    expect(limitCoercion?.requested).toBe(0);
    expect(limitCoercion?.applied).toBe(1);
  });

  it('reports that a negative offset was raised to zero', () => {
    const result = searchGatewayCapabilities({ query: 'actor', offset: -1 });
    const coercions = result.coercions as Array<Record<string, unknown>> | undefined;
    expect(coercions, 'offset=-1 was clamped with no disclosure').toBeDefined();
    const offsetCoercion = (coercions ?? []).find((entry) => entry.parameter === 'offset');
    expect(offsetCoercion).toBeDefined();
    expect(offsetCoercion?.requested).toBe(-1);
    expect(offsetCoercion?.applied).toBe(0);
  });

  it('says nothing when every argument was honoured exactly', () => {
    const result = searchGatewayCapabilities({ query: 'actor', limit: 5, offset: 0 });
    expect(result.coercions).toBeUndefined();
  });
});

describe('MCPBB-072 — the page reports how many rows it actually served', () => {
  it('servedCount matches the rows in the response', () => {
    const result = searchGatewayCapabilities({ query: 'actor', limit: 5 });
    const rows = result.results as unknown[];
    expect(result.servedCount, 'no servedCount: `limit` echoes the request, not the page').toBe(rows.length);
  });

  it('servedCount tracks the surviving rows when the byte budget truncates the page', () => {
    const result = searchGatewayCapabilities({ query: 'actor', limit: 25, maxBytes: 4096 });
    const rows = result.results as unknown[];
    expect(result.servedCount).toBe(rows.length);
    expect(result.limit).toBe(25);
  });
});
