import type { CapabilityId } from '../identifiers.js';
import { ALL_CAPABILITY_RECORDS } from '../records/aggregate.js';
import { UnmappedLegacyPairError } from './types.js';
import type { LegacyKey } from './types.js';

/**
 * Legacy `{tool, action}` -> the live capability record that pair selects.
 *
 * The normalization inventory carries a Task 5 ANALYSIS reference per
 * occurrence (`cap:<namespace>:<action>`) that no capability record ever
 * shipped, and five of those references (`cap:shared:*`) are claimed by two
 * different legacy pairs that resolve to two distinct records. Keying by the
 * stale reference is therefore impossible; the legacy pair is the only total,
 * injective key into the live catalog.
 *
 * Targets come from the hand-authored record aggregate rather than the
 * generated registry, so the canonical-registry generator can import the
 * migration map without depending on its own output.
 *
 * Last-writer-wins on a duplicate pair mirrors `buildExecuteTargetIndex`, so a
 * migration target can never disagree with the record execute would dispatch.
 */
const CANONICAL_TARGETS: ReadonlyMap<LegacyKey, CapabilityId> = (() => {
  const targets = new Map<LegacyKey, CapabilityId>();
  for (const record of ALL_CAPABILITY_RECORDS) {
    for (const legacy of record.legacyIds) {
      targets.set(`${legacy.tool}::${legacy.action}`, record.id);
    }
  }
  return targets;
})();

export function requireCanonicalTarget(legacyKey: LegacyKey): CapabilityId {
  const target = CANONICAL_TARGETS.get(legacyKey);
  if (target === undefined) throw new UnmappedLegacyPairError(legacyKey);
  return target;
}
