/**
 * Top-level inventory validation orchestrator.
 *
 * Performs unknown-field detection, duplicate-key/definition detection, orphan
 * reference detection, deterministic-ordering checks, canonical-collision
 * checks, and metric self-consistency. Record-level field validators live in
 * `validate-records.ts`.
 */

import { assertRouteDispositionsComplete } from './routedispositions.js';
import type {
  Classification,
  Disposition,
  NormalizationInventory,
  RouteDisposition,
  RouteDispositionStatus,
} from './types.js';
import { recomputeMetrics } from './validate-recomputed.js';
import {
  assertNoExtraKeys,
  compareKeys,
  InventoryValidationError,
  isRecord,
  validateCanonical,
  validateOccurrence,
  validateRouteDisposition,
} from './validate-records.js';

const TOP_KEYS = new Set([
  'schemaVersion',
  'metadata',
  'metrics',
  'canonicalDefinitions',
  'occurrences',
  'routeDispositions',
]);
const META_KEYS = new Set(['generatedBy', 'sourceToolCount', 'occurrenceCount', 'contentSha256']);
const METRICS_KEYS = new Set([
  'occurrenceCount',
  'toolCount',
  'distinctActionNames',
  'duplicateNames',
  'duplicateNameOccurrences',
  'maxExactNameReductions',
  'actualCanonicalReductions',
  'verbFamilyAddCreateSetConfigure',
  'unclassifiedOccurrences',
  'canonicalCollisions',
  'classificationCounts',
  'dispositionCounts',
  'routeDispositionTotal',
  'routeDispositionUnresolved',
  'routeStatusCounts',
  'routeDispositionCounts',
]);

/** Validate a parsed artifact; throws InventoryValidationError on any violation. */
export function validateInventoryData(input: unknown): NormalizationInventory {
  if (!isRecord(input)) {
    throw new InventoryValidationError('artifact root is not an object');
  }
  assertNoExtraKeys(input, TOP_KEYS, 'root');
  if (typeof input.schemaVersion !== 'string') {
    throw new InventoryValidationError('schemaVersion missing');
  }
  const meta = input.metadata;
  if (!isRecord(meta)) throw new InventoryValidationError('metadata missing');
  assertNoExtraKeys(meta, META_KEYS, 'metadata');
  const metricsRaw = input.metrics;
  if (!isRecord(metricsRaw)) throw new InventoryValidationError('metrics missing');
  assertNoExtraKeys(metricsRaw, METRICS_KEYS, 'metrics');

  if (!Array.isArray(input.occurrences)) {
    throw new InventoryValidationError('occurrences not array');
  }
  if (!Array.isArray(input.canonicalDefinitions)) {
    throw new InventoryValidationError('canonicalDefinitions not array');
  }

  const occurrences = input.occurrences.map((v, i) =>
    validateOccurrence(v, `occurrences[${i}]`),
  );
  const canonicalDefinitions = input.canonicalDefinitions.map((v, i) =>
    validateCanonical(v, `canonicalDefinitions[${i}]`),
  );
  if (!Array.isArray(input.routeDispositions)) {
    throw new InventoryValidationError('routeDispositions not array');
  }
  const routeDispositions = input.routeDispositions.map((v, i) =>
    validateRouteDisposition(v, `routeDispositions[${i}]`),
  );
  const routeKeys = new Set(routeDispositions.map((r) => r.dispositionKey));
  if (routeKeys.size !== routeDispositions.length) {
    throw new InventoryValidationError('duplicate routeDispositionKey detected');
  }
  for (let i = 1; i < routeDispositions.length; i++) {
    if (compareKeys(routeDispositions[i - 1].dispositionKey, routeDispositions[i].dispositionKey) > 0) {
      throw new InventoryValidationError('routeDispositions are not deterministically sorted');
    }
  }
  assertRouteDispositionsComplete(routeDispositions);

  const occKeys = new Set(occurrences.map((o) => o.occurrenceKey));
  if (occKeys.size !== occurrences.length) {
    throw new InventoryValidationError('duplicate occurrenceKey detected');
  }
  const canonIds = new Set(canonicalDefinitions.map((c) => c.canonicalId));
  if (canonIds.size !== canonicalDefinitions.length) {
    throw new InventoryValidationError('duplicate canonicalId detected');
  }
  for (const o of occurrences) {
    if (!canonIds.has(o.canonicalId)) {
      throw new InventoryValidationError(`orphan canonicalId "${o.canonicalId}" at ${o.occurrenceKey}`);
    }
  }
  for (const c of canonicalDefinitions) {
    for (const key of c.occurrences) {
      if (!occKeys.has(key)) {
        throw new InventoryValidationError(`canonical ${c.canonicalId} references missing occurrence ${key}`);
      }
    }
    for (const key of c.aliases) {
      if (!occKeys.has(key)) {
        throw new InventoryValidationError(`canonical ${c.canonicalId} aliases missing occurrence ${key}`);
      }
    }
    const names = new Set(
      occurrences.filter((o) => o.canonicalId === c.canonicalId).map((o) => o.action),
    );
    if (names.size > 1) {
      throw new InventoryValidationError(`canonical collision at ${c.canonicalId}`);
    }
  }

  for (let i = 1; i < occurrences.length; i++) {
    if (compareKeys(occurrences[i - 1].occurrenceKey, occurrences[i].occurrenceKey) > 0) {
      throw new InventoryValidationError('occurrences are not deterministically sorted');
    }
  }
  for (let i = 1; i < canonicalDefinitions.length; i++) {
    if (compareKeys(canonicalDefinitions[i - 1].canonicalId, canonicalDefinitions[i].canonicalId) > 0) {
      throw new InventoryValidationError('canonicalDefinitions are not deterministically sorted');
    }
  }
  for (const c of canonicalDefinitions) {
    for (let i = 1; i < c.occurrences.length; i++) {
      if (compareKeys(c.occurrences[i - 1], c.occurrences[i]) > 0) {
        throw new InventoryValidationError(`canonical ${c.canonicalId} occurrences not sorted`);
      }
    }
  }

  // Compared field-by-field against one list instead of two hand-mirrored object
  // literals. The mirrors had to be edited together to stay meaningful, and
  // missing one silently dropped that metric from the check while the gate
  // stayed green. Iterating also lets the error name the offending metric.
  //
  // Deliberately NOT METRICS_KEYS: that set also carries
  // `actualCanonicalReductions`, which the mirrors did not compare, so reusing
  // it would widen the check rather than preserve it.
  const recomputed = recomputeMetrics(occurrences, routeDispositions);
  const compared = [...METRICS_KEYS].filter((key) => key !== 'actualCanonicalReductions');
  const recomputedRecord = recomputed as unknown as Record<string, unknown>;
  for (const key of compared) {
    if (JSON.stringify(recomputedRecord[key]) !== JSON.stringify(metricsRaw[key])) {
      throw new InventoryValidationError(
        `metrics.${key} is inconsistent with occurrences `
        + `(recomputed ${JSON.stringify(recomputedRecord[key])}, artifact ${JSON.stringify(metricsRaw[key])})`
      );
    }
  }

  return {
    schemaVersion: input.schemaVersion as string,
    metadata: {
      generatedBy: meta.generatedBy as string,
      sourceToolCount: meta.sourceToolCount as number,
      occurrenceCount: meta.occurrenceCount as number,
      contentSha256: meta.contentSha256 as string,
    },
    metrics: {
      occurrenceCount: metricsRaw.occurrenceCount as number,
      toolCount: metricsRaw.toolCount as number,
      distinctActionNames: metricsRaw.distinctActionNames as number,
      duplicateNames: metricsRaw.duplicateNames as number,
      duplicateNameOccurrences: metricsRaw.duplicateNameOccurrences as number,
      maxExactNameReductions: metricsRaw.maxExactNameReductions as number,
      actualCanonicalReductions: metricsRaw.actualCanonicalReductions as number,
      verbFamilyAddCreateSetConfigure: metricsRaw.verbFamilyAddCreateSetConfigure as number,
      unclassifiedOccurrences: metricsRaw.unclassifiedOccurrences as number,
      canonicalCollisions: metricsRaw.canonicalCollisions as number,
      classificationCounts: metricsRaw.classificationCounts as Record<Classification, number>,
      dispositionCounts: metricsRaw.dispositionCounts as Record<Disposition, number>,
      routeDispositionTotal: metricsRaw.routeDispositionTotal as number,
      routeDispositionUnresolved: metricsRaw.routeDispositionUnresolved as number,
      routeStatusCounts: metricsRaw.routeStatusCounts as Record<RouteDispositionStatus, number>,
      routeDispositionCounts: metricsRaw.routeDispositionCounts as Record<RouteDisposition, number>,
    },
    canonicalDefinitions,
    occurrences,
    routeDispositions,
  };
}
