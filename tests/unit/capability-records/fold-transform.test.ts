// Unit tests for the fold transform itself: widen()'s schema algebra and
// applyFolds()'s position math / throw paths. fold.ts runs at module load for
// every parent, so a bad merge changes the shipped contract for 1,543 callable
// pairs — these pin the merge rules directly, independent of any one fold.
import { describe, expect, it } from 'vitest';
import type { CapabilityRecordSource, JsonObject } from '../../../src/tools/catalog/capabilities/model.js';
import { createCapabilityRecord } from '../../../src/tools/catalog/capabilities/parser.js';
import { buildCoreRecord } from '../../../src/tools/catalog/capabilities/records/core/builder.js';
import { applyFolds, byName, type FoldSpec } from '../../../src/tools/catalog/capabilities/records/shared/fold.js';

const member = (
  action: string,
  inputProps: JsonObject,
  required: readonly string[],
  requiredOneOf?: readonly string[],
): CapabilityRecordSource =>
  buildCoreRecord({
    parentTool: 'system_control',
    action,
    domain: 'test-domain',
    family: 'test-family',
    summary: action,
    whenToUse: [],
    whenNotToUse: [],
    inputProps,
    required,
    requiredOneOf,
    effect: 'read',
    costLatency: 'instant',
    costResources: 'low',
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET',
    normalizationRationale: 'test',
    exampleInput: {},
    exampleOutput: { success: true },
  });

const FOLD: FoldSpec = {
  primary: 'query', selector: 'which', summary: 'query variants',
  members: { one: 'q_one', two: 'q_two' },
};

const foldTwo = (
  inputOne: JsonObject,
  inputTwo: JsonObject,
  required: readonly string[],
  spec: FoldSpec = FOLD,
): CapabilityRecordSource =>
  applyFolds(
    [member('q_one', inputOne, required), member('q_two', inputTwo, required)],
    [spec],
    'system_control',
  )[0] as CapabilityRecordSource;

describe('widen() merge rules (two-member folds)', () => {
  it('unions enums of a shared property', () => {
    const merged = foldTwo(
      { kind: { type: 'string', enum: ['x'] } },
      { kind: { type: 'string', enum: ['y'] } },
      ['kind'],
    );
    expect((merged.schemas.input.properties.kind as JsonObject).enum).toEqual(['x', 'y']);
  });

  it('drops a shared default the members disagree on', () => {
    const merged = foldTwo(
      { depth: { type: 'number', default: 1 } },
      { depth: { type: 'number', default: 2 } },
      ['depth'],
    );
    expect(merged.schemas.input.properties.depth).toEqual({ type: 'number' });
  });

  it('drops a one-sided default instead of imposing it on the sibling member calls', () => {
    const merged = foldTwo(
      { depth: { type: 'number', default: 7 } },
      { depth: { type: 'number' } },
      ['depth'],
    );
    expect(merged.schemas.input.properties.depth).toEqual({ type: 'number' });
  });

  it('widens numeric bounds to the looser side', () => {
    const merged = foldTwo(
      { depth: { type: 'number', minimum: 0, maximum: 10 } },
      { depth: { type: 'number', minimum: 5, maximum: 100 } },
      ['depth'],
    );
    expect(merged.schemas.input.properties.depth).toEqual({ type: 'number', minimum: 0, maximum: 100 });
  });

  it('drops a narrowing keyword declared by one member only', () => {
    const merged = foldTwo(
      { tag: { type: 'string', pattern: '^a' } },
      { tag: { type: 'string' } },
      ['tag'],
    );
    expect(merged.schemas.input.properties.tag).toEqual({ type: 'string' });
  });

  it('drops a one-sided type instead of narrowing the free member', () => {
    const merged = foldTwo(
      { value: { type: 'number' } },
      { value: {} },
      ['value'],
    );
    expect(merged.schemas.input.properties.value).toEqual({});
  });

  it('refuses an input-side type disagreement', () => {
    expect(() => foldTwo(
      { value: { type: 'number' } },
      { value: { type: 'string' } },
      ['value'],
    )).toThrow(/is typed .* by different members/);
  });

  it('drops nested properties declared by one member only', () => {
    const merged = foldTwo(
      {
        loc: {
          type: 'object',
          properties: { x: { type: 'number' }, y: { type: 'number' } },
          required: ['x'],
        },
      },
      {
        loc: {
          type: 'object',
          properties: { x: { type: 'number' } },
          required: ['x'],
        },
      },
      ['loc'],
    );
    const loc = merged.schemas.input.properties.loc as JsonObject;
    expect(Object.keys(loc.properties as JsonObject)).toEqual(['x']);
  });

  it('drops nested required declared by one member only', () => {
    const merged = foldTwo(
      { loc: { type: 'object', properties: { x: { type: 'number' } }, required: ['x'] } },
      { loc: { type: 'object', properties: { x: { type: 'number' } } } },
      ['loc'],
    );
    const loc = merged.schemas.input.properties.loc as JsonObject;
    expect(loc.required).toBeUndefined();
  });

  it('drops a shared closed interior once the members declare different nested keys', () => {
    const merged = foldTwo(
      { loc: { type: 'object', properties: { x: { type: 'number' }, y: { type: 'number' } }, additionalProperties: false } },
      { loc: { type: 'object', properties: { x: { type: 'number' } }, additionalProperties: false } },
      ['loc'],
    );
    const loc = merged.schemas.input.properties.loc as JsonObject;
    expect(Object.keys(loc.properties as JsonObject)).toEqual(['x']);
    expect(loc.additionalProperties).toBeUndefined();
  });

  it('keeps a shared closed interior while the members declare the same nested keys', () => {
    const merged = foldTwo(
      { loc: { type: 'object', description: 'L', properties: { x: { type: 'number' } }, additionalProperties: false } },
      { loc: { type: 'object', properties: { x: { type: 'number' } }, additionalProperties: false } },
      ['loc'],
    );
    const loc = merged.schemas.input.properties.loc as JsonObject;
    expect(loc.additionalProperties).toBe(false);
  });

  it('unions every member requiredOneOf group into the folded obligation', () => {
    const folded = applyFolds(
      [
        member('q_one', { a: { type: 'string' }, b: { type: 'string' } }, [], ['a', 'b']),
        member('q_two', { c: { type: 'string' } }, [], ['c']),
      ],
      [FOLD],
      'system_control',
    )[0] as CapabilityRecordSource;
    expect(folded.schemas.input.requiredOneOf).toEqual(['a', 'b', 'c']);
  });

  it('drops the obligation when any member accepts a parameterless call', () => {
    const folded = applyFolds(
      [member('q_one', { a: { type: 'string' } }, [], ['a']), member('q_two', {}, [])],
      [FOLD],
      'system_control',
    )[0] as CapabilityRecordSource;
    expect(folded.schemas.input.requiredOneOf).toBeUndefined();
  });
});

describe('applyFolds position math and throw paths', () => {
  const records = (): readonly CapabilityRecordSource[] =>
    ['a_one', 'a_two', 'keep', 'b_one', 'b_two']
      .map((action) => member(action, {}, []));

  const FOLD_A: FoldSpec = { primary: 'fused_a', selector: 'kind', summary: 's', members: { one: 'a_one', two: 'a_two' } };
  const FOLD_B: FoldSpec = { primary: 'fused_b', selector: 'kind', summary: 's', members: { one: 'b_one', two: 'b_two' } };

  it('replaces members at the first member position, preserving other order', () => {
    const folded = applyFolds(records(), [FOLD_A, FOLD_B], 'system_control');
    expect(folded.map((record) => String(record.legacyIds[0]?.action))).toEqual(['fused_a', 'keep', 'fused_b']);
  });

  it('refuses a fold naming a member that does not exist', () => {
    expect(() => applyFolds(records(), [{ primary: 'x', summary: 's', members: ['a_one', 'missing'] }], 'system_control'))
      .toThrow(/not found/);
  });

  it('refuses a fold whose new primary collides with an unrelated record', () => {
    expect(() => applyFolds(
      records(),
      [{ primary: 'keep', selector: 'kind', summary: 's', members: { one: 'a_one', two: 'a_two' } }],
      'system_control',
    )).toThrow(/an unrelated record already uses that action/);
  });

  it('refuses a duplicate member action', () => {
    expect(() => applyFolds(records(), [{ primary: 'x', summary: 's', members: ['a_one', 'a_one'] }], 'system_control'))
      .toThrow(/listed twice/);
  });

  it('refuses folding an already-folded record', () => {
    const once = applyFolds(records(), [FOLD_A], 'system_control');
    // 'fused_a' now declares routing.dispatchBy, so the is-itself-a-fold guard fires.
    expect(() => applyFolds(
      once,
      [{ primary: 'fused_b', selector: 'kind', summary: 's', members: { one: 'fused_a', two: 'keep' } }],
      'system_control',
    )).toThrow(/is itself a fold/);
  });

  it('keeps the primary member as the advertised pair when it is one of the members', () => {
    const folded = applyFolds(
      records(),
      [{ primary: 'a_one', selector: 'kind', summary: 's', members: { one: 'a_one', two: 'a_two' } }],
      'system_control',
    );
    const record = folded[0] as CapabilityRecordSource;
    expect(String(record.legacyIds[0]?.action)).toBe('a_one');
    expect(record.legacyIds[1]?.folded).toEqual({ kind: 'two' });
    expect((record.schemas.input.properties.kind as JsonObject).default).toBe('one');
    expect(record.schemas.input.required).not.toContain('kind');
  });

  it('requires the selector when the primary is a new name', () => {
    const record = foldTwo({}, {}, []);
    expect(record.schemas.input.required).toContain('which');
    expect((record.schemas.input.properties.which as JsonObject).default).toBeUndefined();
  });

  it('refuses a string-list alias member under a required selector (its old name could not validate)', () => {
    expect(() => foldTwo({}, {}, [], { ...FOLD, aliasMembers: ['legacy_extra'] })).toThrow();
  });

  it('pins a map-form alias member to the selector value its name implied', () => {
    const source = member('legacy_extra', {}, []);
    const members = [member('q_one', {}, []), member('q_two', {}, []), source];
    const folded = applyFolds(
      members,
      [{ ...FOLD, aliasMembers: { one: 'legacy_extra' } }],
      'system_control',
    );
    const record = folded[0] as CapabilityRecordSource;
    const aliasPair = record.legacyIds.find((legacy) => String(legacy.action) === 'legacy_extra');
    expect(aliasPair?.folded).toEqual({ which: 'one' });
    expect(() => createCapabilityRecord(record)).not.toThrow();
  });

  it('byName derives selector values from the actions themselves', () => {
    expect(byName(['a', 'b'])).toEqual({ a: 'a', b: 'b' });
  });
});
