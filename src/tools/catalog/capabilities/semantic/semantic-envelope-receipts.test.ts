import { describe, expect, it } from 'vitest';

import { CapabilityIdSchema } from '../identifiers.js';

import { ReceiptSchema } from './envelope.js';
import {
  NextCallSchema,
  SemanticBoundaryError,
  type SemanticError,
  SemanticErrorSchema,
  TaskStatusSchema,
} from './errors.js';

describe('receipt / result envelope', () => {
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

describe('SemanticErrorSchema exact contract (typed error algebra)', () => {
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

describe('ReceiptSchema exact contract (z.unknown placeholders replaced)', () => {
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
