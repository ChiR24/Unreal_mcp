// Task 33: direct lock for the completable-slot registry and the safety gate,
// independent of the provider and fast (no registry import). The safety gate is
// the security boundary that refuses secrets, destructive confirmations, and raw
// host paths before any candidate work, even on an unknown ref.

import { describe, expect, it } from 'vitest';

import { COMPLETION_SLOTS, classifyUnsafe, refIdOf, resolveSlot } from './completion-slots.js';
import { COMPLETION_GUIDANCE_CODES, type CompletionReference } from './completion-types.js';

describe('completion slot registry', () => {
  it('resolves every declared slot by its ref and argument', () => {
    for (const slot of COMPLETION_SLOTS) {
      const ref: CompletionReference =
        slot.refType === 'ref/prompt'
          ? { type: 'ref/prompt', name: slot.refId }
          : { type: 'ref/resource', uri: slot.refId };
      expect(resolveSlot(ref, slot.argumentName)).toEqual(slot);
    }
  });

  it('returns undefined for an unknown ref or argument', () => {
    expect(resolveSlot({ type: 'ref/prompt', name: 'inspect-fix' }, 'newValue')).toBeUndefined();
    expect(resolveSlot({ type: 'ref/resource', uri: 'ue://capability/{capabilityId}' }, 'nope')).toBeUndefined();
    expect(resolveSlot({ type: 'ref/resource', uri: 'ue://unknown/{x}' }, 'x')).toBeUndefined();
  });

  it('normalizes the ref id from a prompt name or a resource uri', () => {
    expect(refIdOf({ type: 'ref/prompt', name: 'asset-import' })).toBe('asset-import');
    expect(refIdOf({ type: 'ref/resource', uri: 'ue://asset/{assetPath}' })).toBe('ue://asset/{assetPath}');
  });
});

describe('completion safety gate', () => {
  it('refuses secret-named arguments', () => {
    for (const name of ['token', 'apiKey', 'api_key', 'password', 'bearerToken', 'authSecret', 'privateKey', 'credential']) {
      expect(classifyUnsafe(name, 'x')).toBe(COMPLETION_GUIDANCE_CODES.SECRET_FIELD);
    }
  });

  it('refuses destructive-confirmation arguments', () => {
    for (const name of ['confirm', 'forceDelete', 'overwrite', 'purge', 'wipe', 'destroy']) {
      expect(classifyUnsafe(name, 'yes')).toBe(COMPLETION_GUIDANCE_CODES.DESTRUCTIVE_FIELD);
    }
  });

  it('refuses raw host filesystem paths and traversal', () => {
    for (const value of ['/home/user/x', '/etc/passwd', 'C:\\Users\\me', '~/secrets', '../escape', 'a/../b']) {
      expect(classifyUnsafe('assetPath', value)).toBe(COMPLETION_GUIDANCE_CODES.UNBOUNDED_PATH);
    }
  });

  it('passes safe UE content paths, identifiers, and the empty prefix', () => {
    for (const value of ['asset.', '/Game/Heroes/BP_Hero', 'StaticMesh', '5.7', '']) {
      expect(classifyUnsafe('capabilityId', value)).toBeUndefined();
    }
  });
});
