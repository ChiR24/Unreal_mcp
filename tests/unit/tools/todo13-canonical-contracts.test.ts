// Plan Todo 13 (BB-019, BB-027, BB-038, BB-060, BB-067, BB-069, BB-071) -
// every canonical field a handler or native body actually reads must be
// declarable, and every shape the record advertises must be executable.
//
// Written after the fixes landed, so non-vacuity is proven by mutation instead
// of by a captured RED: toggle any one fix off and the case naming it fails.
// Independently reproduced by an external verifier, which deleted the shared
// geometry alias line and reverted both native `<=` gates and saw each mutation
// fail the case naming it.
//
// The record assertions read ALL_CAPABILITY_RECORDS (the hand-authored source),
// never the generated artifacts, which stay stale until the Todo 36 convergence.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';
import { describe, expect, it } from 'vitest';

import { ALL_CAPABILITY_RECORDS } from '../../../src/tools/catalog/capabilities/records/aggregate.js';
import type { CapabilityRecord } from '../../../src/tools/catalog/capabilities/model.js';
import { applyEffectArgumentAliases } from '../../../src/tools/handlers/effect/effect-argument-normalization.js';
import { handleSequenceTools } from '../../../src/tools/handlers/sequence/sequence-handlers.js';
import { handleSequenceCoreAction } from '../../../src/tools/handlers/sequence/sequence-core-actions.js';
import type { EffectArgs } from '../../../src/types/handlers/handler-types.js';
import type { ITools } from '../../../src/types/tools/tool-interfaces.js';

const plain = (value: unknown): Record<string, unknown> =>
  JSON.parse(JSON.stringify(value)) as Record<string, unknown>;

function record(id: string): CapabilityRecord {
  const found = ALL_CAPABILITY_RECORDS.find((entry) => String(entry.id) === id);
  if (!found) throw new Error(`fixture record '${id}' is absent from the canonical source`);
  return found;
}

function inputSchema(id: string): {
  properties: Record<string, unknown>;
  required: string[];
  requiredOneOf: string[];
} {
  const schema = plain(record(id).schemas.input);
  return {
    properties: plain(schema.properties),
    required: Array.isArray(schema.required) ? (schema.required as string[]) : [],
    requiredOneOf: Array.isArray(schema.requiredOneOf) ? (schema.requiredOneOf as string[]) : []
  };
}

function exampleInput(id: string): Record<string, unknown> {
  return plain(record(id).examples[0]?.input ?? {});
}

// The gates under test all refuse BEFORE executeAutomationRequest, so a bridge
// that throws on contact proves the refusal never reached dispatch.
const unreachableTools = {
  get automationBridge(): never {
    throw new Error('dispatch must not be reached: the gate should refuse pre-dispatch');
  }
} as unknown as ITools;

describe('todo13 BB-019/BB-060: geometry targets the field native actually reads', () => {
  it.each([
    ['manage_geometry.array_radial', ['action', 'actorName', 'count']],
    ['manage_geometry.recalculate_normals', ['action', 'actorName']]
  ])('%s requires actorName, never the targetActor alias', (id, expected) => {
    const { properties, required } = inputSchema(id);

    expect(required).toEqual(expected);
    expect(required).not.toContain('targetActor');
    // The alias stays declared so an existing caller is not refused outright;
    // the handler collapses it onto actorName before dispatch.
    expect(Object.keys(properties)).toContain('targetActor');
    expect(Object.keys(properties)).toContain('actorName');
  });

  it('the reported examples lead with the canonical field', () => {
    for (const id of ['manage_geometry.array_radial', 'manage_geometry.recalculate_normals']) {
      const example = exampleInput(id);
      expect(example.actorName, id).toBe('DM_A');
      expect(example.targetActor, id).toBeUndefined();
    }
  });
});

describe('todo13 BB-027: validate_niagara_system declares the systemPath native demands', () => {
  const id = 'manage_effect.validate_niagara_system';

  it('declares systemPath and accepts any one of the three spellings', () => {
    const { properties, requiredOneOf } = inputSchema(id);

    expect(Object.keys(properties)).toContain('systemPath');
    expect(requiredOneOf).toEqual(['systemPath', 'assetPath', 'system']);
  });

  it('the example uses the canonical spelling', () => {
    expect(exampleInput(id).systemPath).toBe('/Game/NS_Fire');
  });

  it.each([
    ['assetPath', { action: 'validate_niagara_system', assetPath: '/Game/NS_Fire' }],
    ['system', { action: 'validate_niagara_system', system: '/Game/NS_Fire' }]
  ])('normalizes the %s alias onto systemPath before dispatch', (_alias, args) => {
    const mutable: Record<string, unknown> = { ...args };
    applyEffectArgumentAliases(mutable, mutable as unknown as EffectArgs);

    expect(mutable.systemPath).toBe('/Game/NS_Fire');
  });

  it('leaves assetPath alone for the sibling action that reads it canonically', () => {
    const mutable: Record<string, unknown> = { action: 'get_niagara_info', assetPath: '/Game/NS_Fire' };
    applyEffectArgumentAliases(mutable, mutable as unknown as EffectArgs);

    expect(mutable.assetPath).toBe('/Game/NS_Fire');
    expect(mutable.systemPath).toBeUndefined();
  });
});

describe('todo13 BB-038: bound tracks accept either binding spelling', () => {
  it.each([
    'sequence.cinematic.add_transform_track',
    'sequence.cinematic.add_property_track'
  ])('%s declares bindingGuid and requires one of it or actorName', (id) => {
    const { properties, requiredOneOf } = inputSchema(id);

    expect(Object.keys(properties)).toContain('bindingGuid');
    expect(requiredOneOf).toEqual(['actorName', 'bindingGuid']);
  });

  it('the property-track example satisfies its own requiredOneOf', () => {
    const example = exampleInput('sequence.cinematic.add_property_track');

    expect(example.actorName).toBe('CineCameraActor_1');
    expect(example.property).toBe('Transform');
  });
});

describe('todo13 BB-069: add_keyframe honours requiredOneOf and validates composed Transforms', () => {
  const id = 'sequence.add_keyframe';

  it('the record keeps bindingId/actorName as alternatives, not both required', () => {
    const { required, requiredOneOf } = inputSchema(id);

    expect(requiredOneOf).toEqual(['bindingId', 'actorName']);
    expect(required).not.toContain('actorName');
    expect(required).not.toContain('bindingId');
  });

  it('documents the composed Transform shape on value', () => {
    const { properties } = inputSchema(id);
    const value = plain(properties.value);

    expect(String(value.description)).toContain('location');
    expect(String(value.description)).toContain('rotation');
    expect(String(value.description)).toContain('scale');
    // No declared type: a non-Transform property legitimately keys a scalar.
    expect(value.type).toBeUndefined();
  });

  it('refuses a half-filled Transform component pre-dispatch, naming the axis', async () => {
    const result = await handleSequenceCoreAction(
      'add_keyframe',
      {
        action: 'add_keyframe',
        path: '/Game/Cinematics/SEQ_Master',
        actorName: 'Cube',
        property: 'Transform',
        frame: 0,
        value: { location: { x: 0, y: 0 } }
      },
      unreachableTools
    ) as Record<string, unknown>;

    expect(result.success).toBe(false);
    expect(result.error).toBe('INVALID_ARGUMENT');
    expect(String(result.message)).toContain('value.location');
  });

  it('accepts a fully specified composed Transform (no pre-dispatch refusal)', async () => {
    await expect(handleSequenceCoreAction(
      'add_keyframe',
      {
        action: 'add_keyframe',
        path: '/Game/Cinematics/SEQ_Master',
        bindingId: 'ABC-123',
        property: 'Transform',
        frame: 0,
        value: { location: { x: 1, y: 2, z: 3 }, rotation: { pitch: 0, yaw: 90, roll: 0 } }
      },
      unreachableTools
    )).rejects.toThrow(/dispatch must not be reached/);
  });

  it('rejects a call that names neither actorName nor bindingId', async () => {
    await expect(handleSequenceCoreAction(
      'add_keyframe',
      { action: 'add_keyframe', path: '/Game/Cinematics/SEQ_Master', property: 'Transform', frame: 0 },
      unreachableTools
    )).rejects.toThrow(/one of actorName or bindingId/);
  });
});

describe('todo13 BB-071: the MRQ range is end-exclusive', () => {
  const id = 'sequence.mrq.configure_output_settings';

  it('documents end-exclusive semantics on endFrame', () => {
    const { properties } = inputSchema(id);
    const start = plain(properties.startFrame);
    const end = plain(properties.endFrame);

    expect(end.minimum).toBe(1);
    expect(String(end.description)).toContain('EXCLUSIVE');

    // startFrame is ALSO declared by the cinematic records, so any extra
    // constraint keyword here makes the two declarations diverge and the merged
    // parent union degrades from Schema.Integer to Schema.TypeUnion in the
    // generated native registry - which the native contract gate rejects. Keep
    // it a bare typed field; the strict range is enforced in code, not here.
    expect(Object.keys(start).sort()).toEqual(['description', 'type']);
    expect(start.type).toBe('integer');
  });

  it.each([
    [0, 0],
    [24, 24],
    [30, 12]
  ])('refuses startFrame=%i endFrame=%i before dispatch', async (startFrame, endFrame) => {
    const result = await handleSequenceTools(
      'configure_output_settings',
      { action: 'configure_output_settings', jobId: 'render-job-1', startFrame, endFrame },
      unreachableTools
    ) as Record<string, unknown>;

    expect(result.success).toBe(false);
    expect(result.error).toBe('INVALID_FRAME_RANGE');
    expect(String(result.message)).toContain('end-exclusive');
  });

  it('lets a real one-frame range through to dispatch', async () => {
    await expect(handleSequenceTools(
      'configure_output_settings',
      { action: 'configure_output_settings', jobId: 'render-job-1', startFrame: 0, endFrame: 1 },
      unreachableTools
    )).rejects.toThrow(/dispatch must not be reached/);
  });
});

// Source-contract guard: an independent verifier proved that reverting the
// native comparison to `<` was detected by nothing in the repo, so the TS
// pre-dispatch gate above was the only thing holding BB-071. Both native payload
// gates are pinned here; the third site (ResourceLimits.cpp `RangeSpan < 0`)
// reads already-applied job state, not the payload, and is deliberately excluded.
describe('todo13 BB-071: both native payload gates reject an empty range', () => {
  const nativeRoot = join(
    'plugins', 'McpAutomationBridge', 'Source', 'McpAutomationBridge', 'Private',
    'Domains', 'Sequence', 'MovieRender'
  );

  it.each([
    'McpAutomationBridge_SequenceMovieRenderOutput.cpp',
    'McpAutomationBridge_SequenceMovieRenderOutputValidation.cpp'
  ])('%s compares EndFrame <= StartFrame, never <', (file) => {
    const source = readFileSync(join(nativeRoot, file), 'utf8');

    expect(source).toContain('EndFrame <= StartFrame');
    expect(source).not.toMatch(/EndFrame\s*<\s*StartFrame/u);
    expect(source).toContain('INVALID_FRAME_RANGE');
  });
});

describe('todo13 BB-067: SCS contracts declare what the handler forwards', () => {
  it('modify_scs requires the operations batch the handler actually sends', () => {
    const { properties, required } = inputSchema('blueprint.modify_scs');

    expect(required).toEqual(['action', 'blueprintPath', 'operations']);
    expect(Object.keys(properties)).toContain('operations');
    // The top-level bag was read by nobody for this action.
    expect(Object.keys(properties)).not.toContain('properties');
  });

  it('the modify_scs example matches the shape the live suite proves works', () => {
    const example = exampleInput('blueprint.modify_scs');
    const operations = example.operations as Array<Record<string, unknown>>;

    expect(Array.isArray(operations)).toBe(true);
    expect(operations[0]?.type).toBe('add_component');
    expect(example.properties).toBeUndefined();
  });

  it('set_scs_property requires the value it exists to write', () => {
    const { required } = inputSchema('blueprint.set_scs_property');

    expect(required).toContain('propertyValue');
  });
});

describe('todo13 disclosure: set_pin_default_value prefers propertyValue with a legacy value fallback', () => {
  const nativeRoot = join(
    'plugins', 'McpAutomationBridge', 'Source', 'McpAutomationBridge', 'Private',
    'Domains', 'BlueprintGraph', 'PinMutations'
  );

  it('advertises only propertyValue (closed schema) and leaves it optional', () => {
    const { properties, required } = inputSchema('blueprint.set_pin_default_value');

    expect(Object.keys(properties)).toContain('propertyValue');
    // `value` is a legacy spelling the handler still accepts, but the closed
    // schema may only advertise the canonical field.
    expect(Object.keys(properties)).not.toContain('value');
    // Optional: clearing a pin default is a valid call that omits it.
    expect(required).not.toContain('propertyValue');
  });

  it('the native handler reads propertyValue first and falls back to value', () => {
    const source = readFileSync(join(nativeRoot, 'McpAutomationBridge_BlueprintGraphPinSetDefaultValue.cpp'), 'utf8');

    expect(source).toContain('TryGetField(TEXT("propertyValue"))');
    expect(source).toContain('TryGetField(TEXT("value"))');
  });

  it('the native handler refuses with INVALID_ARGUMENT when neither field is present', () => {
    const source = readFileSync(join(nativeRoot, 'McpAutomationBridge_BlueprintGraphPinSetDefaultValue.cpp'), 'utf8');

    expect(source).toContain('INVALID_ARGUMENT');
    expect(source).toContain('field required');
  });
});
