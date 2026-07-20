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
