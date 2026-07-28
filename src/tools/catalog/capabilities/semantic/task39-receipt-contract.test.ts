// Task 39 — strict typed contract for the correlated receipt and the one typed
// error algebra. Written failing-first: the revision schemas, the new error
// kinds, and the enriched receipt fields do not exist yet, so every assertion
// below is a clean RED against the current semantic layer. Builder inputs are
// routed through `unknown`-cast helpers so the RED lives at the runtime
// assertion boundary rather than at tsc (the new fields are not yet typed).

import { describe, expect, it } from 'vitest';

import { CapabilityIdSchema } from '../identifiers.js';

import {
  buildErrorReceipt,
  buildSuccessReceipt,
  ReceiptSchema,
  serializeReceipt,
  type Receipt
} from './envelope.js';
import { CorrelationIdSchema, IdempotencyKeySchema } from './ids.js';
import * as ids from './ids.js';
import { SemanticErrorSchema } from './errors.js';
import { maskSecrets, maskSecretsDeep } from './receipt-redaction.js';

const CAP = CapabilityIdSchema.parse('asset.import');
const CORRELATION = CorrelationIdSchema.parse('gw-42');
const IDEMPOTENCY = IdempotencyKeySchema.parse('idem-1');
const HEX16 = '740752bc2cdcb7b9';
const HEX64 = 'a'.repeat(64);
const SCHEMA_HEX64 = 'b'.repeat(64);
const CATALOG_REVISION = ids.CatalogRevisionSchema.parse(HEX16);
const CAPABILITY_REVISION = ids.CapabilityRevisionSchema.parse(HEX64);
const SCHEMA_REVISION = ids.SchemaRevisionSchema.parse(SCHEMA_HEX64);
const REQUEST_ONE = ids.RequestIdSchema.parse('str:req-1');
const REQUEST_TWO = ids.RequestIdSchema.parse('str:req-2');
const LIVE_REVISIONS = { selection: 2, level: 3, assetRegistry: 4, package: 5 } as const;

const buildSuccess = (input: Parameters<typeof buildSuccessReceipt>[0]): Receipt =>
  buildSuccessReceipt(input);
const buildError = (input: Parameters<typeof buildErrorReceipt>[0]): Receipt =>
  buildErrorReceipt(input);

function schemaOf(name: string): { safeParse: (v: unknown) => { success: boolean } } {
  return (ids as Record<string, unknown>)[name] as { safeParse: (v: unknown) => { success: boolean } };
}

describe('task39 ids: revision fields are bounded, distinct, hex strings', () => {
  it('accepts the live catalog digest string on CatalogRevisionSchema (no longer a number brand)', () => {
    expect(ids.CatalogRevisionSchema.safeParse(HEX16).success).toBe(true);
  });

  it('rejects a numeric catalog revision (the old number branding is gone)', () => {
    expect(ids.CatalogRevisionSchema.safeParse(123).success).toBe(false);
  });

  it('mints a bounded CapabilityRevisionSchema over the record content hash', () => {
    const schema = schemaOf('CapabilityRevisionSchema');
    expect(schema).toBeDefined();
    expect(schema.safeParse(HEX64).success).toBe(true);
    expect(schema.safeParse('not-hex').success).toBe(false);
    expect(schema.safeParse('a'.repeat(200)).success).toBe(false);
  });

  it('mints a bounded SchemaRevisionSchema over the record schema hash', () => {
    const schema = schemaOf('SchemaRevisionSchema');
    expect(schema).toBeDefined();
    expect(schema.safeParse(SCHEMA_HEX64).success).toBe(true);
    expect(schema.safeParse('').success).toBe(false);
  });

  it('mints a bounded RequestIdSchema for the external MCP request id', () => {
    const schema = schemaOf('RequestIdSchema');
    expect(schema).toBeDefined();
    expect(schema.safeParse('str:abc-1').success).toBe(true);
    expect(schema.safeParse('').success).toBe(false);
    expect(schema.safeParse('x'.repeat(4000)).success).toBe(false);
  });
});

describe('task39 errors: the additive discriminated algebra covers every plan class', () => {
  const validErrorFixtures: readonly Record<string, unknown>[] = [
    { kind: 'validation', code: 'VALIDATION_ERROR', message: 'v' },
    { kind: 'capability', code: 'CAPABILITY_DISABLED', message: 'disabled', retryable: false },
    { kind: 'capability', code: 'CAPABILITY_UNAVAILABLE', message: 'gone', retryable: false },
    { kind: 'consent', code: 'CONSENT_REQUIRED', message: 'consent', scope: 'destructive' },
    { kind: 'staleState', code: 'STALE_STATE', message: 'stale', currentRevision: HEX16, expectedRevision: 'deadbeef' },
    { kind: 'conflict', code: 'STATE_CONFLICT', message: 'conflict' },
    { kind: 'cancellation', code: 'OPERATION_CANCELLED', message: 'cancelled' },
    { kind: 'dispatch', code: 'NOT_CONNECTED', message: 'not connected', retryable: true },
    { kind: 'dispatch', code: 'DISPATCH_ERROR', message: 'routing', retryable: false },
    { kind: 'output', code: 'OUTPUT_SCHEMA_VIOLATION', message: 'bad output', pointer: '/x' },
    { kind: 'output', code: 'RESULT_TOO_LARGE', message: 'too big', resultChars: 150_000 }
  ];

  const legacyErrorFixtures: readonly Record<string, unknown>[] = [
    { kind: 'path', code: 'PATH_TRAVERSAL', message: 't', input: '/x/..' },
    { kind: 'option', code: 'UNSUPPORTED_OPTION', option: 'o', supported: ['timeoutMs'], message: 'no' },
    { kind: 'handle', code: 'HANDLE_KIND_MISMATCH', expected: 'actor', received: 'component', message: 'k' },
    { kind: 'range', code: 'OUT_OF_RANGE', field: 'r', message: 'oob' },
    { kind: 'timeout', code: 'TIMEOUT_EXCEEDED', message: 'to', boundMs: 1000 },
    { kind: 'execution', code: 'EXECUTION_ERROR', message: 'x', retryable: false },
    { kind: 'unknown', code: 'UNKNOWN_ERROR', message: '?' }
  ];

  it('parses a valid instance of every plan error class', () => {
    for (const error of validErrorFixtures) {
      expect(SemanticErrorSchema.safeParse(error).success, `${String(error.kind)}/${String(error.code)}`).toBe(true);
    }
  });

  it('still parses every preserved legacy variant', () => {
    for (const error of legacyErrorFixtures) {
      expect(SemanticErrorSchema.safeParse(error).success, String(error.kind)).toBe(true);
    }
  });

  it('rejects a malformed instance of the new kinds', () => {
    expect(SemanticErrorSchema.safeParse({ kind: 'output', code: 'WRONG', message: 'x' }).success).toBe(false);
    expect(SemanticErrorSchema.safeParse({ kind: 'capability', code: 'CAPABILITY_DISABLED', message: 'x' }).success).toBe(false);
    expect(SemanticErrorSchema.safeParse({ kind: 'staleState', code: 'STALE_STATE' }).success).toBe(false);
    expect(SemanticErrorSchema.safeParse({ kind: 'bogus', code: 'X', message: 'x' }).success).toBe(false);
  });
});

describe('task39 envelope: the enriched receipt carries correlation, ids and revisions', () => {
  it('round-trips a fully enriched success receipt through ReceiptSchema', () => {
    const receipt = buildSuccess({
      capabilityId: CAP,
      data: { assetPath: '/Game/A' },
      correlationId: CORRELATION,
      requestId: REQUEST_ONE,
      idempotencyId: IDEMPOTENCY,
      catalogRevision: CATALOG_REVISION,
      capabilityRevision: CAPABILITY_REVISION,
      schemaRevision: SCHEMA_REVISION,
      timingMs: 12,
      validation: { outputSchema: 'passed', level: 'strict' }
    });

    expect(ReceiptSchema.safeParse(receipt).success).toBe(true);
    const view = receipt as Record<string, unknown>;
    expect(view.correlationId).toBe(CORRELATION);
    expect(view.requestId).toBe('str:req-1');
    expect(view.catalogRevision).toBe(HEX16);
    expect(view.capabilityRevision).toBe(HEX64);
    expect(view.schemaRevision).toBe(SCHEMA_HEX64);
    expect(view.timingMs).toBe(12);
    expect(view.validation).toEqual({ outputSchema: 'passed', level: 'strict' });
  });

  it('carries correlation, request id and revisions on an error receipt', () => {
    const receipt = buildError({
      capabilityId: CAP,
      error: { kind: 'output', code: 'OUTPUT_SCHEMA_VIOLATION', message: 'bad', pointer: '/x' },
      correlationId: CORRELATION,
      requestId: REQUEST_TWO,
      catalogRevision: CATALOG_REVISION,
      capabilityRevision: CAPABILITY_REVISION,
      schemaRevision: SCHEMA_REVISION,
      timingMs: 3
    });

    expect(ReceiptSchema.safeParse(receipt).success).toBe(true);
    const view = receipt as Record<string, unknown>;
    expect(view.requestId).toBe('str:req-2');
    expect(view.capabilityRevision).toBe(HEX64);
    expect(view.schemaRevision).toBe(SCHEMA_HEX64);
  });

  it('carries the same strict live revision snapshot on success and error receipts', () => {
    const success = buildSuccess({ capabilityId: CAP, data: {}, liveRevisions: LIVE_REVISIONS });
    const error = buildError({
      capabilityId: CAP,
      error: { kind: 'execution', code: 'UNREAL_ENGINE_ERROR', message: 'failed', retryable: false },
      liveRevisions: LIVE_REVISIONS
    });

    expect(ReceiptSchema.safeParse(success).success).toBe(true);
    expect(ReceiptSchema.safeParse(error).success).toBe(true);
    expect((success as Record<string, unknown>).liveRevisions).toEqual(LIVE_REVISIONS);
    expect((error as Record<string, unknown>).liveRevisions).toEqual(LIVE_REVISIONS);
  });

  it('rejects receipt snapshots that omit or add a live revision key', () => {
    const success = buildSuccess({ capabilityId: CAP, data: {} });
    expect(ReceiptSchema.safeParse({
      ...success,
      liveRevisions: { selection: 2, level: 3, assetRegistry: 4 }
    }).success).toBe(false);
    expect(ReceiptSchema.safeParse({
      ...success,
      liveRevisions: { ...LIVE_REVISIONS, futureState: 6 }
    }).success).toBe(false);
  });
});

describe('task39 envelope: adversarial bounds and secret redaction', () => {
  it('bounds an oversized changes array on the receipt', () => {
    const receipt = buildSuccess({
      capabilityId: CAP,
      data: {},
      changes: Array.from({ length: 5000 }, (_v, i) => `/Game/Actor_${i}`)
    });
    if (receipt.status !== 'success') throw new Error('expected success');
    expect(receipt.changes.length).toBeLessThanOrEqual(200);
  });

  it('redacts a secret-looking token in a warning instead of leaking it', () => {
    const receipt = buildSuccess({
      capabilityId: CAP,
      data: {},
      warnings: ['auth token=abcdef0123456789abcdef0123456789 was rotated']
    });
    const serialized = serializeReceipt(receipt);
    expect(serialized).not.toContain('abcdef0123456789abcdef0123456789');
    expect(serialized).toContain('[REDACTED]');
  });

  it('truncates an over-long warning string', () => {
    const receipt = buildSuccess({
      capabilityId: CAP,
      data: {},
      warnings: ['w'.repeat(10_000)]
    });
    if (receipt.status !== 'success') throw new Error('expected success');
    expect(receipt.warnings[0]?.length ?? 0).toBeLessThanOrEqual(2048);
  });
});

describe('task39 redaction: secret masking is identical for every credential shape (TS/native parity)', () => {
  const SECRET = 'sk-supersecret-abcdef0123456789';

  it('masks the TOKEN in an "Authorization: Bearer <token>" header, not just the scheme word', () => {
    const masked = maskSecrets(`Authorization: Bearer ${SECRET}`);
    expect(masked).not.toContain(SECRET);
    expect(masked).toContain('[REDACTED]');
  });

  it('masks a bare "Bearer <token>" outside an assignment', () => {
    const masked = maskSecrets(`Bearer ${SECRET}`);
    expect(masked).not.toContain(SECRET);
    expect(masked).toContain('Bearer [REDACTED]');
  });

  it('masks a JSON-like quoted assignment ("token":"<value>")', () => {
    const masked = maskSecrets(`{"token":"${SECRET}","keep":"public"}`);
    expect(masked).not.toContain(SECRET);
    expect(masked).toContain('[REDACTED]');
    expect(masked).toContain('public');
  });

  it('masks a JSON-like nested authorization Bearer value', () => {
    const masked = maskSecrets(`{"authorization":"Bearer ${SECRET}"}`);
    expect(masked).not.toContain(SECRET);
    expect(masked).toContain('[REDACTED]');
  });

  it('masks bare keyword assignments (token=, secret:, password=, api_key:)', () => {
    for (const line of [`token=${SECRET}`, `secret: ${SECRET}`, `password=${SECRET}`, `api_key: ${SECRET}`]) {
      const masked = maskSecrets(line);
      expect(masked, line).not.toContain(SECRET);
      expect(masked, line).toContain('[REDACTED]');
    }
  });

  it('deep-masks a secret in a nested object VALUE under a secret-shaped string leaf while preserving legitimate data', () => {
    const masked = maskSecretsDeep({
      outer: { note: `Authorization: Bearer ${SECRET}` },
      list: [`token=${SECRET}`],
      keep: '/Game/Meshes/SM_Rock'
    }) as Record<string, unknown>;
    const serialized = JSON.stringify(masked);
    expect(serialized).not.toContain(SECRET);
    expect(serialized).toContain('[REDACTED]');
    expect(serialized).toContain('/Game/Meshes/SM_Rock');
  });

  it('leaves ordinary prose untouched (no over-masking)', () => {
    const prose = 'The token bucket refilled and the password field is required.';
    expect(maskSecrets(prose)).toBe(prose);
  });
});
