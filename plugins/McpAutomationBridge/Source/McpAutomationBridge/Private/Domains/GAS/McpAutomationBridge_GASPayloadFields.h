#pragma once

#include "Domains/GAS/McpAutomationBridge_GASAvailability.h"

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersJsonFields.h"

#if WITH_EDITOR && MCP_HAS_GAS
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
}
#endif
