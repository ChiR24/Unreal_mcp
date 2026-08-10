// scripts/gateway-manifest/validate.ts
// Canonical pilot catalog validation: precise JSON pointers, recoverable
// canonical IDs, duplicate-ID reporting with exact ID value, and expected-ID
// set support so a removed record reports its exact missing canonical ID.
// Reuses CapabilityCatalogSchema (Task 2) - no second schema DSL.

import { CapabilityCatalogSchema, capabilityErrorPointers } from '../../src/tools/catalog/capabilities/index.js';
import type { CapabilityRecord } from '../../src/tools/catalog/capabilities/model.js';

export interface PilotValidationError {
  readonly pointer: string;
  readonly message: string;
  readonly canonicalId?: string;
}

export type PilotValidationResult =
  | { readonly success: true; readonly records: readonly CapabilityRecord[] }
  | { readonly success: false; readonly errors: readonly PilotValidationError[] };

export class PilotCatalogValidationError extends Error {
  readonly errors: readonly PilotValidationError[];
  constructor(errors: readonly PilotValidationError[]) {
    super('Pilot catalog validation failed');
    this.name = 'PilotCatalogValidationError';
    this.errors = errors;
  }
}

function isPlainObject(value: unknown): value is Record<string, unknown> {
  return typeof value === 'object' && value !== null && !Array.isArray(value);
}

function recoverCanonicalId(input: unknown, index: number): string | undefined {
  if (!Array.isArray(input)) return undefined;
  const record = input[index];
  if (!isPlainObject(record)) return undefined;
  const id = record.id;
  return typeof id === 'string' ? id : undefined;
}

function isDuplicateIdIssue(message: string): boolean {
  return message === 'duplicate canonical capability id';
}

export function validatePilotCatalog(
  input: unknown,
  expectedIds?: readonly string[]
): PilotValidationResult {
  const result = CapabilityCatalogSchema.safeParse(input);
  if (!result.success) {
    const pointers = capabilityErrorPointers(result.error);
    const errors: PilotValidationError[] = result.error.issues.map((issue, i) => {
      const pointer = pointers[i] ?? '';
      const canonicalId = recoverCanonicalId(input, Number(issue.path[0] ?? -1));
      const message = isDuplicateIdIssue(issue.message) && canonicalId
        ? `duplicate canonical capability id: ${canonicalId}`
        : issue.message;
      return { pointer, message, ...(canonicalId ? { canonicalId } : {}) };
    });
    return { success: false, errors };
  }

  const records = result.data;
  if (expectedIds && expectedIds.length > 0) {
    const present: ReadonlySet<string> = new Set(records.map((r) => r.id));
    const missing = expectedIds.filter((id) => !present.has(id));
    if (missing.length > 0) {
      const errors: PilotValidationError[] = missing.map((id) => ({
        pointer: '',
        message: `expected canonical id not found in catalog: ${id}`,
        canonicalId: id
      }));
      return { success: false, errors };
    }
  }

  return { success: true, records };
}
