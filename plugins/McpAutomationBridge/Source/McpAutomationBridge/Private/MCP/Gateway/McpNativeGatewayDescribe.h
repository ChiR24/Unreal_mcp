// McpNativeGatewayDescribe.h — capability describe for the unreal gateway.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "MCP/Gateway/McpNativeGatewaySearch.h"

class FMcpCapabilityStore;

/** Progressive contract: tool summary -> exact capability contract -> one parameter schema. */
TSharedPtr<FJsonObject> McpGatewayDescribeCapability(
	const FMcpDiscoveryQuery& Input, const FMcpCapabilityStore& Store,
	FMcpToolEnabledPredicate IsToolEnabled);
