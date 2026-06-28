// McpAutomationBridge_DataTableHandlers.cpp
// Data Table Handlers
//
// Complete data table management including:
// - create_data_table: Create a new UDataTable from a struct type
// - list_rows: List all row names in a data table
// - get_row: Get a specific row by name with all column values
// - add_row: Add a new row with JSON column values
// - edit_row: Edit an existing row's column values
// - remove_row: Remove a row by name
// - get_structure: Get the struct definition (column names and types)
// - import_json: Import rows from a JSON string
// - export_json: Export all rows as JSON

#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Engine/DataTable.h"
#include "Factories/DataTableFactory.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "JsonObjectConverter.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/UnrealType.h"
#include "AssetRegistry/AssetRegistryModule.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogMcpDataTableHandlers, Log, All);

// ============================================================================
// Helper Functions
// ============================================================================

namespace DataTableHelpers
{
#if WITH_EDITOR

    /**
     * Load a UDataTable asset by path.
     */
    UDataTable* LoadDataTable(const FString& AssetPath)
    {
        FString SanitizedPath = SanitizeProjectRelativePath(AssetPath);
        if (SanitizedPath.IsEmpty())
        {
            return nullptr;
        }
        return LoadObject<UDataTable>(nullptr, *SanitizedPath);
    }

    /**
     * Serialize a single row's property values to a JSON object.
     * Iterates FProperty fields on the row struct and exports each value.
     */
    TSharedPtr<FJsonObject> RowToJson(const UScriptStruct* RowStruct, const uint8* RowData)
    {
        TSharedPtr<FJsonObject> RowJson = MakeShareable(new FJsonObject());
        if (!RowStruct || !RowData)
        {
            return RowJson;
        }

        for (TFieldIterator<FProperty> PropIt(RowStruct); PropIt; ++PropIt)
        {
            FProperty* Property = *PropIt;
            const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(RowData);

            FString ValueStr;
            MCP_PROPERTY_EXPORT_TEXT(Property, ValueStr, ValuePtr, nullptr, nullptr, PPF_None);
            RowJson->SetStringField(Property->GetName(), ValueStr);
        }

        return RowJson;
    }

    /**
     * Deserialize JSON values into a row's property fields.
     * For each key in the JSON object, finds the matching FProperty and imports the text value.
     * Returns true if at least one property was set.
     */
    bool JsonToRow(const UScriptStruct* RowStruct, uint8* RowData, const TSharedPtr<FJsonObject>& JsonValues, FString& OutError)
    {
        if (!RowStruct || !RowData || !JsonValues.IsValid())
        {
            OutError = TEXT("Invalid struct, row data, or JSON values");
            return false;
        }

        int32 PropertiesSet = 0;
        for (const auto& Pair : JsonValues->Values)
        {
            FProperty* Property = RowStruct->FindPropertyByName(FName(*Pair.Key));
            if (!Property)
            {
                UE_LOG(LogMcpDataTableHandlers, Warning, TEXT("Property '%s' not found in struct '%s'"),
                    *Pair.Key, *RowStruct->GetName());
                continue;
            }

            void* ValuePtr = Property->ContainerPtrToValuePtr<void>(RowData);
            FString ValueStr;

            // Handle different JSON value types
            if (Pair.Value->Type == EJson::String)
            {
                ValueStr = Pair.Value->AsString();
            }
            else if (Pair.Value->Type == EJson::Number)
            {
                ValueStr = FString::SanitizeFloat(Pair.Value->AsNumber());
            }
            else if (Pair.Value->Type == EJson::Boolean)
            {
                ValueStr = Pair.Value->AsBool() ? TEXT("True") : TEXT("False");
            }
            else
            {
                // For arrays/objects, serialize to string
                TSharedRef<FJsonValue> ValRef = Pair.Value.ToSharedRef();
                FString JsonStr;
                TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonStr);
                FJsonSerializer::Serialize(ValRef, TEXT(""), Writer);
                Writer->Close();
                ValueStr = JsonStr;
            }

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
            const TCHAR* ImportResult = Property->ImportText_Direct(*ValueStr, ValuePtr, nullptr, PPF_None);
#else
            const TCHAR* ImportResult = Property->ImportText(*ValueStr, ValuePtr, PPF_None, nullptr);
#endif
            if (ImportResult)
            {
                PropertiesSet++;
            }
            else
            {
                UE_LOG(LogMcpDataTableHandlers, Warning, TEXT("Failed to import value '%s' for property '%s'"),
                    *ValueStr, *Pair.Key);
            }
        }

        if (PropertiesSet == 0)
        {
            OutError = TEXT("No properties were successfully set");
            return false;
        }

        return true;
    }

#endif // WITH_EDITOR
}

// ============================================================================
// Sub-Action Handlers
// ============================================================================

#if WITH_EDITOR

// ----------------------------------------------------------------------------
// create_data_table
// ----------------------------------------------------------------------------
static bool HandleCreateDataTable(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString FolderPath = GetJsonStringField(Payload, TEXT("folderPath"), TEXT(""));
    FString AssetName = GetJsonStringField(Payload, TEXT("assetName"), TEXT(""));
    FString StructPath = GetJsonStringField(Payload, TEXT("structPath"), TEXT(""));

    if (AssetName.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("assetName is required"), nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    if (StructPath.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("structPath is required (e.g., /Script/Engine.DataTableRowHandle or a UScriptStruct path)"),
            nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    // Validate the creation path
    FString FullPath, PathError;
    if (!ValidateAssetCreationPath(FolderPath, AssetName, FullPath, PathError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            *PathError, nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // Find the row struct
    UScriptStruct* RowStruct = FindObject<UScriptStruct>(nullptr, *StructPath);
    if (!RowStruct)
    {
        // Try loading it
        RowStruct = LoadObject<UScriptStruct>(nullptr, *StructPath);
    }
    if (!RowStruct)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Could not find UScriptStruct at path: %s"), *StructPath),
            nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    // Validate the struct derives from FTableRowBase
    // FTableRowBase is the base for all data table row structs
    const UScriptStruct* TableRowBase = FTableRowBase::StaticStruct();
    if (!RowStruct->IsChildOf(TableRowBase))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Struct '%s' does not derive from FTableRowBase"), *StructPath),
            nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // Create the data table using the factory
    FString SanitizedFolder = SanitizeProjectRelativePath(FolderPath);
    if (SanitizedFolder.IsEmpty())
    {
        SanitizedFolder = TEXT("/Game");
    }

    UDataTableFactory* Factory = NewObject<UDataTableFactory>(GetTransientPackage());
    Factory->Struct = RowStruct;

    IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
    UObject* NewAsset = AssetTools.CreateAsset(AssetName, SanitizedFolder, UDataTable::StaticClass(), Factory);

    if (!NewAsset)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to create DataTable asset"), nullptr);
        return true;
    }

    UDataTable* DataTable = Cast<UDataTable>(NewAsset);
    McpSafeAssetSave(DataTable);

    TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject());
    ResultJson->SetStringField(TEXT("assetPath"), DataTable->GetPathName());
    ResultJson->SetStringField(TEXT("assetName"), AssetName);
    ResultJson->SetStringField(TEXT("structType"), RowStruct->GetPathName());

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Created DataTable: %s"), *DataTable->GetPathName()), ResultJson);
    return true;
}

// ----------------------------------------------------------------------------
// list_rows
// ----------------------------------------------------------------------------
static bool HandleListRows(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString AssetPath = GetJsonStringField(Payload, TEXT("assetPath"), TEXT(""));
    if (AssetPath.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("assetPath is required"), nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    UDataTable* DataTable = DataTableHelpers::LoadDataTable(AssetPath);
    if (!DataTable)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("DataTable not found at path: %s"), *AssetPath),
            nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    const TMap<FName, uint8*>& RowMap = DataTable->GetRowMap();
    TArray<TSharedPtr<FJsonValue>> RowNames;
    RowNames.Reserve(RowMap.Num());

    for (const auto& Pair : RowMap)
    {
        RowNames.Add(MakeShareable(new FJsonValueString(Pair.Key.ToString())));
    }

    TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject());
    ResultJson->SetArrayField(TEXT("rowNames"), RowNames);
    ResultJson->SetNumberField(TEXT("rowCount"), RowMap.Num());
    ResultJson->SetStringField(TEXT("assetPath"), DataTable->GetPathName());

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Found %d rows in DataTable"), RowMap.Num()), ResultJson);
    return true;
}

// ----------------------------------------------------------------------------
// get_row
// ----------------------------------------------------------------------------
static bool HandleGetRow(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString AssetPath = GetJsonStringField(Payload, TEXT("assetPath"), TEXT(""));
    FString RowName = GetJsonStringField(Payload, TEXT("rowName"), TEXT(""));

    if (AssetPath.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("assetPath is required"), nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }
    if (RowName.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("rowName is required"), nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    UDataTable* DataTable = DataTableHelpers::LoadDataTable(AssetPath);
    if (!DataTable)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("DataTable not found at path: %s"), *AssetPath),
            nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    const UScriptStruct* RowStruct = DataTable->GetRowStruct();
    if (!RowStruct)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("DataTable has no row struct defined"), nullptr);
        return true;
    }

    uint8* RowData = DataTable->FindRowUnchecked(FName(*RowName));
    if (!RowData)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Row '%s' not found in DataTable"), *RowName),
            nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    TSharedPtr<FJsonObject> RowJson = DataTableHelpers::RowToJson(RowStruct, RowData);

    TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject());
    ResultJson->SetStringField(TEXT("rowName"), RowName);
    ResultJson->SetObjectField(TEXT("values"), RowJson);
    ResultJson->SetStringField(TEXT("structType"), RowStruct->GetPathName());

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Retrieved row: %s"), *RowName), ResultJson);
    return true;
}

// ----------------------------------------------------------------------------
// add_row
// ----------------------------------------------------------------------------
static bool HandleAddRow(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString AssetPath = GetJsonStringField(Payload, TEXT("assetPath"), TEXT(""));
    FString RowName = GetJsonStringField(Payload, TEXT("rowName"), TEXT(""));

    if (AssetPath.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("assetPath is required"), nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }
    if (RowName.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("rowName is required"), nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    UDataTable* DataTable = DataTableHelpers::LoadDataTable(AssetPath);
    if (!DataTable)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("DataTable not found at path: %s"), *AssetPath),
            nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    const UScriptStruct* RowStruct = DataTable->GetRowStruct();
    if (!RowStruct)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("DataTable has no row struct defined"), nullptr);
        return true;
    }

    // Check if row already exists
    if (DataTable->FindRowUnchecked(FName(*RowName)))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Row '%s' already exists in DataTable"), *RowName),
            nullptr, TEXT("ALREADY_EXISTS"));
        return true;
    }

    // Allocate and zero-initialize a new row
    uint8* NewRowData = static_cast<uint8*>(FMemory::Malloc(RowStruct->GetStructureSize()));
    RowStruct->InitializeStruct(NewRowData);

    // Populate from JSON values if provided
    const TSharedPtr<FJsonObject>* ValuesObj = nullptr;
    if (Payload->TryGetObjectField(TEXT("values"), ValuesObj) && ValuesObj && (*ValuesObj).IsValid())
    {
        FString ImportError;
        DataTableHelpers::JsonToRow(RowStruct, NewRowData, *ValuesObj, ImportError);
        // Non-fatal: some properties may not have been set
        if (!ImportError.IsEmpty())
        {
            UE_LOG(LogMcpDataTableHandlers, Warning, TEXT("add_row partial import: %s"), *ImportError);
        }
    }

    // Add the row to the table
    DataTable->AddRow(FName(*RowName), *reinterpret_cast<FTableRowBase*>(NewRowData));

    // Clean up the temporary allocation
    RowStruct->DestroyStruct(NewRowData);
    FMemory::Free(NewRowData);

    McpSafeAssetSave(DataTable);

    TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject());
    ResultJson->SetStringField(TEXT("rowName"), RowName);
    ResultJson->SetStringField(TEXT("assetPath"), DataTable->GetPathName());

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Added row '%s' to DataTable"), *RowName), ResultJson);
    return true;
}

// ----------------------------------------------------------------------------
// edit_row
// ----------------------------------------------------------------------------
static bool HandleEditRow(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString AssetPath = GetJsonStringField(Payload, TEXT("assetPath"), TEXT(""));
    FString RowName = GetJsonStringField(Payload, TEXT("rowName"), TEXT(""));

    if (AssetPath.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("assetPath is required"), nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }
    if (RowName.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("rowName is required"), nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    UDataTable* DataTable = DataTableHelpers::LoadDataTable(AssetPath);
    if (!DataTable)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("DataTable not found at path: %s"), *AssetPath),
            nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    const UScriptStruct* RowStruct = DataTable->GetRowStruct();
    if (!RowStruct)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("DataTable has no row struct defined"), nullptr);
        return true;
    }

    uint8* RowData = DataTable->FindRowUnchecked(FName(*RowName));
    if (!RowData)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Row '%s' not found in DataTable"), *RowName),
            nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    const TSharedPtr<FJsonObject>* ValuesObj = nullptr;
    if (!Payload->TryGetObjectField(TEXT("values"), ValuesObj) || !ValuesObj || !(*ValuesObj).IsValid())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("values object is required"), nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    FString ImportError;
    bool bResult = DataTableHelpers::JsonToRow(RowStruct, RowData, *ValuesObj, ImportError);
    if (!bResult)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Failed to edit row: %s"), *ImportError), nullptr);
        return true;
    }

    // Notify the data table that it has been modified
    DataTable->HandleDataTableChanged(FName(*RowName));
    McpSafeAssetSave(DataTable);

    TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject());
    ResultJson->SetStringField(TEXT("rowName"), RowName);
    ResultJson->SetStringField(TEXT("assetPath"), DataTable->GetPathName());

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Edited row '%s' in DataTable"), *RowName), ResultJson);
    return true;
}

// ----------------------------------------------------------------------------
// remove_row
// ----------------------------------------------------------------------------
static bool HandleRemoveRow(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString AssetPath = GetJsonStringField(Payload, TEXT("assetPath"), TEXT(""));
    FString RowName = GetJsonStringField(Payload, TEXT("rowName"), TEXT(""));

    if (AssetPath.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("assetPath is required"), nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }
    if (RowName.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("rowName is required"), nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    UDataTable* DataTable = DataTableHelpers::LoadDataTable(AssetPath);
    if (!DataTable)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("DataTable not found at path: %s"), *AssetPath),
            nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    // Verify row exists before removal
    if (!DataTable->FindRowUnchecked(FName(*RowName)))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Row '%s' not found in DataTable"), *RowName),
            nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    DataTable->RemoveRow(FName(*RowName));
    McpSafeAssetSave(DataTable);

    TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject());
    ResultJson->SetStringField(TEXT("rowName"), RowName);
    ResultJson->SetStringField(TEXT("assetPath"), DataTable->GetPathName());

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Removed row '%s' from DataTable"), *RowName), ResultJson);
    return true;
}

// ----------------------------------------------------------------------------
// get_structure
// ----------------------------------------------------------------------------
static bool HandleGetStructure(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString AssetPath = GetJsonStringField(Payload, TEXT("assetPath"), TEXT(""));
    if (AssetPath.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("assetPath is required"), nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    UDataTable* DataTable = DataTableHelpers::LoadDataTable(AssetPath);
    if (!DataTable)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("DataTable not found at path: %s"), *AssetPath),
            nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    const UScriptStruct* RowStruct = DataTable->GetRowStruct();
    if (!RowStruct)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("DataTable has no row struct defined"), nullptr);
        return true;
    }

    TArray<TSharedPtr<FJsonValue>> ColumnsArray;
    for (TFieldIterator<FProperty> PropIt(RowStruct); PropIt; ++PropIt)
    {
        FProperty* Property = *PropIt;

        TSharedPtr<FJsonObject> ColumnJson = MakeShareable(new FJsonObject());
        ColumnJson->SetStringField(TEXT("name"), Property->GetName());
        ColumnJson->SetStringField(TEXT("type"), Property->GetCPPType());
        ColumnJson->SetStringField(TEXT("category"), Property->GetMetaData(TEXT("Category")));

        // Add more detailed type info for common types
        if (const FStructProperty* StructProp = CastField<FStructProperty>(Property))
        {
            ColumnJson->SetStringField(TEXT("structType"), StructProp->Struct->GetPathName());
        }
        else if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
        {
            if (UEnum* Enum = EnumProp->GetEnum())
            {
                ColumnJson->SetStringField(TEXT("enumType"), Enum->GetPathName());
                TArray<TSharedPtr<FJsonValue>> EnumValues;
                for (int32 i = 0; i < Enum->NumEnums() - 1; ++i)
                {
                    EnumValues.Add(MakeShareable(new FJsonValueString(Enum->GetNameStringByIndex(i))));
                }
                ColumnJson->SetArrayField(TEXT("enumValues"), EnumValues);
            }
        }
        else if (const FByteProperty* ByteProp = CastField<FByteProperty>(Property))
        {
            if (UEnum* Enum = ByteProp->Enum)
            {
                ColumnJson->SetStringField(TEXT("enumType"), Enum->GetPathName());
            }
        }
        else if (const FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
        {
            ColumnJson->SetStringField(TEXT("innerType"), ArrayProp->Inner->GetCPPType());
        }

        ColumnsArray.Add(MakeShareable(new FJsonValueObject(ColumnJson)));
    }

    TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject());
    ResultJson->SetStringField(TEXT("structName"), RowStruct->GetName());
    ResultJson->SetStringField(TEXT("structPath"), RowStruct->GetPathName());
    ResultJson->SetArrayField(TEXT("columns"), ColumnsArray);
    ResultJson->SetNumberField(TEXT("columnCount"), ColumnsArray.Num());
    ResultJson->SetStringField(TEXT("assetPath"), DataTable->GetPathName());

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Structure info for DataTable with %d columns"), ColumnsArray.Num()), ResultJson);
    return true;
}

// ----------------------------------------------------------------------------
// import_json
// ----------------------------------------------------------------------------
static bool HandleImportJson(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString AssetPath = GetJsonStringField(Payload, TEXT("assetPath"), TEXT(""));
    FString JsonString = GetJsonStringField(Payload, TEXT("jsonData"), TEXT(""));

    if (AssetPath.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("assetPath is required"), nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }
    if (JsonString.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("jsonData is required"), nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    UDataTable* DataTable = DataTableHelpers::LoadDataTable(AssetPath);
    if (!DataTable)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("DataTable not found at path: %s"), *AssetPath),
            nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    const UScriptStruct* RowStruct = DataTable->GetRowStruct();
    if (!RowStruct)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("DataTable has no row struct defined"), nullptr);
        return true;
    }

    // Parse the JSON string - expecting an object where keys are row names
    // and values are objects with column values
    TSharedPtr<FJsonObject> RootObj;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    if (!FJsonSerializer::Deserialize(Reader, RootObj) || !RootObj.IsValid())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to parse jsonData - expected a JSON object with row names as keys"),
            nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    bool bClearExisting = GetJsonBoolField(Payload, TEXT("clearExisting"), false);
    if (bClearExisting)
    {
        DataTable->EmptyTable();
    }

    int32 RowsImported = 0;
    int32 RowsFailed = 0;
    for (const auto& Pair : RootObj->Values)
    {
        const TSharedPtr<FJsonObject>* RowObj = nullptr;
        if (!Pair.Value->TryGetObject(RowObj) || !RowObj || !(*RowObj).IsValid())
        {
            UE_LOG(LogMcpDataTableHandlers, Warning, TEXT("import_json: Skipping row '%s' - value is not a JSON object"), *Pair.Key);
            RowsFailed++;
            continue;
        }

        // Allocate a new row
        uint8* NewRowData = static_cast<uint8*>(FMemory::Malloc(RowStruct->GetStructureSize()));
        RowStruct->InitializeStruct(NewRowData);

        FString ImportError;
        DataTableHelpers::JsonToRow(RowStruct, NewRowData, *RowObj, ImportError);

        DataTable->AddRow(FName(*Pair.Key), *reinterpret_cast<FTableRowBase*>(NewRowData));

        RowStruct->DestroyStruct(NewRowData);
        FMemory::Free(NewRowData);
        RowsImported++;
    }

    McpSafeAssetSave(DataTable);

    TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject());
    ResultJson->SetNumberField(TEXT("rowsImported"), RowsImported);
    ResultJson->SetNumberField(TEXT("rowsFailed"), RowsFailed);
    ResultJson->SetStringField(TEXT("assetPath"), DataTable->GetPathName());

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Imported %d rows (%d failed) into DataTable"), RowsImported, RowsFailed), ResultJson);
    return true;
}

// ----------------------------------------------------------------------------
// export_json
// ----------------------------------------------------------------------------
static bool HandleExportJson(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString AssetPath = GetJsonStringField(Payload, TEXT("assetPath"), TEXT(""));
    if (AssetPath.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("assetPath is required"), nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    UDataTable* DataTable = DataTableHelpers::LoadDataTable(AssetPath);
    if (!DataTable)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("DataTable not found at path: %s"), *AssetPath),
            nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    const UScriptStruct* RowStruct = DataTable->GetRowStruct();
    if (!RowStruct)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("DataTable has no row struct defined"), nullptr);
        return true;
    }

    const TMap<FName, uint8*>& RowMap = DataTable->GetRowMap();
    TSharedPtr<FJsonObject> RowsJson = MakeShareable(new FJsonObject());

    for (const auto& Pair : RowMap)
    {
        TSharedPtr<FJsonObject> RowJson = DataTableHelpers::RowToJson(RowStruct, Pair.Value);
        RowsJson->SetObjectField(Pair.Key.ToString(), RowJson);
    }

    // Serialize the JSON object to a string
    FString JsonOutput;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonOutput);
    FJsonSerializer::Serialize(RowsJson.ToSharedRef(), Writer);
    Writer->Close();

    TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject());
    ResultJson->SetStringField(TEXT("jsonData"), JsonOutput);
    ResultJson->SetNumberField(TEXT("rowCount"), RowMap.Num());
    ResultJson->SetStringField(TEXT("structType"), RowStruct->GetPathName());
    ResultJson->SetStringField(TEXT("assetPath"), DataTable->GetPathName());

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Exported %d rows from DataTable"), RowMap.Num()), ResultJson);
    return true;
}

#endif // WITH_EDITOR

// ============================================================================
// Main Dispatcher
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleManageDataTableAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    FString SubAction = GetJsonStringField(Payload, TEXT("subAction"), TEXT(""));

    UE_LOG(LogMcpDataTableHandlers, Verbose, TEXT("HandleManageDataTableAction: SubAction=%s"), *SubAction);

    if (SubAction == TEXT("create_data_table"))
    {
        return HandleCreateDataTable(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("list_rows"))
    {
        return HandleListRows(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("get_row"))
    {
        return HandleGetRow(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("add_row"))
    {
        return HandleAddRow(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("edit_row"))
    {
        return HandleEditRow(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("remove_row"))
    {
        return HandleRemoveRow(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("get_structure"))
    {
        return HandleGetStructure(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("import_json"))
    {
        return HandleImportJson(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("export_json"))
    {
        return HandleExportJson(this, RequestId, Payload, Socket);
    }

    // Unknown action
    SendAutomationResponse(Socket, RequestId, false,
        FString::Printf(TEXT("Unknown data_table subAction: %s"), *SubAction), nullptr, TEXT("UNKNOWN_ACTION"));
    return true;

#else
    SendAutomationResponse(Socket, RequestId, false,
        TEXT("DataTable operations require editor build"), nullptr, TEXT("EDITOR_ONLY"));
    return true;
#endif
}
