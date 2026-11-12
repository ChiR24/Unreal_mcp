#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "EngineWireSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/BridgeHelpers/Assets/McpAssetCreation.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"
#include "Safety/McpSafeOperationsAssetSave.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Kismet2/StructureEditorUtils.h"
#include "UserDefinedStructure/UserDefinedStructEditorData.h"
#include MCP_USER_DEFINED_STRUCT_HEADER
#include "Kismet2/BlueprintEditorUtils.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPCompileHelpers.h"
#include "Engine/Blueprint.h"
#include "Foundation/Blueprint/McpBlueprintUtils.h"

// A new UserDefinedStruct in Package with the engine-seeded default member (MemberVar_0) removed. Removing the
// last member makes the engine re-seed a placeholder, so a caller that adds members drops that one afterwards.
inline UUserDefinedStruct* CreateUnseededUserStruct(UPackage* Package, const FString& Name)
{
    UUserDefinedStruct* Struct = FStructureEditorUtils::CreateUserDefinedStruct(Package, FName(*Name), RF_Public | RF_Standalone);
    if (Struct)
    {
        TArray<FGuid> Seeded;
        for (const FStructVariableDescription& Var : FStructureEditorUtils::GetVarDesc(Struct)) { Seeded.Add(Var.VarGuid); }
        for (const FGuid& Guid : Seeded) { FStructureEditorUtils::RemoveVariable(Struct, Guid); }
    }
    return Struct;
}

// Apply `tooltip` and `metadata` {key: value} from Payload to one member.
// Shared by add_struct_member (which declared both and dropped them) and
// set_struct_member_metadata.
inline void ApplyStructMemberTooltipAndMetadata(UUserDefinedStruct* S, const FGuid& G, const TSharedPtr<FJsonObject>& Payload)
{
    const TSharedPtr<FJsonObject>* MetaObj = nullptr;
    if (Payload->TryGetObjectField(TEXT("metadata"), MetaObj) && MetaObj && (*MetaObj).IsValid())
    {
#if ENGINE_MAJOR_VERSION > 5 || ENGINE_MINOR_VERSION >= 5
        for (const auto& Pair : (*MetaObj)->Values) { FStructureEditorUtils::SetMetaData(S, G, *Pair.Key, Pair.Value->AsString()); }
#endif
    }
    const FString Tooltip = GetJsonStringField(Payload, TEXT("tooltip"));
    if (!Tooltip.IsEmpty()) { FStructureEditorUtils::ChangeVariableTooltip(S, G, Tooltip); }
}

// structPath empty -> MISSING_PARAMETER, not loadable -> ASSET_NOT_FOUND; both
// replies are sent here and nullptr comes back.
UUserDefinedStruct* LoadStructOrReply(UEngineWireSubsystem& Bridge, const FString& RequestId,
    TSharedPtr<FMcpBridgeWebSocket> Socket, const FString& StructPath);
FGuid ResolveMemberGuid(UUserDefinedStruct* S, const FString& VarGuidStr, const FString& MemberName);
// The member the payload's varGuid or memberName names; false once "Struct member not found" (MEMBER_NOT_FOUND) is sent.
bool ResolveMemberOrReply(UEngineWireSubsystem& Bridge, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket,
    UUserDefinedStruct* S, const TSharedPtr<FJsonObject>& Payload, FGuid& OutGuid);
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
 * The end of every struct edit: McpRefreshStructDependents (which compiles S), the package dirtied and, with bSave, the
 * save. The save comes last because the engine's compile dirties the package: saved before the refresh, a struct was
 * dirty again a moment later. True when S was saved.
 */
bool FinishStructEdit(UUserDefinedStruct* S, bool bSave);

/**
 * A `members[]` array is validated first and applied second, so a malformed request never leaves a struct half
 * built: create_struct and import_struct validate before they create anything.
 *
 * ValidateStructMembers resolves each entry's type through McpBlueprintUtils::ResolvePinType (SelfStructPath feeds
 * its guard against a struct that holds itself by value) and collects one failure per bad entry: a missing name or
 * type, an unresolved Struct:/Enum:/SoftObject: reference, an illegal map key. An entry may spell its fields
 * name/type or memberName/memberType, and its default as defaultValue (the declared name, which wins) or default.
 * It returns true when nothing failed; OutParsed then goes to ApplyParsedStructMembers.
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

/**
 * add_struct_member's own contract declares a `members` array, but the handler
 * only ever read the single memberName/memberType pair and refused the array
 * form with MISSING_PARAMETER -- so a ten-field struct cost ten calls (twenty,
 * with the type failures that follow from guessing). Validate and apply the
 * whole array in one call, reusing the same parser create_struct uses.
 * Returns false when the payload carries no usable `members` array.
 */
bool AddStructMembersFromArray(
    UEngineWireSubsystem& Bridge,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);

bool HandleStructLifecycleActions(UEngineWireSubsystem& Bridge, const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleStructMemberAddRemoveActions(UEngineWireSubsystem& Bridge, const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleStructMemberEditActions(UEngineWireSubsystem& Bridge, const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleStructAnalysisActions(UEngineWireSubsystem& Bridge, const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleStructListActions(UEngineWireSubsystem& Bridge, const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleStructImportActions(UEngineWireSubsystem& Bridge, const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
// delete_struct; duplicate_struct and rename_struct; refresh_struct_dependencies.
bool HandleStructAssetAction_Delete(UEngineWireSubsystem& Bridge, const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleStructAssetAction_Rename(UEngineWireSubsystem& Bridge, const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleStructAssetAction_Refresh(UEngineWireSubsystem& Bridge, const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
