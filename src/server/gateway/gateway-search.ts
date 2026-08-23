// src/server/gateway/gateway-search.ts
// Bounded capability search over the generated canonical registry.
//
// Ranking reuses the Task 13 retrieval scorer so the TypeScript and native
// surfaces order identical inputs identically. An empty query is a browse, not
// a ranked search, and falls back to canonical ID order for determinism.
//
// Three independent budgets bound every response: a result limit, a resumable
// cursor, and a serialized byte ceiling. The byte ceiling is enforced by
// dropping whole rows from the end of the page, so a truncated page is always
// a prefix of the untruncated one and `hasMore` stays honest.

import type { CapabilityRecord } from '../../tools/catalog/capabilities/model.js';
import { rankCapabilityRecords } from '../../tools/catalog/capabilities/retrieval/scoring.js';
import { capabilityIndex, catalogRevision } from './gateway-capability-index.js';
import { capabilitySearchRow } from './gateway-capability-view.js';
import {
  DEFAULT_SEARCH_MAX_BYTES,
  MAX_SEARCH_MAX_BYTES,
  MIN_SEARCH_MAX_BYTES,
  decodeCursor,
  encodeCursor,
  readFilters,
  selectCandidates,
  validateFilters,
  type SearchFilters
} from './gateway-search-filters.js';
import {
  DEFAULT_SEARCH_LIMIT,
  MAX_SEARCH_LIMIT,
  gatewayError,
  getBoundedInteger,
  getString,
  integerCoercion,
  type ParameterCoercion
} from './gateway-shared.js';

type ScoredCandidate = { readonly record: CapabilityRecord; readonly reasons: readonly unknown[] };

function orderCandidates(
  candidates: readonly CapabilityRecord[],
  query: string
): readonly ScoredCandidate[] {
  if (query.trim().length === 0) {
    return candidates.map((record) => ({ record, reasons: [] }));
  }
  const ranked = rankCapabilityRecords(capabilityIndex().search, candidates, query);
  return ranked.map((entry) => ({ record: entry.record, reasons: entry.reasons }));
}

function declaredFilters(filters: SearchFilters): Record<string, unknown> {
  const declared: Record<string, unknown> = {};
  for (const [name, value] of Object.entries(filters)) {
    if (name !== 'query' && typeof value === 'string') declared[name] = value;
  }
  return declared;
}

function envelope(
  filters: SearchFilters,
  page: { offset: number; limit: number; total: number; maxBytes: number },
  rows: Array<Record<string, unknown>>,
  byteBudgetTruncated: boolean,
  coercions: readonly ParameterCoercion[]
): Record<string, unknown> {
  const hasMore = page.offset + rows.length < page.total;
  const truncated = byteBudgetTruncated || hasMore;
  const truncationReason = byteBudgetTruncated ? 'byte-budget' : hasMore ? 'limit' : 'none';
  const result: Record<string, unknown> = {
    success: true,
    operation: 'search',
    catalogRevision: catalogRevision(),
    query: filters.query,
    filters: declaredFilters(filters),
    results: rows,
    total: page.total,
    offset: page.offset,
    limit: page.limit,
    effectiveLimit: page.limit,
    servedCount: rows.length,
    maxBytes: page.maxBytes,
    hasMore,
    truncated,
    truncationReason,
    message: 'Results are compact. Call describe with an exact capability before execute.'
  };
  if (hasMore) result.nextCursor = encodeCursor(page.offset + rows.length);
  if (coercions.length > 0) result.coercions = coercions;
  return result;
}

/**
 * Drop whole rows from the end of the page until the serialized response fits
 * the byte budget. Rebuilding the envelope each step keeps `hasMore` and
 * `nextCursor` consistent with the rows that actually survived.
 */
function withinByteBudget(
  filters: SearchFilters,
  page: { offset: number; limit: number; total: number; maxBytes: number },
  rows: Array<Record<string, unknown>>,
  coercions: readonly ParameterCoercion[]
): Record<string, unknown> {
  let kept = rows;
  let response = envelope(filters, page, kept, false, coercions);
  while (kept.length > 0 && JSON.stringify(response).length > page.maxBytes) {
    kept = kept.slice(0, kept.length - 1);
    response = envelope(filters, page, kept, true, coercions);
  }
  // Not even one row fits. `envelope` would still advertise hasMore with a
  // nextCursor equal to the offset the caller just used, so a client following
  // the cursor spins on identical empty pages forever. The page genuinely
  // cannot advance at this budget, so say that instead of implying otherwise:
  // drop the cursor and tell the caller the only thing that will help.
  if (kept.length === 0 && rows.length > 0) {
    delete response.nextCursor;
    response.budgetExceeded = true;
    response.message =
      `No result fits maxBytes=${page.maxBytes}. Raise maxBytes and retry; `
      + 'this page cannot advance at the current budget.';
  }
  return response;
}

function resolveOffset(args: Record<string, unknown>): number | Record<string, unknown> {
  const cursor = getString(args, 'cursor');
  if (cursor === undefined) return getBoundedInteger(args.offset, 0, 0, Number.MAX_SAFE_INTEGER);
  const decoded = decodeCursor(cursor);
  if (decoded === undefined) {
    return {
      ...gatewayError('search', 'INVALID_CURSOR', 'cursor is not a gateway search cursor. Re-run search without a cursor.'),
      nextCall: { operation: 'search' }
    };
  }
  return decoded;
}

// A cursor supersedes `offset` entirely, so an offset clamp is only something
// the caller experienced when no cursor was supplied.
function pageCoercions(args: Record<string, unknown>): readonly ParameterCoercion[] {
  const candidates = [
    getString(args, 'cursor') === undefined
      ? integerCoercion('offset', args.offset, 0, Number.MAX_SAFE_INTEGER)
      : undefined,
    integerCoercion('limit', args.limit, 1, MAX_SEARCH_LIMIT),
    integerCoercion('maxBytes', args.maxBytes, MIN_SEARCH_MAX_BYTES, MAX_SEARCH_MAX_BYTES)
  ];
  return candidates.filter((entry): entry is ParameterCoercion => entry !== undefined);
}

export function searchGatewayCapabilities(args: Record<string, unknown>): Record<string, unknown> {
  const filters = readFilters(args);
  const invalidFilter = validateFilters(filters);
  if (invalidFilter) return invalidFilter;

  const offset = resolveOffset(args);
  if (typeof offset !== 'number') return offset;

  const limit = getBoundedInteger(args.limit, DEFAULT_SEARCH_LIMIT, 1, MAX_SEARCH_LIMIT);
  const maxBytes = getBoundedInteger(
    args.maxBytes,
    DEFAULT_SEARCH_MAX_BYTES,
    MIN_SEARCH_MAX_BYTES,
    MAX_SEARCH_MAX_BYTES
  );

  const ordered = orderCandidates(selectCandidates(filters), filters.query);
  const rows = ordered
    .slice(offset, offset + limit)
    .map((entry) => capabilitySearchRow(entry.record, entry.reasons));

  return withinByteBudget(
    filters,
    { offset, limit, total: ordered.length, maxBytes },
    rows,
    pageCoercions(args)
  );
}
