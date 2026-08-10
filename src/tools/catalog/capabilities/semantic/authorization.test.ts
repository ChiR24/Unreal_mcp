import { describe, expect, it } from 'vitest';

import {
  ConsentGrantSchema,
  isConsentSatisfied,
  isScopeAuthorized,
  type PolicyScope
} from './authorization.js';

describe('isScopeAuthorized — exact-set with admin wildcard (not rank)', () => {
  it('admin wildcard authorizes every required scope', () => {
    for (const required of ['read', 'write', 'destructive', 'admin'] as const) {
      expect(isScopeAuthorized(required, ['admin'])).toBe(true);
    }
  });

  it('exact membership only — write does NOT imply read or destructive', () => {
    const granted: PolicyScope[] = ['write'];
    expect(isScopeAuthorized('write', granted)).toBe(true);
    expect(isScopeAuthorized('read', granted)).toBe(false);
    expect(isScopeAuthorized('destructive', granted)).toBe(false);
    expect(isScopeAuthorized('admin', granted)).toBe(false);
  });

  it('an empty granted set authorizes nothing', () => {
    for (const required of ['read', 'write', 'destructive', 'admin'] as const) {
      expect(isScopeAuthorized(required, [])).toBe(false);
    }
  });

  it('multiple explicit scopes authorize exactly their members', () => {
    const granted: PolicyScope[] = ['read', 'write'];
    expect(isScopeAuthorized('read', granted)).toBe(true);
    expect(isScopeAuthorized('write', granted)).toBe(true);
    expect(isScopeAuthorized('destructive', granted)).toBe(false);
    expect(isScopeAuthorized('admin', granted)).toBe(false);
  });

  it('a scoped principal may never hold admin implicitly', () => {
    // A scoped token lists only read/write/destructive; admin is legacy/loopback only.
    expect(isScopeAuthorized('admin', ['read', 'write', 'destructive'])).toBe(false);
  });
});

describe('isConsentSatisfied — no inference, current call only', () => {
  it('policy none always passes regardless of acknowledgement', () => {
    expect(isConsentSatisfied('none', undefined)).toBe(true);
    expect(isConsentSatisfied('none', 'explicit')).toBe(true);
    expect(isConsentSatisfied('none', 'elevated')).toBe(true);
  });

  it('explicit requires explicit or the stronger elevated', () => {
    expect(isConsentSatisfied('explicit', undefined)).toBe(false);
    expect(isConsentSatisfied('explicit', 'explicit')).toBe(true);
    expect(isConsentSatisfied('explicit', 'elevated')).toBe(true);
  });

  it('elevated requires elevated exactly', () => {
    expect(isConsentSatisfied('elevated', undefined)).toBe(false);
    expect(isConsentSatisfied('elevated', 'explicit')).toBe(false);
    expect(isConsentSatisfied('elevated', 'elevated')).toBe(true);
  });
});

describe('ConsentGrantSchema — strict, capability-bound, single-call', () => {
  it('accepts a well-formed grant', () => {
    expect(
      ConsentGrantSchema.safeParse({ capability: 'manage_asset.delete_asset', acknowledge: 'explicit' }).success
    ).toBe(true);
    expect(
      ConsentGrantSchema.safeParse({ capability: 'manage_asset.delete_asset', acknowledge: 'elevated' }).success
    ).toBe(true);
  });

  it('rejects a bad acknowledgement, unknown keys, and malformed capability id', () => {
    expect(
      ConsentGrantSchema.safeParse({ capability: 'manage_asset.delete_asset', acknowledge: 'yes' }).success
    ).toBe(false);
    expect(
      ConsentGrantSchema.safeParse({ capability: 'manage_asset.delete_asset', acknowledge: 'explicit', extra: 1 }).success
    ).toBe(false);
    expect(ConsentGrantSchema.safeParse({ capability: 'NotSnakeCase', acknowledge: 'explicit' }).success).toBe(false);
    expect(ConsentGrantSchema.safeParse({ acknowledge: 'explicit' }).success).toBe(false);
  });
});
