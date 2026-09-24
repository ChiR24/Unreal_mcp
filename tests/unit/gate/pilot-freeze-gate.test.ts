// tests/unit/gate/pilot-freeze-gate.test.ts
// Task 14 isolated pilot architecture-freeze gate. Proves the clean pilot state
// (PILOT_CAPABILITY_CATALOG, asserted below), frozen emitter hashes, retrieval
// disclosure, and six seeded regressions each failing their exact invariant.
// No other repo file changes. The record counts in the re-freeze log that
// follows are DATED: each names the catalog size at the moment that freeze was
// taken, and they are deliberately not rewritten as the catalog moves.

import { mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { describe, expect, it } from 'vitest';
import { hashManifestContent } from '../../../scripts/gateway-manifest/hash.js';
import { pilotJson, pilotTsText } from '../../../scripts/gateway-manifest/pilot.js';
import {
  CapabilityCatalogSchema,
  type CapabilityRecord,
  capabilityErrorPointers,
  createCapabilityRecord,
} from '../../../src/tools/catalog/capabilities/index.js';
import { BUILD_ENVIRONMENT_RECORDS } from '../../../src/tools/catalog/capabilities/records/build-environment/index.js';
import { MANAGE_ASSET_RECORDS } from '../../../src/tools/catalog/capabilities/records/manage-asset/index.js';
import { MANAGE_BLUEPRINT_RECORDS } from '../../../src/tools/catalog/capabilities/records/manage-blueprint/index.js';
import { MANAGE_SEQUENCE_RECORDS } from '../../../src/tools/catalog/capabilities/records/manage-sequence/index.js';
import {
  CapabilityCatalogSizeError,
  createCapabilityRetriever,
  createPilotCapabilityCatalog,
  PILOT_CAPABILITY_CATALOG,
  RETRIEVAL_FIELD_WEIGHTS,
  RETRIEVAL_SCORE_CONSTANTS,
  RETRIEVAL_TOKENIZATION,
  retrieveCapabilities,
} from '../../../src/tools/catalog/capabilities/retrieval/index.js';

// Re-frozen twice. First when `create_landscape` gained `landscapeName` in
// inputProps (the record declared requiredOneOf ['name','landscapeName'] naming
// a property that inputProps did not publish, so the at-least-one-of group
// advertised a parameter that could never be sent; the property is now
// published). Then again after the requiredOneOf coverage audit added
// at-least-one-of groups to manage-asset (channel_pack, create_enum,
// connect_nodes/connect_material_pins) and manage-sequence (add_keyframe),
// removing the group members from `required` so the groups actually enforce
// the native alternatives. The 510-record structure is unchanged.
//
// Re-frozen a third time after the record-vs-native field parity audit:
// asset delete/delete_asset/delete_assets now declare `path` (native reads
// `path`+`paths`) alongside the `assetPath` compatibility alias and the
// bridge-delete `force` flag the live suite exercises; enable_world_partition
// and configure_grid_size no longer require the native-ignored `levelPath`
// (configure_grid_size also gained `priority`); list_light_types output is
// `types` (native emits `types`). The 510-record structure is unchanged.
//
// Re-frozen a fourth time after the MCP black-box ledger remediation: the
// record-vs-native parity fixes across all four pilot parents moved their
// schema and content hashes — build-environment lighting (light type/count
// outputs), manage-asset (asset lifecycle/query/enum parity), manage-blueprint
// (graph node and widget parity), and manage-sequence (cinematic/media/
// timeline parity). The 510-record structure and exact ID set are unchanged.
//
// Re-frozen a fifth time after the material-authoring and spline parity fixes:
// manage-asset gained published output schemas for connect_nodes and
// get_material_node_details, get_material_node_details accepts materialPath
// alongside assetPath, add_scalar_parameter publishes defaultValue, and
// build-environment's create_road_spline publishes closedLoop/materialPath.
// The 510-record structure was unchanged.
//
// Re-frozen a sixth time after the graph-placement feedback work: material node
// creation and blueprint create_node report posX/posY, an estimated extent and
// an overlap warning, and the component-transform capabilities document how
// Unreal combines parent and child scale. The 510-record structure was unchanged.
//
// Re-frozen a seventh time, and this one DOES move the structure: publishing
// material.set_node_position takes the pilot catalog from 510 to 511 records
// (manage-asset 158 -> 159). Material nodes could be placed at a coordinate but
// never moved, so a badly laid-out graph could only be fixed by removing and
// re-adding a node, which drops its connections.
//
// Re-frozen an eighth time, also moving the structure: publishing
// asset.list_content_sources and asset.migrate_assets takes the pilot catalog
// from 511 to 513 records (manage-asset 159 -> 161). Content already installed
// on the machine — engine templates, Quixel Bridge and Fab packs — was
// unreachable, because asset.import runs source-file importers and refuses any
// path outside the project directory. Re-hashed once more when the contract
// gained the `fabLibrary` root and a corrected note on where the Bridge library
// actually lives: the shell Documents folder is OneDrive-redirected on some
// machines while Bridge keeps writing to the profile Documents folder, so a
// single probe reported a library that was not there, and again when
// list_content_sources gained bounded pagination: an unfiltered sweep returns
// ~750 sources because every engine plugin shipping content counts, which was
// a ~200 KB default response. Re-frozen again for asset.list_fab_downloads
// (513 -> 514): the fabLibrary root reports where Fab caches downloads but not
// what is in it, so an agent could not tell "nothing downloaded" from "cache
// directory moved". Re-frozen once more for asset.list_megascans_library and
// asset.import_megascans_asset (514 -> 516): unlike Fab, the Bridge plugin
// exports FAssetsImportController::DataReceived, so a downloaded Megascans pack
// can be imported headlessly through the plugin's own importer. Re-frozen a
// final time for the Quixel online surface — search_quixel_assets,
// get_quixel_asset_details (preview image inlined as imageBase64) and
// download_quixel_asset (516 -> 521), which call the API the shipped Bridge
// frontend uses with the token Bridge itself persists.
// Re-frozen once more for asset.add_fab_asset_to_project (521 -> 521): the
// signed-in Fab page resolves the listing and hands the download to Fab's own
// importer, so a listing id is the only input MCP ever supplies.
//
// Re-frozen again for the Fab catalog-reach fix (521 -> 521, content only).
// search_fab_listings no longer pins channels=unreal-engine: the pin kept every
// hit addable but hid the Quixel/Megascans library outright, so the capability
// could not find assets a user can see on fab.com and the only way to reach one
// was to read its uid out of a browser by hand -- the manual step the capability
// exists to remove. Reach moved to search, and the addability question moved to
// where it can actually be answered: get_fab_listing_details now publishes
// assetFormats and hasUnrealBuild, so a caller learns a listing ships fbx/gltf
// rather than an Unreal build BEFORE add_fab_asset_to_project refuses it with
// NO_UNREAL_FORMAT. Verified live: Canyon Sandstone Campfire (Quixel) reports
// hasUnrealBuild false, Grid material reports true and imported 30 assets.
//
// Re-frozen once more for the Fab source-format import (521 -> 521, content
// only). add_fab_asset_to_project had hardcoded AssetType "unreal-engine" and
// IsQuixel false, so it rejected every listing shipping gltf/glb/fbx even
// though FabBrowserApi::AddToProject routes those to the Interchange and
// Quixel workflows -- three of the four importers the plugin ships were
// unreachable. It now resolves the format, passes it as AssetType, derives
// IsQuixel from the seller, and prefers a quality tier over the raw scan.
// Proven by importing a gltf-only listing (Syringe Prop, 5 assets). Quixel
// listings remain blocked -- their downloads resolve outside the Fab listing
// API, confirmed by identical 404s across two listings and every identifier
// and URL form -- so get_fab_listing_details reports canAddToProject false
// with addBlockedReason rather than promising an import that fails.
//
// Re-frozen again for the entitlement step (521 -> 521, content only).
// add_fab_asset_to_project now claims the listing before resolving a download,
// because Fab answers 404 for a file the account does not own -- which is why
// an unowned listing failed at download-info while an owned one resolved. That
// claim is a real change to the signed-in Fab library, so the capability
// description states it rather than leaving it implicit.
// Trimmed with it: the entitlement wording pushed add_fab_asset_to_project to
// 951 characters against a 500-720 neighbourhood, and gateway search is byte
// budgeted, so a verbose summary truncates result pages and makes a cursor
// stop advancing -- caught by todo11-search-truthfulness, which is a real
// search regression rather than a brittle assertion.
// Re-frozen a third time (2026-09-02) after the render post-process, screen,
// ray-trace and lighting build_environment records gained an optional
// `actorName` (the handlers already resolved one, and two unbound volumes made
// the family AMBIGUOUS with no way to pick) and create_lighting_enabled_level
// gained `levelName`.
// Re-frozen (2026-09-05) for the search-vocabulary work (521 -> 521, content
// only): pilot records gained retrieval `topics`, blueprint.create /
// get_blueprint / add_scs_component and asset.search_assets declared verb
// aliases, search_assets says "find" in its summary and asset.move lost a
// redundant topic. Schemas and the ID set are unchanged.
// Re-frozen (2026-09-08) for the folded-family widen() correction (132 ->
// 132, content only): widen() no longer inherits a keyword declared by only
// one member of a family. Three shipped schemas stopped imposing one
// member's constraint on its siblings' calls: asset.query_marketplace.limit
// lost a one-sided default, material.add_material_node.defaultValue and
// material.set_material_parameter.value lost a one-sided `type` (the union
// accepts what every member accepted). ID set unchanged.
// Re-frozen for struct.create_struct and the four datatable write actions
// publishing `save`.
// Re-frozen again for the add_foliage summary correction: its 'scatter'
// variant creates a foliage type asset and places no instances. The 132-record
// structure is unchanged; only that one input schema gained a field. The
// native handler has always read `save` (GetPayloadBool -> McpSafeAssetSave)
// but the record never declared it, so the gateway refused it as
// UNDECLARED_PARAMETER and every struct authored over MCP stayed in memory
// only and was lost on the next editor restart.
//
// Re-frozen after the live MCP defect sweep remediation: manage-sequence
// add_actors/remove_actors now declare the per-actor results[]/total/successful/
// failed the native handler already produced (output validation had been
// dropping all of it, so binding a nonexistent actor reported a bare success),
// manage-blueprint get_scs declares inheritedComponents/inheritedComponentCount
// (a Character Blueprint reported "0 SCS components" while owning four
// inherited ones that add_scs_component now accepts as parentComponent), and
// manage-asset's edit_material_instance fold summary no longer promises
// instance parameter overrides that only set_material_parameter performs.
// Only those records' schema/content hashes move; the record structure and the
// exact ID set are unchanged.

// Re-frozen again: manage-blueprint modify_scs now documents meshPath and
// materialPath on its batched operations[]. add_scs_component has always taken
// them, the batched form silently dropped them, and the contract said nothing
// either way — so the one call meant to build a whole prefab quietly produced
// components with no mesh. Content hash only; the ID set is unchanged.

// Re-frozen again in the same sweep: manage-blueprint connect_pins now declares
// the connection evidence the handler computes (connected/sourcePinName/
// targetPinName/pin types/saved) — it previously answered a bare "Pins
// connected" with an identical dataDigest whatever it linked — get_widget_info
// declares widgets[]/rootWidget beside the bare slot-name list, and
// manage-asset's import_data_table_rows documents the {rowName, rowData{}}
// shape its own example had contradicted. Content hashes only; the ID set and
// record structure are unchanged.
// Re-frozen 2026-09-17: manage-blueprint set_style/set_widget_layout declares
// cornerRadius/outlineColor/outlineWidth. UMG has drawn RoundedBox brushes
// since 5.0, but nothing on the published surface reached them, so every panel
// this tool can author was a hard-edged rectangle and "round this card" had no
// answer. Content hash only; the ID set and record structure are unchanged.
// Re-frozen 2026-09-17: set_style also declares text/texturePath/renderOpacity
// (the handler already applied text and renderOpacity, but neither was declared,
// so the gateway rejected them -- nothing published could change an existing
// widget's label or icon), and get_widget_slot_info declares visibility beside
// isVisible, which read the live Slate widget a design-time template does not
// have and so answered false for everything. Content hash only.
//
// Re-frozen again for the Fab library usability fix (content only, structure
// unchanged). list_fab_library read one column, FabObjectNameColumn, so a row
// was a bare name with nothing that could be handed to add_fab_asset_to_project
// -- the inventory could be listed but not acted on. It now also reads
// FabObjectColumn, which carries AssetId, ListingType, Seller and Source, and
// honours the `filter` its contract already declared, because a synced library
// is mostly engine versions and plugins (Source "uem") that fill the row limit.
// The empty-result note also stopped telling a signed-in caller to run
// Fab.Login: being signed in is not sufficient, the rows appear only after
// Fab.TEDS.MyFolderIntegration runs. Verified live against a signed-in editor:
// 20 rows, all carrying both columns, 11 of them legacy "uem" entries.
//
// Re-frozen for the create_node subsystem-node fix (content only, structure
// unchanged). The K2Node_GetSubsystem family stores its subsystem type in a
// CustomClass UPROPERTY that the palette seeds via Initialize(), so a node
// spawned generically came back untyped and the blueprint stopped compiling
// with "Node Invalid Subsystem Type must have a class specified" -- with no
// repair path, since the visible Class pin is only promoted during node
// reconstruction and set_node_property cannot reach CustomClass. create_node
// now seeds it from targetClass and refuses without one, and the targetClass
// description says so. Hit live while wiring AddMappingContext into Battle
// Rumble's player controller.
// Re-frozen 2026-09-19 for the build_environment lighting record-vs-handler
// parity fix (content only, structure unchanged). Five records declared
// parameters no handler forwards and omitted the ones that are forwarded, and
// because the input schemas are additionalProperties:false the real parameters
// were unreachable: configure_shadows hid shadowQuality/cascadedShadows/
// shadowDistance/contactShadows/rayTracedShadows/virtualShadowMaps;
// set_exposure omitted `method`; set_ambient_occlusion declared amount+method
// (read by nothing) instead of enabled/intensity/radius/quality;
// setup_volumetric_fog declared settings+intensity while the handler forwarded
// three fields native ignores and not the one it reads (viewDistance);
// setup_global_illumination hid quality/indirectLightingIntensity/bounces.
// set_exposure and set_ambient_occlusion also now forward actorName, which
// native uses to pick the PostProcessVolume and the TS payloads had dropped.
// Re-frozen 2026-09-20 for asset.import gaining importAnimations and
// skeletonPath (content only, structure unchanged). The handler built
// UAutomatedAssetImportData with no factory, so AssetTools routed every FBX
// through Interchange and an FBX carrying a mocap take imported as a bare
// SkeletalMesh with the animation silently dropped -- a film tool that could
// not import an animation. Naming UFbxFactory is what lets import options
// reach the importer, and overwrite now means something: UFbxFactory hands an
// existing destination to FReimportManager and returns early, which made the
// same call behave differently depending on whether the asset was resident.
// Re-frozen 2026-09-23 for blueprint.build_graph, a new edit_graph member
// (edit: "batch") that runs many graph edits in one call. Wiring one event
// chain cost a round trip per node, per link and per pin default; building a
// whole game's Blueprints that way was hundreds of calls.
// Re-frozen again the same day (content only): create_node declares the
// `parameters` its CustomEvent branch always read, and add_custom_expression
// declares `description` and says which outputType values exist.
// Re-frozen again the same day (content only): edit_scs documents the batch
// attach_component/reparent operation and that a failed operation is named in
// warnings. Six reparents in one batch had been rejected as an unknown type
// while the call still answered success.
// Re-frozen 2026-09-23: update_custom_expression now also takes inputs, output
// type, additional outputs and title, and an input that keeps its name keeps its wire.
// Re-frozen again the same day (content only): paint_foliage takes a box `area`,
// an exact `count`, `snapToSurface` and a scale/yaw range, and the add_foliage
// fold names the plain phrasings a small model searches with ("scatter grass").
// Re-frozen 2026-09-24 (content only): asset.list declares the `filter` it now
// honours (a string filter narrows a listing by asset name instead of being ignored).
const FROZEN_JSON_HASH = 'dcaab6ee53eb9e30c7f0c7225fac5c34b3e2d08c820f16ea3d7c6bca8846443c';
const FROZEN_TS_HASH = '528dd3e76361da18a2011ab4c197ed9c50b75b1ff874db74d54d69547ae4399c';

const ALL_PLUGINS = [...new Set(PILOT_CAPABILITY_CATALOG.flatMap((r) => r.availability.requiredPlugins))].sort();
const ALL_PARENTS = [...new Set(PILOT_CAPABILITY_CATALOG.map((r) => r.routing.parentTool))].sort();
const FROZEN_REQUEST = {
  query: 'search material assets',
  limit: 5,
  profile: {
    unrealVersion: { major: 5, minor: 7, patch: 4, channel: 'stable' },
    installedPlugins: ALL_PLUGINS,
    editorState: 'edit',
    enabledParents: ALL_PARENTS,
    enabledCategories: ['core', 'world', 'gameplay', 'utility'],
    authorizedScopes: ['read', 'write', 'destructive', 'admin'],
    requestedEffects: ['read', 'write', 'destructive'],
    requiredOutputFields: [],
  },
} as const;

function findRecord(id: string): CapabilityRecord {
  const record = PILOT_CAPABILITY_CATALOG.find((r) => r.id === id);
  if (record === undefined) throw new TypeError(`Missing pilot record ${id}`);
  return record;
}

function rehash(record: CapabilityRecord): Record<string, unknown> {
  const { hashes, ...source } = record;
  if (hashes.algorithm !== 'sha256') throw new TypeError('Expected sha256 capability record');
  return source;
}

describe('pilot architecture-freeze gate: clean 132-record state', () => {
  it('Given the four tracked pilot exports, When aggregated, Then the breakdown is 40+46+27+19=132 with exact unique IDs', () => {
    expect(BUILD_ENVIRONMENT_RECORDS.length).toBe(40);
    expect(MANAGE_ASSET_RECORDS.length).toBe(46);
    expect(MANAGE_BLUEPRINT_RECORDS.length).toBe(27);
    expect(MANAGE_SEQUENCE_RECORDS.length).toBe(19);
    expect(PILOT_CAPABILITY_CATALOG.length).toBe(132);
    expect(new Set(PILOT_CAPABILITY_CATALOG.map((r) => r.id)).size).toBe(132);
  });

  it('Given the frozen pilot emitter outputs, When hashed, Then JSON/TS hashes match the freeze contract exactly', () => {
    expect(hashManifestContent(pilotJson(PILOT_CAPABILITY_CATALOG))).toBe(FROZEN_JSON_HASH);
    expect(hashManifestContent(pilotTsText(PILOT_CAPABILITY_CATALOG))).toBe(FROZEN_TS_HASH);
  });

  it('Given the frozen retrieval configuration, When inspected, Then tokenization, weights, and score constants are frozen and locale-invariant', () => {
    expect(Object.isFrozen(RETRIEVAL_TOKENIZATION)).toBe(true);
    expect(Object.isFrozen(RETRIEVAL_FIELD_WEIGHTS)).toBe(true);
    expect(Object.isFrozen(RETRIEVAL_SCORE_CONSTANTS)).toBe(true);
    expect(RETRIEVAL_TOKENIZATION.locale).toBe('invariant');
  });

  it('Given the frozen search-material-assets request, When retrieval runs on the clean catalog, Then asset.search_assets is top-1 with bounded disclosure and no schema leakage', () => {
    const result = retrieveCapabilities(FROZEN_REQUEST);
    expect(result.matches[0]?.id).toBe('asset.query_asset');
    // Disclosure is BOUNDED by the 5-result cap, not required to fill it. The
    // alias fold removed material.rebuild_material as an independent document
    // because it is a declared alias of material.compile_material, which still
    // appears here; asserting exactly 5 would re-require that duplicate.
    expect(result.matches.length).toBeLessThanOrEqual(5);
    expect(result.matches.length).toBeGreaterThan(0);
    expect(new Set(result.matches.map((match) => match.id)).size).toBe(result.matches.length);
    expect(JSON.stringify(result)).not.toMatch(/schemas|properties|inputSchema|outputSchema/u);
  });
});

describe('pilot architecture-freeze gate: six seeded regressions', () => {
  it('Given a missing canonical record and a duplicated legacy mapping, When the catalog is built and parsed, Then 510 completeness fails and an exact legacyIds issue path is reported', () => {
    const truncated = {
      buildEnvironment: BUILD_ENVIRONMENT_RECORDS.slice(0, -1),
      manageAsset: MANAGE_ASSET_RECORDS,
      manageBlueprint: MANAGE_BLUEPRINT_RECORDS,
      manageSequence: MANAGE_SEQUENCE_RECORDS,
    };
    expect(() => createPilotCapabilityCatalog(truncated)).toThrow(CapabilityCatalogSizeError);

    const base = PILOT_CAPABILITY_CATALOG[0];
    const other = PILOT_CAPABILITY_CATALOG[1];
    const duplicateLegacy = createCapabilityRecord({
      ...rehash(other),
      id: 'freeze.duplicate_legacy',
      legacyIds: base.legacyIds,
    });
    const result = CapabilityCatalogSchema.safeParse([base, duplicateLegacy]);
    expect(result.success).toBe(false);
    if (!result.success) {
      const pointers = capabilityErrorPointers(result.error);
      expect(pointers.some((p) => p.includes('/legacyIds/'))).toBe(true);
    }
  });

  it('Given a manually drifted native registry, When the parity audit runs against a temp copy, Then an extra canonical tool mismatch is reported', async () => {
    const { auditNativeMcpParity } = await import('../../native-mcp-parity-audit.mjs');
    const registrySrc = readFileSync(
      join(process.cwd(), 'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Registry/McpToolRegistry.cpp'),
      'utf8',
    );
    const drifted = registrySrc.replace(
      'TEXT("manage_pcg")',
      'TEXT("manage_pcg"),\n\t\tTEXT("synthetic_freeze_drift")',
    );
    const tempDir = mkdtempSync(join(tmpdir(), 'pilot-freeze-native-'));
    const tempPath = join(tempDir, 'McpToolRegistry.cpp');
    try {
      writeFileSync(tempPath, drifted);
      const result = auditNativeMcpParity({ nativeToolRegistryPath: tempPath });
      expect(result.hasMismatches).toBe(true);
      expect(result.toolNameGaps.extraInNativeRegistry).toContain('synthetic_freeze_drift');
    } finally {
      rmSync(tempDir, { recursive: true, force: true });
    }
  });

  it('Given a record source missing its output schema, When the catalog schema parses it, Then a failure pointer ends in /schemas/output', () => {
    const base = PILOT_CAPABILITY_CATALOG[0];
    const malformed = { ...base, schemas: { input: base.schemas.input } };
    const result = CapabilityCatalogSchema.safeParse([malformed]);
    expect(result.success).toBe(false);
    if (!result.success) {
      const pointers = capabilityErrorPointers(result.error);
      expect(pointers.some((p) => p.endsWith('/schemas/output'))).toBe(true);
    }
  });

  it('Given a rehashed record reusing the blueprint.create_widget alias, When the catalog schema parses it alongside the owner, Then an exact duplicate-alias issue path and message are reported', () => {
    const owner = PILOT_CAPABILITY_CATALOG.find((r) => r.aliases.some((a) => a === 'blueprint.create_widget'));
    if (owner === undefined) throw new TypeError('blueprint.create_widget alias owner not found');
    const base = findRecord('asset.query_asset');
    const duplicateAlias = createCapabilityRecord({
      ...rehash(base),
      id: 'freeze.duplicate_alias',
      aliases: ['blueprint.create_widget'],
    });
    const result = CapabilityCatalogSchema.safeParse([owner, duplicateAlias]);
    expect(result.success).toBe(false);
    if (!result.success) {
      const pointers = capabilityErrorPointers(result.error);
      expect(pointers.some((p) => p.includes('/aliases/'))).toBe(true);
      expect(result.error.issues.some((i) => i.message === 'duplicate capability alias across records')).toBe(true);
    }
  });

  it('Given a record whose action input schema is replaced by a same-parent sibling schema, When the pilot output is rehashed, Then the frozen JSON hash changes for the exact record ID', () => {
    // Two unfolded siblings: a folded family's selector routing would refuse a
    // foreign schema before the hash could even be compared.
    const target = findRecord('asset.import');
    const sibling = findRecord('asset.create_folder');
    const modified = createCapabilityRecord({
      ...rehash(target),
      schemas: { input: sibling.schemas.input, output: target.schemas.output },
    });
    expect(modified.id).toBe('asset.import');
    const modifiedCatalog = PILOT_CAPABILITY_CATALOG.map((r) => (r.id === modified.id ? modified : r));
    expect(hashManifestContent(pilotJson(modifiedCatalog))).not.toBe(FROZEN_JSON_HASH);
  });

  it('Given a valid modified competitor record, When retrieval runs on the modified catalog, Then the asset.search_assets top-1 invariant changes', () => {
    const competitorBase = findRecord('asset.import');
    // The competitor declares one fresh legacy pair, so it carries no fold routing.
    const { dispatchBy: _folded, ...routing } = competitorBase.routing;
    const competitor = createCapabilityRecord({
      ...rehash(competitorBase),
      routing,
      aliases: [...competitorBase.aliases, 'search.material.assets'],
      legacyIds: [{ tool: 'manage_asset', action: 'search_material_assets' }],
      discovery: {
        ...competitorBase.discovery,
        topics: ['search', 'material', 'assets'],
        summary: 'Search material assets in the project content registry.',
        whenToUse: ['Use when you need to search material assets by type, tag, or path.'],
      },
    });
    const modifiedCatalog = PILOT_CAPABILITY_CATALOG.map((r) => (r.id === competitor.id ? competitor : r));
    const retriever = createCapabilityRetriever(modifiedCatalog);
    const result = retriever.retrieve(FROZEN_REQUEST);
    expect(result.matches[0]?.id).not.toBe('asset.query_asset');
    expect(result.matches[0]?.id).toBe(competitor.id);
  });
});
