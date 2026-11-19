// An asset this server creates carries RF_Transactional, as the editor's own asset creation gives it. Without it the
// asset's Modify() records nothing, so no later edit of it (set_property, a material parameter, a graph change) could be
// undone, and the flags are saved with the asset, so it stayed that way after a restart.

import { readFileSync, readdirSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const PRIVATE = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private');
const PUBLIC_STANDALONE = /RF_Public\s*\|\s*RF_Standalone(?!\s*\|\s*RF_Transactional)/gu;

describe('assets this server creates are transactional', () => {
  it('a flag set of RF_Public | RF_Standalone also holds RF_Transactional outside tests', () => {
    const missing: string[] = [];
    for (const file of readdirSync(PRIVATE, { recursive: true, encoding: 'utf8' })) {
      if (!/\.(cpp|h)$/u.test(file) || /(^|[\\/])Tests[\\/]|Tests\.cpp$/u.test(file)) continue;
      const source = readFileSync(join(PRIVATE, file), 'utf8').replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');
      for (const match of source.matchAll(PUBLIC_STANDALONE)) {
        const before = source.slice(Math.max(0, match.index - 40), match.index);
        if (/(ClearFlags|HasAnyFlags)\(\s*$/u.test(before)) continue; // clearing or testing the flags creates nothing
        missing.push(`${file}: ${source.slice(Math.max(0, match.index - 60), match.index + 30).replace(/\s+/gu, ' ')}`);
      }
    }
    expect(missing).toEqual([]);
  });
});
