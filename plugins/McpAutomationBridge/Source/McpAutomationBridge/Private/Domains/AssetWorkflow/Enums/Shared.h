#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/BridgeHelpers/Assets/McpAutomationBridgeHelpersAssetCreation.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersJsonFields.h"
#include "Safety/McpSafeOperationsAssetSave.h"
// Supplies `using McpSafeOperations::McpSafeAssetSave`, which makes the
// unqualified call below legal in any unity-build blob.
#include "Foundation/BridgeHelpers/Security/McpAutomationBridgeHelpersSafeOperationsFacade.h"
#include "AssetRegistry/AssetRegistryModule.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

#include "Engine/UserDefinedEnum.h"

// Mirror the struct/datatable handlers' JSON payload accessors
// (GetJsonStringField / GetJsonBoolField / GetJsonNumberField live in
// McpAutomationBridgeHelpersJsonFields.h).
#define GetPayloadString GetJsonStringField
#define GetPayloadBool GetJsonBoolField
#define GetPayloadNumber GetJsonNumberField

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
    FString EnumPath = GetPayloadString(Params, TEXT("enumPath"));
    if (EnumPath.IsEmpty())
    {
        return nullptr;
    }
    return LoadObject<UUserDefinedEnum>(nullptr, *EnumPath);
}

// Fill OutResult with a minimal success/error envelope used by the
// OutResult-style handlers (mirrors HandleDataTableAction behavior).
inline void SetEnumResultFields(TSharedPtr<FJsonObject>& OutResult, bool bSuccess, const FString& Message)
{
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), bSuccess);
    Result->SetStringField(TEXT("message"), Message);
    OutResult = Result;
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
        SetEnumResultFields(OutResult, false, TEXT("Enum not found"));
        bHandled = true;
    }
    return Enum;
}
