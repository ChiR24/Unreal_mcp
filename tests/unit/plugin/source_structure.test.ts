import {
  existsSync,
  readFileSync,
  readdirSync,
} from 'node:fs';
import {
  extname,
  resolve,
} from 'node:path';

import {
  describe,
  expect,
  it,
} from 'vitest';

const privateRoot = resolve(
  process.cwd(),
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private',
);
const sourceExtensions = new Set(['.cpp', '.h']);
const privateIncludePrefixes = [
  'Core/',
  'Domains/',
  'Foundation/',
  'MCP/',
  'Safety/',
  'Transport/',
  'UI/',
] as const;

const listDirectories = (directory: string): readonly string[] =>
  readdirSync(directory, { withFileTypes: true }).flatMap((entry) => {
    if (!entry.isDirectory()) {
      return [];
    }

    const entryPath = resolve(directory, entry.name);
    return [entryPath, ...listDirectories(entryPath)];
  });

const listSourceFiles = (directory: string): readonly string[] =>
  readdirSync(directory, { withFileTypes: true }).flatMap((entry) => {
    const entryPath = resolve(directory, entry.name);
    if (entry.isDirectory()) {
      return listSourceFiles(entryPath);
    }

    return sourceExtensions.has(extname(entry.name)) ? [entryPath] : [];
  });

describe('plugin source structure', () => {
  it('keeps implementation files out of the private source root', () => {
    const rootSourceFiles = readdirSync(privateRoot, { withFileTypes: true })
      .filter(
        (entry) =>
          entry.isFile() && sourceExtensions.has(extname(entry.name)),
      )
      .map((entry) => entry.name);

    expect(rootSourceFiles).toEqual([]);
  });

  it('keeps responsibility folders within a reviewable file count', () => {
    const overloadedDirectories = listDirectories(privateRoot).flatMap(
      (directory) => {
        const sourceFileCount = readdirSync(directory, {
          withFileTypes: true,
        }).filter(
          (entry) =>
            entry.isFile() && sourceExtensions.has(extname(entry.name)),
        ).length;

        return sourceFileCount > 25
          ? [`${directory}: ${sourceFileCount}`]
          : [];
      },
    );

    expect(overloadedDirectories).toEqual([]);
  });

  it('resolves every module-relative private include', () => {
    const unresolvedIncludes = listSourceFiles(privateRoot).flatMap((file) => {
      const source = readFileSync(file, 'utf8');
      return [...source.matchAll(/^\s*#include\s+"([^"]+)"/gmu)]
        .map((match) => match[1])
        .filter(
          (includePath): includePath is string =>
            includePath !== undefined &&
            privateIncludePrefixes.some((prefix) =>
              includePath.startsWith(prefix),
            ),
        )
        .filter((includePath) => !existsSync(resolve(privateRoot, includePath)))
        .map((includePath) => `${file}: ${includePath}`);
    });

    expect(unresolvedIncludes).toEqual([]);
  }, 60_000);
});
