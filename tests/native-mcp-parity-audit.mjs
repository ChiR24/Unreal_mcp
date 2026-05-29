#!/usr/bin/env node

import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const repoRoot = path.resolve(__dirname, '..');

const tsToolDefinitionsPath = path.join(repoRoot, 'src/tools/consolidated-tool-definitions.ts');
const nativeMcpRoot = path.join(repoRoot, 'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP');
const nativeToolRegistryPath = path.join(nativeMcpRoot, 'McpToolRegistry.cpp');
const nativeConsolidatedActionsPath = path.join(nativeMcpRoot, 'McpConsolidatedActionRouting.h');
const nativeToolsRoot = path.join(nativeMcpRoot, 'Tools');

function stringArrayFromEnumBody(enumBody, constants) {
  const values = [];
  for (const match of enumBody.matchAll(/'([^']+)'|\.\.\.([A-Z0-9_]+_ACTIONS)/g)) {
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
  for (const match of source.matchAll(/export const ([A-Z0-9_]+_ACTIONS)\s*=\s*\[([\s\S]*?)\]\s+as const/g)) {
    constants.set(match[1], stringArrayFromEnumBody(match[2], constants));
  }
  return constants;
}

function extractTypeScriptTools() {
  const source = fs.readFileSync(tsToolDefinitionsPath, 'utf8');
  const constants = extractActionConstants(source);
  const tools = [];
  const toolStarts = [...source.matchAll(/\n\s*\{\s*\n\s*name:\s*'([^']+)'/g)];

  for (let index = 0; index < toolStarts.length; index += 1) {
    const match = toolStarts[index];
    const next = toolStarts[index + 1];
    const blockEnd = next?.index ?? source.indexOf('\n];', match.index);
    if (blockEnd < 0) {
      throw new Error('Could not locate the end of consolidatedToolDefinitions');
    }
    const block = source.slice(match.index, blockEnd);
    const actionEnum = block.match(/action:\s*\{[\s\S]*?enum:\s*\[([\s\S]*?)\]\s*,\s*description:/);
    tools.push({
      name: match[1],
      actions: actionEnum ? stringArrayFromEnumBody(actionEnum[1], constants) : []
    });
  }

  return tools.sort((left, right) => left.name.localeCompare(right.name));
}

function extractTextValues(text) {
  return [...text.matchAll(/TEXT\(\s*"([^"]+)"\s*\)/g)].map((match) => match[1]);
}

function extractNativeCanonicalRegistry() {
  const source = fs.readFileSync(nativeToolRegistryPath, 'utf8');
  const registryMatch = source.match(/CanonicalToolNames\s*=\s*\{([\s\S]*?)\};/);
  return [...new Set(extractTextValues(registryMatch?.[1] ?? ''))].sort();
}

function nativeToolFiles() {
  return fs.readdirSync(nativeToolsRoot, { withFileTypes: true })
    .filter((entry) => entry.isFile() && entry.name.endsWith('.cpp'))
    .map((entry) => path.join(nativeToolsRoot, entry.name))
    .sort();
}

function buildNativeActionEvaluator() {
  const source = fs.readFileSync(nativeConsolidatedActionsPath, 'utf8');
  const functionBodies = new Map(
    [...source.matchAll(/inline\s+(?:const\s+)?TArray<FString>&?\s+(\w+)\s*\(\)\s*\{([\s\S]*?)\n\}/g)]
      .map((match) => [match[1], match[2]])
  );
  const memo = new Map();

  function evaluate(functionName) {
    if (memo.has(functionName)) return memo.get(functionName);
    const body = functionBodies.get(functionName);
    if (!body) return [];

    const staticList = body.match(/static const TArray<FString> Actions = \{([\s\S]*?)\};/);
    if (staticList) {
      const values = [...new Set(extractTextValues(staticList[1]))].sort();
      memo.set(functionName, values);
      return values;
    }

    const firstSource = body.match(/TArray<FString> Actions = (\w+)\(\);/);
    let values = firstSource ? evaluate(firstSource[1]) : [];
    for (const append of body.matchAll(/AppendUniqueActions\(Actions, (\w+)\(\)\);/g)) {
      values = [...new Set([...values, ...evaluate(append[1])])].sort();
    }
    memo.set(functionName, values);
    return values;
  }

  return evaluate;
}

function extractActionEnumFromNativeTool(source, evaluateActions) {
  const routedActionEnum = source.match(
    /\.StringEnum\(\s*TEXT\(\s*"action"\s*\)\s*,\s*McpConsolidatedActions::(\w+)\(\)\s*,/
  );
  if (routedActionEnum) {
    return evaluateActions(routedActionEnum[1]);
  }

  const inlineActionEnum = source.match(
    /\.StringEnum\(\s*TEXT\(\s*"action"\s*\)\s*,\s*\{([\s\S]*?)\}\s*,/
  );
  return [...new Set(extractTextValues(inlineActionEnum?.[1] ?? ''))].sort();
}

function extractNativeToolDefinitions() {
  const evaluateActions = buildNativeActionEvaluator();
  const tools = [];

  for (const filePath of nativeToolFiles()) {
    const source = fs.readFileSync(filePath, 'utf8');
    const name = source.match(/GetName\(\)\s+const\s+override\s*\{\s*return\s+TEXT\(\s*"([^"]+)"\s*\)\s*;/)?.[1];
    if (!name) continue;
    tools.push({
      name,
      actions: extractActionEnumFromNativeTool(source, evaluateActions)
    });
  }

  return tools.sort((left, right) => left.name.localeCompare(right.name));
}

function difference(left, right) {
  const rightSet = new Set(right);
  return left.filter((value) => !rightSet.has(value)).sort();
}

const typeScriptTools = extractTypeScriptTools();
const nativeRegistry = extractNativeCanonicalRegistry();
const nativeTools = extractNativeToolDefinitions();
const nativeCanonicalTools = nativeTools.filter((tool) => nativeRegistry.includes(tool.name));
const nativeToolsByName = new Map(nativeCanonicalTools.map((tool) => [tool.name, tool]));

const toolNameGaps = {
  missingFromNativeRegistry: difference(typeScriptTools.map((tool) => tool.name), nativeRegistry),
  extraInNativeRegistry: difference(nativeRegistry, typeScriptTools.map((tool) => tool.name))
};

const actionGaps = [];
for (const tool of typeScriptTools) {
  const nativeActions = nativeToolsByName.get(tool.name)?.actions ?? [];
  const missingNativeActions = difference(tool.actions, nativeActions);
  const extraNativeActions = difference(nativeActions, tool.actions);
  if (missingNativeActions.length > 0 || extraNativeActions.length > 0) {
    actionGaps.push({ tool: tool.name, missingNativeActions, extraNativeActions });
  }
}

console.log('Native MCP Action Parity Audit');
console.log(`TypeScript tools: ${typeScriptTools.length}`);
console.log(`Native canonical tools: ${nativeRegistry.length}`);
console.log(`Native canonical definitions: ${nativeCanonicalTools.length}`);
console.log(`Tools with action mismatches: ${actionGaps.length}`);

if (toolNameGaps.missingFromNativeRegistry.length > 0 || toolNameGaps.extraInNativeRegistry.length > 0 || actionGaps.length > 0) {
  console.error(JSON.stringify({ toolNameGaps, actionGaps }, null, 2));
  process.exitCode = 1;
}
