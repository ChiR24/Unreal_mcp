import { z } from 'zod';

import {
  CatalogRevisionSchema,
  IdempotencyKeySchema
} from './ids.js';

// Cross-cutting execution controls live in a typed gateway `options` envelope, never
// inside action `params`. Each capability declares which subset of these keys it
// supports; anything else (including the wrong-unit `durationSeconds`) is rejected.

export const EXECUTION_OPTION_KEYS = [
  'idempotencyKey',
  'expectedCatalogRevision',
  'expectedRevisions',
  'timeoutMs'
] as const;

export type ExecutionOptionKey = (typeof EXECUTION_OPTION_KEYS)[number];

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
  timeoutMs: z.number().int().positive().max(MAX_TIMEOUT_MS).optional()
} satisfies Record<ExecutionOptionKey, z.ZodType>;

export const ExecutionOptionsSchema = z.strictObject(ExecutionOptionsShape).readonly();
