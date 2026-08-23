// Content-source discovery and ingestion: list_content_sources, migrate_assets.
//
// These two cover the "use assets that already exist on this machine" path —
// engine templates, engine/plugin content, and Quixel Bridge / Fab packs
// downloaded to the Megascans library. Bridge and Fab deliver cooked .uasset
// packs rather than source art, so pulling one in is a package copy plus an
// asset-registry scan, not an importer run; `asset.import` remains the route
// for FBX/PNG/WAV source files.

import type { CapabilityBehaviorSource, CapabilityNormalization } from '../../model.js';
import type { RecordSpec } from './builder.js';
import { arr, arrObj, bool, boundedLimit, ex, HIGH, LOW, MEDIUM, num, READ, READ_POLICY, r, RETAIN, schema, str, WRITE_POLICY } from './builder.js';

const POST_MIGRATION: CapabilityNormalization = {
  ...RETAIN,
  provenance: 'post-migration',
  rationale: 'Authored after the gateway migration; no pre-gateway occurrence to audit.',
};

// A migration or Bridge import walks a whole content pack, so it is long-running by cost, and it
// copies files outside any transaction — dryRun is the preview and there is no
// undo. Re-running is safe: with overwrite off it skips what is already there.
const MIGRATE_BEHAVIOR: CapabilityBehaviorSource = {
  effect: 'write', idempotency: 'idempotent', longRunning: true,
  safeToRetry: true, supportsPreview: true, supportsUndo: false
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
      filter: str('Case-sensitive substring matched against the source id and, for plugins, the category.'),
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
    { dispatchAction: 'list_content_sources', dispatchMode: 'action', normalization: POST_MIGRATION,
      examples: [
        ex('Find the installed vehicle template', { sourceRoot: 'engineTemplates', filter: 'Vehicle' }, { success: true, sourceCount: 1 }),
        ex('List downloaded Quixel/Fab packs with counts', { sourceRoot: 'megascansLibrary', includePackageCounts: true }, { success: true })
      ] }
  ),

  r('list_fab_downloads', 'asset',
    'Report what the Fab plugin has already downloaded to this machine, with the cache directory it used. Pair with list_content_sources(sourceRoot="fabLibrary") and asset.migrate_assets to bring a downloaded pack into the project. This reads local state only: Fab\'s catalog is not on disk — the plugin\'s browser is an authenticated web view that fetches listings and short-lived signed download URLs — so browsing and purchasing stay in the editor\'s Fab tab, and this reports what that leaves behind.',
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
    { dispatchAction: 'list_fab_downloads', dispatchMode: 'action', normalization: POST_MIGRATION,
      examples: [ex('Check for downloaded Fab content', {}, { success: true, downloadCount: 0 })] }
  ),

  r('list_fab_library', 'asset',
    'List your Fab "My Library" entries that the Fab plugin has synced into the editor\'s data storage (TEDS). This is the searchable inventory of what your Fab account owns — distinct from list_fab_downloads, which only reports packs already downloaded to disk. Prerequisites are two console commands via control_editor.console_command: `Fab.Login` (opens Epic\'s account portal so the plugin authenticates itself — no credential ever passes through this tool) then `Fab.TEDS.MyFolderIntegration <batchSize>`, which pages the library in. Columns are resolved by path, and the data storage is reached through the modular-features registry, so this never links the Fab module and keeps working when Fab changes its schema.',
    schema({
      columnTypes: arr('Column struct paths to read, for example "/Script/Fab.FabObjectNameColumn". Defaults to the columns Fab currently writes. Override this when a Fab update renames or adds columns; unresolved paths are reported rather than failing the call.'),
      limit: { type: 'number', default: 200, minimum: 1, maximum: 1000, description: 'Maximum rows to return, clamped plugin-side.' }
    }, []),
    schema({
      success: bool('Operation succeeded.'),
      entries: arrObj('Library rows. Each entry maps column struct name to that column\'s properties, read by reflection.'),
      entryCount: num('Rows returned.'),
      unresolvedColumnTypes: arr('Requested column paths that do not exist in this build — usually a Fab schema change.'),
      note: str('Guidance on refreshing or paging the sync.')
    }, ['success']),
    READ, READ_POLICY, MEDIUM,
    { dispatchAction: 'list_fab_library', dispatchMode: 'action', normalization: POST_MIGRATION,
      examples: [ex('List the synced Fab library', { limit: 50 }, { success: true, entryCount: 0 })] }
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
    { dispatchAction: 'download_fab_asset', dispatchMode: 'action', normalization: POST_MIGRATION,
      examples: [ex('Download a pack from a signed URL', { assetId: 'abc123', downloadUrl: 'https://example.invalid/signed' }, { success: false })] }
  ),

  r('get_fab_listing_details', 'asset',
    'Describe one Fab listing — description, tags, seller, preview image and the asset formats it ships — so a caller can choose between search hits rather than guess from a title. Requires the Fab tab open and signed in. canAddToProject answers what search cannot: whether add_fab_asset_to_project can import this listing, true for unreal-engine, gltf, glb and fbx alike; hasUnrealBuild is narrower and covers only the packaged case. The preview comes back as imageBase64, promoted into a real MCP image block rather than a URL, and is omitted rather than truncated past the reply cap. When a field cannot be read the response names the keys it did see, so a Fab schema change reports itself.',
    schema({ listingId: str('Fab listing uid. Restricted to [A-Za-z0-9_-], 64 characters max.') }, ['listingId']),
    schema({
      success: bool('Listing was described.'),
      listingId: str('Listing that was described.'),
      title: str('Listing title.'),
      listingType: str('Content kind, e.g. 3d-model or material.'),
      description: str('Listing prose, truncated to 4000 characters.'),
      seller: str('Publisher name.'),
      tags: arr('Listing tags.'),
      imageBase64: str('Preview image bytes, base64. Promoted to an MCP image content block.'),
      mimeType: str('Preview image MIME type.'),
      hasImage: bool('False when no preview could be inlined.'),
      imageOmitted: str('Present when the preview was skipped for exceeding the reply cap.'),
      descriptionKeys: arr('Listing keys observed when no description field matched.'),
      thumbnailShape: arr('Thumbnail keys observed when no image URL matched.'),
      assetFormats: arr('Asset format codes this listing ships, e.g. unreal-engine, fbx, gltf.'),
      hasUnrealBuild: bool('True when the listing ships a packaged unreal-engine build, which Fab imports through its pack workflow.'),
      canAddToProject: bool('True when add_fab_asset_to_project can actually import this listing. unreal-engine, gltf, glb and fbx are importable through the pack and Interchange workflows. Quixel/Megascans listings are the exception: Fab will not serve their download until the listing is claimed, and the claim is CSRF-protected with no token exposed to the page, so this reports false and addBlockedReason says so. Check this rather than hasUnrealBuild before adding.'),
      addBlockedReason: str('Present when canAddToProject is false: why this listing cannot be imported.')
    }, ['success']),
    READ, READ_POLICY, MEDIUM,
    { dispatchAction: 'get_fab_listing_details', dispatchMode: 'action', normalization: POST_MIGRATION,
      examples: [ex('Describe a listing before adding it', { listingId: 'ac2818b3-7d35-4cf5-a1af-cbf8ff5c61c1' }, { success: true, hasImage: true })] }
  ),

  r('search_fab_listings', 'asset',
    'Search the whole public Fab catalog through the signed-in Fab tab and get listing ids you can pass straight to add_fab_asset_to_project. Requires that tab open and signed in. A hit is a candidate, not a promise: no channel filter is applied, because pinning one hid the Quixel/Megascans library entirely. Whether a listing can be imported is resolved at add time, which reports NO_IMPORTABLE_FORMAT only when the listing ships none of unreal-engine, gltf, glb or fbx; call get_fab_listing_details first if you want canAddToProject up front. Results carry ids and labels only; no thumbnail, download URL or account field leaves the page.',
    schema({
      query: str('Free-text search. At most 128 characters, and no quotes, backslashes or control characters.'),
      freeOnly: { type: 'boolean', default: false, description: 'Restrict to free listings.' },
      limit: { type: 'number', minimum: 1, maximum: 50, default: 12, description: 'Maximum listings to return (1-50).' }
    }, []),
    schema({
      success: bool('Search completed.'),
      listings: arrObj('Matched listings: listingId, title, listingType, isFree (derived from price), rawIsFree (the listing flag, which disagrees), tags, and unresolvedPriceShape when price could not be read.'),
      listingCount: num('Listings returned.'),
      query: str('Query that was run.'),
      note: str('How to use a returned listingId, and what listingType does and does not guarantee.')
    }, ['success']),
    READ, READ_POLICY, MEDIUM,
    { dispatchAction: 'search_fab_listings', dispatchMode: 'action', normalization: POST_MIGRATION,
      examples: [ex('Find free Unreal rocks on Fab', { query: 'rock', freeOnly: true, limit: 5 }, { success: true, listingCount: 0 })] }
  ),

  r('add_fab_asset_to_project', 'asset',
    'Add one Fab listing to this project through the signed-in Fab tab, which must be open. Claims the listing first: Fab answers 404 for a download the account does not own, so this posts add-to-library exactly as the Fab UI does when you press Add to Project — a real change to your Fab library, free to claim and harmless to repeat on a listing you already own. Then it picks the format the Fab importer accepts (unreal-engine, else gltf/glb/fbx) and hands over the download. Supply only a listing id; the signed URL, EOS token and session cookie stay inside the page and reach no response or log. Fab chooses the destination, so this reports importedRoot — use asset.migrate_assets to relocate. Success is decided by the asset registry, not by Fab: IMPORT_TIMED_OUT when nothing appears.',
    schema({
      listingId: str('Fab listing uid, as it appears in a fab.com/listings/<uid> URL. Restricted to [A-Za-z0-9_-], 64 characters max, because it is used to build an API path.')
    }, ['listingId']),
    schema({
      success: bool('True only when assets actually appeared in the registry.'),
      listingId: str('Listing that was requested.'),
      accepted: bool('True when Fab accepted the workflow. Not the same as content existing.'),
      assetCount: num('New assets the registry gained during the import.'),
      importedRoot: str('Where the pack landed, chosen by Fab (typically /Game/<PackName>).'),
      engineExactMatch: bool('False when no listing version declared the running engine and the first version was used instead.'),
      versionName: str('Listing version selected for this engine.'),
      sampleAssetPaths: arr('Up to ten imported asset paths, as registry evidence.'),
      note: str('How to relocate the imported tree.')
    }, ['success']),
    MIGRATE_BEHAVIOR, WRITE_POLICY, HIGH,
    { dispatchAction: 'add_fab_asset_to_project', dispatchMode: 'action', normalization: POST_MIGRATION,
      examples: [ex('Add a Fab listing to the project', { listingId: 'ac2818b3-7d35-4cf5-a1af-cbf8ff5c61c1' }, { success: false })] }
  ),

  r('list_megascans_library', 'asset',
    'List the Quixel Bridge / Megascans library index on this machine. Unlike Fab — whose catalog exists only inside an authenticated web view — Bridge writes a plain uassetsData.json next to the downloaded packs, so the inventory is an ordinary local read. Pair with import_megascans_asset to bring an entry into the project.',
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
    { dispatchAction: 'list_megascans_library', dispatchMode: 'action', normalization: POST_MIGRATION,
      examples: [ex('List the downloaded Megascans library', {}, { success: true, assetCount: 0 })] }
  ),

  r('import_megascans_asset', 'asset',
    'Import a downloaded Megascans pack through the Bridge plugin\'s own importer, headlessly — no Bridge window, no drag, no sign-in. Calls FAssetsImportController::DataReceived, the exported entry point the Bridge desktop app drives over its local TCP socket, so quality tiers, master materials and the MSPresets setup all apply exactly as they would from the UI. This imports content ALREADY on disk: downloading remains the Bridge app\'s job, since no download or catalog-search API is exported. Assets land under /Game/Megascans.',
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
    { dispatchAction: 'import_megascans_asset', dispatchMode: 'action', normalization: POST_MIGRATION,
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
    { dispatchAction: 'migrate_assets', dispatchMode: 'action', normalization: POST_MIGRATION,
      examples: [
        ex('Preview migrating the advanced vehicle template', { sourceRoot: 'engineTemplates', sourceId: 'TP_VehicleAdvBP', dryRun: true }, { success: true, referenceIntegrity: 'preserved' }),
        ex('Migrate a downloaded Megascans pack', { sourceRoot: 'megascansLibrary', sourceId: 'Rock_Cliff_ud4kcfxda' }, { success: true })
      ] }
  )
];
