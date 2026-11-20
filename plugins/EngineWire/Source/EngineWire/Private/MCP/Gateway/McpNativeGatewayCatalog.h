// McpNativeGatewayCatalog.h — what a capability record shows a caller: its availability, its search row, and the
// availability gate execute applies (engine version, editor state).

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

class FMcpToolRegistry;
class FMcpDynamicToolManager;
class FMcpToolDefinition;
struct FMcpCapabilityRecord;
struct FMcpDiscoveryQuery;

/** The first feature availability.compiled names that this build did not compile (McpCompiledFeatures), else empty. */
FString McpMissingCompiledFeature(const FMcpCapabilityRecord& Record);

/** The first engine role (or role domain) availability.roles names that nothing plays on this engine at the last
 *  refresh (McpEngineRoles), else empty; OutWhy says what was searched. Any thread. */
FString McpMissingEngineRole(const FMcpCapabilityRecord& Record, FString* OutWhy = nullptr);

/** Whether this editor has the capability: this build compiled every feature it lists, and every role it lists resolves. */
bool McpRunsOnThisEngine(const FMcpCapabilityRecord& Record);

/**
 * availability as search and describe report it: status (available, disabled when the parent tool is off,
 * unavailable when this build left out a feature the record needs or nothing here plays one of its engine roles)
 * and the reasons (not_compiled with missingFeature; engine_role with missingRole and missingRoleWhy), then the
 * record's declared plugins, editor states, compiled features and roles.
 */
TSharedPtr<FJsonObject> McpCapabilityAvailabilityView(const FMcpCapabilityRecord& Record, bool bEnabled);

/**
 * One search row. Its nextCall describes the capability, or enables a disabled tool through configure first;
 * runnable is false for a disabled tool or an engine older than the record needs.
 */
TSharedPtr<FJsonObject> McpSearchRow(const FMcpCapabilityRecord& Record, bool bEnabled);

/** The filters a search applied, as sent. */
TSharedPtr<FJsonObject> McpSearchFilters(const FMcpDiscoveryQuery& Input);

// Refuses (OutCode and OutMessage set) a capability whose feature this build left out or one of whose engine roles
// nothing plays here (FEATURE_UNAVAILABLE, naming it), one that
// is not a read while a Live Coding compile runs (LIVE_CODING_ACTIVE: the engine marks nothing unsaved then), or one
// whose editorStates exclude "edit" while no Play-In-Editor world runs (EDITOR_STATE_MISMATCH). Returns true when
// the call may proceed. Lives here so the execute validation file stays under the pure-line ceiling.
bool McpCheckAvailabilityGate(const FMcpCapabilityRecord& Record, FString& OutCode, FString& OutMessage);

// Discovery (search/describe) is sourced from the generated capability store;
// see McpNativeGatewaySearch.h and McpNativeGatewayDescribe.h.
