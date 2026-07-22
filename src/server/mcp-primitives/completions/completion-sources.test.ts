// Task 33: the concrete static source reads only safe, static, in-memory data
// (the generated canonical registry, the bounded enum sets, and the class-alias
// cache) and normalizes to the provider candidate shape. Paired with end-to-end
// passes through complete() over the REAL source, proving the capability and
// project-handle cases on real data rather than fixtures.

import { describe, expect, it } from 'vitest';

import { MINIMAL_PROFILE, SessionCapabilityProfile } from '../session-capability-profile.js';
import { complete } from './completion-provider.js';
import { createStaticCompletionSource } from './completion-sources.js';
import type { CompletionRequest, CompletionSlot } from './completion-types.js';

const source = createStaticCompletionSource();
const SESSION = 'ses-src-1';

// Enable every capability id the source knows so capability-scoped filtering does
// not hide real ids in these source-level assertions.
function allEnabledProfile(): SessionCapabilityProfile {
  const enabled = new Set(source.capabilityCandidates().map((candidate) => candidate.capabilityId ?? ''));
  return new SessionCapabilityProfile(
    { ...MINIMAL_PROFILE, hasCompletions: true },
    { enabledCapabilityIds: () => enabled },
  );
}

const ASSET_SLOT: CompletionSlot = {
  refType: 'ref/resource',
  refId: 'ue://asset/{assetPath}',
  argumentName: 'assetPath',
  kind: 'project-handle',
  capabilityScoped: false,
};

const ENGINE_SLOT: CompletionSlot = {
  refType: 'ref/resource',
  refId: 'ue://knowledge/{engineVersion}/{topic}',
  argumentName: 'engineVersion',
  kind: 'enum',
  capabilityScoped: false,
};

describe('static completion source', () => {
  it('draws capability and legacy candidates from the generated canonical registry', () => {
    const candidates = source.capabilityCandidates();
    expect(candidates.length).toBeGreaterThan(100);
    expect(candidates.every((c) => c.value.length > 0 && c.capabilityId !== undefined)).toBe(true);
    expect(candidates.some((c) => c.kind === 'capability')).toBe(true);
    expect(candidates.some((c) => c.kind === 'legacy-id')).toBe(true);
  });

  it('never emits a raw filesystem path as a project handle', () => {
    for (const candidate of source.projectHandleCandidates(ASSET_SLOT)) {
      expect(candidate.value.startsWith('/')).toBe(false);
      expect(candidate.value.includes('/Script/')).toBe(false);
      expect(/^[a-zA-Z]:[\\/]/u.test(candidate.value)).toBe(false);
    }
  });

  it('exposes the bounded engine-version enum set', () => {
    const values = source.enumCandidates(ENGINE_SLOT).map((candidate) => candidate.value);
    expect(values).toContain('5.7');
    expect(values.length).toBeLessThanOrEqual(20);
  });

  it('completes a real canonical capability prefix end-to-end', () => {
    const first = source.capabilityCandidates().find((candidate) => candidate.kind === 'capability');
    if (first === undefined) throw new Error('expected at least one canonical capability candidate');
    const request: CompletionRequest = {
      ref: { type: 'ref/resource', uri: 'ue://capability/{capabilityId}' },
      argument: { name: 'capabilityId', value: first.value.slice(0, 4) },
    };
    const outcome = complete(request, SESSION, allEnabledProfile(), source);
    expect(outcome.completion.values.length).toBeGreaterThan(0);
    expect(outcome.completion.values).toContain(first.value);
    expect(outcome.completion.values.length).toBeLessThanOrEqual(100);
    expect(outcome.guidance).toBeUndefined();
  });

  it('completes a real project handle prefix end-to-end', () => {
    const request: CompletionRequest = {
      ref: { type: 'ref/resource', uri: 'ue://asset/{assetPath}' },
      argument: { name: 'assetPath', value: 'Point' },
    };
    const outcome = complete(request, SESSION, allEnabledProfile(), source);
    expect(outcome.completion.values).toContain('PointLight');
  });
});
