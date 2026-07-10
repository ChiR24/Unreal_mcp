#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/BridgeHelpers/Assets/McpAutomationBridgeHelpersAssetCreation.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersJsonFields.h"
#include "Safety/McpSafeOperationsAssetSave.h"
#include "AssetRegistry/AssetRegistryModule.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "JsonObjectConverter.h"

#include "Engine/DataTable.h"
#include "Kismet2/StructureEditorUtils.h"
#include "StructUtils/UserDefinedStruct.h"
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
