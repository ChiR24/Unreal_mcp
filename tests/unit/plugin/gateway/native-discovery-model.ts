/// <reference types="node" />

// Task 25: the normalized discovery contract shared by the native `/mcp`
// gateway and the TypeScript gateway.
//
// This module is the executable specification. It reads the SAME generated
// canonical registry the native capability shards are emitted from, and renders
// results through a canonical serializer, so a native harness and this
// reference can be diffed byte-for-byte.
//
// Serialization rules (both surfaces must match exactly):
//   * object keys sorted by UTF-16 code unit, compact separators, no spaces
//   * every code unit above 0x7e escaped as \uXXXX, so output is pure ASCII
//   * integers printed as integers
//
// Record subtrees that carry non-integer numbers (`examples`) are deliberately
// NOT inlined: floating-point text formatting is the one place C++ and
// JavaScript can disagree without either being wrong. Examples are covered by
// the generator's per-record content hash instead, which both surfaces echo.

import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

export type JsonValue =
  | string
  | number
  | boolean
  | null
  | readonly JsonValue[]
  | { readonly [key: string]: JsonValue };

export interface DiscoveryRecord {
  readonly id: string;
  readonly discovery: {
    readonly domain: string;
    readonly family: string;
    readonly topics: readonly string[];
    readonly summary: string;
    readonly whenToUse: readonly string[];
    readonly whenNotToUse: readonly string[];
  };
  readonly schemas: { readonly input: JsonValue; readonly output: JsonValue };
  readonly examples: readonly JsonValue[];
  readonly availability: JsonValue;
  readonly behavior: { readonly effect: string } & Record<string, JsonValue>;
  readonly policy: JsonValue;
  readonly cost: JsonValue;
  readonly routing: { readonly parentTool: string; readonly dispatchAction: string };
  readonly deprecation: { readonly status: string };
  readonly hashes: { readonly algorithm: string; readonly schema: string; readonly content: string };
}

export interface CanonicalRegistry {
  readonly catalogRevision: string;
  readonly recordCount: number;
  readonly records: readonly DiscoveryRecord[];
}

const REGISTRY_PATH = resolve(
  process.cwd(),
  'src/tools/catalog/capabilities/generated/canonical-registry.generated.json',
);

let cached: CanonicalRegistry | undefined;

export const loadCanonicalRegistry = (): CanonicalRegistry => {
  if (cached === undefined) {
    cached = JSON.parse(readFileSync(REGISTRY_PATH, 'utf8')) as CanonicalRegistry;
  }
  return cached;
};

const escapeString = (value: string): string => {
  let out = '"';
  for (const ch of [...value]) {
    const code = ch.codePointAt(0) ?? 0;
    if (ch === '"') out += '\\"';
    else if (ch === '\\') out += '\\\\';
    else if (code >= 0x20 && code <= 0x7e) out += ch;
    else {
      for (let i = 0; i < ch.length; i += 1) {
        out += `\\u${ch.charCodeAt(i).toString(16).padStart(4, '0')}`;
      }
    }
  }
  return `${out}"`;
};

/** Deterministic, ASCII-only, key-sorted JSON. Integers only. */
export const canonicalJson = (value: JsonValue): string => {
  if (value === null) return 'null';
  if (typeof value === 'boolean') return value ? 'true' : 'false';
  if (typeof value === 'string') return escapeString(value);
  if (typeof value === 'number') {
    if (!Number.isInteger(value)) {
      throw new Error(`canonicalJson: non-integer number ${value} is not cross-language stable`);
    }
    return String(value);
  }
  if (Array.isArray(value)) return `[${value.map(canonicalJson).join(',')}]`;
  const entries = Object.entries(value as Record<string, JsonValue>)
    .filter(([, v]) => v !== undefined)
    .sort(([a], [b]) => (a < b ? -1 : a > b ? 1 : 0));
  return `{${entries.map(([k, v]) => `${escapeString(k)}:${canonicalJson(v)}`).join(',')}}`;
};

export const MAX_SUGGESTIONS = 3;

const levenshtein = (a: string, b: string): number => {
  const m = a.length;
  const n = b.length;
  if (m === 0) return n;
  if (n === 0) return m;
  let previous = Array.from({ length: n + 1 }, (_, index) => index);
  let current = new Array<number>(n + 1).fill(0);
  for (let i = 1; i <= m; i += 1) {
    current[0] = i;
    for (let j = 1; j <= n; j += 1) {
      const cost = a[i - 1] === b[j - 1] ? 0 : 1;
      current[j] = Math.min(previous[j] + 1, current[j - 1] + 1, previous[j - 1] + cost);
    }
    [previous, current] = [current, previous];
  }
  return previous[n];
};

const commonPrefixLength = (left: string, right: string): number => {
  const limit = Math.min(left.length, right.length);
  let shared = 0;
  while (shared < limit && left[shared] === right[shared]) shared += 1;
  return shared;
};

/** Ordinal comparison. Canonical ids/actions/params are ASCII, so this is the
 *  same order C++ FString comparison produces. */
export const ordinalCompare = (a: string, b: string): number => (a < b ? -1 : a > b ? 1 : 0);

// Edit distance alone ties candidates a caller would never confuse, and the
// shared prefix a typo keeps is the better discriminator. The trailing ordinal
// comparison makes the order total, so neither surface depends on an unstable
// sort or on catalog insertion order.
export const closestMatches = (
  target: string,
  candidates: readonly string[],
  limit: number = MAX_SUGGESTIONS,
): readonly string[] => {
  if (limit <= 0) return [];
  const normalized = target.trim().toLowerCase();
  if (normalized.length === 0) return candidates.slice(0, limit);
  return candidates
    .map((candidate) => {
      const lower = candidate.toLowerCase();
      let score = levenshtein(lower, normalized);
      if (lower.includes(normalized) || normalized.includes(lower)) score -= 4;
      return { candidate, score, prefix: commonPrefixLength(lower, normalized) };
    })
    .sort(
      (left, right) =>
        left.score - right.score ||
        right.prefix - left.prefix ||
        ordinalCompare(left.candidate, right.candidate),
    )
    .slice(0, limit)
    .map((entry) => entry.candidate);
};

export const utf8Length = (value: string): number => Buffer.byteLength(value, 'utf8');

export interface DiscoveryInput {
  readonly query?: string;
  readonly domain?: string;
  readonly family?: string;
  readonly tool?: string;
  readonly action?: string;
  readonly param?: string;
  readonly limit?: number;
  readonly offset?: number;
}

export const clamp = (value: number, low: number, high: number): number =>
  Math.min(Math.max(value, low), high);

export const boundedLimit = (raw: number | undefined, fallback: number, max: number): number =>
  raw === undefined ? fallback : clamp(Math.trunc(raw), 1, max);

export const boundedOffset = (raw: number | undefined): number =>
  raw === undefined ? 0 : Math.max(0, Math.trunc(raw));

export const isAvailable = (record: DiscoveryRecord): boolean =>
  record.deprecation.status !== 'removed';

export const guidedError = (
  operation: string,
  errorCode: string,
  message: string,
  extra: Readonly<Record<string, JsonValue>>,
): JsonValue => ({
  catalogRevision: loadCanonicalRegistry().catalogRevision,
  error: message,
  errorCode,
  message,
  operation,
  success: false,
  ...extra,
});

export const sortedUnique = (values: readonly string[]): readonly string[] =>
  [...new Set(values)].sort(ordinalCompare);

export const allDomains = (): readonly string[] =>
  sortedUnique(loadCanonicalRegistry().records.map((r) => r.discovery.domain));

export const allFamilies = (): readonly string[] =>
  sortedUnique(loadCanonicalRegistry().records.map((r) => r.discovery.family));

export const allParents = (): readonly string[] =>
  sortedUnique(loadCanonicalRegistry().records.map((r) => r.routing.parentTool));

