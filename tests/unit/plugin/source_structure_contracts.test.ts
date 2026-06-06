import {
  readFileSync,
  readdirSync,
} from 'node:fs';
import {
  basename,
  resolve,
} from 'node:path';

import {
  describe,
  expect,
  it,
} from 'vitest';

const pluginSourceRoot = resolve(
  process.cwd(),
  'plugins/McpAutomationBridge/Source/McpAutomationBridge',
);
const sourceExtensionPattern = /\.(?:cpp|cs|h)$/u;
const splitArtifactPattern =
  /Common.*\.(?:cpp|cs|h)$|(?:^|[_-])Part(?:[_-]?\d+)?\.(?:cpp|cs|h)$|(?:^|[_-])\d+\.(?:cpp|cs|h)$|\.in[cl]$/u;

const listFiles = (directory: string): readonly string[] => {
  const files: string[] = [];

  for (const entry of readdirSync(directory, { withFileTypes: true })) {
    const entryPath = resolve(directory, entry.name);
    if (entry.isDirectory()) {
      files.push(...listFiles(entryPath));
    } else {
      files.push(entryPath);
    }
  }

  return files;
};

const countPureLines = (source: string): number =>
  source
    .split(/\r?\n/u)
    .filter((line) => !/^\s*$/u.test(line) && !/^\s*(?:#|\/\/)/u.test(line))
    .length;

describe('plugin source structure contracts', () => {
  it('keeps handwritten C++ and C# files within 250 pure lines', () => {
    // Given
    const sourceFiles = listFiles(pluginSourceRoot).filter((file) =>
      sourceExtensionPattern.test(file),
    );

    // When
    const oversizedFiles = sourceFiles
      .map((file) => ({
        file,
        pureLines: countPureLines(readFileSync(file, 'utf8')),
      }))
      .filter(({ pureLines }) => pureLines > 250)
      .map(({ file, pureLines }) => `${pureLines} ${file}`);

    // Then
    expect(oversizedFiles).toEqual([]);
  });

  it('rejects catch-all and mechanical split artifacts', () => {
    // Given
    const sourceFiles = listFiles(pluginSourceRoot);

    // When
    const splitArtifacts = sourceFiles
      .filter((file) => splitArtifactPattern.test(basename(file)))
      .map((file) => basename(file))
      .sort();

    // Then
    expect(splitArtifacts).toEqual([]);
  });

  it('resolves every local Mcp include to a source file', () => {
    // Given
    const sourceFiles = listFiles(pluginSourceRoot);
    const sourceNames = new Set(sourceFiles.map((file) => basename(file)));

    // When
    const missingIncludes = sourceFiles
      .filter((file) => sourceExtensionPattern.test(file))
      .flatMap((file) => {
        const source = readFileSync(file, 'utf8');
        return [...source.matchAll(/^\s*#include\s+"([^"]+)"/gmu)]
          .map((match) => basename(match[1] ?? ''))
          .filter(
            (includeName) =>
              includeName.startsWith('Mcp') &&
              !includeName.endsWith('.generated.h') &&
              !sourceNames.has(includeName),
          )
          .map((includeName) => `${file}: ${includeName}`);
      });

    // Then
    expect(missingIncludes).toEqual([]);
  });
});
