// A result too large for one reply, paged instead of refused. When one top-level array is what makes it too big, the
// items that fit go out with a page cursor, and the rest waits here briefly for the same principal, capability and
// params to read with options.pageCursor, without the handler running again. Each page is the capability's own
// output with a slice of that array, so it still meets the declared output schema. A cursor names the kept result and
// the offset its page starts at, so reading it again (a retry, or an idempotent replay of the first call) returns the
// same page instead of the one after. Mirrors McpNativeResultPages.cpp.

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
  rest: Record<string, unknown>;
  readonly field: string;
  readonly items: readonly unknown[];
  /** The offsets this result's pages handed out: a cursor rewritten to another offset read a page that overlapped. */
  readonly issued: Set<number>;
  expires: number;
}

const kept = new Map<string, KeptResult>();

const sizeOf = (value: unknown): number => JSON.stringify(value)?.length ?? 0;

function roomFor(rest: Record<string, unknown>, budget: number): number {
  return budget - sizeOf(rest) - PAGE_OVERHEAD_CHARS;
}

function issue(entry: KeptResult, id: string, offset: number): string {
  entry.issued.add(offset);
  return `${id}:${offset}`;
}

function slice(entry: KeptResult, id: string, offset: number, budget: number, now: number): ResultPage {
  const room = roomFor(entry.rest, budget);
  let used = 0;
  let end = offset;
  // firstPage checked that every item fits on its own, so each page takes at least one and paging always advances.
  while (end < entry.items.length && (end === offset || used + sizeOf(entry.items[end]) + 1 <= room)) {
    used += sizeOf(entry.items[end]) + 1;
    end += 1;
  }
  const page: PageInfo = {
    field: entry.field,
    offset,
    returned: end - offset,
    total: entry.items.length,
    ...(end < entry.items.length ? { cursor: issue(entry, id, end) } : {})
  };
  entry.expires = now + PAGE_TTL_MS;
  return { result: { ...entry.rest, [entry.field]: entry.items.slice(offset, end) }, page };
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
  const id = randomUUID();
  const entry: KeptResult = { ...owner, rest, field, items, issued: new Set<number>(), expires: now + PAGE_TTL_MS };
  kept.set(id, entry);
  const first = slice(entry, id, 0, budget, now);
  // The image goes out once, with the first page: later pages have the flat budget, and an image kept in rest left
  // them room for one item each while it was sent again with every page.
  entry.rest = Object.fromEntries(Object.entries(rest).filter(([key]) => key !== 'imageBase64'));
  return first;
}

export type PageRefusal = 'PAGE_CURSOR_EXPIRED' | 'PAGE_CURSOR_MISMATCH';

/** The page a cursor names, or why there is none: expired (or never issued), or owned by another principal, capability or set of params. */
export function nextPage(cursor: string, owner: PageOwner, budget: number, now = Date.now()): ResultPage | PageRefusal {
  const colon = cursor.lastIndexOf(':');
  const id = cursor.slice(0, colon);
  const text = cursor.slice(colon + 1);
  const offset = Number(text);
  const entry = colon < 0 ? undefined : kept.get(id);
  if (entry !== undefined && entry.expires <= now) kept.delete(id);
  // Only an offset a page of this result issued: a whole number inside it, written as the cursor wrote it.
  const issued = Number.isInteger(offset) && String(offset) === text && offset > 0 && entry?.issued.has(offset) === true;
  if (entry === undefined || entry.expires <= now || !issued || offset >= entry.items.length) return 'PAGE_CURSOR_EXPIRED';
  if (entry.principal !== owner.principal || entry.capabilityId !== owner.capabilityId || entry.fingerprint !== owner.fingerprint) {
    return 'PAGE_CURSOR_MISMATCH';
  }
  return slice(entry, id, offset, budget, now);
}

/** Test seam: forget every kept result. */
export function resetPagesForTest(): void {
  kept.clear();
}
