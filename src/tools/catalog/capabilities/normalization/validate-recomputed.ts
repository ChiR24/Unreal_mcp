/**
 * Deterministic metric recomputation used by the top-level inventory
 * validator. Kept in its own module so `validate.ts` stays within the
 * 250 pure-line ceiling.
 */
import type {
  Classification,
  Disposition,
  InventoryMetrics,
  OccurrenceRecord,
  RouteDisposition,
  RouteDispositionRecord,
  RouteDispositionStatus,
} from './types.js';

/** Recompute metrics directly from the occurrence + route-disposition rows. */
export function recomputeMetrics(
  occurrences: readonly OccurrenceRecord[],
  routeDispositions: readonly RouteDispositionRecord[],
): InventoryMetrics {
  const byName = new Map<string, number>();
  for (const o of occurrences) {
    byName.set(o.action, (byName.get(o.action) ?? 0) + 1);
  }
  const duplicateGroups = [...byName.values()].filter((c) => c > 1);
  const duplicateNameOccurrences = duplicateGroups.reduce((s, c) => s + c, 0);
  const classificationCounts: Record<Classification, number> = {
    A: 0, B: 0, C: 0, D: 0, E: 0, F: 0,
  };
  const dispositionCounts: Record<Disposition, number> = {
    keep: 0, alias: 0, map: 0, promote: 0, remove: 0, review: 0,
  };
  for (const o of occurrences) {
    classificationCounts[o.classification] += 1;
    dispositionCounts[o.disposition] += 1;
  }
  const routeStatusCounts: Record<RouteDispositionStatus, number> = { hidden: 0, raw: 0, dead: 0 };
  const routeDispositionCounts: Record<RouteDisposition, number> = { promote: 0, map: 0, remove: 0 };
  let routeUnresolved = 0;
  for (const r of routeDispositions) {
    routeStatusCounts[r.status] += 1;
    routeDispositionCounts[r.disposition] += 1;
    if (!r.resolved || r.evidence.source.length === 0) routeUnresolved += 1;
  }
  return {
    occurrenceCount: occurrences.length,
    toolCount: new Set(occurrences.map((o) => o.tool)).size,
    distinctActionNames: byName.size,
    duplicateNames: duplicateGroups.length,
    duplicateNameOccurrences,
    maxExactNameReductions: duplicateNameOccurrences - duplicateGroups.length,
    actualCanonicalReductions: 0,
    verbFamilyAddCreateSetConfigure: occurrences.filter((o) =>
      ['add', 'create', 'set', 'configure'].includes(o.action.split('_')[0] ?? ''),
    ).length,
    unclassifiedOccurrences: occurrences.filter((o) => !['A', 'B', 'C', 'D', 'E', 'F'].includes(o.classification)).length,
    canonicalCollisions: 0,
    classificationCounts,
    dispositionCounts,
    routeDispositionTotal: routeDispositions.length,
    routeDispositionUnresolved: routeUnresolved,
    routeStatusCounts,
    routeDispositionCounts,
  };
}
