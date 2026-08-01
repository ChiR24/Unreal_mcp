// scripts/canonical-registry/docs-reference.ts
//
// Emits the published action reference and the legacy->canonical migration
// reference.
//
// Both are generator targets, so `registry:check` fails if either drifts from
// the records / migration map. A hand-maintained action table silently rots the
// moment a record changes; this one cannot.
//
// Neither doc may describe a client-visible multi-tool surface: the 23 canonical
// parents are an INTERNAL routing boundary and the only public tool on either
// transport is `unreal`. tests/unit/docs/docs-claim-contract.test.ts enforces
// that on the emitted bytes.

import type { CapabilityRecord } from '../../src/tools/catalog/capabilities/model.js';
import { sortById as byId } from '../../src/utils/serialization/ordering.js';

/**
 * The migration-map projection this module consumes. Deliberately structural
 * and minimal so the generator entrypoint keeps passing the real
 * `MigrationMap` without this file importing runtime migration code.
 */
export type MigrationEntryView = {
  readonly disposition: string;
  readonly canonicalId?: string | null;
  readonly removal?: {
    readonly since: string;
    readonly guidance: string;
    readonly replacement?: string;
  };
};

export interface DocsReferenceInput {
  readonly records: readonly CapabilityRecord[];
  readonly catalogRevision: string;
}

export interface MigrationReferenceInput extends DocsReferenceInput {
  readonly migrationEntries: ReadonlyMap<string, MigrationEntryView>;
}

// Authored prose reaches markdown cells (guidance, summaries), so an unescaped
// pipe or newline would silently corrupt the table it lands in.
const cell = (value: string): string =>
  value.replace(/\|/g, '\\|').replace(/\r?\n/g, ' ').trim();

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
  const deprecated = records.filter((r) => r.deprecation.status !== 'active');

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
    '  resolve to this capability. See the migration reference for the full map.',
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
    '## Deprecated and removed capabilities',
    '',
    deprecated.length === 0
      ? 'No capability record carries a `deprecated` or `removed` status.'
      : `${deprecated.length} capability record${deprecated.length === 1 ? ' is' : 's are'} no longer \`active\`.`,
    '',
    '| Capability | Status | Since | Guidance | Replacement |',
    '| --- | --- | --- | --- | --- |',
    ...deprecated.map((r) => {
      const d = r.deprecation;
      const since = 'since' in d ? d.since : '—';
      const guidance = 'guidance' in d ? cell(d.guidance) : '—';
      const replacement = 'replacement' in d && d.replacement ? `\`${d.replacement}\`` : '—';
      return `| \`${r.id}\` | ${d.status} | ${since} | ${guidance} | ${replacement} |`;
    }),
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

type DispositionCounts = Readonly<Record<string, number>>;

function countDispositions(entries: ReadonlyMap<string, MigrationEntryView>): DispositionCounts {
  const counts: Record<string, number> = {};
  for (const entry of entries.values()) {
    counts[entry.disposition] = (counts[entry.disposition] ?? 0) + 1;
  }
  return counts;
}

const sortedKeys = (entries: ReadonlyMap<string, MigrationEntryView>): readonly string[] =>
  [...entries.keys()].sort();

export function buildMigrationReferenceDoc(input: MigrationReferenceInput): string {
  const { migrationEntries } = input;
  const keys = sortedKeys(migrationEntries);
  const counts = countDispositions(migrationEntries);
  const removed = keys.filter((k) => migrationEntries.get(k)?.disposition === 'removed');

  const lines: string[] = [
    ...GENERATED_HEADER(
      'src/tools/catalog/capabilities/migration/migration-map.ts (Task 20 migration map)',
    ),
    '# Legacy to canonical migration reference',
    '',
    `Catalog revision: \`${input.catalogRevision}\``,
    '',
    ...SURFACE_NOTE,
    `Every one of the ${keys.length} shipped legacy \`{tool, action}\` occurrences`,
    'resolves to exactly one disposition. Nothing falls through to a default.',
    '',
    '## Dispositions',
    '',
    '| Disposition | Count | Meaning |',
    '| --- | --- | --- |',
    `| canonical | ${counts.canonical ?? 0} | Maps 1:1 to a live capability record. |`,
    `| alias | ${counts.alias ?? 0} | An alias of a canonical capability; resolves losslessly. |`,
    `| removed | ${counts.removed ?? 0} | The verb was retired. A typed removal with guidance — NOT a silent fallback. |`,
    `| non-translatable | ${counts['non-translatable'] ?? 0} | Refuses translation rather than coercing lossy parameters. |`,
    '',
    '## Retired verbs (typed removals)',
    '',
    'A `removed` disposition is a hard failure carrying guidance. Treat it as an',
    'error, never as a reason to fall back to the old behavior.',
    '',
    '| Legacy call | Since | Guidance | Replacement |',
    '| --- | --- | --- | --- |',
    ...removed.map((key) => {
      const entry = migrationEntries.get(key);
      const removal = entry?.removal;
      return (
        `| \`${key}\` | ${removal ? cell(removal.since) : '—'} | ` +
        `${removal ? cell(removal.guidance) : '—'} | ` +
        `${removal?.replacement ? `\`${removal.replacement}\`` : '—'} |`
      );
    }),
    '',
    '## Full migration map',
    '',
    '| Legacy call | Disposition | Canonical capability |',
    '| --- | --- | --- |',
    ...keys.map((key) => {
      const entry = migrationEntries.get(key);
      const canonical = entry?.canonicalId ? `\`${entry.canonicalId}\`` : '—';
      return `| \`${key}\` | ${entry?.disposition ?? 'unknown'} | ${canonical} |`;
    }),
    '',
  ];
  return lines.join('\n');
}
