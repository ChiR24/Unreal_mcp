import { ZodError } from 'zod';

import { CapabilityCatalogSchema } from './catalog-schema.js';
import { computeCapabilityHashes } from './hashing.js';
import type { CapabilityCatalog, CapabilityRecord } from './model.js';
import { CapabilityRecordSchema, CapabilityRecordSourceSchema } from './record-schema.js';

function escapeToken(segment: PropertyKey): string {
  return String(segment).replace(/~/g, '~0').replace(/\//g, '~1');
}

function toJsonPointer(path: readonly PropertyKey[]): string {
  if (path.length === 0) return '';
  return `/${path.map(escapeToken).join('/')}`;
}

export function capabilityErrorPointers(error: unknown): readonly string[] {
  if (!(error instanceof ZodError)) return [''];
  const pointers = new Set<string>();
  for (const issue of error.issues) {
    if (issue.code === 'unrecognized_keys') {
      for (const key of issue.keys) {
        pointers.add(toJsonPointer([...issue.path, key]));
      }
      continue;
    }
    pointers.add(toJsonPointer(issue.path));
  }
  return [...pointers];
}

export function createCapabilityRecord(source: unknown): CapabilityRecord {
  const validated = CapabilityRecordSourceSchema.parse(source);
  const hashes = computeCapabilityHashes(validated);
  return { ...validated, hashes };
}

export function parseCapabilityRecord(record: unknown): CapabilityRecord {
  return CapabilityRecordSchema.parse(record);
}

export function parseCapabilityCatalog(catalog: unknown): CapabilityCatalog {
  return CapabilityCatalogSchema.parse(catalog);
}
