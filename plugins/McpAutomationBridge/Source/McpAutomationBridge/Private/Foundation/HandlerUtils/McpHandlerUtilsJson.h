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

/**
 * FJsonValue exposes no TryGetString on modern engine versions (the helper that
 * used to exist was removed), so every call site that wants "is this JSON value
 * a string, and if so give it to me" funnels through here. Returns false for a
 * null value or any non-string type; OutValue is left untouched in that case.
 */
inline bool TryGetJsonValueString(const TSharedPtr<FJsonValue>& Value, FString& OutValue)
{
    if (!Value.IsValid() || Value->Type != EJson::String)
    {
        return false;
    }
    OutValue = Value->AsString();
    return true;
}

/** Read a JSON value as a string, or DefaultValue when it is null/not a string. */
inline FString GetJsonValueString(
    const TSharedPtr<FJsonValue>& Value, const FString& DefaultValue = FString())
{
    FString Out;
    return TryGetJsonValueString(Value, Out) ? Out : DefaultValue;
}

MCPAUTOMATIONBRIDGE_API FString JsonValueToString(const TSharedPtr<FJsonValue>& Value);
}
