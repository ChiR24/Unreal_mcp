// Task 33: RED-first behavior lock for the bounded completion provider. Every
// case injects deterministic fixtures (a fake candidate source and a Task 35 C3
// SessionCapabilityProfile) so the provider is exercised with no transport, no
// registry, and no editor. Covers the eleven required cases: capability,
// legacy id, enum, safe path/handle, unavailable, typo, cursor, limit,
// disabled session capability, secret field, and unbounded prefix.

import { describe, expect, it } from 'vitest';

import { MINIMAL_PROFILE, SessionCapabilityProfile } from '../session-capability-profile.js';
import { complete } from './completion-provider.js';
import {
  COMPLETION_GUIDANCE_CODES,
  MAX_COMPLETION_ITEMS,
  MAX_PREFIX_LENGTH,
  type CompletionCandidate,
  type CompletionCandidateSource,
  type CompletionRequest,
  type CompletionSlot,
} from './completion-types.js';

const SESSION = 'ses-test-1';

const CAPABILITY_POOL: readonly CompletionCandidate[] = [
  { value: 'asset.import', kind: 'capability', capabilityId: 'asset.import' },
  { value: 'asset.list', kind: 'capability', capabilityId: 'asset.list' },
  { value: 'asset.validate', kind: 'capability', capabilityId: 'asset.validate' },
  { value: 'manage_level.save', kind: 'capability', capabilityId: 'manage_level.save' },
  { value: 'importAsset', kind: 'legacy-id', capabilityId: 'asset.import' },
];

const ENGINE_VERSIONS: readonly CompletionCandidate[] = ['5.0', '5.3', '5.5', '5.7', '5.8'].map(
  (value) => ({ value, kind: 'enum' }),
);

const PROJECT_HANDLES: readonly CompletionCandidate[] = ['StaticMeshActor', 'PointLight', 'StaticMeshComponent'].map(
  (value) => ({ value, kind: 'project-handle' }),
);

function makeSource(overrides: Partial<CompletionCandidateSource> = {}): CompletionCandidateSource {
  return {
    capabilityCandidates: () => CAPABILITY_POOL,
    enumCandidates: (slot: CompletionSlot) =>
      slot.argumentName === 'engineVersion' ? ENGINE_VERSIONS : [],
    projectHandleCandidates: () => PROJECT_HANDLES,
    ...overrides,
  };
}

function makeProfile(enabled: readonly string[]): SessionCapabilityProfile {
  return new SessionCapabilityProfile(
    { ...MINIMAL_PROFILE, hasCompletions: true },
    { enabledCapabilityIds: () => new Set(enabled) },
  );
}

const ALL_ENABLED = ['asset.import', 'asset.list', 'asset.validate', 'manage_level.save'];

function capabilityRequest(value: string): CompletionRequest {
  return { ref: { type: 'ref/resource', uri: 'ue://capability/{capabilityId}' }, argument: { name: 'capabilityId', value } };
}

describe('completion provider', () => {
  it('ranks canonical capability ids for a partial capability value', () => {
    const outcome = complete(capabilityRequest('asset.'), SESSION, makeProfile(ALL_ENABLED), makeSource());
    expect(outcome.completion.values).toContain('asset.import');
    expect(outcome.completion.values).toContain('asset.list');
    expect(outcome.completion.values).not.toContain('manage_level.save');
    expect(outcome.guidance).toBeUndefined();
  });

  it('surfaces a legacy migration id that matches the prefix', () => {
    const outcome = complete(capabilityRequest('import'), SESSION, makeProfile(ALL_ENABLED), makeSource());
    expect(outcome.completion.values).toContain('importAsset');
  });

  it('completes enum/schema values for an enum slot', () => {
    const request: CompletionRequest = {
      ref: { type: 'ref/resource', uri: 'ue://knowledge/{engineVersion}/{topic}' },
      argument: { name: 'engineVersion', value: '5.' },
    };
    const outcome = complete(request, SESSION, makeProfile(ALL_ENABLED), makeSource());
    expect([...outcome.completion.values]).toEqual(['5.0', '5.3', '5.5', '5.7', '5.8']);
  });

  it('completes safe cached project handles, never raw filesystem paths', () => {
    const request: CompletionRequest = {
      ref: { type: 'ref/resource', uri: 'ue://asset/{assetPath}' },
      argument: { name: 'assetPath', value: 'Static' },
    };
    const outcome = complete(request, SESSION, makeProfile(ALL_ENABLED), makeSource());
    expect(outcome.completion.values).toContain('StaticMeshActor');
    expect(outcome.completion.values).toContain('StaticMeshComponent');
    for (const value of outcome.completion.values) {
      expect(value.startsWith('/')).toBe(false);
      expect(/^[a-zA-Z]:[\\/]/u.test(value)).toBe(false);
    }
  });

  it('returns safe-empty UNAVAILABLE guidance for an argument with no completable slot', () => {
    const request: CompletionRequest = {
      ref: { type: 'ref/prompt', name: 'inspect-fix' },
      argument: { name: 'newValue', value: 'Z=1' },
    };
    const outcome = complete(request, SESSION, makeProfile(ALL_ENABLED), makeSource());
    expect(outcome.completion.values).toEqual([]);
    expect(outcome.guidance?.code).toBe(COMPLETION_GUIDANCE_CODES.UNAVAILABLE);
  });

  it('ranks a near-match candidate despite a single-character typo', () => {
    const outcome = complete(capabilityRequest('aset.'), SESSION, makeProfile(ALL_ENABLED), makeSource());
    expect(outcome.completion.values).toContain('asset.import');
  });

  it('filters by the prefix before the cursor', () => {
    const outcome = complete(capabilityRequest('asset.im'), SESSION, makeProfile(ALL_ENABLED), makeSource());
    expect(outcome.completion.values).toContain('asset.import');
    expect(outcome.completion.values).not.toContain('asset.list');
  });

  it('caps values at the item budget and reports total/hasMore', () => {
    const big: CompletionCandidate[] = Array.from({ length: 250 }, (_v, index) => ({
      value: `asset.gen_${String(index).padStart(4, '0')}`,
      kind: 'capability',
      capabilityId: `asset.gen_${String(index).padStart(4, '0')}`,
    }));
    const enabled = big.map((candidate) => candidate.capabilityId ?? '');
    const outcome = complete(
      capabilityRequest('asset.gen_'),
      SESSION,
      makeProfile(enabled),
      makeSource({ capabilityCandidates: () => big }),
    );
    expect(outcome.completion.values.length).toBe(MAX_COMPLETION_ITEMS);
    expect(outcome.completion.total).toBe(250);
    expect(outcome.completion.hasMore).toBe(true);
  });

  it('never suggests a capability disabled for the session', () => {
    const outcome = complete(capabilityRequest('asset.'), SESSION, makeProfile(['asset.list']), makeSource());
    expect(outcome.completion.values).toContain('asset.list');
    expect(outcome.completion.values).not.toContain('asset.import');
    expect(outcome.completion.values).not.toContain('asset.validate');
  });

  it('refuses to complete a secret-named argument with safe-empty guidance', () => {
    const request: CompletionRequest = {
      ref: { type: 'ref/resource', uri: 'ue://capability/{capabilityId}' },
      argument: { name: 'apiToken', value: 'sk-' },
    };
    const outcome = complete(request, SESSION, makeProfile(ALL_ENABLED), makeSource());
    expect(outcome.completion.values).toEqual([]);
    expect(outcome.guidance?.code).toBe(COMPLETION_GUIDANCE_CODES.SECRET_FIELD);
    expect(JSON.stringify(outcome.guidance)).not.toContain('sk-');
  });

  it('refuses an unbounded prefix rather than scanning', () => {
    const outcome = complete(
      capabilityRequest('a'.repeat(MAX_PREFIX_LENGTH + 1)),
      SESSION,
      makeProfile(ALL_ENABLED),
      makeSource(),
    );
    expect(outcome.completion.values).toEqual([]);
    expect(outcome.guidance?.code).toBe(COMPLETION_GUIDANCE_CODES.UNBOUNDED_PREFIX);
  });

  it('is deterministic: identical requests yield identical ordered values', () => {
    const first = complete(capabilityRequest('asset.'), SESSION, makeProfile(ALL_ENABLED), makeSource());
    const second = complete(capabilityRequest('asset.'), SESSION, makeProfile(ALL_ENABLED), makeSource());
    expect([...first.completion.values]).toEqual([...second.completion.values]);
  });
});
