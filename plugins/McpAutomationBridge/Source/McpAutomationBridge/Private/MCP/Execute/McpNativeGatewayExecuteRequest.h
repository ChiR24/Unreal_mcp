// McpNativeGatewayExecuteRequest.h — accepted execute request forms
//
// Two forms are accepted and normalize to exactly one capability:
//   canonical v2 : { capability, params, options }
//   generated    : { tool, action, params, options }
//
// When both are supplied they must designate the same capability; a
// disagreement is reported as FORM_CONFLICT rather than one form silently
// winning. Cross-cutting execution controls live only in `options` (the Task 3
// key set); a control smuggled into `params` is rejected.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Foundation/McpLiveStateRevisions.h"
#include "MCP/Execute/McpNativeGatewayReceipt.h"

struct FMcpCapabilityRecord;

/** Bounded execution option keys, mirroring EXECUTION_OPTION_KEYS in TypeScript. */
const TArray<FString>& McpExecutionOptionKeys();

/** Upper bound for options.timeoutMs, mirroring MAX_TIMEOUT_MS in TypeScript. */
constexpr int64 McpMaxExecutionTimeoutMs = 600000;

struct FMcpGatewayExecuteRequest
{
	const FMcpCapabilityRecord* Record = nullptr;
	FString CapabilityId;
	TSharedPtr<FJsonObject> Params;
	TSharedPtr<FJsonObject> Options;
};

/**
 * Resolve and shape-check one execute request.
 * Returns true on success; otherwise OutError carries the typed failure and
 * OutGuidance carries the suggestions/nextCall payload when one applies.
 */
bool McpParseGatewayExecuteRequest(
	const TSharedPtr<FJsonObject>& Params, FMcpGatewayExecuteRequest& OutRequest,
	FMcpSemanticError& OutError, TSharedPtr<FJsonObject>& OutGuidance);

/** Validate the `options` envelope alone. Returns true when absent or valid. */
bool McpValidateExecutionOptions(
	const TSharedPtr<FJsonObject>& Options, FMcpSemanticError& OutError);

/**
 * Validate `options.idempotencyKey` alone: absent, or a string of 1..128
 * characters, mirroring IdempotencyKeySchema in TypeScript. Implemented in
 * McpNativeGatewayIdempotency.cpp, beside the ledger the key addresses.
 */
bool McpValidateIdempotencyKeyOption(
	const TSharedPtr<FJsonObject>& Options, FMcpSemanticError& OutError);

/**
 * Validate `options` for one resolved capability: the envelope rules above,
 * then the fail-closed `preview` gate. Implemented in McpNativeGatewayPreview.cpp.
 *
 * No dispatch path implements a dry run, so `preview: true` is refused as
 * UNSUPPORTED_PREVIEW before the request is queued rather than performing the
 * real mutation and reporting it as a preview.
 */
bool McpValidateExecuteOptionsForCapability(
	const TSharedPtr<FJsonObject>& Options, const TSharedPtr<FJsonObject>& Params,
	const FMcpCapabilityRecord* Record, FMcpSemanticError& OutError,
	TSharedPtr<FJsonObject>& OutGuidance);

/**
 * Parse `options.expectedRevisions` into typed live-state preconditions.
 *
 * An absent envelope or absent key pins nothing and returns true, so a caller
 * that never opts in is never refused. A refusal leaves OutRevisions empty so a
 * partially parsed pin set can never be enforced.
 */
bool McpParseExpectedRevisions(
	const TSharedPtr<FJsonObject>& Options,
	TMap<EMcpStateKind, int64>& OutRevisions,
	FMcpSemanticError& OutError);
