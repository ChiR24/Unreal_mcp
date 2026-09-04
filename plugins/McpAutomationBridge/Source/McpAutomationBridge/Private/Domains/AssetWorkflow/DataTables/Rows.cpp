#include "Domains/AssetWorkflow/DataTables/Shared.h"

#if WITH_EDITOR

namespace
{
    TSharedPtr<FJsonObject> MakeInvalidEntry(const FString& RowName, const FString& Reason)
    {
        TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->SetStringField(TEXT("rowName"), RowName);
        Entry->SetStringField(TEXT("reason"), Reason);
        return Entry;
    }

    struct FPendingRow
    {
        FName Name;
        TSharedPtr<FJsonObject> Data;
    };
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
    // conversion failure never destroys the original data.
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

        uint8* RowMem = nullptr;
        FString Err;
        if (!McpBuildDataTableRow(Table->RowStruct, RowData, RowMem, Err))
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

    // === import_data_table_rows ===
    // Validate EVERY row (build + check) before mutating the table. Existing
    // rows are only cleared after all inputs are proven valid, and invalid
    // entries are reported explicitly instead of dropped silently.
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

        TArray<FPendingRow> Pending;
        TArray<TSharedPtr<FJsonValue>> InvalidRows;
        for (const TSharedPtr<FJsonValue>& RowVal : RowsArr)
        {
            TSharedPtr<FJsonObject> RowObj = RowVal->AsObject();
            if (!RowObj.IsValid())
            {
                InvalidRows.Add(MakeShared<FJsonValueObject>(MakeInvalidEntry(TEXT(""), TEXT("Row entry is not a JSON object"))));
                continue;
            }
            FString RowName;
            if (!RowObj->TryGetStringField(TEXT("rowName"), RowName))
            {
                InvalidRows.Add(MakeShared<FJsonValueObject>(MakeInvalidEntry(TEXT(""), TEXT("Missing 'rowName' field"))));
                continue;
            }
            const TSharedPtr<FJsonObject>* RowDataPtr = nullptr;
            RowObj->TryGetObjectField(TEXT("rowData"), RowDataPtr);
            TSharedPtr<FJsonObject> RowData = RowDataPtr ? *RowDataPtr : nullptr;
            if (RowName.IsEmpty() || !RowData.IsValid())
            {
                InvalidRows.Add(MakeShared<FJsonValueObject>(MakeInvalidEntry(RowName, TEXT("Missing or invalid 'rowData' field"))));
                continue;
            }

            uint8* RowMem = nullptr;
            FString Err;
            if (!McpBuildDataTableRow(Table->RowStruct, RowData, RowMem, Err))
            {
                InvalidRows.Add(MakeShared<FJsonValueObject>(MakeInvalidEntry(RowName, Err)));
                continue;
            }
            Pending.Add({ FName(*RowName), RowData });
            McpFreeDataTableRow(Table->RowStruct, RowMem);
        }

        // When clearing is requested, a single invalid entry makes the whole
        // import unsafe: applying the valid subset would silently destroy the
        // existing (cleared) rows. Abort before mutating so no rows are cleared
        // or imported, and report exactly which entries failed validation.
        if (bClearExisting && InvalidRows.Num() > 0)
        {
            OutResult = McpDataTableMakeError(
                TEXT("VALIDATION_FAILED"),
                TEXT("Aborted import_data_table_rows: clearExisting=true but one or more rows failed validation. No existing rows were cleared and no rows were imported."));
            OutResult->SetNumberField(TEXT("skipped"), InvalidRows.Num());
            OutResult->SetArrayField(TEXT("invalidRows"), InvalidRows);
            return true;
        }

        // Only mutate after every input is validated.
        if (bClearExisting)
        {
            for (const FName& N : Table->GetRowNames()) { Table->RemoveRow(N); }
        }

        int32 Imported = 0;
        for (const FPendingRow& P : Pending)
        {
            uint8* RowMem = nullptr;
            FString Err;
            if (McpBuildDataTableRow(Table->RowStruct, P.Data, RowMem, Err))
            {
                Table->RemoveRow(P.Name);
                Table->AddRow(P.Name, RowMem, Table->RowStruct);
                McpFreeDataTableRow(Table->RowStruct, RowMem);
                ++Imported;
            }
            else
            {
                InvalidRows.Add(MakeShared<FJsonValueObject>(MakeInvalidEntry(P.Name.ToString(), Err)));
            }
        }
        if (bSave) { McpSafeAssetSave(Table); }

        OutResult = McpHandlerUtils::CreateResultObject();
        OutResult->SetNumberField(TEXT("imported"), Imported);
        OutResult->SetNumberField(TEXT("skipped"), InvalidRows.Num());
        OutResult->SetArrayField(TEXT("invalidRows"), InvalidRows);
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
