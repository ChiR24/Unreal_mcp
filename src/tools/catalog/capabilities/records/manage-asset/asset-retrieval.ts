// Asset retrieval records: assets found by what they show and what they are, not only by their names (packs whose
// names say little). The calling agent looks at thumbnail_sheet's pictures and writes captions with caption_assets;
// find_assets_like ranks by words, captions, the look of the stored thumbnails, colour and size.
// Native Domains/AssetQuery/Retrieval (McpAsset*).

import type { CapabilityPolicy } from '../../model.js';
import type { RecordSpec } from './builder.js';
import { bool, ex, MEDIUM, num, READ, READ_POLICY, r, str, WRITE } from './builder.js';
import { schema } from '../shared/record-presets.js';

// Captions live in the project's own file under Saved/, never in the assets: no consent, as save_recipe.
const CAPTION_POLICY: CapabilityPolicy = { requiredScope: 'write', consent: 'none', dataAccess: 'project-write' };
const FOLDER = str('Folder to look in, recursively (default /Game).');
const CLASS = str('Only assets of this class, by its name (StaticMesh, Texture2D, MaterialInstanceConstant, SoundWave, Blueprint...).');

export const ASSET_RETRIEVAL_RECORDS: readonly RecordSpec[] = [
  r('thumbnail_sheet', 'asset', 'Show a page of a folder\'s assets as one numbered picture: each tile is the thumbnail the editor keeps for the asset (stored in its package when it was saved from the editor, or made this session), labelled with its number and name, three across; an asset with none shows a plain grey tile and thumbnail false in its row (none is rendered: that would load the asset; asset.process_asset process=thumbnail makes one). By default only assets without a caption are shown (uncaptionedOnly), so captioning a page with caption_assets and asking again brings the next ones. Rows give each number its assetPath and class; nextOffset is the next page. The picture is saved under Saved/Captures (file) and returned as an image.',
    schema({
      folder: FOLDER,
      class: CLASS,
      uncaptionedOnly: bool('Only assets with no caption yet (default true); false shows every asset.'),
      offset: { type: 'number', minimum: 0, description: 'How many assets of the list to skip (default 0): the previous reply\'s nextOffset.' },
      limit: { type: 'number', minimum: 1, maximum: 16, description: 'Assets on the page (1-16, default 12).' },
    }),
    schema({
      success: bool('Operation succeeded.'),
      assets: { type: 'array', description: 'The page, in tile order.', items: { type: 'object', additionalProperties: false, properties: {
        number: num('The number on its tile (its place in the whole list, from 1).'),
        assetPath: str('Its package path.'),
        class: str('Its class.'),
        thumbnail: bool('False when the editor keeps no thumbnail for it (its tile is grey).'),
      } } },
      total: num('Assets in the whole list (after uncaptionedOnly).'),
      nextOffset: num('The offset of the next page; absent on the last.'),
      file: str('The picture\'s file under Saved/Captures.'),
      width: num('Picture width in pixels.'),
      height: num('Picture height in pixels.'),
      imageBase64: str('The picture as a PNG, base64, returned as an image; left out past 3 MB (the file has it).'),
      mimeType: str('image/png, with imageBase64.'),
    }, ['success']),
    READ, READ_POLICY, MEDIUM,
    { topics: ['asset thumbnails', 'contact sheet of assets', 'see the assets', 'pictures of the assets', 'preview assets', 'thumbnail grid'],
      whenToUse: [
        'An agent must see what a folder of assets holds (a pack whose names say little) before captioning or choosing them.',
        'Captioning a library: show a page, caption it with caption_assets, show the next.',
      ],
      whenNotToUse: [
        'One asset\'s thumbnail must be made or refreshed: use asset.process_asset process=thumbnail.',
        'Assets must be found by a description: use find_assets_like.',
      ],
      examples: [ex('Show the first uncaptioned rocks', { folder: '/Game/Rocks', class: 'StaticMesh', limit: 9 },
        { success: true, assets: [{ number: 1, assetPath: '/Game/Rocks/SM_Rock_01', class: 'StaticMesh', thumbnail: true }], total: 40, nextOffset: 9 })] }
  ),

  r('caption_assets', 'asset', 'Keep captions and tags for assets, as the calling agent writes them after looking at thumbnail_sheet: each {assetPath, caption (what it shows, up to 300 characters), tags (up to 12 words or short phrases)} replaces that asset\'s caption; an entry with an empty caption and no tags takes the caption away (also for an asset since deleted). They are kept in the project\'s caption file (Saved/EngineWire/AssetCaptions.json), never in the assets, which stay untouched; find_assets_like searches them. All or nothing: ASSET_NOT_FOUND (missing lists the paths), INVALID_PATH or WRITE_FAILED (the file could not be written) keeps none.',
    schema({
      captions: { type: 'array', minItems: 1, maxItems: 64, description: 'The captions to keep (1-64).', items: { type: 'object', additionalProperties: false, required: ['assetPath'], properties: {
        assetPath: str('The asset, by package path (/Game/Rocks/SM_Rock_01).'),
        caption: { type: 'string', maxLength: 300, description: 'What it shows, in plain words (a mossy grey boulder, flat on top); empty with no tags removes the caption.' },
        tags: { type: 'array', maxItems: 12, items: { type: 'string', maxLength: 40 }, description: 'Words or short phrases to find it by (rock, moss, cliff edge).' },
      } } },
    }, ['captions']),
    schema({
      success: bool('Operation succeeded.'),
      captioned: num('Captions kept.'),
      removed: num('Captions taken away.'),
      totalCaptions: num('Captions in the project\'s caption file now.'),
      file: str('The caption file.'),
    }, ['success']),
    WRITE, CAPTION_POLICY, MEDIUM,
    { topics: ['caption assets', 'tag assets', 'describe assets', 'label assets for search', 'asset captions'],
      whenToUse: ['After looking at a thumbnail_sheet page, its assets must be described so later searches find them by what they show.'],
      whenNotToUse: [
        'Gameplay tags or asset metadata inside the asset must be set: use asset.set_metadata.',
        'Assets must be renamed: use asset.rename.',
      ],
      examples: [ex('Caption two rocks', { captions: [{ assetPath: '/Game/Rocks/SM_Rock_01', caption: 'mossy grey boulder, flat top', tags: ['rock', 'moss'] }, { assetPath: '/Game/Rocks/SM_Rock_02', caption: 'tall jagged cliff rock', tags: ['rock', 'cliff'] }] },
        { success: true, captioned: 2, removed: 0, totalCaptions: 2 })] }
  ),

  r('find_assets_like', 'asset', 'Find assets by what they are and what they look like, best first: query words are matched (BM25) against each asset\'s name and folders, class, and the captions and tags caption_assets kept; likeAsset finds assets like an example: by its own name and folder words (its pack), by the look of its stored thumbnail (colours, edge directions, outline) and by its size, among assets of its class unless class says otherwise; colour (#RRGGBB or a name: red, orange, yellow, green, cyan, blue, purple, pink, brown, black, white, grey) ranks assets by how much of their thumbnail is that colour; sizeCm by how near their largest dimension is (meshes). Every criterion given ranks the assets it can judge and the rankings are fused (Reciprocal Rank Fusion); each row\'s why names the criteria that ranked it and its place there. The index of the assets (Saved/EngineWire/AssetIndex.json) is brought up to date first, reading thumbnails from the packages without loading the assets, for up to 10 s a call: pending counts those not read yet (call again). Refused with ASSET_NOT_FOUND (likeAsset), INVALID_PATH, INVALID_ARGUMENT (a colour it cannot read; a query with no word of two letters or more and nothing else asked).',
    schema({
      query: str('Words to find (mossy rock, red barrel): matched against names, folders, class, captions and tags.'),
      likeAsset: str('An example asset, by package path: assets like it are found (its own pack first, then by look and size).'),
      colour: str('A colour the asset should mostly be: #RRGGBB or red, orange, yellow, green, cyan, blue, purple, pink, brown, black, white, grey.'),
      sizeCm: { type: 'number', minimum: 1, description: 'About how big, in cm (the largest of its three dimensions; meshes only).' },
      class: str('Only assets of this class by its name (default: likeAsset\'s class, else every class).'),
      folder: FOLDER,
      limit: { type: 'number', minimum: 1, maximum: 50, description: 'Rows returned (1-50, default 10).' },
    }, [], ['query', 'likeAsset', 'colour', 'sizeCm']),
    schema({
      success: bool('Operation succeeded.'),
      assets: { type: 'array', description: 'Best first.', items: { type: 'object', additionalProperties: false, properties: {
        assetPath: str('Its package path.'),
        class: str('Its class.'),
        score: num('Its fused score (the sum over criteria of 1 / (60 + place)).'),
        why: { type: 'array', items: { type: 'string' }, description: 'Each criterion that ranked it with its place there: words, name (likeAsset\'s words), look, size (likeAsset\'s), colour, sizeCm.' },
        caption: str('Its caption, when it has one.'),
        sizeCm: { type: 'array', items: { type: 'number' }, description: 'Its size as [x, y, z] in cm (meshes).' },
      } } },
      matched: num('Assets any criterion ranked.'),
      considered: num('Assets looked at (those under folder of the class, with an index entry).'),
      pending: num('Assets not read into the index yet for want of time: another call includes them.'),
    }, ['success']),
    READ, READ_POLICY, MEDIUM,
    { topics: ['find similar assets', 'assets like this one', 'find an asset by description', 'search assets by appearance', 'find a mesh resembling',
        'similar meshes', 'find assets by colour', 'find assets by size'],
      whenToUse: [
        'The right asset must be found among many whose names say little (a pack of SM_Rock_01 to SM_Rock_40): by description, by an example or by colour and size.',
        'Swapping an asset for another like it, from the same pack or another.',
      ],
      whenNotToUse: [
        'Assets must be listed by name, class or folder only: use asset.query_asset lookup=search.',
        'Where a piece of text appears in assets: use asset.query_asset lookup=text.',
      ],
      examples: [ex('Rocks like an example', { likeAsset: '/Game/Rocks/SM_Rock_01', limit: 5 },
        { success: true, assets: [{ assetPath: '/Game/Rocks/SM_Rock_07', class: 'StaticMesh', score: 0.049, why: ['name 1', 'look 2', 'size 1'] }], matched: 39, considered: 40, pending: 0 })] }
  ),
];
