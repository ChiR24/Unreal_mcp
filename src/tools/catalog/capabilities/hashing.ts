import { createHash } from 'node:crypto';

export class CapabilitySerializationError extends Error {
  readonly code = 'CAPABILITY_SERIALIZATION_ERROR' as const;
  constructor(message: string) {
    super(message);
    this.name = 'CapabilitySerializationError';
  }
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

/**
 * Drop `undefined`-valued properties so a value can be handed to
 * `stableJsonStringify`, which rejects `undefined` outright.
 *
 * Lives here beside the serializer it feeds, because the pair
 * `stableJsonStringify(stripUndefined(x))` IS the project's canonical
 * deterministic encoding — it was written out privately in the receipt envelope
 * and again in the migration artifact, and the second copy had already dropped
 * the number validation the first one relies on.
 *
 * Uses `defineProperty` for `__proto__`: `JSON.parse` creates that key as a real
 * own property, and a plain assignment would hand it to `Object.prototype`'s
 * setter, silently dropping the field and changing the hash.
 */
export function stripUndefined(value: unknown): unknown {
  if (Array.isArray(value)) return value.map(stripUndefined);
  if (value !== null && typeof value === 'object') {
    const out: Record<string, unknown> = {};
    for (const [key, entry] of Object.entries(value as Record<string, unknown>)) {
      if (entry === undefined) continue;
      const shaped = stripUndefined(entry);
      if (key === '__proto__') {
        Object.defineProperty(out, key, {
          value: shaped, enumerable: true, writable: true, configurable: true,
        });
      } else {
        out[key] = shaped;
      }
    }
    return out;
  }
  return value;
}
