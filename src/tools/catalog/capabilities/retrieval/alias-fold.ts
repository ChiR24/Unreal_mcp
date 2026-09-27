import type { CapabilityRecord } from '../model.js';

/**
 * Maps each former capability id that a folded family absorbed (and still
 * declares as an alias) to that family, so search answers in the family's id.
 */
export type AliasFold = {
  /** alias capability id -> the primary capability id it defers to. */
  readonly targets: ReadonlyMap<string, string>;
};

export function deriveAliasFold(records: readonly CapabilityRecord[]): AliasFold {
  const known = new Set<string>(records.map((record) => String(record.id)));
  const targets = new Map<string, string>();
  for (const record of records) {
    // A folded family already absorbed its members: each former record id is
    // now one of this record's aliases, so it canonicalises to the family.
    const namespace = String(record.id).split('.').slice(0, -1).join('.');
    for (const legacy of record.legacyIds) {
      if (legacy.folded === undefined) continue;
      const former = namespace.length === 0 ? String(legacy.action) : `${namespace}.${String(legacy.action)}`;
      // Only a name the record still declares as an alias resolves; a former
      // id withheld from the alias list (a vocabulary collision) stays callable
      // by its legacy pair but is not a capability id any more.
      if (!known.has(former) && record.aliases.some((alias) => String(alias) === former)) {
        targets.set(former, String(record.id));
      }
    }
  }
  return Object.freeze({ targets });
}

export function canonicalCapabilityId(fold: AliasFold, id: string): string {
  return fold.targets.get(id) ?? id;
}
