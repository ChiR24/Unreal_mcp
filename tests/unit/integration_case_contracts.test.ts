import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

describe('canonical integration case contracts', () => {
  it('allows destructive asset cleanup to finish dependency-safe deletion', () => {
    const source = readFileSync(
      resolve(process.cwd(), 'tests/integration.mjs'),
      'utf8'
    );

    for (const scenario of [
      'Cleanup: delete test folder',
      'Cleanup: delete advanced test folder'
    ]) {
      const caseLine = source
        .split('\n')
        .find((line) => line.includes(`scenario: '${scenario}'`));

      expect(caseLine).toBeDefined();
      expect(caseLine).toMatch(/timeoutMs:\s*(?:[2-9]\d{4}|\d{6,})/);
    }
  });
});
