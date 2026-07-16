import { z } from 'zod';

import { stableJsonStringify } from '../hashing.js';

import { type CapabilityId, CapabilityIdSchema } from '../identifiers.js';
import { type NextCall, NextCallSchema, type SemanticError, SemanticErrorSchema, type TaskStatus, TaskStatusSchema } from './errors.js';
import type { TypedHandle } from './handles.js';
import { TypedHandleSchema } from './handles.js';
import type {
  CatalogRevision,
  CorrelationId,
  IdempotencyKey
} from './ids.js';
import {
  CatalogRevisionSchema,
  CorrelationIdSchema,
  IdempotencyKeySchema
} from './ids.js';
import { JsonValueSchema } from './property-assignment.js';

// Stable, key-sorted serialization so receipt bytes are reproducible regardless of
// object insertion order (used for hashing / equality / diffing across transports).
// Undefined-valued optional fields are omitted so the shared capability serializer
// (which rejects `undefined`) stays byte-stable across transports.
function stripUndefined(value: unknown): unknown {
  if (Array.isArray(value)) return value.map(stripUndefined);
  if (value !== null && typeof value === 'object') {
    const out: Record<string, unknown> = {};
    for (const [key, entry] of Object.entries(value)) {
      if (entry !== undefined) out[key] = stripUndefined(entry);
    }
    return out;
  }
  return value;
}

// The `Receipt` type is derived directly from `ReceiptSchema` (with Readonly
// composition for immutable fields) so the wire envelope can never drift from
// its Zod contract (single source of truth).
export type Receipt = Readonly<z.infer<typeof ReceiptSchema>>;

// The receipt `data` payload is the schema's JSON boundary (`z.json()`), so the
// builder accepts exactly that type rather than a looser project alias.
type SemanticData = z.infer<typeof JsonValueSchema>;

export function buildSuccessReceipt(input: {
  capabilityId: CapabilityId;
  data: SemanticData;
  handles?: readonly TypedHandle[];
  correlationId?: CorrelationId;
  idempotencyId?: IdempotencyKey;
  catalogRevision?: CatalogRevision;
  changes?: readonly string[];
  warnings?: readonly string[];
  timingMs?: number;
  task?: TaskStatus;
  nextCalls?: readonly NextCall[];
}): Receipt {
  return {
    status: 'success',
    capabilityId: input.capabilityId,
    correlationId: input.correlationId,
    idempotencyId: input.idempotencyId,
    catalogRevision: input.catalogRevision,
    handles: input.handles ? [...input.handles] : [],
    changes: input.changes ? [...input.changes] : [],
    warnings: input.warnings ? [...input.warnings] : [],
    timingMs: input.timingMs,
    task: input.task,
    nextCalls: input.nextCalls ? [...input.nextCalls] : [],
    data: input.data
  };
}

export function buildErrorReceipt(input: {
  capabilityId: CapabilityId;
  error: SemanticError;
  correlationId?: CorrelationId;
  nextCalls?: readonly NextCall[];
}): Receipt {
  return {
    status: 'error',
    capabilityId: input.capabilityId,
    correlationId: input.correlationId,
    error: input.error,
    nextCalls: input.nextCalls ? [...input.nextCalls] : []
  };
}

export function serializeReceipt(receipt: Receipt): string {
  return stableJsonStringify(stripUndefined(receipt));
}

// Exact, strict, schema-backed receipt contract. Every field (handles, task,
// nextCalls, error, data) is parsed against its matching typed schema (see the
// `Receipt` type, derived from `ReceiptSchema`), and unknown top-level keys are
// rejected, so a malformed receipt cannot cross a transport boundary. Array
// fields use `.readonly()` so inferred types are ReadonlyArray (immutable).
// Each discriminated branch uses `.readonly()` so Zod v4 deep-freezes the parsed
// receipt (Object.isFrozen === true) - the readonly guarantee is runtime-real at
// the public boundary, not merely a disconnected type alias.
export const ReceiptSchema = z.discriminatedUnion('status', [
  z
    .strictObject({
      status: z.literal('success'),
      capabilityId: CapabilityIdSchema,
      correlationId: CorrelationIdSchema.optional(),
      idempotencyId: IdempotencyKeySchema.optional(),
      catalogRevision: CatalogRevisionSchema.optional(),
      handles: z.array(TypedHandleSchema).readonly(),
      changes: z.array(z.string()).readonly(),
      warnings: z.array(z.string()).readonly(),
      timingMs: z.number().optional(),
      task: TaskStatusSchema.optional(),
      nextCalls: z.array(NextCallSchema).readonly(),
      data: JsonValueSchema
    })
    .readonly(),
  z
    .strictObject({
      status: z.literal('error'),
      capabilityId: CapabilityIdSchema,
      correlationId: CorrelationIdSchema.optional(),
      error: SemanticErrorSchema,
      nextCalls: z.array(NextCallSchema).readonly()
    })
    .readonly()
]);
