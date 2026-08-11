/**
 * tests/unit/capability-records/record-parity.test.ts
 *
 * TASK 29 GATE - exact capability-level parity across every shipped surface.
 *
 * The pre-existing gates prove parity at PARENT-COUNT granularity only:
 * `npm run test:native-parity` compares 23 tool names + per-tool action-name
 * SETS, and compares input-schema shape for the default schema scope, which
 * is every canonical parent the TypeScript surface already discovered (all 23
 * in the real repository). Fixture tests can still narrow the scope via an
 * explicit `schemaParityTools` override. `npm run registry:check` byte-compares
 * generated files and reports only "<path> is stale". Neither proves that the
 * 1,380 canonical records are field-for-field identical on both transports.
 *
 * This gate closes that gap by comparing STRUCTURED DATA for all 1,380 records
 * across the three shipped surfaces:
 *   TS canonical records  (src/tools/catalog/capabilities/records/aggregate.ts)
 *   neutral JSON          (generated/canonical-registry.generated.json)
 *   native shards         (Private/MCP/Generated/McpGeneratedCapabilityShards_*.cpp)
 *
 * The default-scope widening makes this gate strictly stronger: even when
 * every per-parent schema check passes, a single record drift on either
 * transport still fails here. Every field is in scope: aliases, legacyIds,
 * discovery, BOTH schemas (input and output), examples, availability,
 * behavior, policy, cost, routing, normalization, deprecation, parent
 * metadata, and both hashes. Failures name the exact capability id and JSON
 * pointer.
 */
import { describe, expect, it } from 'vitest';
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { ALL_CAPABILITY_RECORDS } from '../../../src/tools/catalog/capabilities/records/aggregate.js';
import { readAllNativeShardRecords, listNativeShardFiles } from './native-shard-records.js';
import { diffPointers, formatDiffs, type PointerDiff } from './support.js';

const EXPECTED_RECORDS = 1380;
const EXPECTED_PARENTS = 23;

const NEUTRAL_JSON_PATH = resolve(
  process.cwd(),
  'src/tools/catalog/capabilities/generated/canonical-registry.generated.json',
);

interface NeutralRegistry {
  readonly catalogRevision: string;
  readonly recordCount: number;
  readonly records: readonly Record<string, unknown>[];
  readonly summaries: readonly Record<string, unknown>[];
}

const loadNeutral = (): NeutralRegistry =>
  JSON.parse(readFileSync(NEUTRAL_JSON_PATH, 'utf8')) as NeutralRegistry;

/** Round-trip through JSON so readonly/branded TS values compare as plain data. */
const plain = (value: unknown): unknown => JSON.parse(JSON.stringify(value));

const byId = (records: readonly Record<string, unknown>[]): ReadonlyMap<string, Record<string, unknown>> => {
  const map = new Map<string, Record<string, unknown>>();
  for (const r of records) {
    const id = r.id;
    if (typeof id !== 'string') throw new TypeError('Task 29: record without a string id.');
    if (map.has(id)) throw new Error(`Task 29: duplicate canonical id "${id}".`);
    map.set(id, r);
  }
  return map;
};

describe('Task 29 - the full 1,380-record universe exists on every surface', () => {
  it('TypeScript canonical records number exactly 1,380 with unique ids', () => {
    expect(ALL_CAPABILITY_RECORDS.length).toBe(EXPECTED_RECORDS);
    expect(new Set(ALL_CAPABILITY_RECORDS.map((r) => r.id)).size).toBe(EXPECTED_RECORDS);
  });

  it('the neutral JSON carries exactly 1,380 full records and a matching recordCount', () => {
    const neutral = loadNeutral();
    expect(neutral.records.length).toBe(EXPECTED_RECORDS);
    expect(neutral.recordCount).toBe(EXPECTED_RECORDS);
    expect(neutral.summaries.length).toBe(EXPECTED_RECORDS);
    expect(new Set(neutral.records.map((r) => String(r.id))).size).toBe(EXPECTED_RECORDS);
  });

  it('the native shards decode to exactly 1,380 records across 23 per-parent shards', () => {
    expect(listNativeShardFiles().length).toBe(EXPECTED_PARENTS);
    expect(readAllNativeShardRecords().size).toBe(EXPECTED_RECORDS);
  });

  it('the canonical id set is identical across TypeScript, neutral JSON and native', () => {
    const ts = new Set(ALL_CAPABILITY_RECORDS.map((r) => String(r.id)));
    const neutral = new Set(loadNeutral().records.map((r) => String(r.id)));
    const native = new Set(readAllNativeShardRecords().keys());

    const missingFromNeutral = [...ts].filter((id) => !neutral.has(id)).sort();
    const missingFromNative = [...ts].filter((id) => !native.has(id)).sort();
    const extraInNeutral = [...neutral].filter((id) => !ts.has(id)).sort();
    const extraInNative = [...native].filter((id) => !ts.has(id)).sort();

    expect(missingFromNeutral, `missing from neutral JSON: ${missingFromNeutral.join(', ')}`).toEqual([]);
    expect(missingFromNative, `missing from native shards: ${missingFromNative.join(', ')}`).toEqual([]);
    expect(extraInNeutral, `extra in neutral JSON: ${extraInNeutral.join(', ')}`).toEqual([]);
    expect(extraInNative, `extra in native shards: ${extraInNative.join(', ')}`).toEqual([]);
  });
});

describe('Task 29 - every field of every record is identical on both transports', () => {
  it('all 1,380 TypeScript records equal their neutral-JSON projection field-for-field', () => {
    const neutral = byId(loadNeutral().records);
    const diffs: PointerDiff[] = [];
    let compared = 0;
    for (const record of ALL_CAPABILITY_RECORDS) {
      const id = String(record.id);
      const other = neutral.get(id);
      if (other === undefined) {
        diffs.push({ id, pointer: '/', expected: '<present>', actual: '<absent>' });
        continue;
      }
      compared += 1;
      diffs.push(...diffPointers(id, plain(record), other));
      if (diffs.length >= 5) break;
    }
    expect(diffs.length, formatDiffs('TypeScript vs neutral JSON', diffs)).toBe(0);
    expect(compared).toBe(EXPECTED_RECORDS);
  });

  it('all 1,380 TypeScript records equal their native shard projection field-for-field', () => {
    const native = readAllNativeShardRecords();
    const diffs: PointerDiff[] = [];
    let compared = 0;
    for (const record of ALL_CAPABILITY_RECORDS) {
      const id = String(record.id);
      const other = native.get(id);
      if (other === undefined) {
        diffs.push({ id, pointer: '/', expected: '<present>', actual: '<absent>' });
        continue;
      }
      compared += 1;
      diffs.push(...diffPointers(id, plain(record), other));
      if (diffs.length >= 5) break;
    }
    expect(diffs.length, formatDiffs('TypeScript vs native shards', diffs)).toBe(0);
    expect(compared).toBe(EXPECTED_RECORDS);
  });

  it('both transports agree on both hashes for all 1,380 records', () => {
    const native = readAllNativeShardRecords();
    const neutral = byId(loadNeutral().records);
    const mismatches: string[] = [];
    for (const record of ALL_CAPABILITY_RECORDS) {
      const id = String(record.id);
      const expectedHashes = plain(record.hashes);
      for (const [label, source] of [['native', native.get(id)], ['neutral', neutral.get(id)]] as const) {
        const actual = source?.hashes;
        if (JSON.stringify(expectedHashes) !== JSON.stringify(actual)) {
          mismatches.push(`${id} (${label}) pointer=/hashes expected=${JSON.stringify(expectedHashes)} actual=${JSON.stringify(actual)}`);
        }
      }
    }
    expect(mismatches, `hash drift:\n${mismatches.slice(0, 10).join('\n')}`).toEqual([]);
  });
});

describe('Task 29 - the 23 private parent routes survive intact', () => {
  it('exactly 23 distinct parent tools own the 1,380 records on every surface', () => {
    const tsParents = new Set(ALL_CAPABILITY_RECORDS.map((r) => String(r.routing.parentTool)));
    expect(tsParents.size).toBe(EXPECTED_PARENTS);

    const nativeParents = new Set<string>();
    for (const record of readAllNativeShardRecords().values()) {
      const routing = record.routing;
      if (typeof routing !== 'object' || routing === null) {
        throw new TypeError('Task 29: native record without routing.');
      }
      nativeParents.add(String((routing as Record<string, unknown>).parentTool));
    }
    expect(nativeParents.size).toBe(EXPECTED_PARENTS);
    expect([...nativeParents].sort()).toEqual([...tsParents].sort());
  });

  /**
   * `routing.dispatchAction` is the INTERNAL dispatch target, not an echo of the
   * legacy action: 207 records deliberately delegate to a different internal
   * handler (dispatchMode `action` / `tool` / `local`). The invariant that must
   * hold for the 23 private parent routes is therefore that the legacy TOOL and
   * the owning parent agree, that every legacy pair is globally unique, and that
   * the dispatch mode is one of the three declared modes.
   */
  it('every record carries exactly one legacy {tool, action} pair owned by its own parent', () => {
    const offenders: string[] = [];
    const seenPairs = new Set<string>();
    const validModes = new Set(['tool', 'action', 'local']);

    for (const record of ALL_CAPABILITY_RECORDS) {
      const id = String(record.id);
      if (record.legacyIds.length !== 1) {
        offenders.push(`${id} pointer=/legacyIds has ${record.legacyIds.length} entries, expected 1`);
        continue;
      }
      const legacy = record.legacyIds[0];
      if (legacy === undefined) {
        offenders.push(`${id} pointer=/legacyIds/0 absent`);
        continue;
      }
      if (String(legacy.tool) !== String(record.routing.parentTool)) {
        offenders.push(
          `${id} pointer=/legacyIds/0/tool ${String(legacy.tool)} != /routing/parentTool ${String(record.routing.parentTool)}`,
        );
      }
      const pair = `${String(legacy.tool)}::${String(legacy.action)}`;
      if (seenPairs.has(pair)) {
        offenders.push(`${id} pointer=/legacyIds/0 duplicates legacy pair ${pair}`);
      }
      seenPairs.add(pair);

      if (!validModes.has(String(record.routing.dispatchMode))) {
        offenders.push(
          `${id} pointer=/routing/dispatchMode unknown mode ${String(record.routing.dispatchMode)}`,
        );
      }
      if (String(record.routing.dispatchAction).length === 0) {
        offenders.push(`${id} pointer=/routing/dispatchAction is empty`);
      }
    }

    expect(offenders, `parent-route drift:\n${offenders.slice(0, 10).join('\n')}`).toEqual([]);
    expect(seenPairs.size).toBe(EXPECTED_RECORDS);
  });
});
