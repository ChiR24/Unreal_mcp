#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace McpHandlerUtils
{
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

/**
 * Read a JSON array field as strings, skipping any element that is not a
 * string. A missing field, a null payload or a non-array value all yield an
 * empty array, so callers never need to pre-check. This is the one place that
 * knows the shape; the configure/visibility paths on both the native gateway
 * and the dynamic tool manager read their `tools` list through it.
 */
inline TArray<FString> GetStringArrayField(
    const TSharedPtr<FJsonObject>& Payload, const FString& FieldName)
{
    TArray<FString> Names;
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!Payload.IsValid() || !Payload->TryGetArrayField(FieldName, Values) || !Values)
    {
        return Names;
    }
    for (const TSharedPtr<FJsonValue>& Value : *Values)
    {
        FString Element;
        if (TryGetJsonValueString(Value, Element))
        {
            Names.Add(MoveTemp(Element));
        }
    }
    return Names;
}

MCPAUTOMATIONBRIDGE_API FString JsonValueToString(const TSharedPtr<FJsonValue>& Value);
}
