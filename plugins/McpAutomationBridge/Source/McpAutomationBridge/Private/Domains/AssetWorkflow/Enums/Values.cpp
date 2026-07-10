#include "Domains/AssetWorkflow/Enums/Shared.h"
#include "Kismet2/EnumEditorUtils.h"

#if WITH_EDITOR

// Replicates the removed UUserDefinedEnum::GetDisplayNameMap(): returns the
// (name, value) pairs for user entries, skipping the auto-generated MAX/COUNT
// sentinel that UE 5.7 always appends to a UEnum.
static TArray<TPair<FName, int64>> GetEnumDisplayNamePairs(UUserDefinedEnum* Enum)
{
    TArray<TPair<FName, int64>> Result;
    if (!Enum) { return Result; }
    const int32 Num = Enum->NumEnums();
    for (int32 i = 0; i < Num; ++i)
    {
        const FString EntryName = Enum->GetNameStringByIndex(i);
        if (EntryName.EndsWith(TEXT("_MAX"))) { continue; }
        Result.Add(TPair<FName, int64>(FName(*EntryName), Enum->GetValueByIndex(i)));
    }
    return Result;
}

bool HandleEnumValueActions(
    const FString& Action,
    const TSharedPtr<FJsonObject>& Params,
    TSharedPtr<FJsonObject>& OutResult)
{
    if (Action == TEXT("add_enum_value"))
    {
        bool bHandled = false;
        UUserDefinedEnum* Enum = RequireEnum(Params, OutResult, bHandled);
        if (bHandled) { return true; }

        FString ValueName = GetPayloadString(Params, TEXT("valueName"));
        if (ValueName.IsEmpty()) { SetEnumResultFields(OutResult, false, TEXT("Missing required parameter: valueName")); return true; }

        TArray<TPair<FName, int64>> Names = GetEnumDisplayNamePairs(Enum);
        const int64 NextValue = Names.Num() > 0 ? Names.Last().Value + 1 : 0;
        Names.Add(TPair<FName, int64>(FName(*ValueName), NextValue));
        Enum->SetEnums(Names, UEnum::ECppForm::Namespaced);
        FinalizeEnum(Enum, GetPayloadBool(Params, TEXT("save"), false));

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("valueName"), ValueName);
        Result->SetNumberField(TEXT("index"), static_cast<double>(Names.Num() - 1));
        Result->SetNumberField(TEXT("valueCount"), Names.Num());
        OutResult = Result;
        return true;
    }

    if (Action == TEXT("remove_enum_value"))
    {
        bool bHandled = false;
        UUserDefinedEnum* Enum = RequireEnum(Params, OutResult, bHandled);
        if (bHandled) { return true; }

        FString ValueName = GetPayloadString(Params, TEXT("valueName"));
        if (ValueName.IsEmpty()) { SetEnumResultFields(OutResult, false, TEXT("Missing required parameter: valueName")); return true; }

        TArray<TPair<FName, int64>> Names = GetEnumDisplayNamePairs(Enum);
        const int32 Removed = Names.RemoveAll([&ValueName](const TPair<FName, int64>& Pair)
        {
            return Pair.Key.ToString() == ValueName;
        });
        if (Removed == 0) { SetEnumResultFields(OutResult, false, TEXT("Enum value not found")); return true; }

        Enum->SetEnums(Names, UEnum::ECppForm::Namespaced);
        FinalizeEnum(Enum, GetPayloadBool(Params, TEXT("save"), false));

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("valueName"), ValueName);
        Result->SetBoolField(TEXT("removed"), true);
        Result->SetNumberField(TEXT("valueCount"), Names.Num());
        OutResult = Result;
        return true;
    }

    if (Action == TEXT("rename_enum_value"))
    {
        bool bHandled = false;
        UUserDefinedEnum* Enum = RequireEnum(Params, OutResult, bHandled);
        if (bHandled) { return true; }

        FString ValueName = GetPayloadString(Params, TEXT("valueName"));
        FString NewValueName = GetPayloadString(Params, TEXT("newValueName"));
        if (ValueName.IsEmpty() || NewValueName.IsEmpty()) { SetEnumResultFields(OutResult, false, TEXT("Missing required parameter: valueName or newValueName")); return true; }

        TArray<TPair<FName, int64>> Names = GetEnumDisplayNamePairs(Enum);
        bool bFound = false;
        for (TPair<FName, int64>& Pair : Names)
        {
            if (Pair.Key.ToString() == ValueName) { Pair.Key = FName(*NewValueName); bFound = true; break; }
        }
        if (!bFound) { SetEnumResultFields(OutResult, false, TEXT("Enum value not found")); return true; }

        Enum->SetEnums(Names, UEnum::ECppForm::Namespaced);
        FinalizeEnum(Enum, GetPayloadBool(Params, TEXT("save"), false));

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("valueName"), NewValueName);
        OutResult = Result;
        return true;
    }

    if (Action == TEXT("reorder_enum_values"))
    {
        bool bHandled = false;
        UUserDefinedEnum* Enum = RequireEnum(Params, OutResult, bHandled);
        if (bHandled) { return true; }

        const TArray<TSharedPtr<FJsonValue>>* OrderArr = nullptr;
        if (!Params->TryGetArrayField(TEXT("order"), OrderArr) || !OrderArr) { SetEnumResultFields(OutResult, false, TEXT("Missing required parameter: order")); return true; }

        TArray<FString> RequestedOrder;
        for (const TSharedPtr<FJsonValue>& V : *OrderArr) { RequestedOrder.Add(V->AsString()); }

        TArray<TPair<FName, int64>> Current = GetEnumDisplayNamePairs(Enum);

        // Build the reordered map; a missing requested name is invalid -> no-op.
        TArray<TPair<FName, int64>> NewNames;
        for (const FString& Req : RequestedOrder)
        {
            bool bMatched = false;
            for (const TPair<FName, int64>& Pair : Current)
            {
                if (Pair.Key.ToString() == Req) { NewNames.Add(Pair); bMatched = true; break; }
            }
            if (!bMatched) { SetEnumResultFields(OutResult, true, TEXT("reorder no-op: requested value not present")); return true; }
        }

        // No-op guard: requested order already matches the current order.
        if (NewNames.Num() == Current.Num() && NewNames == Current)
        {
            TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
            Result->SetBoolField(TEXT("reordered"), false);
            Result->SetBoolField(TEXT("noOp"), true);
            OutResult = Result;
            return true;
        }

        Enum->SetEnums(NewNames, UEnum::ECppForm::Namespaced);
        FinalizeEnum(Enum, GetPayloadBool(Params, TEXT("save"), false));

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetBoolField(TEXT("reordered"), true);
        Result->SetBoolField(TEXT("noOp"), false);
        OutResult = Result;
        return true;
    }

    if (Action == TEXT("set_enum_value_metadata"))
    {
        bool bHandled = false;
        UUserDefinedEnum* Enum = RequireEnum(Params, OutResult, bHandled);
        if (bHandled) { return true; }

        FString ValueName = GetPayloadString(Params, TEXT("valueName"));
        FString Key = GetPayloadString(Params, TEXT("key"));
        FString Value = GetPayloadString(Params, TEXT("value"));
        if (ValueName.IsEmpty() || Key.IsEmpty()) { SetEnumResultFields(OutResult, false, TEXT("Missing required parameter: valueName or key")); return true; }

        const FString MetaKey = FString::Printf(TEXT("Value_%s_%s"), *ValueName, *Key);
        Enum->SetMetaData(*MetaKey, *Value);
        FinalizeEnum(Enum, GetPayloadBool(Params, TEXT("save"), false));

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("valueName"), ValueName);
        Result->SetStringField(TEXT("value"), Value);
        OutResult = Result;
        return true;
    }

    if (Action == TEXT("split_enum"))
    {
        bool bHandled = false;
        UUserDefinedEnum* Enum = RequireEnum(Params, OutResult, bHandled);
        if (bHandled) { return true; }

        FString NewName = GetPayloadString(Params, TEXT("newEnumName"));
        if (NewName.IsEmpty()) { SetEnumResultFields(OutResult, false, TEXT("Missing required parameter: newEnumName")); return true; }

        const TArray<TSharedPtr<FJsonValue>>* ValuesArr = nullptr;
        if (!Params->TryGetArrayField(TEXT("values"), ValuesArr) || !ValuesArr) { SetEnumResultFields(OutResult, false, TEXT("Missing required parameter: values")); return true; }
        TArray<FString> KeepNames;
        for (const TSharedPtr<FJsonValue>& V : *ValuesArr) { KeepNames.Add(V->AsString()); }

        const TArray<TPair<FName, int64>>& Current = GetEnumDisplayNamePairs(Enum);
        TArray<TPair<FName, int64>> Subset;
        for (const TPair<FName, int64>& Pair : Current)
        {
            if (KeepNames.Contains(Pair.Key.ToString())) { Subset.Add(Pair); }
        }
        if (Subset.Num() == 0) { SetEnumResultFields(OutResult, false, TEXT("No matching values to split")); return true; }

        FString Path = GetPayloadString(Params, TEXT("path"), TEXT("/Game/Enums"));
        FString PathError;
        FString SanitizedName = SanitizeAssetName(NewName);
        FString PackageName;
        if (!ValidateAssetCreationPath(Path, SanitizedName, PackageName, PathError)) { SetEnumResultFields(OutResult, false, PathError); return true; }

        UPackage* Package = CreatePackage(*PackageName);
        if (!Package) { SetEnumResultFields(OutResult, false, TEXT("Failed to create package")); return true; }

        UUserDefinedEnum* NewEnum = Cast<UUserDefinedEnum>(FEnumEditorUtils::CreateUserDefinedEnum(Package, FName(*SanitizedName), RF_Public | RF_Standalone));
        if (!NewEnum) { SetEnumResultFields(OutResult, false, TEXT("Failed to create split enum")); return true; }

        NewEnum->SetEnums(Subset, UEnum::ECppForm::Namespaced);
        FinalizeEnum(NewEnum, GetPayloadBool(Params, TEXT("save"), false));
        FAssetRegistryModule::AssetCreated(NewEnum);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("enumPath"), PackageName + TEXT(".") + SanitizedName);
        Result->SetStringField(TEXT("enumName"), SanitizedName);
        Result->SetNumberField(TEXT("valueCount"), Subset.Num());
        Result->SetStringField(TEXT("sourceEnumPath"), GetPayloadString(Params, TEXT("enumPath")));
        OutResult = Result;
        return true;
    }

    return false;
}

#endif // WITH_EDITOR
