// tests/eval/hash.ts
// Deterministic canonical hashing used for corpus and report fingerprints.

import { createHash } from 'node:crypto';

function stableStringify(value: unknown): string {
  if (value === null || typeof value !== 'object') {
    return JSON.stringify(value);
  }
  if (Array.isArray(value)) {
    return `[${value.map((item) => stableStringify(item)).join(',')}]`;
  }
  const record = value as Record<string, unknown>;
  const keys = Object.keys(record).sort();
  const pairs = keys.map((key) => `${JSON.stringify(key)}:${stableStringify(record[key])}`);
  return `{${pairs.join(',')}}`;
}

export function stableStringifyValue(value: unknown): string {
  return stableStringify(value);
}

export function sha256Canonical(value: unknown): string {
  return createHash('sha256').update(stableStringify(value)).digest('hex');
}
