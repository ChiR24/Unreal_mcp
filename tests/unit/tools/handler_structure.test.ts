import {
  readdirSync,
} from 'node:fs';
import {
  resolve,
} from 'node:path';

import {
  describe,
  expect,
  it,
} from 'vitest';

const handlerRoot = resolve(process.cwd(), 'src/tools/handlers');

const listDirectories = (directory: string): readonly string[] =>
  readdirSync(directory, { withFileTypes: true }).flatMap((entry) => {
    if (!entry.isDirectory()) {
      return [];
    }

    const entryPath = resolve(directory, entry.name);
    return [entryPath, ...listDirectories(entryPath)];
  });

describe('tool handler source structure', () => {
  it('keeps implementation files out of the handler root', () => {
    // Given
    const entries = readdirSync(handlerRoot, { withFileTypes: true });

    // When
    const rootTypeScriptFiles = entries
      .filter((entry) => entry.isFile() && entry.name.endsWith('.ts'))
      .map((entry) => entry.name)
      .sort();

    // Then
    // Every handler lives under a responsibility folder. The root previously
    // held a re-export barrel; it had no importers, so the root is now empty of
    // TypeScript entirely and any new file here is a layering regression.
    expect(rootTypeScriptFiles).toEqual([]);
  });

  it('keeps each responsibility folder within a reviewable file count', () => {
    // Given
    const directories = listDirectories(handlerRoot);

    // When
    const overloadedDirectories = directories.flatMap((directory) => {
      const typeScriptFiles = readdirSync(directory, { withFileTypes: true })
        .filter((entry) => entry.isFile() && entry.name.endsWith('.ts'))
        .map((entry) => entry.name);

      return typeScriptFiles.length > 25
        ? [`${directory}: ${typeScriptFiles.length}`]
        : [];
    });

    // Then
    expect(overloadedDirectories).toEqual([]);
  });
});
