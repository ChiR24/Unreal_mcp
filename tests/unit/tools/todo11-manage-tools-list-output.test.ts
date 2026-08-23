import { describe, expect, it } from 'vitest';

import { handleUnrealGatewayCall } from '../../../src/server/tool-registry-gateway.js';
import { gatewayContext } from './support/gateway-context-fixture.js';

const context = gatewayContext(
  {
    isConnected: () => true,
    sendAutomationRequest: async () => ({ success: true }),
    isCapabilityTokenConfigured: async () => false,
    getAuthority: () => ({ scopes: ['read', 'write', 'destructive', 'admin'] })
  },
  'todo11-manage-tools'
);

function expectSchemaTruthfulList(payload: Record<string, unknown>): void {
  expect(payload.totalTools).toBe(23);
  expect(payload.enabledCount).toBe(23);
  expect(payload.disabledCount).toBe(0);
  const tools = payload.tools as Array<Record<string, unknown>>;
  expect(tools).toHaveLength(23);
  for (const tool of tools) {
    expect(Object.keys(tool).sort()).toEqual(['category', 'enabled', 'name']);
  }
}

describe('Todo 11 manage_tools list_tools output contract', () => {
  it('returns schema-valid counts through execute', async () => {
    // Given a connected gateway using the canonical manage_tools handler
    // When list_tools is executed through the public gateway module
    const result = await handleUnrealGatewayCall(
      { operation: 'execute', tool: 'manage_tools', action: 'list_tools', params: {} },
      context
    );

    // Then the output matches the closed canonical schema and passed validation
    expect(result.success, JSON.stringify(result)).toBe(true);
    expectSchemaTruthfulList(result.data as Record<string, unknown>);
    expect((result.receipt as Record<string, unknown>).validation).toEqual(
      expect.objectContaining({ outputSchema: 'passed' })
    );
  });

  it('returns the same schema-valid shape through configure', async () => {
    // Given the local configure path
    // When list_tools is requested
    const result = await handleUnrealGatewayCall(
      { operation: 'configure', tool: 'manage_tools', action: 'list_tools' },
      context
    );

    // Then its output matches the same closed schema
    expect(result.success, JSON.stringify(result)).toBe(true);
    expectSchemaTruthfulList(result.result as Record<string, unknown>);
  });

  it('reports identical counts through configure and execute', async () => {
    // Given both public module paths
    // When both list_tools forms are called
    const [configured, executed] = await Promise.all([
      handleUnrealGatewayCall(
        { operation: 'configure', tool: 'manage_tools', action: 'list_tools' },
        context
      ),
      handleUnrealGatewayCall(
        { operation: 'execute', tool: 'manage_tools', action: 'list_tools', params: {} },
        context
      )
    ]);

    // Then their count fields agree exactly
    const configuredPayload = configured.result as Record<string, unknown>;
    const executedPayload = executed.data as Record<string, unknown>;
    expect({
      totalTools: configuredPayload.totalTools,
      enabledCount: configuredPayload.enabledCount,
      disabledCount: configuredPayload.disabledCount
    }).toEqual({
      totalTools: executedPayload.totalTools,
      enabledCount: executedPayload.enabledCount,
      disabledCount: executedPayload.disabledCount
    });
  });

  it('preserves the exact-action boundary', async () => {
    // Given an action smuggled into params
    // When execute receives no top-level action
    const result = await handleUnrealGatewayCall(
      {
        operation: 'execute',
        tool: 'manage_tools',
        action: 'list_tools',
        params: { action: 'list_tools' }
      },
      context
    );

    // Then it refuses the malformed path before routing
    expect(result.success).toBe(false);
    expect(result.errorCode).toBe('INVALID_PARAMS');
  });
});
