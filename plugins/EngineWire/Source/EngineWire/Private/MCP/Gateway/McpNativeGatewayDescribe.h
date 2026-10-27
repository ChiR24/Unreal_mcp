// McpNativeGatewayDescribe.h — capability describe for the unreal gateway.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "MCP/Gateway/McpNativeGatewaySearch.h"

class FMcpCapabilityStore;

/** Progressive discovery, as on the TS door: catalog -> domain -> family -> one capability -> one parameter. */
/** describe without a tool: the domains, one domain's families, or one family's capabilities (gateway-describe-browse.ts). */
TSharedPtr<FJsonObject> McpGatewayDescribeBrowse(
	const FMcpDiscoveryQuery& Input, const FMcpCapabilityStore& Store, FMcpToolEnabledPredicate IsToolEnabled);
/** describe { tool }: the parent's action names and a browse call, the whole list unless the caller sent limit. */
TSharedPtr<FJsonObject> McpGatewayToolSummary(
	const FString& Tool, const TArray<FString>& Actions, const FMcpDiscoveryQuery& Input, bool bEnabled);
/** Copy of a capability input schema without the `action` envelope key. */
TSharedPtr<FJsonObject> McpStripActionFromInputSchema(const TSharedPtr<FJsonObject>& Schema);
TSharedPtr<FJsonObject> McpGatewayDescribeCapability(
	const FMcpDiscoveryQuery& Input, const FMcpCapabilityStore& Store,
	FMcpToolEnabledPredicate IsToolEnabled);
