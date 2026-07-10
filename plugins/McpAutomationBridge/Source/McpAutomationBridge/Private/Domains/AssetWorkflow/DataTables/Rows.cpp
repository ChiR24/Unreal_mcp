#include "Domains/AssetWorkflow/DataTables/Shared.h"

#if WITH_EDITOR

namespace
{
    uint8* BuildRow(const UScriptStruct* RowStruct, const TSharedPtr<FJsonObject>& RowData)
    {
        uint8* RowMem = static_cast<uint8*>(FMemory::Malloc(RowStruct->GetStructureSize()));
        RowStruct->InitializeStruct(RowMem);
        FJsonObjectConverter::JsonObjectToUStruct(RowData.ToSharedRef(), RowStruct, RowMem, 0, 0);
        return RowMem;
    }

    TSharedPtr<FJsonObject> ExportRow(const UScriptStruct* RowStruct, const void* RowMem)
    {
        TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
        FJsonObjectConverter::UStructToJsonObject(RowStruct, RowMem, Json.ToSharedRef(), 0, 0);
        return Json;
    }

    void FreeRow(const UScriptStruct* RowStruct, uint8* RowMem)
    {
        RowStruct->DestroyStruct(RowMem);
        FMemory::Free(RowMem);
    }
}

bool HandleDataTableRowActions(
    const FString& Action,
    const TSharedPtr<FJsonObject>& Params,
    TSharedPtr<FJsonObject>& OutResult)
{
    // === add_data_table_row ===
    if (Action == TEXT("add_data_table_row"))
    {
        TSharedPtr<FJsonObject> R;
        UDataTable* Table = ResolveDataTable(Params, R);
        if (!Table) { OutResult = R; return true; }
        FString RowName = GetPayloadString(Params, TEXT("rowName"));
        const TSharedPtr<FJsonObject>* RowDataPtr = nullptr;
        Params->TryGetObjectField(TEXT("rowData"), RowDataPtr);
        TSharedPtr<FJsonObject> RowData = RowDataPtr ? *RowDataPtr : nullptr;
        bool bSave = GetPayloadBool(Params, TEXT("save"), false);
        if (RowName.IsEmpty() || !RowData.IsValid()) { OutResult = McpDataTableMakeError(TEXT("MISSING_PARAMETER"), nullptr); return true; }
        if (!Table->RowStruct) { OutResult = McpDataTableMakeError(TEXT("INVALID_OPERATION"), nullptr); return true; }

        uint8* RowMem = BuildRow(Table->RowStruct, RowData);
        Table->AddRow(FName(*RowName), RowMem, Table->RowStruct);
        FreeRow(Table->RowStruct, RowMem);
        if (bSave) { McpSafeAssetSave(Table); }

        OutResult = McpHandlerUtils::CreateResultObject();
        OutResult->SetBoolField(TEXT("added"), true);
        OutResult->SetStringField(TEXT("rowName"), RowName);
        McpHandlerUtils::AddVerification(OutResult, Table);
        return true;
    }

    // === get_data_table_row ===
    if (Action == TEXT("get_data_table_row"))
    {
        TSharedPtr<FJsonObject> R;
        UDataTable* Table = ResolveDataTable(Params, R);
        if (!Table) { OutResult = R; return true; }
        FString RowName = GetPayloadString(Params, TEXT("rowName"));
        if (RowName.IsEmpty()) { OutResult = McpDataTableMakeError(TEXT("MISSING_PARAMETER"), nullptr); return true; }

        const void* Row = Table->FindRow<FTableRowBase>(FName(*RowName), TEXT(""), false);
        OutResult = McpHandlerUtils::CreateResultObject();
        if (Row && Table->RowStruct)
        {
            OutResult->SetBoolField(TEXT("found"), true);
            OutResult->SetStringField(TEXT("rowName"), RowName);
            OutResult->SetObjectField(TEXT("rowData"), ExportRow(Table->RowStruct, Row));
        }
        else
        {
            OutResult->SetBoolField(TEXT("found"), false);
        }
        McpHandlerUtils::AddVerification(OutResult, Table);
        return true;
    }

    // === update_data_table_row ===
    if (Action == TEXT("update_data_table_row"))
    {
        TSharedPtr<FJsonObject> R;
        UDataTable* Table = ResolveDataTable(Params, R);
        if (!Table) { OutResult = R; return true; }
        FString RowName = GetPayloadString(Params, TEXT("rowName"));
        const TSharedPtr<FJsonObject>* RowDataPtr = nullptr;
        Params->TryGetObjectField(TEXT("rowData"), RowDataPtr);
        TSharedPtr<FJsonObject> RowData = RowDataPtr ? *RowDataPtr : nullptr;
        bool bSave = GetPayloadBool(Params, TEXT("save"), false);
        if (RowName.IsEmpty() || !RowData.IsValid()) { OutResult = McpDataTableMakeError(TEXT("MISSING_PARAMETER"), nullptr); return true; }
        if (!Table->RowStruct) { OutResult = McpDataTableMakeError(TEXT("INVALID_OPERATION"), nullptr); return true; }

        Table->RemoveRow(FName(*RowName));
        uint8* RowMem = BuildRow(Table->RowStruct, RowData);
        Table->AddRow(FName(*RowName), RowMem, Table->RowStruct);
        FreeRow(Table->RowStruct, RowMem);
        if (bSave) { McpSafeAssetSave(Table); }

        OutResult = McpHandlerUtils::CreateResultObject();
        OutResult->SetBoolField(TEXT("updated"), true);
        OutResult->SetStringField(TEXT("rowName"), RowName);
        McpHandlerUtils::AddVerification(OutResult, Table);
        return true;
    }

    // === delete_data_table_row ===
    if (Action == TEXT("delete_data_table_row"))
    {
        TSharedPtr<FJsonObject> R;
        UDataTable* Table = ResolveDataTable(Params, R);
        if (!Table) { OutResult = R; return true; }
        FString RowName = GetPayloadString(Params, TEXT("rowName"));
        if (RowName.IsEmpty()) { OutResult = McpDataTableMakeError(TEXT("MISSING_PARAMETER"), nullptr); return true; }

        Table->RemoveRow(FName(*RowName));
        if (GetPayloadBool(Params, TEXT("save"), false)) { McpSafeAssetSave(Table); }

        OutResult = McpHandlerUtils::CreateResultObject();
        OutResult->SetBoolField(TEXT("removed"), true);
        OutResult->SetStringField(TEXT("rowName"), RowName);
        McpHandlerUtils::AddVerification(OutResult, Table);
        return true;
    }

    // === list_data_table_rows ===
    if (Action == TEXT("list_data_table_rows"))
    {
        TSharedPtr<FJsonObject> R;
        UDataTable* Table = ResolveDataTable(Params, R);
        if (!Table) { OutResult = R; return true; }

        TArray<FName> Names = Table->GetRowNames();
        TArray<TSharedPtr<FJsonValue>> RowsArr;
        for (const FName& N : Names) { RowsArr.Add(MakeShared<FJsonValueString>(N.ToString())); }

        OutResult = McpHandlerUtils::CreateResultObject();
        OutResult->SetArrayField(TEXT("rows"), RowsArr);
        OutResult->SetNumberField(TEXT("count"), RowsArr.Num());
        McpHandlerUtils::AddVerification(OutResult, Table);
        return true;
    }

    // === import_data_table_rows ===
    if (Action == TEXT("import_data_table_rows"))
    {
        TSharedPtr<FJsonObject> R;
        UDataTable* Table = ResolveDataTable(Params, R);
        if (!Table) { OutResult = R; return true; }
        TArray<TSharedPtr<FJsonValue>> RowsArr;
        const TArray<TSharedPtr<FJsonValue>>* RowsArrPtr = nullptr;
        Params->TryGetArrayField(TEXT("rows"), RowsArrPtr);
        if (RowsArrPtr) { RowsArr = *RowsArrPtr; }
        bool bClearExisting = GetPayloadBool(Params, TEXT("clearExisting"), false);
        bool bSave = GetPayloadBool(Params, TEXT("save"), false);
        if (RowsArr.Num() == 0) { OutResult = McpDataTableMakeError(TEXT("MISSING_PARAMETER"), nullptr); return true; }
        if (!Table->RowStruct) { OutResult = McpDataTableMakeError(TEXT("INVALID_OPERATION"), nullptr); return true; }

        if (bClearExisting) { for (const FName& N : Table->GetRowNames()) { Table->RemoveRow(N); } }

        int32 Imported = 0;
        for (const TSharedPtr<FJsonValue>& RowVal : RowsArr)
        {
            TSharedPtr<FJsonObject> RowObj = RowVal->AsObject();
            if (!RowObj.IsValid()) { continue; }
            FString RowName = RowObj->GetStringField(TEXT("rowName"));
            const TSharedPtr<FJsonObject>* RowDataPtr = nullptr;
            RowObj->TryGetObjectField(TEXT("rowData"), RowDataPtr);
            TSharedPtr<FJsonObject> RowData = RowDataPtr ? *RowDataPtr : nullptr;
            if (RowName.IsEmpty() || !RowData.IsValid()) { continue; }
            Table->RemoveRow(FName(*RowName));
            uint8* RowMem = BuildRow(Table->RowStruct, RowData);
            Table->AddRow(FName(*RowName), RowMem, Table->RowStruct);
            FreeRow(Table->RowStruct, RowMem);
            ++Imported;
        }
        if (bSave) { McpSafeAssetSave(Table); }

        OutResult = McpHandlerUtils::CreateResultObject();
        OutResult->SetNumberField(TEXT("imported"), Imported);
        McpHandlerUtils::AddVerification(OutResult, Table);
        return true;
    }

    // === clear_data_table_rows ===
    if (Action == TEXT("clear_data_table_rows"))
    {
        TSharedPtr<FJsonObject> R;
        UDataTable* Table = ResolveDataTable(Params, R);
        if (!Table) { OutResult = R; return true; }

        for (const FName& N : Table->GetRowNames()) { Table->RemoveRow(N); }
        if (GetPayloadBool(Params, TEXT("save"), false)) { McpSafeAssetSave(Table); }

        OutResult = McpHandlerUtils::CreateResultObject();
        OutResult->SetBoolField(TEXT("cleared"), true);
        McpHandlerUtils::AddVerification(OutResult, Table);
        return true;
    }

    return false;
}

#endif // WITH_EDITOR
