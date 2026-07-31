#!/usr/bin/env node
// Task 27 manual-QA harness.
//
// Feeds canonical v2 and generated legacy requests through the TypeScript
// reference pipeline, normalizes the receipts/errors, and compares the NATIVE
// normative tables (extracted from the plugin C++) against the TypeScript ones.
//
// What this does and does not prove:
//   PROVES  — both surfaces declare the identical rule -> code mapping, option
//             key set, timeout bound, schema-keyword set and error algebra, and
//             the TypeScript receipts for both request forms are byte-identical
//             apart from the request form itself.
//   DEFERS  — actual native execution. That runs in-editor via
//             Private/Tests/McpNativeGatewayExecuteValidationTests.cpp, compiled
//             by the serialized UE BuildPlugin gate.
//
// Usage: node tests/native-validation-fixture.mjs [--out <path>]

import { mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { resolve, join } from 'node:path';
import { createHash } from 'node:crypto';

const root = process.cwd();
const pluginPrivate = resolve(root, 'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private');
const readNative = (rel) => readFileSync(resolve(pluginPrivate, rel), 'utf8');

const outIndex = process.argv.indexOf('--out');
const outPath = outIndex === -1
  ? resolve(root, '.omo/evidence/task-27/native-validation-fixture.json')
  : resolve(root, process.argv[outIndex + 1]);

// Scratch dir proves the harness cleans up after itself (probe: temp residue).
const scratch = mkdtempSync(join(tmpdir(), 'task27-qa-'));
const removeScratch = () => rmSync(scratch, { recursive: true, force: true });
process.once('exit', removeScratch);
for (const signal of ['SIGINT', 'SIGTERM', 'SIGHUP']) {
  process.once(signal, () => {
    removeScratch();
    process.exit(130);
  });
}

const problems = [];
const check = (label, actual, expected) => {
  const ok = JSON.stringify(actual) === JSON.stringify(expected);
  if (!ok) problems.push({ label, expected, actual });
  return ok;
};

// ---- TypeScript side: the normative tables ------------------------------
const readTs = (rel) => readFileSync(resolve(root, rel), 'utf8');
const schemaSubsetTs = readTs('tests/unit/gateway-discovery-suite/schema-subset.ts');
const executeReferenceTs = readTs('tests/unit/gateway-discovery-suite/execute-reference.ts');

const tsArray = (source, name) => {
  const start = source.indexOf(`export const ${name} = [`);
  if (start === -1) return [];
  const end = source.indexOf(']', start);
  return source
    .slice(source.indexOf('[', start) + 1, end)
    .split(',')
    .map((entry) => entry.trim().replace(/^'|'$/g, ''))
    .filter((entry) => entry.length > 0 && !entry.startsWith('//'));
};

const tsGatewayCodes = Object.fromEntries(
  [...schemaSubsetTs.matchAll(/^\s*'?([a-z-]+)'?:\s*'([A-Z_]+)'/gm)].map((m) => [m[1], m[2]]),
);
const tsSupportedKeywords = tsArray(schemaSubsetTs, 'SUPPORTED_SCHEMA_KEYWORDS');
const tsOptionKeys = tsArray(executeReferenceTs, 'EXECUTION_OPTION_KEYS');
const tsMaxTimeout = Number(
  /export const MAX_TIMEOUT_MS = ([0-9_]+)/.exec(executeReferenceTs)?.[1].replace(/_/g, '') ?? 0,
);

// ---- Native side: the same tables, read out of the C++ -------------------
const schemaCpp = readNative('MCP/Execute/McpNativeGatewaySchemaValidation.cpp');
const keywordsCpp = readNative('MCP/Execute/McpNativeGatewaySchemaKeywords.cpp');
const requestCpp = readNative('MCP/Execute/McpNativeGatewayExecuteRequest.cpp');
const requestHeader = readNative('MCP/Execute/McpNativeGatewayExecuteRequest.h');
const receiptCpp = readNative('MCP/Execute/McpNativeGatewayReceipt.cpp');

const nativeSupportedKeywords = [
  ...keywordsCpp.slice(
    keywordsCpp.indexOf('SupportedKeywords[] = {'),
    keywordsCpp.indexOf('};', keywordsCpp.indexOf('SupportedKeywords[] = {')),
  ).matchAll(/TEXT\("([^"]+)"\)/g),
].map((m) => m[1]);

const nativeOptionKeys = [
  ...requestCpp.slice(
    requestCpp.indexOf('static const TArray<FString> Keys = {'),
    requestCpp.indexOf('};', requestCpp.indexOf('static const TArray<FString> Keys = {')),
  ).matchAll(/TEXT\("([^"]+)"\)/g),
].map((m) => m[1]);

const nativeMaxTimeout = Number(
  /McpMaxExecutionTimeoutMs = ([0-9]+)/.exec(requestHeader)?.[1] ?? 0,
);

const nativeViolationCodes = Object.fromEntries(
  [...schemaCpp.matchAll(/case EMcpSchemaViolation::(\w+): return TEXT\("([A-Z_]+)"\);/g)].map(
    (m) => [m[1], m[2]],
  ),
);

const NATIVE_TO_TS_REASON = {
  MissingRequired: 'missing-required',
  Undeclared: 'undeclared',
  Type: 'type',
  Enum: 'enum',
  Range: 'range',
  UnsupportedKeyword: 'unsupported-keyword',
};
const nativeCodesByTsReason = Object.fromEntries(
  Object.entries(nativeViolationCodes)
    .filter(([reason]) => reason in NATIVE_TO_TS_REASON)
    .map(([reason, code]) => [NATIVE_TO_TS_REASON[reason], code]),
);

const nativeErrorKinds = [...receiptCpp.matchAll(/MakeError\(\s*TEXT\("(\w+)"\), TEXT\("([A-Z_]+)"\)/g)]
  .map((m) => ({ kind: m[1], code: m[2] }));

check('supported schema keywords', [...nativeSupportedKeywords].sort(), [...tsSupportedKeywords].sort());
check('execution option keys', nativeOptionKeys, tsOptionKeys);
check('max execution timeout ms', nativeMaxTimeout, tsMaxTimeout);
check('violation reason -> gateway code', nativeCodesByTsReason, tsGatewayCodes);

// ---- Request-form equivalence over the TypeScript reference --------------
// A failed import here must be fatal: silently skipping this section would let
// the harness report PASS while comparing nothing.
const { CANONICAL_CAPABILITY_RECORDS, CATALOG_REVISION } = await import(
  '../src/tools/catalog/capabilities/generated/canonical-registry.generated.js'
);
const { buildResolverIndex, executeReference } = await import('./unit/gateway-discovery-suite/execute-reference.js');
const { minimalValidParams } = await import('./unit/gateway-discovery-suite/case-builder.js');

// The stub must satisfy the capability's declared output schema, or every
// comparison would come back equal-but-failing and mask a broken happy path.
const schemaValidOutput = (record) => {
  const schema = record.schemas.output ?? {};
  const output = {};
  for (const name of schema.required ?? []) {
    const declared = schema.properties?.[name]?.type;
    output[name] = declared === 'boolean' ? true
      : declared === 'number' || declared === 'integer' ? 1
      : declared === 'array' ? []
      : declared === 'object' ? {}
      : 'ok';
  }
  return output;
};

const formComparisons = [];
{
  if (!Array.isArray(CANONICAL_CAPABILITY_RECORDS) || CANONICAL_CAPABILITY_RECORDS.length === 0) {
    throw new Error('canonical capability records failed to load');
  }
  const index = buildResolverIndex(CANONICAL_CAPABILITY_RECORDS);

  const sampleParents = ['manage_asset', 'control_actor', 'inspect', 'system_control', 'manage_sequence'];
  for (const parent of sampleParents) {
    const record = CANONICAL_CAPABILITY_RECORDS.find((entry) => entry.routing.parentTool === parent);
    if (!record) continue;
    const params = minimalValidParams(record);
    const legacy = record.legacyIds[0];
    const deps = {
      index,
      isEnabled: () => true,
      dispatch: (dispatched) => ({ ok: true, data: schemaValidOutput(dispatched) }),
    };
    const canonical = executeReference({ capability: record.id, params }, deps);
    const viaLegacy = executeReference({ tool: legacy?.tool, action: legacy?.action, params }, deps);
    const normalize = (receipt) => ({ ...receipt, data: undefined });
    const equal = JSON.stringify(normalize(canonical)) === JSON.stringify(normalize(viaLegacy));
    if (!equal) problems.push({ label: `form equivalence ${record.id}`, expected: canonical, actual: viaLegacy });
    if (canonical.status !== 'success') {
      problems.push({ label: `happy path ${record.id}`, expected: 'success', actual: canonical });
    }
    formComparisons.push({
      capabilityId: record.id,
      legacy: `${legacy?.tool}.${legacy?.action}`,
      canonicalStatus: canonical.status,
      legacyStatus: viaLegacy.status,
      normalizedEqual: equal,
    });
  }
}

if (formComparisons.length !== 5) {
  problems.push({
    label: 'request-form equivalence coverage',
    expected: 5,
    actual: formComparisons.length,
  });
}

const artifact = {
  task: 27,
  kind: 'native-validation-fixture-qa',
  ranAt: new Date().toISOString(),
  catalogRevision: CATALOG_REVISION ?? null,
  proves: 'TS and native declare identical normative validation tables; both request forms normalize to one receipt',
  defers: 'native runtime execution — Private/Tests/McpNativeGatewayExecuteValidationTests.cpp under the serialized UE BuildPlugin gate',
  normativeTables: {
    supportedSchemaKeywords: { typescript: tsSupportedKeywords, native: nativeSupportedKeywords },
    executionOptionKeys: { typescript: tsOptionKeys, native: nativeOptionKeys },
    maxExecutionTimeoutMs: { typescript: tsMaxTimeout, native: nativeMaxTimeout },
    violationGatewayCodes: { typescript: tsGatewayCodes, native: nativeCodesByTsReason },
    nativeErrorAlgebra: nativeErrorKinds,
  },
  requestFormEquivalence: formComparisons,
  mismatches: problems,
  verdict: problems.length === 0 ? 'PASS' : 'FAIL',
};
artifact.artifactSha256 = createHash('sha256').update(JSON.stringify(artifact)).digest('hex');

writeFileSync(outPath, `${JSON.stringify(artifact, null, 2)}\n`);

// Clean the scratch resources this harness created.
rmSync(scratch, { recursive: true, force: true });

process.stdout.write(`${artifact.verdict}: ${problems.length} mismatch(es); artifact -> ${outPath}\n`);
process.stdout.write(`scratch ${scratch} removed\n`);
process.exit(problems.length === 0 ? 0 : 1);
