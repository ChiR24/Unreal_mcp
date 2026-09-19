import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

const read = (rel: string): string =>
  readFileSync(resolve(process.cwd(), rel), 'utf8');

// Release hygiene: the plugin archive must never ship generated build dirs
// (Binaries/Intermediate/Saved) which can leak local paths and object files.
// This test pins both the release.yml archive step and the package-plugin.sh
// packaging script to exclude them.
describe('release archive excludes generated build dirs', () => {
  const releaseYml = read('.github/workflows/release.yml');
  const packageScript = read('scripts/package-plugin.sh');

  it('release.yml excludes Binaries/Intermediate/Saved from the plugin tar/zip', () => {
    expect(releaseYml).toContain('plugins/McpAutomationBridge/Binaries');
    expect(releaseYml).toContain('plugins/McpAutomationBridge/Intermediate');
    expect(releaseYml).toContain('plugins/McpAutomationBridge/Saved');
    // Verification step rejects any leaked generated dir.
    expect(releaseYml).toContain('contains generated build dirs');
  });

  it('package-plugin.sh excludes Binaries/Intermediate/Saved from the zip', () => {
    expect(packageScript).toContain('McpAutomationBridge/Binaries/*');
    expect(packageScript).toContain('McpAutomationBridge/Intermediate/*');
    expect(packageScript).toContain('McpAutomationBridge/Saved/*');
  });

  it('never archives a generated build dir without an exclude on the same command', () => {
    const offenders: string[] = [];
    for (const [name, text] of [
      ['release.yml', releaseYml],
      ['package-plugin.sh', packageScript],
    ] as const) {
      for (const line of text.split(/\r?\n/u)) {
        if (!/\b(?:tar|zip)\b/i.test(line)) continue;
        if (!/Binaries|Intermediate|Saved/.test(line)) continue;
        if (/--exclude|-x\s/.test(line)) continue;
        offenders.push(`${name}: ${line.trim()}`);
      }
    }

    expect(offenders).toEqual([]);
  });
});
