#include "Domains/AssetWorkflow/DataTables/Shared.h"
#include "Domains/AssetWorkflow/Structs/McpAutomationBridge_AssetWorkflowStructsShared.h"

#if WITH_EDITOR

namespace
{
    UScriptStruct* ResolveRowStruct(const FString& StructPath, TSharedPtr<FJsonObject>& OutResult)
    {
        UScriptStruct* S = LoadObject<UScriptStruct>(nullptr, *StructPath);
        if (!S) { OutResult = McpDataTableMakeError(TEXT("ASSET_NOT_FOUND"), nullptr); }
        return S;
    }

    UUserDefinedStruct* CreateEmptyRowStruct(UPackage* Package, const FString& SanitizedName)
    {
        UUserDefinedStruct* S = FStructureEditorUtils::CreateUserDefinedStruct(
            Package, FName(*SanitizedName), RF_Public | RF_Standalone);
        if (!S) return nullptr;
        // Drop the engine-seeded default variable so the struct starts empty.
        TArray<FGuid> SeededGuids;
        for (const FStructVariableDescription& Var : FStructureEditorUtils::GetVarDesc(S)) { SeededGuids.Add(Var.VarGuid); }
        for (const FGuid& G : SeededGuids) { FStructureEditorUtils::RemoveVariable(S, G); }
        FStructureEditorUtils::CompileStructure(S);
        return S;
    }
}

bool HandleDataTableAction(
    FString Action,
    const TSharedPtr<FJsonObject>& Params,
    TSharedPtr<FJsonObject>& OutResult)
{
    const FString Lower = Action.ToLower();

    // === create_data_table ===
    if (Lower == TEXT("create_data_table"))
    {
        FString Name = GetPayloadString(Params, TEXT("name"));
        FString Path = GetPayloadString(Params, TEXT("path"), TEXT("/Game/DataTables"));
        FString RowStructPath = GetPayloadString(Params, TEXT("rowStructPath"));
        bool bSave = GetPayloadBool(Params, TEXT("save"), false);
        if (Name.IsEmpty() || RowStructPath.IsEmpty()) { OutResult = McpDataTableMakeError(TEXT("MISSING_PARAMETER"), nullptr); return true; }

        FString PathError, PackageName, SanitizedName = SanitizeAssetName(Name);
        if (!ValidateAssetCreationPath(Path, SanitizedName, PackageName, PathError)) { OutResult = McpDataTableMakeError(TEXT("PACKAGE_CREATE_FAILED"), *PathError); return true; }

        UScriptStruct* RowStruct = ResolveRowStruct(RowStructPath, OutResult);
        if (!RowStruct) { return true; }

        UPackage* Package = CreatePackage(*PackageName);
        if (!Package) { OutResult = McpDataTableMakeError(TEXT("PACKAGE_CREATE_FAILED"), TEXT("Failed to create package")); return true; }

        UDataTable* Table = NewObject<UDataTable>(Package, FName(*SanitizedName), RF_Public | RF_Standalone);
        if (!Table) { OutResult = McpDataTableMakeError(TEXT("ASSET_CREATE_FAILED"), TEXT("Failed to create data table")); return true; }

        Table->RowStruct = RowStruct;
        Table->OnDataTableChanged();
        FAssetRegistryModule::AssetCreated(Table);
        if (bSave) { McpSafeAssetSave(Table); }

        OutResult = McpHandlerUtils::CreateResultObject();
        OutResult->SetBoolField(TEXT("created"), true);
        OutResult->SetStringField(TEXT("assetPath"), PackageName + TEXT(".") + SanitizedName);
        OutResult->SetStringField(TEXT("rowStructPath"), RowStructPath);
        OutResult->SetBoolField(TEXT("hasRowStruct"), true);
        OutResult->SetBoolField(TEXT("saved"), bSave);
        McpHandlerUtils::AddVerification(OutResult, Table);
        return true;
    }

    // === create_row_struct ===
    if (Lower == TEXT("create_row_struct"))
    {
        FString Name = GetPayloadString(Params, TEXT("name"));
        FString Path = GetPayloadString(Params, TEXT("path"), TEXT("/Game/Structs"));
        bool bSave = GetPayloadBool(Params, TEXT("save"), false);
        if (Name.IsEmpty()) { OutResult = McpDataTableMakeError(TEXT("MISSING_PARAMETER"), nullptr); return true; }

        FString PathError, PackageName, SanitizedName = SanitizeAssetName(Name);
        if (!ValidateAssetCreationPath(Path, SanitizedName, PackageName, PathError)) { OutResult = McpDataTableMakeError(TEXT("PACKAGE_CREATE_FAILED"), *PathError); return true; }

        UPackage* Package = CreatePackage(*PackageName);
        if (!Package) { OutResult = McpDataTableMakeError(TEXT("PACKAGE_CREATE_FAILED"), TEXT("Failed to create package")); return true; }

        UUserDefinedStruct* S = CreateEmptyRowStruct(Package, SanitizedName);
        if (!S) { OutResult = McpDataTableMakeError(TEXT("ASSET_CREATE_FAILED"), TEXT("Failed to create user defined struct")); return true; }

        FAssetRegistryModule::AssetCreated(S);
        if (bSave) { McpSafeAssetSave(S); }

        OutResult = McpHandlerUtils::CreateResultObject();
        OutResult->SetBoolField(TEXT("created"), true);
        OutResult->SetStringField(TEXT("assetPath"), PackageName + TEXT(".") + SanitizedName);
        OutResult->SetStringField(TEXT("structName"), SanitizedName);
        OutResult->SetBoolField(TEXT("usableAsRowStruct"), true);
        OutResult->SetBoolField(TEXT("saved"), bSave);
        McpHandlerUtils::AddVerification(OutResult, S);
        return true;
    }

    // === set_data_table_row_struct ===
    if (Lower == TEXT("set_data_table_row_struct"))
    {
        TSharedPtr<FJsonObject> R;
        UDataTable* Table = ResolveDataTable(Params, R);
        if (!Table) { OutResult = R; return true; }
        FString RowStructPath = GetPayloadString(Params, TEXT("rowStructPath"));
        bool bSave = GetPayloadBool(Params, TEXT("save"), false);
        if (RowStructPath.IsEmpty()) { OutResult = McpDataTableMakeError(TEXT("MISSING_PARAMETER"), nullptr); return true; }
        UScriptStruct* RowStruct = ResolveRowStruct(RowStructPath, OutResult);
        if (!RowStruct) { return true; }

        Table->RowStruct = RowStruct;
        Table->OnDataTableChanged();
        if (bSave) { McpSafeAssetSave(Table); }

        OutResult = McpHandlerUtils::CreateResultObject();
        OutResult->SetBoolField(TEXT("updated"), true);
        OutResult->SetStringField(TEXT("rowStructPath"), RowStructPath);
        OutResult->SetBoolField(TEXT("hasRowStruct"), true);
        OutResult->SetBoolField(TEXT("saved"), bSave);
        McpHandlerUtils::AddVerification(OutResult, Table);
        return true;
    }

    // === get_row_struct ===
    if (Lower == TEXT("get_row_struct"))
    {
        TSharedPtr<FJsonObject> R;
        UDataTable* Table = ResolveDataTable(Params, R);
        if (!Table) { OutResult = R; return true; }

        OutResult = McpHandlerUtils::CreateResultObject();
        if (Table->RowStruct)
        {
            OutResult->SetBoolField(TEXT("hasRowStruct"), true);
            OutResult->SetStringField(TEXT("rowStructPath"), Table->RowStruct->GetPathName());
        }
        else
        {
            OutResult->SetBoolField(TEXT("hasRowStruct"), false);
        }
        McpHandlerUtils::AddVerification(OutResult, Table);
        return true;
    }

    // === set_struct_as_row_struct ===
    if (Lower == TEXT("set_struct_as_row_struct"))
    {
        FString StructPath = GetPayloadString(Params, TEXT("structPath"));
        bool bSave = GetPayloadBool(Params, TEXT("save"), false);
        if (StructPath.IsEmpty()) { OutResult = McpDataTableMakeError(TEXT("MISSING_PARAMETER"), nullptr); return true; }
        UUserDefinedStruct* S = LoadObject<UUserDefinedStruct>(nullptr, *StructPath);
        if (!S) { OutResult = McpDataTableMakeError(TEXT("ASSET_NOT_FOUND"), nullptr); return true; }

        FString ValidityMsg;
        if (!FStructureEditorUtils::IsStructureValid(S, nullptr, &ValidityMsg))
        {
            OutResult = McpDataTableMakeError(TEXT("INVALID_OPERATION"), *ValidityMsg);
            return true;
        }

        FStructureEditorUtils::CompileStructure(S);
        FAssetRegistryModule::AssetCreated(S);
        if (bSave) { McpSafeAssetSave(S); }

        OutResult = McpHandlerUtils::CreateResultObject();
        OutResult->SetBoolField(TEXT("set"), true);
        OutResult->SetStringField(TEXT("assetPath"), StructPath);
        OutResult->SetBoolField(TEXT("usableAsRowStruct"), true);
        OutResult->SetStringField(TEXT("message"), TEXT("set_struct_as_row_struct validates and compiles the struct for row-struct use; it does not bind it to a data table. Use set_data_table_row_struct or create_data_table to bind a row struct to a table."));
        OutResult->SetStringField(TEXT("status"), UserDefinedStructureStatusToString(S->Status));
        OutResult->SetBoolField(TEXT("saved"), bSave);
        McpHandlerUtils::AddVerification(OutResult, S);
        return true;
    }

    // === Row-scoped actions ===
    if (HandleDataTableRowActions(Lower, Params, OutResult)) { return true; }

    OutResult = McpDataTableMakeError(TEXT("UNKNOWN_ACTION"), nullptr);
    return true;
}

#endif // WITH_EDITOR
