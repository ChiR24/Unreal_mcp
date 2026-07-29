import { readFileSync, readdirSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

const domainsRoot = resolve(
  process.cwd(),
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains',
);

const migratedDomains = [
  'AI',
  'AnimationAuthoring',
  'Combat',
  'GAS',
  'Geometry',
  'Skeleton',
] as const;

const retiredAliases = [
  'GetStringFieldAI',
  'GetNumberFieldAI',
  'GetBoolFieldAI',
  'GetStringFieldGeom',
  'GetNumberFieldGeom',
  'GetBoolFieldGeom',
  'GetIntFieldGeom',
  'GetStringFieldSkel',
  'GetNumberFieldSkel',
  'GetBoolFieldSkel',
  'GetStringFieldAnimAuth',
  'GetNumberFieldAnimAuth',
  'GetBoolFieldAnimAuth',
  'GetStringFieldCombat',
  'GetNumberFieldCombat',
  'GetBoolFieldCombat',
  'GetStringFieldGAS',
  'GetNumberFieldGAS',
  'GetBoolFieldGAS',
] as const;

const listSourceFiles = (directory: string): readonly string[] => {
  const files: string[] = [];
  for (const entry of readdirSync(directory, { withFileTypes: true })) {
    const entryPath = resolve(directory, entry.name);
    if (entry.isDirectory()) {
      files.push(...listSourceFiles(entryPath));
    } else if (/\.(?:cpp|h)$/u.test(entry.name)) {
      files.push(entryPath);
    }
  }
  return files;
};

const readDomain = (domain: string): string =>
  listSourceFiles(resolve(domainsRoot, domain))
    .map((file) => readFileSync(file, 'utf8'))
    .join('\n');

describe('domain JSON helper alias contracts', () => {
  it('keeps retired aliases out of migrated domains', () => {
    const source = migratedDomains.map(readDomain).join('\n');
    const remaining = retiredAliases.filter((alias) => source.includes(alias));

    expect(remaining).toEqual([]);
  });

  it('preserves the specialized Skeleton integer helper', () => {
    const source = readDomain('Skeleton');

    expect(source).toContain('GetIntFieldSkel');
    expect(source).not.toContain('#define GetIntFieldSkel');
  });

  it('leaves the Texture authoring aliases outside migration scope', () => {
    const source = readDomain('Texture');

    expect(source).toContain('#define GetStringFieldTextAuth');
    expect(source).toContain('#define GetNumberFieldTextAuth');
    expect(source).toContain('#define GetBoolFieldTextAuth');
  });
});
