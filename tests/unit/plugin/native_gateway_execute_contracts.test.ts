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
const RECEIPT_BUILD_CPP = 'MCP/Gateway/McpNativeGatewayExecuteReceiptBuild.cpp';
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
    // The completion path is where a handler result actually arrives; the
    // receipt it builds lives in MCP/Gateway (extracted from the transport's
    // pending-request file, which is socket bookkeeping, not receipt shape).
    const build = read(RECEIPT_BUILD_CPP);
    expect(build).toContain('ValidateGatewayExecuteOutput');
    expect(build).toContain('McpBuildSuccessReceipt');
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

describe('Task 39: native receipt parity — correlated revisions and typed plan-class errors', () => {
  it('sources capability and schema revisions from the record content/schema hashes', () => {
    const receipt = read(RECEIPT_CPP);
    expect(receipt).toContain('TEXT("capabilityRevision")');
    expect(receipt).toContain('TEXT("schemaRevision")');
    expect(receipt).toContain('Hashes->TryGetStringField(TEXT("content")');
    expect(receipt).toContain('Hashes->TryGetStringField(TEXT("schema")');
  });

  it('adds the plan-class error kinds shared with TypeScript', () => {
    const receipt = read(RECEIPT_CPP);
    for (const kind of ['capability', 'output', 'staleState', 'dispatch']) {
      expect(receipt, `error kind '${kind}' must be representable`).toContain(`TEXT("${kind}")`);
    }
    // STALE_STATE is hardcoded in the receipt constructor; the disabled/output
    // codes are supplied by the call sites in the validation module.
    expect(receipt).toContain('STALE_STATE');
  });

  it('carries retryability as a true boolean, not the old Field="retryable" string hack', () => {
    const receipt = read(RECEIPT_CPP);
    expect(receipt).toContain('SetBoolField(TEXT("retryable")');
    expect(receipt).not.toContain('Error.Field = bRetryable');
  });

  it('classifies a disabled capability and an output violation as their own kinds', () => {
    const validation = read(VALIDATION_CPP);
    expect(validation).toContain('McpCapabilityError(TEXT("TOOL_DISABLED")');
    expect(validation).toContain('McpOutputError(TEXT("OUTPUT_SCHEMA_VIOLATION")');
  });

  it('refuses a stale expectedCatalogRevision before dispatch', () => {
    const validation = read(VALIDATION_CPP);
    expect(validation).toContain('expectedCatalogRevision');
    expect(validation).toContain('McpStaleStateError');
    expect(validation).toContain('McpBuildErrorReceipt(Request.CapabilityId');
  });
});

describe('Task 39 REMEDIATION: native execute emits the nested canonical receipt at full cross-transport parity', () => {
  const RECEIPT = read(RECEIPT_CPP);
  const RECEIPT_HDR = read('MCP/Execute/McpNativeGatewayReceipt.h');
  const VALIDATION = read(VALIDATION_CPP);
  const PENDING = read('MCP/Transport/McpNativeTransportPendingRequests.cpp');
  const RECEIPT_BUILD = read(RECEIPT_BUILD_CPP);
  const STREAM = read('MCP/Transport/McpNativeTransportGatewayStream.cpp');
  const CONN = read('MCP/Transport/McpNativeTransportConnectionTypes.h');
  const ENRICH = read('MCP/Execute/McpNativeReceiptEnrichment.cpp');

  it('wraps a nested canonical `receipt` object carrying the typed error as a nested `error` object', () => {
    // The TS gateway nests the strict ReceiptSchema object under `receipt`; the
    // native flat envelope additionally emits the same nested object (attached in
    // the receipt module), assembled by the enrichment module with the typed error
    // as an `error` object (the flat `typedError` may remain for back-compat).
    expect(RECEIPT).toContain('SetObjectField(TEXT("receipt")');
    expect(ENRICH).toContain('SetObjectField(TEXT("error")');
  });

  it('threads a receipt context so requestId, idempotencyId and timingMs reach the receipt', () => {
    expect(RECEIPT_HDR).toContain('FMcpReceiptContext');
    for (const field of ['requestId', 'idempotencyId', 'timingMs']) {
      expect(ENRICH, `receipt must carry ${field}`).toContain(`TEXT("${field}")`);
    }
  });

  it('carries validation evidence, handles, changes and bounded nextCalls on the canonical receipt', () => {
    for (const field of ['validation', 'handles', 'changes', 'nextCalls']) {
      expect(ENRICH, `receipt must carry ${field}`).toContain(`TEXT("${field}")`);
    }
  });

  it('enforces the same serialized result-size limit as TypeScript (RESULT_TOO_LARGE at 100000 chars)', () => {
    expect(RECEIPT_BUILD).toContain('RESULT_TOO_LARGE');
    expect(`${RECEIPT_BUILD}${STREAM}${RECEIPT}`).toContain('100000');
  });

  it('classifies a real dispatch failure (queue full / subsystem unavailable / invalid session) as the dispatch kind', () => {
    // The completion path must map these transport-level failures onto the typed
    // dispatch algebra, not McpUnrealExecutionError (which is the execution kind).
    expect(RECEIPT_BUILD).toContain('McpDispatchError');
  });

  it('carries the client-facing correlation id from the pending state into the completed receipt (existing native crossing)', () => {
    expect(CONN).toContain('CorrelationId');
    expect(STREAM).toContain('Conn->CorrelationId');
    expect(PENDING).toContain('Conn->CorrelationId');
  });

  it('fails closed on a malformed expectedCatalogRevision (empty / non-string / non-hex / over-length) as INVALID_OPTIONS with a pointer, mirroring TS', () => {
    expect(VALIDATION).toContain('expectedCatalogRevision');
    expect(VALIDATION).toContain('IsCatalogRevisionDigest');
    // Explicit JSON-type check so a numeric pin (e.g. 12345) is not silently
    // coerced to a hex-looking string and misclassified as stale.
    expect(VALIDATION).toContain('EJson::String');
    expect(VALIDATION).toContain('McpValidationError(TEXT("INVALID_OPTIONS")');
    expect(VALIDATION).toContain('/options/expectedCatalogRevision');
    expect(VALIDATION).toContain('lowercase hex catalog-revision digest');
  });
});

describe('Task 39 POLISH: native receipt applies the same bounds/redaction/warnings as TypeScript', () => {
  const ENRICH = read('MCP/Execute/McpNativeReceiptEnrichment.cpp');
  const REDACTION = read('MCP/Execute/McpNativeReceiptRedaction.cpp');
  const REDACTION_HDR = read('MCP/Execute/McpNativeReceiptRedaction.h');

  it('caps receipt arrays at 200 and free text at 2048, mirroring receipt-redaction.ts', () => {
    expect(REDACTION).toContain('McpMaxReceiptArray = 200');
    expect(REDACTION).toContain('McpMaxReceiptText = 2048');
    expect(REDACTION_HDR).toContain('McpBoundJsonArray');
    expect(REDACTION_HDR).toContain('McpRedactText');
  });

  it('bounds handles/changes/warnings arrays and redacts changes/warnings/error text on the canonical receipt', () => {
    expect(ENRICH).toContain('McpBoundJsonArray(McpExtractReceiptHandles');
    expect(ENRICH).toContain('McpBoundJsonArray(MoveTemp(Changes))');
    expect(ENRICH).toContain('McpBoundJsonArray(MoveTemp(Warnings))');
    expect(ENRICH).toContain('McpRedactText(Change)');
    expect(ENRICH).toContain('McpRedactText(Error.Message)');
  });

  it('populates receipt warnings from the same deprecation metadata as TS deprecationWarnings', () => {
    expect(ENRICH).toContain('DeprecationStatus == TEXT("deprecated")');
    expect(ENRICH).toContain('is deprecated: %s');
    expect(ENRICH).toContain('TryGetStringField(TEXT("guidance")');
  });

  it('masks Authorization: Bearer <token>, bare Bearer, and JSON-like quoted assignments identically to TS', () => {
    expect(REDACTION).toContain('SkipOptionalBearerScheme');
    expect(REDACTION).toContain('SkipOptionalQuote');
    expect(REDACTION).toContain('Bearer');
    expect(REDACTION).toContain('stops at whitespace OR a quote');
  });
});
