import { afterEach, describe, expect, it } from 'vitest';
import { expressionRequiresUnsafePermission, unsafePermissionGranted } from './expression-safety.js';

describe('debug expression safety', () => {
  afterEach(() => delete process.env.UE_MCP_DEBUG_ALLOW_UNSAFE);

  it('allows read-only expressions and rejects common side effects', () => {
    expect(expressionRequiresUnsafePermission('Dt')).toBe(false);
    expect(expressionRequiresUnsafePermission('Missile->Velocity.X + 1.0')).toBe(false);
    expect(expressionRequiresUnsafePermission('Dt = 0.0f')).toBe(true);
    expect(expressionRequiresUnsafePermission('Actor->Destroy()')).toBe(true);
    expect(expressionRequiresUnsafePermission('++Frame')).toBe(true);
  });

  it('requires environment and per-call authorization together', () => {
    process.env.UE_MCP_DEBUG_ALLOW_UNSAFE = 'true';
    expect(unsafePermissionGranted(false)).toBe(false);
    expect(unsafePermissionGranted(true)).toBe(true);
    delete process.env.UE_MCP_DEBUG_ALLOW_UNSAFE;
    expect(unsafePermissionGranted(true)).toBe(false);
  });
});
