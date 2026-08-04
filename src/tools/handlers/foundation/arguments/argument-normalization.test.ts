import { describe, expect, it } from 'vitest';

import { createActionNormalizer } from './argument-normalization.js';

describe('createActionNormalizer', () => {
  describe('alias resolution', () => {
    it('returns the mapped value for an exact alias key match', () => {
      const normalizer = createActionNormalizer({ spawn_actor: 'spawn' });

      expect(normalizer('spawn_actor')).toBe('spawn');
    });

    it('returns the mapped value when the input matches an alias key case-insensitively (lowercase fallback)', () => {
      const normalizer = createActionNormalizer({ spawn_actor: 'spawn' });

      // lowercase fallback: 'SPAWN_ACTOR'.toLowerCase() === 'spawn_actor' matches
      expect(normalizer('SPAWN_ACTOR')).toBe('spawn');
    });

    it('returns the input unchanged when the action is not in the aliases (no match)', () => {
      const normalizer = createActionNormalizer({ spawn_actor: 'spawn' });

      expect(normalizer('unknown_action')).toBe('unknown_action');
    });
  });

  describe('value preservation', () => {
    it('returns the mapped value verbatim (no re-lowering of the mapped value)', () => {
      // The map value is returned exactly — not re-processed through lowercase lookup
      const normalizer = createActionNormalizer({ 'MyAction': 'MixedCase_Value' });

      expect(normalizer('MyAction')).toBe('MixedCase_Value');
    });

    it('returns the input unchanged when the lowercase form is also not a key', () => {
      const normalizer = createActionNormalizer({ spawn_actor: 'spawn' });

      // 'unknown' is not a key; 'unknown'.toLowerCase() is also not a key → returns unchanged
      expect(normalizer('unknown')).toBe('unknown');
    });
  });

  describe('factory isolation', () => {
    it('returns a new function per call (no shared state between normalizers)', () => {
      const normalizer1 = createActionNormalizer({ foo: 'bar' });
      const normalizer2 = createActionNormalizer({ foo: 'baz' });

      expect(normalizer1).not.toBe(normalizer2);
      expect(normalizer1('foo')).toBe('bar');
      expect(normalizer2('foo')).toBe('baz');
    });
  });
});