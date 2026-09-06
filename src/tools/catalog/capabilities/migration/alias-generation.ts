import { compareAscii } from '../../../../utils/serialization/ordering.js';
import { CapabilityAliasSchema } from '../identifiers.js';
import type { CapabilityAlias } from '../identifiers.js';
import { migrationMap } from './migration-map.js';
import type { CanonicalCapabilityRef, LegacyKey } from './types.js';

/**
 * Alias generation helpers.
 *
 * Aliases are GENERATED from the migration map (derived from the audited
 * inventory), never hand-written into handlers. Each alias resolves
 * deterministically to exactly one canonical capability id; conflicting
 * aliases (the same alias string mapped to two different canonical ids) are
 * detected and reported so the generation fails loudly instead of silently.
 */

export type GeneratedAlias = {
  readonly alias: CapabilityAlias;
  readonly canonicalId: CanonicalCapabilityRef;
  readonly source: 'inventory-alias' | 'lossy-replacement';
};

export type AliasConflict = {
  readonly alias: CapabilityAlias;
  readonly canonicalIds: ReadonlyArray<CanonicalCapabilityRef>;
};

function aliasFromLegacyKey(key: LegacyKey): CapabilityAlias {
  // Legacy `tool::action` strings are valid lower-snake dotted ids already.
  return CapabilityAliasSchema.parse(key.replace('::', '.'));
}

export function generateAliases(): {
  readonly aliases: ReadonlyArray<GeneratedAlias>;
  readonly conflicts: ReadonlyArray<AliasConflict>;
} {
  const byAlias = new Map<CapabilityAlias, Set<CanonicalCapabilityRef>>();

  const add = (alias: CapabilityAlias, canonicalId: CanonicalCapabilityRef): void => {
    const set = byAlias.get(alias) ?? new Set<CanonicalCapabilityRef>();
    set.add(canonicalId);
    byAlias.set(alias, set);
  };

  for (const entry of migrationMap.entries.values()) {
    if (entry.disposition === 'alias' && entry.canonicalId) {
      add(aliasFromLegacyKey(entry.legacyKey), entry.canonicalId);
    }
  }

  const aliases: GeneratedAlias[] = [];
  const conflicts: AliasConflict[] = [];
  for (const [alias, canonicalSet] of byAlias) {
    const canonicalIds = [...canonicalSet];
    if (canonicalIds.length > 1) {
      conflicts.push({ alias: CapabilityAliasSchema.parse(alias), canonicalIds });
      continue;
    }
    aliases.push({
      alias,
      canonicalId: canonicalIds[0],
      source: 'inventory-alias'
    });
  }

  aliases.sort((a, b) => compareAscii(a.alias, b.alias));
  return { aliases, conflicts };
}
