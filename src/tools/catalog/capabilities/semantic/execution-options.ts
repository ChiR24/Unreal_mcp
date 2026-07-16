import { z } from 'zod';

import { isRecord } from '../../../../utils/validation/type-guards.js';

import { SemanticBoundaryError, type SemanticError } from './errors.js';

import {
  CatalogRevisionSchema,
  IdempotencyKeySchema
} from './ids.js';

import { type SavePolicy, SavePolicySchema } from './save-policy.js';

// Cross-cutting execution controls live in a typed gateway `options` envelope, never
// inside action `params`. Each capability declares which subset of these keys it
// supports; anything else (including the wrong-unit `durationSeconds`) is rejected.

export const EXECUTION_OPTION_KEYS = [
  'idempotencyKey',
  'expectedCatalogRevision',
  'preview',
  'savePolicy',
  'timeoutMs',
  'validationLevel',
  'taskPreference'
] as const;

export type ExecutionOptionKey = (typeof EXECUTION_OPTION_KEYS)[number];

export const ValidationLevelSchema = z.enum(['strict', 'lenient', 'none']);
export type ValidationLevel = z.infer<typeof ValidationLevelSchema>;

export const TaskPreferenceSchema = z.enum(['foreground', 'background', 'queued']);
export type TaskPreference = z.infer<typeof TaskPreferenceSchema>;

const MAX_TIMEOUT_MS = 600_000;

const ExecutionOptionsShape = {
  idempotencyKey: IdempotencyKeySchema.optional(),
  expectedCatalogRevision: CatalogRevisionSchema.optional(),
  preview: z.boolean().optional(),
  savePolicy: SavePolicySchema.optional(),
  timeoutMs: z.number().int().positive().max(MAX_TIMEOUT_MS).optional(),
  validationLevel: ValidationLevelSchema.optional(),
  taskPreference: TaskPreferenceSchema.optional()
} satisfies Record<ExecutionOptionKey, z.ZodType>;

export const ExecutionOptionsSchema = z.strictObject(ExecutionOptionsShape).readonly();
export type ExecutionOptions = z.infer<typeof ExecutionOptionsSchema>;

export function parseExecutionOptions(
  raw: unknown,
  supported: readonly ExecutionOptionKey[]
): ExecutionOptions {
  if (raw === undefined || raw === null) return {};
  if (!isRecord(raw)) {
    throw new SemanticBoundaryError({
      kind: 'validation',
      code: 'VALIDATION_ERROR',
      message: 'Execution options must be an object'
    });
  }
  const supportedSet = new Set<string>(supported);
  for (const key of Object.keys(raw)) {
    if (!supportedSet.has(key)) {
      const semanticError: SemanticError = {
        kind: 'option',
        code: 'UNSUPPORTED_OPTION',
        option: key,
        supported: [...supported],
        message: `Unsupported execution option '${key}'. Supported: [${supported.join(', ')}]`
      };
      throw new SemanticBoundaryError(semanticError);
    }
  }
  return ExecutionOptionsSchema.parse(raw);
}

export function rejectGatewayControlsInParams(
  params: Record<string, unknown>,
  controlKeys: readonly string[]
): void {
  const controlSet = new Set<string>(controlKeys);
  for (const key of Object.keys(params)) {
    if (controlSet.has(key)) {
      throw new SemanticBoundaryError({
        kind: 'option',
        code: 'UNSUPPORTED_OPTION',
        option: key,
        supported: [],
        message: `Gateway control '${key}' must not appear in action params`
      });
    }
  }
}

export type { SavePolicy };
