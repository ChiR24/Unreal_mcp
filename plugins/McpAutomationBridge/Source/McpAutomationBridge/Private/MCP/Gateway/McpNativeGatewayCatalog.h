// McpNativeGatewayCatalog.h — registry-schema readers for the execute path.
//
// Error envelopes and closest-match guidance live in McpNativeGatewayGuidance.h,
// which this header re-exports so existing execute-path includes keep working.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "MCP/Gateway/McpNativeGatewayGuidance.h"

class FMcpToolRegistry;
class FMcpDynamicToolManager;
class FMcpToolDefinition;


/** Exact action enum for a canonical tool, read from its input schema 'action' field. */
TArray<FString> GatewayGetActionValues(FMcpToolDefinition* Tool);

/** Declared parameter field names (schema properties minus action/subAction/params). */
TArray<FString> GatewayGetParameterNames(FMcpToolDefinition* Tool);

// Discovery (search/describe) is sourced from the generated capability store;
// see McpNativeGatewaySearch.h and McpNativeGatewayDescribe.h.
