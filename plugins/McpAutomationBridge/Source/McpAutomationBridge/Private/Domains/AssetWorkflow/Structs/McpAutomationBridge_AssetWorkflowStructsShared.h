#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/BridgeHelpers/Assets/McpAutomationBridgeHelpersAssetCreation.h"
#include "Foundation/Reflection/McpPropertyReflection.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersJsonFields.h"
#include "Safety/McpSafeOperationsAssetSave.h"
// Supplies `using McpSafeOperations::McpSafeAssetSave`, so the handlers below
// can call it unqualified. Enums/Shared.h already pulls this in; these two
// siblings did not, and only compiled where a transitive include happened to
// provide it -- which an installed-engine build does not.
#include "Foundation/BridgeHelpers/Security/McpAutomationBridgeHelpersSafeOperationsFacade.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Kismet2/StructureEditorUtils.h"
#include "UserDefinedStructure/UserDefinedStructEditorData.h"
#include MCP_USER_DEFINED_STRUCT_HEADER
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Engine/Blueprint.h"
#include "K2Node_MakeStruct.h"
#include "K2Node_BreakStruct.h"
#include "EdGraph/EdGraph.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsBlueprintGraph.h"

// Mirror the inventory handlers' JSON payload accessors (GetJsonStringField /
// GetJsonBoolField / GetJsonNumberField live in McpAutomationBridgeHelpersJsonFields.h).
#define GetPayloadString GetJsonStringField
#define GetPayloadBool GetJsonBoolField
#define GetPayloadNumber GetJsonNumberField

FGuid ResolveMemberGuid(UUserDefinedStruct* S, const FString& VarGuidStr, const FString& MemberName);
FString PinTypeToSummary(const FEdGraphPinType& Pin);
FString UserDefinedStructureStatusToString(EUserDefinedStructureStatus Status);
TSharedPtr<FJsonObject> VariableDescriptionToJson(const FStructVariableDescription& Var);
// Native-USTRUCT counterpart to VariableDescriptionToJson: must keep the same member JSON shape.
TSharedPtr<FJsonObject> NativePropertyToMemberJson(FProperty* Prop);
FString BuildDefaultExportText(UUserDefinedStruct* S, FProperty* Prop, const TSharedPtr<FJsonValue>& JsonValue);
void ForEachReferencingBlueprint(UUserDefinedStruct* S, TFunction<void(UBlueprint*)> Callback);

/**
 * Enumerate every asset that references struct S — Blueprints, other
 * UserDefinedStructs (nested struct members), DataAssets, and any other
 * referencer type. Callback receives the raw UObject; caller Casts<> as
 * needed. This is the generalised form of ForEachReferencingBlueprint (which
 * is now a thin wrapper that filters for UBlueprint only).
 */
void ForEachReferencingAsset(UUserDefinedStruct* S, TFunction<void(UObject*)> Callback);

/**
 * Recompile a UserDefinedStruct and every asset that references it, then
 * notify any DataTables whose RowStruct points at it (so their rows stay
 * consistent). Nested UserDefinedStruct referencers are recompiled before
 * Blueprints so downstream BPs rebuild against the updated nested layout.
 * DataAsset referencers are reported (notification only — no recompile needed).
 * Enum-typed members are reported for coverage only (no recompile needed).
 * All out-params are optional; pass nullptr when only the side effect
 * (recompile + notify) is desired. This is the shared implementation behind
 * refresh_struct_dependencies and the auto-trigger that fires after every
 * struct mutation (import, add/remove member, rename, duplicate, ...).
 */
void McpRefreshStructDependents(UUserDefinedStruct* S,
    TArray<FString>* OutBlueprints = nullptr,
    TArray<FString>* OutDataTables = nullptr,
    TArray<FString>* OutEnums = nullptr,
    TArray<FString>* OutStructs = nullptr,
    TArray<FString>* OutDataAssets = nullptr);

/**
 * Validate-then-apply a `members[]` array to a UserDefinedStruct.
 *
 * Validation phase: for every member entry, resolves its `type` through
 * `McpBlueprintUtils::ResolvePinType` with `SelfStructPath` for the
 * recursive by-value self-reference guard. Collects per-member failures
 * (missing name/type, unresolved Struct:/Enum:/SoftObject: refs, illegal
 * map keys) BEFORE mutating the struct — so a malformed request never
 * leaves the struct in a half-populated state.
 *
 * Apply phase: only runs if the validation phase produced zero failures.
 * For each validated member, calls AddVariable, RenameVariable, sets
 * `default`/`defaultValue`, `tooltip` and per-key `metadata`. The struct
 * is NOT compiled/saved here — the caller owns the compile/save boundary
 * (different actions want different post-conditions: e.g. create_struct
 * saves eagerly when save=true, import_struct always saves on success).
 *
 * Returns true on a clean validate+apply (Failures.Num() == 0). On any
 * validation failure, no members are added and Failures is populated;
 * apply is skipped entirely.
 *
 * Field-name policy: the schema advertises `defaultValue`, but legacy
 * callers used `default`. Both are accepted for parity (the schema field
 * wins when both are present).
 */
bool ApplyStructMembers(
    UUserDefinedStruct* S,
    const TArray<TSharedPtr<FJsonValue>>& Members,
    const FName& SelfStructPath,
    int32& OutImported,
    TArray<FString>& Failures);

/**
 * Validation phase of ApplyStructMembers, exposed separately so callers that
 * do not yet have a struct instance (e.g. create_struct's pre-create check)
 * can run the same validation run before committing to asset creation.
 *
 * Returns the array of validated members' (TypeStr, resolved PinType, name,
 * default, tooltip, metadata) plus per-member failure messages. When the
 * returned Failures is empty, the caller can hand OutParsed to
 * ApplyParsedStructMembers for the (now-cannot-fail) mutation.
 *
 * SelfStructPath is used by ResolvePinType's recursive by-value
 * self-reference guard.
 */
struct FParsedMember
{
    FString Name;
    FEdGraphPinType PinType;
    FString TypeStr;
    FString Default;
    FString Tooltip;
    TSharedPtr<FJsonObject> Metadata;
};

bool ValidateStructMembers(
    const TArray<TSharedPtr<FJsonValue>>& Members,
    const FName& SelfStructPath,
    TArray<FParsedMember>& OutParsed,
    TArray<FString>& Failures);

/**
 * Apply the output of ValidateStructMembers to a UserDefinedStruct. The struct
 * is mutated in place; the caller owns compile/save. Returns the count of
 * members that were actually applied (will equal OutParsed.Num() unless an
 * unexpected AddVariable failure happens — those go into Failures).
 */
int32 ApplyParsedStructMembers(
    UUserDefinedStruct* S,
    const TArray<FParsedMember>& Parsed,
    TArray<FString>& Failures);

bool HandleStructAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleStructLifecycleActions(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleStructMemberAddRemoveActions(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleStructMemberEditActions(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleStructAnalysisActions(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleStructSerializationActions(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleStructImportActions(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleStructAssetActions(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleStructPropertyAction(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
