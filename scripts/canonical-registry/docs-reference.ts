// scripts/canonical-registry/docs-reference.ts
//
// Emits the published action reference. It is a generator target, so
// `registry:check` fails if it drifts from the records; a hand-maintained
// action table would silently rot the moment a record changed.
//
// The doc must not describe a client-visible multi-tool surface: the 23
// canonical parents are an INTERNAL routing boundary and the only public tool
// on either transport is `unreal`.

import type { CapabilityRecord } from '../../src/tools/catalog/capabilities/model.js';
import { sortById as byId } from '../../src/utils/serialization/ordering.js';

export interface DocsReferenceInput {
  readonly records: readonly CapabilityRecord[];
  readonly catalogRevision: string;
}

// Authored prose reaches markdown cells (guidance, summaries), so an unescaped
// pipe or newline would silently corrupt the table it lands in.
const cell = (value: string): string =>
  value.replace(/\|/g, '&#124;').replace(/\r?\n/g, ' ').trim();

const GENERATED_HEADER = (source: string): readonly string[] => [
  '<!-- GENERATED FILE - DO NOT EDIT.',
  '     Regenerate with `npm run registry:generate`; `npm run registry:check` gates drift.',
  `     Source of truth: ${source} -->`,
  '',
];

// The one framing sentence every generated reference repeats, so a reader who
// lands on a table cannot mistake the internal parents for a public listing.
const SURFACE_NOTE: readonly string[] = [
  'Both transports expose exactly ONE public MCP tool, `unreal`, with the four',
  'operations `search` / `describe` / `execute` / `configure`. The parent tools',
  'named in these tables are an INTERNAL routing boundary: they are never listed',
  'by `tools/list` and a direct `tools/call` on one returns a',
  '`DIRECT_TOOL_CALL_REMOVED` receipt rather than executing',
  '(`src/server/gateway/direct-call-migration.ts`).',
  '',
];

const legacyPairs = (record: CapabilityRecord): string =>
  record.legacyIds.length === 0
    ? '—'
    : record.legacyIds.map((l) => `\`${l.tool}.${l.action}\``).join(' ');

function parentSummaryRows(records: readonly CapabilityRecord[]): readonly string[] {
  const parents = [...new Set(records.map((r) => r.routing.parentTool))].sort();
  return parents.map((parent) => {
    const owned = records.filter((r) => r.routing.parentTool === parent);
    const count = (effect: string): number =>
      owned.filter((r) => r.behavior.effect === effect).length;
    const domains = [...new Set(owned.map((r) => r.discovery.domain))].sort();
    return (
      `| \`${parent}\` | ${owned.length} | ${count('read')} | ${count('write')} | ` +
      `${count('destructive')} | ${cell(domains.join(', '))} |`
    );
  });
}

export function buildActionReferenceDoc(input: DocsReferenceInput): string {
  const records = byId(input.records);
  const consented = records.filter((r) => r.policy.consent !== 'none');

  const lines: string[] = [
    ...GENERATED_HEADER('src/tools/catalog/capabilities/records/**'),
    '# Action reference',
    '',
    `Catalog revision: \`${input.catalogRevision}\``,
    '',
    ...SURFACE_NOTE,
    `The catalog declares ${records.length} capabilities across`,
    `${new Set(records.map((r) => r.routing.parentTool)).size} internal parent tools.`,
    'Every row is derived from the capability record that the gateway actually',
    'validates against, so `execute` cannot accept an action this table omits.',
    '',
    '## Reading a row',
    '',
    '- **Capability** — the canonical id. Pass it to `describe`/`execute` as the',
    '  `tool` + `action` pair shown in the same row.',
    '- **Effect** — `read` | `write` | `destructive` (`behavior.effect`).',
    '- **Scope** — the capability scope the caller must hold',
    '  (`policy.requiredScope`). Scope membership is EXACT-SET with an `admin`',
    '  wildcard: holding `write` does NOT imply `read`.',
    '- **Consent** — `none` | `explicit` | `elevated` (`policy.consent`). A',
    '  non-`none` value must be satisfied by an execute-envelope `consent`',
    '  sibling naming THAT capability; it is never a handler parameter.',
    '- **Legacy pairs** — the pre-gateway `{tool, action}` spellings that still',
    '  resolve to this capability.',
    '',
    '## Per-parent totals',
    '',
    '| Parent tool | Capabilities | read | write | destructive | Domains |',
    '| --- | --- | --- | --- | --- | --- |',
    ...parentSummaryRows(records),
    '',
    '## Capabilities requiring consent',
    '',
    consented.length === 0
      ? 'No capability declares a non-`none` consent mode.'
      : `${consented.length} of ${records.length} capabilities require consent.`,
    '',
    '| Capability | Tool | Action | Effect | Consent |',
    '| --- | --- | --- | --- | --- |',
    ...consented.map(
      (r) =>
        `| \`${r.id}\` | \`${r.routing.parentTool}\` | \`${r.routing.dispatchAction}\` | ` +
        `${r.behavior.effect} | ${r.policy.consent} |`,
    ),
    '',
    '## Full action reference',
    '',
    '| Capability | Tool | Action | Effect | Scope | Consent | Legacy pairs |',
    '| --- | --- | --- | --- | --- | --- | --- |',
    ...records.map(
      (r) =>
        `| \`${r.id}\` | \`${r.routing.parentTool}\` | \`${r.routing.dispatchAction}\` | ` +
        `${r.behavior.effect} | ${r.policy.requiredScope} | ${r.policy.consent} | ` +
        `${legacyPairs(r)} |`,
    ),
    '',
  ];
  return lines.join('\n');
}
