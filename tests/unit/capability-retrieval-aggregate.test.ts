import { describe, expect, it } from 'vitest';

import { BUILD_ENVIRONMENT_RECORDS } from '../../src/tools/catalog/capabilities/records/build-environment/index.js';
import { MANAGE_ASSET_RECORDS } from '../../src/tools/catalog/capabilities/records/manage-asset/index.js';
import { MANAGE_BLUEPRINT_RECORDS } from '../../src/tools/catalog/capabilities/records/manage-blueprint/index.js';
import { MANAGE_SEQUENCE_RECORDS } from '../../src/tools/catalog/capabilities/records/manage-sequence/index.js';
import {
  CapabilityCatalogSizeError,
  createPilotCapabilityCatalog,
  PILOT_CAPABILITY_CATALOG,
  PILOT_CAPABILITY_RECORD_COUNT,
} from '../../src/tools/catalog/capabilities/retrieval/index.js';

const COMPLETE_SOURCES = {
  buildEnvironment: BUILD_ENVIRONMENT_RECORDS,
  manageAsset: MANAGE_ASSET_RECORDS,
  manageBlueprint: MANAGE_BLUEPRINT_RECORDS,
  manageSequence: MANAGE_SEQUENCE_RECORDS,
} as const;

describe('pilot capability retrieval aggregate', () => {
  it('Given the four completed pilots, When the aggregate is created, Then it contains exactly 510 unique parsed records', () => {
    const catalog = createPilotCapabilityCatalog(COMPLETE_SOURCES);

    expect(PILOT_CAPABILITY_RECORD_COUNT).toBe(510);
    expect(catalog).toHaveLength(510);
    expect(new Set(catalog.map((record) => record.id))).toHaveLength(510);
    expect(catalog.every((record) => record.hashes.algorithm === 'sha256')).toBe(true);
  });

  it('Given the exported aggregate, When its order is inspected, Then canonical IDs provide a frozen deterministic order', () => {
    const ids = PILOT_CAPABILITY_CATALOG.map((record) => record.id);

    expect(Object.isFrozen(PILOT_CAPABILITY_CATALOG)).toBe(true);
    expect(ids).toEqual([...ids].sort());
  });

  it('Given one missing environment source, When the aggregate is created, Then the exact-size invariant fails closed', () => {
    const incomplete = {
      ...COMPLETE_SOURCES,
      buildEnvironment: BUILD_ENVIRONMENT_RECORDS.slice(1),
    };

    expect(() => createPilotCapabilityCatalog(incomplete)).toThrow(CapabilityCatalogSizeError);
  });

  it('Given a duplicate canonical source, When the aggregate is created, Then catalog validation rejects it', () => {
    const duplicate = {
      ...COMPLETE_SOURCES,
      buildEnvironment: [
        ...BUILD_ENVIRONMENT_RECORDS.slice(0, -1),
        BUILD_ENVIRONMENT_RECORDS[0],
      ],
    };

    expect(() => createPilotCapabilityCatalog(duplicate)).toThrow(/duplicate canonical capability id/u);
  });
});
