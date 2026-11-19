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

/** availability.minEngineVersion ("5.4"): the first engine whose build has the capability; empty when every 5.x has it. */
FString McpRecordMinEngineVersion(const FMcpCapabilityRecord& Record);

/** Whether engine Major.Minor is at least Version ("5.4"); a version that is not two numbers is never met. */
bool McpEngineVersionAtLeast(const FString& Version, int32 Major, int32 Minor);

/** The first feature availability.compiled names that this build did not compile (McpCompiledFeatures), else empty. */
FString McpMissingCompiledFeature(const FMcpCapabilityRecord& Record);

/** Whether this editor has the capability: this build compiled every feature it lists, and its minEngineVersion, if any, is met. */
bool McpRunsOnThisEngine(const FMcpCapabilityRecord& Record);

/**
 * availability as search and describe report it: status (available, disabled when the parent tool is off,
 * unavailable when this build left out a feature the record needs or this engine is older than its minEngineVersion)
 * and the reasons (not_compiled with missingFeature, engine_version), then the record's declared plugins, editor
 * states, compiled features and minimum engine version.
 */
TSharedPtr<FJsonObject> McpCapabilityAvailabilityView(const FMcpCapabilityRecord& Record, bool bEnabled);

/**
 * One search row. Its nextCall describes the capability, or enables a disabled tool through configure first;
 * runnable is false for a disabled tool or an engine older than the record needs.
 */
TSharedPtr<FJsonObject> McpSearchRow(const FMcpCapabilityRecord& Record, bool bEnabled);

/** The filters a search applied, as sent. */
TSharedPtr<FJsonObject> McpSearchFilters(const FMcpDiscoveryQuery& Input);

// Refuses (OutCode and OutMessage set) a capability whose feature this build left out (FEATURE_UNAVAILABLE, naming it),
// one this engine is too old for (ENGINE_VERSION_UNSUPPORTED), one that
// is not a read while a Live Coding compile runs (LIVE_CODING_ACTIVE: the engine marks nothing unsaved then), or one
// whose editorStates exclude "edit" while no Play-In-Editor world runs (EDITOR_STATE_MISMATCH). Returns true when
// the call may proceed. Lives here so the execute validation file stays under the pure-line ceiling.
bool McpCheckAvailabilityGate(const FMcpCapabilityRecord& Record, FString& OutCode, FString& OutMessage);

// Discovery (search/describe) is sourced from the generated capability store;
// see McpNativeGatewaySearch.h and McpNativeGatewayDescribe.h.
