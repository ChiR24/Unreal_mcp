import { describe, it, expect } from 'vitest';
import { oneLine } from './logger.js';

describe('oneLine', () => {
  it('keeps one log call to one record: line breaks and other control characters are escaped, tabs kept', () => {
    expect(oneLine('Unreal\r\n[NativeProxy] forged')).toBe('Unreal\\u000d\\u000a[NativeProxy] forged');
    expect(oneLine('a\tb\u007f')).toBe('a\tb\\u007f');
  });
});
