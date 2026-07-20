import fs from 'node:fs';
import path from 'node:path';
import ts from 'typescript';
import {
  isGeneratedDefinitionsRoot,
  readRuntimeFacadeToolDefinitions
} from './parameter-audit-context.mjs';

function propertyName(node) {
  if (ts.isIdentifier(node.name) || ts.isStringLiteral(node.name) || ts.isNumericLiteral(node.name)) {
    return node.name.text;
  }
  return undefined;
}

function getProperty(objectLiteral, name) {
  for (const property of objectLiteral.properties) {
    if (!ts.isPropertyAssignment(property)) continue;
    if (propertyName(property) === name) return property.initializer;
  }
  return undefined;
}

function sourcePathsFromDefinitions(definitionsRoot) {
  const sourcePaths = [];
  const pendingDirectories = [definitionsRoot];

  while (pendingDirectories.length > 0) {
    const currentDirectory = pendingDirectories.pop();
    for (const entry of fs.readdirSync(currentDirectory, { withFileTypes: true })) {
      const entryPath = path.join(currentDirectory, entry.name);
      if (entry.isDirectory()) {
        pendingDirectories.push(entryPath);
      } else if (entry.name.endsWith('.ts')) {
        sourcePaths.push(entryPath);
      }
    }
  }

  return sourcePaths.sort();
}

function unwrapExpression(node) {
  let current = node;
  while (
    ts.isAsExpression(current)
    || ts.isParenthesizedExpression(current)
    || ts.isSatisfiesExpression(current)
    || ts.isTypeAssertionExpression(current)
  ) {
    current = current.expression;
  }
  return current;
}

function declarationInitializer(symbol) {
  const declaration = symbol.valueDeclaration
    ?? symbol.declarations?.find((candidate) => ts.isVariableDeclaration(candidate));
  return declaration && ts.isVariableDeclaration(declaration)
    ? declaration.initializer
    : undefined;
}

function resolveExpression(node, checker, resolving = new Set()) {
  const expression = unwrapExpression(node);
  if (!ts.isIdentifier(expression)) return expression;

  const locatedSymbol = checker.getSymbolAtLocation(expression);
  if (!locatedSymbol) return undefined;
  const symbol = locatedSymbol.flags & ts.SymbolFlags.Alias
    ? checker.getAliasedSymbol(locatedSymbol)
    : locatedSymbol;
  if (resolving.has(symbol)) return undefined;

  const initializer = declarationInitializer(symbol);
  if (!initializer) return undefined;
  const nextResolving = new Set(resolving);
  nextResolving.add(symbol);
  return resolveExpression(initializer, checker, nextResolving);
}

function stringArray(node, checker) {
  const resolved = resolveExpression(node, checker);
  if (!resolved || !ts.isArrayLiteralExpression(resolved)) return [];

  const values = [];
  for (const element of resolved.elements) {
    if (ts.isStringLiteral(element) || ts.isNoSubstitutionTemplateLiteral(element)) {
      values.push(element.text);
    } else if (ts.isSpreadElement(element)) {
      values.push(...stringArray(element.expression, checker));
    }
  }
  return [...new Set(values)].sort();
}

function collectProperties(propertiesNode, checker, resolving = new Set()) {
  const resolved = resolveExpression(propertiesNode, checker, resolving);
  if (!resolved || !ts.isObjectLiteralExpression(resolved)) return { names: [], actionEnum: [] };
  const names = [];
  let actionEnum = [];

  for (const property of resolved.properties) {
    if (ts.isSpreadAssignment(property)) {
      const spread = collectProperties(property.expression, checker, resolving);
      names.push(...spread.names);
      if (spread.names.includes('action')) actionEnum = spread.actionEnum;
      continue;
    }
    if (!ts.isPropertyAssignment(property)) continue;
    const name = propertyName(property);
    if (typeof name !== 'string') continue;
    names.push(name);
    if (name === 'action') {
      actionEnum = [];
      const action = resolveExpression(property.initializer, checker);
      if (action && ts.isObjectLiteralExpression(action)) {
        const enumNode = getProperty(action, 'enum');
        if (enumNode) actionEnum = stringArray(enumNode, checker);
      }
    }
  }

  return { names: [...new Set(names)].sort(), actionEnum };
}

function createDefinitionsProgram(definitionsRoot) {
  const sourcePaths = sourcePathsFromDefinitions(definitionsRoot);
  const program = ts.createProgram({
    rootNames: sourcePaths,
    options: {
      target: ts.ScriptTarget.ES2022,
      module: ts.ModuleKind.NodeNext,
      moduleResolution: ts.ModuleResolutionKind.NodeNext,
      skipLibCheck: true
    }
  });
  const sourceFiles = sourcePaths
    .map((filePath) => program.getSourceFile(filePath))
    .filter((sourceFile) => sourceFile !== undefined);
  return { program, sourceFiles };
}

function toolDefinitionDeclarations(sourceFiles) {
  const declarations = [];
  for (const sourceFile of sourceFiles) {
    for (const statement of sourceFile.statements) {
      if (!ts.isVariableStatement(statement)) continue;
      for (const declaration of statement.declarationList.declarations) {
        if (
          ts.isIdentifier(declaration.name)
          && declaration.name.text.endsWith('ToolDefinition')
          && declaration.initializer
        ) {
          declarations.push(declaration);
        }
      }
    }
  }
  return declarations;
}

function sortedStrings(values) {
  return Array.isArray(values)
    ? [...new Set(values.filter((value) => typeof value === 'string'))].sort()
    : [];
}

function schemaFromRuntimeFacade(definition) {
  const properties = definition.inputSchema?.properties ?? {};
  return {
    name: definition.name,
    actions: sortedStrings(properties['action']?.enum),
    properties: Object.keys(properties).sort(),
    required: sortedStrings(definition.inputSchema?.required)
  };
}

function toolSchemasFromSource(definitionsRoot) {
  const { program, sourceFiles } = createDefinitionsProgram(definitionsRoot);
  const checker = program.getTypeChecker();
  const tools = [];

  for (const declaration of toolDefinitionDeclarations(sourceFiles)) {
    const initializer = resolveExpression(declaration.initializer, checker);
    if (!initializer || !ts.isObjectLiteralExpression(initializer)) continue;
    const nameNode = getProperty(initializer, 'name');
    if (!nameNode || !ts.isStringLiteral(nameNode)) continue;
    const inputSchemaNode = getProperty(initializer, 'inputSchema');
    const inputSchema = inputSchemaNode
      ? resolveExpression(inputSchemaNode, checker)
      : undefined;
    if (!inputSchema || !ts.isObjectLiteralExpression(inputSchema)) continue;
    const properties = getProperty(inputSchema, 'properties');
    if (!properties) continue;
    const required = getProperty(inputSchema, 'required');
    const collected = collectProperties(properties, checker);

    tools.push({
      name: nameNode.text,
      actions: collected.actionEnum,
      properties: collected.names,
      required: required ? stringArray(required, checker) : []
    });
  }

  return tools;
}

/**
 * Real-repo mode projects the generated runtime facade, which is the surface the
 * server actually exposes. Explicit fixture roots keep the compiler-AST parser,
 * which the schema-discovery fixtures exercise directly.
 *
 * The facade loader fails closed on an empty artifact, so a missing or emptied
 * generated surface raises instead of silently reporting zero coverage.
 */
export function extractToolSchemas(config = {}) {
  const tools = isGeneratedDefinitionsRoot(config.definitionsRoot)
    ? readRuntimeFacadeToolDefinitions().map(schemaFromRuntimeFacade)
    : toolSchemasFromSource(config.definitionsRoot);

  return tools.sort((left, right) => left.name.localeCompare(right.name));
}
