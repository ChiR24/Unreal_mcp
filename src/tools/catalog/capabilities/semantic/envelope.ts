import { z } from 'zod';

import { stableJsonStringify, stripUndefined } from '../hashing.js';

import { type CapabilityId, CapabilityIdSchema } from '../identifiers.js';
import { type NextCall, NextCallSchema, type SemanticError, SemanticErrorSchema, type TaskStatus, TaskStatusSchema } from './errors.js';
import type { TypedHandle } from './handles.js';
import { TypedHandleSchema } from './handles.js';
import { LiveStateRevisionsSchema, type LiveStateRevisions } from './live-state-revisions.js';
import type {
  CapabilityRevision,
  CatalogRevision,
  CorrelationId,
  IdempotencyKey,
  RequestId,
  SchemaRevision
} from './ids.js';
import {
  CapabilityRevisionSchema,
  CatalogRevisionSchema,
  CorrelationIdSchema,
  IdempotencyKeySchema,
  RequestIdSchema,
  SchemaRevisionSchema
} from './ids.js';
import { JsonValueSchema } from './property-assignment.js';
import { boundArray, boundStrings, redactText } from './receipt-redaction.js';

// Evidence that the handler result was held to the capability's declared output
// contract before it became a success receipt: `outputSchema` names whether the
// schema was checked, `level` echoes the requested validation level.
export const ValidationEvidenceSchema = z
  .strictObject({
    outputSchema: z.enum(['passed', 'skipped']),
    level: z.enum(['strict', 'lenient', 'none']).optional()
  })
  .readonly();
export type ValidationEvidence = z.infer<typeof ValidationEvidenceSchema>;

// Stable, key-sorted serialization so receipt bytes are reproducible regardless of
// object insertion order (used for hashing / equality / diffing across transports).
// Undefined-valued optional fields are omitted so the shared capability serializer
// (which rejects `undefined`) stays byte-stable across transports.

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
  requestId?: RequestId;
  idempotencyId?: IdempotencyKey;
  catalogRevision?: CatalogRevision;
  capabilityRevision?: CapabilityRevision;
  schemaRevision?: SchemaRevision;
  changes?: readonly string[];
  warnings?: readonly string[];
  timingMs?: number;
  validation?: ValidationEvidence;
  liveRevisions?: LiveStateRevisions;
  task?: TaskStatus;
  nextCalls?: readonly NextCall[];
}): Receipt {
  return {
    status: 'success',
    capabilityId: input.capabilityId,
    correlationId: input.correlationId,
    requestId: input.requestId,
    idempotencyId: input.idempotencyId,
    catalogRevision: input.catalogRevision,
    capabilityRevision: input.capabilityRevision,
    schemaRevision: input.schemaRevision,
    handles: boundArray(input.handles ?? []),
    changes: boundStrings(input.changes ?? []),
    warnings: boundStrings(input.warnings ?? []),
    timingMs: input.timingMs,
    validation: input.validation,
    liveRevisions: input.liveRevisions,
    task: input.task,
    nextCalls: boundArray(input.nextCalls ?? []),
    data: input.data
  };
}

// A typed error crosses the boundary with its free-text message and suggestions
// secret-masked and bounded, re-parsed through its own schema so the redaction
// can never produce an off-contract error.
function redactErrorMessage(error: SemanticError): SemanticError {
  const suggestions = 'suggestions' in error && Array.isArray(error.suggestions)
    ? { suggestions: boundStrings(error.suggestions) }
    : {};
  return SemanticErrorSchema.parse({ ...error, message: redactText(error.message), ...suggestions });
}

export function buildErrorReceipt(input: {
  capabilityId: CapabilityId;
  error: SemanticError;
  correlationId?: CorrelationId;
  requestId?: RequestId;
  idempotencyId?: IdempotencyKey;
  catalogRevision?: CatalogRevision;
  capabilityRevision?: CapabilityRevision;
  schemaRevision?: SchemaRevision;
  timingMs?: number;
  liveRevisions?: LiveStateRevisions;
  nextCalls?: readonly NextCall[];
}): Receipt {
  return {
    status: 'error',
    capabilityId: input.capabilityId,
    correlationId: input.correlationId,
    requestId: input.requestId,
    idempotencyId: input.idempotencyId,
    catalogRevision: input.catalogRevision,
    capabilityRevision: input.capabilityRevision,
    schemaRevision: input.schemaRevision,
    timingMs: input.timingMs,
    liveRevisions: input.liveRevisions,
    error: redactErrorMessage(input.error),
    nextCalls: boundArray(input.nextCalls ?? [])
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
      requestId: RequestIdSchema.optional(),
      idempotencyId: IdempotencyKeySchema.optional(),
      catalogRevision: CatalogRevisionSchema.optional(),
      capabilityRevision: CapabilityRevisionSchema.optional(),
      schemaRevision: SchemaRevisionSchema.optional(),
      handles: z.array(TypedHandleSchema).readonly(),
      changes: z.array(z.string()).readonly(),
      warnings: z.array(z.string()).readonly(),
      timingMs: z.number().optional(),
      validation: ValidationEvidenceSchema.optional(),
      liveRevisions: LiveStateRevisionsSchema.optional(),
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
      requestId: RequestIdSchema.optional(),
      idempotencyId: IdempotencyKeySchema.optional(),
      catalogRevision: CatalogRevisionSchema.optional(),
      capabilityRevision: CapabilityRevisionSchema.optional(),
      schemaRevision: SchemaRevisionSchema.optional(),
      timingMs: z.number().optional(),
      liveRevisions: LiveStateRevisionsSchema.optional(),
      error: SemanticErrorSchema,
      nextCalls: z.array(NextCallSchema).readonly()
    })
    .readonly()
]);
