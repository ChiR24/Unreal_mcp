/**
 * tests/unit/capability-records/native-shard-records.ts
 *
 * Task 29 support: decode the COMPLETE canonical records that the generated
 * native capability shards carry, as structured data.
 *
 * The shards are emitted by scripts/canonical-registry/native-shards.ts as
 * bounded, backslash-escaped, pure-ASCII C++ string literals:
 *
 *   TEXT("<chunk0>"),
 *   TEXT("<chunk1>"),
 *
 * Reconstruction decodes each literal and concatenates them in order. Non-ASCII was already turned
 * into JSON `\uXXXX` escapes BEFORE the C++ escaping, so undoing the C++ layer
 * leaves valid JSON that `JSON.parse` restores to the identical value.
 *
 * This is deliberately NOT a regex over C++ semantics: it decodes the payload
 * and parses it, so every downstream comparison is structured-data equality
 * rather than source-text matching.
 */
import { readFileSync, readdirSync } from 'node:fs';
import { resolve } from 'node:path';

const SHARD_DIR = resolve(
  process.cwd(),
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Generated',
);

/** One `{ "record": <CapabilityRecord> }` wrapper as emitted into a shard. */
export interface NativeShardEntry {
  readonly record: Record<string, unknown>;
}

export interface NativeShardFile {
  readonly file: string;
  readonly records: readonly Record<string, unknown>[];
}

/** Ordered shard source paths (excludes the index header). */
export const listNativeShardFiles = (): readonly string[] =>
  readdirSync(SHARD_DIR)
    .filter((n) => n.startsWith('McpGeneratedCapabilityShards_') && n.endsWith('.cpp'))
    .sort()
    .map((n) => resolve(SHARD_DIR, n));

// The emitter escapes only `\` and `"` — JSON's own string escapes — so every
// TEXT("...") literal is a JSON string literal, and JSON.parse inverts it exactly.
const decodeChunks = (source: string): string =>
  [...source.matchAll(/TEXT\("((?:[^"\\]|\\.)*)"\)/g)].map((match) => JSON.parse(`"${match[1]}"`) as string).join('');

/** Decode one shard source into its canonical records. */
export const readNativeShard = (path: string): NativeShardFile => {
  const source = readFileSync(path, 'utf8');
  const payload = decodeChunks(source);
  if (payload.trim().length === 0) {
    throw new Error(`Task 29: native shard ${path} produced an empty payload.`);
  }
  const parsed: unknown = JSON.parse(payload);
  if (!Array.isArray(parsed)) {
    throw new TypeError(`Task 29: native shard ${path} did not decode to an array.`);
  }
  const records = parsed.map((entry, index) => {
    if (typeof entry !== 'object' || entry === null || !('record' in entry)) {
      throw new TypeError(
        `Task 29: native shard ${path} entry ${index} is not a { record } wrapper.`,
      );
    }
    const rec = (entry as NativeShardEntry).record;
    if (typeof rec !== 'object' || rec === null) {
      throw new TypeError(`Task 29: native shard ${path} entry ${index} has a non-object record.`);
    }
    return rec;
  });
  return { file: path, records };
};

/** Every canonical record the native surface carries, keyed by canonical id. */
export const readAllNativeShardRecords = (): ReadonlyMap<string, Record<string, unknown>> => {
  const byId = new Map<string, Record<string, unknown>>();
  for (const path of listNativeShardFiles()) {
    for (const record of readNativeShard(path).records) {
      const id = record.id;
      if (typeof id !== 'string') {
        throw new TypeError(`Task 29: native shard ${path} carries a record without a string id.`);
      }
      if (byId.has(id)) {
        throw new Error(`Task 29: duplicate native canonical id "${id}" across shards.`);
      }
      byId.set(id, record);
    }
  }
  return byId;
};
