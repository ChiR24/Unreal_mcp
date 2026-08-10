// Task 26 — canonical execute validation and envelopes on the TypeScript surface.
//
// Written failing-first against the Task 24 seam (`src/server/gateway/gateway-execute.ts`),
// which at that point still carried the pre-extraction manifest-driven behavior.
//
// The stage order asserted here is the one the native `/mcp` surface implements
// (`tests/unit/gateway-discovery-suite/execute-reference.ts` is the shared normative spec):
//   form/alias -> availability -> params object -> reserved+control keys ->
//   options -> defaults -> exact per-action input schema -> connection ->
//   dispatch -> output schema -> receipt.
// Nothing reaches `handleConsolidatedToolCall` until every earlier stage passes.

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';

import { Logger } from '../../../src/utils/logging/logger.js';
import type { ITools } from '../../../src/types/tools/tool-interfaces.js';
import type { GatewayContext } from '../../../src/server/tool-registry-gateway.js';
import { handleUnrealGatewayCall } from '../../../src/server/tool-registry-gateway.js';
import { isRecord } from '../../../src/utils/validation/type-guards.js';
import { dynamicToolManager } from '../../../src/tools/dynamic/dynamic-tool-manager.js';
import {
  capabilityIndex,
  catalogRevision
} from '../../../src/server/gateway/gateway-capability-index.js';
import {
  buildExecuteTargetIndex,
  resolveExecuteTarget
} from '../../../src/server/gateway/gateway-execute-resolve.js';
import type { CapabilityRecord } from '../../../src/tools/catalog/capabilities/model.js';
import {
  CapabilityAliasSchema,
  CapabilityIdSchema
} from '../../../src/tools/catalog/capabilities/identifiers.js';
import {
  firstRecordWithVariant,
  minimalValidOutput,
  minimalValidParams
} from './support/capability-fixtures.js';

type Dispatch = { tool: string; args: Record<string, unknown> };

const dispatched: Dispatch[] = [];
let handlerResult: unknown = { success: true, message: 'ok' };

vi.mock('../../../src/tools/orchestration/consolidated-tool-handlers.js', () => ({
  handleConsolidatedToolCall: vi.fn(async (tool: string, args: Record<string, unknown>) => {
    dispatched.push({ tool, args });
    return handlerResult;
  })
}));

function makeContext(connected = true): GatewayContext {
  const tools: ITools = {
    systemTools: {
      executeConsoleCommand: async () => ({ success: false }),
      getProjectSettings: async () => ({})
    },
    assetResources: { list: async () => ({}) }
  };
  return {
    tools,
    logger: new Logger('gateway-execute-canonical', 'error'),
    elicitationTimeoutMs: 0,
    ensureConnected: async () => connected
  };
}

async function execute(args: Record<string, unknown>, connected = true): Promise<Record<string, unknown>> {
  return await handleUnrealGatewayCall({ operation: 'execute', ...args }, makeContext(connected));
}

function record(id: string): CapabilityRecord {
  const found = capabilityIndex().byId.get(id);
  if (!found) throw new Error(`fixture capability '${id}' is absent from the generated registry`);
  return found;
}

function legacyOf(id: string): { tool: string; action: string } {
  const first = record(id).legacyIds[0];
  if (!first) throw new Error(`capability '${id}' declares no legacy pair`);
  return { tool: first.tool, action: first.action };
}

function asRecord(value: unknown): Record<string, unknown> {
  if (!isRecord(value)) throw new Error(`expected an object, received ${String(value)}`);
  return value;
}

// The correlation id, external request id and wall-clock timing are minted per
// request, so two separate calls (the canonical and legacy forms of one
// capability) necessarily differ on them. Structural receipt equality across the
// forms is therefore asserted on the stable semantic content only.
function stableReceipt(value: unknown): Record<string, unknown> {
  const clone = { ...asRecord(value) };
  delete clone.correlationId;
  delete clone.requestId;
  delete clone.timingMs;
  return clone;
}

beforeEach(() => {
  handlerResult = { success: true, message: 'ok' };
});

afterEach(() => {
  dynamicToolManager.reset();
  dispatched.length = 0;
});

describe('execute: canonical v2 and generated legacy forms normalize to one dispatch', () => {
  it('accepts the v2 {capability, params} form and reports the canonical capability', async () => {
    const result = await execute({
      capability: 'asset.import',
      params: { sourcePath: '/tmp/a.fbx', destinationPath: '/Game/A' }
    });

    expect(result.success).toBe(true);
    expect(result.operation).toBe('execute');
    expect(result.capability).toBe('asset.import');
    expect(result.catalogRevision).toBe(catalogRevision());
    expect(dispatched).toHaveLength(1);
  });

  it('accepts the generated legacy {tool, action, params} form for the same capability', async () => {
    const legacy = legacyOf('asset.import');
    const result = await execute({
      tool: legacy.tool,
      action: legacy.action,
      params: { sourcePath: '/tmp/a.fbx', destinationPath: '/Game/A' }
    });

    expect(result.success).toBe(true);
    expect(result.capability).toBe('asset.import');
    expect(dispatched).toHaveLength(1);
  });

  it('produces an identical receipt for the canonical and legacy forms of one capability', async () => {
    const params = { sourcePath: '/tmp/a.fbx', destinationPath: '/Game/A' };
    const canonical = await execute({ capability: 'asset.import', params });
    const canonicalDispatch = dispatched.splice(0);

    const legacy = legacyOf('asset.import');
    const migrated = await execute({ tool: legacy.tool, action: legacy.action, params });
    const legacyDispatch = dispatched.splice(0);

    expect(stableReceipt(migrated.receipt)).toEqual(stableReceipt(canonical.receipt));
    expect(migrated.capability).toBe(canonical.capability);
    expect(legacyDispatch).toEqual(canonicalDispatch);
  });

  it('dispatches exactly once, through the parent tool, with canonical params only', async () => {
    await execute({
      capability: 'asset.import',
      params: { sourcePath: '/tmp/a.fbx', destinationPath: '/Game/A' },
      options: { timeoutMs: 1000 }
    });

    expect(dispatched).toHaveLength(1);
    const call = dispatched[0];
    expect(call.tool).toBe('manage_asset');
    expect(call.args.action).toBe('import');
    expect(call.args.subAction).toBe('import');
    // Gateway controls never leak into the action payload. Do not re-add
    // `preview` here: it is refused before dispatch (UNSUPPORTED_PREVIEW below).
    expect(call.args.options).toBeUndefined();
    expect(call.args.capability).toBeUndefined();
    expect(call.args.timeoutMs).toBeUndefined();
  });

  it('applies declared input defaults before dispatch', async () => {
    await execute({ capability: 'asset.list', params: {} });

    expect(dispatched).toHaveLength(1);
    // asset.list declares path="/Game" and limit=50 as generated defaults.
    expect(dispatched[0].args.path).toBe('/Game');
    expect(dispatched[0].args.limit).toBe(50);
  });
});

describe('execute: alias migration is resolved visibly', () => {
  it('resolves a declared alias to its canonical capability and reports the alias', async () => {
    const owner = capabilityIndex().records.find((entry) => entry.aliases.length > 0);
    if (owner === undefined) throw new Error('the generated catalog declares no aliases');
    const alias = owner.aliases[0];
    handlerResult = minimalValidOutput(owner);

    const result = await execute({ capability: alias, params: minimalValidParams(owner) });

    expect(result.success).toBe(true);
    expect(result.capability).toBe(owner.id);
    expect(result.resolvedFromAlias).toBe(alias);
  });

  it('reports the legacy pair a migrated call resolved from', async () => {
    const legacy = legacyOf('asset.list');
    const result = await execute({ tool: legacy.tool, action: legacy.action, params: {} });

    expect(result.capability).toBe('asset.list');
    expect(result.migratedFrom).toEqual({ tool: legacy.tool, action: legacy.action });
  });

  it('rejects an alias owned by more than one capability instead of picking a winner', () => {
    const base = record('asset.import');
    const shared = CapabilityAliasSchema.parse('fixture.shared');
    const left: CapabilityRecord = { ...base, id: CapabilityIdSchema.parse('fixture.left'), aliases: [shared] };
    const right: CapabilityRecord = { ...base, id: CapabilityIdSchema.parse('fixture.right'), aliases: [shared] };
    const index = buildExecuteTargetIndex([left, right]);

    const resolution = resolveExecuteTarget({ capability: 'fixture.shared' }, index);

    expect(resolution.ok).toBe(false);
    if (resolution.ok) return;
    expect(resolution.failure.errorCode).toBe('ALIAS_CONFLICT');
    expect(resolution.failure.message).toContain('fixture.left');
    expect(resolution.failure.message).toContain('fixture.right');
  });

  it('never lets an alias shadow a capability that owns the same canonical ID', () => {
    const base = record('asset.import');
    const owner: CapabilityRecord = { ...base, id: CapabilityIdSchema.parse('fixture.owner'), aliases: [] };
    const shadow: CapabilityRecord = {
      ...base,
      id: CapabilityIdSchema.parse('fixture.shadow'),
      aliases: [CapabilityAliasSchema.parse('fixture.owner')]
    };
    const index = buildExecuteTargetIndex([owner, shadow]);

    const resolution = resolveExecuteTarget({ capability: 'fixture.owner' }, index);

    expect(resolution.ok).toBe(true);
    if (!resolution.ok) return;
    expect(resolution.target.record.id).toBe('fixture.owner');
  });
});

describe('execute: conflicting, unknown and retired selectors fail loudly', () => {
  it('rejects a capability that disagrees with the legacy pair rather than picking one', async () => {
    const legacy = legacyOf('asset.list');
    const result = await execute({ capability: 'asset.import', tool: legacy.tool, action: legacy.action });

    expect(result.success).toBe(false);
    expect(result.errorCode).toBe('FORM_CONFLICT');
    expect(dispatched).toHaveLength(0);
  });

  it('accepts a capability that agrees with the legacy pair', async () => {
    const legacy = legacyOf('asset.list');
    const result = await execute({ capability: 'asset.list', tool: legacy.tool, action: legacy.action, params: {} });

    expect(result.success).toBe(true);
    expect(result.capability).toBe('asset.list');
  });

  it('rejects an unknown capability with bounded suggestions and an executable nextCall', async () => {
    const result = await execute({ capability: 'asset.improt', params: {} });

    expect(result.errorCode).toBe('UNKNOWN_CAPABILITY');
    expect(Array.isArray(result.suggestions)).toBe(true);
    expect(result.suggestions).toContain('asset.import');
    expect((result.suggestions as string[]).length).toBeLessThanOrEqual(3);
    expect(asRecord(result.nextCall).operation).toBe('describe');
    expect(result.inputSchema).toBeUndefined();
  });

  it('rejects an execute call with no selector at all', async () => {
    const result = await execute({ params: {} });

    expect(result.success).toBe(false);
    expect(result.errorCode).toBe('MISSING_SELECTOR');
    expect(dispatched).toHaveLength(0);
  });

  it('refuses a legacy pair whose migration disposition is removed', async () => {
    const result = await execute({
      tool: 'animation_physics',
      action: 'assign_cloth_asset_to_mesh',
      params: {}
    });

    expect(result.success).toBe(false);
    expect(result.errorCode).toBe('CAPABILITY_REMOVED');
    expect(dispatched).toHaveLength(0);
  });

  it('refuses a lossy legacy translation instead of silently dropping data', async () => {
    const result = await execute({
      tool: 'manage_level_structure',
      action: 'set_volume_bounds',
      params: { bounds: { origin: [0, 0, 0], extent: [10, 10, 10] } }
    });

    expect(result.success).toBe(false);
    expect(result.errorCode).toBe('MIGRATION_NON_TRANSLATABLE');
    expect(String(result.message)).toContain('set_volume_extent');
    expect(dispatched).toHaveLength(0);
  });
});

describe('execute: exact per-action input validation', () => {
  it('rejects an undeclared parameter against the exact action schema', async () => {
    const result = await execute({
      capability: 'asset.import',
      params: { sourcePath: '/tmp/a.fbx', destinationPath: '/Game/A', sorcePath: '/tmp/b.fbx' }
    });

    expect(result.errorCode).toBe('UNDECLARED_PARAMETER');
    expect(dispatched).toHaveLength(0);
  });

  it('rejects a missing required parameter', async () => {
    const result = await execute({ capability: 'asset.import', params: { sourcePath: '/tmp/a.fbx' } });

    expect(result.errorCode).toBe('MISSING_REQUIRED_PARAMETER');
    expect(String(result.pointer)).toContain('destinationPath');
    expect(dispatched).toHaveLength(0);
  });

  it('rejects a parameter of the wrong declared type', async () => {
    const result = await execute({
      capability: 'asset.import',
      params: { sourcePath: 7, destinationPath: '/Game/A' }
    });

    expect(result.errorCode).toBe('INVALID_PARAMETER_TYPE');
    expect(dispatched).toHaveLength(0);
  });

  it('rejects a value outside a declared enum', async () => {
    const { record: target, variant } = firstRecordWithVariant(capabilityIndex().records, 'enum');

    const result = await execute({ capability: target.id, params: variant.params });

    expect(result.errorCode).toBe('INVALID_PARAMETER_VALUE');
    expect(String(result.pointer)).toContain(variant.offendingParam);
    expect(dispatched).toHaveLength(0);
  });

  it('rejects a numeric value outside its declared range', async () => {
    const result = await execute({ capability: 'asset.list', params: { limit: 5000 } });

    expect(result.errorCode).toBe('OUT_OF_RANGE');
    expect(dispatched).toHaveLength(0);
  });

  it('rejects an array outside its declared item bounds', async () => {
    const { record: target, variant } = firstRecordWithVariant(capabilityIndex().records, 'range');

    const result = await execute({ capability: target.id, params: variant.params });

    expect(result.errorCode).toBe('OUT_OF_RANGE');
    expect(dispatched).toHaveLength(0);
  });

  it('keeps the reserved action keys out of params', async () => {
    const override = await execute({ capability: 'asset.list', params: { action: 'hack' } });
    expect(override.errorCode).toBe('INVALID_PARAMS');

    const subAction = await execute({ capability: 'asset.list', params: { subAction: 'hack' } });
    expect(subAction.errorCode).toBe('INVALID_PARAMS');
    expect(dispatched).toHaveLength(0);
  });
});

describe('execute: gateway options are typed and never become action params', () => {
  it('rejects an unsupported execution option', async () => {
    const result = await execute({
      capability: 'asset.list',
      params: {},
      options: { durationSeconds: 10 }
    });

    expect(result.errorCode).toBe('UNSUPPORTED_OPTION');
    expect(String(result.message)).toContain('durationSeconds');
    expect(dispatched).toHaveLength(0);
  });

  it('rejects a gateway control smuggled into action params', async () => {
    const result = await execute({ capability: 'asset.list', params: { timeoutMs: 1000 } });

    expect(result.errorCode).toBe('UNSUPPORTED_OPTION');
    expect(dispatched).toHaveLength(0);
  });

  it('rejects a non-object options envelope', async () => {
    const result = await execute({ capability: 'asset.list', params: {}, options: 'fast' });

    expect(result.errorCode).toBe('INVALID_OPTIONS');
    expect(dispatched).toHaveLength(0);
  });

  it('rejects a timeout outside the supported bound', async () => {
    const tooLarge = await execute({ capability: 'asset.list', params: {}, options: { timeoutMs: 600_001 } });
    expect(tooLarge.errorCode).toBe('OUT_OF_RANGE');

    const notInteger = await execute({ capability: 'asset.list', params: {}, options: { timeoutMs: 1.5 } });
    expect(notInteger.errorCode).toBe('OUT_OF_RANGE');

    const negative = await execute({ capability: 'asset.list', params: {}, options: { timeoutMs: -1 } });
    expect(negative.errorCode).toBe('OUT_OF_RANGE');

    expect(dispatched).toHaveLength(0);
  });

  it('refuses options.preview instead of dispatching the real call under a preview flag', async () => {
    const result = await execute({
      capability: 'asset.list',
      params: {},
      options: { preview: true, timeoutMs: 5_000 }
    });

    expect(result.success).toBe(false);
    expect(result.errorCode).toBe('UNSUPPORTED_PREVIEW');
    expect(result.options).toBeUndefined();
    expect(dispatched).toHaveLength(0);
  });

  it('accepts the honoured options and echoes them on the receipt without dispatching them', async () => {
    const result = await execute({
      capability: 'asset.list',
      params: {},
      options: { timeoutMs: 5_000 }
    });

    expect(result.success).toBe(true);
    expect(result.options).toEqual({ timeoutMs: 5_000 });
    expect(dispatched).toHaveLength(1);
    expect(dispatched[0].args.timeoutMs).toBeUndefined();
  });
});

describe('execute: output validation and structured Unreal failures', () => {
  it('fails an output that violates the declared capability schema', async () => {
    handlerResult = { success: 'yes', message: 'ok' };

    const result = await execute({ capability: 'asset.list', params: {} });

    expect(result.success).toBe(false);
    expect(result.errorCode).toBe('OUTPUT_SCHEMA_VIOLATION');
    expect(dispatched).toHaveLength(1);
  });

  it('preserves the handler payload as structured detail on an output violation', async () => {
    handlerResult = { success: 'yes', message: 'partial', data: { assets: ['/Game/A'] } };

    const result = await execute({ capability: 'asset.list', params: {} });

    expect(result.errorCode).toBe('OUTPUT_SCHEMA_VIOLATION');
    expect(result.result).toEqual(handlerResult);
  });

  it('never reports an output violation as a success', async () => {
    handlerResult = { message: 'no success field at all' };

    const result = await execute({ capability: 'asset.list', params: {} });

    expect(result.success).toBe(false);
    expect(asRecord(result.receipt).status).toBe('error');
  });

  it('turns a failed handler result into a typed execution error that keeps Unreal detail', async () => {
    handlerResult = {
      success: false,
      isError: true,
      message: 'Automation bridge is not connected to Unreal Engine.',
      data: null
    };

    const result = await execute({ capability: 'asset.list', params: {} });

    expect(result.success).toBe(false);
    expect(result.errorCode).toBe('UNREAL_EXECUTION_ERROR');
    expect(result.result).toEqual(handlerResult);
    expect(String(result.message)).toContain('Automation bridge is not connected');
  });
});

describe('execute: semantic receipt and error envelopes', () => {
  it('emits a schema-valid success receipt carrying the canonical capability ID', async () => {
    const result = await execute({ capability: 'asset.list', params: {} });

    const receipt = asRecord(result.receipt);
    expect(receipt.status).toBe('success');
    expect(receipt.capabilityId).toBe('asset.list');
    expect(receipt.handles).toEqual([]);
    expect(receipt.changes).toEqual([]);
    expect(receipt.warnings).toEqual([]);
    expect(receipt.nextCalls).toEqual([]);
  });

  it('emits a typed semantic error receipt once the capability is known', async () => {
    const result = await execute({ capability: 'asset.import', params: { sourcePath: '/tmp/a.fbx' } });

    const receipt = asRecord(result.receipt);
    expect(receipt.status).toBe('error');
    expect(receipt.capabilityId).toBe('asset.import');
    const error = asRecord(receipt.error);
    expect(error.kind).toBe('validation');
    expect(error.code).toBe('VALIDATION_ERROR');
  });

  it('classifies an option failure as the option error kind', async () => {
    const result = await execute({ capability: 'asset.list', params: {}, options: { nope: 1 } });

    const error = asRecord(asRecord(result.receipt).error);
    expect(error.kind).toBe('option');
    expect(error.code).toBe('UNSUPPORTED_OPTION');
    expect(error.option).toBe('nope');
  });

  it('classifies a range failure as the range error kind', async () => {
    const result = await execute({ capability: 'asset.list', params: { limit: 5000 } });

    const error = asRecord(asRecord(result.receipt).error);
    expect(error.kind).toBe('range');
    expect(error.code).toBe('OUT_OF_RANGE');
  });

  it('stamps the catalog revision on both success and failure', async () => {
    const ok = await execute({ capability: 'asset.list', params: {} });
    const bad = await execute({ capability: 'asset.list', params: { limit: 5000 } });

    expect(ok.catalogRevision).toBe(catalogRevision());
    expect(bad.catalogRevision).toBe(catalogRevision());
  });
});

describe('execute: pre-existing gateway guarantees are preserved', () => {
  it('still refuses a disabled parent tool with a configure nextCall', async () => {
    dynamicToolManager.disableTools(['manage_asset']);

    const result = await execute({ capability: 'asset.list', params: {} });

    expect(result.errorCode).toBe('TOOL_DISABLED');
    expect(result.nextCall).toEqual({ operation: 'configure', tool: 'manage_asset' });
    expect(dispatched).toHaveLength(0);
  });

  it('still refuses to dispatch while Unreal is disconnected', async () => {
    const result = await execute({ capability: 'asset.list', params: {} }, false);

    expect(result.errorCode).toBe('NOT_CONNECTED');
    expect(result.nextCall).toEqual({ operation: 'search' });
    expect(dispatched).toHaveLength(0);
  });

  it('still exempts system_control.get_project_settings from the connection gate', async () => {
    const result = await execute({ capability: 'system_control.get_project_settings', params: {} }, false);

    expect(result.errorCode).toBeUndefined();
    expect(dispatched).toHaveLength(1);
  });

  it('still refuses a non-object params envelope', async () => {
    const result = await execute({ capability: 'asset.list', params: 'nope' });

    expect(result.errorCode).toBe('INVALID_PARAMS');
    expect(dispatched).toHaveLength(0);
  });

  it('still caps an oversized result', async () => {
    handlerResult = { success: true, message: 'x'.repeat(150_000) };

    const result = await execute({ capability: 'asset.list', params: {} });

    expect(result.errorCode).toBe('RESULT_TOO_LARGE');
    expect(typeof result.resultChars).toBe('number');
  });
});

// Regression: `key in object` consults the prototype chain, so every
// Object.prototype member name once read as "declared" at the
// additionalProperties gate. `__proto__` passed validation and reached
// dispatch, and `constructor`/`toString` were refused under the wrong code.
// Native compares against a TMap, which has no chain, so own-key lookup is
// also what keeps the two surfaces reporting one code for one payload.
describe('execute: prototype-chain keys cannot pass as declared parameters', () => {
  it('refuses a wire-shaped __proto__ key as undeclared instead of dispatching it', async () => {
    const params = JSON.parse('{"path":"/Game","__proto__":{"polluted":true}}') as Record<string, unknown>;
    expect(Object.prototype.hasOwnProperty.call(params, '__proto__')).toBe(true);

    const result = await execute({ capability: 'asset.list', params });

    expect(result.errorCode).toBe('UNDECLARED_PARAMETER');
    expect(dispatched).toHaveLength(0);
    expect(Object.prototype).not.toHaveProperty('polluted');
  });

  it.each(['constructor', 'toString', 'valueOf', 'hasOwnProperty', 'isPrototypeOf'])(
    'refuses Object.prototype member %s as undeclared, not as a type error',
    async (name) => {
      const result = await execute({ capability: 'asset.list', params: { [name]: 'x' } });

      expect(result.errorCode).toBe('UNDECLARED_PARAMETER');
      expect(dispatched).toHaveLength(0);
    }
  );

  it('ignores an inherited action/subAction rather than refusing or forwarding it', async () => {
    const params = Object.create({ action: 'hack', subAction: 'hack' }) as Record<string, unknown>;
    params.path = '/Game';
    expect('action' in params).toBe(true);
    expect(Object.prototype.hasOwnProperty.call(params, 'action')).toBe(false);

    const result = await execute({ capability: 'asset.list', params });

    expect(result.success).toBe(true);
    expect(dispatched).toHaveLength(1);
    expect(dispatched[0]?.args.action).toBe(record('asset.list').routing.dispatchAction);
  });

  it('still refuses an OWN action override at the same gate', async () => {
    const result = await execute({ capability: 'asset.list', params: { action: 'hack' } });

    expect(result.errorCode).toBe('INVALID_PARAMS');
    expect(dispatched).toHaveLength(0);
  });

  it('does not treat an inherited default or required name as satisfied', async () => {
    const hostile = Object.create({ sourcePath: '/inherited', destinationPath: '/Game/x' }) as Record<string, unknown>;

    const result = await execute({ capability: 'asset.import', params: hostile });

    expect(result.errorCode).toBe('MISSING_REQUIRED_PARAMETER');
    expect(dispatched).toHaveLength(0);
  });
});
