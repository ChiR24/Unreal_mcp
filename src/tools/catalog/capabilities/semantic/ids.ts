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

// Revision digests are lowercase hex strings produced by the canonical registry
// hashing: the catalog digest is a folded 16-hex string and the per-record
// content/schema digests are sha256 (64 hex). All three are bounded, distinct
// branded strings so a receipt can never mix a catalog digest with a capability
// or schema digest, and the old number branding (inconsistent with the string
// runtime digest) is gone.
const HEX_REVISION = /^[0-9a-f]{1,64}$/;

export const CatalogRevisionSchema = z
  .string()
  .min(1)
  .max(64)
  .regex(HEX_REVISION)
  .brand<'CatalogRevision'>();
export type CatalogRevision = z.infer<typeof CatalogRevisionSchema>;

export const CapabilityRevisionSchema = z
  .string()
  .min(1)
  .max(64)
  .regex(HEX_REVISION)
  .brand<'CapabilityRevision'>();
export type CapabilityRevision = z.infer<typeof CapabilityRevisionSchema>;

export const SchemaRevisionSchema = z
  .string()
  .min(1)
  .max(64)
  .regex(HEX_REVISION)
  .brand<'SchemaRevision'>();
export type SchemaRevision = z.infer<typeof SchemaRevisionSchema>;

// The external MCP request id, already canonicalized to a collision-free string
// (`num:1` / `str:abc`) before it reaches the gateway. Bounded so an untrusted
// client id can never balloon the receipt; never carries the internal automation
// request id or any token.
export const RequestIdSchema = z
  .string()
  .min(1)
  .max(256)
  .brand<'RequestId'>();
export type RequestId = z.infer<typeof RequestIdSchema>;
