// McpNativeGatewayDescribe.h — capability describe for the unreal gateway.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "MCP/Gateway/McpNativeGatewaySearch.h"

class FMcpCapabilityStore;

/** Progressive contract: tool summary -> exact capability contract -> one parameter schema. */
/** describe {} : the canonical parent tools with action counts (no other listing exists). */
TSharedPtr<FJsonObject> McpGatewayDescribeToolOverview(const FMcpDiscoveryQuery& Input, const FMcpCapabilityStore& Store);
/** Copy of a capability input schema without the `action` envelope key. */
TSharedPtr<FJsonObject> McpStripActionFromInputSchema(const TSharedPtr<FJsonObject>& Schema);
TSharedPtr<FJsonObject> McpGatewayDescribeCapability(
	const FMcpDiscoveryQuery& Input, const FMcpCapabilityStore& Store,
	FMcpToolEnabledPredicate IsToolEnabled);
