#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/BridgeHelpers/Assets/McpAutomationBridgeHelpersAssetCreation.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersJsonFields.h"
#include "Safety/McpSafeOperationsAssetSave.h"
// Supplies `using McpSafeOperations::McpSafeAssetSave`, which makes the
// unqualified call below legal in any unity-build blob.
#include "Safety/McpSafeOperations.h"
#include "AssetRegistry/AssetRegistryModule.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

#include "Engine/UserDefinedEnum.h"

// Mirror the struct/datatable handlers' JSON payload accessors
// (GetJsonStringField / GetJsonBoolField / GetJsonNumberField live in
// McpAutomationBridgeHelpersJsonFields.h).

// Single entry point for all UserDefinedEnum authoring actions. Mirrors the
// DataTable handler signature style: the result object is returned through an
// out-parameter so the calling layer can embed it in its own response envelope.
bool HandleEnumAction(
    FString Action,
    const TSharedPtr<FJsonObject>& Params,
    TSharedPtr<FJsonObject>& OutResult);

// Lifecycle actions: create_enum, delete_enum, get_enum.
bool HandleEnumLifecycleActions(
    const FString& Action,
    const TSharedPtr<FJsonObject>& Params,
    TSharedPtr<FJsonObject>& OutResult);

// Value-scoped actions: add/remove/rename/reorder/metadata/split.
bool HandleEnumValueActions(
    const FString& Action,
    const TSharedPtr<FJsonObject>& Params,
    TSharedPtr<FJsonObject>& OutResult);

// Resolve a UUserDefinedEnum from the "enumPath" payload field, or nullptr.
inline UUserDefinedEnum* ResolveUserDefinedEnum(const TSharedPtr<FJsonObject>& Params)
{
    FString EnumPath = GetJsonStringField(Params, TEXT("enumPath"));
    if (EnumPath.IsEmpty())
    {
        return nullptr;
    }
    return LoadObject<UUserDefinedEnum>(nullptr, *EnumPath);
}

// Fill OutResult with a minimal success/error envelope used by the
// OutResult-style handlers. A failure also carries error + errorCode, which the
// manage_asset dispatcher turns into a failed response (it used to answer every
// enum action with success=true, so "Enum not found" read as a success).
inline void SetEnumResultFields(TSharedPtr<FJsonObject>& OutResult, bool bSuccess, const FString& Message, const TCHAR* ErrorCode = TEXT("ENUM_ACTION_FAILED"))
{
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), bSuccess);
    Result->SetStringField(TEXT("message"), Message);
    if (!bSuccess)
    {
        Result->SetStringField(TEXT("error"), Message);
        Result->SetStringField(TEXT("errorCode"), ErrorCode);
    }
    OutResult = Result;
}

// Enum edits save unless the caller passes save:false, like DataTable and struct edits: an
// unsaved enum change is gone after the next editor restart.
inline bool EnumSaveRequested(const TSharedPtr<FJsonObject>& Params)
{
    return GetJsonBoolField(Params, TEXT("save"), true);
}

// Commit a UUserDefinedEnum mutation: refresh editor state, mark the package
// dirty, and persist through the safe save wrapper when bSave is set.
inline void FinalizeEnum(UUserDefinedEnum* Enum, bool bSave)
{
    Enum->PostEditChange();
    Enum->GetOutermost()->MarkPackageDirty();
    if (bSave)
    {
        McpSafeAssetSave(Enum);
    }
}

// Resolve the enum from Params; on failure populate OutResult and set bHandled
// so the caller can early-return without duplicating the not-found boilerplate.
inline UUserDefinedEnum* RequireEnum(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject>& OutResult, bool& bHandled)
{
    bHandled = false;
    UUserDefinedEnum* Enum = ResolveUserDefinedEnum(Params);
    if (!Enum)
    {
        SetEnumResultFields(OutResult, false, FString::Printf(TEXT("No user-defined enum at enumPath '%s'."), *GetJsonStringField(Params, TEXT("enumPath"))), TEXT("ASSET_NOT_FOUND"));
        bHandled = true;
    }
    return Enum;
}
