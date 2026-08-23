// tests/unit/gate/pilot-freeze-gate.test.ts
// Task 14 isolated pilot architecture-freeze gate. Proves the clean 511-record
// pilot state, frozen emitter hashes, retrieval disclosure, and six seeded
// regressions each fail their exact invariant. No other repo file changes.

import { mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { describe, expect, it } from 'vitest';
import { hashManifestContent } from '../../../scripts/gateway-manifest/hash.js';
import { pilotHeaderText, pilotJson, pilotTsText } from '../../../scripts/gateway-manifest/pilot.js';
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
const FROZEN_JSON_HASH = 'e263280a4c305612441e65657fbe6fe42b0063fb8e818d4ef30417fadafc7668';
const FROZEN_TS_HASH = '0765bd175cf55528f69a7d07073ffaadc323c44adf5a1ded6fb9ad5d1f1a662a';
const FROZEN_NATIVE_HASH = 'dc17abdbe6f4e35642a42e07893f882565fe585e791c8d7dd7bab3322f6621c6';

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

describe('pilot architecture-freeze gate: clean 521-record state', () => {
  it('Given the four tracked pilot exports, When aggregated, Then the breakdown is 150+169+121+81=521 with exact unique IDs', () => {
    expect(BUILD_ENVIRONMENT_RECORDS.length).toBe(150);
    expect(MANAGE_ASSET_RECORDS.length).toBe(169);
    expect(MANAGE_BLUEPRINT_RECORDS.length).toBe(121);
    expect(MANAGE_SEQUENCE_RECORDS.length).toBe(81);
    expect(PILOT_CAPABILITY_CATALOG.length).toBe(521);
    expect(new Set(PILOT_CAPABILITY_CATALOG.map((r) => r.id)).size).toBe(521);
  });

  it('Given the frozen pilot emitter outputs, When hashed, Then JSON/TS/native hashes match the freeze contract exactly', () => {
    expect(hashManifestContent(pilotJson(PILOT_CAPABILITY_CATALOG))).toBe(FROZEN_JSON_HASH);
    expect(hashManifestContent(pilotTsText(PILOT_CAPABILITY_CATALOG))).toBe(FROZEN_TS_HASH);
    expect(hashManifestContent(pilotHeaderText(PILOT_CAPABILITY_CATALOG))).toBe(FROZEN_NATIVE_HASH);
  });

  it('Given the frozen retrieval configuration, When inspected, Then tokenization, weights, and score constants are frozen and locale-invariant', () => {
    expect(Object.isFrozen(RETRIEVAL_TOKENIZATION)).toBe(true);
    expect(Object.isFrozen(RETRIEVAL_FIELD_WEIGHTS)).toBe(true);
    expect(Object.isFrozen(RETRIEVAL_SCORE_CONSTANTS)).toBe(true);
    expect(RETRIEVAL_TOKENIZATION.locale).toBe('invariant');
  });

  it('Given the frozen search-material-assets request, When retrieval runs on the clean catalog, Then asset.search_assets is top-1 with bounded disclosure and no schema leakage', () => {
    const result = retrieveCapabilities(FROZEN_REQUEST);
    expect(result.matches[0]?.id).toBe('asset.search_assets');
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
    const base = findRecord('asset.search_assets');
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
    const target = findRecord('asset.search_assets');
    const sibling = findRecord('asset.analyze_graph');
    const modified = createCapabilityRecord({
      ...rehash(target),
      schemas: { input: sibling.schemas.input, output: target.schemas.output },
    });
    expect(modified.id).toBe('asset.search_assets');
    const modifiedCatalog = PILOT_CAPABILITY_CATALOG.map((r) => (r.id === modified.id ? modified : r));
    expect(hashManifestContent(pilotJson(modifiedCatalog))).not.toBe(FROZEN_JSON_HASH);
  });

  it('Given a valid modified competitor record, When retrieval runs on the modified catalog, Then the asset.search_assets top-1 invariant changes', () => {
    const competitorBase = findRecord('asset.analyze_graph');
    const competitor = createCapabilityRecord({
      ...rehash(competitorBase),
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
    expect(result.matches[0]?.id).not.toBe('asset.search_assets');
    expect(result.matches[0]?.id).toBe(competitor.id);
  });
});
