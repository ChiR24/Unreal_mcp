#pragma once

#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/Reflection/McpPropertyReflection.h"
#include "Foundation/Render/McpConsoleVariableWrite.h"

#include "HAL/IConsoleManager.h"
#include "UObject/UnrealType.h"

class APostProcessVolume;

namespace McpRenderHandlers
{

inline void AddStringArray(
    const TSharedPtr<FJsonObject>& Result,
    const FString& FieldName,
    const TArray<FString>& Values)
{
    Result->SetArrayField(FieldName, McpHandlerUtils::ToJsonStringArray(Values));
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
    // The engine's hard limits (ClampMin/ClampMax), which its Details panel holds every value to, checked before
    // anything is written: a motion blur amount of 50 or an f-stop of 0.5 was written and answered applied.
    for (const auto& Pair : Settings->Values)
    {
        FProperty* Property = StructType->FindPropertyByName(FName(*Pair.Key));
        double Number = 0.0;
        if (Property && Property->IsA<FNumericProperty>() && Pair.Value.IsValid() && Pair.Value->Type == EJson::Number &&
            Pair.Value->TryGetNumber(Number) && !McpPropertyReflection::CheckRange(Property, Number, FString(*Pair.Key), OutError))
        {
            return false;
        }
    }
    const int32 AppliedBefore = OutApplied.Num();
    TArray<FString> Unknown;
    for (const auto& Pair : Settings->Values)
    {
        const FString Name(*Pair.Key);
        FProperty* Property = StructType->FindPropertyByName(FName(*Name));
        if (!Property)
        {
            OutUnsupported.Add(Name);
            Unknown.Add(Name);
            continue;
        }
        if (!ApplyJsonValueToProperty(Target, Property, Pair.Value, OutError))
        {
            OutError = FString::Printf(TEXT("%s: %s"), *Name, *OutError);
            return false;
        }
        if (bSetOverrides)
        {
            FBoolProperty* Override = CastField<FBoolProperty>(
                StructType->FindPropertyByName(FName(*(TEXT("bOverride_") + Name))));
            // bMobileHQGaussian's flag is bOverride_MobileHQGaussian: a b-prefixed field kept its override off, so the
            // value it was given never applied.
            if (!Override && Name.Len() > 1 && Name[0] == TEXT('b') && FChar::IsUpper(Name[1]))
            {
                Override = CastField<FBoolProperty>(StructType->FindPropertyByName(FName(*(TEXT("bOverride_") + Name.Mid(1)))));
            }
            if (Override)
            {
                Override->SetPropertyValue_InContainer(Target, true);
            }
        }
        OutApplied.Add(Name);
    }
    // Every key unknown used to answer success with them all under unsupportedSettings, so
    // {"saturation": 1.1} read as applied. Refuse, and name the fields the keys come close to.
    if (Unknown.Num() > 0 && OutApplied.Num() == AppliedBefore)
    {
        // bOverride_ flags are set with their field, so they are no suggestion: they crowded
        // ColorSaturation out of the eight names for {"saturation": 1.1}.
        TArray<FString> Near;
        for (TFieldIterator<FProperty> It(StructType); It && Near.Num() < 8; ++It)
        {
            if (!It->GetName().StartsWith(TEXT("bOverride_")) &&
                Unknown.ContainsByPredicate([&It](const FString& Key) { return It->GetName().Contains(Key); }))
            {
                Near.Add(It->GetName());
            }
        }
        OutError = FString::Printf(
            TEXT("No settings key is a %s field: %s. Keys are the engine field names%s."),
            *StructType->GetName(), *FString::Join(Unknown, TEXT(", ")),
            *(Near.Num() > 0 ? TEXT(", e.g. ") + FString::Join(Near, TEXT(", ")) : FString()));
        return false;
    }
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
    // actorName, else set_actor_light_channel's targetActor: the two names a render record declares for an actor.
    OutReference = McpGetFirstStringField(Payload, {TEXT("actorName"), TEXT("targetActor")});
    return !OutReference.IsEmpty();
}
}
