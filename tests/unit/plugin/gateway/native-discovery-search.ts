/// <reference types="node" />

// Task 25: normalized `search` reference. Deterministic filter, score, order,
// page and byte budget; mirrored exactly by McpNativeGatewaySearch.cpp.

import {
  allDomains,
  allFamilies,
  boundedLimit,
  boundedOffset,
  canonicalJson,
  closestMatches,
  guidedError,
  isAvailable,
  loadCanonicalRegistry,
  ordinalCompare,
  utf8Length,
  type DiscoveryInput,
  type DiscoveryRecord,
  type JsonValue,
} from './native-discovery-model.js';

export const SEARCH_DEFAULT_LIMIT = 12;
export const SEARCH_MAX_LIMIT = 25;
// Genuinely binding: the widest 25 results the catalog can produce total 10,263
// bytes, so a 16 KB cap could never fire. 8 KB bounds a full page of typical
// results and is exercised by real queries on both surfaces.
export const MAX_RESULT_BYTES = 8192;

// Ordered highest-signal first. A record matches when at least one fires; the
// score is the sum, so a query hitting the id outranks one hitting only prose.
const MATCH_RULES: readonly { readonly reason: string; readonly weight: number }[] = [
  { reason: 'id-exact', weight: 100 },
  { reason: 'id', weight: 50 },
  { reason: 'family', weight: 20 },
  { reason: 'domain', weight: 15 },
  { reason: 'topic', weight: 12 },
  { reason: 'summary', weight: 8 },
  { reason: 'parent', weight: 5 },
];

const ruleHits = (record: DiscoveryRecord, query: string): readonly boolean[] => [
  record.id.toLowerCase() === query,
  record.id.toLowerCase().includes(query),
  record.discovery.family.toLowerCase().includes(query),
  record.discovery.domain.toLowerCase().includes(query),
  record.discovery.topics.some((topic) => topic.toLowerCase().includes(query)),
  record.discovery.summary.toLowerCase().includes(query),
  record.routing.parentTool.toLowerCase().includes(query),
];

export const searchCapabilities = (input: DiscoveryInput): JsonValue => {
  const registry = loadCanonicalRegistry();
  const query = (input.query ?? '').trim().toLowerCase();
  const limit = boundedLimit(input.limit, SEARCH_DEFAULT_LIMIT, SEARCH_MAX_LIMIT);
  const offset = boundedOffset(input.offset);

  if (input.domain !== undefined && !allDomains().includes(input.domain)) {
    return guidedError('search', 'UNKNOWN_DOMAIN', `Unknown domain '${input.domain}'.`, {
      nextCall: { operation: 'search' },
      suggestions: closestMatches(input.domain, allDomains()) as readonly JsonValue[],
    });
  }
  if (input.family !== undefined && !allFamilies().includes(input.family)) {
    return guidedError('search', 'UNKNOWN_FAMILY', `Unknown family '${input.family}'.`, {
      nextCall: { operation: 'search' },
      suggestions: closestMatches(input.family, allFamilies()) as readonly JsonValue[],
    });
  }

  const scored: { readonly record: DiscoveryRecord; readonly score: number; readonly reasons: readonly string[] }[] = [];
  for (const record of registry.records) {
    if (input.domain !== undefined && record.discovery.domain !== input.domain) continue;
    if (input.family !== undefined && record.discovery.family !== input.family) continue;
    if (query.length === 0) {
      scored.push({ record, score: 0, reasons: [] });
      continue;
    }
    const hits = ruleHits(record, query);
    const reasons = MATCH_RULES.filter((_, index) => hits[index]).map((rule) => rule.reason);
    if (reasons.length === 0) continue;
    const score = MATCH_RULES.reduce((sum, rule, index) => (hits[index] ? sum + rule.weight : sum), 0);
    scored.push({ record, score, reasons });
  }

  scored.sort((left, right) => right.score - left.score || ordinalCompare(left.record.id, right.record.id));

  const total = scored.length;
  const page = scored.slice(offset, offset + limit);
  const results: JsonValue[] = [];
  let bytes = 0;
  let truncated = false;
  for (const entry of page) {
    const view: JsonValue = {
      available: isAvailable(entry.record),
      capability: entry.record.id,
      domain: entry.record.discovery.domain,
      effect: entry.record.behavior.effect,
      family: entry.record.discovery.family,
      matchReasons: entry.reasons as readonly JsonValue[],
      nextCall: {
        action: entry.record.routing.dispatchAction,
        operation: 'describe',
        tool: entry.record.routing.parentTool,
      },
      parent: entry.record.routing.parentTool,
      score: entry.score,
      summary: entry.record.discovery.summary,
    };
    const size = utf8Length(canonicalJson(view));
    if (bytes + size > MAX_RESULT_BYTES && results.length > 0) {
      truncated = true;
      break;
    }
    bytes += size;
    results.push(view);
  }

  const hasMore = offset + results.length < total;
  const envelope: Record<string, JsonValue> = {
    catalogRevision: registry.catalogRevision,
    hasMore,
    limit,
    message:
      'Results are capability-level and bounded. Call describe with the exact capability before execute.',
    offset,
    operation: 'search',
    query: input.query ?? '',
    results,
    success: true,
    total,
    truncated,
  };
  if (input.domain !== undefined) envelope.domain = input.domain;
  if (input.family !== undefined) envelope.family = input.family;
  if (hasMore) envelope.nextCursor = String(offset + results.length);
  return envelope;
};

