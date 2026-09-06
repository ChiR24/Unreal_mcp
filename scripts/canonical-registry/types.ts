// scripts/canonical-registry/types.ts
//
// Shared types and low-level helpers for the Task-23 canonical registry
// generator. No side effects; pure functions only.

import { createHash } from 'node:crypto';
import type { CapabilityRecord } from '../../src/tools/catalog/capabilities/model.js';
import type { ToolDefinition } from '../../src/tools/definitions/shared/tool-definition.js';
import { sortById } from '../../src/utils/serialization/ordering.js';
export { sortById } from '../../src/utils/serialization/ordering.js';

export type JsonSchemaNode = Record<string, unknown>;

export interface CanonicalRecordSummary {
  readonly id: string;
  readonly parentTool: string;
  readonly dispatchAction: string;
  readonly domain: string;
  readonly schemaHash: string;
  readonly contentHash: string;
}

export const sha256Hex = (text: string): string =>
  createHash('sha256').update(text, 'utf8').digest('hex');

// Escape a value for a C++ double-quoted string literal (TEXT("...")).
export const cppStringLiteral = (value: string): string =>
  value
    .replace(/\\/g, '\\\\')
    .replace(/"/g, '\\"')
    .replace(/\n/g, '\\n')
    .replace(/\r/g, '\\r')
    .replace(/\t/g, '\\t');

// Convert a tool name such as "manage_level_structure" to "ManageLevelStructure".
export const pascalCase = (name: string): string =>
  name
    .split(/[_-]/)
    .map((part) => part.charAt(0).toUpperCase() + part.slice(1))
    .join('');

export const buildRecordSummaries = (
  records: readonly CapabilityRecord[],
): CanonicalRecordSummary[] =>
  sortById(records).map((r) => ({
    id: r.id,
    parentTool: r.routing.parentTool,
    dispatchAction: r.routing.dispatchAction,
    domain: r.discovery.domain,
    schemaHash: r.hashes.schema,
    contentHash: r.hashes.content,
  }));

export const buildLexicalIndex = (
  records: readonly CapabilityRecord[],
): Record<string, string[]> => {
  const index: Record<string, string[]> = {};
  for (const r of records) {
    const tokens = new Set<string>();
    tokens.add(r.id);
    tokens.add(r.routing.parentTool);
    tokens.add(r.routing.dispatchAction);
    tokens.add(r.discovery.domain);
    tokens.add(r.discovery.family);
    for (const t of r.discovery.topics) tokens.add(t);
    for (const w of r.discovery.summary.split(/\s+/)) {
      const clean = w.toLowerCase().replace(/[^a-z0-9_]/g, '');
      if (clean.length > 2) tokens.add(clean);
    }
    index[r.id] = [...tokens].sort();
  }
  return index;
};

export interface MigrationEntry {
  readonly legacyKey: string;
  readonly canonicalId: string | null;
  readonly disposition: string;
}

export interface MigrationMapLike {
  readonly entries: ReadonlyMap<string, { readonly canonicalId?: string | null; readonly disposition: string }>;
}

export const buildMigrationData = (
  map: MigrationMapLike,
): { schemaVersion: string; entryCount: number; entries: MigrationEntry[] } => {
  const list: MigrationEntry[] = [...map.entries.entries()]
    .map(([legacyKey, entry]) => ({
      legacyKey,
      canonicalId: entry.canonicalId ?? null,
      disposition: entry.disposition,
    }))
    .sort((a, b) => (a.legacyKey < b.legacyKey ? -1 : 1));
  return {
    schemaVersion: 'task20.migration.v1',
    entryCount: list.length,
    entries: list,
  };
};

export interface AliasData {
  readonly aliasCount: number;
  readonly conflictCount: number;
  readonly conflicts: ReadonlyArray<{ alias: string; canonicalIds: readonly string[] }>;
  readonly aliases: ReadonlyArray<{ alias: string; canonicalId: string; source: string }>;
}

export const buildAliasData = (
  generateAliases: () => {
    readonly aliases: ReadonlyArray<{ alias: string; canonicalId: string; source: string }>;
    readonly conflicts: ReadonlyArray<{ alias: string; canonicalIds: readonly string[] }>;
  },
): AliasData => {
  const { aliases, conflicts } = generateAliases();
  return {
    aliasCount: aliases.length,
    conflictCount: conflicts.length,
    conflicts: conflicts.map((c) => ({ alias: c.alias, canonicalIds: [...c.canonicalIds] })),
    aliases: [...aliases]
      .sort((a, b) => (a.alias < b.alias ? -1 : 1))
      .map((a) => ({ alias: a.alias, canonicalId: a.canonicalId, source: a.source })),
  };
};

export interface DocEntry {
  readonly name: string;
  readonly category: string | null;
  readonly description: string;
  readonly actionCount: number;
}

// Count the parent's action enum; counts the real `properties.action.enum`.
const isRecordLike = (value: unknown): value is Record<string, unknown> =>
  typeof value === 'object' && value !== null && !Array.isArray(value);

const isStringArray = (value: unknown): value is readonly string[] => {
  if (!Array.isArray(value)) return false;
  return value.every((item) => typeof item === 'string');
};

const deriveActionCount = (parent: ToolDefinition): number => {
  const inputSchema = parent.inputSchema;
  if (!isRecordLike(inputSchema)) return 0;
  const properties = inputSchema.properties;
  if (!isRecordLike(properties)) return 0;
  const action = properties.action;
  if (!isRecordLike(action)) return 0;
  const actionEnum = action.enum;
  return isStringArray(actionEnum) ? actionEnum.length : 0;
};

export const buildDocsData = (parents: readonly ToolDefinition[]): DocEntry[] =>
  parents.map((p) => ({
    name: p.name,
    category: p.category ?? null,
    description: p.description,
    actionCount: deriveActionCount(p),
  }));

// Sort every capability record by canonical id exactly once (stable, no
// mutable reference shared downstream). The same ordering drives the TS data
// module, the neutral JSON, and the native capability shards so all three
// derived artifacts stay deterministically aligned.
export const buildSortedRecords = (
  records: readonly CapabilityRecord[],
): readonly CapabilityRecord[] => sortById(records);
