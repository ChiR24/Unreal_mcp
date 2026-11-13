#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
// GetJsonStringField / GetJsonIntField / GetJsonNumberField / GetJsonBoolField.
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"

// A JSON object key as an FString. FJsonObject::Values is keyed by FString up to 5.7 and by a shared-string type from
// 5.8 that no FString method or conversion takes, so a key is copied from its length and its text.
template <typename KeyType>
FString McpJsonKey(const KeyType& Key)
{
    return FString(Key.Len(), *Key);
}

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

// Strings as a JSON string array.
inline TArray<TSharedPtr<FJsonValue>> ToJsonStringArray(const TArray<FString>& Strings)
{
    TArray<TSharedPtr<FJsonValue>> Values;
    Values.Reserve(Strings.Num());
    for (const FString& String : Strings)
    {
        Values.Add(MakeShared<FJsonValueString>(String));
    }
    return Values;
}

// FNames as a JSON string array (its own name: an overload would make ToJsonStringArray({TEXT("...")}) ambiguous).
inline TArray<TSharedPtr<FJsonValue>> NamesToJsonStringArray(const TArray<FName>& Names)
{
    TArray<TSharedPtr<FJsonValue>> Values;
    Values.Reserve(Names.Num());
    for (const FName& Name : Names)
    {
        Values.Add(MakeShared<FJsonValueString>(Name.ToString()));
    }
    return Values;
}

// A reply's nextCall (or any call it hands back): execute Tool.Action with Params, an empty object when none.
inline TSharedPtr<FJsonObject> MakeExecuteCall(const FString& Tool, const FString& Action, const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Call = MakeShared<FJsonObject>();
    Call->SetStringField(TEXT("operation"), TEXT("execute"));
    Call->SetStringField(TEXT("tool"), Tool);
    Call->SetStringField(TEXT("action"), Action);
    Call->SetObjectField(TEXT("params"), Params.IsValid() ? Params : MakeShared<FJsonObject>());
    return Call;
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

/** The object a field holds; null when the payload is null or the field is missing or holds anything else. */
inline TSharedPtr<FJsonObject> GetObjectFieldOrNull(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName)
{
    const TSharedPtr<FJsonObject>* Found = nullptr;
    return Payload.IsValid() && Payload->TryGetObjectField(FieldName, Found) && Found ? *Found : TSharedPtr<FJsonObject>();
}

ENGINEWIRE_API FString JsonValueToString(const TSharedPtr<FJsonValue>& Value);

/**
 * When the payload lists names in ListField, keeps only the Rows whose "name"
 * is one of them (case-insensitive) and writes the listed names no row matched
 * to Result's MissingField. Checking one component of a 27-component Blueprint
 * used to return all 27. Without ListField, Rows are left untouched.
 */
ENGINEWIRE_API void FilterRowsByListedNames(
    const TSharedPtr<FJsonObject>& Payload, const FString& ListField,
    TArray<TSharedPtr<FJsonValue>>& Rows, const TSharedPtr<FJsonObject>& Result, const FString& MissingField);
}

// The first `limit` rows of a find (1-1000, default 200), with truncated and totalCount when the list was cut: a class
// or a tag most of a level shares answered every actor, hundreds of rows, while a mesh or material find stopped at 200.
// Returns how many matched in all.
inline int32 McpCapFindRows(const TSharedPtr<FJsonObject>& Payload, TArray<TSharedPtr<FJsonValue>>& Rows,
                            const TSharedPtr<FJsonObject>& Data)
{
    double Requested = 0.0;
    const int32 Limit = Payload->TryGetNumberField(TEXT("limit"), Requested)
                            ? FMath::Clamp(static_cast<int32>(Requested), 1, 1000) : 200;
    const int32 Total = Rows.Num();
    if (Total > Limit)
    {
        Rows.SetNum(Limit);
        Data->SetBoolField(TEXT("truncated"), true);
        Data->SetNumberField(TEXT("totalCount"), Total);
    }
    return Total;
}
