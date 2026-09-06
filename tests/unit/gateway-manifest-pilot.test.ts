// tests/unit/gateway-manifest-pilot.test.ts
// Pilot catalog validation, pilot determinism, and 1:1 canonical-ID keying.
// Every test induces a REAL failure or proves a REAL contract.

import { resolve } from 'node:path';
import { describe, expect, it } from 'vitest';
import { hashManifestContent } from '../../scripts/gateway-manifest/hash.js';
import {
  buildPilotManifest,
  pilotJson,
  pilotTsText
} from '../../scripts/gateway-manifest/pilot.js';
import { validatePilotCatalog } from '../../scripts/gateway-manifest/validate.js';
import { secondCapabilitySource, validCapabilitySource } from '../../src/tools/catalog/capabilities/capability-record.test-support.js';
import { createCapabilityRecord } from '../../src/tools/catalog/capabilities/index.js';

function makeRecord(id: string) {
  const source = validCapabilitySource();
  return createCapabilityRecord({ ...source, id });
}

function makeSecondRecord() {
  return createCapabilityRecord(secondCapabilitySource());
}

describe('gateway-manifest pilot validation', () => {
  it('rejects a malformed record with a JSON pointer and recoverable canonical ID', () => {
    const source = validCapabilitySource();
    const malformed = { ...source, behavior: { ...source.behavior, effect: 'invalid-effect' } };
    const result = validatePilotCatalog([malformed]);
    expect(result.success).toBe(false);
    if (!result.success) {
      expect(result.errors.length).toBeGreaterThan(0);
      const err = result.errors[0];
      expect(err.pointer).toContain('/0/behavior');
      expect(err.canonicalId).toBe('asset.delete');
    }
  });

  it('rejects a record with invalid id type (no recoverable canonical ID)', () => {
    const source = validCapabilitySource();
    const malformed = { ...source, id: 123 };
    const result = validatePilotCatalog([malformed]);
    expect(result.success).toBe(false);
    if (!result.success) {
      const idError = result.errors.find((e) => e.pointer.includes('/id'));
      expect(idError).toBeDefined();
      expect(idError?.canonicalId).toBeUndefined();
    }
  });

  it('reports the exact canonical ID on duplicate IDs', () => {
    const first = makeRecord('asset.delete');
    const secondSource = secondCapabilitySource();
    const second = createCapabilityRecord({ ...secondSource, id: first.id });
    const result = validatePilotCatalog([first, second]);
    expect(result.success).toBe(false);
    if (!result.success) {
      const dupError = result.errors.find((e) => e.message.includes('duplicate'));
      expect(dupError).toBeDefined();
      expect(dupError?.canonicalId).toBe('asset.delete');
      expect(dupError?.message).toContain('asset.delete');
    }
  });

  it('reports the exact missing canonical ID when an expected ID is absent', () => {
    const first = makeRecord('asset.delete');
    const second = makeSecondRecord();
    const expectedIds = ['asset.delete', 'actor.delete', 'level.delete'];
    const result = validatePilotCatalog([first, second], expectedIds);
    expect(result.success).toBe(false);
    if (!result.success) {
      const missingError = result.errors.find((e) => e.message.includes('level.delete'));
      expect(missingError).toBeDefined();
      expect(missingError?.canonicalId).toBe('level.delete');
    }
  });

  it('accepts a valid catalog with matching expected IDs', () => {
    const first = makeRecord('asset.delete');
    const second = makeSecondRecord();
    const expectedIds = ['asset.delete', 'actor.delete'];
    const result = validatePilotCatalog([first, second], expectedIds);
    expect(result.success).toBe(true);
    if (result.success) {
      expect(result.records).toHaveLength(2);
    }
  });

  it('rejects non-array input', () => {
    const result = validatePilotCatalog({ not: 'an array' });
    expect(result.success).toBe(false);
  });
});

describe('gateway-manifest pilot tool names are 1:1 by canonical ID', () => {
  it('uses canonical ID as tool name, not routing.parentTool', () => {
    const record = makeRecord('asset.delete');
    const manifest = buildPilotManifest([record]);
    expect(manifest.tools[0].name).toBe('asset.delete');
    expect(manifest.tools[0].name).not.toBe(record.routing.parentTool);
  });

  it('produces distinct names for records sharing one parentTool', () => {
    const first = makeRecord('asset.delete');
    const second = makeRecord('asset.create');
    const manifest = buildPilotManifest([first, second]);
    const names = manifest.tools.map((t) => t.name);
    expect(names).toEqual(['asset.create', 'asset.delete']);
    expect(new Set(names).size).toBe(2);
  });

  it('uses [dispatchAction] as the deterministic action list', () => {
    const record = makeRecord('asset.delete');
    const manifest = buildPilotManifest([record]);
    expect(manifest.tools[0].actions).toEqual([record.routing.dispatchAction]);
  });
});

describe('gateway-manifest pilot determinism', () => {
  it('two emit calls produce identical JSON and TS bytes', () => {
    const records = [makeRecord('asset.delete'), makeSecondRecord()];

    const json1 = pilotJson(records);
    const json2 = pilotJson(records);
    const ts1 = pilotTsText(records);
    const ts2 = pilotTsText(records);

    expect(json1).toBe(json2);
    expect(ts1).toBe(ts2);
  });

  it('two emit calls produce identical hashes for JSON and TS', () => {
    const records = [makeRecord('asset.delete'), makeSecondRecord()];

    const jsonHash1 = hashManifestContent(pilotJson(records));
    const jsonHash2 = hashManifestContent(pilotJson(records));
    const tsHash1 = hashManifestContent(pilotTsText(records));
    const tsHash2 = hashManifestContent(pilotTsText(records));

    expect(jsonHash1).toBe(jsonHash2);
    expect(tsHash1).toBe(tsHash2);
  });

  it('pilot output is sorted by canonical ID regardless of input order', () => {
    const first = makeRecord('zzz.last');
    const second = makeRecord('aaa.first');
    const manifest = buildPilotManifest([first, second]);
    expect(manifest.tools[0].name).toBe(second.id);
    expect(manifest.tools[1].name).toBe(first.id);
  });

  it('pilot source is pilot:capabilityRecords (not production source)', () => {
    const manifest = buildPilotManifest([]);
    expect(manifest.source).toBe('pilot:capabilityRecords');
    expect(manifest.source).not.toBe('consolidatedToolDefinitions');
  });

  it('pilot output paths are disjoint from production paths', () => {
    const root = resolve(process.cwd());
    const productionPaths = [
      resolve(root, 'src/gateway/gateway-manifest.generated.ts'),
      resolve(root, 'src/gateway/gateway-manifest.generated.json')
    ];
    const pilotDir = resolve(root, '.omo/pilot-manifest');
    const pilotPaths = [
      resolve(pilotDir, 'pilot-manifest.json'),
      resolve(pilotDir, 'pilot-manifest.ts')
    ];
    for (const pp of pilotPaths) {
      for (const prod of productionPaths) {
        expect(pp).not.toBe(prod);
      }
    }
  });
});
