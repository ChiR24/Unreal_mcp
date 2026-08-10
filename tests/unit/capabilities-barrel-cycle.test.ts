import { readFileSync } from 'node:fs';
import { dirname, relative, resolve } from 'node:path';
import ts from 'typescript';
import { describe, expect, it } from 'vitest';

const CAPABILITIES_ROOT = resolve(process.cwd(), 'src/tools/catalog/capabilities');
const BARREL_PATH = resolve(CAPABILITIES_ROOT, 'index.ts');

function isTypeOnlyImport(node: ts.ImportDeclaration): boolean {
  const clause = node.importClause;
  if (clause === undefined || clause.isTypeOnly) return clause?.isTypeOnly ?? false;
  if (clause.name !== undefined || clause.namedBindings === undefined) return false;
  return ts.isNamedImports(clause.namedBindings)
    && clause.namedBindings.elements.every((element) => element.isTypeOnly);
}

function isTypeOnlyExport(node: ts.ExportDeclaration): boolean {
  if (node.isTypeOnly) return true;
  return node.exportClause !== undefined
    && ts.isNamedExports(node.exportClause)
    && node.exportClause.elements.every((element) => element.isTypeOnly);
}

function resolveLocalModule(importer: string, specifier: string): string | undefined {
  if (!specifier.startsWith('.')) return undefined;
  return resolve(dirname(importer), specifier.replace(/\.js$/u, '.ts'));
}

function valueDependencies(filePath: string): readonly string[] {
  const source = readFileSync(filePath, 'utf8');
  const sourceFile = ts.createSourceFile(filePath, source, ts.ScriptTarget.Latest, true);
  const dependencies: string[] = [];

  for (const statement of sourceFile.statements) {
    if (ts.isImportDeclaration(statement) && !isTypeOnlyImport(statement)) {
      const specifier = ts.isStringLiteral(statement.moduleSpecifier)
        ? statement.moduleSpecifier.text
        : undefined;
      const dependency = specifier === undefined ? undefined : resolveLocalModule(filePath, specifier);
      if (dependency !== undefined) dependencies.push(dependency);
    }
    if (ts.isExportDeclaration(statement) && !isTypeOnlyExport(statement)) {
      const specifier = statement.moduleSpecifier !== undefined
        && ts.isStringLiteral(statement.moduleSpecifier)
        ? statement.moduleSpecifier.text
        : undefined;
      const dependency = specifier === undefined ? undefined : resolveLocalModule(filePath, specifier);
      if (dependency !== undefined) dependencies.push(dependency);
    }
  }

  return dependencies;
}

function findCycle(filePath: string, path: readonly string[]): readonly string[] | undefined {
  if (path.includes(filePath)) return [...path.slice(path.indexOf(filePath)), filePath];
  for (const dependency of valueDependencies(filePath)) {
    const cycle = findCycle(dependency, [...path, filePath]);
    if (cycle !== undefined) return cycle;
  }
  return undefined;
}

describe('capabilities barrel topology', () => {
  it('has no value-import cycle from the capabilities barrel', () => {
    // Given: the authored value-import graph rooted at the capabilities barrel.
    // When: the graph is traversed recursively.
    const cycle = findCycle(BARREL_PATH, []);

    // Then: no value edge returns to a module already on the active path.
    expect(cycle?.map((filePath) => relative(CAPABILITIES_ROOT, filePath))).toBeUndefined();
  });
});
