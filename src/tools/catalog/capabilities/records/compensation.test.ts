import { describe, expect, it } from 'vitest';
import { ALL_CAPABILITY_RECORDS } from './aggregate.js';
import { COMPENSATION } from './compensation.js';

describe('compensation', () => {
  const ids = new Set<string>(ALL_CAPABILITY_RECORDS.map((record) => String(record.id)));

  it('keys and inverses name live records, never the key itself', () => {
    for (const [key, compensation] of COMPENSATION) {
      expect(ids.has(key), key).toBe(true);
      for (const target of 'inverse' in compensation ? compensation.inverse : []) {
        expect(ids.has(target), `${key} -> ${target}`).toBe(true);
        expect(target).not.toBe(key);
      }
    }
  });
});
