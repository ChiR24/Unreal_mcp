#!/usr/bin/env node
// Task 27: characterize the CURRENT native `/mcp` gateway execute validation +
// envelope behavior. Run before the change it reports RED, run after it reports
// GREEN; the frozen RED capture is .omo/evidence/task-27/baseline.json.
//
// Detectors read the execute PIPELINE rather than one file, so splitting a
// module for the 250-line ceiling cannot make an implemented stage look absent.
//
// Reproducible: reads plugin C++ source and the generated canonical registry,
// emits JSON to stdout. No editor, no build, no network.

import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

const root = process.cwd();
const pluginPrivate = resolve(
  root,
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private',
);

const read = (rel) => readFileSync(resolve(pluginPrivate, rel), 'utf8');

const gateway =
  read('MCP/Transport/McpNativeTransportGateway.cpp') +
  read('MCP/Execute/McpNativeTransportGatewayExecute.cpp');
const validation = [
  'McpNativeGatewayValidation.cpp',
  'McpNativeGatewayExecuteRequest.cpp',
  'McpNativeGatewayReceipt.cpp',
  'McpNativeGatewaySchemaValidation.cpp',
  'McpNativeGatewaySchemaKeywords.cpp',
  'McpNativeGatewayCanonicalRecords.cpp',
].map((module) => read(`MCP/Execute/${module}`)).join('\n');
const argValidation = read('MCP/Transport/McpNativeTransportArgumentValidation.cpp');
const toolDefinition = read('MCP/Registry/McpToolDefinition.h');
const pendingRequests = read('MCP/Transport/McpNativeTransportPendingRequests.cpp');
const jsonRpc = read('MCP/Protocol/McpJsonRpc.cpp');

// --- request forms the native execute branch accepts -----------------------
const requestParsing = `${gateway}${validation}`;
const acceptsLegacyForm =
  /TEXT\("tool"\)/.test(requestParsing) && /TEXT\("action"\)/.test(requestParsing);
const acceptsV2Form = /TEXT\("capability"\)/.test(requestParsing);
const readsOptions = /TEXT\("options"\)/.test(gateway) || /TEXT\("options"\)/.test(validation);

// --- validation stages present in the execute path -------------------------
const stages = {
  unknownTool: validation.includes('UNKNOWN_TOOL'),
  unknownAction: validation.includes('UNKNOWN_ACTION'),
  toolDisabled: validation.includes('TOOL_DISABLED'),
  paramsMustBeObject: validation.includes('INVALID_PARAMS'),
  rejectActionOverride: validation.includes('must not override action or subAction'),
  undeclaredParameter: validation.includes('UNDECLARED_PARAMETER'),
  aliasResolution: /ResolveAlias|ALIAS_CONFLICT/.test(validation),
  capabilityResolution: /TEXT\("capability"\)|FMcpCanonicalRecord/.test(validation),
  optionValidation: /UNSUPPORTED_OPTION/.test(validation),
  outputValidation: /schemas?\s*\.\s*output|OUTPUT_SCHEMA|ValidateOutput/i.test(
    `${validation}${gateway}${pendingRequests}`,
  ),
};

// --- undeclared-parameter scope: tool-union vs exact per-action ------------
const undeclaredParamScope = validation.includes('GatewayGetParameterNames(Tool)')
  ? 'tool-union'
  : validation.includes('Request.Record->InputSchema')
    ? 'exact-per-action'
    : 'unknown';

// --- strict argument coverage (the plan's "5/23 opt-in") -------------------
const strictOverride = 'EnforceStrictArguments() const override { return true; }';
const registryFiles = [
  'Core_Actor', 'Core_Asset', 'Core_Blueprint', 'Core_System',
  'Gameplay_AI', 'Gameplay_Anim', 'Gameplay_Combat', 'Gameplay_Sys',
  'Utility_Audio', 'Utility_Sequence',
  'World_Environment', 'World_EnvironmentFields', 'World_EnvironmentStructures',
  'World_Geometry', 'World_Structure',
];
const strictTools = [];
const allTools = [];
for (const shard of registryFiles) {
  let text;
  try {
    text = read(`MCP/Tools/McpGeneratedParentRegistry_${shard}.cpp`);
  } catch {
    continue;
  }
  // Each generated parent class block starts at `class FMcpGenTool_`.
  for (const block of text.split(/(?=class FMcpGenTool_)/g)) {
    const nameMatch = block.match(/GetName\(\) const override \{ return TEXT\("([a-z_]+)"\); \}/);
    if (!nameMatch) continue;
    allTools.push(nameMatch[1]);
    if (block.includes(strictOverride)) strictTools.push(nameMatch[1]);
  }
}

// --- receipt / envelope shape ---------------------------------------------
const envelope = {
  builder: 'FMcpJsonRpc::BuildToolResult',
  emitsStructuredContent: jsonRpc.includes('SetObjectField(TEXT("structuredContent")'),
  emitsIsError: jsonRpc.includes('SetBoolField(TEXT("isError")'),
  emitsCapabilityId: /capabilityId/.test(`${jsonRpc}${gateway}${validation}`),
  emitsCatalogRevision: /catalogRevision/i.test(`${jsonRpc}${gateway}${validation}`),
  emitsCorrelationId: /correlationId/i.test(`${jsonRpc}${gateway}${validation}`),
  emitsSemanticStatusField: /SetStringField\(TEXT\("status"\), TEXT\("(success|error)"\)\)/.test(
    `${jsonRpc}${gateway}${validation}`,
  ),
  typedErrorAlgebraKinds: /"kind"/.test(`${jsonRpc}${gateway}${validation}`),
};

// --- schema keyword inventory actually used by canonical records -----------
const registrySrc = readFileSync(
  resolve(root, 'src/tools/catalog/capabilities/generated/canonical-registry.generated.ts'),
  'utf8',
);
const keywordInventory = {};
const collectKeywords = (schema) => {
  if (schema === null || typeof schema !== 'object') return;
  if (Array.isArray(schema)) {
    for (const entry of schema) collectKeywords(entry);
    return;
  }
  for (const [key, value] of Object.entries(schema)) {
    keywordInventory[key] = (keywordInventory[key] ?? 0) + 1;
    if (key === 'properties' && value && typeof value === 'object') {
      for (const sub of Object.values(value)) collectKeywords(sub);
    } else if (key === 'items' || key === 'propertyNames') {
      collectKeywords(value);
    }
  }
};

// The generated module is a huge TS literal; parse the embedded JSON blocks by
// slicing each `"schemas": { ... }` occurrence via balanced-brace scanning.
const sliceBalanced = (text, startIndex) => {
  let depth = 0;
  for (let i = startIndex; i < text.length; i += 1) {
    const ch = text[i];
    if (ch === '{') depth += 1;
    else if (ch === '}') {
      depth -= 1;
      if (depth === 0) return text.slice(startIndex, i + 1);
    }
  }
  return undefined;
};

let recordSchemaCount = 0;
let searchFrom = 0;
for (;;) {
  const marker = registrySrc.indexOf('"schemas": {', searchFrom);
  if (marker === -1) break;
  const block = sliceBalanced(registrySrc, registrySrc.indexOf('{', marker + 10));
  searchFrom = marker + 12;
  if (!block) continue;
  let parsed;
  try {
    parsed = JSON.parse(block);
  } catch {
    continue;
  }
  recordSchemaCount += 1;
  collectKeywords(parsed.input);
  collectKeywords(parsed.output);
}

const baseline = {
  task: 27,
  kind: 'baseline-characterization',
  capturedAt: new Date().toISOString(),
  surface: 'native /mcp gateway execute',
  requestForms: {
    legacyToolAction: acceptsLegacyForm,
    canonicalV2Capability: acceptsV2Form,
    optionsEnvelope: readsOptions,
  },
  validationStages: stages,
  undeclaredParameterScope: undeclaredParamScope,
  strictArgumentCoverage: {
    note: 'strictTools counts the legacy per-tool opt-in only; it is the FALLBACK once '
      + 'canonicalPerAction is true, not the active gate',
    strictTools,
    strictCount: strictTools.length,
    totalParentTools: allTools.length,
    canonicalPerAction: argValidation.includes('McpValidateCanonicalToolArguments'),
    legacyGateIsFallbackOnly: /if \(!McpCanonicalRecordsAvailable\(\)\)/.test(argValidation),
    gate: argValidation.includes('McpValidateCanonicalToolArguments')
      ? 'McpValidateCanonicalToolArguments() per canonical action'
      : argValidation.includes('!ToolDefinition->EnforceStrictArguments()')
        ? 'FMcpToolDefinition::EnforceStrictArguments()'
        : 'unknown',
    defaultInBaseClass: toolDefinition.includes(
      'virtual bool EnforceStrictArguments() const { return false; }',
    )
      ? false
      : 'unknown',
  },
  envelope,
  canonicalRecordSchemaKeywords: {
    recordsScanned: recordSchemaCount,
    keywords: Object.fromEntries(
      Object.entries(keywordInventory).sort(([, a], [, b]) => b - a),
    ),
  },
};

process.stdout.write(`${JSON.stringify(baseline, null, 2)}\n`);
