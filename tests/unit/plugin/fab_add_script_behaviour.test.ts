// What the add's page script does with Fab's answers, run against a scripted page.
//
// These are behaviour checks on the JavaScript the adapter sends into Fab's authenticated page: which
// format it picks, which file and quality tier, what it reports to C++, and that nothing secret is in
// the reply. The security contracts pin what the source may contain; this pins what it does.

import vm from 'node:vm';
import { describe, expect, it } from 'vitest';

import { fillSlots, rawScript, runPageScript, type Route } from './fab-page-script.js';

const LISTING = 'ac2818b3-7d35-4cf5-a1af-cbf8ff5c61c1';
const CSRF = 'csrf-value-that-must-never-be-reported';
const SIGNED_URL = 'https://cdn.example.invalid/signed?sig=must-never-be-reported';

/** combine mirrors the C++ side: -1 when the caller said nothing about merging meshes, 0 for false, 1 for true. */
const addScript = (engine = '5.8', combine = -1): string =>
  fillSlots(rawScript('McpFabAddToProject.cpp'), [
    'req-1', LISTING, engine, combine, rawScript('McpFabSelectionScript.cpp'), rawScript('McpFabDownloadScript.cpp'),
  ]);

const listing = (over: Record<string, unknown> = {}): Record<string, unknown> => ({
  title: 'Concrete Barrier',
  user: { sellerName: 'Quixel Megascans' },
  assetFormats: [{ assetFormatType: { code: 'gltf' } }, { assetFormatType: { code: 'fbx' } }],
  licenses: [{ offerId: 'offer-1' }],
  ...over,
});

const tiers = (sizes: Partial<Record<'raw' | 'high' | 'mid' | 'low', number | null>>) =>
  (['raw', 'high', 'mid', 'low'] as const).map((tier) => ({
    name: `concrete_barrier_ubitfhtfa_ue_${tier}.zip`,
    uid: `uid-${tier}`,
    fileSize: sizes[tier] ?? null,
  }));

const routes = (formatBody: unknown, code: string, listingBody: unknown = listing()): Route[] => [
  [/\/i\/listings\/[^/]+$/u, { body: listingBody }],
  [/\/add-to-library$/u, { status: 200, body: {} }],
  [new RegExp(`/asset-formats/${code}$`, 'u'), { body: formatBody }],
  [/\/download-info/u, { body: { downloadInfo: [{ url: SIGNED_URL, distributionPointBaseUrls: [] }] } }],
];

describe('the add script: a Megascans listing', () => {
  const sizes = { raw: 268_000_000, high: 64_000_000, mid: 16_000_000, low: 700_000 };

  it('takes the high tier by default and reports the file, tier and size', async () => {
    const run = await runPageScript(addScript(), routes({ files: tiers(sizes) }, 'gltf'), `fab_csrftoken=${CSRF}`);

    expect(run.results).toHaveLength(1);
    const [reply] = run.results;
    expect(reply).toMatchObject({
      accepted: true,
      formatCode: 'gltf',
      quality: 'high',
      downloadBytes: 64_000_000,
      versionName: 'concrete_barrier_ubitfhtfa_ue_high.zip',
    });
    expect(run.addToProject).toHaveLength(1);
    expect(run.addToProject[0]?.metadata).toMatchObject({ AssetType: 'gltf', IsQuixel: true });
  });

  it('leaves the size out when Fab publishes none, rather than reporting zero', async () => {
    const run = await runPageScript(addScript(), routes({ files: tiers({}) }, 'gltf'), `fab_csrftoken=${CSRF}`);

    expect(run.results[0]).toMatchObject({ accepted: true, quality: 'high' });
    expect(run.results[0]).not.toHaveProperty('downloadBytes');
  });

  it('never reports the csrf value or the signed download URL', async () => {
    const run = await runPageScript(addScript(), routes({ files: tiers(sizes) }, 'gltf'), `fab_csrftoken=${CSRF}`);

    const everything = JSON.stringify([run.results, run.errors, run.fetches]);
    expect(everything).not.toContain(CSRF);
    expect(everything).not.toContain('must-never-be-reported');
    // The token is used, once, as the claim's header: that is the only place it may appear.
    expect(run.posts[0]?.headers['X-CSRFToken']).toBe(CSRF);
  });
});

describe('the add script: an unreal-engine pack', () => {
  const versions = [
    { name: 'Pack 5.0', uid: 'v50', engineVersions: ['UE_5.0', 'UE_5.1'] },
    { name: 'Pack 5.8', uid: 'v58', engineVersions: ['UE_5.7', 'UE_5.8'] },
  ];
  const packListing = listing({ user: { sellerName: 'Someone' }, assetFormats: [{ assetFormatType: { code: 'unreal-engine' } }] });

  it('matches the running engine even though Fab spells its versions UE_5.x', async () => {
    const run = await runPageScript(addScript('5.8'), routes({ versions }, 'unreal-engine', packListing), `fab_csrftoken=${CSRF}`);

    expect(run.results[0]).toMatchObject({ accepted: true, engineExactMatch: true, versionName: 'Pack 5.8', formatCode: 'unreal-engine' });
    // A pack publishes no size: unknown is not zero.
    expect(run.results[0]).not.toHaveProperty('downloadBytes');
  });

  it('falls back to the first version and says the engine did not match', async () => {
    const run = await runPageScript(addScript('5.9'), routes({ versions }, 'unreal-engine', packListing), `fab_csrftoken=${CSRF}`);

    expect(run.results[0]).toMatchObject({ accepted: true, engineExactMatch: false, versionName: 'Pack 5.0' });
  });
});

describe('the add script: a listing with nothing importable', () => {
  it('refuses with the formats it saw', async () => {
    const run = await runPageScript(
      addScript(),
      routes({}, 'none', listing({ assetFormats: [{ assetFormatType: { code: 'weird-format' } }] })),
      `fab_csrftoken=${CSRF}`,
    );

    expect(run.results[0]).toMatchObject({ error: 'NO_IMPORTABLE_FORMAT', formatCodes: ['weird-format'] });
    expect(run.addToProject).toHaveLength(0);
  });
});

describe('the add script: a scene-sized mesh file', () => {
  // Fab's own importer merges every mesh in the file into one static mesh; a 189 MB city became one
  // mesh that needed 14 GB to build and held the editor for many minutes.
  const scene = (bytes: number | null) => ({ files: [{ name: 'modern_city.zip', uid: 'u-city', fileSize: bytes, fileType: 'fbx' }] });
  const cityListing = listing({ user: { sellerName: 'Some Studio' }, assetFormats: [{ assetFormatType: { code: 'fbx' } }] });
  const city = (bytes: number | null): Route[] => routes(scene(bytes), 'fbx', cityListing);

  it('is refused before anything is downloaded unless the caller chose a way to import it', async () => {
    const run = await runPageScript(addScript('5.8', -1), city(189_000_000), `fab_csrftoken=${CSRF}`);

    expect(run.results[0]).toMatchObject({ error: 'LARGE_SCENE_FILE', formatCode: 'fbx', downloadBytes: 189_000_000, combinesMeshes: true });
    expect(run.addToProject).toHaveLength(0);
    expect(run.fetches.some((line) => line.includes('/download-info'))).toBe(false);
  });

  it('goes ahead when the caller asks for separate meshes, or explicitly accepts one merged mesh', async () => {
    for (const combine of [0, 1]) {
      const run = await runPageScript(addScript('5.8', combine), city(189_000_000), `fab_csrftoken=${CSRF}`);

      expect(run.results[0], `combine=${combine}`).toMatchObject({ accepted: true, combinesMeshes: true, downloadBytes: 189_000_000 });
      expect(run.addToProject).toHaveLength(1);
    }
  });

  it('is not held back when the file is small or its size is unknown', async () => {
    for (const bytes of [12_000_000, null]) {
      const run = await runPageScript(addScript('5.8', -1), city(bytes), `fab_csrftoken=${CSRF}`);

      expect(run.results[0], `size ${String(bytes)}`).toMatchObject({ accepted: true, combinesMeshes: true });
    }
  });

  it('never applies to Megascans, which has its own importer, or to a packaged build', async () => {
    const megascans = await runPageScript(
      addScript('5.8', -1),
      routes({ files: tiers({ high: 900_000_000 }) }, 'gltf'),
      `fab_csrftoken=${CSRF}`,
    );
    expect(megascans.results[0]).toMatchObject({ accepted: true, combinesMeshes: false });

    const pack = await runPageScript(
      addScript('5.8', -1),
      routes({ versions: [{ name: 'Pack', uid: 'v', engineVersions: ['UE_5.8'], fileSize: 900_000_000 }] }, 'unreal-engine',
        listing({ user: { sellerName: 'Someone' }, assetFormats: [{ assetFormatType: { code: 'unreal-engine' } }] })),
      `fab_csrftoken=${CSRF}`,
    );
    expect(pack.results[0]).toMatchObject({ accepted: true, combinesMeshes: false });
  });
});

describe('the add script: a listing Fab will not hand a download for', () => {
  const files = { files: tiers({ high: 64_000_000 }) };
  const withOffers = (licenses: unknown[]) => listing({ licenses });
  const refuseDownloads = (status: number, detail: string): Route => [/\/download-info/u, { status, body: { detail } }];
  const claim = (status: number, text: string): Route => [/\/add-to-library$/u, { status, text }];
  const run = (listingBody: unknown, extra: Route[], format: unknown = files) =>
    runPageScript(
      addScript(),
      [...extra, ...routes(format, 'gltf', listingBody)],
      `fab_csrftoken=${CSRF}`,
    );
  const detailOf = (reply: unknown): string => String((reply as { stepDetail?: string }).stepDetail);

  it('names the claim as the step that failed, with the words Fab used', async () => {
    const result = await run(withOffers([{ offerId: 'o1', isCc0: true }]), [
      claim(403, '{"detail":"This listing requires the seller licence"}'),
      refuseDownloads(404, 'Not found'),
    ]);

    expect(result.results[0]).toMatchObject({ error: 'NO_DOWNLOAD_URL', failedStep: 'claim', stepStatus: 403 });
    expect(detailOf(result.results[0])).toContain('requires the seller licence');
  });

  it('names the license step when the listing publishes no offer to claim', async () => {
    const result = await run({ ...listing(), licenses: [] }, [refuseDownloads(404, 'Not found')]);

    expect(result.results[0]).toMatchObject({ error: 'NO_DOWNLOAD_URL', failedStep: 'license' });
  });

  it('names download-info, with its status and words, when the claim went through', async () => {
    const result = await run(listing(), [refuseDownloads(404, 'No such file for this account')]);

    expect(result.results[0]).toMatchObject({ error: 'NO_DOWNLOAD_URL', failedStep: 'download-info', stepStatus: 404 });
    expect(detailOf(result.results[0])).toContain('No such file for this account');
  });

  it('tries the next free offer when Fab refuses the first, and carries on once one is accepted', async () => {
    let claims = 0;
    const result = await run(
      withOffers([{ offerId: 'paid', priceTier: { price: 10 } }, { offerId: 'free-a', isCc0: true }, { offerId: 'free-b', priceTier: { price: 0 } }]),
      [[/\/add-to-library$/u, () => (claims++ === 0 ? { status: 403, text: 'no' } : { status: 200 })]],
    );

    expect(result.posts.map((post) => post.form.offer_id)).toEqual(['free-a', 'free-b']);
    expect(result.results[0]).toMatchObject({ accepted: true, entitleStatus: 200 });
  });

  it('finds the address under an unfamiliar key, but never takes a preview or thumbnail', async () => {
    const odd = await run(listing(), [[/\/download-info/u, { body: { downloadInfo: [{ signedDownloadLink: SIGNED_URL }] } }]]);
    expect(odd.results[0]).toMatchObject({ accepted: true });
    expect(odd.addToProject[0]?.url).toBe(SIGNED_URL);

    const pictures = { downloadInfo: [{ previewUrl: 'https://cdn.example.invalid/p.png', thumbnailUrl: 'https://cdn.example.invalid/t.png' }] };
    const previewOnly = await run(listing(), [[/\/download-info/u, { body: pictures }]]);
    expect(previewOnly.results[0]).toMatchObject({ error: 'NO_DOWNLOAD_URL', failedStep: 'download-info' });
    expect(previewOnly.addToProject).toHaveLength(0);
    expect(detailOf(previewOnly.results[0])).toContain('previewUrl');
  });
});

describe('the shared selection functions', () => {
  const selection = vm.runInNewContext(
    `${rawScript('McpFabSelectionScript.cpp')}; ({ engineNum, tierOf, pickFile, fileBytes })`,
  ) as {
    engineNum: (value: string) => number;
    tierOf: (name: string) => string;
    pickFile: (files: unknown[], code: string) => { name: string } | null;
    fileBytes: (file: unknown) => number;
  };

  it('reads engine strings in every spelling Fab uses', () => {
    expect(selection.engineNum('UE_5.8')).toBe(5008);
    expect(selection.engineNum('5.8')).toBe(5008);
    expect(selection.engineNum('5.8.3')).toBe(5008);
    expect(selection.engineNum('five')).toBe(-1);
  });

  it('reads the tier from the name suffix, the last one winning', () => {
    expect(selection.tierOf('concrete_barrier_ubitfhtfa_ue_high.zip')).toBe('high');
    expect(selection.tierOf('city_high_rise_ubit_ue_low.zip')).toBe('low');
    expect(selection.tierOf('barrier_raw.fbx')).toBe('raw');
    expect(selection.tierOf('barrier.zip')).toBe('');
  });

  it('prefers high, then mid, then low, and raw only when nothing else is offered', () => {
    const file = (name: string) => ({ name, uid: name });
    expect(selection.pickFile([file('a_raw.zip'), file('a_low.zip'), file('a_mid.zip')], 'gltf')?.name).toBe('a_mid.zip');
    expect(selection.pickFile([file('a_raw.zip')], 'gltf')?.name).toBe('a_raw.zip');
    expect(selection.pickFile([], 'gltf')).toBeNull();
  });

  it('reports size only when Fab gives one', () => {
    expect(selection.fileBytes({ fileSize: 64 })).toBe(64);
    expect(selection.fileBytes({ fileSize: null })).toBe(-1);
    expect(selection.fileBytes({})).toBe(-1);
  });
});
