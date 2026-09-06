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

// capabilityRevision / schemaRevision on a receipt come straight from the resolved
// record's content/schema hashes, the same runtime sources the TypeScript receipt
// reads, so all three revision strings stay distinct and truthful across transports.
void McpSetReceiptRecordRevisions(const TSharedPtr<FJsonObject>& Receipt, const FString& CapabilityId);

// Discovery (search/describe) is sourced from the generated capability store;
// see McpNativeGatewaySearch.h and McpNativeGatewayDescribe.h.
