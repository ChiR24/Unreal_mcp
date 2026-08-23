import { describe, expect, it } from 'vitest';

import { CONTROL_ACTOR_RECORDS } from '../../src/tools/catalog/capabilities/records/control-actor/index.js';
import { CONTROL_EDITOR_RECORDS } from '../../src/tools/catalog/capabilities/records/control-editor/index.js';
import { INSPECT_RECORDS } from '../../src/tools/catalog/capabilities/records/inspect/index.js';
import { MANAGE_ASSET_RECORDS } from '../../src/tools/catalog/capabilities/records/manage-asset/index.js';
import { MANAGE_BLUEPRINT_RECORDS } from '../../src/tools/catalog/capabilities/records/manage-blueprint/index.js';
import { MANAGE_LEVEL_RECORDS } from '../../src/tools/catalog/capabilities/records/manage-level/index.js';
import { MANAGE_TOOLS_RECORDS } from '../../src/tools/catalog/capabilities/records/manage-tools/index.js';
import { SYSTEM_CONTROL_RECORDS } from '../../src/tools/catalog/capabilities/records/system-control/index.js';
import {
  CapabilityCatalogSizeError,
  CORE_CAPABILITY_CATALOG,
  CORE_CAPABILITY_RECORD_COUNT,
  createCoreCapabilityCatalog,
  PILOT_CAPABILITY_CATALOG,
  PILOT_CAPABILITY_RECORD_COUNT,
} from '../../src/tools/catalog/capabilities/retrieval/index.js';

const COMPLETE_CORE_SOURCES = {
  manageAsset: MANAGE_ASSET_RECORDS,
  manageBlueprint: MANAGE_BLUEPRINT_RECORDS,
  controlActor: CONTROL_ACTOR_RECORDS,
  controlEditor: CONTROL_EDITOR_RECORDS,
  manageLevel: MANAGE_LEVEL_RECORDS,
  systemControl: SYSTEM_CONTROL_RECORDS,
  inspect: INSPECT_RECORDS,
  manageTools: MANAGE_TOOLS_RECORDS,
} as const;

describe('core capability retrieval aggregate', () => {
  it('contains exactly 505 unique hashed records in canonical order', () => {
    const catalog = createCoreCapabilityCatalog(COMPLETE_CORE_SOURCES);
    const ids = catalog.map((record) => record.id);

    expect(CORE_CAPABILITY_RECORD_COUNT).toBe(505);
    expect(catalog).toHaveLength(505);
    expect(new Set(ids)).toHaveLength(505);
    expect(ids).toEqual([...ids].sort());
    expect(catalog.every((record) => record.hashes.algorithm === 'sha256')).toBe(true);
  });

  it('exports a frozen core catalog without changing the frozen pilot catalog', () => {
    expect(Object.isFrozen(CORE_CAPABILITY_CATALOG)).toBe(true);
    expect(CORE_CAPABILITY_CATALOG).toHaveLength(505);
    expect(PILOT_CAPABILITY_RECORD_COUNT).toBe(521);
    expect(PILOT_CAPABILITY_CATALOG).toHaveLength(521);
  });

  it('fails closed when one core source is missing', () => {
    const incomplete = {
      ...COMPLETE_CORE_SOURCES,
      controlActor: CONTROL_ACTOR_RECORDS.slice(1),
    };

    expect(() => createCoreCapabilityCatalog(incomplete)).toThrow(CapabilityCatalogSizeError);
  });

  it('rejects a duplicate canonical core source', () => {
    const duplicate = {
      ...COMPLETE_CORE_SOURCES,
      manageTools: [
        ...MANAGE_TOOLS_RECORDS.slice(0, -1),
        MANAGE_TOOLS_RECORDS[0],
      ],
    };

    expect(() => createCoreCapabilityCatalog(duplicate)).toThrow(/duplicate canonical capability id/u);
  });
});
