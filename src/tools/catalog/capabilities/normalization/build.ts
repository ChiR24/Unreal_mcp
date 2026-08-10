/**
 * Deterministic builder for the normalization inventory artifact.
 *
 * Given the authoritative occurrence extraction, this module assigns canonical
 * ids, A-F classifications, and primary/alias roles, then assembles the full
 * artifact with stable ordering (no wall-clock timestamps). The reviewed fixed
 * metrics are asserted against the live source: if the source drifts, building
 * FAILS with an evidence-backed error rather than forcing the counts. A separate
 * `routeDispositions` model enumerates the non-public hidden/raw/dead routes.
 */

import { createHash } from 'node:crypto';
import {
  isPreset,
  justificationFor,
  MERGE_SECONDARY_DISPOSITION,
  MERGED_SHARED_NAMES,
  mergedCanonicalId,
  namespaceOf,
  REVIEWED_METRICS,
  toolCanonicalId,
} from './adjudicate.js';
import {
  buildRawRoute,
  extractOccurrences,
  type RawOccurrence,
} from './extract.js';
import {
  assertRouteDispositionsComplete,
  buildRouteDispositions,
  tallyRouteDispositions,
} from './routedispositions.js';
import {
  type CanonicalDefinition,
  CLASSIFICATIONS,
  type Classification,
  type Disposition,
  INVENTORY_SCHEMA_VERSION,
  type InventoryMetrics,
  type NormalizationInventory,
  type OccurrenceRecord,
  type Role,
  VERB_FAMILY,
} from './types.js';
import type { CapabilityRecord } from '../model.js';
import { compareAscii as compareKeys } from '../../../../utils/serialization/ordering.js';

export class InventoryBuildError extends Error {
  constructor(message: string) {
    super(message);
    this.name = 'InventoryBuildError';
  }
}

const VERB_SET = new Set<string>(VERB_FAMILY);

interface Decision {
  readonly classification: Classification;
  readonly role: Role;
  readonly canonicalId: string;
  readonly disposition: Disposition;
}

function decideGroup(
  action: string,
  items: readonly RawOccurrence[],
): ReadonlyMap<RawOccurrence, Decision> {
  const map = new Map<RawOccurrence, Decision>();
  if (items.length === 1) {
    const [item] = items;
    const classification: Classification = isPreset(action) ? 'E' : 'C';
    map.set(item, {
      classification,
      role: 'primary',
      canonicalId: toolCanonicalId(item.tool, action),
      disposition: 'keep',
    });
    return map;
  }
  if (MERGED_SHARED_NAMES.has(action)) {
    const sorted = [...items].sort((a, b) => compareKeys(a.tool, b.tool));
    sorted.forEach((item, index) => {
      map.set(item, {
        classification: 'A',
        role: index === 0 ? 'primary' : 'alias',
        canonicalId: mergedCanonicalId(action),
        disposition: index === 0 ? 'keep' : MERGE_SECONDARY_DISPOSITION,
      });
    });
    return map;
  }
  for (const item of items) {
    map.set(item, {
      classification: 'C',
      role: 'primary',
      canonicalId: toolCanonicalId(item.tool, action),
      disposition: 'keep',
    });
  }
  return map;
}

function assertReviewedMetrics(metrics: InventoryMetrics): void {
  const checks: ReadonlyArray<readonly [string, number, number]> = [
    ['occurrenceCount', metrics.occurrenceCount, REVIEWED_METRICS.occurrenceCount],
    ['duplicateNames', metrics.duplicateNames, REVIEWED_METRICS.duplicateNames],
    ['duplicateNameOccurrences', metrics.duplicateNameOccurrences, REVIEWED_METRICS.duplicateNameOccurrences],
    ['maxExactNameReductions', metrics.maxExactNameReductions, REVIEWED_METRICS.maxExactNameReductions],
    ['verbFamilyAddCreateSetConfigure', metrics.verbFamilyAddCreateSetConfigure, REVIEWED_METRICS.verbFamilyAddCreateSetConfigure],
  ];
  for (const [name, actual, expected] of checks) {
    if (actual !== expected) {
      throw new InventoryBuildError(
        `Reviewed metric mismatch for ${name}: source produced ${actual} but the reviewed baseline requires ${expected}. ` +
          'Do not force the count; treat this as an evidence-backed blocker.',
      );
    }
  }
}

function emptyClassificationCounts(): Record<Classification, number> {
  return { A: 0, B: 0, C: 0, D: 0, E: 0, F: 0 };
}

function emptyDispositionCounts(): Record<Disposition, number> {
  return { keep: 0, alias: 0, map: 0, promote: 0, remove: 0, review: 0 };
}

export function buildInventory(
  records?: readonly CapabilityRecord[],
): NormalizationInventory {
  const raw = extractOccurrences(records);

  const groups = new Map<string, RawOccurrence[]>();
  for (const r of raw) {
    const arr = groups.get(r.action);
    if (arr === undefined) groups.set(r.action, [r]);
    else arr.push(r);
  }

  const decisions = new Map<RawOccurrence, Decision>();
  for (const [action, items] of groups) {
    const decision = decideGroup(action, items);
    for (const [item, value] of decision) decisions.set(item, value);
  }

  const occurrences: OccurrenceRecord[] = [];
  for (const r of raw) {
    const decision = decisions.get(r);
    if (decision === undefined) {
      throw new InventoryBuildError(`no decision for ${r.tool}:${r.action}`);
    }
    occurrences.push({
      occurrenceKey: `${r.tool}:${r.action}`,
      tool: r.tool,
      action: r.action,
      canonicalId: decision.canonicalId,
      classification: decision.classification,
      role: decision.role,
      disposition: decision.disposition,
      evidence: r.evidence,
      rawRoute: buildRawRoute(r.tool, namespaceOf(decision.canonicalId)),
      justification: justificationFor(decision.classification, decision.role, r.tool, r.action),
    });
  }
  occurrences.sort((a, b) => compareKeys(a.occurrenceKey, b.occurrenceKey));

  const byCanonical = new Map<string, OccurrenceRecord[]>();
  for (const occ of occurrences) {
    const arr = byCanonical.get(occ.canonicalId);
    if (arr === undefined) byCanonical.set(occ.canonicalId, [occ]);
    else arr.push(occ);
  }

  const canonicalDefinitions: CanonicalDefinition[] = [];
  let canonicalCollisions = 0;
  for (const [canonicalId, occs] of byCanonical) {
    occs.sort((a, b) => compareKeys(a.occurrenceKey, b.occurrenceKey));
    const names = new Set(occs.map((o) => o.action));
    if (names.size > 1) canonicalCollisions++;
    const classification: Classification = occs.some((o) => o.classification === 'A')
      ? 'A'
      : occs[0].classification;
    const aliases = occs.filter((o) => o.role === 'alias').map((o) => o.occurrenceKey);
    canonicalDefinitions.push({
      canonicalId,
      action: occs[0].action,
      namespace: namespaceOf(canonicalId),
      classification,
      disposition: occs[0].disposition,
      occurrences: occs.map((o) => o.occurrenceKey),
      evidence: occs[0].evidence,
      aliases,
    });
  }
  canonicalDefinitions.sort((a, b) => compareKeys(a.canonicalId, b.canonicalId));

  const toolSet = new Set(occurrences.map((o) => o.tool));
  const duplicateGroups = [...groups.values()].filter((g) => g.length > 1);
  const occurrenceCount = occurrences.length;
  const duplicateNames = duplicateGroups.length;
  const duplicateNameOccurrences = duplicateGroups.reduce((s, g) => s + g.length, 0);
  const maxExactNameReductions = duplicateNameOccurrences - duplicateNames;
  const verbFamily = occurrences.filter((o) => VERB_SET.has(o.action.split('_')[0] ?? '')).length;

  const classificationCounts = emptyClassificationCounts();
  const dispositionCounts = emptyDispositionCounts();
  for (const o of occurrences) {
    classificationCounts[o.classification] += 1;
    dispositionCounts[o.disposition] += 1;
  }

  const routeDispositions = [...buildRouteDispositions()].sort((a, b) =>
    compareKeys(a.dispositionKey, b.dispositionKey),
  );
  const route = tallyRouteDispositions(routeDispositions);
  assertRouteDispositionsComplete(routeDispositions);

  const metrics: InventoryMetrics = {
    occurrenceCount,
    toolCount: toolSet.size,
    distinctActionNames: groups.size,
    duplicateNames,
    duplicateNameOccurrences,
    maxExactNameReductions,
    actualCanonicalReductions: occurrenceCount - canonicalDefinitions.length,
    verbFamilyAddCreateSetConfigure: verbFamily,
    unclassifiedOccurrences: occurrences.filter((o) => !CLASSIFICATIONS.includes(o.classification)).length,
    canonicalCollisions,
    classificationCounts,
    dispositionCounts,
    routeDispositionTotal: route.total,
    routeDispositionUnresolved: route.unresolved,
    routeStatusCounts: route.statusCounts,
    routeDispositionCounts: route.dispositionCounts,
  };

  assertReviewedMetrics(metrics);

  const body: NormalizationInventory = {
    schemaVersion: INVENTORY_SCHEMA_VERSION,
    metadata: {
      generatedBy: 'generate-normalization-inventory',
      sourceToolCount: toolSet.size,
      occurrenceCount,
      contentSha256: '',
    },
    metrics,
    canonicalDefinitions,
    occurrences,
    routeDispositions,
  };

  const contentSha256 = createHash('sha256')
    .update(JSON.stringify(body, null, 2), 'utf8')
    .digest('hex');

  return {
    ...body,
    metadata: { ...body.metadata, contentSha256 },
  };
}
