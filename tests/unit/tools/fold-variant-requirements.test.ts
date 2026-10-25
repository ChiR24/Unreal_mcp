// A folded family's union schema keeps only what every variant requires, so 592 variants lost a
// requirement their own record declared: debug_shape without location passed the gateway and failed in
// the engine as an execution error, and describe called the location optional. routing.dispatchBy.requires
// keeps each dispatched action's own requirements; both doors refuse before dispatch, and describe and the
// search signature show them.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';
import { describe, expect, it, vi } from 'vitest';

import { Logger } from '../../../src/utils/logging/logger.js';
import type { ITools } from '../../../src/types/tools/tool-interfaces.js';
import type { GatewayContext } from '../../../src/server/tool-registry-gateway.js';
import { handleUnrealGatewayCall } from '../../../src/server/tool-registry-gateway.js';
import { capabilityIndex } from '../../../src/server/gateway/gateway-capability-index.js';
import { capabilitySignature } from '../../../src/server/gateway/gateway-capability-view.js';
import { missingVariantRequirement, variantRequired } from '../../../src/server/gateway/gateway-dispatch-by.js';
import { describeGatewayCapability } from '../../../src/server/gateway/gateway-describe.js';

const record = (id: string) => {
  const found = capabilityIndex().byId.get(id);
  if (found === undefined) throw new Error(`${id} is not in the catalogue`);
  return found;
};

const dispatched: string[] = [];
const context = (): GatewayContext => {
  const tools: ITools = {
    automationBridge: {
      isConnected: () => true,
      sendAutomationRequest: vi.fn(async (_tool: string, payload: Record<string, unknown>) => {
        dispatched.push(String(payload.action));
        return { success: true, message: 'ok' };
      })
    }
  };
  return { tools, logger: new Logger('fold-variant-requirements', 'error'), ensureConnected: async () => true };
};

const PRIVATE = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'MCP');
const native = (...segments: readonly string[]): string => readFileSync(join(PRIVATE, ...segments), 'utf8');

describe('a folded variant is held to its own required parameters', () => {
  it('refuses debugOp=draw without location before anything is dispatched, and runs clear without it', async () => {
    dispatched.length = 0;
    const refused = await handleUnrealGatewayCall({ operation: 'execute', tool: 'manage_effect', action: 'debug_shape', params: { shapeType: 'sphere' } }, context());
    expect(refused.errorCode).toBe('MISSING_REQUIRED_PARAMETER');
    expect(refused.pointer).toBe('/location');
    expect(String(refused.message)).toContain("Missing required parameter 'location' when debugOp is draw");
    expect(dispatched).toEqual([]);

    const cleared = await handleUnrealGatewayCall({ operation: 'execute', tool: 'manage_effect', action: 'debug_shape', params: { debugOp: 'clear' } }, context());
    expect(cleared.errorCode).toBeUndefined();
    expect(dispatched).toEqual(['clear_debug_shapes']);
  });

  it('an old name pins its variant, so its own requirements apply too', async () => {
    dispatched.length = 0;
    const refused = await handleUnrealGatewayCall({ operation: 'execute', tool: 'manage_blueprint', action: 'set_node_property', params: { blueprintPath: '/Game/BP', propertyName: 'X', propertyValue: 1 } }, context());
    expect(refused.errorCode).toBe('MISSING_REQUIRED_ONEOF');
    expect(String(refused.message)).toContain('At least one of [nodeId, nodeGuid] must be provided when edit is set_node_property');
    expect(dispatched).toEqual([]);
  });

  it('names every missing parameter at once', () => {
    expect(missingVariantRequirement(record('animation_physics.edit_skeleton'), 'add_bone', { edit: 'add_bone' })).toEqual({
      reason: 'missing-required',
      pointer: '/skeletonPath',
      message: 'Missing required parameters when edit is add_bone: skeletonPath, boneName'
    });
  });

  it('describe marks the parameter requiredFor its variants and the selector carries variantRequired; so does the signature', () => {
    const contract = describeGatewayCapability({ operation: 'describe', capability: 'manage_effect.debug_shape' });
    const rows = contract.parameters as Array<Record<string, unknown>>;
    expect(rows.find((row) => row.name === 'location')).toMatchObject({ required: false, requiredFor: ['draw'] });
    expect(rows.find((row) => row.name === 'debugOp')?.variantRequired).toEqual({ draw: ['location'] });
    expect(rows.find((row) => row.name === 'radius')).not.toHaveProperty('requiredFor');
    expect(capabilitySignature(record('manage_effect.debug_shape')).variantRequired).toEqual({ draw: ['location'] });
    expect(variantRequired(record('blueprint.inspect_graph'))?.node).toEqual(['blueprintPath', 'nodeId|nodeGuid']);
    // spawn_blueprint requires blueprintPath and names it as its one-of group too; it is listed once.
    expect(variantRequired(record('control_actor.spawn'))?.blueprint).toEqual(['blueprintPath']);
    const one = describeGatewayCapability({ operation: 'describe', capability: 'manage_effect.debug_shape', param: 'location' });
    expect(one.requiredFor).toEqual(['draw']);
  });

  it('the native door enforces and shows the same data with the same text', () => {
    const rules = native('Execute', 'Request', 'McpFoldVariantRules.cpp');
    expect(rules).toContain('TEXT("Missing required parameter \'%s\' %s")');
    expect(rules).toContain('TEXT("Missing required parameters %s: %s")');
    expect(rules).toContain('TEXT("At least one of [%s] must be provided %s")');
    expect(rules).toContain('TEXT("when %s is %s")');
    expect(rules).toContain('TEXT("variantRequired")');
    expect(rules).toContain('TEXT("requiredFor")');
    expect(native('Execute', 'McpNativeGatewayValidation.cpp')).toMatch(
      /McpValidateObjectAgainstCanonicalSchema\(ToValidate, InputSchema, Violation\)\s*\|\| !McpCheckVariantRequirements\(/u);
    expect(native('Gateway', 'McpNativeGatewayFolding.cpp')).toMatch(/TryGetObjectField\(TEXT\("requires"\), Requires\)/u);
    expect(native('Gateway', 'McpNativeGatewayDescribe.cpp')).toContain('McpSetVariantRequirementViews(Out, Record, Name);');
    expect(native('Gateway', 'McpNativeGatewaySearch.cpp')).toContain('Signature->SetObjectField(TEXT("variantRequired"), VariantRequired);');
  });
});
