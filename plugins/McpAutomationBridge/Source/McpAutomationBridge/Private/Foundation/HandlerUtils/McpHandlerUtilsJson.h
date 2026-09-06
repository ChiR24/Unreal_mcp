#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace McpHandlerUtils
{
inline bool TryGetRequiredString(
    const TSharedPtr<FJsonObject>& Payload,
    const FString& FieldName,
    FString& OutValue,
    FString& OutError)
{
    if (!Payload.IsValid())
    {
        OutError = FString::Printf(TEXT("Payload is null when extracting '%s'"), *FieldName);
        return false;
    }
    if (!Payload->TryGetStringField(FieldName, OutValue))
    {
        OutError = FString::Printf(TEXT("Missing required field '%s'"), *FieldName);
        return false;
    }
    if (OutValue.IsEmpty())
    {
        OutError = FString::Printf(TEXT("Field '%s' is empty"), *FieldName);
        return false;
    }
    return true;
}

inline FString GetOptionalString(
    const TSharedPtr<FJsonObject>& Payload,
    const FString& FieldName,
    const FString& DefaultValue = FString())
{
    FString Value;
    return Payload.IsValid() && Payload->TryGetStringField(FieldName, Value) ? Value : DefaultValue;
}

inline int32 GetOptionalInt(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName, int32 DefaultValue = 0)
{
    int32 Value = DefaultValue;
    if (Payload.IsValid())
    {
        Payload->TryGetNumberField(FieldName, Value);
    }
    return Value;
}

inline double GetOptionalFloat(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName, double DefaultValue = 0.0)
{
    double Value = DefaultValue;
    if (Payload.IsValid())
    {
        Payload->TryGetNumberField(FieldName, Value);
    }
    return Value;
}

inline bool GetOptionalBool(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName, bool DefaultValue = false)
{
    bool Value = DefaultValue;
    if (Payload.IsValid())
    {
        Payload->TryGetBoolField(FieldName, Value);
    }
    return Value;
}

MCPAUTOMATIONBRIDGE_API FString JsonValueToString(const TSharedPtr<FJsonValue>& Value);
}
