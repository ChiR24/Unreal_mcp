// scripts/canonical-registry/grouping.ts
//
// Deterministic, authoritative grouping of the 23 canonical parent tools
// into the native registry shards.
//
// Each parent MUST appear in exactly one group. Unknown or missing parent
// names are rejected (the generator refuses to emit a partial/foreign set).
//
// A parent whose parameter catalog alone would push its shard past the 250
// pure-LOC source ceiling opts into a schema split via `splitSchemaParent`:
// the parameters move into semantically named namespace fragments while the
// tool identity, action enum, and registration stay in the parent shard.
// Re-grouping parents cannot help such a parent -- build_environment is the
// only parent in its group and its schema body alone is ~244 lines -- so the
// split is intra-parent by necessity.
//
// GENERATED_SHARD_STEMS is the single source of truth for every emitted
// McpGeneratedParentRegistry_<stem>.cpp: target paths, stale-target metadata,
// and shard counts all derive from it.

import type { ToolDefinition } from '../../src/tools/definitions/shared/tool-definition.js';
import { pascalCase, type JsonSchemaNode } from './types.js';

export interface ParentGroup {
  // Stable shard suffix, e.g. "Core_Actor" -> McpGeneratedParentRegistry_Core_Actor.cpp
  readonly shard: string;
  // Ordered parent tool names that live in this shard.
  readonly parents: readonly string[];
  // Opt-in schema split. The named parent MUST be this group's only parent;
  // its parameters are emitted into one fragment per SCHEMA_FRAGMENT_SUFFIXES.
  readonly splitSchemaParent?: string;
}

// Responsibility suffixes for a split parent's schema fragments: single-line
// leaf parameters vs multi-line composite parameters. Deliberately semantic --
// the plugin source-structure contract rejects mechanical split artifacts
// (Part/Common/trailing-digit/.inl names).
export const SCHEMA_FRAGMENT_SUFFIXES = ['Fields', 'Structures'] as const;

export type SchemaFragmentSuffix = (typeof SCHEMA_FRAGMENT_SUFFIXES)[number];

export const PARENT_GROUPS: readonly ParentGroup[] = [
  { shard: 'Core_Actor', parents: ['control_actor', 'control_editor', 'inspect', 'manage_level'] },
  { shard: 'Core_Asset', parents: ['manage_asset'] },
  { shard: 'Core_Blueprint', parents: ['manage_blueprint', 'manage_tools'] },
  { shard: 'Core_System', parents: ['system_control', 'manage_networking'] },
  { shard: 'Gameplay_AI', parents: ['manage_ai', 'manage_interaction'] },
  { shard: 'Gameplay_Anim', parents: ['animation_physics', 'manage_character'] },
  { shard: 'Gameplay_Combat', parents: ['manage_combat', 'manage_effect'] },
  { shard: 'Gameplay_Sys', parents: ['manage_gas', 'manage_inventory'] },
  { shard: 'World_Environment', parents: ['build_environment'], splitSchemaParent: 'build_environment' },
  { shard: 'World_Geometry', parents: ['manage_geometry'] },
  { shard: 'World_Structure', parents: ['manage_level_structure', 'manage_pcg'] },
  { shard: 'Utility_Audio', parents: ['manage_audio'] },
  { shard: 'Utility_Sequence', parents: ['manage_sequence'] },
];

// Every emitted McpGeneratedParentRegistry_<stem>.cpp, in emission order: each
// shard followed by its schema fragments. Target paths, stale-target metadata
// and the emitted-file count all derive from this one list.
export const GENERATED_SHARD_STEMS: readonly string[] = PARENT_GROUPS.flatMap((group) =>
  group.splitSchemaParent === undefined
    ? [group.shard]
    : [group.shard, ...SCHEMA_FRAGMENT_SUFFIXES.map((suffix) => `${group.shard}${suffix}`)],
);

// Reject a shard plan whose split parents would produce ambiguous fragment
// stems or colliding file names. Throws before any file is written.
export const assertShardPlanConsistent = (): void => {
  const seenShards = new Set<string>();

  for (const group of PARENT_GROUPS) {
    if (seenShards.has(group.shard)) {
      throw new Error(`FATAL: duplicate shard name "${group.shard}" in the parent group plan.`);
    }
    seenShards.add(group.shard);

    const split = group.splitSchemaParent;
    if (split === undefined) continue;

    if (!group.parents.includes(split)) {
      throw new Error(
        `FATAL: shard "${group.shard}" splits the schema of "${split}", which is not one of ` +
          `its parents (${group.parents.join(', ')}).`,
      );
    }
    if (group.parents.length !== 1) {
      throw new Error(
        `FATAL: shard "${group.shard}" splits the schema of "${split}" but carries ` +
          `${group.parents.length} parents. A split parent must be alone in its shard so the ` +
          'fragment file stems stay unambiguous.',
      );
    }
  }

  if (new Set(GENERATED_SHARD_STEMS).size !== GENERATED_SHARD_STEMS.length) {
    throw new Error(
      `FATAL: generated shard stems are not unique: ${GENERATED_SHARD_STEMS.join(', ')}.`,
    );
  }
};

// Validate that the supplied parents exactly cover PARENT_GROUPS with no
// unknown and no missing names. Throws before any file is written.
export const assertGroupingComplete = (parents: readonly ToolDefinition[]): void => {
  assertShardPlanConsistent();

  const expected = new Set(PARENT_GROUPS.flatMap((g) => g.parents));
  const seen = new Set<string>();

  for (const p of parents) {
    if (!expected.has(p.name)) {
      throw new Error(
        `FATAL: parent tool "${p.name}" is not part of the canonical ${PARENT_GROUPS.length}-group plan. ` +
        'Refusing to generate a foreign/unknown parent.',
      );
    }
    if (seen.has(p.name)) {
      throw new Error(`FATAL: parent tool "${p.name}" appears more than once in the input.`);
    }
    seen.add(p.name);
  }

  const missing = [...expected].filter((n) => !seen.has(n));
  if (missing.length > 0) {
    throw new Error(
      `FATAL: missing canonical parent tools: ${missing.join(', ')}. ` +
        'Refusing to generate an incomplete registry.',
    );
  }
};

export const classNameFor = (name: string): string => `FMcpGenTool_${pascalCase(name)}`;

// Schema fragments live in a real C++ namespace, never as class statics: the
// native parity audit resolves a cross-file schema helper by matching
// `namespace <Name> {` (tests/audits/native-schema-sources.mjs), so a static
// member would make the audit read an empty schema for the split parent.
export const SCHEMA_FRAGMENT_NAMESPACE = 'McpGeneratedParentSchema';

export const schemaFragmentFunction = (
  parent: string,
  suffix: SchemaFragmentSuffix,
): string => `Append${pascalCase(parent)}${suffix}`;

export interface GroupedShard {
  readonly shard: string;
  readonly parents: readonly ToolDefinition[];
  readonly splitSchemaParent: string | undefined;
}

// Build the per-shard parent lists (in shard / intra-shard order), grouped
// from the input parents.
export const groupedParents = (
  parents: readonly ToolDefinition[],
): readonly GroupedShard[] =>
  PARENT_GROUPS.map((group) => ({
    shard: group.shard,
    parents: group.parents
      .map((n) => parents.find((p) => p.name === n))
      .filter((p): p is ToolDefinition => p !== undefined),
    splitSchemaParent: group.splitSchemaParent,
  }));

// The native parent registry class name. Exposed for the aggregator header.
export const REGISTRY_CLASS = 'FMcpGeneratedParentRegistry';

// Discriminate whether a parent enforces strict arguments. Currently only
// manage_sequence does; this is the authoritative source for that flag.
const strictParents = new Set<string>(['manage_sequence']);

export const isStrict = (name: string): boolean => strictParents.has(name);

// Re-export so the registry module does not re-import types from two roots.
export type { JsonSchemaNode };
