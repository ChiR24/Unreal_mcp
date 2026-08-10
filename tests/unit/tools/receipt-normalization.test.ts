// Task 39 — strict cross-transport receipt DEEP comparator + compiled parity proof.
//
// This is the CI-side mirror of the live-capture comparator in
// scripts/qa/task-39-parity-normalize.mjs. Both encode the same contract: every
// stable receipt field is compared (status, capabilityId, the typed error
// kind/code/pointer/revisions, handles, changes, warnings, nextCalls, validation,
// the three revision VALUES, requestId, idempotencyId and the redacted data
// payload); only genuinely volatile values are excluded — the correlation id
// VALUE, timingMs, error.message wording, and data.cursor/nextCursor. The TS
// receipt is built through the REAL builders; the native receipt is the exact
// shape McpBuildCanonicalReceipt emits (mirrored here from the C++ source).
// Negative injections prove the comparator is not a tautology.

import { describe, expect, it } from 'vitest';

import { CapabilityIdSchema } from '../../../src/tools/catalog/capabilities/identifiers.js';
import {
  buildErrorReceipt,
  buildSuccessReceipt,
  serializeReceipt,
  type Receipt
} from '../../../src/tools/catalog/capabilities/semantic/envelope.js';
import {
  CapabilityRevisionSchema,
  CatalogRevisionSchema,
  CorrelationIdSchema,
  IdempotencyKeySchema,
  RequestIdSchema,
  SchemaRevisionSchema
} from '../../../src/tools/catalog/capabilities/semantic/ids.js';

const CAP = CapabilityIdSchema.parse('asset.list');
const CATALOG = '740752bc2cdcb7b9';
const CAPREV = 'c'.repeat(64);
const SCHEMAREV = 'd'.repeat(64);

type Json = Record<string, unknown>;

const VOLATILE_RECEIPT = new Set(['correlationId', 'timingMs']);
const VOLATILE_ERROR = new Set(['message']);
const VOLATILE_DATA = new Set(['cursor', 'nextCursor']);

function stripData(data: unknown): unknown {
  if (Array.isArray(data)) return data.map(stripData);
  if (data !== null && typeof data === 'object') {
    const out: Json = {};
    for (const [k, v] of Object.entries(data)) {
      if (!VOLATILE_DATA.has(k)) out[k] = stripData(v);
    }
    return out;
  }
  return data;
}

function stripVolatile(receipt: Json): Json {
  const out: Json = {};
  for (const [k, v] of Object.entries(receipt)) {
    if (VOLATILE_RECEIPT.has(k)) continue;
    if (k === 'error' && v !== null && typeof v === 'object' && !Array.isArray(v)) {
      const err: Json = {};
      for (const [ek, ev] of Object.entries(v)) {
        if (!VOLATILE_ERROR.has(ek)) err[ek] = ev;
      }
      out.error = err;
    } else if (k === 'data') {
      out.data = stripData(v);
    } else {
      out[k] = v;
    }
  }
  return out;
}

function deepEqual(a: unknown, b: unknown): boolean {
  if (a === b) return true;
  if (Array.isArray(a) && Array.isArray(b)) {
    return a.length === b.length && a.every((x, i) => deepEqual(x, b[i]));
  }
  if (a !== null && b !== null && typeof a === 'object' && typeof b === 'object') {
    const ak = Object.keys(a);
    const bk = Object.keys(b);
    if (ak.length !== bk.length) return false;
    return ak.every((k) => deepEqual((a as Json)[k], (b as Json)[k]));
  }
  return false;
}

function stableParity(ts: Json, native: Json): boolean {
  return deepEqual(stripVolatile(ts), stripVolatile(native));
}

const wire = (receipt: Receipt): Json => JSON.parse(serializeReceipt(receipt)) as Json;

function tsSuccess(overrides: Partial<Parameters<typeof buildSuccessReceipt>[0]> = {}): Json {
  return wire(buildSuccessReceipt({
    capabilityId: CAP,
    data: { folders: ['/Game/Collections'], cursor: 'ts-cursor' },
    correlationId: CorrelationIdSchema.parse('gw-1'),
    requestId: RequestIdSchema.parse('num:100'),
    idempotencyId: IdempotencyKeySchema.parse('k-1'),
    catalogRevision: CatalogRevisionSchema.parse(CATALOG),
    capabilityRevision: CapabilityRevisionSchema.parse(CAPREV),
    schemaRevision: SchemaRevisionSchema.parse(SCHEMAREV),
    timingMs: 5,
    validation: { outputSchema: 'passed' },
    ...overrides
  }));
}

// Exactly what McpBuildCanonicalReceipt(bSuccess=true) emits for the same
// scenario: native GUID correlation, distinct timing/cursor, identical stable fields.
function nativeSuccess(overrides: Json = {}): Json {
  return {
    status: 'success',
    capabilityId: 'asset.list',
    correlationId: '232255C5-DA17-46EC-9530-07803C6130A5',
    requestId: 'num:100',
    idempotencyId: 'k-1',
    catalogRevision: CATALOG,
    capabilityRevision: CAPREV,
    schemaRevision: SCHEMAREV,
    timingMs: 7,
    nextCalls: [],
    handles: [],
    changes: [],
    warnings: [],
    validation: { outputSchema: 'passed' },
    data: { folders: ['/Game/Collections'], cursor: 'native-cursor' },
    ...overrides
  };
}

describe('task39 deep comparator: the TS receipt and the native builder shape are stable-field identical', () => {
  it('matches a success receipt whose only differences are correlation, timing and the data cursor', () => {
    expect(stableParity(tsSuccess(), nativeSuccess())).toBe(true);
  });

  it('matches a typed error receipt on kind/code/revisions while message wording stays volatile', () => {
    const ts = wire(buildErrorReceipt({
      capabilityId: CAP,
      error: { kind: 'staleState', code: 'STALE_STATE', message: 'stale (ts wording)', currentRevision: CATALOG, expectedRevision: 'deadbeef' },
      correlationId: CorrelationIdSchema.parse('gw-9'),
      requestId: RequestIdSchema.parse('num:200'),
      catalogRevision: CatalogRevisionSchema.parse(CATALOG),
      capabilityRevision: CapabilityRevisionSchema.parse(CAPREV),
      schemaRevision: SchemaRevisionSchema.parse(SCHEMAREV),
      timingMs: 2
    }));
    const native: Json = {
      status: 'error', capabilityId: 'asset.list', correlationId: 'B1E2-native-guid',
      requestId: 'num:200', catalogRevision: CATALOG, capabilityRevision: CAPREV, schemaRevision: SCHEMAREV,
      timingMs: 9, nextCalls: [],
      error: { kind: 'staleState', code: 'STALE_STATE', message: 'stale (native wording)', currentRevision: CATALOG, expectedRevision: 'deadbeef' }
    };
    expect(stableParity(ts, native)).toBe(true);
  });

  it('matches a malformed-pin validation error (INVALID_OPTIONS pointer) across transports', () => {
    const ts = wire(buildErrorReceipt({
      capabilityId: CAP,
      error: { kind: 'validation', code: 'VALIDATION_ERROR', message: 'must be hex (ts)', pointer: '/options/expectedCatalogRevision' },
      correlationId: CorrelationIdSchema.parse('gw-3'),
      catalogRevision: CatalogRevisionSchema.parse(CATALOG),
      capabilityRevision: CapabilityRevisionSchema.parse(CAPREV),
      schemaRevision: SchemaRevisionSchema.parse(SCHEMAREV)
    }));
    const native: Json = {
      status: 'error', capabilityId: 'asset.list', correlationId: 'native-guid',
      catalogRevision: CATALOG, capabilityRevision: CAPREV, schemaRevision: SCHEMAREV, nextCalls: [],
      error: { kind: 'validation', code: 'VALIDATION_ERROR', message: 'must be hex (native)', pointer: '/options/expectedCatalogRevision' }
    };
    expect(stableParity(ts, native)).toBe(true);
  });
});

describe('task39 deep comparator: stable-field divergences are DETECTED (not a tautology)', () => {
  it('flags a deep error CODE change', () => {
    const ts = wire(buildErrorReceipt({ capabilityId: CAP, error: { kind: 'staleState', code: 'STALE_STATE', message: 'm', currentRevision: CATALOG, expectedRevision: 'deadbeef' } }));
    const native: Json = { status: 'error', capabilityId: 'asset.list', nextCalls: [], error: { kind: 'staleState', code: 'VALIDATION_ERROR', message: 'm', currentRevision: CATALOG, expectedRevision: 'deadbeef' } };
    expect(stableParity(ts, native)).toBe(false);
  });

  it('flags an array CONTENT change (changes)', () => {
    expect(stableParity(tsSuccess({ changes: ['/Game/A'] }), nativeSuccess({ changes: ['/Game/B'] }))).toBe(false);
  });

  it('flags an array LENGTH change (handles)', () => {
    expect(stableParity(tsSuccess(), nativeSuccess({ handles: [{ kind: 'asset', path: '/Game/Extra' }] }))).toBe(false);
  });

  it('flags a redacted-data difference (a secret unmasked on one side)', () => {
    expect(stableParity(tsSuccess(), nativeSuccess({ data: { folders: ['token=leaked-secret'] } }))).toBe(false);
  });

  it('flags a revision VALUE change', () => {
    expect(stableParity(tsSuccess(), nativeSuccess({ catalogRevision: 'ffffffffffffffff' }))).toBe(false);
  });

  it('flags a requestId change', () => {
    expect(stableParity(tsSuccess(), nativeSuccess({ requestId: 'num:999' }))).toBe(false);
  });

  it('flags the pre-remediation flat/leaner native shape (missing ids/arrays/validation)', () => {
    const legacyNative: Json = {
      status: 'success', capabilityId: 'asset.list', correlationId: 'guid',
      catalogRevision: CATALOG, capabilityRevision: CAPREV, schemaRevision: SCHEMAREV,
      data: { folders: ['/Game/Collections'] }
    };
    expect(stableParity(tsSuccess(), legacyNative)).toBe(false);
  });
});
