// Task 38 lane D — normalized cross-transport PARITY for the direct-call-removal
// migration receipt and per-session profile-store cleanup. Compares receipt
// semantics (operation, message, executable nextCall, stripped control fields)
// and the store's shared cleanup invariants, never transport framing.

import { describe, expect, it } from 'vitest';
import { buildDirectCallMigration } from '../../../src/server/gateway/direct-call-migration.js';
import { allToolNames } from '../../../src/server/gateway/gateway-shared.js';
import { ClientProfileStore } from '../../../src/server/mcp-primitives/client-profile-store.js';
import { MINIMAL_PROFILE, SessionCapabilityProfile } from '../../../src/server/mcp-primitives/session-capability-profile.js';
import { loadNativeOracle, nativeDirectCallFromOracle } from './profiles-fallback-oracle-model.js';

const oracle = loadNativeOracle();
const parents = allToolNames();
const KNOWN = 'manage_asset';

function tsCore(toolName: string, args: Record<string, unknown>): Record<string, unknown> {
  const { suggestions: _suggestions, ...core } = buildDirectCallMigration(toolName, args);
  return core;
}

describe('Task 38 lane D — direct-call migration parity (TS vs native model)', () => {
  it('exposes manage_asset as a known parent tool', () => {
    expect(parents).toContain(KNOWN);
  });

  it('migrates an unknown tool identically (search branch)', () => {
    expect(tsCore('not_a_tool', {})).toEqual(nativeDirectCallFromOracle(oracle, 'not_a_tool', {}, parents));
    const receipt = buildDirectCallMigration('not_a_tool', {});
    expect(Array.isArray(receipt.suggestions)).toBe(true);
    expect((receipt.suggestions ?? []).length).toBeLessThanOrEqual(3);
  });

  it('migrates a known tool with no action identically (describe branch)', () => {
    expect(tsCore(KNOWN, {})).toEqual(nativeDirectCallFromOracle(oracle, KNOWN, {}, parents));
  });

  it('migrates a known tool with an action identically, stripping control fields (execute branch)', () => {
    const args = { action: 'import_asset', subAction: 'ignored', operation: 'nope', params: { sourcePath: '/x' }, destinationPath: '/Game/y' };
    expect(tsCore(KNOWN, args)).toEqual(nativeDirectCallFromOracle(oracle, KNOWN, args, parents));
  });

  it('falls back from a whitespace action to subAction identically', () => {
    const args = { action: '   ', subAction: 'do_thing' };
    expect(tsCore(KNOWN, args)).toEqual(nativeDirectCallFromOracle(oracle, KNOWN, args, parents));
    expect((tsCore(KNOWN, args).nextCall as { action: string }).action).toBe('do_thing');
  });
});

describe('Task 38 lane D — profile-store cleanup shared-invariant parity', () => {
  const profile = (): SessionCapabilityProfile => new SessionCapabilityProfile(MINIMAL_PROFILE);

  it('never keys a blank/unauthenticated session on either surface', () => {
    const store = new ClientProfileStore();
    expect(() => store.setSession('', profile())).toThrow(RangeError);
    expect(store.hasSession('')).toBe(false);
    expect(store.size).toBe(0);
    expect(oracle.store.emptySessionId).toBe('silent-noop');
  });

  it('clears a disconnected session and stays isolated across reconnect on both surfaces', () => {
    const store = new ClientProfileStore();
    store.setSession('s1', profile());
    store.setSession('s2', profile());
    store.clearSession('s1');
    expect(store.hasSession('s1')).toBe(false);
    expect(store.hasSession('s2')).toBe(true);
    store.setSession('s1', profile());
    expect(store.size).toBe(2);
    expect(oracle.store.clearRemovesOnlyNamed).toBe(true);
    expect(oracle.store.overwriteOnReinit).toBe(true);
    expect(oracle.store.sessionIsolation).toBe(true);
  });

  it('treats a stale double clear as an idempotent no-op on both surfaces', () => {
    const store = new ClientProfileStore();
    store.setSession('s1', profile());
    store.clearSession('s1');
    store.clearSession('s1');
    store.clearSession('never');
    expect(store.size).toBe(0);
    expect(oracle.store.clearAbsentIsNoop).toBe(true);
  });
});
