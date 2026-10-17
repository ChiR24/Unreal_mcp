// A result too large for one reply, paged instead of refused. When one top-level array is what makes it too big, the
// items that fit go out with a page cursor, and the rest waits here briefly for the same principal, capability and
// params to read with options.pageCursor, without the handler running again. Each page is the capability's own
// output with a slice of that array, so it still meets the declared output schema. Mirrors McpNativeResultPages.cpp.

import { randomUUID } from 'node:crypto';

/** How long the rest of a paged result waits for its next read, and how many paged results are kept at once. */
export const PAGE_TTL_MS = 5 * 60_000;
export const MAX_KEPT_PAGES = 8;
// The page block, its continuation call and the field name, kept out of the items' share of the budget.
const PAGE_OVERHEAD_CHARS = 2_000;

/** Who may read the rest: the principal that ran the call, for the same capability and params. */
export interface PageOwner {
  readonly principal: string;
  readonly capabilityId: string;
  readonly fingerprint: string;
}

export interface PageInfo {
  readonly field: string;
  readonly offset: number;
  readonly returned: number;
  readonly total: number;
  /** Present while items remain: pass it back as options.pageCursor. */
  readonly cursor?: string;
}

export interface ResultPage {
  readonly result: Record<string, unknown>;
  readonly page: PageInfo;
}

interface KeptResult extends PageOwner {
  readonly rest: Record<string, unknown>;
  readonly field: string;
  readonly items: readonly unknown[];
  offset: number;
  expires: number;
}

const kept = new Map<string, KeptResult>();

const sizeOf = (value: unknown): number => JSON.stringify(value)?.length ?? 0;

function roomFor(rest: Record<string, unknown>, budget: number): number {
  return budget - sizeOf(rest) - PAGE_OVERHEAD_CHARS;
}

function slice(entry: KeptResult, cursor: string, budget: number, now: number): ResultPage {
  const room = roomFor(entry.rest, budget);
  let used = 0;
  let end = entry.offset;
  // firstPage checked that every item fits on its own, so each page takes at least one and paging always advances.
  while (end < entry.items.length && (end === entry.offset || used + sizeOf(entry.items[end]) + 1 <= room)) {
    used += sizeOf(entry.items[end]) + 1;
    end += 1;
  }
  const page: PageInfo = {
    field: entry.field,
    offset: entry.offset,
    returned: end - entry.offset,
    total: entry.items.length,
    ...(end < entry.items.length ? { cursor } : {})
  };
  const result = { ...entry.rest, [entry.field]: entry.items.slice(entry.offset, end) };
  entry.offset = end;
  entry.expires = now + PAGE_TTL_MS;
  if (end >= entry.items.length) kept.delete(cursor);
  return { result, page };
}

/**
 * The first page of an oversized result, or undefined when it cannot be paged: no top-level array of two or more
 * items, the rest alone over budget, or one item too large for a reply of its own.
 */
export function firstPage(result: Record<string, unknown>, budget: number, owner: PageOwner, now = Date.now()): ResultPage | undefined {
  let field: string | undefined;
  let largest = -1;
  for (const [key, value] of Object.entries(result)) {
    if (!Array.isArray(value) || value.length < 2) continue;
    const size = sizeOf(value);
    if (size > largest) {
      largest = size;
      field = key;
    }
  }
  if (field === undefined) return undefined;
  const items = result[field] as readonly unknown[];
  const rest = Object.fromEntries(Object.entries(result).filter(([key]) => key !== field));
  const room = roomFor(rest, budget);
  if (room <= 0 || items.some((item) => sizeOf(item) + 1 > room)) return undefined;

  for (const [cursor, entry] of kept) {
    if (entry.expires <= now) kept.delete(cursor);
  }
  while (kept.size >= MAX_KEPT_PAGES) {
    const oldest = kept.keys().next().value;
    if (oldest === undefined) break;
    kept.delete(oldest);
  }
  const cursor = randomUUID();
  const entry: KeptResult = { ...owner, rest, field, items, offset: 0, expires: now + PAGE_TTL_MS };
  kept.set(cursor, entry);
  return slice(entry, cursor, budget, now);
}

export type PageRefusal = 'PAGE_CURSOR_EXPIRED' | 'PAGE_CURSOR_MISMATCH';

/** The next page for a cursor, or why there is none: expired (or never issued, or already read to the end), or owned by another principal, capability or set of params. */
export function nextPage(cursor: string, owner: PageOwner, budget: number, now = Date.now()): ResultPage | PageRefusal {
  const entry = kept.get(cursor);
  if (entry === undefined || entry.expires <= now) {
    kept.delete(cursor);
    return 'PAGE_CURSOR_EXPIRED';
  }
  if (entry.principal !== owner.principal || entry.capabilityId !== owner.capabilityId || entry.fingerprint !== owner.fingerprint) {
    return 'PAGE_CURSOR_MISMATCH';
  }
  return slice(entry, cursor, budget, now);
}

/** Test seam: forget every kept result. */
export function resetPagesForTest(): void {
  kept.clear();
}
