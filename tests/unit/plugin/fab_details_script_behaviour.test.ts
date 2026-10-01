// What the listing details' page script reports, run against a scripted page.
//
// The details answer two things a caller needs before adding: what the listing is, and what the add would
// do with it. These run the JavaScript the adapter sends into Fab's page against canned responses, so the
// shapes the reply takes -- and the ones it must leave out -- are checked without an editor.

import { describe, expect, it } from 'vitest';

import { fillSlots, rawScript, runPageScript, type Route } from './fab-page-script.js';

const LISTING = 'deab69d8-fee9-469b-8b45-579283a8ce46';

const detailsScript = (engine = '5.8'): string =>
  fillSlots(rawScript('McpFabDetailsOperation.cpp'), [
    'req-1', LISTING, engine, rawScript('McpFabSelectionScript.cpp'), rawScript('McpFabListingScript.cpp'),
  ]);

const listing = (over: Record<string, unknown> = {}): Record<string, unknown> => ({
  title: 'Concrete Barrier',
  listingType: '3d-model',
  description: 'A barrier.',
  tags: [{ name: 'concrete' }, 'barrier'],
  user: { sellerName: 'Quixel Megascans' },
  assetFormats: [{ assetFormatType: { code: 'gltf' } }, { assetFormatType: { code: 'fbx' } }],
  category: { name: 'Props', path: ['3d-models', 'props'] },
  averageRating: 4.5,
  ratings: { averageRating: 4.5, total: 31 },
  licenses: [{ name: 'Standard License', isCc0: false }, { name: 'CC0', isCc0: true }],
  startingPrice: { price: 0, currencyCode: 'USD' },
  isFree: false,
  publishedAt: '2024-03-01T00:00:00Z',
  ...over,
});

const tiers = (sizes: Partial<Record<'raw' | 'high' | 'mid' | 'low', number | null>>, prefix = 'barrier_ue') =>
  (['raw', 'high', 'mid', 'low'] as const).map((tier) => ({ name: `${prefix}_${tier}.zip`, uid: `u-${tier}`, fileSize: sizes[tier] ?? null }));

type Format = { code: string; body: Record<string, unknown> };
const routes = (listingBody: unknown, formats: readonly Format[]): Route[] => [
  [/\/i\/listings\/[^/]+$/u, { body: listingBody }],
  [/\/asset-formats$/u, {
    body: formats.map((f) => ({ assetFormatType: { code: f.code }, files: (f.body.files as unknown[] | undefined) ?? [] })),
  }],
  ...formats.map((f): Route => [new RegExp(`/asset-formats/${f.code}$`, 'u'), { body: f.body }]),
];

const pack = (versions: unknown[], distributionMethod = 'asset_pack'): Format => ({
  code: 'unreal-engine',
  body: { distributionMethod, versions, files: versions.map((v) => ({ name: (v as { name: string }).name, fileSize: null })) },
});
const packListing = (over: Record<string, unknown> = {}) =>
  listing({ user: { sellerName: 'Epic Games' }, assetFormats: [{ assetFormatType: { code: 'unreal-engine' } }], ...over });

const reply = async (script: string, table: Route[]): Promise<Record<string, unknown>> => {
  const run = await runPageScript(script, table);
  expect(run.errors).toEqual([]);
  expect(run.results).toHaveLength(1);
  // Describing a listing claims nothing and sends nothing.
  expect(run.posts).toHaveLength(0);
  expect(run.fetches.every((line) => line.startsWith('GET '))).toBe(true);
  return run.results[0] ?? {};
};

describe('the details script: what the listing says about itself', () => {
  it('reports publisher, category, rating, licenses, price and date', async () => {
    const out = await reply(detailsScript(), routes(listing(), [{ code: 'gltf', body: { files: tiers({ high: 64_000_000 }) } }]));

    expect(out).toMatchObject({
      title: 'Concrete Barrier',
      seller: 'Quixel Megascans',
      category: 'Props',
      categoryPath: '3d-models/props',
      averageRating: 4.5,
      ratingCount: 31,
      licenseNames: ['Standard License', 'CC0'],
      isCc0: true,
      price: 0,
      currency: 'USD',
      publishedAt: '2024-03-01T00:00:00Z',
    });
  });

  it('derives isFree from the price, whatever the listing flag says, and never reports the flag', async () => {
    const free = await reply(detailsScript(), routes(listing({ isFree: false, startingPrice: { price: 0 } }), []));
    expect(free.isFree).toBe(true);

    const paid = await reply(detailsScript(), routes(listing({ isFree: true, startingPrice: { price: 12.5, currencyCode: 'EUR' } }), []));
    expect(paid).toMatchObject({ isFree: false, price: 12.5, currency: 'EUR' });
    expect(JSON.stringify([free, paid])).not.toContain('rawIsFree');
  });

  it('says what an unfamiliar price field looked like instead of guessing', async () => {
    const out = await reply(detailsScript(), routes(listing({ isFree: false, startingPrice: { amountInCents: 1200, tier: 'a' } }), []));

    expect(out).not.toHaveProperty('price');
    expect(out).toMatchObject({ isFree: false });
    expect(out.priceShape).toContain('amountInCents');
  });

  it('leaves out what the listing does not carry, rather than reporting zero or empty', async () => {
    const out = await reply(
      detailsScript(),
      routes({ title: 'Bare', assetFormats: [{ assetFormatType: { code: 'fbx' } }] }, [{ code: 'fbx', body: { files: [] } }]),
    );

    for (const absent of ['averageRating', 'ratingCount', 'category', 'categoryPath', 'licenseNames', 'isCc0', 'publishedAt', 'seller']) {
      expect(out, absent).not.toHaveProperty(absent);
    }
  });
});

describe('the details script: a Megascans listing', () => {
  const sizes = { raw: 268_000_000, high: 64_000_000, mid: 16_000_000, low: 700_000 };
  const table = routes(listing(), [{ code: 'gltf', body: { files: tiers(sizes) } }, { code: 'fbx', body: { files: tiers({ raw: 319_000_000 }, 'barrier') } }]);

  it('names the file the add would fetch, with exactly its size and tier', async () => {
    const out = await reply(detailsScript(), table);

    expect(out).toMatchObject({
      canAddToProject: true,
      addFormat: 'gltf',
      downloadFile: 'barrier_ue_high.zip',
      downloadBytes: 64_000_000,
      downloadSizeKnown: true,
      quality: 'high',
      qualities: ['raw', 'high', 'mid', 'low'],
    });
    expect(out).not.toHaveProperty('addBlockedReason');
  });

  it('lists every format with each file and its size', async () => {
    const out = await reply(detailsScript(), table);
    const formats = out.formats as { code: string; files: { name: string; bytes?: number; quality?: string }[] }[];

    expect(formats.map((f) => f.code)).toEqual(['gltf', 'fbx']);
    expect(formats[0]?.files).toContainEqual({ name: 'barrier_ue_high.zip', bytes: 64_000_000, quality: 'high' });
    expect(formats[1]?.files).toContainEqual({ name: 'barrier_raw.zip', bytes: 319_000_000, quality: 'raw' });
    // A file with no published size has no bytes at all, not a zero.
    expect(formats[1]?.files).toContainEqual({ name: 'barrier_high.zip', quality: 'high' });
  });

  it('is not held back by the scene-file warning, which is for the generic importer only', async () => {
    const out = await reply(detailsScript(), routes(listing(), [{ code: 'gltf', body: { files: tiers({ high: 900_000_000 }) } }]));

    expect(out).not.toHaveProperty('addWarnings');
  });
});

describe('the details script: an unreal-engine pack', () => {
  const versions = [
    { name: 'Pack 5.0 to 5.4', uid: 'v0', engineVersions: ['UE_5.0', 'UE_5.1', 'UE_5.4'] },
    { name: 'Pack 5.7', uid: 'v1', engineVersions: ['UE_5.7', 'UE_5.8'] },
  ];

  it('reports the engines the pack declares and that the running one is among them', async () => {
    const out = await reply(detailsScript('5.8'), routes(packListing(), [pack(versions)]));

    expect(out).toMatchObject({
      canAddToProject: true,
      addFormat: 'unreal-engine',
      distributionMethod: 'asset_pack',
      runningEngine: '5.8',
      engineVersions: ['UE_5.0', 'UE_5.1', 'UE_5.4', 'UE_5.7', 'UE_5.8'],
      supportsRunningEngine: true,
      engineMatch: 'exact',
      versionName: 'Pack 5.7',
      pickedEngineVersion: 'UE_5.8',
    });
    expect(out).not.toHaveProperty('addWarnings');
  });

  it('says unknown, never zero, when Fab publishes no size for the pack', async () => {
    const out = await reply(detailsScript('5.8'), routes(packListing(), [pack(versions)]));

    expect(out).toMatchObject({ downloadSizeKnown: false });
    expect(out).not.toHaveProperty('downloadBytes');
    // Nothing in the per-format files claims a size either.
    expect(JSON.stringify(out.formats)).not.toContain('"bytes"');
  });

  it('picks the highest build at or below the running engine, and warns that it is older', async () => {
    const out = await reply(detailsScript('5.6'), routes(packListing(), [pack(versions)]));

    expect(out).toMatchObject({ supportsRunningEngine: false, engineMatch: 'older', versionName: 'Pack 5.0 to 5.4', pickedEngineVersion: 'UE_5.4' });
    expect(String((out.addWarnings as string[])[0])).toMatch(/running engine 5\.6.*UE_5\.4.*older/u);
  });

  it('picks the lowest build above the running engine when none is at or below it', async () => {
    const out = await reply(detailsScript('4.27'), routes(packListing(), [pack(versions)]));

    expect(out).toMatchObject({ supportsRunningEngine: false, engineMatch: 'newer', versionName: 'Pack 5.0 to 5.4', pickedEngineVersion: 'UE_5.0' });
    expect(String((out.addWarnings as string[])[0])).toMatch(/newer/u);
  });

  it('says nothing about a match when no version declares an engine', async () => {
    const out = await reply(detailsScript('5.8'), routes(packListing(), [pack([{ name: 'Pack', uid: 'v', engineVersions: [] }])]));

    expect(out).toMatchObject({ canAddToProject: true, versionName: 'Pack', supportsRunningEngine: false });
    expect(out).not.toHaveProperty('engineMatch');
    expect(out).not.toHaveProperty('engineVersions');
  });
});

describe('the details script: what the add would refuse', () => {
  it('blocks a complete project, with the way forward in the reason', async () => {
    const out = await reply(detailsScript('5.8'), routes(packListing(), [pack([{ name: 'Project', uid: 'v', engineVersions: ['UE_5.8'] }], 'complete_project')]));

    expect(out).toMatchObject({
      canAddToProject: false,
      addBlockedCode: 'COMPLETE_PROJECT',
      addBlockedReason: 'complete project: create it as a new project from Fab, then migrate its content',
      distributionMethod: 'complete_project',
    });
  });

  it('blocks a MetaHuman listing with its own reason rather than the generic one', async () => {
    const out = await reply(
      detailsScript(),
      routes(listing({ title: 'Mason - Editable MetaHuman Character Preset', assetFormats: [{ assetFormatType: { code: 'metahuman' } }] }), []),
    );

    expect(out).toMatchObject({ canAddToProject: false, addBlockedCode: 'METAHUMAN_FORMAT' });
    expect(String(out.addBlockedReason)).toContain('MetaHuman');
    expect(out).not.toHaveProperty('addFormat');
  });

  it('keeps the generic reason for a listing with nothing importable', async () => {
    const out = await reply(detailsScript(), routes(listing({ assetFormats: [{ assetFormatType: { code: 'udata' } }] }), []));

    expect(out).toMatchObject({ canAddToProject: false, addBlockedCode: 'NO_IMPORTABLE_FORMAT' });
    expect(String(out.addBlockedReason)).toContain('ships no format Fab can import');
  });

  it('warns about a scene-sized mesh file, and only for the generic importer', async () => {
    const scene = { files: [{ name: 'modern_city.zip', uid: 'u', fileSize: 189_000_000, fileType: 'fbx' }] };
    const generic = await reply(
      detailsScript(),
      routes(listing({ user: { sellerName: 'Some Studio' }, assetFormats: [{ assetFormatType: { code: 'fbx' } }] }), [{ code: 'fbx', body: scene }]),
    );

    expect(generic).toMatchObject({ canAddToProject: true, downloadBytes: 189_000_000 });
    expect(String((generic.addWarnings as string[])[0])).toMatch(/189 MB.*LARGE_SCENE_FILE.*combineMeshes/u);

    const small = await reply(
      detailsScript(),
      routes(listing({ user: { sellerName: 'Some Studio' }, assetFormats: [{ assetFormatType: { code: 'fbx' } }] }), [
        { code: 'fbx', body: { files: [{ name: 'prop.zip', uid: 'u', fileSize: 4_000_000 }] } },
      ]),
    );
    expect(small).not.toHaveProperty('addWarnings');
  });

  it('says so when Fab answers no file list for the format the add would use', async () => {
    const out = await reply(detailsScript(), [
      [/\/i\/listings\/[^/]+$/u, { body: listing() }],
      [/\/asset-formats/u, { status: 404, body: {} }],
    ]);

    expect(out.canAddToProject).toBe(true);
    expect(String((out.addWarnings as string[])[0])).toContain('gltf');
  });
});

describe('the details script reads the same selection functions as the add', () => {
  it('interpolates the shared selection and listing scripts rather than carrying its own copy', () => {
    const script = rawScript('McpFabDetailsOperation.cpp');
    for (const own of ['function pickVersion', 'function pickFile', 'function tierOf', 'function engineNum', 'function priceOf']) {
      expect(script, `${own} must come from the shared script`).not.toContain(own);
    }
    expect(rawScript('McpFabSelectionScript.cpp')).toContain('function pickVersion');
    expect(rawScript('McpFabListingScript.cpp')).toContain('function priceOf');
  });

  it('only ever reads: it posts nothing, claims nothing and touches no cookie or token', () => {
    for (const file of ['McpFabDetailsOperation.cpp', 'McpFabListingScript.cpp', 'McpFabSelectionScript.cpp']) {
      expect(rawScript(file), file).not.toMatch(/method\s*:|POST|document\.cookie|csrf|add-to-library|download-info/iu);
    }
  });
});
