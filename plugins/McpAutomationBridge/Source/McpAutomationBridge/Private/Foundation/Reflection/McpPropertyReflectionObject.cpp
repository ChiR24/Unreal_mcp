#include "Foundation/Reflection/McpPropertyReflectionPrivate.h"

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

TSharedPtr<FJsonObject> ExportObjectToJsonBounded(UObject* Object, bool bIncludeTransient, int32 MaxProperties)
{
    if (!Object) return nullptr;

    const int32 Cap = FMath::Max(0, MaxProperties);
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    int32 Total = 0;
    int32 Emitted = 0;
    bool bTruncated = false;

    for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
    {
        FProperty* Property = *It;
        if (!Property ||
            (!bIncludeTransient && Property->HasAnyPropertyFlags(CPF_Transient)) ||
            Property->HasAnyPropertyFlags(CPF_Deprecated))
        {
            continue;
        }

        ++Total;
        // Truncation is claimed ONLY when the cap is what stopped us. A property
        // that simply fails to export must not be reported as withheld.
        if (Emitted >= Cap)
        {
            bTruncated = true;
            continue;
        }

        TSharedPtr<FJsonValue> Value = McpPropertyReflection::ExportPropertyToJsonValue(Object, Property);
        if (Value.IsValid())
        {
            Result->SetField(Property->GetName(), Value);
            ++Emitted;
        }
    }

    // The leading dollar cannot occur in a UPROPERTY name, so these bookkeeping
    // keys can never collide with a reflected property on this same object.
    Result->SetNumberField(TEXT("$mcpPropertyCount"), Total);
    Result->SetNumberField(TEXT("$mcpMaxProperties"), Cap);
    Result->SetBoolField(TEXT("$mcpTruncated"), bTruncated);

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
}
