// A folded family: one record stands for a set of bridge actions that differ
// only by a selector value. These tests pin the two data-driven rules both
// doors apply (pins before validation, action after it) against the shipped
// volume fold, and the authoring-time invariants that keep a fold consistent.
import { beforeEach, describe, expect, it, vi } from 'vitest';
import { Logger } from '../../src/utils/logging/logger.js';
import type { ITools } from '../../src/types/tools/tool-interfaces.js';
import type { GatewayContext } from '../../src/server/tool-registry-gateway.js';
import { handleUnrealGatewayCall } from '../../src/server/tool-registry-gateway.js';
import { executeTargetIndex, resolveExecuteTarget, type ExecuteTarget } from '../../src/server/gateway/gateway-execute-resolve.js';
import { resolveDispatchAction } from '../../src/server/gateway/gateway-dispatch-by.js';
import { matchedFoldedGrant } from '../../src/server/gateway/gateway-execute-policy.js';
import {
  CapabilityRecordSourceSchema,
  createCapabilityRecord
} from '../../src/tools/catalog/capabilities/index.js';
import type { CapabilityRecordSource } from '../../src/tools/catalog/capabilities/index.js';
import { LEVEL_VOLUME_A_RECORDS } from '../../src/tools/catalog/capabilities/records/world/manage-level-structure.volume-a.data.js';

const dispatched: Array<{ tool: string; args: Record<string, unknown> }> = [];

vi.mock('../../src/tools/orchestration/consolidated-tool-handlers.js', () => ({
  handleConsolidatedToolCall: vi.fn(async (tool: string, args: Record<string, unknown>) => {
    dispatched.push({ tool, args });
    return { success: true, message: 'ok' };
  })
}));

function makeContext(): GatewayContext {
  const tools: ITools = {
    systemTools: {
      executeConsoleCommand: async () => ({ success: false }),
      getProjectSettings: async () => ({})
    },
    assetResources: { list: async () => ({}) }
  };
  return {
    tools,
    logger: new Logger('dispatch-by', 'error'),
    elicitationTimeoutMs: 0,
    ensureConnected: async () => true
  };
}

async function execute(args: Record<string, unknown>): Promise<Record<string, unknown>> {
  return await handleUnrealGatewayCall({ operation: 'execute', ...args }, makeContext());
}

const TOOL = 'manage_level_structure';
const ID = 'manage_level_structure.create_volume';
const ORIGIN = { x: 0, y: 0, z: 0 };

describe('folded family: execute on both request forms', () => {
  beforeEach(() => {
    dispatched.length = 0;
  });

  it('the primary operation maps its selector to the bridge action the handlers already implement', async () => {
    const result = await execute({ capability: ID, params: { volumeClass: 'KillZVolume', location: ORIGIN } });
    expect(result.errorCode).toBeUndefined();
    expect(dispatched).toHaveLength(1);
    expect(dispatched[0]?.tool).toBe(TOOL);
    expect(dispatched[0]?.args.action).toBe('add_kill_z_volume');
    expect(dispatched[0]?.args.volumeClass).toBe('KillZVolume');
  });

  it('an old name dispatches itself, with the class it implied pinned so the folded contract accepts it', async () => {
    const result = await execute({
      tool: TOOL,
      action: 'create_trigger_box',
      params: { location: ORIGIN, boxExtent: { x: 1, y: 1, z: 1 } }
    });
    expect(result.errorCode).toBeUndefined();
    expect(dispatched).toHaveLength(1);
    expect(dispatched[0]?.args.action).toBe('create_trigger_box');
    expect(dispatched[0]?.args.volumeClass).toBe('TriggerBox');
  });

  it('an old id used as an alias behaves exactly like the old name', async () => {
    const result = await execute({ capability: 'manage_level_structure.create_kill_z_volume', params: { location: ORIGIN } });
    expect(result.errorCode).toBeUndefined();
    expect(dispatched[0]?.args.action).toBe('create_kill_z_volume');
    expect(dispatched[0]?.args.volumeClass).toBe('KillZVolume');
  });

  it('a caller who names the old action AND sends a disagreeing selector value is refused, not dispatched', async () => {
    const result = await execute({
      tool: TOOL,
      action: 'create_kill_z_volume',
      params: { volumeClass: 'TriggerBox', location: ORIGIN }
    });
    expect(result.errorCode).toBe('INVALID_PARAMETER_VALUE');
    expect(dispatched).toHaveLength(0);
  });

  it('the primary operation refuses a missing or unknown class before anything is dispatched', async () => {
    const missing = await execute({ capability: ID, params: { location: ORIGIN } });
    expect(missing.errorCode).toBe('MISSING_REQUIRED_PARAMETER');
    const unknown = await execute({ capability: ID, params: { volumeClass: 'LavaVolume', location: ORIGIN } });
    expect(unknown.errorCode).toBe('INVALID_PARAMETER_VALUE');
    expect(dispatched).toHaveLength(0);
  });
});

describe('folded family: discovery', () => {
  it('the parent advertises one volume creation action and still resolves every old name to it', async () => {
    const summary = await handleUnrealGatewayCall({ operation: 'describe', tool: TOOL }, makeContext());
    const actions = Array.isArray(summary.actions) ? summary.actions : [];
    expect(actions).toContain('create_volume');
    expect(actions).not.toContain('create_trigger_volume');
    expect(actions).not.toContain('add_post_process_volume');

    const old = await handleUnrealGatewayCall(
      { operation: 'describe', tool: TOOL, action: 'create_trigger_volume' },
      makeContext()
    );
    expect(JSON.stringify(old)).toContain(ID);
    expect(old.errorCode).toBeUndefined();
  });

  it('the selector advertises every class the map can route', async () => {
    const contract = await handleUnrealGatewayCall(
      { operation: 'describe', capability: ID, param: 'volumeClass' },
      makeContext()
    );
    const text = JSON.stringify(contract);
    for (const value of ['TriggerVolume', 'KillZVolume', 'PostProcessVolume', 'NavMeshBoundsVolume']) {
      expect(text).toContain(value);
    }
  });
});

describe('fold invariants are enforced at authoring time', () => {
  const source = LEVEL_VOLUME_A_RECORDS[0] as CapabilityRecordSource;
  const dispatchBy = source.routing.dispatchBy;
  if (dispatchBy === undefined) throw new Error('the volume fold must declare routing.dispatchBy');

  it('the shipped fold satisfies them', () => {
    expect(() => createCapabilityRecord(source)).not.toThrow();
  });

  it('a mapped action outside the folded pairs is refused', () => {
    const broken = {
      ...source,
      routing: { ...source.routing, dispatchBy: { ...dispatchBy, actions: { ...dispatchBy.actions, TriggerBox: 'create_trigger_prism' } } }
    };
    expect(() => CapabilityRecordSourceSchema.parse(broken)).toThrow(/folded legacy actions/);
  });

  it('a map key outside the selector enum is refused', () => {
    const broken = {
      ...source,
      routing: { ...source.routing, dispatchBy: { ...dispatchBy, actions: { ...dispatchBy.actions, LavaVolume: 'create_trigger_box' } } }
    };
    expect(() => CapabilityRecordSourceSchema.parse(broken)).toThrow(/keys must equal the enum/);
  });

  it('a pin naming an undeclared property is refused', () => {
    const [primary, first, ...rest] = source.legacyIds;
    const broken = { ...source, legacyIds: [primary, { ...first, folded: { lava: true } }, ...rest] };
    expect(() => CapabilityRecordSourceSchema.parse(broken)).toThrow(/declared input property/);
  });

  it('the primary pair cannot itself be folded', () => {
    const [primary, ...rest] = source.legacyIds;
    const broken = { ...source, legacyIds: [{ ...primary, folded: { volumeClass: 'TriggerBox' } }, ...rest] };
    expect(() => CapabilityRecordSourceSchema.parse(broken)).toThrow(/cannot be folded/);
  });
});

describe('a required-selector family keeps every old name callable', () => {
  beforeEach(() => {
    dispatched.length = 0;
  });

  // Regression: control_actor.find is a NEW primary, so its selector is
  // required with no default; its alias members must pin the selector value
  // their name implied, or the two shipped find_actors_by_* names are refused
  // at validation (MISSING_REQUIRED_PARAMETER) on both doors.
  const FIND = 'control_actor.find';

  it('every folded pair of a required-selector record pins the selector', async () => {
    const result = await handleUnrealGatewayCall(
      { operation: 'describe', capability: FIND },
      makeContext()
    );
    expect(result.errorCode).toBeUndefined();
    const record = await handleUnrealGatewayCall(
      { operation: 'describe', capability: FIND, param: 'findBy' },
      makeContext()
    );
    expect(JSON.stringify(record)).toContain('class');
    expect(JSON.stringify(record)).toContain('name');
  });

  it('an alias member under a required selector dispatches itself with its pin', async () => {
    const result = await execute({
      tool: 'control_actor',
      action: 'find_actors_by_class',
      params: { className: 'PointLight' }
    });
    expect(result.errorCode).toBeUndefined();
    expect(dispatched).toHaveLength(1);
    expect(dispatched[0]?.args.action).toBe('find_actors_by_class');
    expect(dispatched[0]?.args.findBy).toBe('class');
  });

  it('the required selector is pinned for the name alias too', async () => {
    const result = await execute({
      tool: 'control_actor',
      action: 'find_actors_by_name',
      params: { name: 'PointLight1' }
    });
    expect(result.errorCode).toBeUndefined();
    expect(dispatched[0]?.args.action).toBe('find_actors_by_name');
    expect(dispatched[0]?.args.findBy).toBe('name');
  });
});

describe('a grant naming a folded pair authorizes that pair only', () => {
  const FIND = 'control_actor.find';
  const targetOf = (capability: string): ExecuteTarget => {
    const index = executeTargetIndex();
    const resolution = resolveExecuteTarget({ capability, params: {} }, index);
    if (!resolution.ok) throw new Error(`unresolvable: ${capability}`);
    return resolution.target;
  };

  it('matchedFoldedGrant returns the pair for any folded pair name and undefined for the canonical id', async () => {
    const target = targetOf(FIND);
    const pair = matchedFoldedGrant('control_actor.find_by_class', target);
    expect(pair?.action).toBe('find_by_class');
    // An alias member is itself a folded pair: a grant by that name authorizes it.
    const aliasPair = matchedFoldedGrant('control_actor.find_actors_by_class', target);
    expect(aliasPair?.action).toBe('find_actors_by_class');
    expect(matchedFoldedGrant(FIND, target)).toBeUndefined();
  });

  it('a grant naming one folded sibling does not authorize dispatching another', async () => {
    const index = executeTargetIndex();
    const resolution = resolveExecuteTarget(
      { tool: 'control_actor', action: 'find_by_name', params: { name: 'X' } },
      index
    );
    expect(resolution.ok).toBe(true);
    if (!resolution.ok) return;
    const granted = matchedFoldedGrant('control_actor.find_by_class', resolution.target);
    const dispatchAction = resolveDispatchAction(resolution.target, { name: 'X' });
    expect(granted?.action).toBe('find_by_class');
    expect(dispatchAction).toBe('find_by_name');
  });
});

describe('volume former names: pair-callable, advertised as aliases only off the shape words', () => {
  beforeEach(() => {
    dispatched.length = 0;
  });

  it('dispatches the three trigger shapes by {tool, action} with the class each implied', async () => {
    const shapes = ['create_trigger_box', 'create_trigger_sphere', 'create_trigger_capsule'];
    for (const name of shapes) {
      const result = await execute({ tool: TOOL, action: name, params: { location: ORIGIN } });
      expect(result.errorCode, name).toBeUndefined();
    }
    expect(dispatched.map((entry) => entry.args.action)).toEqual(shapes);
    expect(dispatched.map((entry) => entry.args.volumeClass)).toEqual(['TriggerBox', 'TriggerSphere', 'TriggerCapsule']);
  });

  it('refuses a shape name as a capability id while an advertised former name resolves', async () => {
    const shape = await execute({ capability: `${TOOL}.create_trigger_box`, params: { location: ORIGIN } });
    expect(shape.errorCode).toBe('UNKNOWN_CAPABILITY');
    const advertised = await execute({ capability: `${TOOL}.add_trigger_volume`, params: { location: ORIGIN } });
    expect(advertised.errorCode).toBeUndefined();
  });
});

describe('a consent grant only scopes a capability that requires consent', () => {
  beforeEach(() => {
    dispatched.length = 0;
  });

  it('does not refuse a policy-none call that carried an unnecessary folded grant', async () => {
    const result = await execute({
      tool: 'control_actor',
      action: 'spawn',
      params: { spawnKind: 'class', classPath: '/Script/Engine.PointLight', actorName: 'IT_Grant' },
      consent: { capability: 'control_actor.spawn_actor', acknowledge: 'explicit' }
    });
    expect(result.errorCode).toBeUndefined();
    expect(dispatched).toHaveLength(1);
    expect(dispatched[0]?.args.action).toBe('spawn');
  });

  it('still refuses a folded grant used to run a different sibling when consent is required', async () => {
    const result = await execute({
      tool: 'control_actor',
      action: 'delete_by_tag',
      params: { tag: 'IT_Tag' },
      consent: { capability: 'control_actor.destroy_actor', acknowledge: 'explicit' }
    });
    expect(result.errorCode).toBe('CONSENT_REQUIRED');
    expect(dispatched).toHaveLength(0);
  });
});

describe('folded family: an omitted selector follows the parameters', () => {
  const MATERIAL = '/Game/M_Test';
  const GRANT = { capability: 'material.add_material_node', acknowledge: 'explicit' };
  beforeEach(() => {
    dispatched.length = 0;
  });

  it('runs the one variant that declares the parameters sent', async () => {
    // A small model sent operations without nodeKind "batch" and got the plain
    // node adder, which ignored the steps and failed on a missing nodeType.
    const result = await execute({
      tool: 'manage_asset',
      action: 'add_material_node',
      params: { materialPath: MATERIAL, operations: [{ edit: 'add_texture_coordinate', id: 'uv' }] },
      consent: GRANT
    });
    expect(result.errorCode).toBeUndefined();
    expect(dispatched[0]?.args.action).toBe('build_material_graph');
  });

  it('keeps the default when the parameters point at different variants, and never overrides a sent selector', async () => {
    await execute({
      tool: 'manage_asset',
      action: 'add_material_node',
      params: { materialPath: MATERIAL, operations: [], code: 'return 1;' },
      consent: GRANT
    });
    await execute({
      tool: 'manage_asset',
      action: 'add_material_node',
      params: { materialPath: MATERIAL, nodeKind: 'node', nodeType: 'Lerp', operations: [] },
      consent: GRANT
    });
    expect(dispatched.map((call) => call.args.action)).toEqual(['add_material_node', 'add_material_node']);
  });

  it('a grant for one variant does not cover the variant the parameters select', async () => {
    const result = await execute({
      tool: 'manage_asset',
      action: 'add_material_node',
      params: { materialPath: MATERIAL, operations: [] },
      consent: { capability: 'manage_asset.add_scalar_parameter', acknowledge: 'explicit' }
    });
    expect(result.errorCode).toBe('CONSENT_REQUIRED');
    expect(dispatched).toHaveLength(0);
  });
});

describe('a folded family still refuses a bare call no member accepted', () => {
  beforeEach(() => {
    dispatched.length = 0;
  });

  it('refuses a spawn with no class, actor class or blueprint path before dispatch', async () => {
    const result = await execute({ capability: 'control_actor.spawn', params: {} });
    expect(result.errorCode).toBe('MISSING_REQUIRED_ONEOF');
    expect(dispatched).toHaveLength(0);
  });
});
