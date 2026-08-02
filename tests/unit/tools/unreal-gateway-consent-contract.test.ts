import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import Ajv from 'ajv';
import { describe, expect, it } from 'vitest';

import { handleUnrealGatewayCall, type GatewayContext } from '../../../src/server/tool-registry-gateway.js';
import { unrealGatewayToolDefinition } from '../../../src/tools/catalog/unreal-gateway-definition.js';
import { gatewayContext } from './support/gateway-context-fixture.js';
import { EXECUTION_OPTION_KEYS } from '../../../src/tools/catalog/capabilities/semantic/execution-options.js';

// Task 40 Blocker 3 regression: consent must have a LEGAL input channel on the
// only publicly advertised tool. `gateway-execute.ts` reads `args.consent`, so
// the declared `unreal` contract has to accept it — otherwise every capability
// carrying an explicit/elevated consent policy is unreachable through the
// advertised contract and the default no-token loopback configuration regresses.

const nativeGatewayDefinitionPath = resolve(
  process.cwd(),
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Gateway/McpNativeGatewayDefinition.cpp'
);

// A real destructive capability whose policy is `consent: 'elevated'`.
const CAPABILITY_ID = 'asset.delete_asset';

function validateGatewayArgs(args: unknown): { valid: boolean; errors: string } {
  const ajv = new Ajv({ strict: false, allErrors: true });
  const validate = ajv.compile(unrealGatewayToolDefinition.inputSchema);
  const valid = validate(args) === true;
  return { valid, errors: JSON.stringify(validate.errors ?? []) };
}

function makeContext(scopes: readonly string[] | undefined): GatewayContext {
  return gatewayContext(
    {
      isConnected: () => true,
      sendAutomationRequest: async () => ({ success: true }),
      isCapabilityTokenConfigured: async () => false,
      getAuthority: () => (scopes === undefined ? undefined : { scopes: [...scopes] })
    },
    'consent-contract'
  );
}

describe('Task 40 — consent has a legal input channel on the `unreal` tool', () => {
  it('the declared inputSchema accepts a consent-carrying execute call', () => {
    const { valid, errors } = validateGatewayArgs({
      operation: 'execute',
      tool: 'manage_asset',
      action: 'delete_asset',
      params: { assetPath: '/Game/MCPTest/Disposable' },
      consent: { capability: CAPABILITY_ID, acknowledge: 'elevated' }
    });
    expect(valid, `consent rejected by the declared gateway contract: ${errors}`).toBe(true);
  });

  it('the declared inputSchema still rejects an undeclared sibling (additionalProperties stays closed)', () => {
    const { valid } = validateGatewayArgs({ operation: 'search', notAThing: 1 });
    expect(valid).toBe(false);
  });

  it('the declared inputSchema rejects a malformed consent grant', () => {
    expect(validateGatewayArgs({
      operation: 'execute',
      tool: 'manage_asset',
      action: 'delete_asset',
      consent: { capability: CAPABILITY_ID, acknowledge: 'sure' }
    }).valid).toBe(false);
    expect(validateGatewayArgs({
      operation: 'execute',
      tool: 'manage_asset',
      action: 'delete_asset',
      consent: { capability: CAPABILITY_ID }
    }).valid).toBe(false);
  });

  it('consent is the envelope sibling, NOT an execution option', () => {
    expect(EXECUTION_OPTION_KEYS).not.toContain('consent');
  });

  it('the native /mcp gateway declares the same consent sibling', () => {
    const cpp = readFileSync(nativeGatewayDefinitionPath, 'utf8');
    // Anchored on the DECLARATION. A bare /TEXT\("consent"\)/ also matched an
    // unrelated TryGetObjectField read, so deleting the schema property left
    // this test green while the sibling became undeclarable.
    expect(
      cpp.includes('.Object(TEXT("consent")'),
      'native gateway definition must DECLARE a consent property on the schema'
    ).toBe(true);
  });
});

describe('`params` is declared as an open object map, not an underspecified object', () => {
  const paramsSchema = (unrealGatewayToolDefinition.inputSchema as {
    properties: Record<string, Record<string, unknown>>;
  }).properties.params;

  it('declares an object that explicitly admits arbitrary action-specific keys', () => {
    expect(paramsSchema.type).toBe('object');
    expect(
      paramsSchema.additionalProperties,
      'params carries per-action keys the gateway cannot enumerate, so the schema must say so explicitly'
    ).toBe(true);
  });

  it('accepts a nested object value under a key no gateway schema enumerates', () => {
    const { valid, errors } = validateGatewayArgs({
      operation: 'execute',
      tool: 'control_actor',
      action: 'spawn',
      params: { classPath: '/Script/Engine.StaticMeshActor', location: { x: 0, y: 0, z: 500 } }
    });
    expect(valid, `declared contract rejected a normal execute payload: ${errors}`).toBe(true);
  });

  it('the native /mcp gateway declares params open too, or the surfaces disagree', () => {
    const cpp = readFileSync(nativeGatewayDefinitionPath, 'utf8');
    expect(cpp.includes('.Object(TEXT("params")')).toBe(true);
    const paramsBlock = cpp.slice(cpp.indexOf('TryGetObjectField(TEXT("params")'));
    expect(
      /ParamsProp\)->SetBoolField\(TEXT\("additionalProperties"\), true\)/.test(paramsBlock),
      'native params must publish additionalProperties: true like the TypeScript surface'
    ).toBe(true);
  });
});

describe('Task 40 — consent driven through the real `unreal` tool arguments', () => {
  it('refuses a consent-requiring action when no grant is supplied (positive control)', async () => {
    const result = await handleUnrealGatewayCall(
      { operation: 'execute', tool: 'manage_asset', action: 'delete_asset', params: { assetPath: '/Game/X' } },
      makeContext(['admin'])
    );
    expect(result.success).toBe(false);
    expect(result.errorCode).toBe('CONSENT_REQUIRED');
  });

  it('accepts the same call when the grant names the exact capability', async () => {
    const result = await handleUnrealGatewayCall(
      {
        operation: 'execute',
        tool: 'manage_asset',
        action: 'delete_asset',
        params: { assetPath: '/Game/X' },
        consent: { capability: CAPABILITY_ID, acknowledge: 'elevated' }
      },
      makeContext(['admin'])
    );
    expect(result.errorCode).toBeUndefined();
    expect(result.success).toBe(true);
  });

  it('still refuses when the grant names a different capability', async () => {
    const result = await handleUnrealGatewayCall(
      {
        operation: 'execute',
        tool: 'manage_asset',
        action: 'delete_asset',
        params: { assetPath: '/Game/X' },
        consent: { capability: 'asset.rename_asset', acknowledge: 'elevated' }
      },
      makeContext(['admin'])
    );
    expect(result.errorCode).toBe('CONSENT_REQUIRED');
  });
});

describe('Task 40 — the required grant is discoverable BEFORE the first refusal', () => {
  it('describe surfaces the exact grant a consent-requiring capability needs', async () => {
    const described = await handleUnrealGatewayCall(
      { operation: 'describe', tool: 'manage_asset', action: 'delete_asset' },
      makeContext(['admin'])
    );
    expect(described.capability).toBe(CAPABILITY_ID);
    expect(described.consentGrant).toEqual({ capability: CAPABILITY_ID, acknowledge: 'elevated' });
  });

  it('describe omits the grant for a capability that needs no consent', async () => {
    const described = await handleUnrealGatewayCall(
      { operation: 'describe', tool: 'manage_asset', action: 'list_assets' },
      makeContext(['admin'])
    );
    expect(described.consentGrant).toBeUndefined();
  });

  it('describe nextCall composed with consentGrant is a valid, accepted gateway call', async () => {
    const described = await handleUnrealGatewayCall(
      { operation: 'describe', tool: 'manage_asset', action: 'delete_asset' },
      makeContext(['admin'])
    );
    const composed = {
      ...(described.nextCall as Record<string, unknown>),
      consent: described.consentGrant
    };
    expect(validateGatewayArgs(composed).valid).toBe(true);

    // The grant read from describe is the one the gate accepts: whatever else
    // an empty-params call does, it is never turned away for consent.
    const executed = await handleUnrealGatewayCall(composed, makeContext(['admin']));
    expect(executed.errorCode).not.toBe('CONSENT_REQUIRED');
    expect(executed.errorCode).not.toBe('INVALID_CONSENT');
  });
});
