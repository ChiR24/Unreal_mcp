#include "Domains/AssetWorkflow/DataTables/Shared.h"

#if WITH_EDITOR

// Split out of Rows.cpp: the merge fix on update_row pushed that shard past the
// 250 pure-line ceiling, and bulk import/clear is a separate responsibility from
// the single-row verbs anyway. The anonymous-namespace helpers below moved with
// it wholesale, so no name is defined twice in this Unity-merged folder.
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

bool HandleDataTableBulkRowActions(
    const FString& Action,
    const TSharedPtr<FJsonObject>& Params,
    TSharedPtr<FJsonObject>& OutResult)
{
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
        if (RowsArr.Num() == 0)
        {
            // "MISSING_PARAMETER" as the whole message named neither the field
            // nor the shape, so the only way to learn it was trial and error.
            OutResult = McpDataTableMakeError(TEXT("MISSING_PARAMETER"),
                TEXT("'rows' must be a non-empty array of {\"rowName\": \"<name>\", \"rowData\": { <field>: <value>, ... }} objects. Field names are the row struct's own, e.g. {\"rowName\":\"ArcRifle\",\"rowData\":{\"DisplayName\":\"Arc Rifle\",\"Damage\":42}}."));
            return true;
        }
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

        // import_rows REPLACES each named row: McpBuildDataTableRow starts from a
        // default-constructed struct, so any column an entry omits comes back as the
        // struct default rather than keeping its old value. That is correct for an
        // import, and silently destructive when a caller passes a partial row meaning
        // to patch it -- so count the omissions and say so.
        int32 Imported = 0;
        int32 WorstOmitted = 0;
        FString WorstOmittedRow;
        TSet<FString> OmittedFields;
        int32 ColumnCount = 0;
        for (TFieldIterator<FProperty> PropIt(Table->RowStruct); PropIt; ++PropIt) { ++ColumnCount; }

        for (const FPendingRow& P : Pending)
        {
            if (ColumnCount > 0 && P.Data.IsValid())
            {
                const int32 Omitted = ColumnCount - P.Data->Values.Num();
                if (Omitted > WorstOmitted)
                {
                    WorstOmitted = Omitted;
                    WorstOmittedRow = P.Name.ToString();
                    for (TFieldIterator<FProperty> PropIt(Table->RowStruct); PropIt; ++PropIt)
                    {
                        const FString Field = PropIt->GetAuthoredName();
                        if (!P.Data->HasField(Field)) { OmittedFields.Add(Field); }
                    }
                }
            }
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
        if (WorstOmitted > 0)
        {
            TArray<FString> Names = OmittedFields.Array();
            Names.Sort();
            OutResult->SetNumberField(TEXT("fieldsDefaulted"), WorstOmitted);
            OutResult->SetStringField(
                TEXT("dataLossWarning"),
                FString::Printf(
                    TEXT("import_rows replaces rows rather than merging: row '%s' supplied %d of %d columns, so %d were reset to the row struct's defaults (%s). Use update_row to patch a row without touching its other columns."),
                    *WorstOmittedRow, ColumnCount - WorstOmitted, ColumnCount, WorstOmitted,
                    *FString::Join(Names, TEXT(", "))));
        }
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
