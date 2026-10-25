#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "MCP/Execute/McpNativeGatewaySchemaValidation.h"

struct FMcpCapabilityRecord;

// The per-variant rules of a folded family. Its union schema lists every variant's parameters
// and keeps only what every variant requires; routing.dispatchBy.declaredBy and .requires say
// which variant reads and requires what. Mirrors of gateway-dispatch-by.ts.

/** Parameters a call sent that only other variants read: one receipt warning each (unreadVariantParams). */
TArray<FString> McpUnreadVariantParams(
	const FMcpCapabilityRecord& Record, const TArray<FString>& Sent, const TSharedPtr<FJsonObject>& Params);

/**
 * False, with the violation, when the dispatched variant misses a parameter its own record requires
 * (missingVariantRequirement): debugOp=draw without location reached the engine and failed there.
 */
bool McpCheckVariantRequirements(
	const FMcpCapabilityRecord& Record, const FString& DispatchTarget,
	const TSharedPtr<FJsonObject>& Params, FMcpSchemaViolationDetail& OutViolation);

/** Selector value -> what its variant requires beyond the union schema, a one-of group as "a|b"; null when none. */
TSharedPtr<FJsonObject> McpVariantRequired(const FMcpCapabilityRecord& Record);

/** describe: requiredFor on a parameter only some variants require, variantRequired on the selector. */
void McpSetVariantRequirementViews(
	const TSharedPtr<FJsonObject>& View, const FMcpCapabilityRecord& Record, const FString& Name);
