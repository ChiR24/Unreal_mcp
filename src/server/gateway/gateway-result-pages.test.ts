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

  // A cursor that moved its result on read the page after it the second time: a replayed first receipt skipped one.
  it('reads the same page again from the same cursor', () => {
    const first = firstPage(big, BUDGET, OWNER, 0);
    const again = (): unknown => nextPage(first?.page.cursor ?? '', OWNER, BUDGET, 1);
    const once = again();
    expect(typeof once).not.toBe('string');
    expect(again()).toEqual(once);
  });

  it('refuses another owner, a cursor it never issued, and one left unread past its time', () => {
    const first = firstPage(big, BUDGET, OWNER, 0);
    const cursor = first?.page.cursor ?? '';
    expect(nextPage(cursor, { ...OWNER, principal: 'p2' }, BUDGET, 1)).toBe('PAGE_CURSOR_MISMATCH');
    expect(nextPage(cursor, { ...OWNER, fingerprint: 'other params' }, BUDGET, 1)).toBe('PAGE_CURSOR_MISMATCH');
    expect(nextPage('never-issued', OWNER, BUDGET, 1)).toBe('PAGE_CURSOR_EXPIRED');
    const id = cursor.slice(0, cursor.lastIndexOf(':'));
    for (const forged of [`${id}:0`, `${id}:300`, `${id}:-1`, `${id}:1.5`, `${id}:01`, id]) {
      expect(nextPage(forged, OWNER, BUDGET, 1), forged).toBe('PAGE_CURSOR_EXPIRED');
    }
    expect(typeof nextPage(cursor, OWNER, BUDGET, 1), 'the issued cursor still reads').not.toBe('string');
    expect(nextPage(cursor, OWNER, BUDGET, 1 + PAGE_TTL_MS)).toBe('PAGE_CURSOR_EXPIRED');
  });

  it('pages nothing it cannot: no array, or one item too large for a reply of its own', () => {
    expect(firstPage({ text: 'x'.repeat(50_000) }, BUDGET, OWNER, 0)).toBeUndefined();
    expect(firstPage({ assets: [{ blob: 'x'.repeat(10_000) }, { blob: 'y' }] }, BUDGET, OWNER, 0)).toBeUndefined();
  });

  // An image kept with the rest went out again with every page and, against the flat later-page budget, left room
  // for one item per page.
  it('sends an image with the first page only, so later pages keep their whole budget', () => {
    const image = 'i'.repeat(6_000);
    const first = firstPage({ ...big, imageBase64: image }, BUDGET + image.length, OWNER, 0);
    expect(first?.result.imageBase64).toBe(image);
    const next = nextPage(first?.page.cursor ?? '', OWNER, BUDGET, 1);
    if (typeof next === 'string') throw new Error(next);
    expect(next.result.imageBase64).toBeUndefined();
    expect(next.page.returned).toBeGreaterThanOrEqual(first?.page.returned ?? Infinity);
  });

  it('keeps at most the newest few paged results', () => {
    const cursors = Array.from({ length: MAX_KEPT_PAGES + 1 }, () => firstPage(big, BUDGET, OWNER, 0)?.page.cursor ?? '');
    expect(nextPage(cursors[0] ?? '', OWNER, BUDGET, 1)).toBe('PAGE_CURSOR_EXPIRED');
    expect(typeof nextPage(cursors[MAX_KEPT_PAGES] ?? '', OWNER, BUDGET, 1)).not.toBe('string');
  });
});
