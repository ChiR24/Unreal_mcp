import { describe, expect, it } from 'vitest';
import { getPrompt } from './prompts.js';

describe('getPrompt arguments', () => {
  it('refuses a value outside the declared allowed set', () => {
    expect(() => getPrompt('asset-import', { destinationPath: '/Game/Props', sourceFormat: 'exe' })).toThrow(/must be one of/);
    expect(() => getPrompt('asset-import', { destinationPath: '/Game/Props', sourceFormat: 'fbx' })).not.toThrow();
  });

  it('refuses an oversized value', () => {
    expect(() => getPrompt('inspect-fix', { objectPath: 'x'.repeat(513) })).toThrow(/longer than 512/);
  });
});
