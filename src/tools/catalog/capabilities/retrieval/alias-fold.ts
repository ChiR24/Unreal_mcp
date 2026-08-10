import type { CapabilityRecord } from '../model.js';

/**
 * Derives the alias -> primary relation from each record's OWN declared
 * normalization rationale. The relation is read from the catalog, never from an
 * evaluation corpus or a hand-kept list, so it stays correct as records change.
 *
 * It exists because a rationale-declared alias is a second document describing
 * one capability. Left in the index it competes with its own primary for the
 * same query and frequently outranks it, which is a ranking artifact of the
 * catalog's shape rather than a real distinction between two capabilities.
 */
const ALIAS_RATIONALE = /\balias (?:of|for)\s+([A-Za-z0-9_.]+)/i;

export type AliasFold = {
  /** alias capability id -> the primary capability id it defers to. */
  readonly targets: ReadonlyMap<string, string>;
  /** primary capability id -> the alias records it absorbs. */
  readonly absorbed: ReadonlyMap<string, readonly CapabilityRecord[]>;
};

function resolveTarget(
  record: CapabilityRecord,
  declared: string,
  known: ReadonlySet<string>,
): string | undefined {
  const trimmed = declared.replace(/\.+$/u, '');
  if (known.has(trimmed)) return trimmed;
  // A bare action name is qualified against the alias's own namespace, which is
  // the only namespace its rationale can be referring to.
  const namespace = String(record.id).split('.').slice(0, -1).join('.');
  const qualified = namespace.length === 0 ? trimmed : `${namespace}.${trimmed}`;
  return known.has(qualified) ? qualified : undefined;
}

export function deriveAliasFold(records: readonly CapabilityRecord[]): AliasFold {
  const known = new Set<string>(records.map((record) => String(record.id)));
  const targets = new Map<string, string>();
  for (const record of records) {
    const match = ALIAS_RATIONALE.exec(record.normalization.rationale);
    if (match === null) continue;
    const declared = match[1];
    if (declared === undefined) continue;
    const target = resolveTarget(record, declared, known);
    if (target === undefined || target === String(record.id)) continue;
    targets.set(String(record.id), target);
  }
  // An alias whose target is itself an alias would otherwise leave a dangling
  // document, so every chain is walked to the primary that ends it.
  for (const [alias] of targets) {
    const seen = new Set<string>([alias]);
    let target = targets.get(alias);
    while (target !== undefined && targets.has(target) && !seen.has(target)) {
      seen.add(target);
      target = targets.get(target);
    }
    if (target !== undefined) targets.set(alias, target);
  }
  const absorbed = new Map<string, CapabilityRecord[]>();
  for (const record of records) {
    const target = targets.get(String(record.id));
    if (target === undefined) continue;
    const bucket = absorbed.get(target);
    if (bucket === undefined) absorbed.set(target, [record]);
    else bucket.push(record);
  }
  return Object.freeze({ targets, absorbed });
}

export function canonicalCapabilityId(fold: AliasFold, id: string): string {
  return fold.targets.get(id) ?? id;
}
