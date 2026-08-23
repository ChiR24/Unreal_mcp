#pragma once

#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#if WITH_EDITOR
#include "HAL/IConsoleManager.h"
#include "UObject/UnrealType.h"
#endif

class APostProcessVolume;

namespace McpRenderHandlers
{
#if WITH_EDITOR

// BB-021: bounded post-process volume settings summary exposed for
// inspect/component reads (implemented in
// Foundation/Render/McpPostProcessVolumeResolution.cpp).
TSharedPtr<FJsonObject> McpDescribePostProcessVolume(const APostProcessVolume& Volume);

inline void AddStringArray(
    const TSharedPtr<FJsonObject>& Result,
    const FString& FieldName,
    const TArray<FString>& Values)
{
    TArray<TSharedPtr<FJsonValue>> JsonValues;
    for (const FString& Value : Values)
    {
        JsonValues.Add(MakeShared<FJsonValueString>(Value));
    }
    Result->SetArrayField(FieldName, JsonValues);
}

inline void AddNumberArray(
    const TSharedPtr<FJsonObject>& Result,
    const FString& FieldName,
    const TArray<int32>& Values)
{
    TArray<TSharedPtr<FJsonValue>> JsonValues;
    for (const int32 Value : Values)
    {
        JsonValues.Add(MakeShared<FJsonValueNumber>(Value));
    }
    Result->SetArrayField(FieldName, JsonValues);
}

inline bool ApplyJsonSettings(
    void* Target,
    UStruct* StructType,
    const TSharedPtr<FJsonObject>& Settings,
    bool bSetOverrides,
    TArray<FString>& OutApplied,
    TArray<FString>& OutUnsupported,
    FString& OutError)
{
    if (!Target || !StructType || !Settings.IsValid())
    {
        return true;
    }
    for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Settings->Values)
    {
        FProperty* Property = StructType->FindPropertyByName(FName(*Pair.Key));
        if (!Property)
        {
            OutUnsupported.Add(Pair.Key);
            continue;
        }
        if (!ApplyJsonValueToProperty(Target, Property, Pair.Value, OutError))
        {
            OutError = FString::Printf(TEXT("%s: %s"), *Pair.Key, *OutError);
            return false;
        }
        if (bSetOverrides)
        {
            FBoolProperty* Override = CastField<FBoolProperty>(
                StructType->FindPropertyByName(FName(*(TEXT("bOverride_") + Pair.Key))));
            if (Override)
            {
                Override->SetPropertyValue_InContainer(Target, true);
            }
        }
        OutApplied.Add(Pair.Key);
    }
    return true;
}

inline bool SetConsoleVariable(
    const FString& Name,
    const FString& Value,
    TArray<FString>& OutApplied,
    TArray<FString>& OutUnsupported)
{
    IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(*Name);
    if (!Variable)
    {
        OutUnsupported.Add(Name);
        return false;
    }
    Variable->Set(*Value, ECVF_SetByCode);
    OutApplied.Add(Name);
    return true;
}

inline bool ReadBoundedIntField(
    const TSharedPtr<FJsonObject>& Payload,
    const TCHAR* FieldName,
    int32 DefaultValue,
    int32 MinValue,
    int32 MaxValue,
    int32& OutValue,
    FString& OutError)
{
    double RawValue = static_cast<double>(DefaultValue);
    if (Payload.IsValid() &&
        Payload->HasField(FieldName) &&
        !Payload->TryGetNumberField(FieldName, RawValue))
    {
        OutError = FString::Printf(TEXT("%s must be a number"), FieldName);
        return false;
    }

    if (!FMath::IsFinite(RawValue) ||
        RawValue < static_cast<double>(MinValue) ||
        RawValue > static_cast<double>(MaxValue))
    {
        OutError = FString::Printf(TEXT("%s must be an integer between %d and %d"), FieldName, MinValue, MaxValue);
        return false;
    }

    const int32 RoundedValue = FMath::RoundToInt(RawValue);
    if (!FMath::IsNearlyEqual(RawValue, static_cast<double>(RoundedValue)))
    {
        OutError = FString::Printf(TEXT("%s must be an integer between %d and %d"), FieldName, MinValue, MaxValue);
        return false;
    }

    OutValue = RoundedValue;
    return true;
}

inline bool ReadBoundedNumberField(
    const TSharedPtr<FJsonObject>& Payload,
    const TCHAR* FieldName,
    double DefaultValue,
    double MinValue,
    double MaxValue,
    double& OutValue,
    FString& OutError)
{
    OutValue = DefaultValue;
    if (Payload.IsValid() &&
        Payload->HasField(FieldName) &&
        !Payload->TryGetNumberField(FieldName, OutValue))
    {
        OutError = FString::Printf(TEXT("%s must be a number"), FieldName);
        return false;
    }
    if (!FMath::IsFinite(OutValue) || OutValue < MinValue || OutValue > MaxValue)
    {
        OutError = FString::Printf(TEXT("%s must be between %.0f and %.0f"), FieldName, MinValue, MaxValue);
        return false;
    }
    return true;
}

inline bool ValidateBoundedNumberSetting(
    const TSharedPtr<FJsonObject>& Settings,
    const TCHAR* FieldName,
    double MinValue,
    double MaxValue,
    FString& OutError)
{
    if (!Settings.IsValid() || !Settings->HasField(FieldName))
    {
        return true;
    }

    double Value = 0.0;
    if (!Settings->TryGetNumberField(FieldName, Value) ||
        !FMath::IsFinite(Value) ||
        Value < MinValue ||
        Value > MaxValue)
    {
        OutError = FString::Printf(TEXT("%s must be between %.0f and %.0f"), FieldName, MinValue, MaxValue);
        return false;
    }
    return true;
}

inline TSharedPtr<FJsonObject> GetSettingsObject(const TSharedPtr<FJsonObject>& Payload)
{
    const TSharedPtr<FJsonObject>* Settings = nullptr;
    return Payload.IsValid() &&
            Payload->TryGetObjectField(TEXT("settings"), Settings) &&
            Settings
        ? *Settings
        : nullptr;
}

inline bool ReadActorReference(
    const TSharedPtr<FJsonObject>& Payload,
    FString& OutReference)
{
    OutReference = GetJsonStringField(Payload, TEXT("actorName"));
    if (OutReference.IsEmpty())
    {
        OutReference = GetJsonStringField(Payload, TEXT("targetActor"));
    }
    if (OutReference.IsEmpty())
    {
        OutReference = GetJsonStringField(Payload, TEXT("actorPath"));
    }
    return !OutReference.IsEmpty();
}
#endif
}
