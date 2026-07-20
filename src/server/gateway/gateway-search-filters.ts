// src/server/gateway/gateway-search-filters.ts
// Argument parsing, validation and candidate selection for gateway `search`.
//
// Every filter is validated against the generated catalog before any ranking
// runs, so a typo reports the exact dimension that failed with bounded
// suggestions instead of silently returning an empty result set.

import type { CapabilityRecord } from '../../tools/catalog/capabilities/model.js';
import { capabilityIndex, resolveCapability, resolveLegacyPair } from './gateway-capability-index.js';
import { closestMatches, MAX_SUGGESTIONS } from './gateway-guidance.js';
import { gatewayError, getString } from './gateway-shared.js';

export const DEFAULT_SEARCH_MAX_BYTES = 24_576;
export const MIN_SEARCH_MAX_BYTES = 512;
export const MAX_SEARCH_MAX_BYTES = 262_144;
const CURSOR_PREFIX = 'c:';

export type SearchFilters = {
  readonly query: string;
  readonly domain: string | undefined;
  readonly family: string | undefined;
  readonly tool: string | undefined;
  readonly action: string | undefined;
  readonly capability: string | undefined;
  readonly effect: string | undefined;
};

export function readFilters(args: Record<string, unknown>): SearchFilters {
  return {
    query: getString(args, 'query') ?? '',
    domain: getString(args, 'domain'),
    family: getString(args, 'family'),
    tool: getString(args, 'tool'),
    action: getString(args, 'action'),
    capability: getString(args, 'capability'),
    effect: getString(args, 'effect')
  };
}

export function encodeCursor(offset: number): string {
  return `${CURSOR_PREFIX}${offset}`;
}

export function decodeCursor(cursor: string): number | undefined {
  if (!cursor.startsWith(CURSOR_PREFIX)) return undefined;
  const raw = cursor.slice(CURSOR_PREFIX.length);
  if (!/^\d+$/.test(raw)) return undefined;
  const offset = Number.parseInt(raw, 10);
  return Number.isSafeInteger(offset) ? offset : undefined;
}

/**
 * A rejected filter hands back the same search with the bad dimension replaced
 * by its closest declared value, so the caller can re-issue it verbatim.
 */
function guidedFilterError(
  errorCode: string,
  message: string,
  dimension: string,
  target: string,
  candidates: readonly string[],
  base: Record<string, unknown>
): Record<string, unknown> {
  const suggestions = closestMatches(target, [...candidates], MAX_SUGGESTIONS);
  const nextCall: Record<string, unknown> = { operation: 'search', ...base };
  if (suggestions.length > 0) nextCall[dimension] = suggestions[0];
  return { ...gatewayError('search', errorCode, message), suggestions, nextCall };
}

function validateDomain(filters: SearchFilters): Record<string, unknown> | undefined {
  if (filters.domain === undefined) return undefined;
  const { domains } = capabilityIndex();
  if (domains.includes(filters.domain)) return undefined;
  return guidedFilterError(
    'UNKNOWN_DOMAIN',
    `Unknown domain '${filters.domain}'. Call describe with no selector to list domains.`,
    'domain',
    filters.domain,
    domains,
    {}
  );
}

function validateFamily(filters: SearchFilters): Record<string, unknown> | undefined {
  if (filters.family === undefined) return undefined;
  const index = capabilityIndex();
  const candidates = filters.domain === undefined
    ? [...new Set(index.records.map((record) => record.discovery.family))].sort()
    : index.familiesByDomain.get(filters.domain) ?? [];
  if (candidates.includes(filters.family)) return undefined;
  const base = filters.domain === undefined ? {} : { domain: filters.domain };
  return guidedFilterError(
    'UNKNOWN_FAMILY',
    `Unknown family '${filters.family}'. Call describe on the domain to list its families.`,
    'family',
    filters.family,
    candidates,
    base
  );
}

function validateTool(filters: SearchFilters): Record<string, unknown> | undefined {
  if (filters.tool === undefined) return undefined;
  const { byParentTool } = capabilityIndex();
  if (byParentTool.has(filters.tool)) return undefined;
  return guidedFilterError(
    'UNKNOWN_TOOL',
    `Unknown tool '${filters.tool}'. Call search without a tool filter to browse capabilities.`,
    'tool',
    filters.tool,
    [...byParentTool.keys()].sort(),
    {}
  );
}

function validateEffect(filters: SearchFilters): Record<string, unknown> | undefined {
  if (filters.effect === undefined) return undefined;
  const effects = [...new Set(capabilityIndex().records.map((record) => record.behavior.effect))].sort();
  if (effects.some((effect) => effect === filters.effect)) return undefined;
  return guidedFilterError(
    'UNKNOWN_EFFECT',
    `Unknown effect '${filters.effect}'. Declared effects: ${effects.join(', ')}.`,
    'effect',
    filters.effect,
    effects,
    {}
  );
}

export function validateFilters(filters: SearchFilters): Record<string, unknown> | undefined {
  return validateDomain(filters)
    ?? validateFamily(filters)
    ?? validateTool(filters)
    ?? validateEffect(filters);
}

function matchesFilters(record: CapabilityRecord, filters: SearchFilters): boolean {
  if (filters.domain !== undefined && record.discovery.domain !== filters.domain) return false;
  if (filters.family !== undefined && record.discovery.family !== filters.family) return false;
  if (filters.tool !== undefined && record.routing.parentTool !== filters.tool) return false;
  if (filters.effect !== undefined && record.behavior.effect !== filters.effect) return false;
  return true;
}

/**
 * Candidate set before ranking. An exact capability reference or an exact
 * legacy tool+action pair resolves to a single record; anything else is the
 * filtered catalog in canonical ID order.
 */
export function selectCandidates(filters: SearchFilters): readonly CapabilityRecord[] {
  if (filters.capability !== undefined) {
    const resolved = resolveCapability(filters.capability);
    return resolved.kind === 'unknown' ? [] : [resolved.record];
  }
  if (filters.tool !== undefined && filters.action !== undefined) {
    const resolved = resolveLegacyPair(filters.tool, filters.action);
    return resolved.kind === 'unknown' ? [] : [resolved.record];
  }
  const candidates = capabilityIndex().records.filter((record) => matchesFilters(record, filters));
  if (filters.action === undefined) return candidates;
  return candidates.filter((record) => record.routing.dispatchAction === filters.action);
}
