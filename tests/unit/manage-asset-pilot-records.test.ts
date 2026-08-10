// tests/unit/manage-asset-pilot-records.test.ts
// Exact-set, schema, continuation, divergence, alias, and hash-parity tests
// for the 158 manage_asset capability records.
import { describe, expect, it } from 'vitest';
import { hashManifestContent } from '../../scripts/gateway-manifest/hash.js';
import { buildPilotManifest, pilotHeaderText, pilotJson, pilotTsText } from '../../scripts/gateway-manifest/pilot.js';
import type { CapabilityRecord } from '../../src/tools/catalog/capabilities/model.js';
import { MANAGE_ASSET_EXPECTED_IDS, MANAGE_ASSET_RECORDS } from '../../src/tools/catalog/capabilities/records/manage-asset/index.js';

const RECORDS: readonly CapabilityRecord[] = MANAGE_ASSET_RECORDS;
const IDS = RECORDS.map((r) => r.id);

function findRecord(id: string): CapabilityRecord {
  const record = RECORDS.find((r) => r.id === id);
  if (!record) throw new Error(`Record not found: ${id}`);
  return record;
}

function inputProps(r: CapabilityRecord): Record<string, unknown> {
  return r.schemas.input.properties as Record<string, unknown>;
}

function outputProps(r: CapabilityRecord): Record<string, unknown> {
  return r.schemas.output.properties as Record<string, unknown>;
}

describe('manage-asset pilot exact-set', () => {
  it('contains exactly 158 records', () => {
    expect(RECORDS.length).toBe(158);
  });

  it('has 158 unique canonical IDs', () => {
    expect(new Set(IDS).size).toBe(158);
  });

  it('expected IDs match actual IDs (sorted)', () => {
    expect(MANAGE_ASSET_EXPECTED_IDS.length).toBe(158);
    expect([...IDS].sort()).toEqual([...MANAGE_ASSET_EXPECTED_IDS].sort());
  });

  it('every record has a valid sha256 content hash', () => {
    for (const r of RECORDS) {
      expect(r.hashes.algorithm).toBe('sha256');
      expect(r.hashes.content).toMatch(/^[0-9a-f]{64}$/);
      expect(r.hashes.schema).toMatch(/^[0-9a-f]{64}$/);
    }
  });

  it('every record routes to manage_asset parent tool', () => {
    for (const r of RECORDS) {
      expect(r.routing.parentTool).toBe('manage_asset');
    }
  });

  it('every record is active (not deprecated or removed)', () => {
    for (const r of RECORDS) {
      expect(r.deprecation.status).toBe('active');
    }
  });

  it('reconciles all 158 manage_asset enum action strings as legacyIds', () => {
    const legacyActions = new Set<string>();
    for (const r of RECORDS) {
      for (const lid of r.legacyIds) {
        expect(lid.tool).toBe('manage_asset');
        legacyActions.add(lid.action);
      }
    }
    expect(legacyActions.size).toBe(158);
  });
});

describe('manage-asset pilot family distribution', () => {
  it('has 37 asset-family records', () => {
    expect(RECORDS.filter((r) => r.discovery.domain === 'asset').length).toBe(37);
  });

  it('has 56 material-family records', () => {
    expect(RECORDS.filter((r) => r.discovery.domain === 'material').length).toBe(56);
  });

  it('has 21 texture-family records', () => {
    expect(RECORDS.filter((r) => r.discovery.domain === 'texture').length).toBe(21);
  });

  it('has 23 struct-family records', () => {
    expect(RECORDS.filter((r) => r.discovery.domain === 'struct').length).toBe(23);
  });

  it('has 12 datatable-family records', () => {
    expect(RECORDS.filter((r) => r.discovery.domain === 'datatable').length).toBe(12);
  });

  it('has 9 enum-family records', () => {
    expect(RECORDS.filter((r) => r.discovery.domain === 'enum').length).toBe(9);
  });
});

describe('manage-asset pilot schema validation', () => {
  it('every input schema is Draft 2020-12 object', () => {
    for (const r of RECORDS) {
      expect(r.schemas.input.$schema).toBe('https://json-schema.org/draft/2020-12/schema');
      expect(r.schemas.input.type).toBe('object');
      expect(r.schemas.input.additionalProperties).toBe(false);
    }
  });

  it('every output schema is Draft 2020-12 object', () => {
    for (const r of RECORDS) {
      expect(r.schemas.output.$schema).toBe('https://json-schema.org/draft/2020-12/schema');
      expect(r.schemas.output.type).toBe('object');
      expect(r.schemas.output.additionalProperties).toBe(false);
    }
  });

  it('every record has at least a success property in output', () => {
    for (const r of RECORDS) {
      expect(outputProps(r).success).toBeDefined();
    }
  });
});

describe('manage-asset pilot bounded continuation', () => {
  it('asset.list has bounded limit (max 500, default 50)', () => {
    const list = findRecord('asset.list');
    const limitProp = inputProps(list).limit as Record<string, unknown>;
    expect(limitProp.maximum).toBe(500);
    expect(limitProp.minimum).toBe(1);
    expect(limitProp.default).toBe(50);
  });

  it('asset.list output has continuation fields (hasMore, nextCursor, cursor)', () => {
    const props = outputProps(findRecord('asset.list'));
    expect(props.hasMore).toBeDefined();
    expect(props.nextCursor).toBeDefined();
    expect(props.cursor).toBeDefined();
  });

  it('asset.search_assets has bounded limit (max 500)', () => {
    const limitProp = inputProps(findRecord('asset.search_assets')).limit as Record<string, unknown>;
    expect(limitProp.maximum).toBe(500);
  });

  it('no record permits unbounded list (all limit props have maximum)', () => {
    for (const r of RECORDS) {
      const limit = inputProps(r).limit;
      if (limit && typeof limit === 'object') {
        const limitObj = limit as Record<string, unknown>;
        expect(limitObj.maximum).toBeDefined();
        expect(typeof limitObj.maximum).toBe('number');
      }
    }
  });
});

describe('manage-asset pilot transport divergence', () => {
  it('asset.analyze_graph routes to get_asset_graph bridge action', () => {
    const ag = findRecord('asset.analyze_graph');
    expect(ag.routing.dispatchAction).toBe('get_asset_graph');
    expect(ag.normalization.rationale).toContain('Transport divergence');
  });

  it('asset.get_source_control_state routes through asset_query', () => {
    const sc = findRecord('asset.get_source_control_state');
    expect(sc.routing.dispatchAction).toBe('asset_query');
    expect(sc.normalization.rationale).toContain('Transport divergence');
  });

  it('asset.create_render_target routes through manage_texture', () => {
    const rt = findRecord('asset.create_render_target');
    expect(rt.routing.dispatchAction).toBe('manage_texture');
    expect(rt.normalization.rationale).toContain('manage_texture');
  });

  it('asset.nanite_rebuild_mesh routes through manage_render', () => {
    const nr = findRecord('asset.nanite_rebuild_mesh');
    expect(nr.routing.dispatchAction).toBe('manage_render');
    expect(nr.normalization.rationale).toContain('manage_render');
  });

  it('asset.create_thumbnail dispatches with generate_thumbnail subAction', () => {
    const ct = findRecord('asset.create_thumbnail');
    expect(ct.routing.dispatchAction).toBe('generate_thumbnail');
    expect(ct.normalization.rationale).toContain('generate_thumbnail');
  });

  it('asset.analyze_graph and asset.get_asset_graph are distinct records', () => {
    const ag = findRecord('asset.analyze_graph');
    const gag = findRecord('asset.get_asset_graph');
    expect(ag.id).not.toBe(gag.id);
  });
});

describe('manage-asset pilot alias normalization', () => {
  it('duplicate/duplicate_asset form a B_ALIAS pair', () => {
    const dup = findRecord('asset.duplicate');
    const dupAsset = findRecord('asset.duplicate_asset');
    expect(dup.normalization.class).toBe('B_ALIAS');
    expect(dup.normalization.disposition).toBe('canonical');
    expect(dupAsset.normalization.class).toBe('B_ALIAS');
    expect(dupAsset.normalization.disposition).toBe('alias');
  });

  it('delete/delete_asset/delete_assets form a B_ALIAS group', () => {
    const del = findRecord('asset.delete');
    const delAsset = findRecord('asset.delete_asset');
    const delAssets = findRecord('asset.delete_assets');
    expect(del.normalization.disposition).toBe('canonical');
    expect(delAsset.normalization.disposition).toBe('alias');
    expect(delAssets.normalization.disposition).toBe('alias');
  });

  it('rename/rename_asset and move/move_asset form B_ALIAS pairs', () => {
    for (const [canonicalId, aliasId] of [['asset.rename', 'asset.rename_asset'], ['asset.move', 'asset.move_asset']] as const) {
      const c = findRecord(canonicalId);
      const a = findRecord(aliasId);
      expect(c.normalization.disposition).toBe('canonical');
      expect(a.normalization.disposition).toBe('alias');
    }
  });

  it('connect_material_pins is alias of connect_nodes', () => {
    const cn = findRecord('material.connect_nodes');
    const cmp = findRecord('material.connect_material_pins');
    expect(cn.normalization.disposition).toBe('canonical');
    expect(cmp.normalization.disposition).toBe('alias');
  });

  it('rebuild_material is alias of compile_material', () => {
    const cm = findRecord('material.compile_material');
    const rm = findRecord('material.rebuild_material');
    expect(cm.normalization.disposition).toBe('canonical');
    expect(rm.normalization.disposition).toBe('alias');
  });

  it('break_material_connections is alias of disconnect_nodes', () => {
    const dn = findRecord('material.disconnect_nodes');
    const bmc = findRecord('material.break_material_connections');
    expect(dn.normalization.disposition).toBe('canonical');
    expect(bmc.normalization.disposition).toBe('alias');
  });

  it('no two records share the same legacy action', () => {
    const seen = new Map<string, string>();
    for (const r of RECORDS) {
      for (const lid of r.legacyIds) {
        const key = `${lid.tool}:${lid.action}`;
        expect(seen.has(key), `duplicate legacy action ${key} on ${r.id} and ${seen.get(key)}`).toBe(false);
        seen.set(key, r.id);
      }
    }
  });
});

describe('manage-asset pilot hash parity', () => {
  it('pilot TS/JSON/H outputs are deterministic across two emits', () => {
    const json1 = pilotJson(RECORDS);
    const json2 = pilotJson(RECORDS);
    const ts1 = pilotTsText(RECORDS);
    const ts2 = pilotTsText(RECORDS);
    const h1 = pilotHeaderText(RECORDS);
    const h2 = pilotHeaderText(RECORDS);
    expect(json1).toBe(json2);
    expect(ts1).toBe(ts2);
    expect(h1).toBe(h2);
  });

  it('pilot hashes are deterministic across two emits', () => {
    expect(hashManifestContent(pilotJson(RECORDS))).toBe(hashManifestContent(pilotJson(RECORDS)));
    expect(hashManifestContent(pilotTsText(RECORDS))).toBe(hashManifestContent(pilotTsText(RECORDS)));
    expect(hashManifestContent(pilotHeaderText(RECORDS))).toBe(hashManifestContent(pilotHeaderText(RECORDS)));
  });

  it('pilot tool names are 1:1 by canonical ID', () => {
    const manifest = buildPilotManifest(RECORDS);
    expect(manifest.tools.length).toBe(158);
    const names = manifest.tools.map((t) => t.name);
    expect(new Set(names).size).toBe(158);
  });
});
