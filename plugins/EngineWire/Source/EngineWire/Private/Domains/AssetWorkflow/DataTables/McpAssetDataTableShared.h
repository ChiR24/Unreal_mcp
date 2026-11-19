#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "EngineWireSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/BridgeHelpers/Assets/McpAssetCreation.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"
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
// TOptional properties (FOptionalProperty): older engines have no such header and no such property.
#if __has_include("UObject/PropertyOptional.h")
#include "UObject/PropertyOptional.h"
#endif
#include MCP_USER_DEFINED_STRUCT_HEADER
#include "UserDefinedStructure/UserDefinedStructEditorData.h"

// Mirror the struct handlers' JSON payload accessors
// (GetJsonStringField / GetJsonBoolField / GetJsonNumberField live in
// McpJsonFields.h).

// Single entry point for all DataTable + RowStruct authoring actions.
// Mirrors the existing handler signature style but returns the result object
// through an out-parameter so it can be embedded by the calling layer.
bool HandleDataTableAction(
    FString Action,
    const TSharedPtr<FJsonObject>& Params,
    TSharedPtr<FJsonObject>& OutResult);

// Row-scoped actions (add/get/update/delete/list), implemented in the Rows
// shard and dispatched from HandleDataTableAction.
bool HandleDataTableRowActions(
    const FString& Action,
    const TSharedPtr<FJsonObject>& Params,
    TSharedPtr<FJsonObject>& OutResult);

// Bulk row actions (import/clear), split into McpAssetDataTableRowsBulk.cpp to keep both
// within the 250 pure-line ceiling. Reached through HandleDataTableRowActions,
// which tail-calls it, so the dispatch surface is unchanged.
bool HandleDataTableBulkRowActions(
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
    FString Path = GetJsonStringField(Params, TEXT("dataTablePath"));
    if (Path.IsEmpty()) { OutResult = McpDataTableMakeError(TEXT("MISSING_PARAMETER"), TEXT("dataTablePath is required (e.g. /Game/DataTables/DT_Weapons).")); return nullptr; }
    UDataTable* Table = LoadObject<UDataTable>(nullptr, *Path);
    if (!Table) { OutResult = McpDataTableMakeError(TEXT("ASSET_NOT_FOUND"), *FString::Printf(TEXT("No data table at '%s'."), *Path)); return nullptr; }
    return Table;
}

// Every DataTable write persists unless the caller passes save=false: an edit
// that lived only in memory was silently lost on the next editor restart.
// Returns whether the asset is now on disk.
inline bool McpDataTableSaveIfRequested(const TSharedPtr<FJsonObject>& Params, UObject* Asset)
{
    return GetJsonBoolField(Params, TEXT("save"), true) && McpSafeOperations::McpSafeAssetSave(Asset);
}

inline bool McpStructNumbersParse(const UStruct* Struct, const TSharedPtr<FJsonObject>& Object, const FString& Prefix, FString& OutError);

// The converter reads a string into a number with a parse that never fails, so "ten" was written as 0 and the row was
// not reported invalid. A string for a number must be a number wherever the converter writes it: a column, a struct's
// field, an array's or a set's element, a map's key or value, a fixed array's slots (as many as it has; a lone value
// fills the first), a TOptional's value (an enum byte takes value names). Name says where ("Stats.Damage", "Levels[1]").
inline bool McpNumbersParse(const FProperty* Property, const TSharedPtr<FJsonValue>& Value, const FString& Name, FString& OutError,
                            bool bSlot = false)
{
    const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
    const TSharedPtr<FJsonObject>* Object = nullptr;
    const auto ItemsParse = [&Items, &Name, &OutError](const FProperty* Item, bool bItemSlot, int32 Count)
    {
        for (int32 Index = 0; Index < FMath::Min(Items->Num(), Count); ++Index)
        {
            if (!McpNumbersParse(Item, (*Items)[Index], FString::Printf(TEXT("%s[%d]"), *Name, Index), OutError, bItemSlot)) return false;
        }
        return true;
    };
    if (!Value.IsValid())
    {
        return true;
    }
    if (!bSlot && Property->ArrayDim > 1)
    {
        return Value->TryGetArray(Items) ? ItemsParse(Property, true, Property->ArrayDim) : McpNumbersParse(Property, Value, Name, OutError, true);
    }
#if __has_include("UObject/PropertyOptional.h")
    if (const FOptionalProperty* Optional = CastField<FOptionalProperty>(Property))
    {
        return McpNumbersParse(Optional->GetValueProperty(), Value, Name, OutError, true);
    }
#endif
    const FByteProperty* Byte = CastField<FByteProperty>(Property);
    double Parsed = 0.0;
    if (Property->IsA<FNumericProperty>() && !(Byte && Byte->Enum) && Value->Type == EJson::String &&
        !LexTryParseString(Parsed, *Value->AsString()))
    {
        OutError = FString::Printf(TEXT("'%s' takes a number; '%s' is not a number"), *Name, *Value->AsString());
        return false;
    }
    if (const FStructProperty* Struct = CastField<FStructProperty>(Property))
    {
        return !Value->TryGetObject(Object) || McpStructNumbersParse(Struct->Struct, *Object, Name + TEXT("."), OutError);
    }
    const FArrayProperty* Array = CastField<FArrayProperty>(Property);
    const FSetProperty* Set = CastField<FSetProperty>(Property);
    if (const FProperty* Element = Array ? Array->Inner : Set ? Set->ElementProp : nullptr)
    {
        return !Value->TryGetArray(Items) || ItemsParse(Element, false, MAX_int32);
    }
    const FMapProperty* Map = CastField<FMapProperty>(Property);
    if (Map && Value->TryGetObject(Object))
    {
        for (const auto& Pair : (*Object)->Values)
        {
            const FString Key(*Pair.Key);
            const FString Entry = FString::Printf(TEXT("%s[%s]"), *Name, *Key);
            if (!McpNumbersParse(Map->KeyProp, MakeShared<FJsonValueString>(Key), Entry, OutError) ||
                !McpNumbersParse(Map->ValueProp, Pair.Value, Entry, OutError))
            {
                return false;
            }
        }
    }
    return true;
}

inline bool McpStructNumbersParse(const UStruct* Struct, const TSharedPtr<FJsonObject>& Object, const FString& Prefix, FString& OutError)
{
    for (TFieldIterator<FProperty> It(Struct); It; ++It)
    {
        const FString Name = It->GetAuthoredName();
        if (!McpNumbersParse(*It, Object->TryGetField(Name), Prefix + Name, OutError))
        {
            return false;
        }
    }
    return true;
}

// Build a row from JSON against RowStruct. Returns false (and sets OutError)
// when conversion fails so callers short-circuit instead of writing a partial
// row. OutRowMem is only valid on success; on failure it is left null.
inline bool McpBuildDataTableRow(const UScriptStruct* RowStruct, const TSharedPtr<FJsonObject>& RowData, uint8*& OutRowMem, FString& OutError)
{
    OutRowMem = nullptr;
    OutError.Empty();
    if (!McpStructNumbersParse(RowStruct, RowData, FString(), OutError))
    {
        return false;
    }
    uint8* RowMem = static_cast<uint8*>(FMemory::Malloc(RowStruct->GetStructureSize()));
    RowStruct->InitializeStruct(RowMem);
    // The converter standardizes case on both sides of its property lookup, so a
    // write accepts either spelling; only the read side needed correcting.
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
    // The converter lower-cases the first letter of every field name, so a row
    // read back as {displayName, damage} could not be fed into a write that
    // reports its fields as DisplayName/Damage without re-casing every key. Put
    // the authored spelling back. FJsonObject's key map compares
    // case-insensitively, so HasField cannot tell the two apart - the value has
    // to be pulled and re-set under the authored name.
    for (TFieldIterator<FProperty> It(RowStruct); It; ++It)
    {
        const FString Authored = It->GetAuthoredName();
        if (Authored.IsEmpty()) { continue; }
        if (const TSharedPtr<FJsonValue> Value = Json->TryGetField(Authored))
        {
            Json->RemoveField(Authored);
            Json->SetField(Authored, Value);
        }
    }
    return Json;
}

inline void McpFreeDataTableRow(const UScriptStruct* RowStruct, uint8* RowMem)
{
    RowStruct->DestroyStruct(RowMem);
    FMemory::Free(RowMem);
}
