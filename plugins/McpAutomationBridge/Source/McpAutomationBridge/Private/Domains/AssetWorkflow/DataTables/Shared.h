#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/BridgeHelpers/Assets/McpAutomationBridgeHelpersAssetCreation.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersJsonFields.h"
#include "Safety/McpSafeOperationsAssetSave.h"
// Supplies `using McpSafeOperations::McpSafeAssetSave`, so the handlers below
// can call it unqualified. Enums/Shared.h already pulls this in; these two
// siblings did not, and only compiled where a transitive include happened to
// provide it -- which an installed-engine build does not.
#include "Foundation/BridgeHelpers/Security/McpAutomationBridgeHelpersSafeOperationsFacade.h"
#include "AssetRegistry/AssetRegistryModule.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "JsonObjectConverter.h"

#include "Engine/DataTable.h"
#include "Kismet2/StructureEditorUtils.h"
#include "Core/Compatibility/McpVersionCompatibility.h"
#include MCP_USER_DEFINED_STRUCT_HEADER
#include "UserDefinedStructure/UserDefinedStructEditorData.h"

// Mirror the struct handlers' JSON payload accessors
// (GetJsonStringField / GetJsonBoolField / GetJsonNumberField live in
// McpAutomationBridgeHelpersJsonFields.h).
#define GetPayloadString GetJsonStringField
#define GetPayloadBool GetJsonBoolField
#define GetPayloadNumber GetJsonNumberField

// Single entry point for all DataTable + RowStruct authoring actions.
// Mirrors the existing handler signature style but returns the result object
// through an out-parameter so it can be embedded by the calling layer.
bool HandleDataTableAction(
    FString Action,
    const TSharedPtr<FJsonObject>& Params,
    TSharedPtr<FJsonObject>& OutResult);

// Row-scoped actions (add/get/update/delete/list/import/clear), implemented in
// the Rows shard and dispatched from HandleDataTableAction.
bool HandleDataTableRowActions(
    const FString& Action,
    const TSharedPtr<FJsonObject>& Params,
    TSharedPtr<FJsonObject>& OutResult);

// Inline so the Unity-merged shards do not collide on duplicate definitions.
// Named McpDataTableMakeError (not MakeError) to avoid the engine's
// MakeError(...) template in ValueOrError.h, which would otherwise win overload
// resolution and return a TValueOrError proxy instead of a JSON error object.
inline TSharedPtr<FJsonObject> McpDataTableMakeError(const TCHAR* Code, const TCHAR* Msg)
{
    TSharedPtr<FJsonObject> R = McpHandlerUtils::CreateResultObject();
    R->SetStringField(TEXT("error"), Msg ? Msg : Code);
    R->SetStringField(TEXT("errorCode"), Code);
    return R;
}

inline UDataTable* ResolveDataTable(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject>& OutResult)
{
    FString Path = GetPayloadString(Params, TEXT("dataTablePath"));
    if (Path.IsEmpty()) { OutResult = McpDataTableMakeError(TEXT("MISSING_PARAMETER"), nullptr); return nullptr; }
    UDataTable* Table = LoadObject<UDataTable>(nullptr, *Path);
    if (!Table) { OutResult = McpDataTableMakeError(TEXT("ASSET_NOT_FOUND"), nullptr); return nullptr; }
    return Table;
}

// Build a row from JSON against RowStruct. Returns false (and sets OutError)
// when conversion fails so callers short-circuit instead of writing a partial
// row. OutRowMem is only valid on success; on failure it is left null.
inline bool McpBuildDataTableRow(const UScriptStruct* RowStruct, const TSharedPtr<FJsonObject>& RowData, uint8*& OutRowMem, FString& OutError)
{
    OutRowMem = nullptr;
    OutError.Empty();
    uint8* RowMem = static_cast<uint8*>(FMemory::Malloc(RowStruct->GetStructureSize()));
    RowStruct->InitializeStruct(RowMem);
    const bool bOk = FJsonObjectConverter::JsonObjectToUStruct(RowData.ToSharedRef(), RowStruct, RowMem, 0, 0);
    if (!bOk)
    {
        RowStruct->DestroyStruct(RowMem);
        FMemory::Free(RowMem);
        OutError = TEXT("Row data failed validation against the assigned row struct");
        return false;
    }
    OutRowMem = RowMem;
    return true;
}

inline TSharedPtr<FJsonObject> McpExportDataTableRow(const UScriptStruct* RowStruct, const void* RowMem)
{
    TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
    FJsonObjectConverter::UStructToJsonObject(RowStruct, RowMem, Json.ToSharedRef(), 0, 0);
    return Json;
}

inline void McpFreeDataTableRow(const UScriptStruct* RowStruct, uint8* RowMem)
{
    RowStruct->DestroyStruct(RowMem);
    FMemory::Free(RowMem);
}
