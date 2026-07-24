// Per-request correlation and revision inputs for the canonical execute receipt.
//
// One gateway correlation id is minted per request and threaded here so the same
// value lands on both the success and the error receipt across the dispatch
// await. The external MCP request id is read from the async-local request
// context (bounded, canonicalized `num:`/`str:` id — never the internal
// automation id or a token) and echoed only when truthfully present. The three
// revision digests come straight from live runtime sources: the global catalog
// digest and the resolved record's content/schema hashes.

import type { CapabilityRecord } from '../../tools/catalog/capabilities/model.js';
import { getMcpRequestContext } from '../../automation/request-context.js';
import type {
  CapabilityRevision,
  CatalogRevision,
  CorrelationId,
  IdempotencyKey,
  RequestId,
  SchemaRevision
} from '../../tools/catalog/capabilities/semantic/ids.js';
import {
  CapabilityRevisionSchema,
  CatalogRevisionSchema,
  IdempotencyKeySchema,
  RequestIdSchema,
  SchemaRevisionSchema
} from '../../tools/catalog/capabilities/semantic/ids.js';
import { catalogRevision } from './gateway-capability-index.js';

export type GatewayReceiptContext = {
  readonly correlationId: CorrelationId;
  readonly requestId?: RequestId;
  readonly idempotencyId?: IdempotencyKey;
  readonly startedAt: number;
};

export function buildReceiptContext(
  correlationId: CorrelationId,
  options: Record<string, unknown> | undefined
): GatewayReceiptContext {
  const active = getMcpRequestContext();
  const request = active?.requestId === undefined ? undefined : RequestIdSchema.safeParse(active.requestId);
  const rawIdempotency = options?.idempotencyKey;
  const idempotency = typeof rawIdempotency === 'string' ? IdempotencyKeySchema.safeParse(rawIdempotency) : undefined;
  return {
    correlationId,
    ...(request?.success ? { requestId: request.data } : {}),
    ...(idempotency?.success ? { idempotencyId: idempotency.data } : {}),
    startedAt: Date.now()
  };
}

export function correlationFields(context: GatewayReceiptContext): {
  correlationId: CorrelationId;
  requestId?: RequestId;
  idempotencyId?: IdempotencyKey;
} {
  return {
    correlationId: context.correlationId,
    ...(context.requestId === undefined ? {} : { requestId: context.requestId }),
    ...(context.idempotencyId === undefined ? {} : { idempotencyId: context.idempotencyId })
  };
}

export function revisionFields(record: CapabilityRecord): {
  catalogRevision?: CatalogRevision;
  capabilityRevision?: CapabilityRevision;
  schemaRevision?: SchemaRevision;
} {
  const catalog = CatalogRevisionSchema.safeParse(catalogRevision());
  const capability = CapabilityRevisionSchema.safeParse(record.hashes.content);
  const schema = SchemaRevisionSchema.safeParse(record.hashes.schema);
  return {
    ...(catalog.success ? { catalogRevision: catalog.data } : {}),
    ...(capability.success ? { capabilityRevision: capability.data } : {}),
    ...(schema.success ? { schemaRevision: schema.data } : {})
  };
}

export function elapsedMs(context: GatewayReceiptContext): number {
  return Math.max(0, Date.now() - context.startedAt);
}
