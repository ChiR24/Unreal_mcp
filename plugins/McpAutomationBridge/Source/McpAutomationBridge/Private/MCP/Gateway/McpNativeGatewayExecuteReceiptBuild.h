// McpNativeGatewayExecuteReceiptBuild.h — turn one completed automation
// response into the gateway execute receipt.
//
// Extracted from McpNativeTransportPendingRequests.cpp: it is receipt
// construction, not socket bookkeeping, and it takes plain values rather than
// an FSSEConnection so it carries no transport dependency.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "MCP/Execute/McpNativeGatewayReceipt.h"

/**
 * Build the semantic receipt for a finished gateway execute.
 *
 * A failure classifies onto the typed error algebra; a success is bounded by
 * the response-size policy and then projected against `OutputSchema`, so a
 * schema violation is reported as an error rather than returned as a success.
 */
TSharedPtr<FJsonObject> McpBuildGatewayExecuteReceipt(
	const FString& CapabilityId, const TSharedPtr<FJsonObject>& OutputSchema,
	const FMcpReceiptContext& Context, bool bSuccess, const FString& Message,
	const TSharedPtr<FJsonObject>& Result, const FString& ErrorCode);
