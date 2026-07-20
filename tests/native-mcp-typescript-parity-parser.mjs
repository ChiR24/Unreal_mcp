import fs from 'node:fs';
import path from 'node:path';
import {
  maskTypeScriptComments,
  maskTypeScriptLiteralsAndComments
} from './native-mcp-source-parser.mjs';
import { extractTypeScriptSchemaMap } from './audits/typescript-schema-parser.mjs';
import {
  isGeneratedDefinitionsRoot,
  readGeneratedParentToolDefinitions
} from './parameter-audit-context.mjs';

function recursiveFiles(root, predicate) {
  return fs.readdirSync(root, { withFileTypes: true })
    .flatMap((entry) => {
      const entryPath = path.join(root, entry.name);
      return entry.isDirectory() ? recursiveFiles(entryPath, predicate) : [entryPath];
    })
    .filter(predicate)
    .sort();
}

function sourceInsideMatch(source, match, openCharacter, closeCharacter) {
  if (match.index === undefined) return '';
  const openIndex = match.index + match[0].indexOf(openCharacter) + 1;
  const closeIndex = match.index + match[0].lastIndexOf(closeCharacter);
  return source.slice(openIndex, closeIndex);
}

function stringArrayFromEnumBody(enumBody, constants) {
  const values = [];
  const uncommentedBody = maskTypeScriptComments(enumBody);
  for (const match of uncommentedBody.matchAll(/'([^']+)'|\.\.\.([A-Z0-9_]+_ACTIONS)/g)) {
    if (match[1]) {
      values.push(match[1]);
    } else if (match[2]) {
      values.push(...(constants.get(match[2]) ?? []));
    }
  }
  return [...new Set(values)].sort();
}

function extractActionConstants(source) {
  const constants = new Map();
  const maskedSource = maskTypeScriptLiteralsAndComments(source);
  for (const match of maskedSource.matchAll(
    /export const ([A-Z0-9_]+_ACTIONS)\s*=\s*\[[\s\S]*?\]\s+as const/g
  )) {
    const enumBody = sourceInsideMatch(source, match, '[', ']');
    constants.set(match[1], stringArrayFromEnumBody(enumBody, constants));
  }
  return constants;
}

function actionEnumsFromSource(source, constants) {
  const maskedSource = maskTypeScriptLiteralsAndComments(source);
  return [...maskedSource.matchAll(
    /action:\s*\{[\s\S]*?enum:\s*\[[\s\S]*?\]\s*,\s*description:/g
  )].map((match) => {
    const enumBody = sourceInsideMatch(source, match, '[', ']');
    return stringArrayFromEnumBody(enumBody, constants);
  });
}

function selectUnambiguousActions(toolName, candidates) {
  const uniqueCandidates = new Map(
    candidates.map((actions) => [JSON.stringify(actions), actions])
  );
  if (uniqueCandidates.size > 1) {
    throw new Error(
      `Ambiguous TypeScript action enums for ${toolName}: ` +
      [...uniqueCandidates.keys()].join(', ')
    );
  }
  return uniqueCandidates.values().next().value ?? [];
}

function activeToolName(source) {
  const maskedSource = maskTypeScriptLiteralsAndComments(source);
  return [...source.matchAll(/name:\s*'([^']+)'/g)]
    .find((match) => match.index !== undefined
      && maskedSource.slice(match.index, match.index + 'name'.length) === 'name')?.[1];
}

function generatedParityTools() {
  return readGeneratedParentToolDefinitions().map((definition) => {
    const inputSchema = definition.inputSchema ?? { type: 'object', properties: {}, required: [] };
    const actionEnum = inputSchema.properties?.['action']?.enum;
    return {
      name: definition.name,
      actions: Array.isArray(actionEnum) ? [...new Set(actionEnum)].sort() : [],
      schema: inputSchema
    };
  });
}

function sourceParityTools(paths) {
  const schemas = extractTypeScriptSchemaMap(paths.tsDefinitionsRoot);
  const actionSource = fs.readFileSync(
    path.join(paths.tsDefinitionsRoot, 'shared', 'action-sets.ts'),
    'utf8'
  );
  const constants = extractActionConstants(actionSource);
  const toolFiles = recursiveFiles(
    paths.tsDefinitionsRoot,
    (filePath) => filePath.endsWith('-tool.ts')
  );
  const tools = [];

  for (const filePath of toolFiles) {
    const source = fs.readFileSync(filePath, 'utf8');
    const name = activeToolName(source);
    if (!name) continue;
    const definitionDir = path.dirname(filePath);
    const fileName = path.basename(filePath);
    const slug = fileName.replace(/-tool\.ts$/, '');
    const toolCandidates = actionEnumsFromSource(source, constants);
    const siblingCandidates = toolCandidates.length > 0
      ? []
      : fs.readdirSync(definitionDir)
        .filter((entry) =>
          entry.startsWith(`${slug}-`) && !entry.endsWith('-tool.ts')
        )
        .sort()
        .flatMap((entry) => actionEnumsFromSource(
          fs.readFileSync(path.join(definitionDir, entry), 'utf8'),
          constants
        ));
    tools.push({
      name,
      actions: selectUnambiguousActions(
        name,
        toolCandidates.length > 0 ? toolCandidates : siblingCandidates
      ),
      schema: schemas.get(name) ?? { type: 'object', properties: {}, required: [] }
    });
  }

  return tools;
}

/**
 * Real-repo mode reads the generated parent definitions, which are the neutral
 * contract the native plugin generates from the same capability records. They
 * intentionally omit the TypeScript-only `params` passthrough, so comparing them
 * against native parent schemas stays apples-to-apples.
 *
 * Explicit fixture roots keep scanning `*-tool.ts` with the compiler AST parser.
 */
export function extractTypeScriptTools(paths) {
  const tools = isGeneratedDefinitionsRoot(paths.tsDefinitionsRoot)
    ? generatedParityTools()
    : sourceParityTools(paths);

  return tools.sort((left, right) => left.name.localeCompare(right.name));
}
