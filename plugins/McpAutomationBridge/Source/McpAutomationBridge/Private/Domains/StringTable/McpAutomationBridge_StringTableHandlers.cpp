// McpAutomationBridge_StringTableHandlers.cpp
// String table management for UI text and localization:
// - create_string_table: Create a new UStringTable asset
// - add_entry: Add a key-value entry
// - remove_entry: Remove an entry by key
// - edit_entry: Edit an existing entry
// - get_entry: Get a single entry
// - list_entries: List all entries
// - import_json: Import entries from JSON
// - export_json: Export all entries as JSON
// - list_string_tables: List all string table assets

#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Internationalization/StringTable.h"
#include "Internationalization/StringTableCore.h"
#include "Internationalization/StringTableRegistry.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogMcpStringTableHandlers, Log, All);

#if WITH_EDITOR

// Helper: Load a UStringTable by path
static UStringTable* LoadStringTable(const FString& AssetPath)
{
    FString SanitizedPath = SanitizeProjectRelativePath(AssetPath);
    if (SanitizedPath.IsEmpty())
    {
        return nullptr;
    }
    return LoadObject<UStringTable>(nullptr, *SanitizedPath);
}

// ----------------------------------------------------------------------------
// create_string_table
// ----------------------------------------------------------------------------
static bool HandleCreateStringTable(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString FolderPath = GetJsonStringField(Payload, TEXT("folderPath"), TEXT("/Game/Localization"));
    FString AssetName = GetJsonStringField(Payload, TEXT("assetName"), TEXT(""));
    FString TableNamespace = GetJsonStringField(Payload, TEXT("tableNamespace"), TEXT(""));

    if (AssetName.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("assetName is required"), nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    FString SanitizedFolder = SanitizeProjectRelativePath(FolderPath);
    if (SanitizedFolder.IsEmpty())
    {
        SanitizedFolder = TEXT("/Game/Localization");
    }

    FString SanitizedName = SanitizeAssetName(AssetName);
    FString PathError;
    UPackage* Package = CreateValidatedAssetPackage(SanitizedFolder, SanitizedName, PathError);
    if (!Package)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            PathError.IsEmpty() ? TEXT("Failed to create package") : PathError,
            nullptr, TEXT("PACKAGE_CREATE_FAILED"));
        return true;
    }

    UStringTable* NewTable = NewObject<UStringTable>(Package, FName(*SanitizedName),
        RF_Public | RF_Standalone);
    if (!NewTable)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to create string table"), nullptr);
        return true;
    }

    // Set namespace if provided
    if (!TableNamespace.IsEmpty())
    {
        NewTable->GetMutableStringTable()->SetNamespace(TableNamespace);
    }

    NewTable->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(NewTable);
    McpSafeAssetSave(NewTable);

    TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject());
    ResultJson->SetStringField(TEXT("assetPath"), NewTable->GetPathName());
    ResultJson->SetStringField(TEXT("assetName"), SanitizedName);
    if (!TableNamespace.IsEmpty())
    {
        ResultJson->SetStringField(TEXT("namespace"), TableNamespace);
    }

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Created string table: %s"), *NewTable->GetPathName()), ResultJson);
    return true;
}

// ----------------------------------------------------------------------------
// add_entry
// ----------------------------------------------------------------------------
static bool HandleAddEntry(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString AssetPath = GetJsonStringField(Payload, TEXT("assetPath"), TEXT(""));
    FString Key = GetJsonStringField(Payload, TEXT("key"), TEXT(""));
    FString Value = GetJsonStringField(Payload, TEXT("value"), TEXT(""));

    if (AssetPath.IsEmpty() || Key.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("assetPath and key are required"), nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    UStringTable* Table = LoadStringTable(AssetPath);
    if (!Table)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("String table not found: %s"), *AssetPath),
            nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    FStringTableRef TableRef = Table->GetMutableStringTable();
    TableRef->SetSourceString(Key, Value);
    Table->MarkPackageDirty();
    McpSafeAssetSave(Table);

    TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject());
    ResultJson->SetStringField(TEXT("key"), Key);
    ResultJson->SetStringField(TEXT("value"), Value);
    ResultJson->SetStringField(TEXT("assetPath"), Table->GetPathName());

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Added entry '%s' to string table"), *Key), ResultJson);
    return true;
}

// ----------------------------------------------------------------------------
// remove_entry
// ----------------------------------------------------------------------------
static bool HandleRemoveEntry(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString AssetPath = GetJsonStringField(Payload, TEXT("assetPath"), TEXT(""));
    FString Key = GetJsonStringField(Payload, TEXT("key"), TEXT(""));

    if (AssetPath.IsEmpty() || Key.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("assetPath and key are required"), nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    UStringTable* Table = LoadStringTable(AssetPath);
    if (!Table)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("String table not found: %s"), *AssetPath),
            nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    FStringTableRef TableRef = Table->GetMutableStringTable();
    TableRef->RemoveSourceString(Key);
    Table->MarkPackageDirty();
    McpSafeAssetSave(Table);

    TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject());
    ResultJson->SetStringField(TEXT("key"), Key);
    ResultJson->SetStringField(TEXT("assetPath"), Table->GetPathName());

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Removed entry '%s' from string table"), *Key), ResultJson);
    return true;
}

// ----------------------------------------------------------------------------
// edit_entry (same as add_entry - SetSourceString overwrites)
// ----------------------------------------------------------------------------
static bool HandleEditEntry(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString AssetPath = GetJsonStringField(Payload, TEXT("assetPath"), TEXT(""));
    FString Key = GetJsonStringField(Payload, TEXT("key"), TEXT(""));
    FString Value = GetJsonStringField(Payload, TEXT("value"), TEXT(""));

    if (AssetPath.IsEmpty() || Key.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("assetPath, key are required"), nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    UStringTable* Table = LoadStringTable(AssetPath);
    if (!Table)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("String table not found: %s"), *AssetPath),
            nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    FStringTableRef TableRef = Table->GetMutableStringTable();

    // Check if key exists
    FStringTableEntryConstPtr Entry = TableRef->FindEntry(Key);
    if (!Entry.IsValid())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Key '%s' not found in string table"), *Key),
            nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    TableRef->SetSourceString(Key, Value);
    Table->MarkPackageDirty();
    McpSafeAssetSave(Table);

    TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject());
    ResultJson->SetStringField(TEXT("key"), Key);
    ResultJson->SetStringField(TEXT("value"), Value);
    ResultJson->SetStringField(TEXT("assetPath"), Table->GetPathName());

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Updated entry '%s' in string table"), *Key), ResultJson);
    return true;
}

// ----------------------------------------------------------------------------
// get_entry
// ----------------------------------------------------------------------------
static bool HandleGetEntry(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString AssetPath = GetJsonStringField(Payload, TEXT("assetPath"), TEXT(""));
    FString Key = GetJsonStringField(Payload, TEXT("key"), TEXT(""));

    if (AssetPath.IsEmpty() || Key.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("assetPath and key are required"), nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    UStringTable* Table = LoadStringTable(AssetPath);
    if (!Table)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("String table not found: %s"), *AssetPath),
            nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    FStringTableConstRef TableRef = Table->GetStringTable();
    FStringTableEntryConstPtr Entry = TableRef->FindEntry(Key);
    if (!Entry.IsValid())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Key '%s' not found in string table"), *Key),
            nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject());
    ResultJson->SetStringField(TEXT("key"), Key);
    ResultJson->SetStringField(TEXT("value"), Entry->GetSourceString());
    ResultJson->SetStringField(TEXT("assetPath"), Table->GetPathName());

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Entry '%s' found"), *Key), ResultJson);
    return true;
}

// ----------------------------------------------------------------------------
// list_entries
// ----------------------------------------------------------------------------
static bool HandleListEntries(
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

    UStringTable* Table = LoadStringTable(AssetPath);
    if (!Table)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("String table not found: %s"), *AssetPath),
            nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    FStringTableConstRef TableRef = Table->GetStringTable();
    TSharedPtr<FJsonObject> EntriesJson = MakeShareable(new FJsonObject());
    int32 Count = 0;

    TableRef->EnumerateSourceStrings([&](const FString& InKey, const FString& InSourceString) -> bool
    {
        EntriesJson->SetStringField(InKey, InSourceString);
        Count++;
        return true; // Continue enumeration
    });

    TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject());
    ResultJson->SetObjectField(TEXT("entries"), EntriesJson);
    ResultJson->SetNumberField(TEXT("count"), Count);
    ResultJson->SetStringField(TEXT("assetPath"), Table->GetPathName());

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Found %d entries in string table"), Count), ResultJson);
    return true;
}

// ----------------------------------------------------------------------------
// import_json
// ----------------------------------------------------------------------------
static bool HandleImportJsonEntries(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString AssetPath = GetJsonStringField(Payload, TEXT("assetPath"), TEXT(""));
    FString JsonString = GetJsonStringField(Payload, TEXT("jsonData"), TEXT(""));

    if (AssetPath.IsEmpty() || JsonString.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("assetPath and jsonData are required"), nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    UStringTable* Table = LoadStringTable(AssetPath);
    if (!Table)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("String table not found: %s"), *AssetPath),
            nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    TSharedPtr<FJsonObject> RootObj;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    if (!FJsonSerializer::Deserialize(Reader, RootObj) || !RootObj.IsValid())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to parse jsonData"), nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FStringTableRef TableRef = Table->GetMutableStringTable();
    int32 Imported = 0;
    for (const auto& Pair : RootObj->Values)
    {
        if (Pair.Value->Type == EJson::String)
        {
            TableRef->SetSourceString(Pair.Key, Pair.Value->AsString());
            Imported++;
        }
    }

    Table->MarkPackageDirty();
    McpSafeAssetSave(Table);

    TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject());
    ResultJson->SetNumberField(TEXT("entriesImported"), Imported);
    ResultJson->SetStringField(TEXT("assetPath"), Table->GetPathName());

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Imported %d entries into string table"), Imported), ResultJson);
    return true;
}

// ----------------------------------------------------------------------------
// export_json
// ----------------------------------------------------------------------------
static bool HandleExportJsonEntries(
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

    UStringTable* Table = LoadStringTable(AssetPath);
    if (!Table)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("String table not found: %s"), *AssetPath),
            nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    FStringTableConstRef TableRef = Table->GetStringTable();
    TSharedPtr<FJsonObject> EntriesJson = MakeShareable(new FJsonObject());
    int32 Count = 0;

    TableRef->EnumerateSourceStrings([&](const FString& InKey, const FString& InSourceString) -> bool
    {
        EntriesJson->SetStringField(InKey, InSourceString);
        Count++;
        return true;
    });

    FString JsonOutput;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonOutput);
    FJsonSerializer::Serialize(EntriesJson.ToSharedRef(), Writer);
    Writer->Close();

    TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject());
    ResultJson->SetStringField(TEXT("jsonData"), JsonOutput);
    ResultJson->SetNumberField(TEXT("count"), Count);
    ResultJson->SetStringField(TEXT("assetPath"), Table->GetPathName());

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Exported %d entries from string table"), Count), ResultJson);
    return true;
}

// ----------------------------------------------------------------------------
// list_string_tables
// ----------------------------------------------------------------------------
static bool HandleListStringTables(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString SearchPath = GetJsonStringField(Payload, TEXT("searchPath"), TEXT("/Game"));

    FString SanitizedPath = SanitizeProjectRelativePath(SearchPath);
    if (SanitizedPath.IsEmpty())
    {
        SanitizedPath = TEXT("/Game");
    }

    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

    TArray<FAssetData> AssetList;
    AssetRegistry.GetAssetsByPath(FName(*SanitizedPath), AssetList, true);

    TArray<TSharedPtr<FJsonValue>> TablesArray;
    for (const FAssetData& AssetData : AssetList)
    {
        if (AssetData.AssetClassPath.GetAssetName() == UStringTable::StaticClass()->GetFName())
        {
            TSharedPtr<FJsonObject> TableJson = MakeShareable(new FJsonObject());
            TableJson->SetStringField(TEXT("assetPath"), AssetData.GetObjectPathString());
            TableJson->SetStringField(TEXT("assetName"), AssetData.AssetName.ToString());
            TablesArray.Add(MakeShareable(new FJsonValueObject(TableJson)));
        }
    }

    TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject());
    ResultJson->SetArrayField(TEXT("tables"), TablesArray);
    ResultJson->SetNumberField(TEXT("count"), TablesArray.Num());

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Found %d string tables"), TablesArray.Num()), ResultJson);
    return true;
}

#endif // WITH_EDITOR

// ============================================================================
// Main Dispatcher
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleManageStringTableAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    FString SubAction = GetJsonStringField(Payload, TEXT("subAction"), TEXT(""));

    UE_LOG(LogMcpStringTableHandlers, Verbose, TEXT("HandleManageStringTableAction: SubAction=%s"), *SubAction);

    if (SubAction == TEXT("create_string_table")) return HandleCreateStringTable(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("add_entry")) return HandleAddEntry(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("remove_entry")) return HandleRemoveEntry(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("edit_entry")) return HandleEditEntry(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("get_entry")) return HandleGetEntry(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("list_entries")) return HandleListEntries(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("import_json")) return HandleImportJsonEntries(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("export_json")) return HandleExportJsonEntries(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("list_string_tables")) return HandleListStringTables(this, RequestId, Payload, Socket);

    SendAutomationResponse(Socket, RequestId, false,
        FString::Printf(TEXT("Unknown string_table subAction: %s"), *SubAction), nullptr, TEXT("UNKNOWN_ACTION"));
    return true;

#else
    SendAutomationResponse(Socket, RequestId, false,
        TEXT("String table operations require editor build"), nullptr, TEXT("EDITOR_ONLY"));
    return true;
#endif
}
