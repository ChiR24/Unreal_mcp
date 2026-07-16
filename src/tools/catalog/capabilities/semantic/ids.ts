import { z } from 'zod';

// Leaf module: branded correlation / idempotency / catalog-revision identifiers shared by
// the semantic execution options and the receipt envelope. Kept dependency-free so the
// error algebra (errors.ts) and the receipt envelope (envelope.ts) can both import
// these without forming an import cycle.

export const IdempotencyKeySchema = z
  .string()
  .min(1)
  .max(128)
  .brand<'IdempotencyKey'>();
export type IdempotencyKey = z.infer<typeof IdempotencyKeySchema>;

export const CorrelationIdSchema = z
  .string()
  .min(1)
  .max(128)
  .brand<'CorrelationId'>();
export type CorrelationId = z.infer<typeof CorrelationIdSchema>;

export const CatalogRevisionSchema = z
  .number()
  .int()
  .nonnegative()
  .brand<'CatalogRevision'>();
export type CatalogRevision = z.infer<typeof CatalogRevisionSchema>;
