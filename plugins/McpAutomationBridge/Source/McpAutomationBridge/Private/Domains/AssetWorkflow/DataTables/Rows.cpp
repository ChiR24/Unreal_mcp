#include "Domains/AssetWorkflow/DataTables/Shared.h"

#if WITH_EDITOR

// MakeInvalidEntry/FPendingRow moved to RowsBulk.cpp with their only callers.
// Leaving copies here would be a redefinition once Unity merges the two shards.
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

        uint8* RowMem = nullptr;
        FString Err;
        if (!McpBuildDataTableRow(Table->RowStruct, RowData, RowMem, Err))
        {
            OutResult = McpDataTableMakeError(TEXT("INVALID_ROW_DATA"), *Err);
            return true;
        }
        Table->AddRow(FName(*RowName), RowMem, Table->RowStruct);
        McpFreeDataTableRow(Table->RowStruct, RowMem);
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

        const void* Row = Table->FindRowUnchecked(FName(*RowName));
        OutResult = McpHandlerUtils::CreateResultObject();
        if (Row && Table->RowStruct)
        {
            OutResult->SetBoolField(TEXT("found"), true);
            OutResult->SetStringField(TEXT("rowName"), RowName);
            OutResult->SetObjectField(TEXT("rowData"), McpExportDataTableRow(Table->RowStruct, Row));
        }
        else
        {
            OutResult->SetBoolField(TEXT("found"), false);
        }
        McpHandlerUtils::AddVerification(OutResult, Table);
        return true;
    }

    // === update_data_table_row ===
    // Build + validate the replacement BEFORE touching the existing row so a
    // conversion failure never destroys the original data, and MERGE the
    // payload over the row that is already there. This used to replace:
    // McpBuildDataTableRow starts from a default-constructed row, so every
    // column the caller did not name came back as the row struct's default. A
    // call setting two fields silently wiped the other thirteen -- data loss
    // from an action whose name promises the opposite.
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

        const void* Existing = Table->FindRowUnchecked(FName(*RowName));
        if (!Existing)
        {
            OutResult = McpDataTableMakeError(TEXT("ROW_NOT_FOUND"),
                TEXT("No row by that name to update; use add_row to create one."));
            return true;
        }
        TSharedPtr<FJsonObject> Merged = McpExportDataTableRow(Table->RowStruct, Existing);
        int32 Overwritten = 0;
        for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : RowData->Values)
        {
            if (Merged->HasField(Field.Key)) { ++Overwritten; }
            Merged->SetField(Field.Key, Field.Value);
        }

        uint8* RowMem = nullptr;
        FString Err;
        if (!McpBuildDataTableRow(Table->RowStruct, Merged, RowMem, Err))
        {
            OutResult = McpDataTableMakeError(TEXT("INVALID_ROW_DATA"), *Err);
            return true;
        }
        Table->RemoveRow(FName(*RowName));
        Table->AddRow(FName(*RowName), RowMem, Table->RowStruct);
        McpFreeDataTableRow(Table->RowStruct, RowMem);
        if (bSave) { McpSafeAssetSave(Table); }

        OutResult = McpHandlerUtils::CreateResultObject();
        OutResult->SetBoolField(TEXT("updated"), true);
        OutResult->SetStringField(TEXT("rowName"), RowName);
        // Say so out loud: a caller who expected replace semantics can see that
        // the columns it did not name were carried over, not reset.
        OutResult->SetNumberField(TEXT("fieldsWritten"), Overwritten);
        OutResult->SetNumberField(TEXT("fieldsPreserved"), Merged->Values.Num() - Overwritten);
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
    // The listing previously carried only bare row-name strings at the reply
    // root. Rows are now objects, bounded so a large table cannot flood the
    // reply, with rowCount reporting the unbounded total and rowStructPath
    // naming the row struct. The listing is also published under `details`
    // because that is the field the declared output contract projects; the
    // root copy keeps the raw-frame shape raw-frame consumers read.
    if (Action == TEXT("list_data_table_rows"))
    {
        TSharedPtr<FJsonObject> R;
        UDataTable* Table = ResolveDataTable(Params, R);
        if (!Table) { OutResult = R; return true; }

#if ENGINE_MAJOR_VERSION >= 5
        // UDataTable::GetRowNames() is available across the supported 5.x range.
        TArray<FName> Names = Table->GetRowNames();
#else
        TArray<FName> Names;
        for (const TPair<FName, uint8*>& Pair : Table->RowMap) { Names.Add(Pair.Key); }
#endif

        constexpr int32 MaxListedRows = 200;
        TArray<TSharedPtr<FJsonValue>> RowsArr;
        for (const FName& N : Names)
        {
            if (RowsArr.Num() >= MaxListedRows) { break; }
            TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
            Row->SetStringField(TEXT("rowName"), N.ToString());
            // Names alone forced one get_row call per row to read a table.
            // McpExportDataTableRow is the same exporter the single-row path
            // uses, so the listing now round-trips with no extra calls.
            uint8* const* RowMem = Table->RowStruct ? Table->GetRowMap().Find(N) : nullptr;
            if (RowMem) { Row->SetObjectField(TEXT("rowData"), McpExportDataTableRow(Table->RowStruct, *RowMem)); }
            RowsArr.Add(MakeShared<FJsonValueObject>(Row));
        }

        const FString RowStructPath = Table->RowStruct ? Table->RowStruct->GetPathName() : FString();

        TSharedPtr<FJsonObject> Details = MakeShared<FJsonObject>();
        Details->SetArrayField(TEXT("rows"), RowsArr);
        Details->SetNumberField(TEXT("rowCount"), Names.Num());
        Details->SetStringField(TEXT("rowStructPath"), RowStructPath);

        OutResult = McpHandlerUtils::CreateResultObject();
        OutResult->SetObjectField(TEXT("details"), Details);
        OutResult->SetArrayField(TEXT("rows"), RowsArr);
        OutResult->SetNumberField(TEXT("count"), RowsArr.Num());
        McpHandlerUtils::AddVerification(OutResult, Table);
        return true;
    }

    // Bulk import/clear live in RowsBulk.cpp; this shard would otherwise run
    // past the 250 pure-line ceiling. Callers see one entry point either way.
    return HandleDataTableBulkRowActions(Action, Params, OutResult);
}

#endif // WITH_EDITOR
