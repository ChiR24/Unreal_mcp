import {
  readdirSync,
  readFileSync,
} from 'node:fs';
import {
  relative,
  resolve,
} from 'node:path';

import {
  describe,
  expect,
  it,
} from 'vitest';

const sourceRoots = [
  {
    directory: resolve(process.cwd(), 'src/tools'),
    allowedTypeScriptFiles: [],
  },
  {
    directory: resolve(process.cwd(), 'src/tools/definitions'),
    allowedTypeScriptFiles: [],
  },
  {
    directory: resolve(process.cwd(), 'src/utils'),
    allowedTypeScriptFiles: ['index.ts'],
  },
  {
    directory: resolve(process.cwd(), 'src/types'),
    allowedTypeScriptFiles: ['index.ts'],
  },
] as const;

const repositoryRoot = resolve(process.cwd());
const activeCodeRoots = [
  resolve(repositoryRoot, 'src'),
  resolve(repositoryRoot, 'plugins/McpAutomationBridge/Source'),
  resolve(repositoryRoot, 'scripts'),
  resolve(repositoryRoot, 'tests'),
] as const;
const ignoredDirectoryNames = new Set([
  'Binaries',
  'Intermediate',
  'node_modules',
  'reports',
  'evidence',
]);
const activeCodeExtensions = new Set([
  '.c',
  '.cc',
  '.cpp',
  '.cs',
  '.cjs',
  '.h',
  '.hpp',
  '.js',
  '.json',
  '.mjs',
  '.sh',
  '.ts',
  '.tsx',
  '.yaml',
  '.yml',
]);
const forbiddenNumberedPrefix = String.fromCharCode(112, 104, 97, 115, 101);
const forbiddenNumberedIdentifier = new RegExp(
  `(?:is)?${forbiddenNumberedPrefix}[ _-]*\\d+`,
  'i',
);

// The sibling vocabulary, policed on PATHS only. A plan numbers its work items and
// those numbers used to reach the tree: 213 files and 16 directories were named for
// a task rather than for what they test, so `progress-on-the-wire` lived under
// `task-44` and told a reader nothing once the plan was gone. Paths are clean now
// and this keeps them clean. Contents are deliberately NOT policed here - 439 files
// still carry a task number in a comment, and a gate that fails on all of them
// would be switched off rather than satisfied.
const forbiddenTaskPrefix = String.fromCharCode(116, 97, 115, 107);
const forbiddenTaskPath = new RegExp(`${forbiddenTaskPrefix}[ _-]*\\d+`, 'i');

const listDirectories = (directory: string): readonly string[] =>
  readdirSync(directory, { withFileTypes: true }).flatMap((entry) => {
    if (!entry.isDirectory()) {
      return [];
    }

    const entryPath = resolve(directory, entry.name);
    return [entryPath, ...listDirectories(entryPath)];
  });

const fileExtension = (fileName: string): string => {
  const extensionIndex = fileName.lastIndexOf('.');
  return extensionIndex >= 0 ? fileName.slice(extensionIndex) : '';
};

const listActiveCodeFiles = (directory: string): readonly string[] =>
  readdirSync(directory, { withFileTypes: true }).flatMap((entry) => {
    const entryPath = resolve(directory, entry.name);
    if (entry.isDirectory()) {
      return ignoredDirectoryNames.has(entry.name)
        ? []
        : listActiveCodeFiles(entryPath);
    }

    return activeCodeExtensions.has(fileExtension(entry.name))
      ? [entryPath]
      : [];
  });

describe('TypeScript source structure', () => {
  it('keeps implementation files out of organized source roots', () => {
    const unexpectedRootFiles = sourceRoots.flatMap(
      ({ directory, allowedTypeScriptFiles }) => {
        const allowedFiles = new Set<string>(allowedTypeScriptFiles);
        return readdirSync(directory, { withFileTypes: true })
          .filter(
            (entry) =>
              entry.isFile() &&
              entry.name.endsWith('.ts') &&
              !allowedFiles.has(entry.name),
          )
          .map((entry) => resolve(directory, entry.name));
      },
    );

    expect(unexpectedRootFiles).toEqual([]);
  });

  it('keeps responsibility folders within a reviewable file count', () => {
    const overloadedDirectories = sourceRoots.flatMap(({ directory }) =>
      listDirectories(directory).flatMap((nestedDirectory) => {
        const typeScriptFileCount = readdirSync(nestedDirectory, {
          withFileTypes: true,
        }).filter(
          (entry) => entry.isFile() && entry.name.endsWith('.ts'),
        ).length;

        return typeScriptFileCount > 25
          ? [`${nestedDirectory}: ${typeScriptFileCount}`]
          : [];
      }),
    );

    expect(overloadedDirectories).toEqual([]);
  });

  it('keeps numbered roadmap identifiers out of active code and filenames', () => {
    const activeFiles = activeCodeRoots.flatMap(listActiveCodeFiles);
    const invalidPaths = activeFiles
      .map((filePath) => relative(repositoryRoot, filePath))
      .filter((filePath) => forbiddenNumberedIdentifier.test(filePath));
    const invalidContents = activeFiles
      .filter((filePath) =>
        forbiddenNumberedIdentifier.test(readFileSync(filePath, 'utf8')),
      )
      .map((filePath) => relative(repositoryRoot, filePath));
    const invalidTaskPaths = activeFiles
      .map((filePath) => relative(repositoryRoot, filePath))
      .filter((filePath) => forbiddenTaskPath.test(filePath));

    expect({
      invalidContents,
      invalidPaths,
      invalidTaskPaths,
    }).toEqual({
      invalidContents: [],
      invalidPaths: [],
      invalidTaskPaths: [],
    });
  }, 60_000);
});
