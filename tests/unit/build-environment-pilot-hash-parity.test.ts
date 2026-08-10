// tests/unit/build-environment-pilot-hash-parity.test.ts
// Focused tests for build_environment pilot deterministic equal-hash artifacts.
// Proves: identical TS/JSON/native pilot emits and hashes across repeated
// calls, and stable canonical-ID ordering regardless of input order.

import { describe, expect, it } from 'vitest';
import { hashManifestContent } from '../../scripts/gateway-manifest/hash.js';
import {
  pilotHeaderText,
  pilotJson,
  pilotTsText,
} from '../../scripts/gateway-manifest/pilot.js';
import { createCapabilityRecord } from '../../src/tools/catalog/capabilities/index.js';
import { BUILD_ENVIRONMENT_RECORDS } from '../../src/tools/catalog/capabilities/records/build-environment/index.js';

describe('build_environment pilot hash parity (TS/JSON/native)', () => {
  it('two pilot emit calls produce identical JSON, TS, and H bytes', () => {
    const records = BUILD_ENVIRONMENT_RECORDS.map((r) => createCapabilityRecord(r));
    const json1 = pilotJson(records);
    const json2 = pilotJson(records);
    const ts1 = pilotTsText(records);
    const ts2 = pilotTsText(records);
    const h1 = pilotHeaderText(records);
    const h2 = pilotHeaderText(records);
    expect(json1).toBe(json2);
    expect(ts1).toBe(ts2);
    expect(h1).toBe(h2);
  });

  it('two pilot emit calls produce identical hashes for JSON, TS, and H', () => {
    const records = BUILD_ENVIRONMENT_RECORDS.map((r) => createCapabilityRecord(r));
    const jsonHash1 = hashManifestContent(pilotJson(records));
    const jsonHash2 = hashManifestContent(pilotJson(records));
    const tsHash1 = hashManifestContent(pilotTsText(records));
    const tsHash2 = hashManifestContent(pilotTsText(records));
    const hHash1 = hashManifestContent(pilotHeaderText(records));
    const hHash2 = hashManifestContent(pilotHeaderText(records));
    expect(jsonHash1).toBe(jsonHash2);
    expect(tsHash1).toBe(tsHash2);
    expect(hHash1).toBe(hHash2);
  });

  it('pilot output is sorted by canonical ID regardless of input order', () => {
    const records = BUILD_ENVIRONMENT_RECORDS.map((r) => createCapabilityRecord(r));
    const reversed = [...records].reverse();
    const sortedJson = pilotJson(records);
    const reversedJson = pilotJson(reversed);
    expect(sortedJson).toBe(reversedJson);
  });
});
