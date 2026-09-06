import { z } from 'zod';

import { isRecord } from '../../../../utils/validation/type-guards.js';

import { SemanticBoundaryError, type SemanticError } from './errors.js';

import {
  CatalogRevisionSchema,
  IdempotencyKeySchema
} from './ids.js';

import { SavePolicySchema } from './save-policy.js';

// Cross-cutting execution controls live in a typed gateway `options` envelope, never
// inside action `params`. Each capability declares which subset of these keys it
// supports; anything else (including the wrong-unit `durationSeconds`) is rejected.

export const EXECUTION_OPTION_KEYS = [
  'idempotencyKey',
  'expectedCatalogRevision',
  'expectedRevisions',
  'preview',
  'savePolicy',
  'timeoutMs',
  'validationLevel',
  'taskPreference'
] as const;

export type ExecutionOptionKey = (typeof EXECUTION_OPTION_KEYS)[number];

export const ValidationLevelSchema = z.enum(['strict', 'lenient', 'none']);

export const TaskPreferenceSchema = z.enum(['foreground', 'background', 'queued']);

const MAX_TIMEOUT_MS = 600_000;

// Live editor state a client can pin a precondition against. Spelled exactly as
// FMcpLiveStateRevisions::KeyFor in the plugin, because the pin travels over the
// wire to the game-thread gate that enforces it.
export const LIVE_STATE_REVISION_KEYS = [
  'selection',
  'level',
  'assetRegistry',
  'package'
] as const;


// Every key is optional: an absent key is simply not pinned. Strict, so an
// unknown pin name is refused rather than silently ignored.
export const ExpectedRevisionsSchema = z.strictObject({
  selection: z.number().int().min(1).optional(),
  level: z.number().int().min(1).optional(),
  assetRegistry: z.number().int().min(1).optional(),
  package: z.number().int().min(1).optional()
}).readonly();

export type ExpectedRevisions = z.infer<typeof ExpectedRevisionsSchema>;

const ExecutionOptionsShape = {
  idempotencyKey: IdempotencyKeySchema.optional(),
  expectedCatalogRevision: CatalogRevisionSchema.optional(),
  expectedRevisions: ExpectedRevisionsSchema.optional(),
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

