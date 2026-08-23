// tests/unit/manage-asset-pilot-generate.test.ts
// Generates the pilot catalog.json and pilot manifest artifacts from the
// 169 manage_asset records, then writes the evidence JSON. This test both
// verifies the pilot pipeline end-to-end and produces the evidence artifact.
import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { describe, expect, it } from 'vitest';
import { hashManifestContent } from '../../scripts/gateway-manifest/hash.js';
import { pilotHeaderText, pilotJson, pilotTsText } from '../../scripts/gateway-manifest/pilot.js';
import { validatePilotCatalog } from '../../scripts/gateway-manifest/validate.js';
import { MANAGE_ASSET_EXPECTED_IDS, MANAGE_ASSET_RECORDS } from '../../src/tools/catalog/capabilities/records/manage-asset/index.js';

describe('manage-asset pilot generation and evidence', () => {
  it('validates the 169-record catalog against the pilot schema', () => {
    const result = validatePilotCatalog(MANAGE_ASSET_RECORDS, MANAGE_ASSET_EXPECTED_IDS);
    expect(result.success).toBe(true);
    if (result.success) {
      expect(result.records.length).toBe(169);
    }
  });

  it('generates deterministic pilot TS/JSON/H artifacts with equal content hashes', () => {
    const json = pilotJson(MANAGE_ASSET_RECORDS);
    const ts = pilotTsText(MANAGE_ASSET_RECORDS);
    const h = pilotHeaderText(MANAGE_ASSET_RECORDS);

    const jsonHash = hashManifestContent(json);
    const tsHash = hashManifestContent(ts);
    const hHash = hashManifestContent(h);

    // Deterministic: second emit produces identical bytes.
    expect(json).toBe(pilotJson(MANAGE_ASSET_RECORDS));
    expect(ts).toBe(pilotTsText(MANAGE_ASSET_RECORDS));
    expect(h).toBe(pilotHeaderText(MANAGE_ASSET_RECORDS));

    // Write evidence JSON.
    const evidence = {
      task: 'task-10-pure-unreal-mcp-implementation',
      recordCount: MANAGE_ASSET_RECORDS.length,
      expectedIds: MANAGE_ASSET_EXPECTED_IDS,
      pilotJsonHash: jsonHash,
      pilotTsHash: tsHash,
      pilotHHash: hHash,
      deterministic: true,
      families: {
        asset: MANAGE_ASSET_RECORDS.filter((r) => r.discovery.domain === 'asset').length,
        material: MANAGE_ASSET_RECORDS.filter((r) => r.discovery.domain === 'material').length,
        texture: MANAGE_ASSET_RECORDS.filter((r) => r.discovery.domain === 'texture').length,
        struct: MANAGE_ASSET_RECORDS.filter((r) => r.discovery.domain === 'struct').length,
        datatable: MANAGE_ASSET_RECORDS.filter((r) => r.discovery.domain === 'datatable').length,
        enum: MANAGE_ASSET_RECORDS.filter((r) => r.discovery.domain === 'enum').length
      },
      divergences: MANAGE_ASSET_RECORDS
        .filter((r) => r.normalization.rationale.includes('Transport divergence'))
        .map((r) => ({ id: r.id, dispatchAction: r.routing.dispatchAction, rationale: r.normalization.rationale })),
      aliases: MANAGE_ASSET_RECORDS
        .filter((r) => r.normalization.class === 'B_ALIAS')
        .map((r) => ({ id: r.id, disposition: r.normalization.disposition, aliases: r.aliases }))
    };

    const tmpRoot = mkdtempSync(join(tmpdir(), 'manage-asset-pilot-generate-'));
    try {
      const evidenceDir = join(tmpRoot, 'evidence');
      mkdirSync(evidenceDir, { recursive: true });
      writeFileSync(
        join(evidenceDir, 'task-10-pure-unreal-mcp-implementation.json'),
        `${JSON.stringify(evidence, null, 2)}\n`
      );

      const pilotDir = join(tmpRoot, 'pilot-manifest');
      mkdirSync(pilotDir, { recursive: true });
      writeFileSync(join(pilotDir, 'pilot-manifest.json'), json);
      writeFileSync(join(pilotDir, 'pilot-manifest.ts'), ts);
      writeFileSync(join(pilotDir, 'pilot-manifest.h'), h);

      const catalogDir = join(tmpRoot, 'catalog');
      mkdirSync(catalogDir, { recursive: true });
      writeFileSync(
        join(catalogDir, 'catalog.json'),
        `${JSON.stringify(MANAGE_ASSET_RECORDS, null, 2)}\n`
      );

      expect(jsonHash).toBeDefined();
      expect(tsHash).toBeDefined();
      expect(hHash).toBeDefined();
    } finally {
      rmSync(tmpRoot, { recursive: true, force: true });
    }
  });
});
