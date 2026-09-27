#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
// GetJsonStringField / GetJsonIntField / GetJsonNumberField / GetJsonBoolField.
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersJsonFields.h"

namespace McpHandlerUtils
{
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
// Strings as a JSON string array.
inline TArray<TSharedPtr<FJsonValue>> ToJsonStringArray(const TArray<FString>& Strings)
{
    TArray<TSharedPtr<FJsonValue>> Values;
    for (const FString& String : Strings)
    {
        Values.Add(MakeShared<FJsonValueString>(String));
    }
    return Values;
}

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

/**
 * When the payload lists names in ListField, keeps only the Rows whose "name"
 * is one of them (case-insensitive) and writes the listed names no row matched
 * to Result's MissingField. Checking one component of a 27-component Blueprint
 * used to return all 27. Without ListField, Rows are left untouched.
 */
MCPAUTOMATIONBRIDGE_API void FilterRowsByListedNames(
    const TSharedPtr<FJsonObject>& Payload, const FString& ListField,
    TArray<TSharedPtr<FJsonValue>>& Rows, const TSharedPtr<FJsonObject>& Result, const FString& MissingField);
}
