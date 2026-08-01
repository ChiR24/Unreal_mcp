// scripts/canonical-registry/support-matrix.ts
//
// Emits the preview / undo / compensation support matrix for every capability.
//
// Both artifacts are generator targets, so `registry:check` fails if the matrix
// drifts from the records -- a capability cannot quietly change what it claims
// without the published matrix changing with it.

import type { CapabilityRecord } from '../../src/tools/catalog/capabilities/model.js';
import { sortById } from '../../src/utils/serialization/ordering.js';

const MATRIX_DOC = 'docs/capability-support-matrix.md';

type AxisCounts = {
  readonly preview: number;
  readonly undo: number;
  readonly compensation: number;
};

export interface SupportMatrixInput {
  readonly records: readonly CapabilityRecord[];
  readonly catalogRevision: string;
}

const isMutation = (record: CapabilityRecord): boolean => record.behavior.effect !== 'read';

// Markdown cell content is authored data (guidance / citations), so a stray
// pipe or newline would silently corrupt the table.
const cell = (value: string): string => value.replace(/\|/g, '\\|').replace(/\r?\n/g, ' ');

const previewCell = (record: CapabilityRecord): string => {
  const { preview } = record.behavior.semantics;
  if (preview.mode === 'none') return 'none';
  return `${preview.mode} (${preview.reports.join(', ')})`;
};

const undoCell = (record: CapabilityRecord): string => {
  const { undo } = record.behavior.semantics;
  if (undo.mode === 'none') return 'none';
  return `transaction: ${undo.transactionScope ?? ''}`;
};

const compensationCell = (record: CapabilityRecord): string => {
  const { compensation } = record.behavior.semantics;
  if (compensation.mode === 'none') return 'none';
  if (compensation.mode === 'inverse-capability') {
    return `inverse: ${compensation.inverse.join(', ')}`;
  }
  return 'manual-cleanup';
};

const countSupported = (records: readonly CapabilityRecord[]): AxisCounts => ({
  preview: records.filter((r) => r.behavior.semantics.preview.mode !== 'none').length,
  undo: records.filter((r) => r.behavior.semantics.undo.mode !== 'none').length,
  compensation: records.filter((r) => r.behavior.semantics.compensation.mode !== 'none').length
});

function buildParentRows(records: readonly CapabilityRecord[]): readonly string[] {
  const parents = [...new Set(records.map((r) => r.routing.parentTool))].sort();
  return parents.map((parent) => {
    const owned = records.filter((r) => r.routing.parentTool === parent);
    const counts = countSupported(owned);
    const mutations = owned.filter(isMutation).length;
    return `| \`${parent}\` | ${owned.length} | ${mutations} | ${counts.preview} | ${counts.undo} | ${counts.compensation} |`;
  });
}

function buildEarnedRows(records: readonly CapabilityRecord[]): readonly string[] {
  const rows: string[] = [];
  for (const record of records) {
    const { preview, undo, compensation } = record.behavior.semantics;
    if (preview.mode !== 'none') {
      rows.push(
        `| \`${record.id}\` | preview | ${cell(previewCell(record))} | ${preview.evidence.grade} | ${cell(preview.evidence.citation)} |`
      );
    }
    if (undo.mode !== 'none') {
      rows.push(
        `| \`${record.id}\` | undo | ${cell(undoCell(record))} | ${undo.evidence.grade} | ${cell(undo.evidence.citation)} |`
      );
    }
    if (compensation.mode !== 'none') {
      const detail =
        compensation.mode === 'manual-cleanup'
          ? `manual-cleanup: ${compensation.guidance ?? ''}`
          : compensationCell(record);
      rows.push(
        `| \`${record.id}\` | compensation | ${cell(detail)} | ${compensation.evidence.grade} | ${cell(compensation.evidence.citation)} |`
      );
    }
  }
  return rows;
}

export function buildSupportMatrixDoc(input: SupportMatrixInput): string {
  const records = sortById(input.records);
  const mutations = records.filter(isMutation);
  const counts = countSupported(records);
  const mutationCounts = countSupported(mutations);
  const isFullyPessimistic = (record: CapabilityRecord): boolean => {
    const s = record.behavior.semantics;
    return s.preview.mode === 'none' && s.undo.mode === 'none' && s.compensation.mode === 'none';
  };
  const fullyPessimistic = records.filter(isFullyPessimistic).length;
  const fullyPessimisticMutations = mutations.filter(isFullyPessimistic).length;

  const lines: string[] = [
    '<!-- GENERATED FILE - DO NOT EDIT.',
    '     Regenerate with `npm run registry:generate`; `npm run registry:check` gates drift.',
    '     Source of truth: src/tools/catalog/capabilities/records/**',
    '     Claims are elevated only by src/tools/catalog/capabilities/records/semantics/evidence-ledger.ts -->',
    '',
    '# Capability preview / undo / compensation support matrix',
    '',
    `Catalog revision: \`${input.catalogRevision}\``,
    '',
    `Every one of the ${records.length} capabilities declares all three semantics. The`,
    'default on each axis is the pessimistic one (no preview, not undoable, no',
    'compensation); a capability carries a stronger claim only where the ledger',
    'cites the implementation that proves it. A mostly-pessimistic matrix is the',
    'truthful result, not a gap in coverage.',
    '',
    '## Coverage',
    '',
    '| Axis | All capabilities | Mutations only |',
    '| --- | --- | --- |',
    `| Previewable | ${counts.preview} / ${records.length} | ${mutationCounts.preview} / ${mutations.length} |`,
    `| Undoable | ${counts.undo} / ${records.length} | ${mutationCounts.undo} / ${mutations.length} |`,
    `| Compensatable | ${counts.compensation} / ${records.length} | ${mutationCounts.compensation} / ${mutations.length} |`,
    `| Fully pessimistic | ${fullyPessimistic} / ${records.length} | ${fullyPessimisticMutations} / ${mutations.length} |`,
    '',
    '## By parent tool',
    '',
    '| Parent | Capabilities | Mutations | Preview | Undo | Compensation |',
    '| --- | --- | --- | --- | --- | --- |',
    ...buildParentRows(records),
    '',
    '## Capabilities with an earned (non-pessimistic) declaration',
    '',
    '| Capability | Axis | Declaration | Evidence grade | Evidence |',
    '| --- | --- | --- | --- | --- |',
    ...buildEarnedRows(records),
    '',
    '## Full matrix',
    '',
    '| Capability | Effect | Preview | Undo | Compensation |',
    '| --- | --- | --- | --- | --- |',
    ...records.map(
      (record) =>
        `| \`${record.id}\` | ${record.behavior.effect} | ${cell(previewCell(record))} | ${cell(undoCell(record))} | ${cell(compensationCell(record))} |`
    ),
    ''
  ];
  return lines.join('\n');
}

export function buildSupportMatrixJson(input: SupportMatrixInput): string {
  const records = sortById(input.records);
  const mutations = records.filter(isMutation);
  const model = {
    generator: MATRIX_DOC,
    catalogRevision: input.catalogRevision,
    recordCount: records.length,
    mutationCount: mutations.length,
    summary: {
      all: countSupported(records),
      mutations: countSupported(mutations)
    },
    capabilities: records.map((record) => ({
      id: record.id,
      parentTool: record.routing.parentTool,
      effect: record.behavior.effect,
      longRunning: record.behavior.longRunning,
      preview: record.behavior.semantics.preview,
      undo: record.behavior.semantics.undo,
      compensation: record.behavior.semantics.compensation
    }))
  };
  return `${JSON.stringify(model, null, 2)}\n`;
}
