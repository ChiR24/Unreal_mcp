// Content-source discovery and ingestion: list_content_sources, migrate_assets.
//
// These two cover the "use assets that already exist on this machine" path —
// engine templates, engine/plugin content, and Quixel Bridge / Fab packs
// downloaded to the Megascans library. Bridge and Fab deliver cooked .uasset
// packs rather than source art, so pulling one in is a package copy plus an
// asset-registry scan, not an importer run; `asset.import` remains the route
// for FBX/PNG/WAV source files.

import type { CapabilityBehaviorSource } from '../../model.js';
import type { RecordSpec } from './builder.js';
import { arr, arrObj, bool, boundedLimit, ex, HIGH, LOW, MEDIUM, num, READ, READ_POLICY, r, str, WRITE_POLICY } from './builder.js';
import { schema } from '../shared/record-presets.js';

// A migration or Bridge import walks a whole content pack, so it is long-running by cost, and it
// copies files outside any transaction — dryRun is the preview and there is no
// undo. Re-running is safe: with overwrite off it skips what is already there.
const MIGRATE_BEHAVIOR: CapabilityBehaviorSource = {
  effect: 'write', idempotency: 'idempotent', longRunning: true,
  safeToRetry: true
};

const SOURCE_ROOTS =
  'engineTemplates | engineFeaturePacks | engineContent | enginePlugins | megascansLibrary | fabLibrary | projectContent | projectPlugins';

const SOURCE_ROOT_PARAM = str(
  `Content source root token. One of: ${SOURCE_ROOTS}. A filesystem path is never accepted here — the token is resolved plugin-side, so no directory outside these roots is reachable. megascansLibrary probes both the shell Documents folder and the profile Documents folder (OneDrive redirects the first) and honours MCP_MEGASCANS_LIBRARY_DIR; fabLibrary reads the Fab plugin's own UFabSettings.CacheDirectoryPath and honours MCP_FAB_LIBRARY_DIR. The Fab plugin owns its own sign-in and downloading — these roots only read what it already placed on disk.`
);

export const CONTENT_SOURCE_RECORDS: readonly RecordSpec[] = [
  r('list_content_sources', 'asset',
    'Enumerate reusable content already installed on this machine: engine templates (vehicle, first person, ...), engine and plugin content, and Quixel Bridge / Fab packs already downloaded by those plugins. Call this before authoring assets from scratch — a production-grade vehicle, track kit or Megascans surface is usually already on disk. Feed a returned sourceRoot + sourceId straight into asset.migrate_assets. Results are paginated (default 50): an unfiltered sweep finds several hundred sources because every engine plugin that ships content counts, so narrow with sourceRoot or filter rather than paging through all of them.',
    schema({
      sourceRoot: str(`Restrict the listing to one root. One of: ${SOURCE_ROOTS}. Omit to list every root.`),
      filter: str('Substring filter. Listing assets: a case-insensitive match on the asset name. Listing content sources: a case-sensitive match on the source id and, for plugins, the category.'),
      includePackageCounts: bool('Include packageCount per source. Costs a recursive file scan per returned entry, so leave off for a broad sweep and turn on once the candidate list is short.'),
      limit: boundedLimit(500, 50),
      offset: num('Zero-based offset into the full result set.')
    }, []),
    schema({
      success: bool('Operation succeeded.'),
      sources: arrObj('Discovered sources. Each entry carries sourceRoot, sourceId, kind (template | featurePack | megascansPack | plugin | contentFolder), hasContentFolder, migratable, and packageCount when requested.'),
      sourceCount: num('Sources on this page.'),
      totalCount: num('Total matched sources before pagination.'),
      limit: num('Applied page size.'),
      offset: num('Applied zero-based offset.'),
      hasMore: bool('True when more sources exist beyond this page.'),
      nextOffset: num('Offset for the next page, or -1 on the last page.'),
      // Keyed by root token, so the property set is the root list rather than a
      // fixed shape.
      rootDirectories: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Absolute directory each root token resolved to, so an operator can confirm where the Bridge library was found.' },
      missingRoots: arr('Root tokens whose directory does not exist on this machine.')
    }, ['success']),
    READ, READ_POLICY, MEDIUM,
    { dispatchAction: 'list_content_sources', 
      examples: [
        ex('Find the installed vehicle template', { sourceRoot: 'engineTemplates', filter: 'Vehicle' }, { success: true, sourceCount: 1 }),
        ex('List downloaded Quixel/Fab packs with counts', { sourceRoot: 'megascansLibrary', includePackageCounts: true }, { success: true })
      ] }
  ),

  r('list_fab_downloads', 'asset',
    'Report what the Fab plugin has already downloaded to this machine, with the cache directory it used. Pair with list_content_sources(sourceRoot="fabLibrary") and asset.migrate_assets to bring a downloaded pack into the project. This reads local state only: the archives Fab has finished fetching into its cache. Finding a listing is search_fab_listings and adding one is asset.import_marketplace_asset; a download that is still running is not listed until it ends (the add\'s import status shows its progress), and a unreal-engine pack is installed by Fab straight into the project, so it never appears here.',
    schema({}, []),
    schema({
      success: bool('Operation succeeded.'),
      downloads: arrObj('Cached Fab downloads. Each entry carries assetId and cachedFile.'),
      downloadCount: num('Number of cached downloads.'),
      cacheDirectory: str('Directory the Fab plugin caches downloads in.'),
      cacheDirectoryExists: bool('False when nothing has been downloaded yet.'),
      fabModuleAvailable: bool('True when the plugin was built against the Fab module and read the cache through its own API rather than scanning the directory.'),
      note: str('Guidance on what to do next given the current state.')
    }, ['success']),
    READ, READ_POLICY, LOW,
    { dispatchAction: 'list_fab_downloads',
      whenToUse: ['It must be known which Fab packs are already downloaded to this machine and where they are cached.'],
      whenNotToUse: ['A downloaded pack must be copied into the project (use asset.maintain_content with maintenance=migrate).'],
      examples: [ex('Check for downloaded Fab content', {}, { success: true, downloadCount: 0 })] }
  ),

  r('list_fab_library', 'asset',
    'List your Fab "My Library" entries that the Fab plugin has synced into the editor\'s data storage (TEDS). This is the searchable inventory of what your Fab account owns — distinct from list_fab_downloads, which only reports packs already downloaded to disk. Each row carries the listing AssetId, so a row can be handed straight to add_fab_asset_to_project instead of being a name you have to search for again. A table with no rows has never been synced, so this runs Fab\'s own sync itself (the Fab.TEDS.MyFolderIntegration console command, a thousand rows a page) and waits for rows, up to about 12 seconds and until their count has held still for a moment: syncTriggered and syncWaitedSeconds say that it happened. The remaining pages keep loading in the background, so a very large library may need a second call. A table that already holds rows is read as it is, because the sync first removes every row; to refresh one, run Fab.TEDS.MyFolderIntegration through control_editor.console_command. When no rows arrive the account\'s library is empty or the Fab tab is signed out: Fab.Login opens Epic\'s account portal, and no credential ever passes through this tool. A synced library is mostly engine versions and plugins carrying Source "uem", so filter for "fab" to see actual content. Columns are resolved by path, and the data storage is reached through the modular-features registry, so this never links the Fab module and keeps working when Fab changes its schema.',
    schema({
      columnTypes: arr('Column struct paths to read, for example "/Script/Fab.FabObjectNameColumn". Defaults to the name column plus "/Script/Fab.FabObjectColumn", which carries AssetId, ListingType, Seller and Source. Selecting a column is also the row filter, so naming one Fab does not write for every row will hide rows. Override this when a Fab update renames or adds columns; unresolved paths are reported rather than failing the call.'),
      filter: str('Case-sensitive substring matched against each serialized row. Use "fab" to drop the legacy "uem" engine and plugin entries that otherwise fill the row limit.'),
      limit: { type: 'number', default: 200, minimum: 1, maximum: 1000, description: 'Maximum rows to return, clamped plugin-side.' }
    }, []),
    schema({
      success: bool('Operation succeeded.'),
      entries: arrObj('Library rows. Each entry maps column struct name to that column\'s properties, read by reflection. FabObjectColumn.AssetId is the listing id add_fab_asset_to_project takes.'),
      entryCount: num('Rows returned.'),
      unresolvedColumnTypes: arr('Requested column paths that do not exist in this build — usually a Fab schema change.'),
      syncTriggered: bool('True when the table held no rows and Fab\'s library sync was run for this call.'),
      syncWaitedSeconds: num('How long this call waited for rows after starting the sync. Present only when the sync ran.'),
      syncSkipped: str('Why the sync was not run on an empty table: this editor registers no Fab sync command. Present only then.'),
      note: str('What the read found and what to do next: refresh the table, sign in, or call again for later pages.')
    }, ['success']),
    READ, READ_POLICY, MEDIUM,
    { dispatchAction: 'list_fab_library',
      whenToUse: ['The Fab listings the signed-in account owns must be listed, each with an id for adding to the project (an empty table is synced by the call itself).'],
      whenNotToUse: ['A listing from the library is ready to add to the project (use asset.import_marketplace_asset with marketplace=fab_listing).'],
      examples: [ex('List the synced Fab library, skipping legacy engine entries', { limit: 50, filter: 'fab' }, { success: true, entryCount: 0 })] }
  ),

  r('download_fab_asset', 'asset',
    'Download a Fab asset through the Fab plugin\'s own downloader (FFabDownloadRequest), so the transfer uses its HTTP/BuildPatchServices path rather than a parallel implementation that would miss its retry handling. IMPORTANT: the signed downloadUrl is NOT minted here — Fab issues it from its authenticated web session and no C++ entry point initiates that, so supply a URL obtained from the Fab tab\'s own flow. Once the pack lands, asset.migrate_assets with sourceRoot "fabLibrary" places it into /Game. Acquires content against the signed-in account, so it is an explicit-consent write.',
    schema({
      assetId: str('Fab asset id, used as the cache key.'),
      downloadUrl: str('Signed https download URL issued by Fab. Short-lived: a stale URL fails with DOWNLOAD_FAILED.'),
      destinationDirectory: str('Where the pack lands. Defaults to the Fab library cache directory, which list_fab_downloads and the fabLibrary source root both read.'),
      downloadType: { type: 'string', default: 'http', description: 'Transfer mode: "http" or "buildpatch". BuildPatchServices is for Marketplace-era packs; pointing it at a plain URL stalls rather than failing, so it must be opted into.' }
    }, ['assetId', 'downloadUrl']),
    schema({
      success: bool('True when the transfer completed.'),
      downloadSucceeded: bool('Downloader-reported success.'),
      servedFromCache: bool('True when the pack was already cached and no transfer occurred.'),
      completedBytes: num('Bytes transferred.'),
      totalBytes: num('Expected total bytes.'),
      destinationDirectory: str('Directory the pack landed in.'),
      downloadedFiles: arr('Files the downloader reported writing.'),
      note: str('How to place the downloaded pack into /Game.')
    }, ['success']),
    MIGRATE_BEHAVIOR, WRITE_POLICY, HIGH,
    { dispatchAction: 'download_fab_asset',
      whenToUse: ['A signed Fab download URL is in hand and the pack must be fetched to disk; it lands in the Fab cache, not in /Game.'],
      whenNotToUse: [
        'Only a listing id is known, with no signed URL (use marketplace=fab_listing, which adds the listing directly).',
        'Downloads already on disk must be listed, not fetched again (use asset.query_marketplace).',
      ],
      examples: [ex('Download a pack from a signed URL', { assetId: 'abc123', downloadUrl: 'https://example.invalid/signed' }, { success: false })] }
  ),

  r('get_fab_listing_details', 'asset',
    'Describe one Fab listing: what it is (description, tags, publisher, category, rating, licenses, price, publication date and a preview image) and what adding it would do (the format it imports, the build or file it fetches, its size, whether a build exists for the running engine, and why it would refuse), so a caller can choose between search hits and know before adding. Opens the Fab tab when it is closed and waits up to 15 seconds for its page to load (PAGE_NOT_READY after that) instead of failing on the first call; the account must be signed in there. It reads the listing and its asset-formats resources and claims nothing. canAddToProject says whether add_fab_asset_to_project can import the listing, and when it cannot, addBlockedCode and addBlockedReason say why: COMPLETE_PROJECT (a whole project cannot be added to an existing one: create it as a new project from Fab, then migrate its content), METAHUMAN_FORMAT (Fab hands a MetaHuman listing to a workflow the add does not drive) or NO_IMPORTABLE_FORMAT (it ships none of unreal-engine, gltf, glb, fbx, obj or usdz). Megascans listings are importable like any other, because the add claims the listing itself. addFormat is the format the add would import (unreal-engine first, else gltf, glb, fbx, obj, usdz). For a unreal-engine pack, versionName, pickedEngineVersion and engineMatch name the build it would take (the highest at or below the running engine, else the lowest above it), engineVersions lists every engine the pack declares and supportsRunningEngine says whether the running engine is one of them. For a source format, downloadFile, quality and qualities (Megascans offers raw, high, mid and low) name the file. downloadBytes is the size of exactly the file or build the add would fetch and is left out, with downloadSizeKnown false, when Fab publishes none, which is every pack: unknown is never reported as zero. formats lists every format with its files and their sizes. isFree is derived from price, because the listing\'s own isFree flag disagrees with it and is not reported. addWarnings lists what would surprise the caller: a pack with no build for the running engine, and a mesh file so large that the add refuses it until combineMeshes is chosen. The preview comes back as imageBase64, promoted into a real MCP image block rather than a URL, and is omitted rather than truncated past the reply cap. When a field cannot be read the response names the keys it did see, so a Fab schema change reports itself.',
    schema({ listingId: str('Fab listing uid. Restricted to [A-Za-z0-9_-], 64 characters max. fab_import_status also takes it, in place of operationId, to report the newest import of that listing.') }, ['listingId']),
    schema({
      success: bool('Listing was described.'),
      listingId: str('Listing that was described.'),
      title: str('Listing title.'),
      listingType: str('Content kind, e.g. 3d-model or material.'),
      description: str('Listing prose, truncated to 4000 characters.'),
      seller: str('Publisher name.'),
      category: str('Category name.'),
      categoryPath: str('Category path, slash-joined, when Fab publishes one.'),
      averageRating: num('Average rating as Fab reports it. Absent when the listing has none.'),
      ratingCount: num('How many ratings the average rests on. Absent when Fab publishes no count.'),
      licenseNames: arr('Names of the licenses the listing is offered under.'),
      isCc0: bool('True when a license of the listing is CC0 (public domain, no credit needed), read from the license names: Fab\'s own flag is also set on CC-BY licenses. Absent when the listing publishes no licenses.'),
      attributionRequired: bool('True when a license of the listing is Creative Commons Attribution (CC BY, CC BY-SA ...): an asset used under it needs a credit line in the project naming its title, author, source and license, and saying what was changed. Absent when the listing publishes no licenses.'),
      price: num('Starting price in Fab\'s own units; 0 is free. Absent when the price field has a shape this does not know (see priceShape).'),
      currency: str('Currency code of price, when Fab publishes one.'),
      isFree: bool('True when price is 0. Derived from price: the listing\'s own isFree flag disagrees with it and is never reported. When the price cannot be read this falls back to that flag and priceShape says so.'),
      priceShape: str('Key names of a price field this does not recognise, so a Fab change reports itself. Present only then.'),
      publishedAt: str('When the listing was published, as Fab states it.'),
      tags: arr('Listing tags.'),
      imageBase64: str('Preview image bytes, base64. Promoted to an MCP image content block.'),
      mimeType: str('Preview image MIME type.'),
      hasImage: bool('False when no preview could be inlined.'),
      imageOmitted: str('Present when the preview was skipped for exceeding the reply cap.'),
      descriptionKeys: arr('Listing keys observed when no description field matched.'),
      thumbnailShape: arr('Thumbnail keys observed when no image URL matched.'),
      assetFormats: arr('Asset format codes this listing ships, e.g. unreal-engine, fbx, gltf.'),
      hasUnrealBuild: bool('True when the listing ships a packaged unreal-engine build, which Fab imports through its pack workflow.'),
      formats: arrObj('Every format the listing ships: code, and files each with name, bytes (left out when Fab publishes no size) and quality when the name carries a tier.'),
      addFormat: str('The format add_fab_asset_to_project would import: unreal-engine, else gltf, glb, fbx, obj or usdz. Absent when none is importable.'),
      distributionMethod: str('How Fab distributes a unreal-engine listing: asset_pack (content for a project) or complete_project (a whole project). Present only for a listing that ships unreal-engine.'),
      runningEngine: str('The editor\'s engine version, major.minor, that supportsRunningEngine and engineMatch are judged against.'),
      engineVersions: arr('Every engine version a unreal-engine pack declares, lowest first, as Fab spells them (UE_5.4).'),
      supportsRunningEngine: bool('True when a build of the pack declares exactly the running engine. Present only for a unreal-engine listing that publishes versions.'),
      engineMatch: str('exact, older or newer: the build the add would take against the running engine. older is the newest build for an earlier engine, newer the oldest build for a later one. Absent when no version declares an engine.'),
      versionName: str('Name of the pack build the add would take.'),
      pickedEngineVersion: str('The engine version that build declares, for example UE_5.4.'),
      downloadFile: str('The file of a source format the add would download.'),
      quality: str('Quality tier of that file (raw, high, mid or low), when the listing publishes tiers.'),
      qualities: arr('The quality tiers the source format offers, best first. add_fab_asset_to_project takes the highest of high, mid, low, raw that exists.'),
      downloadBytes: num('Size in bytes of exactly the file or build the add would fetch. Absent when Fab publishes no size, which is every pack: unknown, never zero.'),
      downloadSizeKnown: bool('False when downloadBytes is absent because Fab publishes no size for what the add would fetch.'),
      canAddToProject: bool('True when add_fab_asset_to_project can import this listing: it ships unreal-engine as an asset pack, or gltf, glb, fbx, obj or usdz. False for a complete project, a MetaHuman listing and a listing with no importable format (addBlockedCode says which). Megascans listings are importable: the add claims the listing itself. Check this rather than hasUnrealBuild before adding.'),
      addBlockedCode: str('Present when canAddToProject is false: COMPLETE_PROJECT, METAHUMAN_FORMAT or NO_IMPORTABLE_FORMAT, the code the add would answer with.'),
      addBlockedReason: str('Present when canAddToProject is false: why this listing cannot be imported.'),
      addWarnings: arr('What would surprise the caller about adding this listing: a pack with no build for the running engine, or a mesh file large enough that the add refuses it until combineMeshes is chosen.')
    }, ['success']),
    READ, READ_POLICY, MEDIUM,
    { dispatchAction: 'get_fab_listing_details',
      whenToUse: ['A Fab search hit must be judged before adding it: description, seller, rating, price, file formats, sizes, and whether the project can import it.'],
      whenNotToUse: ['The listing is chosen and must be added to the project (use asset.import_marketplace_asset with marketplace=fab_listing).'],
      examples: [ex('Describe a listing before adding it', { listingId: 'ac2818b3-7d35-4cf5-a1af-cbf8ff5c61c1' }, { success: true, hasImage: true, canAddToProject: true })] }
  ),

  r('search_fab_listings', 'asset',
    'Search the whole public Fab catalog through the signed-in Fab tab and get listing ids you can pass straight to add_fab_asset_to_project. Opens the Fab tab when it is closed and waits up to 15 seconds for its page to load (PAGE_NOT_READY after that) instead of failing on the first call; the account must be signed in there. seller narrows the search to one publisher by name (Quixel Megascans for the Megascans library) and listingType to one content kind (3d-model, material), so a Megascans surface is one call rather than a page of unrelated uploads. Every hit says who published it, which category it is in, how it is rated, what it costs and which formats it ships, as far as the search itself returned them; a fact the row did not carry is left out, never filled in, and nothing more is fetched per row. isFree is derived from price, because the listing\'s own isFree flag disagrees with it and is not reported. A hit is a candidate, not a promise: no channel filter is applied, because pinning one hid the Quixel/Megascans library entirely. Whether a listing can be imported is resolved at add time, which reports NO_IMPORTABLE_FORMAT only when the listing ships none of unreal-engine, gltf, glb, fbx, obj or usdz; call get_fab_listing_details first for canAddToProject, the engine build and the download size up front. Results carry ids and labels only; no thumbnail, download URL or account field leaves the page.',
    schema({
      query: str('Free-text search. At most 128 characters, and no quotes, backslashes or control characters.'),
      seller: str('Only listings published by this seller, by name, for example Quixel Megascans. At most 128 characters, and no quotes, backslashes or control characters. Omit for every publisher.'),
      listingType: str('Only this content kind, as a row\'s listingType names it, for example 3d-model or material. One token of [A-Za-z0-9_-], at most 40 characters. Omit for every kind.'),
      freeOnly: { type: 'boolean', default: false, description: 'Restrict to free listings.' },
      limit: { type: 'number', minimum: 1, maximum: 50, default: 12, description: 'Maximum listings to return (1-50).' }
    }, []),
    schema({
      success: bool('Search completed.'),
      listings: arrObj('Matched listings: listingId, title, listingType, isFree (derived from price), tags, and, when the row carried them, seller, category, averageRating, ratingCount, price, currency, isCc0, publishedAt and formats (the format codes it ships). unresolvedPriceShape names the price field\'s keys when the price could not be read. The listing\'s own isFree flag is not reported.'),
      listingCount: num('Listings returned.'),
      query: str('Query that was run.'),
      seller: str('The seller filter that was applied. Present only when one was given.'),
      listingType: str('The content-kind filter that was applied. Present only when one was given.'),
      note: str('How to use a returned listingId, and what listingType does and does not guarantee.')
    }, ['success']),
    READ, READ_POLICY, MEDIUM,
    { dispatchAction: 'search_fab_listings',
      whenToUse: ['Content must be found on Fab by keyword, optionally from one publisher or of one kind, or free listings only, to get listing ids and to tell a Megascans hit from a random upload.'],
      whenNotToUse: ['Content already installed in an engine template, plugin or Bridge folder is wanted (use asset.list with kind=content_sources).'],
      examples: [
        ex('Find free Unreal rocks on Fab', { query: 'rock', freeOnly: true, limit: 5 }, { success: true, listingCount: 0 }),
        ex('Find Megascans materials', { query: 'concrete', seller: 'Quixel Megascans', listingType: 'material', limit: 5 }, { success: true, listingCount: 0 })
      ] }
  ),

  r('add_fab_asset_to_project', 'asset',
    'Start importing one Fab listing into this project through the signed-in Fab tab and return as soon as Fab accepts the download. The tab is opened when it is closed and the page is waited for up to 15 seconds (PAGE_NOT_READY after that); the account must be signed in there. The content does NOT exist yet when this replies: downloading and importing take minutes and hold the editor, far longer than a client waits, so poll asset.query_marketplace with lookup=fab_import_status and the returned operationId until phase is done or failed (marketplace=fab_cancel stops one that is queued or downloading). Never call this again to see progress: a repeat for a listing that is queued or running only hands back the same operation (alreadyRunning), and a repeat that asks for a different quality, combineMeshes, destinationPath or assetName is refused with ADD_ALREADY_RUNNING, naming that operation, since it would ignore them. One import runs at a time, so an add made while another runs is queued (phase queued, queuePosition) and starts by itself, first in first out, and the status read lists the queue; only a full queue (8 waiting) is refused with QUEUE_FULL, which names the import at its head with its phase, bytes and elapsed time and returns the status call as its nextCall. A queued add is not put to Fab until its turn, so a refusal it would have got at once (NO_IMPORTABLE_FORMAT, COMPLETE_PROJECT, LARGE_SCENE_FILE) arrives later as the status read\'s failureCode. It claims the listing first: Fab answers 404 for a download the account does not own, so this posts add-to-library exactly as the Fab UI does when you press Add to Project — a real change to your Fab library, free to claim and harmless to repeat on a listing you already own. It claims each free license of the listing in turn until Fab accepts one. Then it picks the format the Fab importer accepts (unreal-engine, else gltf, glb, fbx, obj or usdz) and hands over the download. A unreal-engine pack that declares no build for the running engine imports the highest build at or below it, else the lowest above it, and engineMatch (older or newer) and engineVersion say which. A complete project (COMPLETE_PROJECT) cannot be added to an existing project: it is refused after the claim, so it is in your library to create a project from, and the reply says to create it as a new project from Fab and migrate its content. A MetaHuman listing (METAHUMAN_FORMAT) is refused before anything is claimed, because Fab hands it to a workflow this add does not drive. A listing Fab will not give an address for fails with NO_DOWNLOAD_URL naming the step that refused (license: no offer to claim; claim: Fab refused the account; download-info: Fab answered no address), Fab\'s HTTP status and Fab\'s own words, so the cause is in the reply rather than the editor log; the reply names the format, the file and its quality tier, and the file size when Fab publishes one. A Megascans listing publishes one file per quality tier (raw, high, mid, low); quality takes exactly the tier named, and a tier the listing does not publish is refused with QUALITY_NOT_AVAILABLE, naming the tiers it does, before anything downloads. Without quality it takes the best game-ready tier: high, else mid, low, raw. A listing that publishes no tiers, and a unreal-engine pack, ignore quality. Supply only a listing id; the signed URL, EOS token and session cookie stay inside the page and reach no response or log. Fab chooses the destination folder (a Megascans add lands under a machine-named tree such as /Game/Fab/Megascans/3D/<name>_<id>/High/<id>_tier_1), and the status read reports importedRoot once the import is done, by which time the packages Fab left unsaved in memory have been saved, and they are saved again 15 and 60 seconds after the import ends for anything the engine marks dirty late; the status read\'s saved, savedCount and unsavedPackages say how it went and name a package that could not be saved. destinationPath moves what the import created in its own folders to a /Game folder you name, keeping the layout beneath it (StaticMeshes, Materials, Textures), and assetName names the import\'s one mesh SM_<assetName> (SK_ for a skeletal mesh) with its material instance and textures following (MI_<assetName>, T_<assetName>_<suffix>); a surface has no mesh and is named by its one material instance. It runs the move and rename that asset.move runs, so references and settings follow and the redirectors left behind are fixed, and it is all or nothing: a name already taken stops it before anything moves. It never touches the shared master materials, material functions and textures in /Game/Fab/Materials, /Game/Fab/MaterialFunctions and /Game/Fab/Textures, and a unreal-engine pack, which arrives outside /Game/Fab, stays where Fab put it. The status read\'s relocated, movedCount and relocationNote say what happened. Fab\'s importer merges every mesh in a source file (fbx, obj, gltf, glb, usdz) into ONE static mesh, whatever the project\'s import settings say, and a file that is really a scene became a single unplaceable mesh that needed gigabytes to build while the editor stayed held for many minutes. So a single mesh file of 50 MB or more (Megascans and unreal-engine packs are not affected) is refused with LARGE_SCENE_FILE, before anything downloads, until combineMeshes says which way to import it.',
    schema({
      listingId: str('Fab listing uid, as it appears in a fab.com/listings/<uid> URL. Restricted to [A-Za-z0-9_-], 64 characters max, because it is used to build an API path.'),
      destinationPath: str('A /Game folder to move what the import created to, for example /Game/Props/Barriers. The layout beneath the import\'s own folder (StaticMeshes, Materials, Textures) is kept under it. Omit to leave the import where Fab put it. The shared master materials in /Game/Fab/Materials, MaterialFunctions and Textures never move, and a unreal-engine pack is left where Fab put it.'),
      assetName: str('The name for the import\'s one mesh: SM_<assetName>, or SK_ for a skeletal mesh, with its material instance and textures following (MI_<assetName>, T_<assetName>_<suffix>); a surface with no mesh is named by its one material instance. Letters, digits and underscores, not starting with a digit, 64 at most, for example ConcreteBarrier. Without destinationPath the assets are renamed where they are. Not applied when the import holds no single mesh or material instance to name; relocationNote says so.'),
      quality: { type: 'string', enum: ['raw', 'high', 'mid', 'low'], description: 'Quality tier of the file to fetch, for a listing that publishes one file per tier, as Megascans does: raw (the unprocessed scan, hundreds of MB), high, mid or low. Omit to take the best game-ready tier: high, else mid, low, raw. A tier the listing does not publish is refused with QUALITY_NOT_AVAILABLE, naming the tiers it does. Ignored for a listing that publishes no tiers and for a unreal-engine pack. The reply\'s quality, fileName and downloadBytes say which file was chosen.' },
      combineMeshes: bool('Only for source mesh formats (fbx, obj, gltf, glb, usdz) that Fab imports itself; ignored for Megascans and unreal-engine packs. Omit to leave Fab\'s behaviour alone: every mesh of a file is merged into ONE static mesh. false imports each mesh as its own asset: the adapter switches off Interchange\'s mesh combining on the pipelines Fab generated for this import, and the status read\'s combineMeshesApplied says whether that landed in time (COMBINE_UNSUPPORTED refuses the call when this engine gives no way to). true accepts the single merged mesh explicitly, which a scene-sized file (one mesh file of 50 MB or more) requires.')
    }, ['listingId']),
    schema({
      success: bool('True when Fab accepted the download. It does not mean content exists: poll fab_import_status for that.'),
      listingId: str('Listing that was requested.'),
      accepted: bool('True when Fab accepted the workflow. Not the same as content existing.'),
      operationId: str('The background import. Pass it to asset.query_marketplace lookup=fab_import_status. On QUEUE_FULL it names the import at the head of the queue instead, and on ADD_ALREADY_RUNNING the open import of this listing.'),
      alreadyRunning: bool('True when this listing was already queued or being imported and this call returned that same operation instead of starting another.'),
      phase: str('queued when another import is running, else downloading right after Fab accepts. For alreadyRunning, the phase the operation was in.'),
      queuePosition: num('1-based place in the queue while phase is queued; absent otherwise.'),
      title: str('The listing title, once Fab has been asked about it (absent while queued).'),
      formatCode: str('Format Fab was asked to import: unreal-engine, gltf, glb, fbx, obj or usdz.'),
      quality: str('Quality tier of the chosen file (raw, high, mid or low) when the listing publishes tiers, as Megascans does: the one asked for in quality, else the best game-ready one. Absent otherwise.'),
      fileName: str('The file Fab downloads. Source formats only: a unreal-engine pack has a version name, not a file.'),
      downloadBytes: num('Size of that file in bytes. Absent when Fab publishes none (a unreal-engine pack): absent means unknown, never zero.'),
      versionName: str('Listing version selected for this engine.'),
      engineExactMatch: bool('False when no build of the pack declares the running engine; engineMatch says which build was taken instead.'),
      engineMatch: str('exact, older or newer: the pack build taken against the running engine. older is the newest build for an earlier engine, newer the oldest build for a later one. Absent for a source format, or when no version declares an engine.'),
      engineVersion: str('The engine version that build declares, for example UE_5.4.'),
      combinesMeshes: bool('True when Fab\'s importer merges every mesh of a file into ONE static mesh for this listing: a source mesh format that is not Megascans. False for Megascans and unreal-engine packs.'),
      note: str('What to do next: poll the status read, do not add again.')
    }, ['success']),
    { effect: 'write', idempotency: 'idempotent', longRunning: true, safeToRetry: true }, WRITE_POLICY, MEDIUM,
    { dispatchAction: 'add_fab_asset_to_project',
      whenToUse: ['A Fab listing id is known and its content must be added to the project through the signed-in Fab tab.'],
      whenNotToUse: [
        'The listing must first be found or judged (use asset.query_marketplace).',
        'An import is already running and only its progress is wanted (use asset.query_marketplace with lookup=fab_import_status).',
        'The pack is already downloaded on disk and only needs copying in (use asset.maintain_content with maintenance=migrate).',
      ],
      examples: [ex('Add a Fab listing to the project', { listingId: 'ac2818b3-7d35-4cf5-a1af-cbf8ff5c61c1' }, { success: true, accepted: true, operationId: 'fab-3f9a1c2e40', phase: 'downloading' })] }
  ),

  r('get_fab_import_status', 'asset',
    'Report a Fab import that asset.import_marketplace_asset (marketplace=fab_listing) started, by operationId (from the add reply) or by listingId (its newest import), together with the queue it is in; give neither to list just the queue. It reads the adapter\'s own record, never Fab\'s page, so it needs no sign-in and answers at once — except while Fab is importing: the import holds the editor for minutes and every call, this one included, then answers EDITOR_BLOCKED. That is the import working, not a failure: retry every 20 to 30 seconds. phase is queued (another import runs first; queuePosition and the queue say how many are ahead, and it starts by itself), resolving (asking Fab for the download), downloading (downloadPercent is the figure on Fab\'s own download notification, for a unreal-engine pack and a source format alike; downloadedBytes of downloadBytes too when Fab\'s download folder shows them, which a pack\'s does not), importing (Fab\'s importer is running; assetsSoFar counts what the asset registry has gained), cancelling (a cancel was accepted; the import ends within a second), done (importedRoot, assetCount and sampleAssetPaths, meshes first) or failed (failureCode, failure, and fabErrors quoting what Fab logged). cancellable says whether a cancel would stop the import right now. Stop polling when finished is true. The editor keeps the last 16 imports of the session and forgets them on restart. Do not call add again to look at progress.',
    schema({
      operationId: str('Operation id from the add reply. Restricted to [A-Za-z0-9_-], 64 characters max. Give this or listingId, or neither to list the queue.'),
      listingId: str('Fab listing uid, to report the newest import of that listing. Used only when operationId is not given.')
    }, []),
    schema({
      success: bool('The import was found and reported (or, with no id, the queue was listed). Read phase for how it is going: success does not mean the import worked.'),
      operationId: str('The import reported.'),
      listingId: str('The listing it imports.'),
      phase: str('queued, resolving, downloading, importing, cancelling, done or failed.'),
      finished: bool('True once phase is done or failed; stop polling.'),
      queuePosition: num('1-based place in the queue while phase is queued; absent otherwise.'),
      queue: arrObj('The import that is running, then every add waiting behind it in the order they start: operationId, listingId, title, phase, queuePosition, elapsedSeconds, and downloadedBytes and downloadPercent when known. Absent when nothing is running or queued.'),
      queueLength: num('How many imports are running or queued.'),
      title: str('The listing title, once Fab has been asked about it.'),
      elapsedSeconds: num('Seconds since the add was requested; frozen once the import finished.'),
      formatCode: str('Format Fab was asked to import.'),
      quality: str('Quality tier of the chosen file, when the listing has tiers.'),
      fileName: str('The file Fab downloads (source formats only).'),
      downloadBytes: num('Expected size of that file, when Fab publishes one; unknown, never zero, when absent.'),
      versionName: str('Listing version selected for this engine.'),
      engineExactMatch: bool('False when no build of the pack declares the running engine; engineMatch says which build was taken instead.'),
      engineMatch: str('exact, older or newer: the pack build taken against the running engine.'),
      engineVersion: str('The engine version that build declares, for example UE_5.4.'),
      combinesMeshes: bool('True when Fab\'s importer merges every mesh of a file into ONE static mesh for this listing.'),
      combineMeshesApplied: bool('Present only when the add passed combineMeshes=false for a listing Fab merges meshes for: true once Interchange\'s mesh combining was switched off before it ran, false while that has not happened yet or could not (the import then yields the single merged mesh).'),
      downloadedBytes: num('Bytes fetched so far, when the download folder shows them. Absent when the download cannot be observed (a unreal-engine pack, or a download that has not begun).'),
      downloadPercent: num('0-100, as Fab\'s own download notification shows it, for a unreal-engine pack and a source format alike. Absent when that notification cannot be read: before the download starts, after it ends, or when Fab shows none.'),
      cancellable: bool('Present while the import is not finished: true when a cancel would stop it right now (queued, a unreal-engine pack that is downloading, or an import Interchange is translating); false for a source-format download, which Fab gives no cancel for.'),
      assetsSoFar: num('New assets the asset registry has gained so far.'),
      assetCount: num('New assets the registry gained during the import. Present once finished and any landed.'),
      importedRoot: str('Where the content landed: the folder Fab chose (typically /Game/Fab/... or /Game/<PackName>), or the destinationPath the add asked for once the import was moved there. Present once finished and any landed.'),
      sampleAssetPaths: arr('Up to ten imported asset paths, the static and skeletal meshes first, at their final paths when the add asked for destinationPath or assetName.'),
      relocated: bool('Present when the add asked for destinationPath or assetName: true when assets were moved or renamed.'),
      movedCount: num('Assets moved or renamed by that relocation.'),
      relocationNote: str('What the relocation did not do, and why: a folder or name already taken (nothing moves then), an assetName with no single asset to name, or content left where Fab put it. Absent when everything asked for was done.'),
      saved: bool('Present once the import settled and any assets landed: true when every package the import left dirty was saved, false when some could not be (see unsavedPackages, and the message and note say so in capitals). Fab leaves what it imports unsaved in memory, so a Megascans add would otherwise be lost when the editor closes. The import is saved again 15 and 60 seconds after it ends, because Interchange marks packages dirty after the last asset has appeared; read again before saving by hand.'),
      savedCount: num('Packages that were saved. A unreal-engine pack arrives on disk already and is not dirty, so it counts none.'),
      unsavedPackages: arr('Packages that could not be saved, by name; control_editor save_all writes them once whatever blocked the save is fixed.'),
      failureCode: str('Present when phase is failed: FAB_IMPORT_FAILED (Fab logged that it gave up), IMPORT_TIMED_OUT (nothing appeared before the ceiling), IMPORT_PARTIAL (still streaming at the ceiling; what landed is reported), CANCELLED (cancel_fab_import stopped it; whatever had landed is listed and left unsaved), or the add\'s own refusal code (COMPLETE_PROJECT, METAHUMAN_FORMAT, LARGE_SCENE_FILE, QUALITY_NOT_AVAILABLE, NO_IMPORTABLE_FORMAT, NO_DOWNLOAD_URL).'),
      failure: str('Present when phase is failed: why, in words.'),
      fabErrors: arr('Error lines Fab logged while the import ran, with URLs and anything credential-shaped masked.'),
      note: str('What to do next, given the phase.')
    }, ['success']),
    READ, READ_POLICY, LOW,
    { dispatchAction: 'get_fab_import_status',
      whenToUse: ['A Fab add has returned an operationId and it must be known whether the import is downloading, importing, done or failed, and where the content landed.'],
      whenNotToUse: ['The listing has not been added yet (use asset.import_marketplace_asset with marketplace=fab_listing).'],
      examples: [ex('Check on a Fab import', { operationId: 'fab-3f9a1c2e40' }, { success: true, phase: 'downloading', finished: false })] }
  ),

  r('cancel_fab_import', 'asset',
    'Stop a Fab import that asset.import_marketplace_asset (marketplace=fab_listing) started, by its operationId. Fab gives three handles and this uses exactly those: an add still waiting in the queue is dropped and never reaches Fab; a unreal-engine pack that is downloading is stopped by pressing the Cancel button on Fab\'s own download notification; an import whose files Interchange is translating is told to cancel its tasks. The download of a source format (fbx, gltf, obj, usdz, Megascans) has no Cancel anywhere in Fab, so it is refused with NOT_CANCELLABLE and the reason, as is an add that is still resolving (seconds, and not interruptible) and an import past its download with no Interchange task to stop; the status read\'s cancellable says beforehand whether a cancel would work. A running import ends as failed with failureCode CANCELLED within a second (phase cancelling until then), with whatever had already landed listed and left unsaved and unmoved, so asset.delete removes it. A call is served only while the editor is free: while Fab builds meshes the editor is held and every call answers EDITOR_BLOCKED, so a cancel has to come before that. A finished import is refused with ALREADY_FINISHED and an unknown id with NOT_FOUND.',
    schema({
      operationId: str('Operation id from the add reply (or the status read\'s queue). Restricted to [A-Za-z0-9_-], 64 characters max.')
    }, ['operationId']),
    schema({
      success: bool('True when the cancel was accepted. The import has then stopped (a queued add) or ends within a second.'),
      operationId: str('The import that was cancelled.'),
      cancelled: bool('True when the cancel was accepted.'),
      phase: str('Where the import stood right after: failed for a dropped queued add, cancelling for a running one.'),
      note: str('What was done: which of the three handles was used.')
    }, ['success']),
    { effect: 'write', idempotency: 'idempotent', longRunning: false, safeToRetry: true }, WRITE_POLICY, LOW,
    { dispatchAction: 'cancel_fab_import',
      whenToUse: ['A Fab import that is queued, or a unreal-engine pack that is downloading, must be stopped: a wrong listing, or one taking too long.'],
      whenNotToUse: [
        'Only the progress of the import is wanted (use asset.query_marketplace with lookup=fab_import_status).',
        'Assets an import already created must be removed (use asset.delete).',
      ],
      examples: [ex('Stop a Fab import', { operationId: 'fab-3f9a1c2e40' }, { success: true, cancelled: true, phase: 'cancelling' })] }
  ),

  r('list_megascans_library', 'asset',
    'List the Quixel Bridge / Megascans library index on this machine. This reads the packs a Quixel Bridge install has already downloaded: Bridge writes a plain uassetsData.json next to them, so the inventory is an ordinary local read. Megascans listings that are not downloaded yet are found with search_fab_listings (seller Quixel Megascans) and imported straight from Fab with import_marketplace_asset (marketplace=fab_listing). Pair this with import_megascans_asset to bring a downloaded entry into the project.',
    schema({ filter: str('Case-sensitive substring matched against each serialized index entry.') }, []),
    schema({
      success: bool('Operation succeeded.'),
      assets: arrObj('Indexed library entries, verbatim from uassetsData.json.'),
      assetCount: num('Entries returned.'),
      libraryDirectory: str('Resolved Megascans library directory.'),
      indexPath: str('Path of the uassetsData.json index.'),
      indexExists: bool('False when Bridge has never written an index here.'),
      importAvailable: bool('True when this build links the MegascansPlugin module, so import_megascans_asset can run.')
    }, ['success']),
    READ, READ_POLICY, LOW,
    { dispatchAction: 'list_megascans_library',
      whenToUse: ['The Megascans packs downloaded by Bridge on this machine must be listed before one is imported.'],
      whenNotToUse: ['A listed pack should be imported into the project (use asset.import_marketplace_asset with marketplace=megascans).'],
      examples: [ex('List the downloaded Megascans library', {}, { success: true, assetCount: 0 })] }
  ),

  r('import_megascans_asset', 'asset',
    'Import a downloaded Megascans pack through the Bridge plugin\'s own importer, headlessly — no Bridge window, no drag, no sign-in. Calls FAssetsImportController::DataReceived, the exported entry point the Bridge desktop app drives over its local TCP socket, so quality tiers, master materials and the MSPresets setup all apply exactly as they would from the UI. This imports content ALREADY on disk, downloaded by the Bridge app. A Megascans listing that is not downloaded yet is fetched and imported by marketplace=fab_listing instead, which needs no Bridge window. Assets land under /Game/Megascans.',
    schema({
      payload: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'A complete Bridge export envelope: { exportPayload: [ { assetId, assetType, exportMode, exportType, folderName, name, assetPaths[] } ] }. Use this to pass through exactly what Bridge would have sent.' },
      assetPaths: arr('Absolute paths of the downloaded pack files. Used with folderName to synthesize a single-entry envelope when payload is omitted.'),
      folderName: str('Destination folder name under /Game/Megascans. Required when synthesizing from assetPaths.'),
      assetType: { type: 'string', default: '3d', description: 'Bridge asset type: 3d, 3dplant, atlas or surface.' },
      exportMode: { type: 'string', default: 'normal', description: 'Bridge export mode: normal, normal_drag or progressive.' },
      assetId: str('Megascans asset id. Defaults to folderName.'),
      name: str('Display name. Defaults to folderName.')
    }, []),
    schema({
      success: bool('Operation succeeded.'),
      entryCount: num('Export-payload entries dispatched to the importer.'),
      note: str('Where the imported content lands.')
    }, ['success']),
    MIGRATE_BEHAVIOR, WRITE_POLICY, HIGH,
    { dispatchAction: 'import_megascans_asset',
      whenToUse: ['A Megascans pack already downloaded by Bridge must be imported with its master materials and presets applied.'],
      whenNotToUse: [
        'The Megascans library index must be browsed first (use asset.query_marketplace).',
        'The pack files should just be copied in as they are (use asset.maintain_content with maintenance=migrate).',
      ],
      examples: [ex('Import a downloaded surface pack', { folderName: 'Rock_Cliff_ud4kcfxda', assetType: 'surface', assetPaths: ['C:/Users/me/Documents/Megascans Library/Downloaded/UAssets/Rock_Cliff_ud4kcfxda'] }, { success: true, entryCount: 1 })] }
  ),

  r('migrate_assets', 'asset',
    'Copy a content tree from an allowlisted source root into this project and scan it into the asset registry. This is how a Quixel Bridge / Fab pack or an engine template (its Blueprints, meshes, materials and maps) becomes usable content in the current project. IMPORTANT: package files store their references as absolute /Game/... paths and a copy cannot rewrite them, so the default destinationPath of "/Game" — which reproduces the source layout exactly — is the only setting that guarantees the migrated assets still resolve each other. Any other destinationPath relocates the tree and comes back with referenceIntegrity "at-risk". Run with dryRun first to see the file count and the package paths that will appear.',
    schema({
      sourceRoot: SOURCE_ROOT_PARAM,
      sourceId: str('Relative id under sourceRoot, exactly as returned by list_content_sources (for example "TP_VehicleAdvBP"). Must be relative: no "..", no leading "/", no drive prefix. Omit to migrate the root itself.'),
      subPath: str('Optional folder under the source content directory, to migrate one subtree instead of the whole pack.'),
      destinationPath: { type: 'string', default: '/Game', description: 'Root the copied tree lands under. Leave at "/Game" to preserve the source layout and keep internal references valid; any deeper path relocates the tree and flags referenceIntegrity as "at-risk".' },
      overwrite: bool('Overwrite packages that already exist at the destination. Default false, which skips them and reports skippedCount.'),
      dryRun: bool('Report what would be copied without writing anything. Returns the same counts and packagePaths sample.'),
      maxPackages: { type: 'number', default: 4000, description: 'Refuse the migration when the source holds more files than this, so a mistyped source cannot copy tens of gigabytes. Narrow with subPath or raise deliberately.' }
    }, ['sourceRoot']),
    schema({
      success: bool('True when every file copied. False with code PARTIAL_FAILURE when some did not.'),
      sourceDirectory: str('Absolute directory the packages were read from.'),
      destinationPath: str('Destination /Game root.'),
      copiedCount: num('Files copied (or that would be copied under dryRun).'),
      skippedCount: num('Files already present at the destination and left alone because overwrite was false.'),
      failedCount: num('Files that could not be copied.'),
      totalFiles: num('Package files discovered in the source.'),
      dryRun: bool('True when nothing was written.'),
      referenceIntegrity: str('"preserved" when the source layout was reproduced under /Game, "at-risk" when destinationPath relocated it.'),
      packagePaths: arr('Up to 40 destination package paths, for verifying the migration landed where expected.'),
      failedFiles: arr('Up to 20 source-relative paths that failed to copy.'),
      warnings: arr('Advisory messages, including the reference-integrity warning for a relocated destination.')
    }, ['success']),
    MIGRATE_BEHAVIOR, WRITE_POLICY, HIGH,
    { dispatchAction: 'migrate_assets',
      whenToUse: [
        'Loose .uasset content from an engine template, plugin or downloaded Fab or Megascans pack must be copied into the project.',
        'A migration must be previewed with a dry run before any file is copied.',
      ],
      whenNotToUse: [
        'A source file (FBX, PNG, WAV) must become an asset (use asset.import).',
        'The available sources are still unknown (use asset.list with kind=content_sources).',
      ],
      examples: [
        ex('Preview migrating the advanced vehicle template', { sourceRoot: 'engineTemplates', sourceId: 'TP_VehicleAdvBP', dryRun: true }, { success: true, referenceIntegrity: 'preserved' }),
        ex('Migrate a downloaded Megascans pack', { sourceRoot: 'megascansLibrary', sourceId: 'Rock_Cliff_ud4kcfxda' }, { success: true })
      ] }
  )
];
