/**
 * tests/unit/canonical-registry-shards.test.ts
 *
 * Pure-function coverage for the Task-23 canonical registry grouping and
 * native C++ registry emission. No generated artifacts are written; we only
 * exercise the deterministic pure modules:
 *   - PARENT_GROUPS (every canonical parent in exactly one shard)
 *   - groupedParents (deterministic group + intra-shard order)
 *   - buildShardTargets (one .cpp per group, in group order)
 *   - buildRegistrySourceTargets (shards + schema fragments, in stem order)
 *   - buildAggregatorHeader / buildAggregatorSource (registration call order)
 *
 * These tests do not await the parallel full-data lane (1335 capability
 * records / live source modules). They run purely from the 23 canonical
 * parent ToolDefinitions, which are already available in consolidated-tool-definitions.
 */
import { describe, expect, it } from 'vitest';
import { consolidatedToolDefinitions } from '../../src/tools/catalog/consolidated-tool-definitions.js';
import {
  GENERATED_SHARD_STEMS,
  PARENT_GROUPS,
  SCHEMA_FRAGMENT_NAMESPACE,
  SCHEMA_FRAGMENT_SUFFIXES,
  assertGroupingComplete,
  assertShardPlanConsistent,
  classNameFor,
  groupedParents,
  schemaFragmentFunction,
} from '../../scripts/canonical-registry/grouping.js';
import { jsonSchemaToCppCalls } from '../../scripts/canonical-registry/cpp-schema.js';
import {
  buildShardTargets,
  buildAggregatorHeader,
  buildAggregatorSource,
  buildRegistrySourceTargets,
} from '../../scripts/canonical-registry/cpp-registry.js';
import type { ToolDefinition } from '../../src/tools/definitions/shared/tool-definition.js';

// The hand-written source is the fixture. Pure tests do not require the
// 1335-record capability lane.
const PARENTS: readonly ToolDefinition[] = consolidatedToolDefinitions;

describe('canonical registry grouping (pure)', () => {
  it('exposes exactly 23 canonical parents across uniquely named shard groups', () => {
    expect(PARENTS.length).toBe(23);
    expect(PARENT_GROUPS.length).toBeGreaterThan(0);
    expect(new Set(PARENT_GROUPS.map((g) => g.shard)).size).toBe(PARENT_GROUPS.length);
  });

  it('accepts the current shard plan', () => {
    expect(() => assertShardPlanConsistent()).not.toThrow();
  });

  it('assigns every one of the 23 parents to exactly one shard', () => {
    const flat = PARENT_GROUPS.flatMap((g) => g.parents);
    expect(flat.length).toBe(23);
    expect(new Set(flat).size).toBe(23);
    const parentNames = new Set(PARENTS.map((p) => p.name));
    for (const name of flat) {
      expect(parentNames.has(name), `shard plan references unknown parent "${name}"`).toBe(true);
    }
  });

  it('produces a deterministic group order via groupedParents', () => {
    const grouped = groupedParents(PARENTS);
    expect(grouped.map((g) => g.shard)).toEqual(PARENT_GROUPS.map((g) => g.shard));
    // Intra-shard order must follow PARENT_GROUPS, not input order.
    for (let i = 0; i < PARENT_GROUPS.length; i += 1) {
      const expected = PARENT_GROUPS[i].parents;
      const actual = grouped[i].parents.map((p) => p.name);
      expect(actual).toEqual(expected);
    }
  });

  it('builds one shard target per group in group order', () => {
    const shards = buildShardTargets(PARENTS);
    expect(shards.length).toBe(PARENT_GROUPS.length);
    expect(shards.map((s) => s.shard)).toEqual(PARENT_GROUPS.map((g) => g.shard));
    // Every shard emits a C++ file referencing the aggregator header.
    for (const s of shards) {
      expect(s.content).toContain('#include "MCP/Tools/McpGeneratedParentRegistry.h"');
      expect(s.content).toContain(`::RegisterGenerated${s.shard}Capabilities(`);
    }
  });
});

describe('canonical registry aggregator emission (pure)', () => {
  it('aggregator header declares one RegisterGenerated<Shard> entry per group in group order', () => {
    const header = buildAggregatorHeader();
    const calls = PARENT_GROUPS.map((g) => `RegisterGenerated${g.shard}Capabilities(`);
    let lastIndex = -1;
    for (const call of calls) {
      const idx = header.indexOf(call);
      expect(idx, `header must declare ${call}`).toBeGreaterThanOrEqual(0);
      expect(idx, 'header declarations must preserve group order').toBeGreaterThan(lastIndex);
      lastIndex = idx;
    }
  });

  it('aggregator source calls every shard registrar in group order', () => {
    const source = buildAggregatorSource();
    const calls = PARENT_GROUPS.map((g) => `RegisterGenerated${g.shard}Capabilities(FMcpToolRegistry::Get())`);
    let lastIndex = -1;
    for (const call of calls) {
      const idx = source.indexOf(call);
      expect(idx, `source must call ${call}`).toBeGreaterThanOrEqual(0);
      expect(idx, 'source registration must preserve group order').toBeGreaterThan(lastIndex);
      lastIndex = idx;
    }
    // The static self-registration block wires RegisterAll at static init.
    expect(source).toContain('FRegisterGeneratedParentTools');
    expect(source).toContain('static FRegisterGeneratedParentTools GRegisterGeneratedParentTools;');
  });
});

describe('canonical registry grouping failure modes (pure)', () => {
  const cloneParents = (): ToolDefinition[] => PARENTS.map((p) => ({ ...p }));

  it('assertGroupingComplete accepts the exact 23-parent set', () => {
    expect(() => assertGroupingComplete(PARENTS)).not.toThrow();
  });

  it('assertGroupingComplete rejects a foreign (unknown) parent', () => {
    const foreign: ToolDefinition = { ...cloneParents()[0], name: 'totally_unknown_tool' };
    expect(() => assertGroupingComplete([...cloneParents(), foreign])).toThrow(/not part of the canonical/);
  });

  it('assertGroupingComplete rejects a missing parent', () => {
    const missing = cloneParents();
    missing.pop();
    expect(missing.length).toBe(22);
    expect(() => assertGroupingComplete(missing)).toThrow(/missing canonical parent/);
  });

  it('assertGroupingComplete rejects a duplicated parent', () => {
    const dup = cloneParents();
    dup.push({ ...dup[0] });
    expect(() => assertGroupingComplete(dup)).toThrow(/appears more than once/);
  });
});

// Mirrors the plugin source-structure contract's counting rule exactly, so a
// pass here means the regenerated .cpp files pass tests/unit/plugin too.
const pureLineCount = (source: string): number =>
  source
    .split(/\r?\n/u)
    .filter((line) => !/^\s*$/u.test(line) && !/^\s*(?:#|\/\/)/u.test(line))
    .length;

const SPLIT_PLANS = PARENT_GROUPS.flatMap((group) =>
  group.splitSchemaParent === undefined
    ? []
    : [{ shard: group.shard, parent: group.splitSchemaParent }],
);

const sourceByStem = (): ReadonlyMap<string, string> =>
  new Map(buildRegistrySourceTargets(PARENTS).map((t) => [t.stem, t.content]));

const requireSource = (sources: ReadonlyMap<string, string>, stem: string): string => {
  const content = sources.get(stem);
  if (content === undefined) throw new Error(`no emitted source for stem "${stem}"`);
  return content;
};

describe('canonical registry schema split (pure)', () => {
  it('emits one source per generated stem, in stem order', () => {
    const sources = buildRegistrySourceTargets(PARENTS);
    expect(sources.map((s) => s.stem)).toEqual([...GENERATED_SHARD_STEMS]);
    expect(sources.length).toBe(
      PARENT_GROUPS.length + SPLIT_PLANS.length * SCHEMA_FRAGMENT_SUFFIXES.length,
    );
    expect(new Set(GENERATED_SHARD_STEMS).size).toBe(GENERATED_SHARD_STEMS.length);
  });

  it('keeps every emitted registry source within the 250 pure-line ceiling', () => {
    const oversized = buildRegistrySourceTargets(PARENTS)
      .map((s) => ({ stem: s.stem, pureLines: pureLineCount(s.content) }))
      .filter(({ pureLines }) => pureLines > 250)
      .map(({ stem, pureLines }) => `${pureLines} ${stem}`);
    expect(oversized).toEqual([]);
  });

  it('names every generated file so the plugin split-artifact contract accepts it', () => {
    const bannedName =
      /Common.*\.(?:cpp|cs|h)$|(?:^|[_-])Part(?:[_-]?\d+)?\.(?:cpp|cs|h)$|(?:^|[_-])\d+\.(?:cpp|cs|h)$|\.in[cl]$/u;
    for (const stem of GENERATED_SHARD_STEMS) {
      expect(bannedName.test(`McpGeneratedParentRegistry_${stem}.cpp`)).toBe(false);
    }
  });

  it('keeps tool identity, action enum and Required in the split parent shard', () => {
    const sources = sourceByStem();
    for (const plan of SPLIT_PLANS) {
      const shard = requireSource(sources, plan.shard);
      expect(shard).toContain(`GetName() const override { return TEXT("${plan.parent}"); }`);
      expect(shard).toContain('.StringEnum(TEXT("action")');
      expect(shard).toContain('Schema.Required(');
      expect(shard).toContain(`Registry.Register(new ${classNameFor(plan.parent)}());`);
    }
  });

  it('routes split parameters into namespace fragments the parity audit can resolve', () => {
    const sources = sourceByStem();
    const header = buildAggregatorHeader();

    for (const plan of SPLIT_PLANS) {
      for (const suffix of SCHEMA_FRAGMENT_SUFFIXES) {
        const fn = schemaFragmentFunction(plan.parent, suffix);
        expect(requireSource(sources, plan.shard)).toContain(
          `${SCHEMA_FRAGMENT_NAMESPACE}::${fn}(Schema);`,
        );
        expect(header).toContain(`void ${fn}(FMcpSchemaBuilder& Schema);`);

        const fragment = requireSource(sources, `${plan.shard}${suffix}`);
        // native-schema-sources.mjs resolves a cross-file helper by matching
        // `namespace <Name> {` and then the function name; a class static would
        // make the audit read an empty schema for this parent.
        expect(fragment).toMatch(new RegExp(`\\bnamespace\\s+${SCHEMA_FRAGMENT_NAMESPACE}\\s*\\{`, 'u'));
        expect(fragment).toContain(`void ${fn}(FMcpSchemaBuilder& Schema)`);
        expect(fragment).not.toContain('GetName() const override');
        expect(fragment).not.toContain('.StringEnum(TEXT("action")');
        expect(fragment).not.toContain('Registry.Register(');
      }
    }
  });

  it('preserves every emitted schema line of a split parent across shard and fragments', () => {
    const sources = sourceByStem();

    for (const plan of SPLIT_PLANS) {
      const parent = PARENTS.find((p) => p.name === plan.parent);
      expect(parent, `split parent "${plan.parent}" must be a canonical parent`).toBeDefined();
      if (!parent) continue;

      const unsplit = jsonSchemaToCppCalls(parent.inputSchema).lines;
      expect(unsplit.length).toBeGreaterThan(0);

      const combined = [
        requireSource(sources, plan.shard),
        ...SCHEMA_FRAGMENT_SUFFIXES.map((suffix) =>
          requireSource(sources, `${plan.shard}${suffix}`),
        ),
      ].join('\n');

      for (const line of unsplit) {
        expect(combined, `schema line lost in the split: ${line.trim().slice(0, 80)}`).toContain(line);
      }
    }
  });
});
