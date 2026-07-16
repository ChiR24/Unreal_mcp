import { z } from 'zod';

import { CapabilityIdSchema } from '../identifiers.js';
import { CorrelationIdSchema } from './ids.js';
import { JsonValueSchema } from './property-assignment.js';

// Discriminated typed error algebra shared across transports. Each variant carries
// structured fields (category via `kind`, retryability, suggestions) so a client gets
// an actionable, typed error rather than a bare string. Distinct from the flat
// `ErrorType` string-union classifier used by the runtime response helpers.
//
// Leaf module (no value imports from envelope/handles/paths) so the receipt
// envelope and path/handle parsers can import it without an import cycle.

// Exact, strict, discriminated schema mirroring `SemanticError`. Unknown `kind`
// values, missing required fields, and unknown keys are all rejected so an invalid
// error cannot cross a transport boundary dressed as a typed one. Array fields use
// `.readonly()` so inferred types are ReadonlyArray (immutable at the type level),
// and each branch uses `.readonly()` so Zod v4 deep-freezes the parsed error
// (Object.isFrozen === true) - the readonly guarantee is runtime-real at the
// public boundary, not merely a disconnected type alias.
export const SemanticErrorSchema = z.discriminatedUnion('kind', [
  z
    .strictObject({
      kind: z.literal('validation'),
      code: z.literal('VALIDATION_ERROR'),
      message: z.string(),
      pointer: z.string().optional(),
      suggestions: z.array(z.string()).readonly().optional()
    })
    .readonly(),
  z
    .strictObject({
      kind: z.literal('path'),
      code: z.literal('INVALID_PATH_ROOT').or(z.literal('PATH_TRAVERSAL')),
      message: z.string(),
      input: z.string(),
      suggestions: z.array(z.string()).readonly().optional()
    })
    .readonly(),
  z
    .strictObject({
      kind: z.literal('option'),
      code: z.literal('UNSUPPORTED_OPTION'),
      option: z.string(),
      supported: z.array(z.string()).readonly(),
      message: z.string(),
      suggestions: z.array(z.string()).readonly().optional()
    })
    .readonly(),
  z
    .strictObject({
      kind: z.literal('handle'),
      code: z.literal('HANDLE_KIND_MISMATCH'),
      expected: z.string(),
      received: z.string(),
      message: z.string(),
      suggestions: z.array(z.string()).readonly().optional()
    })
    .readonly(),
  z
    .strictObject({
      kind: z.literal('range'),
      code: z.literal('OUT_OF_RANGE').or(z.literal('WRONG_UNIT')),
      field: z.string(),
      message: z.string(),
      suggestions: z.array(z.string()).readonly().optional()
    })
    .readonly(),
  z
    .strictObject({
      kind: z.literal('timeout'),
      code: z.literal('TIMEOUT_EXCEEDED'),
      message: z.string(),
      boundMs: z.number(),
      suggestions: z.array(z.string()).readonly().optional()
    })
    .readonly(),
  z
    .strictObject({
      kind: z.literal('execution'),
      code: z
        .literal('EXECUTION_ERROR')
        .or(z.literal('CONNECTION_ERROR'))
        .or(z.literal('UNREAL_ENGINE_ERROR')),
      message: z.string(),
      retryable: z.boolean(),
      correlationId: CorrelationIdSchema.optional(),
      suggestions: z.array(z.string()).readonly().optional()
    })
    .readonly(),
  z
    .strictObject({
      kind: z.literal('unknown'),
      code: z.literal('UNKNOWN_ERROR'),
      message: z.string()
    })
    .readonly()
]);

// `SemanticErrorCode` is derived from `SemanticErrorSchema` so the code union can
// never drift from its Zod contract (single source of truth).
export type SemanticErrorCode = z.infer<typeof SemanticErrorSchema>['code'];

// `SemanticError` is derived directly from `SemanticErrorSchema` so the typed
// error algebra can never drift from its Zod contract (single source of truth).
export type SemanticError = z.infer<typeof SemanticErrorSchema>;

export class SemanticBoundaryError extends Error {
  readonly semanticError: SemanticError;
  constructor(semanticError: SemanticError) {
    super(semanticError.message);
    this.name = 'SemanticBoundaryError';
    this.semanticError = semanticError;
  }
}

export function assertNever(x: never): never {
  throw new Error(`Unexpected semantic value: ${JSON.stringify(x)}`);
}

// `TaskStatus` is derived from `TaskStatusSchema`; `taskId` is non-empty and
// `progress` is bounded to 0..1 at runtime by the schema (these bounds are
// enforced by `TaskStatusSchema.parse`, not expressible as a static type).
export type TaskStatus = z.infer<typeof TaskStatusSchema>;

export const TaskStatusSchema = z
  .strictObject({
    taskId: z.string().min(1),
    state: z.enum(['queued', 'running', 'completed', 'failed', 'cancelled']),
    progress: z.number().min(0).max(1).optional()
  })
  .readonly();

// `NextCall` is derived from `NextCallSchema` so the wired next-call contract
// stays aligned with its Zod definition (single source of truth).
export type NextCall = z.infer<typeof NextCallSchema>;

export const NextCallSchema = z
  .strictObject({
    operation: z.enum(['search', 'describe', 'execute', 'configure']),
    capability: CapabilityIdSchema.optional(),
    params: JsonValueSchema.optional()
  })
  .readonly();
