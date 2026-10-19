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

// A value name the enum already has, by internal or display name, refused: two entries of one name broke the enum.
static bool RefuseTakenValueName(UUserDefinedEnum* Enum, const FString& Name, int32 ExceptIndex, TSharedPtr<FJsonObject>& OutResult)
{
    const int32 Taken = McpFindEnumValue(Enum, Name);
    if (Taken == INDEX_NONE || Taken == ExceptIndex) { return false; }
    SetEnumResultFields(OutResult, false, FString::Printf(TEXT("The enum already has a value named '%s'."), *Name), TEXT("VALUE_EXISTS"));
    return true;
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
        if (RefuseTakenValueName(Enum, ValueName, INDEX_NONE, OutResult)) { return true; }

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

        // The engine's own remove renumbers the later values AND remaps every loaded object and graph pin
        // holding them, by name (FEnumEditorUtils::BroadcastChanges). Renumbering by hand left a stored
        // value reading as whichever entry now sat at its number.
        const int32 Index = McpFindEnumValue(Enum, ValueName);
        if (Index == INDEX_NONE)
        {
            SetEnumResultFields(OutResult, false, FString::Printf(TEXT("Enum value '%s' not found."), *ValueName), TEXT("NOT_FOUND"));
            return true;
        }
        const int32 Before = GetEnumDisplayNamePairs(Enum).Num();
        FEnumEditorUtils::RemoveEnumeratorFromUserDefinedEnum(Enum, Index);
        FinalizeEnum(Enum, EnumSaveRequested(Params));

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("valueName"), ValueName);
        Result->SetBoolField(TEXT("removed"), true);
        Result->SetNumberField(TEXT("valueCount"), Before - 1);
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

        const int32 Index = McpFindEnumValue(Enum, ValueName);
        if (Index == INDEX_NONE)
        {
            SetEnumResultFields(OutResult, false, FString::Printf(TEXT("Enum value '%s' not found."), *ValueName), TEXT("NOT_FOUND"));
            return true;
        }
        if (RefuseTakenValueName(Enum, NewValueName, Index, OutResult)) { return true; }

        // The display name, as the enum editor renames: stored values (defaults, pins, table cells) name the entry by
        // its internal name, so renaming that left every one of them reading as nothing.
        Enum->Modify();
        if (!FEnumEditorUtils::SetEnumeratorDisplayName(Enum, Index, FText::FromString(NewValueName)))
        {
            SetEnumResultFields(OutResult, false, FString::Printf(TEXT("'%s' is not a valid enum value name."), *NewValueName), TEXT("INVALID_ARGUMENT"));
            return true;
        }
        FinalizeEnum(Enum, EnumSaveRequested(Params));

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("valueName"), NewValueName);
        Result->SetStringField(TEXT("internalName"), Enum->GetNameStringByIndex(Index));
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

        const TArray<TPair<FName, int64>> Current = GetEnumDisplayNamePairs(Enum);
        TArray<int32> Order;
        for (const TSharedPtr<FJsonValue>& V : *OrderArr)
        {
            const FString Req = V->AsString();
            const int32 Index = McpFindEnumValue(Enum, Req);
            if (Index == INDEX_NONE)
            {
                SetEnumResultFields(OutResult, false, FString::Printf(TEXT("Enum value '%s' in order is not a value of this enum; nothing was reordered."), *Req), TEXT("NOT_FOUND"));
                return true;
            }
            Order.AddUnique(Index);
        }
        TArray<TPair<FName, int64>> NewNames;
        for (const int32 Index : Order) { NewNames.Add(Current[Index]); }

        if (NewNames.Num() != Current.Num())
        {
            SetEnumResultFields(OutResult, false, TEXT("reorder_enum_values must list every current value once; partial lists are rejected to avoid dropping values"));
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

        // One engine move per misplaced value: each move remaps the loaded objects and pins holding the moved
        // values by name, where renumbering by hand changed what every stored value meant.
        for (int32 Target = 0; Target < NewNames.Num(); ++Target)
        {
            const int32 From = GetEnumDisplayNamePairs(Enum).IndexOfByPredicate(
                [&NewNames, Target](const TPair<FName, int64>& Pair) { return Pair.Key == NewNames[Target].Key; });
            if (From != Target) { FEnumEditorUtils::MoveEnumeratorInUserDefinedEnum(Enum, From, Target); }
        }
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
        const int32 Index = McpFindEnumValue(Enum, ValueName);
        if (Index == INDEX_NONE)
        {
            SetEnumResultFields(OutResult, false, FString::Printf(TEXT("Enum value '%s' not found."), *ValueName), TEXT("NOT_FOUND"));
            return true;
        }

        // On the entry itself (<name>.<key>, as UEnum keeps a ToolTip): a Value_<name>_<key> key on the enum was read
        // by nothing, and a name the enum lacks was answered as set.
        Enum->Modify();
        Enum->SetMetaData(*Key, *Value, Index);
        FinalizeEnum(Enum, EnumSaveRequested(Params));

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("valueName"), ValueName);
        Result->SetStringField(TEXT("key"), Key);
        Result->SetStringField(TEXT("value"), Enum->GetMetaData(*Key, Index));
        OutResult = Result;
        return true;
    }

    return HandleEnumSplit(Action, Params, OutResult);
}
