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

  it('neither archive references forbidden generated paths as inclusions', () => {
    for (const text of [releaseYml, packageScript]) {
      // The only mentions of the generated dirs must be in exclude/verify context.
      const bannedInclude = /(tar|zip)[^\n]*(Binaries|Intermediate|Saved)/i.exec(text);
      // If a line mentions tar/zip AND a generated dir, it must also carry an exclude flag.
      if (bannedInclude) {
        expect(/--exclude|-x\s/.test(bannedInclude[0]) || text.includes('contains generated build dirs')).toBe(
          true,
        );
      }
    }
  });
});
