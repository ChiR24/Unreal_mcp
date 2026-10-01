// What the catalog search's page script sends and reports, run against a scripted page.
//
// The search can be narrowed to one publisher and one content kind, and every row now says who published
// it, how it is rated and what it costs. These run the JavaScript the adapter sends into Fab's page, so the
// query it builds and the rows it returns are checked without an editor.

import { describe, expect, it } from 'vitest';

import { fillSlots, rawScript, runPageScript, type Route } from './fab-page-script.js';

/** The values the C++ side interpolates, in the order the script's slots take them. */
const searchScript = (over: { q?: string; seller?: string; types?: string; limit?: number; free?: boolean } = {}): string =>
  fillSlots(rawScript('McpFabSearchOperation.cpp'), [
    'req-1', over.q ?? '', over.seller ?? '', over.types ?? '', over.limit ?? 12, over.free === true ? 'true' : 'false',
    rawScript('McpFabListingScript.cpp'),
  ]);

const row = (over: Record<string, unknown> = {}): Record<string, unknown> => ({
  uid: 'u-1',
  title: 'Concrete Barrier',
  listingType: '3d-model',
  user: { sellerName: 'Quixel Megascans' },
  category: { name: 'Props' },
  averageRating: 4.5,
  ratings: { averageRating: 4.5, total: 31 },
  reviewCount: 40,
  startingPrice: { price: 0, currencyCode: 'USD' },
  isFree: false,
  licenses: [{ name: 'CC0', isCc0: true }],
  assetFormats: [{ assetFormatType: { code: 'gltf' } }, { assetFormatType: { code: 'fbx' } }],
  publishedAt: '2024-03-01T00:00:00Z',
  tags: [{ name: 'concrete' }, 'barrier'],
  ...over,
});

const catalog = (rows: unknown[]): Route[] => [[/\/i\/listings\/search/u, { body: { results: rows } }]];

const search = async (script: string, rows: unknown[] = [row()]) => {
  const run = await runPageScript(script, catalog(rows));
  expect(run.errors).toEqual([]);
  expect(run.results).toHaveLength(1);
  return { run, listings: (run.results[0]?.listings ?? []) as Record<string, unknown>[] };
};

describe('the search script: the query it builds', () => {
  it('asks for the whole catalog when nothing narrows it', async () => {
    const { run } = await search(searchScript());

    expect(run.fetches).toEqual(['GET https://www.fab.com/i/listings/search?count=12']);
  });

  it('adds the publisher and the content kind, each as an encoded query value', async () => {
    const { run } = await search(searchScript({ q: 'concrete wall', seller: 'Quixel Megascans', types: 'material', free: true, limit: 5 }));

    const url = run.fetches[0] ?? '';
    expect(url).toContain('count=5');
    expect(url).toContain('&is_free=1');
    expect(url).toContain('&q=concrete%20wall');
    expect(url).toContain('&listing_types=material');
    expect(url).toContain('&seller=Quixel%20Megascans');
  });

  it('puts nothing of a filter into the query when it is empty', async () => {
    const { run } = await search(searchScript({ q: 'rock' }));

    expect(run.fetches[0]).not.toMatch(/seller|listing_types/u);
  });
});

describe('the search script: what a row says', () => {
  it('names the publisher, category, rating, price, license and formats the search returned', async () => {
    const { listings } = await search(searchScript());

    expect(listings[0]).toMatchObject({
      uid: 'u-1',
      title: 'Concrete Barrier',
      listingType: '3d-model',
      seller: 'Quixel Megascans',
      category: 'Props',
      averageRating: 4.5,
      ratingCount: 31,
      price: 0,
      currency: 'USD',
      isFree: true,
      isCc0: true,
      publishedAt: '2024-03-01T00:00:00Z',
      formats: ['gltf', 'fbx'],
      tags: ['concrete', 'barrier'],
    });
  });

  it('derives isFree from the price and never reports the listing flag that disagrees with it', async () => {
    const { listings } = await search(searchScript(), [
      row({ isFree: false, startingPrice: { price: 0 } }),
      row({ uid: 'u-2', isFree: true, startingPrice: { price: 19.99, currencyCode: 'EUR' } }),
    ]);

    expect(listings[0]).toMatchObject({ isFree: true });
    expect(listings[1]).toMatchObject({ isFree: false, price: 19.99, currency: 'EUR' });
    expect(JSON.stringify(listings)).not.toContain('rawIsFree');
  });

  it('leaves out what the row did not carry, rather than reporting zero or empty', async () => {
    const { listings } = await search(searchScript(), [{ uid: 'u-bare', title: 'Bare', listingType: 'material' }]);
    const bare = listings[0] ?? {};

    for (const absent of ['seller', 'category', 'averageRating', 'ratingCount', 'isCc0', 'publishedAt', 'formats', 'currency']) {
      expect(bare, absent).not.toHaveProperty(absent);
    }
    expect(bare).toMatchObject({ uid: 'u-bare', tags: [] });
  });

  it('names the keys of a price it does not know instead of guessing', async () => {
    const { listings } = await search(searchScript(), [row({ isFree: true, startingPrice: { amountInCents: 900 } })]);

    expect(listings[0]).toMatchObject({ isFree: true });
    expect(listings[0]).not.toHaveProperty('price');
    expect(String(listings[0]?.priceShape)).toContain('amountInCents');
  });

  it('reads the facts with the same function the listing details use', () => {
    const script = rawScript('McpFabSearchOperation.cpp');
    expect(script).not.toContain('function priceOf');
    expect(script).toContain('listingFacts(x, row)');
    expect(rawScript('McpFabListingScript.cpp')).toContain('function listingFacts');
  });

  it('fetches nothing per row and posts nothing', async () => {
    const { run } = await search(searchScript({ limit: 3 }), [row(), row({ uid: 'u-2' }), row({ uid: 'u-3' })]);

    expect(run.fetches).toHaveLength(1);
    expect(run.posts).toHaveLength(0);
    expect(rawScript('McpFabSearchOperation.cpp')).not.toMatch(/document\.cookie|csrf|add-to-library|download-info/iu);
  });
});
