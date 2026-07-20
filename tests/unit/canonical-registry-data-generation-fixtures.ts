/**
 * tests/unit/canonical-registry-data-generation-fixtures.ts
 *
 * Shared fixtures/helpers for the Task-23 data-generation contract tests.
 * Splits the original 277-LOC test into focused sibling files so each stays
 * under the project 250 pure-LOC ceiling while preserving all 20 cases.
 */
import { loadAllCapabilityRecords } from '../../scripts/qa/capability-metadata-audit.js';
import { buildSortedRecords } from '../../scripts/canonical-registry/types.js';
import {
  buildTsDataModule,
  buildNeutralModel,
} from '../../scripts/canonical-registry/ts-targets.js';
import {
  buildRecordSummaries,
  buildLexicalIndex,
  buildMigrationData,
  buildAliasData,
  buildDocsData,
} from '../../scripts/canonical-registry/types.js';
import type { CapabilityRecord } from '../../src/tools/catalog/capabilities/model.js';

export const RECORDS = loadAllCapabilityRecords();
export const SORTED = buildSortedRecords(RECORDS);

const migrationMap = {
  entries: new Map<string, { canonicalId?: string | null; disposition: string }>(),
};
const generateAliases = () => ({ aliases: [] as const, conflicts: [] as const });

export function buildTsRecords(): readonly CapabilityRecord[] {
  const ts = buildTsDataModule({
    records: SORTED,
    summaries: buildRecordSummaries(SORTED),
    lexicalIndex: buildLexicalIndex(SORTED),
    migrationData: buildMigrationData(migrationMap),
    aliasData: buildAliasData(generateAliases),
    docsData: buildDocsData([]),
    catalogRevision: 'test',
    recordCount: SORTED.length,
  });
  // Records are emitted as consts, each parsed via parseCapabilityCatalog:
  //   const __RECORDS_CHUNK_N = parseCapabilityCatalog([ <json-array> ]);
  // The inner JSON array between `parseCapabilityCatalog([` and `]);` is captured
  // and parsed; collect every chunk.
  const chunkRegex = /const __RECORDS_CHUNK_\d+ = parseCapabilityCatalog\((\[[\s\S]*?\])\);/g;
  const records: unknown[] = [];
  let found = false;
  for (let match = chunkRegex.exec(ts); match !== null; match = chunkRegex.exec(ts)) {
    found = true;
    records.push(...(JSON.parse(match[1]) as unknown[]));
  }
  if (!found) throw new Error('record chunks (parseCapabilityCatalog([ ... ])) not found in generated TS');
  return records as CapabilityRecord[];
}

export function buildNeutralRecords(): readonly CapabilityRecord[] {
  const json = buildNeutralModel({
    catalogRevision: 'test',
    recordCount: SORTED.length,
    records: SORTED,
    summaries: buildRecordSummaries(SORTED),
    lexicalIndex: buildLexicalIndex(SORTED),
    migrationData: buildMigrationData(migrationMap),
    aliasData: buildAliasData(generateAliases),
    docsData: buildDocsData([]),
  });
  return (JSON.parse(json) as { records: CapabilityRecord[] }).records;
}
