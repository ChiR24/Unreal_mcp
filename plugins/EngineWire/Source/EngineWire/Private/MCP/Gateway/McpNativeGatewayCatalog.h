// McpNativeGatewayCatalog.h — what a capability record shows a caller: its availability, its search row, and the
// editor-state gate execute applies.
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
struct FMcpDiscoveryQuery;

/** availability as both doors report it: status and reasons from the parent tool, then the record's plugins and editor states. */
TSharedPtr<FJsonObject> McpCapabilityAvailabilityView(const FMcpCapabilityRecord& Record, bool bEnabled);

/**
 * One search row as a client reads it on both doors (capabilitySearchRow in gateway-capability-view.ts, less the
 * revision hashes, ranking reasons and category both doors leave out). Its nextCall describes the capability, or
 * enables a disabled tool through configure first.
 */
TSharedPtr<FJsonObject> McpSearchRow(const FMcpCapabilityRecord& Record, bool bEnabled);

/** The filters a search applied, as sent (declaredFilters in gateway-search.ts). */
TSharedPtr<FJsonObject> McpSearchFilters(const FMcpDiscoveryQuery& Input);

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
