#include "Domains/AssetWorkflow/Enums/Shared.h"
#include "Kismet2/EnumEditorUtils.h"


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

        FString ValueName = GetJsonStringField(Params, TEXT("valueName"));
        if (ValueName.IsEmpty()) { SetEnumResultFields(OutResult, false, TEXT("Missing required parameter: valueName"), TEXT("MISSING_PARAMETER")); return true; }

        Enum->Modify();
        TArray<TPair<FName, int64>> Names = GetEnumDisplayNamePairs(Enum);
        const FString FullNameStr = Enum->GenerateFullEnumName(*ValueName);
        Names.Emplace(*FullNameStr, 0);
        for (int32 i = 0; i < Names.Num(); ++i) { Names[i].Value = i; }
        MCP_SET_ENUMS(Enum, Names, Enum->GetCppForm());
        FinalizeEnum(Enum, EnumSaveRequested(Params));

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

        FString ValueName = GetJsonStringField(Params, TEXT("valueName"));
        if (ValueName.IsEmpty()) { SetEnumResultFields(OutResult, false, TEXT("Missing required parameter: valueName"), TEXT("MISSING_PARAMETER")); return true; }

        Enum->Modify();
        TArray<TPair<FName, int64>> Names = GetEnumDisplayNamePairs(Enum);
        const int32 Removed = Names.RemoveAll([&ValueName](const TPair<FName, int64>& Pair)
        {
            return MatchesEnumShortName(Pair.Key, ValueName);
        });
        if (Removed == 0)
        {
            SetEnumResultFields(OutResult, false, FString::Printf(TEXT("Enum value '%s' not found."), *ValueName), TEXT("NOT_FOUND"));
            return true;
        }
        for (int32 i = 0; i < Names.Num(); ++i) { Names[i].Value = i; }
        MCP_SET_ENUMS(Enum, Names, Enum->GetCppForm());
        FinalizeEnum(Enum, EnumSaveRequested(Params));

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

        FString ValueName = GetJsonStringField(Params, TEXT("valueName"));
        FString NewValueName = GetJsonStringField(Params, TEXT("newValueName"));
        if (ValueName.IsEmpty() || NewValueName.IsEmpty()) { SetEnumResultFields(OutResult, false, TEXT("Missing required parameter: valueName or newValueName"), TEXT("MISSING_PARAMETER")); return true; }

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
            SetEnumResultFields(OutResult, false, FString::Printf(TEXT("Enum value '%s' not found."), *ValueName), TEXT("NOT_FOUND"));
            return true;
        }
        for (int32 i = 0; i < Names.Num(); ++i) { Names[i].Value = i; }
        MCP_SET_ENUMS(Enum, Names, Enum->GetCppForm());
        FinalizeEnum(Enum, EnumSaveRequested(Params));

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
        if (!Params->TryGetArrayField(TEXT("order"), OrderArr) || !OrderArr) { SetEnumResultFields(OutResult, false, TEXT("Missing required parameter: order"), TEXT("MISSING_PARAMETER")); return true; }

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
                SetEnumResultFields(OutResult, false, FString::Printf(TEXT("Enum value '%s' in order is not a value of this enum; nothing was reordered."), *Req), TEXT("NOT_FOUND"));
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
        FinalizeEnum(Enum, EnumSaveRequested(Params));

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

        FString ValueName = GetJsonStringField(Params, TEXT("valueName"));
        FString Key = GetJsonStringField(Params, TEXT("key"));
        FString Value = GetJsonStringField(Params, TEXT("value"));
        if (ValueName.IsEmpty() || Key.IsEmpty()) { SetEnumResultFields(OutResult, false, TEXT("Missing required parameter: valueName or key"), TEXT("MISSING_PARAMETER")); return true; }

        const FString MetaKey = FString::Printf(TEXT("Value_%s_%s"), *ValueName, *Key);
        Enum->SetMetaData(*MetaKey, *Value);
        FinalizeEnum(Enum, EnumSaveRequested(Params));

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

        FString NewName = GetJsonStringField(Params, TEXT("newEnumName"));
        if (NewName.IsEmpty()) { SetEnumResultFields(OutResult, false, TEXT("Missing required parameter: newEnumName"), TEXT("MISSING_PARAMETER")); return true; }

        // The new enum takes either the named `values` or every value from
        // position `index` on (the declared index used to be ignored).
        const TArray<TSharedPtr<FJsonValue>>* ValuesArr = nullptr;
        const bool bHasValues = Params->TryGetArrayField(TEXT("values"), ValuesArr) && ValuesArr;
        const bool bHasIndex = Params->HasField(TEXT("index"));
        const int32 SplitIndex = static_cast<int32>(GetJsonNumberField(Params, TEXT("index"), 0.0));
        if (bHasValues == bHasIndex)
        {
            SetEnumResultFields(OutResult, false, TEXT("Pass values (names to copy) or index (copy every value from that position on), not both."), bHasValues ? TEXT("INVALID_ARGUMENT") : TEXT("MISSING_PARAMETER"));
            return true;
        }
        TArray<FString> KeepNames;
        if (bHasValues) { for (const TSharedPtr<FJsonValue>& V : *ValuesArr) { KeepNames.Add(V->AsString()); } }

        TArray<TPair<FName, int64>> Current = GetEnumDisplayNamePairs(Enum);
        TArray<FString> KeepShortNames;
        for (int32 i = 0; i < Current.Num(); ++i)
        {
            FString Short = ExtractEnumShortName(Current[i].Key);
            if (bHasValues ? KeepNames.Contains(Short) : i >= SplitIndex) { KeepShortNames.Add(Short); }
        }
        if (KeepShortNames.Num() == 0) { SetEnumResultFields(OutResult, false, TEXT("No matching values to split")); return true; }

        FString Path = GetJsonStringField(Params, TEXT("path"), TEXT("/Game/Enums"));
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
        FinalizeEnum(NewEnum, EnumSaveRequested(Params));
        FAssetRegistryModule::AssetCreated(NewEnum);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("enumPath"), PackageName + TEXT(".") + SanitizedName);
        Result->SetStringField(TEXT("enumName"), SanitizedName);
        Result->SetNumberField(TEXT("valueCount"), KeepShortNames.Num());
        Result->SetStringField(TEXT("sourceEnumPath"), GetJsonStringField(Params, TEXT("enumPath")));
        Result->SetBoolField(TEXT("sourceEnumModified"), false);
        Result->SetStringField(TEXT("message"), TEXT("split_enum copies the listed values into a new enum; the source enum is left unchanged"));
        OutResult = Result;
        return true;
    }

    return false;
}

