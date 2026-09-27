import { describe, expect, it } from 'vitest';
import { getParentToolMetadata } from './parent-metadata.js';

describe('parent-metadata lookup', () => {
  it('throws on a non-canonical parent tool', () => {
    expect(() => getParentToolMetadata('not_a_real_tool')).toThrow();
  });
});
