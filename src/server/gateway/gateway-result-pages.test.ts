import { afterEach, describe, expect, it } from 'vitest';

import { firstPage, MAX_KEPT_PAGES, nextPage, PAGE_TTL_MS, resetPagesForTest, type PageOwner } from './gateway-result-pages.js';

const OWNER: PageOwner = { principal: 'p1', capabilityId: 'asset.list', fingerprint: 'f1' };
const BUDGET = 8_000; // about 110 of the 300 items per page
const items = Array.from({ length: 300 }, (_, index) => ({ path: `/Game/Props/SM_Crate_${String(index).padStart(3, '0')}`, kind: 'StaticMesh' }));
const big = { success: true, message: '300 assets', count: 300, assets: items };

afterEach(() => resetPagesForTest());

describe('paging a result too large for one reply', () => {
  it('sends the items that fit with the rest of the result, then the next ones from the cursor, in order', () => {
    const first = firstPage(big, BUDGET, OWNER, 0);
    expect(first).toBeDefined();
    if (first === undefined) return;
    expect(JSON.stringify(first.result).length).toBeLessThanOrEqual(BUDGET);
    expect(first.result.count, 'the rest of the result rides along').toBe(300);
    expect(first.page).toMatchObject({ field: 'assets', offset: 0, total: 300 });
    const read: unknown[] = [...(first.result.assets as unknown[])];
    let cursor = first.page.cursor;
    while (cursor !== undefined) {
      const next = nextPage(cursor, OWNER, BUDGET, 1);
      if (typeof next === 'string') throw new Error(next);
      expect(next.page.offset).toBe(read.length);
      read.push(...(next.result.assets as unknown[]));
      cursor = next.page.cursor;
    }
    expect(read).toEqual(items);
  });

  it('forgets a cursor read to the end, refuses another owner, and lets an unread one expire', () => {
    const first = firstPage(big, BUDGET, OWNER, 0);
    const cursor = first?.page.cursor ?? '';
    expect(nextPage(cursor, { ...OWNER, principal: 'p2' }, BUDGET, 1)).toBe('PAGE_CURSOR_MISMATCH');
    expect(nextPage(cursor, { ...OWNER, fingerprint: 'other params' }, BUDGET, 1)).toBe('PAGE_CURSOR_MISMATCH');
    expect(nextPage(cursor, OWNER, BUDGET, PAGE_TTL_MS + 1)).toBe('PAGE_CURSOR_EXPIRED');
    expect(nextPage('never-issued', OWNER, BUDGET, 1)).toBe('PAGE_CURSOR_EXPIRED');
  });

  it('pages nothing it cannot: no array, or one item too large for a reply of its own', () => {
    expect(firstPage({ text: 'x'.repeat(50_000) }, BUDGET, OWNER, 0)).toBeUndefined();
    expect(firstPage({ assets: [{ blob: 'x'.repeat(10_000) }, { blob: 'y' }] }, BUDGET, OWNER, 0)).toBeUndefined();
  });

  it('keeps at most the newest few paged results', () => {
    const cursors = Array.from({ length: MAX_KEPT_PAGES + 1 }, () => firstPage(big, BUDGET, OWNER, 0)?.page.cursor ?? '');
    expect(nextPage(cursors[0] ?? '', OWNER, BUDGET, 1)).toBe('PAGE_CURSOR_EXPIRED');
    expect(typeof nextPage(cursors[MAX_KEPT_PAGES] ?? '', OWNER, BUDGET, 1)).not.toBe('string');
  });
});
