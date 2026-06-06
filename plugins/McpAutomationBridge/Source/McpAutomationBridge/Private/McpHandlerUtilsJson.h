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

inline bool TryGetRequiredInt(
    const TSharedPtr<FJsonObject>& Payload,
    const FString& FieldName,
    int32& OutValue,
    FString& OutError)
{
    if (!Payload.IsValid())
    {
        OutError = FString::Printf(TEXT("Payload is null when extracting '%s'"), *FieldName);
        return false;
    }
    if (!Payload->TryGetNumberField(FieldName, OutValue))
    {
        OutError = FString::Printf(TEXT("Missing required integer field '%s'"), *FieldName);
        return false;
    }
    return true;
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

inline bool TryGetRequiredFloat(
    const TSharedPtr<FJsonObject>& Payload,
    const FString& FieldName,
    double& OutValue,
    FString& OutError)
{
    if (!Payload.IsValid())
    {
        OutError = FString::Printf(TEXT("Payload is null when extracting '%s'"), *FieldName);
        return false;
    }
    if (!Payload->TryGetNumberField(FieldName, OutValue))
    {
        OutError = FString::Printf(TEXT("Missing required number field '%s'"), *FieldName);
        return false;
    }
    return true;
}

inline bool TryGetRequiredBool(
    const TSharedPtr<FJsonObject>& Payload,
    const FString& FieldName,
    bool& OutValue,
    FString& OutError)
{
    if (!Payload.IsValid())
    {
        OutError = FString::Printf(TEXT("Payload is null when extracting '%s'"), *FieldName);
        return false;
    }
    if (!Payload->TryGetBoolField(FieldName, OutValue))
    {
        OutError = FString::Printf(TEXT("Missing required boolean field '%s'"), *FieldName);
        return false;
    }
    return true;
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

inline bool TryGetRequiredObject(
    const TSharedPtr<FJsonObject>& Payload,
    const FString& FieldName,
    TSharedPtr<FJsonObject>& OutValue,
    FString& OutError)
{
    if (!Payload.IsValid())
    {
        OutError = FString::Printf(TEXT("Payload is null when extracting '%s'"), *FieldName);
        return false;
    }
    const TSharedPtr<FJsonObject>* ObjectPtr = nullptr;
    if (!Payload->TryGetObjectField(FieldName, ObjectPtr))
    {
        OutError = FString::Printf(TEXT("Missing required object field '%s'"), *FieldName);
        return false;
    }
    OutValue = *ObjectPtr;
    return OutValue.IsValid();
}

inline bool TryGetRequiredArray(
    const TSharedPtr<FJsonObject>& Payload,
    const FString& FieldName,
    TArray<TSharedPtr<FJsonValue>>& OutValue,
    FString& OutError)
{
    if (!Payload.IsValid())
    {
        OutError = FString::Printf(TEXT("Payload is null when extracting '%s'"), *FieldName);
        return false;
    }
    const TArray<TSharedPtr<FJsonValue>>* ArrayPtr = nullptr;
    if (!Payload->TryGetArrayField(FieldName, ArrayPtr))
    {
        OutError = FString::Printf(TEXT("Missing required array field '%s'"), *FieldName);
        return false;
    }
    OutValue = *ArrayPtr;
    return true;
}

MCPAUTOMATIONBRIDGE_API FString JsonValueToString(const TSharedPtr<FJsonValue>& Value);
}
