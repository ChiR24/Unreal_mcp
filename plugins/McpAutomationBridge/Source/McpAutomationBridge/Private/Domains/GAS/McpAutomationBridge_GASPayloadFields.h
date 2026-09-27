#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersJsonFields.h"

namespace McpGASHandlers
{
static inline FString NormalizeGASToken(FString Value)
{
    Value.TrimStartAndEndInline();
    FString Normalized = Value.ToLower();
    Normalized.ReplaceInline(TEXT("_"), TEXT(""));
    Normalized.ReplaceInline(TEXT("-"), TEXT(""));
    Normalized.ReplaceInline(TEXT(" "), TEXT(""));
    return Normalized;
}

static inline FString GetGASStringFieldWithFallback(
    const TSharedPtr<FJsonObject>& Payload,
    const TCHAR* PrimaryField,
    const TCHAR* FallbackField,
    const FString& DefaultValue = FString())
{
    FString Value = GetJsonStringField(Payload, PrimaryField);
    if (!Value.IsEmpty())
    {
        return Value;
    }

    Value = GetJsonStringField(Payload, FallbackField);
    return Value.IsEmpty() ? DefaultValue : Value;
}

static inline double GetGASNumberFieldWithFallback(
    const TSharedPtr<FJsonObject>& Payload,
    const TCHAR* PrimaryField,
    const TCHAR* FallbackField,
    double DefaultValue = 0.0)
{
    double Value = 0.0;
    if (Payload.IsValid() && Payload->TryGetNumberField(PrimaryField, Value))
    {
        return Value;
    }
    if (Payload.IsValid() && Payload->TryGetNumberField(FallbackField, Value))
    {
        return Value;
    }
    return DefaultValue;
}

// A token ("has_duration", "HasDuration", "local predicted") matched against the enumerators' own short
// names under the same normalization; false (Out untouched) when none matches. The _MAX entry never does.
template <typename TEnum>
bool TryParseGASEnum(const FString& Value, TEnum& Out)
{
    const UEnum* Enum = StaticEnum<TEnum>();
    const FString Token = NormalizeGASToken(Value);
    for (int32 Index = 0; Enum && Index < Enum->NumEnums() - 1; ++Index)
    {
        if (NormalizeGASToken(Enum->GetNameStringByIndex(Index)) == Token)
        {
            Out = static_cast<TEnum>(Enum->GetValueByIndex(Index));
            return true;
        }
    }
    return false;
}

// The enumerator's short name ("HasDuration").
template <typename TEnum>
FString GASEnumName(TEnum Value)
{
    const UEnum* Enum = StaticEnum<TEnum>();
    return Enum ? Enum->GetNameStringByValue(static_cast<int64>(Value)) : FString();
}
}
