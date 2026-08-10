/**
 * tests/unit/canonical-registry-parent-derivation.test.ts
 *
 * Proves the Task-23 parent derivation contract: the 23 parent
 * ToolDefinitions come EXCLUSIVELY from the 1,335 CapabilityRecords, their
 * action enums follow the canonical record SEQUENCE (never alphabetised),
 * their schemas are a deterministic permissive union of the exact per-action
 * record properties, and the generator's bootstrap stays acyclic (it imports
 * no facade, no generated artifact, and no hand-authored base).
 */
import { describe, expect, it } from 'vitest';
import { readFileSync } from 'node:fs';
import { deriveParents } from '../../scripts/canonical-registry/parent-derivation.js';
import { mergePropertyUnion } from '../../scripts/canonical-registry/schema-merge.js';
import { buildTargets } from '../../scripts/canonical-registry/targets.js';
import { buildSortedRecords } from '../../scripts/canonical-registry/types.js';
import { consolidatedToolDefinitions } from '../../src/tools/catalog/consolidated-tool-definitions.js';
import { loadAllCapabilityRecords } from '../../scripts/qa/capability-metadata-audit.js';
import type { CapabilityRecord } from '../../src/tools/catalog/capabilities/model.js';
import type { JsonSchemaNode } from '../../scripts/canonical-registry/types.js';

const PARENT_META = { description: 'fixture parent', category: 'core' as const };

type DerivedParent = ReturnType<typeof deriveParents>[number];

const actionsOf = (parent: DerivedParent): readonly string[] =>
  (parent.inputSchema.properties as Record<string, { enum?: readonly string[] }>).action.enum ?? [];

const alphabetised = (actions: readonly string[]): readonly string[] =>
  [...actions].sort((a, b) => a.localeCompare(b));

const inputPropsWithoutAction = (parent: DerivedParent): Record<string, unknown> => {
  const props = { ...(parent.inputSchema.properties as Record<string, unknown>) };
  delete props.action;
  return props;
};

const firstSeenActions = (recs: readonly CapabilityRecord[], parentTool: string): readonly string[] => [
  ...new Set(recs.filter((r) => r.routing.parentTool === parentTool).flatMap((r) => r.legacyIds.map((l) => l.action))),
];

function record(
  id: string,
  action: string,
  inputProps: Record<string, JsonSchemaNode>,
  outputProps: Record<string, JsonSchemaNode>,
): CapabilityRecord {
  return {
    id,
    parent: PARENT_META,
    routing: { parentTool: 'fixture_parent', dispatchAction: action as never },
    legacyIds: [{ tool: 'fixture_parent' as never, action: action as never }],
    discovery: { domain: 'fixture', family: 'fixture', topics: [], summary: '', whenToUse: [], whenNotToUse: [] },
    schemas: {
      input: { $schema: 'https://json-schema.org/draft/2020-12/schema', type: 'object', properties: inputProps, required: ['action'], additionalProperties: false },
      output: { $schema: 'https://json-schema.org/draft/2020-12/schema', type: 'object', properties: outputProps, required: [], additionalProperties: true },
    },
    examples: [],
    normalization: { class: 'C' as never, disposition: 'retain' as never, rationale: '' },
    deprecation: { status: 'active' as never, since: null, replacedBy: null, rationale: null },
    availability: { unreal: { min: { major: 5, minor: 0, patch: 0, channel: 'stable', preview: undefined }, max: { major: 5, minor: 8, patch: 0, channel: 'preview', preview: 1 } }, editorStates: [], requiredPlugins: [] },
    behavior: { effect: 'read' as never, idempotent: true, consent: 'none' as never, longRunning: false, retryable: true },
    policy: { requiredScope: 'local' as never, destructive: false, safeToRetry: true },
    cost: { complexity: 'low' as never, latency: 'low' as never, compute: 'low' as never },
    aliases: [],
    hashes: { schema: '0'.repeat(64), content: '0'.repeat(64) },
  } as unknown as CapabilityRecord;
}

describe('generator bootstrap is acyclic (no facade/generated/base import)', () => {
  it('generate-canonical-registry.ts does not import the consolidated facade or generated parent defs', () => {
    const src = readFileSync('./scripts/generate-canonical-registry.ts', 'utf8');
    const importLines = src.split('\n').filter((l) => /^\s*import\s/.test(l)).join('\n');
    expect(importLines).not.toMatch(/consolidated-tool-definitions/);
    expect(importLines).not.toMatch(/parent-tool-definitions\.generated/);
    expect(src).not.toMatch(/from '.*consolidated-tool-definitions\.js'/);
    expect(src).not.toMatch(/from '.*parent-tool-definitions\.generated\.js'/);
  });

  it('parent-derivation.ts imports neither the facade nor the generated artifact nor allToolDefinitions', () => {
    const src = readFileSync('./scripts/canonical-registry/parent-derivation.ts', 'utf8');
    expect(src).not.toMatch(/consolidated-tool-definitions/);
    expect(src).not.toMatch(/parent-tool-definitions\.generated/);
    expect(src).not.toMatch(/all-tool-definitions/);
  });
});

describe('deriveParents is record-only and deterministic', () => {
  const recs: CapabilityRecord[] = [
    record('a1', 'do_one', { target: { type: 'string' } }, { ok: { type: 'boolean' } }),
    record('a2', 'do_two', { target: { type: 'string' } }, { ok: { type: 'boolean' } }),
  ];

  it('produces exactly one parent per distinct parentTool', () => {
    const parents = deriveParents(recs);
    expect(parents.length).toBe(1);
    expect(parents[0].name).toBe('fixture_parent');
  });

  it('takes name/category/description from record parent metadata', () => {
    const [parent] = deriveParents(recs);
    expect(parent.category).toBe('core');
    expect(parent.description).toBe('fixture parent');
  });

  it('action is a direct string enum (no anyOf) and includes every legacy action', () => {
    const [parent] = deriveParents(recs);
    const action = (parent.inputSchema.properties as Record<string, { anyOf?: unknown; enum?: readonly string[] }>).action;
    expect(action.anyOf).toBeUndefined();
    expect(Array.isArray(action.enum)).toBe(true);
    expect([...(action.enum ?? [])].sort()).toEqual(['do_one', 'do_two']);
  });

  it('input required is only action; additionalProperties is permissive', () => {
    const [parent] = deriveParents(recs);
    expect(parent.inputSchema.required).toEqual(['action']);
    expect(parent.inputSchema.additionalProperties).toBe(true);
  });

  it('derived parent carries no params', () => {
    const [parent] = deriveParents(recs);
    expect((parent.inputSchema.properties as Record<string, unknown>).params).toBeUndefined();
  });

  it('output union applies no per-action required constraints', () => {
    const [parent] = deriveParents(recs);
    const output = parent.outputSchema as Record<string, unknown> | undefined;
    expect(output?.required).toEqual(undefined);
    expect(output?.additionalProperties).toBe(true);
  });

  it('output is byte-identical across runs (deterministic)', () => {
    const a = JSON.stringify(deriveParents(recs));
    const b = JSON.stringify(deriveParents(recs));
    expect(a).toEqual(b);
  });

  it('throws when a record parent is not a valid legacy tool name', () => {
    const bad: CapabilityRecord[] = [
      record('x', 'act', {}, {}),
    ];
    (bad[0] as unknown as { routing: { parentTool: string } }).routing.parentTool = '1bad_parent';
    expect(() => deriveParents(bad)).toThrow();
  });
});

describe('action enum order follows the canonical record sequence', () => {
  const sequenced: CapabilityRecord[] = [
    record('z1', 'zeta_action', {}, {}),
    record('a1', 'alpha_action', {}, {}),
    record('m1', 'mid_action', {}, {}),
  ];

  it('preserves first-seen action order instead of alphabetising', () => {
    const actions = actionsOf(deriveParents(sequenced)[0]);
    expect(actions).toEqual(['zeta_action', 'alpha_action', 'mid_action']);
    expect(actions).not.toEqual(alphabetised(actions));
  });

  it('honours the caller sequence, so an id-sorted view yields a different enum', () => {
    const idSorted = [...sequenced].sort((a, b) => (a.id < b.id ? -1 : 1));
    expect(actionsOf(deriveParents(idSorted)[0])).toEqual(['alpha_action', 'mid_action', 'zeta_action']);
    expect(actionsOf(deriveParents(sequenced)[0])).toEqual(['zeta_action', 'alpha_action', 'mid_action']);
  });

  it('keeps the parent list name-sorted regardless of record sequence', () => {
    const recs: CapabilityRecord[] = [record('r1', 'act_one', {}, {}), record('r2', 'act_two', {}, {})];
    (recs[0] as unknown as { routing: { parentTool: string } }).routing.parentTool = 'z_parent';
    expect(deriveParents(recs).map((p) => p.name)).toEqual(['fixture_parent', 'z_parent']);
  });

  it('every real parent enum equals its first-seen record order, and some are non-alphabetical', () => {
    const recs = loadFixtureRecords();
    const parents = deriveParents(recs);
    for (const p of parents) expect(actionsOf(p)).toEqual(firstSeenActions(recs, p.name));
    const nonAlphabetical = parents.filter((p) => actionsOf(p).join() !== alphabetised(actionsOf(p)).join());
    expect(nonAlphabetical.length).toBeGreaterThan(0);
  });

  it('record sequence changes the action enum only, never the property unions', () => {
    const recs = loadFixtureRecords();
    const natural = deriveParents(recs);
    const idSorted = deriveParents(buildSortedRecords(recs));
    expect(idSorted.map((p) => p.name)).toEqual(natural.map((p) => p.name));
    for (const [i, p] of natural.entries()) {
      expect(idSorted[i].outputSchema).toEqual(p.outputSchema);
      expect(inputPropsWithoutAction(idSorted[i])).toEqual(inputPropsWithoutAction(p));
    }
  });
});

describe('conflict-shape resolution (deterministic oneOf, never anyOf)', () => {
  it('scalar vs object conflict becomes a sorted oneOf', () => {
    const a = { value: { type: 'string' } };
    const b = { value: { type: 'object', properties: { n: { type: 'number' } } } };
    const merged = mergePropertyUnion([a, b]);
    const union = merged.value as { oneOf?: JsonSchemaNode[] };
    expect(union.oneOf).toBeDefined();
    expect((union as { anyOf?: unknown }).anyOf).toBeUndefined();
    expect(union.oneOf?.length).toBe(2);
  });

  it('color array vs object conflict resolves to a two-branch oneOf', () => {
    const arr = { color: { type: 'array', items: { type: 'number' } } };
    const obj = { color: { type: 'object', properties: { r: { type: 'number' } } } };
    const merged = mergePropertyUnion([arr, obj]);
    const union = merged.color as { oneOf?: JsonSchemaNode[] };
    expect(union.oneOf).toHaveLength(2);
  });

  // Overlapping `oneOf` branches are exactly-one, so a value matching two of
  // them is rejected; a pure-type union must collapse to a `type` list to stay
  // a valid accept-any-branch constraint.
  it('nullable / string / number type conflict unions all three branches', () => {
    const a = { v: { type: 'string' } };
    const b = { v: { type: 'number' } };
    const c = { v: { type: 'null' } };
    const merged = mergePropertyUnion([a, b, c]);
    expect(merged.v).toEqual({ type: ['null', 'number', 'string'] });
    expect((merged.v as { oneOf?: unknown }).oneOf).toBeUndefined();
  });

  it('freeform object boundary is preserved (no extra property schema introduced)', () => {
    const a = { meta: { type: 'object', additionalProperties: true } };
    const merged = mergePropertyUnion([a]);
    expect(merged.meta).toEqual({ type: 'object', additionalProperties: true });
  });

  it('output conflicts also become a union (not dropped)', () => {
    const recs: CapabilityRecord[] = [
      record('o1', 'act_a', {}, { result: { type: 'string' } }),
      record('o2', 'act_b', {}, { result: { type: 'number' } }),
    ];
    const [parent] = deriveParents(recs);
    const output = parent.outputSchema as Record<string, JsonSchemaNode> | undefined;
    expect(output?.properties.result).toEqual({ type: ['number', 'string'] });
  });

  it('identical shapes across records are not widened', () => {
    const a = { target: { type: 'string' } };
    const b = { target: { type: 'string' } };
    const merged = mergePropertyUnion([a, b]);
    expect(merged.target).toEqual({ type: 'string' });
    expect((merged.target as { oneOf?: unknown }).oneOf).toBeUndefined();
  });
});

describe('generated parent defs equal record-derived parents (end-to-end)', () => {
  const recs = loadFixtureRecords();
  const emitted = emittedParentDefs(recs);

  it('parent target content deep-equals deriveParents(records)', () => {
    expect(emitted).toEqual(deriveParents(recs));
  });

  // The committed catalog happens to be authored in id order, so only a
  // resequenced catalog can tell "derived from the records" apart from
  // "derived from the globally sorted records".
  it('parent target derives from the record sequence, not the id-sorted view', () => {
    const resequenced = [...recs].reverse();
    const emittedResequenced = emittedParentDefs(resequenced);
    expect(emittedResequenced).toEqual(deriveParents(resequenced));
    expect(emittedResequenced).not.toEqual(deriveParents(buildSortedRecords(resequenced)));
  });

  it('runtime consolidated facade carries params while generated parents do not', () => {
    expect(consolidatedToolDefinitions.length).toBe(23);
    for (const p of consolidatedToolDefinitions) {
      expect((p.inputSchema.properties as Record<string, unknown>).params).toBeDefined();
    }
  });
});

function loadFixtureRecords(): readonly CapabilityRecord[] {
  return loadAllCapabilityRecords();
}

function emittedParentDefs(recs: readonly CapabilityRecord[]): unknown[] {
  const targets = buildTargets({
    records: recs,
    migrationMap: { entries: new Map() },
    generateAliases: () => ({ aliases: [], conflicts: [] }),
  });
  const parentTarget = targets.find(([p]) => p.endsWith('parent-tool-definitions.generated.ts'));
  if (!parentTarget) throw new Error('parent target missing');
  const body = parentTarget[1].match(
    /generatedParentToolDefinitions:\s*readonly ToolDefinition\[\]\s*=\s*(\[[\s\S]*?\]);/,
  );
  if (!body) throw new Error('generated parent definitions body missing');
  return JSON.parse(body[1]) as unknown[];
}
