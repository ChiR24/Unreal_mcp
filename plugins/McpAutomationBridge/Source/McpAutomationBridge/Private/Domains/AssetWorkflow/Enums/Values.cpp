#include "Domains/AssetWorkflow/Enums/Shared.h"
#include "Kismet2/EnumEditorUtils.h"

#if WITH_EDITOR

static TArray<TPair<FName, int64>> GetEnumDisplayNamePairs(UUserDefinedEnum* Enum)
{
    TArray<TPair<FName, int64>> Result;
    if (!Enum) { return Result; }
    const int32 Num = Enum->NumEnums();
    for (int32 i = 0; i < Num; ++i)
    {
        if (Enum->GetNameStringByIndex(i).EndsWith(TEXT("_MAX"))) { continue; }
        Result.Add(TPair<FName, int64>(Enum->GetNameByIndex(i), Enum->GetValueByIndex(i)));
    }
    return Result;
}

static bool MatchesEnumShortName(const FName& StoredName, const FString& RequestedName)
{
    FString Str = StoredName.ToString();
    if (Str == RequestedName) return true;
    int32 Pos = Str.Find(TEXT("::"), ESearchCase::CaseSensitive);
    return (Pos != INDEX_NONE && Str.Mid(Pos + 2) == RequestedName);
}

static FString ExtractEnumShortName(const FName& EnumFName)
{
    FString Str = EnumFName.ToString();
    int32 Pos = Str.Find(TEXT("::"), ESearchCase::CaseSensitive);
    return (Pos != INDEX_NONE) ? Str.Mid(Pos + 2) : Str;
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

        Enum->Modify();
        TArray<TPair<FName, int64>> Names = GetEnumDisplayNamePairs(Enum);
        const FString FullNameStr = Enum->GenerateFullEnumName(*ValueName);
        Names.Emplace(*FullNameStr, 0);
        for (int32 i = 0; i < Names.Num(); ++i) { Names[i].Value = i; }
        MCP_SET_ENUMS(Enum, Names, Enum->GetCppForm());
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

        Enum->Modify();
        TArray<TPair<FName, int64>> Names = GetEnumDisplayNamePairs(Enum);
        const int32 Removed = Names.RemoveAll([&ValueName](const TPair<FName, int64>& Pair)
        {
            return MatchesEnumShortName(Pair.Key, ValueName);
        });
        if (Removed == 0)
        {
            SetEnumResultFields(OutResult, false, TEXT("Enum value not found"));
            return true;
        }
        for (int32 i = 0; i < Names.Num(); ++i) { Names[i].Value = i; }
        MCP_SET_ENUMS(Enum, Names, Enum->GetCppForm());
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

        Enum->Modify();
        TArray<TPair<FName, int64>> Names = GetEnumDisplayNamePairs(Enum);
        const FString NewFullName = Enum->GenerateFullEnumName(*NewValueName);
        bool bFound = false;
        for (TPair<FName, int64>& Pair : Names)
        {
            if (MatchesEnumShortName(Pair.Key, ValueName))
            {
                Pair.Key = *NewFullName;
                bFound = true;
                break;
            }
        }
        if (!bFound)
        {
            SetEnumResultFields(OutResult, false, TEXT("Enum value not found"));
            return true;
        }
        for (int32 i = 0; i < Names.Num(); ++i) { Names[i].Value = i; }
        MCP_SET_ENUMS(Enum, Names, Enum->GetCppForm());
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

        Enum->Modify();
        TArray<TPair<FName, int64>> OldNames = GetEnumDisplayNamePairs(Enum);
        TArray<TPair<FName, int64>> Current = OldNames;

        TArray<TPair<FName, int64>> NewNames;
        for (const FString& Req : RequestedOrder)
        {
            bool bMatched = false;
            for (const TPair<FName, int64>& Pair : Current)
            {
                if (MatchesEnumShortName(Pair.Key, Req)) { NewNames.Add(Pair); bMatched = true; break; }
            }
            if (!bMatched)
            {
                SetEnumResultFields(OutResult, true, TEXT("reorder no-op: requested value not present"));
                return true;
            }
        }

        if (NewNames.Num() != Current.Num())
        {
            SetEnumResultFields(OutResult, false, TEXT("reorder_enum_values must list every current value; partial lists are rejected to avoid dropping values"));
            return true;
        }

        bool bSameOrder = true;
        for (int32 i = 0; i < NewNames.Num(); ++i)
        {
            if (NewNames[i].Key != Current[i].Key) { bSameOrder = false; break; }
        }
        if (bSameOrder)
        {
            TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
            Result->SetBoolField(TEXT("reordered"), false);
            Result->SetBoolField(TEXT("noOp"), true);
            OutResult = Result;
            return true;
        }

        for (int32 i = 0; i < NewNames.Num(); ++i) { NewNames[i].Value = i; }
        MCP_SET_ENUMS(Enum, NewNames, Enum->GetCppForm());
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

        TArray<TPair<FName, int64>> Current = GetEnumDisplayNamePairs(Enum);
        TArray<FString> KeepShortNames;
        for (const TPair<FName, int64>& Pair : Current)
        {
            FString Short = ExtractEnumShortName(Pair.Key);
            if (KeepNames.Contains(Short)) { KeepShortNames.Add(Short); }
        }
        if (KeepShortNames.Num() == 0) { SetEnumResultFields(OutResult, false, TEXT("No matching values to split")); return true; }

        FString Path = GetPayloadString(Params, TEXT("path"), TEXT("/Game/Enums"));
        FString PathError;
        FString SanitizedName = SanitizeAssetName(NewName);
        FString PackageName;
        if (!ValidateAssetCreationPath(Path, SanitizedName, PackageName, PathError)) { SetEnumResultFields(OutResult, false, PathError); return true; }

        UPackage* Package = CreatePackage(*PackageName);
        if (!Package) { SetEnumResultFields(OutResult, false, TEXT("Failed to create package")); return true; }

        UUserDefinedEnum* NewEnum = Cast<UUserDefinedEnum>(FEnumEditorUtils::CreateUserDefinedEnum(Package, FName(*SanitizedName), RF_Public | RF_Standalone));
        if (!NewEnum) { SetEnumResultFields(OutResult, false, TEXT("Failed to create split enum")); return true; }

        TArray<TPair<FName, int64>> NewNames;
        for (int32 i = 0; i < KeepShortNames.Num(); ++i)
        {
            NewNames.Emplace(*NewEnum->GenerateFullEnumName(*KeepShortNames[i]), static_cast<int64>(i));
        }
        MCP_SET_ENUMS(NewEnum, NewNames, NewEnum->GetCppForm());
        FinalizeEnum(NewEnum, GetPayloadBool(Params, TEXT("save"), false));
        FAssetRegistryModule::AssetCreated(NewEnum);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("enumPath"), PackageName + TEXT(".") + SanitizedName);
        Result->SetStringField(TEXT("enumName"), SanitizedName);
        Result->SetNumberField(TEXT("valueCount"), KeepShortNames.Num());
        Result->SetStringField(TEXT("sourceEnumPath"), GetPayloadString(Params, TEXT("enumPath")));
        Result->SetBoolField(TEXT("sourceEnumModified"), false);
        Result->SetStringField(TEXT("message"), TEXT("split_enum copies the listed values into a new enum; the source enum is left unchanged"));
        OutResult = Result;
        return true;
    }

    return false;
}

#endif // WITH_EDITOR
