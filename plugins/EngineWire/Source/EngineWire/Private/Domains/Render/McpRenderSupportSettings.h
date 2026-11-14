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

// Field of Settings as a number from MinValue to MaxValue, a whole one when bRequireWholeNumber. Not sent: true,
// InOutValue kept and bOutPresent false. Sent and not such a number: false, OutError naming the field and its bounds.
// Defined in McpRenderConsole.cpp beside the console actions' table, where the security contract test reads it.
bool ReadBoundedNumberSetting(
    const TSharedPtr<FJsonObject>& Settings,
    const TCHAR* Field,
    double MinValue,
    double MaxValue,
    bool bRequireWholeNumber,
    double& InOutValue,
    bool& bOutPresent,
    FString& OutError);

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
