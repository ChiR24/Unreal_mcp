import { describe, expect, it } from 'vitest';

import { ExecutionOptionsSchema } from './execution-options.js';

describe('execution options boundary', () => {
  it('rejects an unsupported option (wrong-unit duration)', () => {
    expect(ExecutionOptionsSchema.safeParse({ durationSeconds: 5 }).success).toBe(false);
  });

  it('rejects a zero (out-of-range) timeout', () => {
    expect(ExecutionOptionsSchema.safeParse({ timeoutMs: 0 }).success).toBe(false);
  });

  it('rejects an over-bounded timeout', () => {
    expect(ExecutionOptionsSchema.safeParse({ timeoutMs: 9_999_999 }).success).toBe(false);
  });

  it('rejects an unknown execution-option key (strict object)', () => {
    expect(ExecutionOptionsSchema.safeParse({ bogus: true }).success).toBe(false);
  });

});
