import fs from 'node:fs';
import path from 'node:path';
import ts from 'typescript';

function sourcePaths(root) {
  return fs.readdirSync(root, { withFileTypes: true })
    .flatMap((entry) => {
      const entryPath = path.join(root, entry.name);
      return entry.isDirectory() ? sourcePaths(entryPath) : [entryPath];
    })
    .filter((filePath) => filePath.endsWith('.ts'))
    .sort();
}

function propertyName(property) {
  if (
    ts.isIdentifier(property.name)
    || ts.isStringLiteral(property.name)
    || ts.isNumericLiteral(property.name)
  ) {
    return property.name.text;
  }
  return undefined;
}

function unwrap(node) {
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

function symbolInitializer(symbol) {
  const declaration = symbol.valueDeclaration ?? symbol.declarations?.[0];
  if (declaration && ts.isVariableDeclaration(declaration)) return declaration.initializer;
  if (declaration && ts.isPropertyAssignment(declaration)) return declaration.initializer;
  return undefined;
}

function resolveExpression(node, checker, resolving = new Set()) {
  const expression = unwrap(node);
  if (!ts.isIdentifier(expression) && !ts.isPropertyAccessExpression(expression)) {
    return expression;
  }

  const symbolNode = ts.isPropertyAccessExpression(expression) ? expression.name : expression;
  const located = checker.getSymbolAtLocation(symbolNode);
  if (!located) return undefined;
  const symbol = located.flags & ts.SymbolFlags.Alias
    ? checker.getAliasedSymbol(located)
    : located;
  if (resolving.has(symbol)) return undefined;

  const initializer = symbolInitializer(symbol);
  if (!initializer) return undefined;
  const nextResolving = new Set(resolving);
  nextResolving.add(symbol);
  return resolveExpression(initializer, checker, nextResolving);
}

function objectEntries(node, checker) {
  const resolved = resolveExpression(node, checker);
  if (!resolved || !ts.isObjectLiteralExpression(resolved)) return new Map();
  const entries = new Map();
  for (const property of resolved.properties) {
    if (ts.isSpreadAssignment(property)) {
      for (const [name, value] of objectEntries(property.expression, checker)) {
        entries.set(name, value);
      }
      continue;
    }
    if (!ts.isPropertyAssignment(property)) continue;
    const name = propertyName(property);
    if (name) entries.set(name, property.initializer);
  }
  return entries;
}

function stringValue(node, checker) {
  if (!node) return undefined;
  const resolved = resolveExpression(node, checker);
  return resolved && (ts.isStringLiteral(resolved) || ts.isNoSubstitutionTemplateLiteral(resolved))
    ? resolved.text
    : undefined;
}

function stringArray(node, checker) {
  if (!node) return undefined;
  const resolved = resolveExpression(node, checker);
  if (!resolved || !ts.isArrayLiteralExpression(resolved)) return undefined;
  const values = [];
  for (const element of resolved.elements) {
    if (ts.isStringLiteral(element) || ts.isNoSubstitutionTemplateLiteral(element)) {
      values.push(element.text);
    } else if (ts.isSpreadElement(element)) {
      values.push(...(stringArray(element.expression, checker) ?? []));
    }
  }
  return [...new Set(values)].sort();
}

function schemaFromExpression(node, checker) {
  const entries = objectEntries(node, checker);
  const typeNode = entries.get('type');
  const type = stringValue(typeNode, checker) ?? stringArray(typeNode, checker);
  const enumValues = stringArray(entries.get('enum'), checker);
  const required = stringArray(entries.get('required'), checker);
  const propertiesNode = entries.get('properties');
  const properties = propertiesNode
    ? Object.fromEntries(
      [...objectEntries(propertiesNode, checker)]
        .map(([name, value]) => [name, schemaFromExpression(value, checker)])
    )
    : undefined;
  const itemsNode = entries.get('items');
  return {
    ...(type !== undefined ? { type } : {}),
    ...(enumValues !== undefined ? { enum: enumValues } : {}),
    ...(required !== undefined ? { required } : {}),
    ...(properties !== undefined ? { properties } : {}),
    ...(itemsNode ? { items: schemaFromExpression(itemsNode, checker) } : {})
  };
}

function toolDeclarations(sourceFiles) {
  return sourceFiles.flatMap((sourceFile) =>
    sourceFile.statements.flatMap((statement) => {
      if (!ts.isVariableStatement(statement)) return [];
      return [...statement.declarationList.declarations].filter((declaration) =>
        ts.isIdentifier(declaration.name)
        && declaration.name.text.endsWith('ToolDefinition')
        && declaration.initializer
      );
    })
  );
}

export function extractTypeScriptSchemaMap(definitionsRoot) {
  const paths = sourcePaths(definitionsRoot);
  const program = ts.createProgram({
    rootNames: paths,
    options: {
      target: ts.ScriptTarget.ES2022,
      module: ts.ModuleKind.NodeNext,
      moduleResolution: ts.ModuleResolutionKind.NodeNext,
      skipLibCheck: true
    }
  });
  const checker = program.getTypeChecker();
  const sourceFiles = paths
    .map((filePath) => program.getSourceFile(filePath))
    .filter((sourceFile) => sourceFile !== undefined);
  const schemas = new Map();

  for (const declaration of toolDeclarations(sourceFiles)) {
    const definitionEntries = objectEntries(declaration.initializer, checker);
    const name = stringValue(definitionEntries.get('name'), checker);
    const inputSchema = definitionEntries.get('inputSchema');
    if (name && inputSchema) schemas.set(name, schemaFromExpression(inputSchema, checker));
  }
  return schemas;
}
