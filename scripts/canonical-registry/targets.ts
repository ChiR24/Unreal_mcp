// scripts/canonical-registry/targets.ts
//
// Authoritative target-file plan for the Task-23 canonical registry generator.
//
// Owns EVERY generated artifact target (TS data, neutral JSON, parent defs,
// native parent registry, native capability shards + index).
// Malformed / duplicate / missing input fails BEFORE any target content is
// constructed (see native-shards / types validation), so the generator refuses
// to emit a partial or foreign set.

import { resolve } from 'node:path';
import type { CapabilityRecord } from '../../src/tools/catalog/capabilities/model.js';
import { ALL_CAPABILITY_RECORD_COUNT } from '../../src/tools/catalog/capabilities/records/aggregate.js';
import { sortById } from '../../src/utils/serialization/ordering.js';
import { sha256Hex } from './types.js';
import {
  buildTsDataModule,
  buildParentDefsModule,
  buildCostIndexModule,
  buildNeutralModel,
} from './ts-targets.js';
import { buildParentRegistrySource } from './cpp-registry.js';
import {
  buildNativeCapabilityShards,
  buildNativeCapabilityIndexHeader,
  buildNativeCapabilityShardSource,
  type NativeCapabilityShard,
} from './native-shards.js';
import { deriveParents } from './parent-derivation.js';
import { buildActionReferenceDoc } from './docs-reference.js';

import type { GeneratedTarget } from '../lib/generated-files.js';

export type { GeneratedTarget };

export interface BuildTargetsInput {
  readonly records: readonly CapabilityRecord[];
}

const ROOT = resolve(process.cwd());

const nativeCapabilityDir = (): string =>
  resolve(ROOT, 'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Generated');

const nativeToolsDir = (): string =>
  resolve(ROOT, 'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Tools');

const genDir = (): string =>
  resolve(ROOT, 'src/tools/catalog/capabilities/generated');

const docsDir = (): string => resolve(ROOT, 'docs');

export const buildTargets = (input: BuildTargetsInput): GeneratedTarget[] => {
  const { records } = input;
  if (records.length !== ALL_CAPABILITY_RECORD_COUNT) {
    throw new Error(
      `FATAL: expected ${ALL_CAPABILITY_RECORD_COUNT} capability records, `
      + `received ${records.length}. Refusing to generate.`,
    );
  }

  // Single deterministic sort by canonical id; shared by TS module, neutral
  // JSON, and native capability shards -- the record artifacts ONLY.
  const sortedRecords = sortById(records);
  // Parents are derived EXCLUSIVELY from the records (no hand-authored base),
  // which keeps the generator's bootstrap acyclic with its own generated
  // output, and from the canonical record SEQUENCE rather than the id-sorted
  // view so each action enum keeps its authored (non-alphabetical) order.
  const parents = deriveParents(records);
  // The revision moves whenever any record's schema or content hash moves.
  const catalogRevision = sha256Hex(
    JSON.stringify(sortedRecords.map((record) => [record.id, record.hashes.schema, record.hashes.content])),
  ).slice(0, 16);
  const dataParams = { records: sortedRecords, catalogRevision, recordCount: sortedRecords.length };

  const tsData = buildTsDataModule(dataParams);
  const parentDefs = buildParentDefsModule(parents);
  const costIndex = buildCostIndexModule(sortedRecords);
  const neutralJson = `${buildNeutralModel(dataParams)}\n`;

  // Native capability shards: complete canonical records, per-parent, sanitized symbols.
  const nativeShards: readonly NativeCapabilityShard[] = buildNativeCapabilityShards(sortedRecords);
  const capDir = nativeCapabilityDir();
  const capTargets: GeneratedTarget[] = [
    [
      resolve(capDir, 'McpGeneratedCapabilityShards.h'),
      buildNativeCapabilityIndexHeader(nativeShards, catalogRevision),
    ],
    ...nativeShards.map(
      (s) => [resolve(capDir, `McpGeneratedCapabilityShards_${s.symbol}.cpp`), buildNativeCapabilityShardSource(s)] as GeneratedTarget,
    ),
  ];

  const targets: GeneratedTarget[] = [
    [resolve(genDir(), 'canonical-registry.generated.ts'), tsData],
    [resolve(genDir(), 'canonical-registry.generated.json'), neutralJson],
    [resolve(genDir(), 'parent-tool-definitions.generated.ts'), parentDefs],
    [resolve(genDir(), 'capability-cost-index.generated.ts'), costIndex],
    [
      resolve(docsDir(), 'action-reference.generated.md'),
      buildActionReferenceDoc({ records: sortedRecords, catalogRevision }),
    ],
    [resolve(nativeToolsDir(), 'McpGeneratedParentRegistry.cpp'), buildParentRegistrySource(parents)],
    ...capTargets,
  ];

  if (nativeShards.length === 0) {
    throw new Error('FATAL: native capability shards produced zero shards. Refusing to generate.');
  }

  return targets;
};

