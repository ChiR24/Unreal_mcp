import { describe, expect, it } from 'vitest';
import {
  EMPTY_ENABLED_CAPABILITIES,
  MINIMAL_PROFILE,
  SessionCapabilityProfile,
  parseClientCapabilityProfile,
  type ClientCapabilityProfile,
  type EnabledCapabilityProvider,
} from './session-capability-profile.js';

const KEYS = ['resources', 'prompts', 'completions', 'subscriptions', 'elicitation', 'tasks'] as const;
type Key = (typeof KEYS)[number];
const FLAG: Record<Key, keyof ClientCapabilityProfile> = {
  resources: 'hasResources',
  prompts: 'hasPrompts',
  completions: 'hasCompletions',
  subscriptions: 'hasSubscriptions',
  elicitation: 'hasElicitation',
  tasks: 'hasTasks',
};

function expectedFor(mask: number): ClientCapabilityProfile {
  const profile = { ...MINIMAL_PROFILE };
  KEYS.forEach((key, index) => {
    if ((mask & (1 << index)) !== 0) profile[FLAG[key]] = true;
  });
  return profile;
}

describe('session-capability-profile C3 primitive', () => {
  it('derives every one of the 64 structural capability combinations', () => {
    for (let mask = 0; mask < 64; mask += 1) {
      const capabilities: Record<string, unknown> = {};
      KEYS.forEach((key, index) => {
        if ((mask & (1 << index)) !== 0) capabilities[key] = {};
      });
      expect(parseClientCapabilityProfile(capabilities)).toEqual(expectedFor(mask));
    }
  });

  it('treats a bare true and an object identically as present', () => {
    expect(parseClientCapabilityProfile({ elicitation: true }).hasElicitation).toBe(true);
    expect(parseClientCapabilityProfile({ elicitation: {} }).hasElicitation).toBe(true);
    expect(parseClientCapabilityProfile({ elicitation: false }).hasElicitation).toBe(false);
  });

  it('reads capabilities nested under experimental', () => {
    const profile = parseClientCapabilityProfile({ experimental: { tasks: {}, completions: true } });
    expect(profile.hasTasks).toBe(true);
    expect(profile.hasCompletions).toBe(true);
    expect(profile.hasResources).toBe(false);
  });

  it('sets hasSubscriptions from nested resources.subscribe true', () => {
    expect(parseClientCapabilityProfile({ resources: { subscribe: true } }).hasSubscriptions).toBe(true);
    expect(parseClientCapabilityProfile({ resources: {} }).hasSubscriptions).toBe(false);
  });

  it('yields the minimal profile for malformed or empty capabilities', () => {
    for (const bad of [undefined, null, 42, 'x', [], true]) {
      expect(parseClientCapabilityProfile(bad)).toEqual(MINIMAL_PROFILE);
    }
  });

  it('never lets a brand name or version enable a capability', () => {
    const branded = { name: 'Definitely-Trusted-Client', version: '99.0', title: 'Cursor' };
    expect(parseClientCapabilityProfile(branded)).toEqual(MINIMAL_PROFILE);
    const sameCapsDifferentBrand = { elicitation: {}, name: 'evil-tool' };
    const sameCapsOtherBrand = { elicitation: {}, name: 'trusted-tool' };
    expect(parseClientCapabilityProfile(sameCapsDifferentBrand))
      .toEqual(parseClientCapabilityProfile(sameCapsOtherBrand));
  });

  it('forwards enabledCapabilityIds to the injected provider by session id', () => {
    const provider: EnabledCapabilityProvider = {
      enabledCapabilityIds: (sessionId) => new Set([`cap:${sessionId}`]),
    };
    const view = new SessionCapabilityProfile({ ...MINIMAL_PROFILE, hasCompletions: true }, provider);
    expect(view.hasCompletions).toBe(true);
    expect([...view.enabledCapabilityIds('s1')]).toEqual(['cap:s1']);
    expect([...view.enabledCapabilityIds('s2')]).toEqual(['cap:s2']);
  });

  it('defaults to an empty enabled-capability set with no provider', () => {
    const view = new SessionCapabilityProfile(MINIMAL_PROFILE);
    expect(view.enabledCapabilityIds('any').size).toBe(0);
    expect(EMPTY_ENABLED_CAPABILITIES.enabledCapabilityIds('any').size).toBe(0);
  });
});
