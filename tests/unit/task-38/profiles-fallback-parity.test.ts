// Task 38 lane D — normalized cross-transport PARITY for client profiles and
// bounded fallbacks. Compares decision SEMANTICS (profile booleans, normalized
// {primitive,mode,reference} pointers) between the TypeScript runtime and the
// independent native model, never stdio/HTTP framing and never source text.
// Two cases assert exact capability-honesty semantics that were once lane-D RED
// gaps; production has since been made honest, so they are now normal passing
// assertions (see the remediation note before the final describe block and the
// pre-fix run recorded in profiles-fallback-RED.log). No case is expected-fail.

import { describe, expect, it } from 'vitest';
import {
  MINIMAL_PROFILE,
  parseClientCapabilityProfile,
  type ClientCapabilityProfile,
} from '../../../src/server/mcp-primitives/session-capability-profile.js';
import {
  FALLBACK_PRIMITIVES,
  fallbackPointerFor,
  missingPrimitivePointers,
} from '../../../src/server/mcp-primitives/fallback-pointers.js';
import { isSafeToElicit } from '../../../src/server/tool-registry-elicitation.js';
import {
  CAPABILITY_MATRIX,
  loadNativeOracle,
  nativeFallbackFromOracle,
  nativeProfileFromOracle,
  normalizeTsPointer,
} from './profiles-fallback-oracle-model.js';

const oracle = loadNativeOracle();

const FULL: ClientCapabilityProfile = {
  hasResources: true, hasPrompts: true, hasCompletions: true,
  hasSubscriptions: true, hasElicitation: true, hasTasks: true,
};
const PARTIAL: ClientCapabilityProfile = { ...MINIMAL_PROFILE, hasResources: true, hasTasks: true };

describe('Task 38 lane D — normalized profile-derivation parity (TS vs native model)', () => {
  for (const testCase of CAPABILITY_MATRIX) {
    it(`derives an identical structural profile for ${testCase.name} (${testCase.note})`, () => {
      expect(parseClientCapabilityProfile(testCase.capabilities))
        .toEqual(nativeProfileFromOracle(testCase.capabilities));
    });
  }

  it('produces a brand-independent profile on both surfaces', () => {
    const a = { elicitation: {}, name: 'evil-tool' };
    const b = { elicitation: {}, name: 'trusted-tool' };
    expect(parseClientCapabilityProfile(a)).toEqual(parseClientCapabilityProfile(b));
    expect(nativeProfileFromOracle(a)).toEqual(nativeProfileFromOracle(b));
    expect(parseClientCapabilityProfile(a)).toEqual(nativeProfileFromOracle(b));
  });
});

describe('Task 38 lane D — normalized fallback-pointer parity (TS vs native model)', () => {
  for (const profile of [FULL, MINIMAL_PROFILE, PARTIAL]) {
    for (const primitive of FALLBACK_PRIMITIVES) {
      it(`resolves ${primitive} identically for profile hasResources=${profile.hasResources} hasTasks=${profile.hasTasks}`, () => {
        expect(normalizeTsPointer(fallbackPointerFor(profile, primitive)))
          .toEqual(nativeFallbackFromOracle(oracle, profile, primitive));
      });
    }
  }

  it('lists the same bounded gateway pointers a minimal client receives', () => {
    const tsNormalized = missingPrimitivePointers(MINIMAL_PROFILE).map(normalizeTsPointer);
    const nativeNormalized = FALLBACK_PRIMITIVES
      .filter((primitive) => !MINIMAL_PROFILE[({ resources: 'hasResources', prompts: 'hasPrompts', completions: 'hasCompletions', subscriptions: 'hasSubscriptions', tasks: 'hasTasks' } as const)[primitive]])
      .map((primitive) => nativeFallbackFromOracle(oracle, MINIMAL_PROFILE, primitive));
    expect(tsNormalized).toEqual(nativeNormalized);
  });
});

describe('Task 38 lane D — exact parity rejects injected one-field drift', () => {
  it('fails parity when a server profile claims a capability the caps do not structurally support', () => {
    const caps = { resources: 'true', tasks: 'enabled' }; // strings never enable
    const honest = parseClientCapabilityProfile(caps);
    const drifted = { ...honest, hasTasks: true }; // injected false capability
    expect(honest).toEqual(nativeProfileFromOracle(caps));
    expect(drifted).not.toEqual(nativeProfileFromOracle(caps));
  });

  it('fails parity when a fallback pointer carries an extra field beyond {primitive,mode,reference}', () => {
    const honest = normalizeTsPointer(fallbackPointerFor(MINIMAL_PROFILE, 'resources'));
    const drifted = { ...honest, schema: { dumped: true } }; // injected schema dump
    expect(honest).toEqual(nativeFallbackFromOracle(oracle, MINIMAL_PROFILE, 'resources'));
    expect(drifted).not.toEqual(nativeFallbackFromOracle(oracle, MINIMAL_PROFILE, 'resources'));
  });

  it('fails parity when a fallback reference is swapped for a different operation', () => {
    const drifted = { primitive: 'resources' as const, mode: 'gateway' as const, reference: 'execute' };
    expect(drifted).not.toEqual(nativeFallbackFromOracle(oracle, MINIMAL_PROFILE, 'resources'));
  });
});

// Capability-honesty parity. These two assertions were the lane-D RED gaps
// (expected-fail ratchets); they are now converted to normal passing assertions
// after production was made honest. Raw pre-fix failing run: profiles-fallback-RED.log.
describe('Task 38 lane D — capability honesty (remediated): tasks gateway-only + native elicitation mirror', () => {
  it('routes native-mode fallbacks only to server-backed methods and never emits a phantom native Tasks pointer', () => {
    for (const primitive of FALLBACK_PRIMITIVES) {
      const pointer = fallbackPointerFor(FULL, primitive);
      if (pointer.mode === 'native') {
        expect(oracle.serverBackedMethods).toContain((pointer.nextCall as { method?: string }).method);
      } else {
        expect(pointer.mode).toBe('gateway');
      }
    }
    // Tasks is declared by FULL yet unbacked by the server (Task 44 pending): it
    // must resolve to the bounded gateway execute pointer, never a phantom
    // native tasks/list the server would only answer with -32601.
    const tasks = fallbackPointerFor(FULL, 'tasks');
    expect(tasks.mode).toBe('gateway');
    expect(tasks.nextCall).toEqual({ operation: 'execute' });
    expect(oracle.serverBackedMethods).not.toContain('tasks/list');
  });

  it('exposes a native elicitation-decision mirror whose safe-field and boolean-consent policy matches the TS runtime', () => {
    const e = oracle.elicitation as {
      readonly safeFieldPolicy: {
        readonly excludedSecretFields: readonly string[];
        readonly excludedDestructiveFields: readonly string[];
        readonly allowedSafeFields: readonly string[];
      };
      readonly highImpactConsent: {
        readonly field: string;
        readonly type: string;
        readonly reasons: readonly string[];
        readonly grantedOnlyOnExplicitTrue: boolean;
        readonly logsTokenOrValue: boolean;
      };
    } | null;
    expect(e).not.toBeNull();
    if (e === null) return;
    // The mirror's declared secret/destructive exclusions must be classified
    // identically by the live TS safe-field policy (framing-neutral parity).
    for (const field of [...e.safeFieldPolicy.excludedSecretFields, ...e.safeFieldPolicy.excludedDestructiveFields]) {
      expect(isSafeToElicit(field)).toBe(false);
    }
    for (const field of e.safeFieldPolicy.allowedSafeFields) {
      expect(isSafeToElicit(field)).toBe(true);
    }
    // High-impact consent: a single boolean field, granted only on explicit
    // true, three bounded reasons, and never logs a token or field value.
    expect(e.highImpactConsent.field).toBe('consent');
    expect(e.highImpactConsent.type).toBe('boolean');
    expect(e.highImpactConsent.grantedOnlyOnExplicitTrue).toBe(true);
    expect(e.highImpactConsent.reasons).toEqual(['granted', 'declined', 'unsupported']);
    expect(e.highImpactConsent.logsTokenOrValue).toBe(false);
  });
});
