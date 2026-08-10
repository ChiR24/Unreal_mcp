import { describe, expect, it } from 'vitest';
import {
  FALLBACK_PRIMITIVES,
  fallbackPointerFor,
  missingPrimitivePointers,
  SERVER_BACKED_PRIMITIVES,
  type FallbackPrimitive,
  type ServerBackedPrimitive,
} from './fallback-pointers.js';
import { MINIMAL_PROFILE, type ClientCapabilityProfile } from './session-capability-profile.js';

const FULL_PROFILE: ClientCapabilityProfile = {
  hasResources: true,
  hasPrompts: true,
  hasCompletions: true,
  hasSubscriptions: true,
  hasElicitation: true,
  hasTasks: true,
};

const NATIVE_METHOD: Record<ServerBackedPrimitive, string> = {
  resources: 'resources/list',
  prompts: 'prompts/list',
  completions: 'completion/complete',
  subscriptions: 'resources/subscribe',
  tasks: 'tasks/list',
};
const GATEWAY_OP: Record<FallbackPrimitive, string> = {
  resources: 'search',
  prompts: 'describe',
  completions: 'search',
  subscriptions: 'search',
  tasks: 'execute',
};

describe('fallback-pointers', () => {
  it('covers exactly the five absent-primitive kinds', () => {
    expect([...FALLBACK_PRIMITIVES]).toEqual(['resources', 'prompts', 'completions', 'subscriptions', 'tasks']);
  });

  it('gives a capable client a native method reference for every server-backed primitive', () => {
    for (const primitive of SERVER_BACKED_PRIMITIVES) {
      const pointer = fallbackPointerFor(FULL_PROFILE, primitive);
      expect(pointer.mode).toBe('native');
      expect(pointer.nextCall).toEqual({ method: NATIVE_METHOD[primitive] });
    }
  });

  it('routes a Tasks-declaring client to the native tasks method now that the server backs Tasks', () => {
    // Task 44 made tasks/list real on both transports, so pointing a capable
    // client at it is a reachable method rather than a phantom that answers
    // -32601. The Tasks-BLIND case below still gets the bounded gateway op.
    const pointer = fallbackPointerFor(FULL_PROFILE, 'tasks');
    expect(pointer.mode).toBe('native');
    expect(pointer.nextCall).toEqual({ method: 'tasks/list' });
    expect((SERVER_BACKED_PRIMITIVES as readonly string[]).includes('tasks')).toBe(true);

    const blind = fallbackPointerFor(MINIMAL_PROFILE, 'tasks');
    expect(blind.mode).toBe('gateway');
    expect(blind.nextCall).toEqual({ operation: GATEWAY_OP.tasks });
  });

  it('gives a minimal client exactly one bounded gateway operation per primitive', () => {
    for (const primitive of FALLBACK_PRIMITIVES) {
      const pointer = fallbackPointerFor(MINIMAL_PROFILE, primitive);
      expect(pointer.mode).toBe('gateway');
      expect(pointer.nextCall).toEqual({ operation: GATEWAY_OP[primitive] });
      // Bounded: no schema/knowledge dump, one short hint, tiny nextCall.
      expect(Object.keys(pointer.nextCall)).toHaveLength(1);
      expect('schema' in pointer.nextCall).toBe(false);
      expect('inputSchema' in pointer.nextCall).toBe(false);
      expect(pointer.hint.length).toBeLessThan(200);
      expect(JSON.stringify(pointer).length).toBeLessThan(280);
    }
  });

  it('lists one bounded pointer for every absent primitive, in stable order', () => {
    const pointers = missingPrimitivePointers(MINIMAL_PROFILE);
    expect(pointers.map((p) => p.primitive)).toEqual([...FALLBACK_PRIMITIVES]);
    expect(pointers.every((p) => p.mode === 'gateway')).toBe(true);
  });

  it('lists no fallback pointer for a fully capable client', () => {
    expect(missingPrimitivePointers(FULL_PROFILE)).toEqual([]);
  });

  it('lists gateway pointers only for the primitives a partial client lacks', () => {
    const partial: ClientCapabilityProfile = { ...MINIMAL_PROFILE, hasResources: true, hasTasks: true };
    expect(missingPrimitivePointers(partial).map((p) => p.primitive))
      .toEqual(['prompts', 'completions', 'subscriptions']);
  });

  it('is deterministic across repeated calls', () => {
    expect(fallbackPointerFor(MINIMAL_PROFILE, 'tasks')).toEqual(fallbackPointerFor(MINIMAL_PROFILE, 'tasks'));
    expect(missingPrimitivePointers(MINIMAL_PROFILE)).toEqual(missingPrimitivePointers(MINIMAL_PROFILE));
  });
});
