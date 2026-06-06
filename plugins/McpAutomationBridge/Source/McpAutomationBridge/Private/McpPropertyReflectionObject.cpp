#include "McpPropertyReflectionPrivate.h"

namespace McpPropertyReflection
{
TSharedPtr<FJsonObject> ExportObjectToJson(UObject* Object, bool bIncludeTransient)
{
    if (!Object) return nullptr;

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
    {
        FProperty* Property = *It;
        if (!Property ||
            (!bIncludeTransient && Property->HasAnyPropertyFlags(CPF_Transient)) ||
            Property->HasAnyPropertyFlags(CPF_Deprecated))
        {
            continue;
        }

        TSharedPtr<FJsonValue> Value = McpPropertyReflection::ExportPropertyToJsonValue(Object, Property);
        if (Value.IsValid()) Result->SetField(Property->GetName(), Value);
    }

    return Result;
}

TSharedPtr<FJsonObject> ExportPropertiesToJson(UObject* Object, const TArray<FName>& PropertyNames)
{
    if (!Object) return nullptr;

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    UClass* Class = Object->GetClass();

    for (const FName& PropName : PropertyNames)
    {
        FProperty* Property = Class->FindPropertyByName(PropName);
        TSharedPtr<FJsonValue> Value = Property ? McpPropertyReflection::ExportPropertyToJsonValue(Object, Property) : nullptr;
        if (Value.IsValid()) Result->SetField(Property->GetName(), Value);
    }

    return Result;
}

int32 ApplyJsonValuesToObject(UObject* Object, const TMap<FName, TSharedPtr<FJsonValue>>& JsonValues, TMap<FName, FString>* OutErrors)
{
    if (!Object) return 0;

    int32 SuccessCount = 0;
    UClass* Class = Object->GetClass();
    for (const auto& Pair : JsonValues)
    {
        FProperty* Property = Class->FindPropertyByName(Pair.Key);
        if (!Property)
        {
            if (OutErrors) OutErrors->Add(Pair.Key, TEXT("Property not found"));
            continue;
        }

        FString Error;
        if (McpPropertyReflection::ApplyJsonValueToProperty(Object, Property, Pair.Value, Error)) ++SuccessCount;
        else if (OutErrors) OutErrors->Add(Pair.Key, Error);
    }

    return SuccessCount;
}

int32 ApplyJsonObjectToObject(UObject* Object, const TSharedPtr<FJsonObject>& JsonObject, TMap<FName, FString>* OutErrors)
{
    if (!Object || !JsonObject.IsValid()) return 0;

    TMap<FName, TSharedPtr<FJsonValue>> Values;
    for (const auto& Pair : JsonObject->Values)
    {
        Values.Add(FName(*Pair.Key), Pair.Value);
    }

    return ApplyJsonValuesToObject(Object, Values, OutErrors);
}
}
