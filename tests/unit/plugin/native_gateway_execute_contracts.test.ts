/// <reference types="node" />

import { existsSync, readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

// Task 27 source contracts: canonical validation + semantic envelopes on the
// native `/mcp` execute path.
//
// A live editor HTTP harness is not available here and UE BuildPlugin is the
// authoritative compile gate (deferred to the serialized parent job), so these
// read the plugin C++ and assert the required pipeline exists and the pre-Task-27
// behavior is gone. Runtime execution of the same rules is covered by the native
// automation test in Private/Tests/McpNativeGatewayExecuteValidationTests.cpp.
//
// RED (pre-Task-27 baseline, .omo/evidence/task-27/baseline.json): the execute
// path accepted only the legacy {tool, action} form, whitelisted params against
// the TOOL-UNION, ran strict schema validation for 1 of 23 parents, validated no
// options and no output, and emitted no semantic receipt.

const pluginPrivate = resolve(
  process.cwd(),
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private',
);

const read = (rel: string): string => readFileSync(resolve(pluginPrivate, rel), 'utf8');
const exists = (rel: string): boolean => existsSync(resolve(pluginPrivate, rel));

const RECORDS_H = 'MCP/Execute/McpNativeGatewayCanonicalRecords.h';
const RECORDS_CPP = 'MCP/Execute/McpNativeGatewayCanonicalRecords.cpp';
const SCHEMA_CPP = 'MCP/Execute/McpNativeGatewaySchemaValidation.cpp';
const KEYWORDS_CPP = 'MCP/Execute/McpNativeGatewaySchemaKeywords.cpp';
const REQUEST_CPP = 'MCP/Execute/McpNativeGatewayExecuteRequest.cpp';
const RECEIPT_CPP = 'MCP/Execute/McpNativeGatewayReceipt.cpp';
const EXECUTE_CPP = 'MCP/Execute/McpNativeTransportGatewayExecute.cpp';
const VALIDATION_CPP = 'MCP/Execute/McpNativeGatewayValidation.cpp';
const NATIVE_TEST = 'Tests/McpNativeGatewayExecuteValidationTests.cpp';

describe('Task 27: native execute owns a canonical validation pipeline', () => {
  it('ships the canonical-record, schema, request, receipt and execute modules', () => {
    for (const module of [RECORDS_H, RECORDS_CPP, SCHEMA_CPP, REQUEST_CPP, RECEIPT_CPP, EXECUTE_CPP]) {
      expect(exists(module), `${module} must exist`).toBe(true);
    }
  });

  it('loads canonical capability records from the generated shards, not a handwritten table', () => {
    const records = read(RECORDS_CPP);
    expect(records).toContain('McpGeneratedCapabilityShards');
    expect(records).toContain('McpGeneratedCapabilityShards::At(');
    // The shard record total is the load gate, so a truncated catalog cannot pass.
    expect(records).toContain('TotalRecordCount()');
    // Indexes required to resolve every accepted request form.
    expect(records).toContain('FindById');
    expect(records).toContain('ResolveAlias');
    expect(records).toContain('FindByLegacy');
    // Startup failure must be honest, never a silent empty catalog: the index
    // exposes the error and drops every record rather than serving a partial one.
    expect(read(RECORDS_H)).toContain('GetLoadError');
    expect(records).toContain('LoadError = FString::Printf');
    expect(records).toContain('RecordsById.Reset()');
  });

  it('accepts the canonical v2 form and the generated legacy form', () => {
    const request = read(REQUEST_CPP);
    expect(request).toContain('TEXT("capability")');
    expect(request).toContain('TEXT("tool")');
    expect(request).toContain('TEXT("action")');
    expect(request).toContain('TEXT("options")');
  });

  it('rejects an alias that resolves to more than one capability', () => {
    expect(read(REQUEST_CPP)).toContain('ALIAS_CONFLICT');
  });

  it('rejects a request whose v2 and legacy forms disagree instead of silently preferring one', () => {
    expect(read(REQUEST_CPP)).toContain('FORM_CONFLICT');
  });

  it('validates execution options against the bounded Task 3 option set', () => {
    const request = read(REQUEST_CPP);
    for (const key of [
      'idempotencyKey',
      'expectedCatalogRevision',
      'preview',
      'savePolicy',
      'timeoutMs',
      'validationLevel',
      'taskPreference',
    ]) {
      expect(request, `option '${key}' must be declared`).toContain(`TEXT("${key}")`);
    }
    // The code literal lives in the shared error constructor; this asserts the
    // option path routes through it rather than inventing its own code.
    expect(request).toContain('McpOptionError(');
    expect(read(RECEIPT_CPP)).toContain('UNSUPPORTED_OPTION');
    expect(request).toContain('McpMaxExecutionTimeoutMs');
  });

  it('keeps gateway controls out of action params', () => {
    expect(read(REQUEST_CPP)).toContain('must not appear in action params');
  });

  it('enforces the exact per-action schema, not the tool-union parameter list', () => {
    const validation = read(VALIDATION_CPP);
    // The union whitelist was the pre-Task-27 rule; it must be gone from execute.
    expect(validation).not.toContain('GatewayGetParameterNames(Tool)');
    expect(validation).toContain('McpValidateObjectAgainstCanonicalSchema');
    expect(validation).toContain('Request.Record->InputSchema');
  });

  it('implements exactly the schema keywords the canonical records use, fail-closed on the rest', () => {
    // Set equality, not substring hits: that is what makes "exactly" enforceable
    // in both directions — no unimplemented keyword, no keyword the records never use.
    const table = read(KEYWORDS_CPP).match(
      /SupportedKeywords\[\]\s*=\s*\{([\s\S]*?)\};/u,
    )?.[1];
    expect(table, 'SupportedKeywords[] table must exist').toBeDefined();
    const implemented = [...(table ?? '').matchAll(/TEXT\("([^"]+)"\)/gu)].map((m) => m[1]);

    // Keywords the 1,335 records use, per baseline.json canonicalRecordSchemaKeywords,
    // plus the Task 2 reflection boundary that stays open by contract.
    expect([...implemented].sort()).toEqual(
      [
        '$schema', 'additionalProperties', 'default', 'description', 'enum',
        'items', 'maxItems', 'maxLength', 'maximum', 'minItems', 'minimum',
        'properties', 'required', 'type', 'x-unreal-reflection-boundary',
      ].sort(),
    );

    // Anything outside the table must fail closed rather than pass silently.
    expect(read(KEYWORDS_CPP)).toContain('bool IsSupportedKeyword');
    expect(read(SCHEMA_CPP)).toContain('IsSupportedKeyword');
    expect(read(SCHEMA_CPP)).toContain('UNSUPPORTED_SCHEMA_KEYWORD');
  });

  it('enforces per-action canonical strictness on the execute path, not a per-tool opt-in', () => {
    // Task 30: the legacy transport ValidateToolArguments opt-in is deleted; the
    // gateway execute path validates each request against its capability's exact
    // per-action input schema, with no per-tool EnforceStrictArguments gate.
    const validation = read(VALIDATION_CPP);
    expect(validation).not.toContain('EnforceStrictArguments');
    expect(validation).toContain('Request.Record->InputSchema');
    expect(validation).toContain('McpValidateObjectAgainstCanonicalSchema');
  });

  it('fails closed when a capability cannot be resolved instead of skipping validation', () => {
    // Task 30: the records-unavailable fallback is deleted with ValidateToolArguments.
    // Execute resolves the capability from canonical records and returns an error
    // receipt when it cannot, never dispatching unvalidated arguments.
    const validation = read(VALIDATION_CPP);
    const records = read(RECORDS_CPP);
    expect(validation).toContain('McpParseGatewayExecuteRequest');
    expect(validation).toContain('UNKNOWN_TOOL');
    expect(records).toContain('McpCanonicalRecordsAvailable');
  });

  it('validates the handler result against the capability output schema', () => {
    expect(read(VALIDATION_CPP)).toContain('OUTPUT_SCHEMA_VIOLATION');
    // The completion path is where a handler result actually arrives.
    const pending = read('MCP/Transport/McpNativeTransportPendingRequests.cpp');
    expect(pending).toContain('ValidateGatewayExecuteOutput');
    expect(pending).toContain('McpBuildSuccessReceipt');
  });

  it('emits a semantic receipt carrying capabilityId, catalogRevision and status', () => {
    const receipt = read(RECEIPT_CPP);
    expect(receipt).toContain('TEXT("capabilityId")');
    expect(receipt).toContain('TEXT("catalogRevision")');
    expect(receipt).toContain('TEXT("status")');
    expect(receipt).toContain('TEXT("correlationId")');
  });

  it('emits the typed error algebra shared with TypeScript', () => {
    const receipt = read(RECEIPT_CPP);
    expect(receipt).toContain('TEXT("kind")');
    for (const kind of ['validation', 'option', 'range', 'execution']) {
      expect(receipt, `error kind '${kind}' must be representable`).toContain(`TEXT("${kind}")`);
    }
    for (const code of ['VALIDATION_ERROR', 'UNSUPPORTED_OPTION', 'OUT_OF_RANGE', 'UNREAL_ENGINE_ERROR']) {
      expect(receipt, `error code '${code}' must be representable`).toContain(code);
    }
  });

  it('preserves structured Unreal detail on a dispatch failure', () => {
    expect(read(RECEIPT_CPP)).toContain('TEXT("unrealDetail")');
  });

  it('never names a semantic-error helper MakeError, which the engine template hijacks', () => {
    // UE 5.7.4 BuildPlugin regression (.omo/evidence/task-25-27-ue57-build/): a local
    // MakeError() lost overload resolution to the global variadic MakeError() in
    // Templates/ValueOrError.h on any call whose third argument was a TEXT() literal, because
    // the template forwards all arguments exactly while the local helper needs char16_t[N] ->
    // FString. DataTables/Shared.h documents the same hazard and remedy.
    // Negative lookbehind so prefixed names (McpDataTableMakeError) do not match.
    const bare = [...read(RECEIPT_CPP).matchAll(/(?<![A-Za-z0-9_])MakeError\s*\(/gu)];
    expect(bare, 'receipt module must not declare or call a bare MakeError(').toHaveLength(0);
    expect(read(RECEIPT_CPP)).toContain('McpGatewayMakeSemanticError');
  });

  it('never reaches the subsystem queue when validation fails', () => {
    const execute = read(EXECUTE_CPP);
    const queueIndex = execute.indexOf('StreamToolCall');
    const validateIndex = execute.indexOf('ValidateAndResolveGatewayExecute');
    expect(validateIndex).toBeGreaterThanOrEqual(0);
    expect(queueIndex).toBeGreaterThan(validateIndex);
  });

  it('keeps session, dynamic-tool state and queue ownership on the existing path', () => {
    const execute = read(EXECUTE_CPP);
    expect(execute).toContain('TryHandleLocalToolCall');
    expect(execute).toContain('StreamToolCall');
    expect(read(VALIDATION_CPP)).toContain('IsToolEnabled');
  });

  it('ships a native automation test that runs the generated suite in-editor', () => {
    expect(exists(NATIVE_TEST), `${NATIVE_TEST} must exist`).toBe(true);
    const nativeTest = read(NATIVE_TEST);
    expect(nativeTest).toContain('IMPLEMENT_SIMPLE_AUTOMATION_TEST');
    expect(nativeTest).toContain('WITH_DEV_AUTOMATION_TESTS');
    expect(nativeTest).toContain('McpNativeGatewayExecuteSuite');
  });
});
