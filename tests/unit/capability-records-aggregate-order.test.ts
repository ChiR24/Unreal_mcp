/**
 * tests/unit/capability-records-aggregate-order.test.ts
 *
 * Locks the ordering contract of the canonical record loader: the generator and
 * the metadata audit consume records in AUTHORED order (the sequence each
 * record directory declares), while the public `*_CAPABILITY_CATALOG`
 * retrieval projections stay frozen and id-sorted.
 *
 * Feeding the generator an id-sorted view alphabetises every derived parent
 * action enum, so the per-parent sequence checks below are the regression lock.
 */
import { describe, expect, it } from 'vitest';

import {
  ALL_CAPABILITY_RECORDS,
  ALL_CAPABILITY_RECORD_COUNT,
  loadAllCapabilityRecordsInSrc,
} from '../../src/tools/catalog/capabilities/records/aggregate.js';
import {
  CORE_CAPABILITY_CATALOG,
  CORE_CAPABILITY_RECORD_COUNT,
  CORE_CAPABILITY_SOURCE_RECORDS,
} from '../../src/tools/catalog/capabilities/retrieval/aggregate.js';
import {
  WORLD_CAPABILITY_CATALOG,
  WORLD_SOURCE_RECORDS,
} from '../../src/tools/catalog/capabilities/records/world/index.js';
import {
  GAMEPLAY_CAPABILITY_CATALOG,
  GAMEPLAY_SOURCE_RECORDS,
} from '../../src/tools/catalog/capabilities/records/gameplay/index.js';
import {
  UTILITY_CAPABILITY_CATALOG,
  UTILITY_SOURCE_RECORDS,
} from '../../src/tools/catalog/capabilities/records/utility/index.js';
import { loadAllCapabilityRecords } from '../../scripts/qa/capability-metadata-audit.js';
import type {
  CapabilityCatalog,
  CapabilityRecordSource,
} from '../../src/tools/catalog/capabilities/model.js';

const SOURCE_UNIVERSE: readonly CapabilityRecordSource[] = [
  ...WORLD_SOURCE_RECORDS,
  ...GAMEPLAY_SOURCE_RECORDS,
  ...UTILITY_SOURCE_RECORDS,
  ...CORE_CAPABILITY_SOURCE_RECORDS,
];

const PARENT_TOOLS: readonly string[] = [
  ...new Set(SOURCE_UNIVERSE.map((record) => record.routing.parentTool)),
];

const idsFor = (
  records: readonly CapabilityRecordSource[],
  parentTool: string,
): readonly string[] =>
  records.filter((record) => record.routing.parentTool === parentTool).map((record) => record.id);

const actionsFor = (
  records: readonly CapabilityRecordSource[],
  parentTool: string,
): readonly string[] => [
  ...new Set(
    records
      .filter((record) => record.routing.parentTool === parentTool)
      .flatMap((record) => record.legacyIds.map((legacy) => legacy.action)),
  ),
];

const idSorted = (records: readonly CapabilityRecordSource[]): readonly CapabilityRecordSource[] =>
  [...records].sort((left, right) => (left.id < right.id ? -1 : left.id > right.id ? 1 : 0));

const alphabetised = (values: readonly string[]): readonly string[] =>
  [...values].sort((left, right) => left.localeCompare(right));

describe('canonical record loader preserves authored order', () => {
  it('loads exactly 1,384 unique sha256-hashed records', () => {
    expect(ALL_CAPABILITY_RECORD_COUNT).toBe(1401);
    expect(ALL_CAPABILITY_RECORDS).toHaveLength(1401);
    expect(new Set(ALL_CAPABILITY_RECORDS.map((record) => record.id)).size).toBe(1401);
    expect(ALL_CAPABILITY_RECORDS.every((record) => record.hashes.algorithm === 'sha256')).toBe(true);
  });

  it('serves the audit loader and the in-src mirror from one ordered composition', () => {
    expect(loadAllCapabilityRecordsInSrc()).toBe(ALL_CAPABILITY_RECORDS);
    expect(loadAllCapabilityRecords().map((record) => record.id))
      .toEqual(ALL_CAPABILITY_RECORDS.map((record) => record.id));
  });

  it('keeps every parent in the record sequence its source array declares', () => {
    expect(PARENT_TOOLS).toHaveLength(23);
    for (const parentTool of PARENT_TOOLS) {
      expect(idsFor(ALL_CAPABILITY_RECORDS, parentTool))
        .toEqual(idsFor(SOURCE_UNIVERSE, parentTool));
    }
  });

  it('is not the globally id-sorted view', () => {
    const loadedIds = ALL_CAPABILITY_RECORDS.map((record) => record.id);
    expect(loadedIds).not.toEqual(idSorted(ALL_CAPABILITY_RECORDS).map((record) => record.id));
  });

  it('survives with non-alphabetical per-parent action sequences', () => {
    const sorted = idSorted(ALL_CAPABILITY_RECORDS);
    const resequenced = PARENT_TOOLS.filter(
      (parentTool) =>
        actionsFor(ALL_CAPABILITY_RECORDS, parentTool).join() !== actionsFor(sorted, parentTool).join(),
    );
    const nonAlphabetical = PARENT_TOOLS.filter((parentTool) => {
      const actions = actionsFor(ALL_CAPABILITY_RECORDS, parentTool);
      return actions.join() !== alphabetised(actions).join();
    });

    expect(resequenced.length).toBeGreaterThan(0);
    expect(nonAlphabetical.length).toBeGreaterThan(0);
  });
});

describe('retrieval catalogs stay frozen and id-sorted', () => {
  const CATALOGS: ReadonlyArray<readonly [string, CapabilityCatalog]> = [
    ['core', CORE_CAPABILITY_CATALOG],
    ['world', WORLD_CAPABILITY_CATALOG],
    ['gameplay', GAMEPLAY_CAPABILITY_CATALOG],
    ['utility', UTILITY_CAPABILITY_CATALOG],
  ];

  it.each(CATALOGS)('%s catalog is frozen and id-sorted', (_name, catalog) => {
    expect(Object.isFrozen(catalog)).toBe(true);
    expect(catalog.map((record) => record.id))
      .toEqual(idSorted(catalog).map((record) => record.id));
  });

  it('projects the sorted core catalog from the ordered core source view', () => {
    expect(CORE_CAPABILITY_SOURCE_RECORDS).toHaveLength(CORE_CAPABILITY_RECORD_COUNT);
    expect(Object.isFrozen(CORE_CAPABILITY_SOURCE_RECORDS)).toBe(true);
    expect(idSorted(CORE_CAPABILITY_SOURCE_RECORDS).map((record) => record.id))
      .toEqual(CORE_CAPABILITY_CATALOG.map((record) => record.id));
  });

  it('keeps the core source view in authored order, not id order', () => {
    const sourceIds = CORE_CAPABILITY_SOURCE_RECORDS.map((record) => record.id);
    expect(sourceIds).not.toEqual(idSorted(CORE_CAPABILITY_SOURCE_RECORDS).map((record) => record.id));
  });
});
