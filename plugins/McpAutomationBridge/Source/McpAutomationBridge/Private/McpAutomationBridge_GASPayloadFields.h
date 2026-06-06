#pragma once

#include "McpAutomationBridge_GASAvailability.h"

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeHelpersJsonFields.h"

#if WITH_EDITOR && MCP_HAS_GAS
namespace McpGASHandlers
{
static inline FString GetStringFieldGAS(
    const TSharedPtr<FJsonObject>& Payload,
    const FString& Field,
    const FString& DefaultValue = TEXT(""))
{
    return GetJsonStringField(Payload, Field, DefaultValue);
}

static inline double GetNumberFieldGAS(
    const TSharedPtr<FJsonObject>& Payload,
    const FString& Field,
    double DefaultValue = 0.0)
{
    return GetJsonNumberField(Payload, Field, DefaultValue);
}

static inline bool GetBoolFieldGAS(
    const TSharedPtr<FJsonObject>& Payload,
    const FString& Field,
    bool DefaultValue = false)
{
    return GetJsonBoolField(Payload, Field, DefaultValue);
}

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
    FString Value = GetStringFieldGAS(Payload, PrimaryField);
    if (!Value.IsEmpty())
    {
        return Value;
    }

    Value = GetStringFieldGAS(Payload, FallbackField);
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
}
#endif
