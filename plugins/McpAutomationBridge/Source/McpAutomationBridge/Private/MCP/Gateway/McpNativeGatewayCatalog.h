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
struct FMcpCapabilityRecord;

// capabilityRevision / schemaRevision on a receipt come straight from the resolved
// record's content/schema hashes, the same runtime sources the TypeScript receipt
// reads, so all three revision strings stay distinct and truthful across transports.
void McpSetReceiptRecordRevisions(const TSharedPtr<FJsonObject>& Receipt, const FString& CapabilityId);

// Refuses (with OutMessage set) a capability whose editorStates exclude "edit"
// while no Play-In-Editor world is running. Returns true when the call may
// proceed. Mirrors the availability gate stage of the TypeScript pipeline, and
// lives here so the execute validation file stays under the pure-line ceiling.
bool McpCheckEditorStateGate(const FMcpCapabilityRecord& Record, FString& OutMessage);

// Discovery (search/describe) is sourced from the generated capability store;
// see McpNativeGatewaySearch.h and McpNativeGatewayDescribe.h.
