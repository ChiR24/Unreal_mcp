import { describe, expect, it } from 'vitest';
import { ClientProfileStore } from './client-profile-store.js';
import { MINIMAL_PROFILE, SessionCapabilityProfile } from './session-capability-profile.js';

function profile(overrides: Partial<{ hasElicitation: boolean }> = {}): SessionCapabilityProfile {
  return new SessionCapabilityProfile({ ...MINIMAL_PROFILE, ...overrides });
}

describe('ClientProfileStore (standalone explicit-session store)', () => {
  it('stores and retrieves a profile by explicit session id', () => {
    const store = new ClientProfileStore();
    const p = profile({ hasElicitation: true });
    store.setSession('sess-a', p);
    expect(store.getSession('sess-a')).toBe(p);
    expect(store.getSession('sess-a')?.hasElicitation).toBe(true);
    expect(store.hasSession('sess-a')).toBe(true);
    expect(store.size).toBe(1);
  });

  it('isolates sessions: one id never returns another id profile', () => {
    const store = new ClientProfileStore();
    const a = profile({ hasElicitation: true });
    const b = profile({ hasElicitation: false });
    store.setSession('a', a);
    store.setSession('b', b);
    expect(store.getSession('a')).toBe(a);
    expect(store.getSession('b')).toBe(b);
    expect(store.getSession('missing')).toBeUndefined();
  });

  it('clearSession removes only the named session and is idempotent', () => {
    const store = new ClientProfileStore();
    store.setSession('a', profile());
    store.setSession('b', profile());
    store.clearSession('a');
    expect(store.hasSession('a')).toBe(false);
    expect(store.hasSession('b')).toBe(true);
    expect(store.size).toBe(1);
    // Clearing an already-absent (stale) session must not throw.
    store.clearSession('a');
    store.clearSession('never-existed');
    expect(store.size).toBe(1);
  });

  it('rejects an empty session id rather than keying a blank profile', () => {
    const store = new ClientProfileStore();
    expect(() => store.setSession('', profile())).toThrow(RangeError);
    expect(store.size).toBe(0);
  });

  it('overwrites the profile when the same session re-initializes', () => {
    const store = new ClientProfileStore();
    const first = profile({ hasElicitation: false });
    const second = profile({ hasElicitation: true });
    store.setSession('a', first);
    store.setSession('a', second);
    expect(store.getSession('a')).toBe(second);
    expect(store.size).toBe(1);
  });
});
