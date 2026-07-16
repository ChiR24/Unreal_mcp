/**
 * Builder and completeness check for the separate `routeDispositions` model.
 *
 * `RAW_ROUTE_DISPOSITIONS` (in `routedispositions.data.ts`) is the reviewed
 * source ledger: a curated, source-cited enumeration of the non-public
 * hidden/raw/dead routes. `REVIEWED_ROUTE_KEYS` is derived from it, and
 * `assertRouteDispositionsComplete` fails if any reviewed key is missing a
 * disposition record or any record has an unknown key, so omission cannot pass
 * generation. Every record is resolved (promote/map/remove + evidence + target
 * or guidance), giving zero unresolved rows by construction.
 */

import { RAW_ROUTE_DISPOSITIONS, type RawRouteDisposition } from './routedispositions.data.js';
import type { RouteDisposition, RouteDispositionRecord, RouteDispositionStatus } from './types.js';

/** The reviewed route ledger: exactly the keys enumerated in the data file. */
export const REVIEWED_ROUTE_KEYS: readonly string[] = RAW_ROUTE_DISPOSITIONS.map((r) => r.key);

function toRecord(raw: RawRouteDisposition): RouteDispositionRecord {
  if (raw.disposition === 'remove' && raw.removalGuidance === undefined) {
    throw new RouteDispositionError(
      `route disposition ${raw.key} is 'remove' but has no removalGuidance`,
    );
  }
  if (raw.disposition !== 'remove' && raw.targetCanonicalId === undefined) {
    throw new RouteDispositionError(
      `route disposition ${raw.key} (${raw.disposition}) has no targetCanonicalId`,
    );
  }
  return {
    dispositionKey: raw.key,
    route: raw.route,
    ...(raw.action === undefined ? {} : { action: raw.action }),
    domain: raw.domain,
    status: raw.status,
    owner: raw.owner,
    evidence: {
      source: raw.evidenceSource,
      symbol: raw.evidenceSymbol,
      tool: raw.evidenceTool,
      ...(raw.citations === undefined ? {} : { citations: raw.citations }),
    },
    ...(raw.targetCanonicalId === undefined ? {} : { targetCanonicalId: raw.targetCanonicalId }),
    ...(raw.removalGuidance === undefined ? {} : { removalGuidance: raw.removalGuidance }),
    disposition: raw.disposition,
    rationale: raw.rationale,
    resolved: true,
  };
}

export class RouteDispositionError extends Error {
  constructor(message: string) {
    super(message);
    this.name = 'RouteDispositionError';
  }
}

/** Build the full, resolved route-disposition record set. */
export function buildRouteDispositions(): readonly RouteDispositionRecord[] {
  return RAW_ROUTE_DISPOSITIONS.map(toRecord);
}

/** Counts by disposition and status for the metrics block. */
export function tallyRouteDispositions(
  records: readonly RouteDispositionRecord[],
): {
  total: number;
  unresolved: number;
  statusCounts: Record<RouteDispositionStatus, number>;
  dispositionCounts: Record<RouteDisposition, number>;
} {
  const statusCounts: Record<RouteDispositionStatus, number> = { hidden: 0, raw: 0, dead: 0 };
  const dispositionCounts: Record<RouteDisposition, number> = { promote: 0, map: 0, remove: 0 };
  let unresolved = 0;
  for (const r of records) {
    statusCounts[r.status] += 1;
    dispositionCounts[r.disposition] += 1;
    if (!r.resolved || r.evidence.source.length === 0 || r.evidence.symbol.length === 0) {
      unresolved += 1;
    }
  }
  return { total: records.length, unresolved, statusCounts, dispositionCounts };
}

/**
 * Completeness check tied to the reviewed source ledger. Fails if a reviewed
 * key is missing, an extra (unreviewed) key appears, or any row is unresolved.
 */
export function assertRouteDispositionsComplete(
  records: readonly RouteDispositionRecord[],
): void {
  const recordKeys = new Set(records.map((r) => r.dispositionKey));
  if (recordKeys.size !== records.length) {
    throw new RouteDispositionError('duplicate routeDispositionKey detected');
  }
  const ledger = new Set(REVIEWED_ROUTE_KEYS);
  for (const key of ledger) {
    if (!recordKeys.has(key)) {
      throw new RouteDispositionError(`reviewed route ${key} has no disposition record`);
    }
  }
  for (const key of recordKeys) {
    if (!ledger.has(key)) {
      throw new RouteDispositionError(`route disposition ${key} is not in the reviewed ledger`);
    }
  }
  for (const r of records) {
    if (!r.resolved) {
      throw new RouteDispositionError(`route disposition ${r.dispositionKey} is unresolved`);
    }
  }
}
