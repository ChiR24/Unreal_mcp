import { createHash } from 'node:crypto';
import { z } from 'zod';

import { CapabilityAliasSchema, LegacyActionNameSchema, LegacyToolNameSchema } from '../identifiers.js';
import { migrationMap } from './migration-map.js';
import { generateAliases } from './alias-generation.js';
import type { MigrationEntry } from './types.js';

/**
 * Deterministic, undefined-free JSON stringify for hashing. Mirrors the
 * capability hashing convention (sorted keys) but tolerates optional fields
 * that are absent (omitted rather than serialized as `null`/`undefined`).
 */
function deterministicStringify(value: unknown): string {
  return JSON.stringify(sortKeys(stripUndefined(value)));
}

function stripUndefined(value: unknown): unknown {
  if (Array.isArray(value)) return value.map(stripUndefined);
  if (value !== null && typeof value === 'object') {
    const out: Record<string, unknown> = {};
    for (const [key, val] of Object.entries(value as Record<string, unknown>)) {
      if (val !== undefined) out[key] = stripUndefined(val);
    }
    return out;
  }
  return value;
}

function sortKeys(value: unknown): unknown {
  if (Array.isArray(value)) return value.map(sortKeys);
  if (value !== null && typeof value === 'object') {
    const out: Record<string, unknown> = {};
    for (const key of Object.keys(value as Record<string, unknown>).sort()) {
      out[key] = sortKeys((value as Record<string, unknown>)[key]);
    }
    return out;
  }
  return value;
}

/**
 * Schema-validated migration artifact.
 *
 * The artifact is deterministic: building it twice from the same sources yields
 * byte-identical JSON. Consumers (handlers, generated gateways) hash-match
 * against this artifact so a drifted or hand-edited map fails CI.
 */

const argumentTransformSchema = z.strictObject({
  kind: z.enum(['identity', 'rename', 'drop-unsupported']),
  renames: z.record(z.string(), z.string()),
  dropped: z.array(z.string())
});

const replacementGuidanceSchema = z.strictObject({
  canonicalId: z.string().optional(),
  reason: z.string(),
  nextCall: z.strictObject({
    operation: z.literal('execute'),
    tool: LegacyToolNameSchema,
    action: LegacyActionNameSchema
  })
});

const entrySchema = z.strictObject({
  legacyKey: z.string(),
  tool: LegacyToolNameSchema,
  action: LegacyActionNameSchema,
  disposition: z.enum(['canonical', 'alias', 'removed', 'non-translatable']),
  canonicalId: z.string().optional(),
  removal: z
    .strictObject({
      since: z.string(),
      guidance: z.string(),
      replacement: z.string().optional()
    })
    .optional(),
  nonTranslatable: z
    .strictObject({
      reason: z.string(),
      guidance: replacementGuidanceSchema
    })
    .optional(),
  argumentTransform: argumentTransformSchema.optional(),
  deprecation: z.strictObject({
    status: z.enum(['active', 'deprecated', 'removed']),
    window: z.string()
  })
});

const aliasSchema = z.strictObject({
  alias: CapabilityAliasSchema,
  canonicalId: z.string(),
  source: z.enum(['inventory-alias', 'lossy-replacement'])
});

export const MigrationArtifactSchema = z.strictObject({
  schemaVersion: z.literal('task20.migration.v1'),
  generatedAt: z.string(),
  occurrenceCount: z.number().int().nonnegative(),
  entryCount: z.number().int().nonnegative(),
  resolvedCanonicalCount: z.number().int().nonnegative(),
  typedRemovalCount: z.number().int().nonnegative(),
  nonTranslatableCount: z.number().int().nonnegative(),
  aliasCount: z.number().int().nonnegative(),
  conflictCount: z.number().int().nonnegative(),
  contentHash: z.string().regex(/^[0-9a-f]{64}$/),
  entries: z.array(entrySchema),
  aliases: z.array(aliasSchema),
  conflicts: z.array(
    z.strictObject({
      alias: CapabilityAliasSchema,
      canonicalIds: z.array(z.string())
    })
  )
});

export type MigrationArtifact = z.infer<typeof MigrationArtifactSchema>;
export type MigrationArtifactEntry = z.infer<typeof entrySchema>;

function entryToJson(entry: MigrationEntry): MigrationArtifactEntry {
  return {
    legacyKey: entry.legacyKey,
    tool: entry.tool,
    action: entry.action,
    disposition: entry.disposition,
    canonicalId: entry.canonicalId,
    removal: entry.removal,
    nonTranslatable: entry.nonTranslatable,
    argumentTransform: entry.argumentTransform
      ? {
          kind: entry.argumentTransform.kind,
          renames: { ...entry.argumentTransform.renames },
          dropped: [...entry.argumentTransform.dropped]
        }
      : undefined,
    deprecation: entry.deprecation
  };
}

export function buildMigrationArtifact(): MigrationArtifact {
  const entries = [...migrationMap.entries.values()].map(entryToJson);
  entries.sort((a, b) => (a.legacyKey < b.legacyKey ? -1 : a.legacyKey > b.legacyKey ? 1 : 0));

  const { aliases, conflicts } = generateAliases();

  const resolvedCanonicalCount = entries.filter(
    (e) => e.disposition === 'canonical' || e.disposition === 'alias'
  ).length;
  const typedRemovalCount = entries.filter((e) => e.disposition === 'removed').length;
  const nonTranslatableCount = entries.filter((e) => e.disposition === 'non-translatable').length;

  const payload = {
    schemaVersion: migrationMap.schemaVersion,
    generatedAt: migrationMap.generatedAt,
    occurrenceCount: migrationMap.occurrenceCount,
    entryCount: entries.length,
    resolvedCanonicalCount,
    typedRemovalCount,
    nonTranslatableCount,
    aliasCount: aliases.length,
    conflictCount: conflicts.length,
    entries,
    aliases: [...aliases],
    conflicts: conflicts.map((c) => ({ alias: c.alias, canonicalIds: [...c.canonicalIds] }))
  };

  const contentHash = createHash('sha256').update(deterministicStringify(payload)).digest('hex');

  const artifact: MigrationArtifact = { ...payload, contentHash };
  // Validate before returning; throws on schema drift.
  return MigrationArtifactSchema.parse(artifact);
}

export const migrationArtifact: MigrationArtifact = buildMigrationArtifact();
