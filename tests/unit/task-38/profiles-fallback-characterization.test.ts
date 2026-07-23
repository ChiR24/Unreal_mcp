// Task 38 lane D — CHARACTERIZATION (GREEN baseline).
// Pins the CURRENT TypeScript behavior of adaptive client profiles, bounded
// feature fallbacks, per-session profile store cleanup, and advertised-capability
// honesty, plus the native oracle's self-consistency with the real registered
// method table. Every case here must PASS: it locks "what is" before the parity
// suite asserts "what must match across transports".

import { describe, expect, it } from 'vitest';
import {
  MINIMAL_PROFILE,
  SessionCapabilityProfile,
  parseClientCapabilityProfile,
} from '../../../src/server/mcp-primitives/session-capability-profile.js';
import {
  FALLBACK_PRIMITIVES,
  fallbackPointerFor,
  missingPrimitivePointers,
} from '../../../src/server/mcp-primitives/fallback-pointers.js';
import { ClientProfileStore } from '../../../src/server/mcp-primitives/client-profile-store.js';
import {
  ADVERTISED_SESSION_CAPABILITIES,
  deriveAdvertisedCapabilities,
  createPrimitiveRegistry,
  PrimitiveRegistrationError,
} from '../../../src/server/mcp-primitives/primitive-registry.js';
import { REGISTERED_PRIMITIVE_METHODS } from '../../../src/server/mcp-primitives/primitive-handlers.js';
import { loadNativeOracle, normalizeTsPointer } from './profiles-fallback-oracle-model.js';

const oracle = loadNativeOracle();

describe('Task 38 lane D — profile derivation (current TS behavior)', () => {
  it('derives every capability when a full client declares them as objects', () => {
    const profile = parseClientCapabilityProfile({
      resources: { subscribe: true }, prompts: {}, completions: {}, elicitation: {}, tasks: {},
    });
    expect(profile).toEqual({
      hasResources: true, hasPrompts: true, hasCompletions: true,
      hasSubscriptions: true, hasElicitation: true, hasTasks: true,
    });
  });

  it('yields the minimal all-false profile when a client declares nothing', () => {
    expect(parseClientCapabilityProfile({})).toEqual(MINIMAL_PROFILE);
  });

  it('treats every malformed capability payload as the minimal profile', () => {
    for (const bad of [undefined, null, 42, 'resources', [], true]) {
      expect(parseClientCapabilityProfile(bad)).toEqual(MINIMAL_PROFILE);
    }
  });

  it('never lets a string/number/array value falsely enable a capability', () => {
    const profile = parseClientCapabilityProfile({ resources: 'true', subscriptions: 'yes', tasks: 1, prompts: [] });
    expect(profile).toEqual(MINIMAL_PROFILE);
  });

  it('never lets a brand name or version enable a capability or change behavior', () => {
    expect(parseClientCapabilityProfile({ name: 'Cursor', title: 'trusted', version: '99' })).toEqual(MINIMAL_PROFILE);
    const a = parseClientCapabilityProfile({ elicitation: {}, name: 'evil-tool' });
    const b = parseClientCapabilityProfile({ elicitation: {}, name: 'trusted-tool' });
    expect(a).toEqual(b);
  });

  it('derives subscriptions from nested resources.subscribe but ignores it under experimental', () => {
    expect(parseClientCapabilityProfile({ resources: { subscribe: true } }).hasSubscriptions).toBe(true);
    expect(parseClientCapabilityProfile({ resources: {} }).hasSubscriptions).toBe(false);
    expect(parseClientCapabilityProfile({ experimental: { resources: { subscribe: true } } }).hasSubscriptions).toBe(false);
  });

  it('reads capabilities nested under experimental', () => {
    const profile = parseClientCapabilityProfile({ experimental: { tasks: {}, completions: true } });
    expect(profile.hasTasks).toBe(true);
    expect(profile.hasCompletions).toBe(true);
    expect(profile.hasResources).toBe(false);
  });
});

describe('Task 38 lane D — bounded fallback pointers (current TS behavior)', () => {
  const FULL = { hasResources: true, hasPrompts: true, hasCompletions: true, hasSubscriptions: true, hasElicitation: true, hasTasks: true };

  it('gives a fully capable client no fallback pointers', () => {
    expect(missingPrimitivePointers(FULL)).toEqual([]);
  });

  it('gives a minimal client exactly one bounded gateway pointer per primitive, in stable order', () => {
    const pointers = missingPrimitivePointers(MINIMAL_PROFILE);
    expect(pointers.map((p) => p.primitive)).toEqual([...FALLBACK_PRIMITIVES]);
    for (const pointer of pointers) {
      expect(pointer.mode).toBe('gateway');
      expect(Object.keys(pointer.nextCall)).toHaveLength(1);
      expect('schema' in pointer.nextCall).toBe(false);
      expect('inputSchema' in pointer.nextCall).toBe(false);
      expect(pointer.hint.length).toBeLessThan(200);
      expect(JSON.stringify(pointer).length).toBeLessThan(280);
    }
  });

  it('points a capable client at the native method and a lacking client at one gateway operation', () => {
    expect(normalizeTsPointer(fallbackPointerFor(FULL, 'resources'))).toEqual({ primitive: 'resources', mode: 'native', reference: 'resources/list' });
    expect(normalizeTsPointer(fallbackPointerFor(MINIMAL_PROFILE, 'resources'))).toEqual({ primitive: 'resources', mode: 'gateway', reference: 'search' });
  });

  it('lists gateway pointers only for the primitives a partial client lacks', () => {
    const partial = { ...MINIMAL_PROFILE, hasResources: true, hasTasks: true };
    expect(missingPrimitivePointers(partial).map((p) => p.primitive)).toEqual(['prompts', 'completions', 'subscriptions']);
  });
});

describe('Task 38 lane D — per-session profile store reconnect cleanup (current TS behavior)', () => {
  const profile = (): SessionCapabilityProfile => new SessionCapabilityProfile(MINIMAL_PROFILE);

  it('clears a disconnected session and stays clean across a reconnect', () => {
    const store = new ClientProfileStore();
    store.setSession('sess-1', profile());
    store.clearSession('sess-1'); // disconnect
    expect(store.hasSession('sess-1')).toBe(false);
    expect(store.size).toBe(0);
    store.setSession('sess-1', profile()); // reconnect
    expect(store.hasSession('sess-1')).toBe(true);
    expect(store.size).toBe(1);
  });

  it('treats a double disconnect (stale clear) as an idempotent no-op', () => {
    const store = new ClientProfileStore();
    store.setSession('a', profile());
    store.clearSession('a');
    store.clearSession('a');
    store.clearSession('never-existed');
    expect(store.size).toBe(0);
  });

  it('isolates sessions and refuses to key a blank/unauthenticated session', () => {
    const store = new ClientProfileStore();
    const a = profile();
    store.setSession('a', a);
    expect(store.getSession('a')).toBe(a);
    expect(store.getSession('b')).toBeUndefined();
    expect(() => store.setSession('', profile())).toThrow(RangeError);
    expect(store.size).toBe(1);
  });
});

describe('Task 38 lane D — advertised-capability honesty (current TS behavior)', () => {
  const fullTable = new Map<string, unknown>(
    ['tools/list', 'tools/call', 'resources/list', 'resources/templates/list', 'resources/read',
      'resources/subscribe', 'resources/unsubscribe', 'prompts/list', 'prompts/get', 'completion/complete']
      .map((m) => [m, () => ({})]),
  );

  it('advertises exactly the backed session profile and never tasks or elicitation', () => {
    expect(ADVERTISED_SESSION_CAPABILITIES).toEqual({ tools: {}, resources: { subscribe: true }, prompts: {}, completions: {} });
    expect('tasks' in ADVERTISED_SESSION_CAPABILITIES).toBe(false);
    expect('elicitation' in ADVERTISED_SESSION_CAPABILITIES).toBe(false);
  });

  it('derives the advertised surface only from the fully backed handler table', () => {
    expect(deriveAdvertisedCapabilities(fullTable)).toEqual(ADVERTISED_SESSION_CAPABILITIES);
    const noSubscribe = new Map(fullTable);
    noSubscribe.delete('resources/subscribe');
    expect(deriveAdvertisedCapabilities(noSubscribe).resources).toEqual({});
  });

  it('fails closed pre-connect when an advertised capability lacks its handler', () => {
    const missing = new Map(fullTable);
    missing.delete('completion/complete');
    expect(() => createPrimitiveRegistry({ handlers: missing, capabilities: ADVERTISED_SESSION_CAPABILITIES }))
      .toThrow(PrimitiveRegistrationError);
  });
});

describe('Task 38 lane D — native oracle self-consistency', () => {
  it('carries the same server-backed method set the primitive handlers register', () => {
    for (const method of REGISTERED_PRIMITIVE_METHODS) {
      expect(oracle.serverBackedMethods).toContain(method);
    }
    for (const method of ['tools/list', 'tools/call', 'resources/list', 'resources/templates/list', 'resources/read']) {
      expect(oracle.serverBackedMethods).toContain(method);
    }
  });

  it('records the JSON-RPC error-code contract (unsupported vs refused-but-supported)', () => {
    expect(oracle.errorCodes.unsupportedMethod).toBe(-32601);
    expect(oracle.errorCodes.refusedButSupported).toBe(-32602);
  });
});
