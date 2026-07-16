import { createHash } from 'node:crypto';

import type { JsonValue } from './model.js';

const HEX64 = /^[0-9a-f]{64}$/;

export class CapabilitySerializationError extends Error {
  readonly code = 'CAPABILITY_SERIALIZATION_ERROR' as const;
  constructor(message: string) {
    super(message);
    this.name = 'CapabilitySerializationError';
  }
}

export function isHex64(value: unknown): value is string {
  return typeof value === 'string' && HEX64.test(value);
}

function isPlainObject(value: unknown): value is Record<string, unknown> {
  return typeof value === 'object' && value !== null && !Array.isArray(value);
}

function normalize(value: unknown): unknown {
  if (typeof value === 'bigint') {
    throw new CapabilitySerializationError('Capability hash input must not contain bigint');
  }
  if (
    typeof value === 'function' ||
    typeof value === 'symbol' ||
    typeof value === 'undefined'
  ) {
    throw new CapabilitySerializationError('Capability hash input must be JSON-compatible');
  }
  if (typeof value === 'number') {
    if (Number.isNaN(value) || !Number.isFinite(value)) {
      throw new CapabilitySerializationError(
        'Capability hash input must not contain NaN or Infinity'
      );
    }
    if (value === 0 && 1 / value < 0) return 0;
    return value;
  }
  if (Array.isArray(value)) return value.map(normalize);
  if (isPlainObject(value)) {
    const out: Record<string, unknown> = {};
    for (const key of Object.keys(value).sort()) out[key] = normalize(value[key]);
    return out;
  }
  return value;
}

export function stableJsonStringify(value: unknown): string {
  return JSON.stringify(normalize(value));
}

function sha256Hex(input: string): string {
  return createHash('sha256').update(input).digest('hex');
}

export type CapabilityHashBundle = {
  readonly algorithm: 'sha256';
  readonly schema: string;
  readonly content: string;
};

export function readField(source: unknown, key: string): unknown {
  return isPlainObject(source) ? source[key] : undefined;
}

export function computeCapabilityHashes(source: unknown): CapabilityHashBundle {
  const record = isPlainObject(source) ? source : {};
  const rawSchemas = readField(record, 'schemas');
  const schemas = isPlainObject(rawSchemas) ? rawSchemas : {};
  const input = readField(schemas, 'input');
  const output = readField(schemas, 'output');
  const schemaHash = sha256Hex(stableJsonStringify({ input, output }));
  const contentHash = sha256Hex(stableJsonStringify({ ...record, schemaHash }));
  return { algorithm: 'sha256', schema: schemaHash, content: contentHash };
}

export function jsonValueIsDeepEqual(a: JsonValue, b: JsonValue): boolean {
  return stableJsonStringify(a) === stableJsonStringify(b);
}
