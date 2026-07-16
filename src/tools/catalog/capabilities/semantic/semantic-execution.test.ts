import { describe, expect, it } from 'vitest';

import { SemanticBoundaryError } from './errors.js';
import {
  EXECUTION_OPTION_KEYS,
  ExecutionOptionsSchema,
  parseExecutionOptions,
  rejectGatewayControlsInParams
} from './execution-options.js';

describe('execution options boundary', () => {
  it('rejects an unsupported option (wrong-unit duration)', () => {
    expect(() => parseExecutionOptions({ durationSeconds: 5 }, ['timeoutMs'])).toThrow();
  });

  it('rejects a zero (out-of-range) timeout', () => {
    expect(() => parseExecutionOptions({ timeoutMs: 0 }, ['timeoutMs'])).toThrow();
  });

  it('rejects an over-bounded timeout', () => {
    expect(() => parseExecutionOptions({ timeoutMs: 9_999_999 }, ['timeoutMs'])).toThrow();
  });

  it('accepts a bounded timeout', () => {
    const options = parseExecutionOptions({ timeoutMs: 30_000 }, ['timeoutMs']);
    expect(options.timeoutMs).toBe(30_000);
  });

  it('keeps gateway controls out of action params', () => {
    expect(() => rejectGatewayControlsInParams({ timeoutMs: 1000 }, ['timeoutMs'])).toThrow();
  });

  it('exposes the full supported option key set', () => {
    expect(EXECUTION_OPTION_KEYS).toContain('taskPreference');
    expect(EXECUTION_OPTION_KEYS).toContain('idempotencyKey');
  });

  it('rejects an unknown execution-option key (strict object)', () => {
    expect(ExecutionOptionsSchema.safeParse({ bogus: true }).success).toBe(false);
  });

  it('rejects an unsupported option via typed SemanticBoundaryError', () => {
    expect(() => parseExecutionOptions({ durationSeconds: 5 }, ['timeoutMs'])).toThrow(
      SemanticBoundaryError
    );
    try {
      parseExecutionOptions({ durationSeconds: 5 }, ['timeoutMs']);
      throw new Error('expected throw');
    } catch (err) {
      expect(err).toBeInstanceOf(SemanticBoundaryError);
      expect(err instanceof SemanticBoundaryError && err.semanticError.code === 'UNSUPPORTED_OPTION').toBe(true);
    }
  });
});
