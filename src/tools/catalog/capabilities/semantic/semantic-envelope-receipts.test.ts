import { describe, expect, it } from 'vitest';

import { CapabilityIdSchema } from '../identifiers.js';

import {
  buildErrorReceipt,
  buildSuccessReceipt,
  ReceiptSchema,
  serializeReceipt
} from './envelope.js';
import {
  NextCallSchema,
  SemanticBoundaryError,
  type SemanticError,
  SemanticErrorSchema,
  TaskStatusSchema
} from './errors.js';
import { parseActorRef } from './handles.js';

const CAP = CapabilityIdSchema.parse('asset.import');

describe('receipt / result envelope', () => {
  it('serializes a receipt with stable key order regardless of input order', () => {
    const a = buildSuccessReceipt({ capabilityId: CAP, data: { b: 1, a: 2 } });
    const b = buildSuccessReceipt({ capabilityId: CAP, data: { a: 2, b: 1 } });
    expect(serializeReceipt(a)).toBe(serializeReceipt(b));
  });

  it('builds an error receipt for every error variant', () => {
    const variants: readonly SemanticError[] = [
      { kind: 'validation', code: 'VALIDATION_ERROR', message: 'bad', pointer: '/x' },
      { kind: 'path', code: 'PATH_TRAVERSAL', message: 'trav', input: '/Game/../x' },
      { kind: 'path', code: 'INVALID_PATH_ROOT', message: 'root', input: '/Foo' },
      {
        kind: 'option',
        code: 'UNSUPPORTED_OPTION',
        option: 'durationSeconds',
        supported: ['timeoutMs'],
        message: 'no'
      },
      {
        kind: 'handle',
        code: 'HANDLE_KIND_MISMATCH',
        expected: 'component',
        received: 'actor',
        message: 'kind'
      },
      { kind: 'range', code: 'OUT_OF_RANGE', field: 'r', message: 'oob' },
      { kind: 'range', code: 'WRONG_UNIT', field: 'r', message: 'unit' },
      { kind: 'timeout', code: 'TIMEOUT_EXCEEDED', message: 'to', boundMs: 600_000 },
      { kind: 'execution', code: 'EXECUTION_ERROR', message: 'exec', retryable: false },
      { kind: 'execution', code: 'CONNECTION_ERROR', message: 'conn', retryable: true },
      { kind: 'execution', code: 'UNREAL_ENGINE_ERROR', message: 'ue', retryable: false },
      { kind: 'unknown', code: 'UNKNOWN_ERROR', message: '?' }
    ];
    for (const err of variants) {
      const receipt = buildErrorReceipt({ capabilityId: CAP, error: err });
      expect(receipt.status).toBe('error');
      if (receipt.status !== 'error') throw new Error('expected error receipt');
      expect(receipt.error.code).toBe(err.code);
    }
  });

  it('preserves structured domain data in a success receipt with stable handles', () => {
    const receipt = buildSuccessReceipt({
      capabilityId: CAP,
      handles: [{ kind: 'actor', ref: parseActorRef('Foo') }],
      data: { spawnId: 'ABC', location: { x: 0, y: 0, z: 0 } }
    });
    expect(receipt.status).toBe('success');
      if (receipt.status !== 'success') throw new Error('expected success receipt');
      expect(receipt.handles[0]?.kind).toBe('actor');
      expect(serializeReceipt(receipt)).toContain('"spawnId":"ABC"');
  });

  it('wraps a typed error in SemanticBoundaryError', () => {
    const err: SemanticError = {
      kind: 'path',
      code: 'PATH_TRAVERSAL',
      message: 't',
      input: '/x/..'
    };
    const wrapped = new SemanticBoundaryError(err);
    expect(wrapped.semanticError.code).toBe('PATH_TRAVERSAL');
  });
});

describe('SemanticErrorSchema exact contract (RED: typed error algebra)', () => {
  it('rejects an unknown error kind', () => {
    const result = SemanticErrorSchema.safeParse({ kind: 'bogus', code: 'X', message: 'no' });
    expect(result.success).toBe(false);
  });

  it('rejects an execution error missing retryable', () => {
    const result = SemanticErrorSchema.safeParse({
      kind: 'execution',
      code: 'EXECUTION_ERROR',
      message: 'boom'
    });
    expect(result.success).toBe(false);
  });

  it('accepts a fully valid execution error', () => {
    const result = SemanticErrorSchema.safeParse({
      kind: 'execution',
      code: 'EXECUTION_ERROR',
      message: 'boom',
      retryable: false
    });
    expect(result.success).toBe(true);
  });
});

describe('ReceiptSchema exact contract (RED: z.unknown placeholders replaced)', () => {
  const CAP = CapabilityIdSchema.parse('asset.import');

  it('rejects a success receipt with a malformed typed handle', () => {
    const result = ReceiptSchema.safeParse({
      status: 'success',
      capabilityId: CAP,
      handles: [{ kind: 'actor' }],
      changes: [],
      warnings: [],
      nextCalls: [],
      data: {}
    });
    expect(result.success).toBe(false);
  });

  it('rejects an error receipt with a malformed semantic error', () => {
    const result = ReceiptSchema.safeParse({
      status: 'error',
      capabilityId: CAP,
      error: { kind: 'bogus', message: 'no' },
      nextCalls: []
    });
    expect(result.success).toBe(false);
  });

  it('rejects a success receipt whose data is not JSON-safe', () => {
    const result = ReceiptSchema.safeParse({
      status: 'success',
      capabilityId: CAP,
      handles: [],
      changes: [],
      warnings: [],
      nextCalls: [],
      data: () => 1
    });
    expect(result.success).toBe(false);
  });

  it('rejects an unknown top-level key in a success receipt', () => {
    const result = ReceiptSchema.safeParse({
      status: 'success',
      capabilityId: CAP,
      handles: [],
      changes: [],
      warnings: [],
      nextCalls: [],
      data: {},
      leaked: true
    });
    expect(result.success).toBe(false);
  });

  it('round-trips a fully valid success receipt', () => {
    const receipt = buildSuccessReceipt({
      capabilityId: CAP,
      handles: [{ kind: 'actor', ref: parseActorRef('Foo') }],
      changes: ['created'],
      warnings: [],
      nextCalls: [{ operation: 'describe', capability: CAP }],
      data: { spawnId: 'ABC', location: { x: 0, y: 0, z: 0 } }
    });
    const result = ReceiptSchema.safeParse(receipt);
    expect(result.success).toBe(true);
  });
});

describe('schema strictness: unknown fields rejected (audit)', () => {
  it('rejects an unknown field on a semantic error', () => {
    expect(
      SemanticErrorSchema.safeParse({ kind: 'unknown', code: 'UNKNOWN_ERROR', message: 'x', leaked: 1 })
        .success
    ).toBe(false);
  });

  it('rejects an unknown field on a next call', () => {
    expect(NextCallSchema.safeParse({ operation: 'search', leaked: true }).success).toBe(false);
  });

  it('rejects an unknown task state', () => {
    expect(TaskStatusSchema.safeParse({ taskId: 't', state: 'weird' }).success).toBe(false);
  });

  it('rejects out-of-range progress on task status', () => {
    expect(
      TaskStatusSchema.safeParse({ taskId: 't', state: 'running', progress: 2 }).success
    ).toBe(false);
  });
});
