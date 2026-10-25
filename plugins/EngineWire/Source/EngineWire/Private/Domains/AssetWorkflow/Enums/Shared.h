#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "EngineWireSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/BridgeHelpers/Assets/McpAssetCreation.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"
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
// McpJsonFields.h).

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

// split_enum: a new enum holding the named values, or every value from an index on; the source is left as it is.
bool HandleEnumSplit(
    const FString& Action,
    const TSharedPtr<FJsonObject>& Params,
    TSharedPtr<FJsonObject>& OutResult);

// The index of the value named Requested, by its internal name or by its display name (rename_enum_value changes only
// the display name, which keeps every stored value), any case; INDEX_NONE when none matches. The hidden _MAX is no value.
inline int32 McpFindEnumValue(const UUserDefinedEnum* Enum, const FString& Requested)
{
    for (int32 Index = 0; Enum && Index < Enum->NumEnums(); ++Index)
    {
        const FString Short = Enum->GetNameStringByIndex(Index);
        if (!Short.EndsWith(TEXT("_MAX")) && (Short == Requested || Enum->GetDisplayNameTextByIndex(Index).ToString() == Requested))
        {
            return Index;
        }
    }
    return INDEX_NONE;
}

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
// dirty, and persist through the safe save wrapper when bSave is set. True when it was saved.
inline bool FinalizeEnum(UUserDefinedEnum* Enum, bool bSave)
{
    Enum->PostEditChange();
    Enum->GetOutermost()->MarkPackageDirty();
    return bSave && McpSafeAssetSave(Enum);
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
