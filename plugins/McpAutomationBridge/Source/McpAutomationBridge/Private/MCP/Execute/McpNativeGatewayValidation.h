// McpNativeGatewayValidation.h — canonical execute validation for the unreal gateway
//
// Stage order is normative and matches the TypeScript reference
// (tests/unit/task-27-suite/execute-reference.ts): resolve form and alias ->
// registry -> dynamic enabled state -> options -> defaults -> exact per-action
// input schema. Nothing reaches the subsystem queue until every stage passes.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

class FMcpToolRegistry;
class FMcpDynamicToolManager;

struct FMcpGatewayExecutePlan
{
	FString CapabilityId;
	FString ParentTool;

	/** Client-facing action carried in the payload, not routing.dispatchAction. */
	FString LegacyAction;

	/** Automation action on the wire, resolved from the registry dispatch pattern. */
	FString DispatchAction;

	TSharedPtr<FJsonObject> Arguments;
	TSharedPtr<FJsonObject> OutputSchema;
};

/**
 * Validate one gateway execute payload.
 *
 * Returns nullptr when OutPlan is ready to dispatch; otherwise returns the
 * semantic error receipt to send back unchanged. Rejects unknown capabilities,
 * ambiguous aliases, conflicting request forms, disabled capabilities,
 * unsupported options, and any input the exact per-action schema refuses.
 */
TSharedPtr<FJsonObject> ValidateAndResolveGatewayExecute(
	const TSharedPtr<FJsonObject>& GatewayParams,
	const FMcpToolRegistry& Registry, const FMcpDynamicToolManager& ToolManager,
	const FString& CorrelationId, FMcpGatewayExecutePlan& OutPlan);

/**
 * Validate a handler result against the capability output schema.
 *
 * Returns nullptr when the result conforms; otherwise the error receipt. The
 * handler payload is preserved on the error as structured Unreal detail so a
 * schema violation never discards what Unreal actually reported.
 */
TSharedPtr<FJsonObject> ValidateGatewayExecuteOutput(
	const FString& CapabilityId, const TSharedPtr<FJsonObject>& OutputSchema,
	const TSharedPtr<FJsonObject>& Result, const FString& CorrelationId);

/**
 * Per-action strictness for the direct (non-gateway) tools/call path.
 *
 * Resolves {ParentTool, arguments.action} to its canonical record and validates
 * against that action's exact input schema. Callers must gate on
 * McpCanonicalRecordsAvailable() first.
 */
bool McpValidateCanonicalToolArguments(
	const FString& ParentTool, const TSharedPtr<FJsonObject>& Arguments,
	FString& OutArgumentPath, FString& OutErrorCode, FString& OutErrorMessage);
