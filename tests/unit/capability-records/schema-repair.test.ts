/**
 * tests/unit/capability-records/schema-repair.test.ts
 *
 * TASK 29 - native legacy schema repair + comparator acceptance semantics.
 *
 * Two independent defects are locked here, and they are NOT the same defect:
 *
 *  1. GENERATOR NARROWING (a real bug, must be fixed in the emitter).
 *     `emitArrayNode` funnelled every non-object item shape into
 *     `Array(name, desc, TEXT("string"))`, so a canonical `items: {}` -- an
 *     array that accepts ANY item -- was emitted as an array of strings. The
 *     native legacy `BuildInputSchema` then REJECTED payloads the canonical
 *     contract accepts (`manage_networking.states`). The fix is an honest
 *     permissive builder path, not a comparator exception.
 *
 *  2. COMPARATOR BLINDNESS (a false positive, must be fixed in the audit).
 *     `oneOf: [{type:'string'}, {type:'string', enum:[...]}]` accepts exactly
 *     "any string": the unconstrained branch SUBSUMES its constrained
 *     siblings, so projecting it to `type: ["string"]` loses no accepted
 *     value. The old comparator read only `node.type` (undefined for a
 *     `oneOf` node) and reported four such projections as drift.
 *
 * The distinction is the whole point: acceptance-lossless projection passes,
 * real narrowing stays a hard failure. Widening is a failure too -- a native
 * `type: ["string"]` against a TS union that only accepts an enum subset is
 * NOT lossless, and the guards below prove it still fails.
 */
import { describe, expect, it } from 'vitest';
import { jsonSchemaToCppCalls } from '../../../scripts/canonical-registry/cpp-schema.js';
import { compareToolSchemas } from '../../audits/schema-contract.mjs';
import { isRecord } from '../../../src/utils/validation/type-guards.js';

const emit = (properties: Record<string, unknown>): string[] =>
  jsonSchemaToCppCalls({ type: 'object', properties }).lines;

const propertyOf = (schema: unknown, name: string): Record<string, unknown> | undefined => {
  if (!isRecord(schema) || !isRecord(schema.properties)) return undefined;
  const property = schema.properties[name];
  return isRecord(property) ? property : undefined;
};

/** Compare one property in isolation; `undefined` means full parity. */
const compareProperty = (
  typeScript: Record<string, unknown>,
  native: Record<string, unknown>,
): unknown =>
  compareToolSchemas(
    'alpha',
    { type: 'object', properties: { value: typeScript }, required: [] },
    { type: 'object', properties: { value: native }, required: [] },
  );

describe('task 29 - untyped array items keep a permissive native schema', () => {
  it('RED-then-GREEN: `items: {}` emits ArrayOfAny, never an array of strings', () => {
    // Given an array whose items carry no type at all (canonical `items: {}`)
    const lines = emit({ states: { type: 'array', description: 'states', items: {} } });

    // Then the emitter uses the permissive path and never invents `string`
    expect(lines.some((l) => l.includes('Schema.ArrayOfAny(TEXT("states")'))).toBe(true);
    expect(lines.some((l) => l.includes('Schema.Array(TEXT("states")'))).toBe(false);
  });

  it('GREEN: a missing `items` key is equally unconstrained', () => {
    // Given an array node with no `items` at all
    const lines = emit({ tags: { type: 'array', description: 'tags' } });

    // Then it is permissive, not string-typed
    expect(lines.some((l) => l.includes('Schema.ArrayOfAny(TEXT("tags")'))).toBe(true);
    expect(lines.some((l) => l.includes('TEXT("string")'))).toBe(false);
  });

  it('GREEN: an unconstrained array nested in an object emits onto the sub-builder', () => {
    // Given a permissive array inside an object sub-schema
    const lines = emit({
      settings: { type: 'object', properties: { states: { type: 'array', items: {} } } },
    });

    // Then it attaches to `S`, never to the captured outer `Schema`
    expect(lines.some((l) => l.trim().startsWith('S.ArrayOfAny(TEXT("states")'))).toBe(true);
    expect(lines.some((l) => l.includes('Schema.ArrayOfAny'))).toBe(false);
  });

  it('GUARD: typed and union item shapes keep their existing emission', () => {
    // Given typed, enum-constrained, and union item shapes
    const lines = emit({
      names: { type: 'array', items: { type: 'string' } },
      counts: { type: 'array', items: { type: 'integer' } },
      flags: { type: 'array', items: { type: 'boolean' } },
      kinds: { type: 'array', items: { enum: ['a', 'b'] } },
      mixed: { type: 'array', items: { type: ['string', 'number'] } },
      rows: { type: 'array', items: { type: 'object', properties: { id: { type: 'string' } } } },
    });

    // Then none of them is rerouted to the permissive path
    expect(lines.some((l) => l.includes('Schema.Array(TEXT("names"), TEXT(""), TEXT("string"))'))).toBe(true);
    expect(lines.some((l) => l.includes('Schema.Array(TEXT("counts"), TEXT(""), TEXT("integer"))'))).toBe(true);
    expect(lines.some((l) => l.includes('Schema.Array(TEXT("flags"), TEXT(""), TEXT("boolean"))'))).toBe(true);
    expect(lines.some((l) => l.includes('Schema.Array(TEXT("kinds"), TEXT(""), TEXT("string"))'))).toBe(true);
    expect(lines.some((l) => l.includes('Schema.Array(TEXT("mixed"), TEXT(""), TEXT("string"))'))).toBe(true);
    expect(lines.some((l) => l.includes('Schema.ArrayOfObjects(TEXT("rows")'))).toBe(true);
    expect(lines.some((l) => l.includes('ArrayOfAny'))).toBe(false);
  });
});

describe('task 29 - comparator understands oneOf acceptance semantics', () => {
  it('RED-then-GREEN: an open string branch subsuming enum branches is lossless', () => {
    // Given the `manage_blueprint.nodeType` shape: one unconstrained string
    // branch plus an enum-constrained sibling -- acceptance is "any string"
    const gap = compareProperty(
      {
        oneOf: [
          { type: 'string', description: 'Blueprint node type string.' },
          { type: 'string', enum: ['make', 'break'], description: 'Struct node kind.' },
        ],
      },
      { type: ['string'] },
    );

    // Then the native scalar projection accepts exactly the same values
    expect(gap).toBeUndefined();
  });

  it('GREEN: many open branches of one type collapse to that single type', () => {
    // Given the `manage_asset.path` shape: five unconstrained string branches
    const gap = compareProperty(
      { oneOf: [{ type: 'string' }, { type: 'string' }, { type: 'string' }] },
      { type: ['string'] },
    );

    // Then the projection is lossless
    expect(gap).toBeUndefined();
  });

  it('GREEN: multi-type open branches keep every accepted member', () => {
    // Given branches whose own `type` is itself a list
    const gap = compareProperty(
      { oneOf: [{ type: ['number', 'string'] }, { type: 'string' }] },
      { type: ['number', 'string'] },
    );

    // Then both sides accept numbers and strings
    expect(gap).toBeUndefined();
  });

  it('GUARD: enum-only branches are NOT subsumed - native widening still fails', () => {
    // Given a union with no unconstrained branch, so TS accepts only 4 values
    const gap = compareProperty(
      {
        oneOf: [
          { type: 'string', enum: ['a', 'b'] },
          { type: 'string', enum: ['c', 'd'] },
        ],
      },
      { type: ['string'] },
    );

    // Then projecting to "any string" changes what is accepted
    expect(gap).toBeDefined();
  });

  it('GUARD: an enum branch of a type with no open sibling still fails', () => {
    // Given an open `string` branch but a `number` branch limited to two values
    const gap = compareProperty(
      { oneOf: [{ type: 'string' }, { type: 'number', enum: [1, 2] }] },
      { type: ['number', 'string'] },
    );

    // Then the native side accepts every number, which the contract does not
    expect(gap).toBeDefined();
  });

  it('GUARD: a structural union is never flattened to a scalar type list', () => {
    // Given a branch carrying `properties` -- C++ cannot encode it as a type list
    const gap = compareProperty(
      { oneOf: [{ type: 'object', properties: { r: { type: 'number' } } }, { type: 'string' }] },
      { type: ['object', 'string'] },
    );

    // Then the comparator refuses the flattening
    expect(gap).toBeDefined();
  });

  it('GUARD: real array narrowing stays a hard failure', () => {
    // Given a permissive `items: {}` projected onto string items
    const gap = compareProperty(
      { type: 'array', items: {} },
      { type: 'array', items: { type: 'string' } },
    );

    // Then the narrowing is reported -- this is the defect, not an exception
    expect(gap).toBeDefined();
  });
});

/**
 * The repo-wide audit reads the CHECKED-IN generated C++, which still carries
 * the pre-repair `Array(..., TEXT("string"))` until the parent integrator runs
 * `registry:generate`. Regenerating is not this lane's to do, so parity is
 * proved against what the emitter PRODUCES -- generator -> native parser ->
 * comparator, end to end, with no stale artifact in the path.
 */
describe('task 29 - zero schema drift across every canonical parent', () => {
  it('GREEN: every parent emits a native schema the comparator finds identical', async () => {
    // Given the emitter, the native builder parser, and all 23 canonical parents
    const { parseBuilderSchema } = await import('../../audits/native-schema-parser.mjs');
    const { consolidatedToolDefinitions } = await import(
      '../../../src/tools/catalog/consolidated-tool-definitions.js'
    );

    // When each parent's schema is emitted to C++ and read back as the native side
    const gaps = consolidatedToolDefinitions
      .map((tool) => {
        const emitted = jsonSchemaToCppCalls({ ...tool.inputSchema }).lines.join('\n');
        return compareToolSchemas(tool.name, tool.inputSchema, parseBuilderSchema(emitted));
      })
      .filter((gap) => gap !== undefined);

    // Then discovery is non-vacuous and no parent drifts on any property
    expect(consolidatedToolDefinitions.length).toBe(23);
    expect(gaps).toEqual([]);
  });

  it('GREEN: manage_networking.states round-trips as permissive items', async () => {
    // Given the parent that carried the real narrowing
    const { parseBuilderSchema } = await import('../../audits/native-schema-parser.mjs');
    const { consolidatedToolDefinitions } = await import(
      '../../../src/tools/catalog/consolidated-tool-definitions.js'
    );
    const networking = consolidatedToolDefinitions.find((t) => t.name === 'manage_networking');

    // When its emitted C++ is parsed back
    const emitted = jsonSchemaToCppCalls({ ...networking?.inputSchema }).lines.join('\n');

    // Then the item schema is unconstrained on both sides, not narrowed to string
    expect(propertyOf(networking?.inputSchema, 'states')?.items).toEqual({});
    expect(propertyOf(parseBuilderSchema(emitted), 'states')).toEqual({ type: 'array', items: {} });
  });
});
