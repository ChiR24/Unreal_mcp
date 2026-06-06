#pragma once

#include "CoreMinimal.h"
#include "McpHandlerUtils.h"

#include "Dom/JsonObject.h"

namespace McpCombatHandlers
{
inline FString GetStringFieldCombat(
    const TSharedPtr<FJsonObject>& Obj,
    const FString& FieldName,
    const FString& DefaultValue = FString())
{
    return GetJsonStringField(Obj, FieldName, DefaultValue);
}

inline double GetNumberFieldCombat(
    const TSharedPtr<FJsonObject>& Obj,
    const FString& FieldName,
    double DefaultValue = 0.0)
{
    return GetJsonNumberField(Obj, FieldName, DefaultValue);
}

inline bool GetBoolFieldCombat(
    const TSharedPtr<FJsonObject>& Obj,
    const FString& FieldName,
    bool bDefaultValue = false)
{
    return GetJsonBoolField(Obj, FieldName, bDefaultValue);
}

inline FVector GetVectorFromJsonCombat(const TSharedPtr<FJsonObject>& Obj)
{
    if (!Obj.IsValid())
    {
        return FVector::ZeroVector;
    }

    return FVector(
        GetNumberFieldCombat(Obj, TEXT("x"), 0.0),
        GetNumberFieldCombat(Obj, TEXT("y"), 0.0),
        GetNumberFieldCombat(Obj, TEXT("z"), 0.0));
}
}
