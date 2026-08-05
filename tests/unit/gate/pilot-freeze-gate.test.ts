// tests/unit/gate/pilot-freeze-gate.test.ts
// Task 14 isolated pilot architecture-freeze gate. Proves the clean 493-record
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
// the native alternatives. The 493-record structure is unchanged.
const FROZEN_JSON_HASH = '13727126b155eff2f5c93df72146731b4a0a79ecea206e7e8f61cd9d726f81a7';
const FROZEN_TS_HASH = '6dca013b78ec4e25b44ef16bd28cfe88d59c80f0cf0eacf5860a5e9b407bc236';
const FROZEN_NATIVE_HASH = '93836993d57397a86e15f7dade51a339b6c3dd365e01cd8cdf140c543bbd8f89';

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

describe('pilot architecture-freeze gate: clean 493-record state', () => {
  it('Given the four tracked pilot exports, When aggregated, Then the breakdown is 150+158+104+81=493 with exact unique IDs', () => {
    expect(BUILD_ENVIRONMENT_RECORDS.length).toBe(150);
    expect(MANAGE_ASSET_RECORDS.length).toBe(158);
    expect(MANAGE_BLUEPRINT_RECORDS.length).toBe(104);
    expect(MANAGE_SEQUENCE_RECORDS.length).toBe(81);
    expect(PILOT_CAPABILITY_CATALOG.length).toBe(493);
    expect(new Set(PILOT_CAPABILITY_CATALOG.map((r) => r.id)).size).toBe(493);
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
  it('Given a missing canonical record and a duplicated legacy mapping, When the catalog is built and parsed, Then 493 completeness fails and an exact legacyIds issue path is reported', () => {
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
