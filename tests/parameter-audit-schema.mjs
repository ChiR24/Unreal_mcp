import fs from 'node:fs';
import path from 'node:path';
import ts from 'typescript';
import { definitionsRoot } from './parameter-audit-context.mjs';

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

function stringArrayFromArrayLiteral(arrayLiteral, constants) {
  const values = [];
  for (const element of arrayLiteral.elements) {
    if (ts.isStringLiteral(element)) {
      values.push(element.text);
      continue;
    }
    if (ts.isSpreadElement(element) && ts.isIdentifier(element.expression)) {
      values.push(...(constants.get(element.expression.text) ?? []));
    }
  }
  return [...new Set(values)].sort();
}

function extractActionConstants(sourceFile) {
  const constants = new Map();
  for (const statement of sourceFile.statements) {
    if (!ts.isVariableStatement(statement)) continue;
    for (const declaration of statement.declarationList.declarations) {
      if (!ts.isIdentifier(declaration.name) || !declaration.name.text.endsWith('_ACTIONS')) continue;
      if (!declaration.initializer) continue;
      const initializer = ts.isAsExpression(declaration.initializer) ? declaration.initializer.expression : declaration.initializer;
      if (!ts.isArrayLiteralExpression(initializer)) continue;
      constants.set(declaration.name.text, stringArrayFromArrayLiteral(initializer, constants));
    }
  }
  return constants;
}

function sourceFilesFromDefinitions() {
  return fs.readdirSync(definitionsRoot)
    .filter((entry) => entry.endsWith('.ts'))
    .map((entry) => path.join(definitionsRoot, entry))
    .sort()
    .map((filePath) => ts.createSourceFile(
      filePath,
      fs.readFileSync(filePath, 'utf8'),
      ts.ScriptTarget.Latest,
      true,
      ts.ScriptKind.TS
    ));
}

function collectVariableInitializers(sourceFiles, constants) {
  const variableInitializers = new Map();
  for (const sourceFile of sourceFiles) {
    for (const [name, values] of extractActionConstants(sourceFile)) {
      constants.set(name, values);
    }
    for (const statement of sourceFile.statements) {
      if (!ts.isVariableStatement(statement)) continue;
      for (const declaration of statement.declarationList.declarations) {
        if (ts.isIdentifier(declaration.name) && declaration.initializer) {
          variableInitializers.set(declaration.name.text, declaration.initializer);
        }
      }
    }
  }
  return variableInitializers;
}

function collectProperties(propertiesNode, resolveInitializer, constants) {
  const resolved = resolveInitializer(propertiesNode);
  if (!resolved || !ts.isObjectLiteralExpression(resolved)) return { names: [], actionEnum: [] };
  const names = [];
  let actionEnum = [];

  for (const property of resolved.properties) {
    if (ts.isSpreadAssignment(property) && ts.isIdentifier(property.expression)) {
      const spread = collectProperties(property.expression, resolveInitializer, constants);
      names.push(...spread.names);
      if (spread.actionEnum.length > 0) actionEnum = spread.actionEnum;
      continue;
    }
    if (!ts.isPropertyAssignment(property)) continue;
    const name = propertyName(property);
    if (typeof name !== 'string') continue;
    names.push(name);
    if (name === 'action' && ts.isObjectLiteralExpression(property.initializer)) {
      const enumNode = getProperty(property.initializer, 'enum');
      if (enumNode && ts.isArrayLiteralExpression(enumNode)) {
        actionEnum = stringArrayFromArrayLiteral(enumNode, constants);
      }
    }
  }

  return { names: [...new Set(names)].sort(), actionEnum };
}

export function extractToolSchemas() {
  const sourceFiles = sourceFilesFromDefinitions();
  const constants = new Map();
  const variableInitializers = collectVariableInitializers(sourceFiles, constants);
  const resolveInitializer = (node) => (ts.isIdentifier(node) ? variableInitializers.get(node.text) : node);
  const tools = [];

  for (const [variableName, initializer] of variableInitializers) {
    if (!variableName.endsWith('ToolDefinition')) continue;
    if (!ts.isObjectLiteralExpression(initializer)) continue;
    const nameNode = getProperty(initializer, 'name');
    if (!nameNode || !ts.isStringLiteral(nameNode)) continue;
    const inputSchemaNode = getProperty(initializer, 'inputSchema');
    const inputSchema = inputSchemaNode ? resolveInitializer(inputSchemaNode) : undefined;
    if (!inputSchema || !ts.isObjectLiteralExpression(inputSchema)) continue;
    const properties = getProperty(inputSchema, 'properties');
    if (!properties) continue;
    const required = getProperty(inputSchema, 'required');
    const collected = collectProperties(properties, resolveInitializer, constants);

    tools.push({
      name: nameNode.text,
      actions: collected.actionEnum,
      properties: collected.names,
      required: required && ts.isArrayLiteralExpression(required) ? stringArrayFromArrayLiteral(required, constants) : []
    });
  }

  return tools.sort((left, right) => left.name.localeCompare(right.name));
}
