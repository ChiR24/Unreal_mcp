#include "Domains/Environment/McpAutomationBridge_EnvironmentHandlersShared.h"

#if WITH_EDITOR
namespace McpEnvironmentHandlers {

FProperty *McpFindPropertyCaseInsensitive(UObject *Object, const FString &PropertyName)
{
    if (!Object || PropertyName.IsEmpty())
    {
        return nullptr;
    }

    if (FProperty *Exact = Object->GetClass()->FindPropertyByName(FName(*PropertyName)))
    {
        return Exact;
    }

    for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
    {
        FProperty *Property = *It;
        if (!Property)
        {
            continue;
        }

        const FString Candidate = Property->GetName();
        if (Candidate.Equals(PropertyName, ESearchCase::IgnoreCase) ||
            (Candidate.StartsWith(TEXT("b")) && Candidate.Mid(1).Equals(PropertyName, ESearchCase::IgnoreCase)))
        {
            return Property;
        }
    }
    return nullptr;
}
UObject *McpGetObjectPropertyValue(UObject *Object, const FString &PropertyName)
{
    FProperty *Property = McpFindPropertyCaseInsensitive(Object, PropertyName);
    if (FObjectProperty *ObjectProperty = CastField<FObjectProperty>(Property))
    {
        return ObjectProperty->GetObjectPropertyValue_InContainer(Object);
    }
    return nullptr;
}
bool McpSetObjectPropertyValue(UObject *Object, const FString &PropertyName, UObject *Value)
{
    FProperty *Property = McpFindPropertyCaseInsensitive(Object, PropertyName);
    if (FObjectProperty *ObjectProperty = CastField<FObjectProperty>(Property))
    {
        Object->Modify();
        ObjectProperty->SetObjectPropertyValue_InContainer(Object, Value);
        Object->MarkPackageDirty();
        return true;
    }
    return false;
}
UObject *McpInvokeObjectGetter(UObject *Object, const FName &FunctionName)
{
    if (!Object)
    {
        return nullptr;
    }

    UFunction *Function = Object->FindFunction(FunctionName);
    if (!Function)
    {
        return nullptr;
    }

    struct FObjectGetterParams
    {
        UObject *ReturnValue = nullptr;
    };

    FObjectGetterParams Params;
    Object->ProcessEvent(Function, &Params);
    return Params.ReturnValue;
}
bool McpInvokeObjectSetter(UObject *Object, const FName &FunctionName, UObject *Value)
{
    if (!Object)
    {
        return false;
    }

    UFunction *Function = Object->FindFunction(FunctionName);
    if (!Function)
    {
        return false;
    }

    struct FObjectSetterParams
    {
        UObject *Value = nullptr;
    };

    FObjectSetterParams Params;
    Params.Value = Value;
    Object->ProcessEvent(Function, &Params);
    return true;
}
bool McpApplyNumberProperty(UObject *Target, std::initializer_list<const TCHAR *> PropertyNames, double Value,
                                   const FString &ResponseName, TSharedPtr<FJsonObject> Resp, TArray<FString> &Applied)
{
    if (!Target)
    {
        return false;
    }

    for (const TCHAR *PropertyName : PropertyNames)
    {
        if (FProperty *Property = McpFindPropertyCaseInsensitive(Target, PropertyName))
        {
            FString ApplyError;
            if (McpPropertyReflection::ApplyJsonValueToProperty(Target, Property, MakeShared<FJsonValueNumber>(Value), ApplyError))
            {
                Applied.Add(Property->GetName());
                Resp->SetNumberField(ResponseName, Value);
                return true;
            }
        }
    }
    return false;
}
int32 McpApplyPayloadSettings(UObject *Target, const TSharedPtr<FJsonObject> &Payload,
                                     TArray<FString> &AppliedProperties, TArray<FString> &FailedProperties)
{
    if (!Target || !Payload.IsValid())
    {
        return 0;
    }

    // BB-055: the canonical payload key skyLightIntensity does not match the
    // USkyLightComponent property name "Intensity" (McpApplyPayloadSettings
    // below does a case-insensitive exact-name match only), so translate it
    // explicitly before the generic pass. Mirrors spawn_sky_light's top-level
    // intensity handling (LightingHandlersSky.cpp).
    // Live-discovered (Todo 39): configure_sky_light passes the ASkyLight actor
    // (not the component) to McpApplyEnvironmentSettings, so resolve the
    // component from the actor when the direct Cast fails.
    USkyLightComponent *SkyComp = Cast<USkyLightComponent>(Target);
    if (!SkyComp)
    {
        if (ASkyLight *SkyActor = Cast<ASkyLight>(Target))
        {
            SkyComp = Cast<USkyLightComponent>(SkyActor->GetLightComponent());
        }
    }
    int32 SkyApplied = 0;
    if (SkyComp)
    {
        double SkyIntensity = 0.0;
        if (Payload->TryGetNumberField(TEXT("skyLightIntensity"), SkyIntensity))
        {
            SkyComp->Modify();
            SkyComp->SetIntensity(static_cast<float>(SkyIntensity));
            SkyComp->MarkPackageDirty();
            AppliedProperties.Add(TEXT("Intensity"));
            SkyApplied = 1;
        }
    }

    auto ApplyObject = [&](const TSharedPtr<FJsonObject> &ObjectToApply) -> int32
    {
        int32 AppliedCount = 0;
        if (!ObjectToApply.IsValid())
        {
            return AppliedCount;
        }

        static const TSet<FString> IgnoredFields = {
            TEXT("action"), TEXT("name"), TEXT("actorName"), TEXT("targetActor"), TEXT("waterBodyName"),
            TEXT("path"), TEXT("outputPath"), TEXT("heightmapPath"), TEXT("landscapePath"), TEXT("foliageType"),
            TEXT("foliageTypePath"), TEXT("meshPath"), TEXT("staticMesh"), TEXT("materialPath"), TEXT("particleSystemPath"),
            TEXT("curvePath"), TEXT("settings"), TEXT("location"), TEXT("rotation"), TEXT("direction"), TEXT("points")
        };

        for (const auto &Pair : ObjectToApply->Values)
        {
            const FString FieldName(Pair.Key.Len(), *Pair.Key);
            if (IgnoredFields.Contains(FieldName) || !Pair.Value.IsValid())
            {
                continue;
            }

            FProperty *Property = McpFindPropertyCaseInsensitive(Target, FieldName);
            if (!Property)
            {
                continue;
            }

            FString ApplyError;
            if (McpPropertyReflection::ApplyJsonValueToProperty(Target, Property, Pair.Value, ApplyError))
            {
                AppliedProperties.Add(Property->GetName());
                ++AppliedCount;
            }
            else
            {
                FailedProperties.Add(FString::Printf(TEXT("%s: %s"), *FieldName, *ApplyError));
            }
        }
        return AppliedCount;
    };

    int32 TotalApplied = SkyApplied + ApplyObject(Payload);

    const TSharedPtr<FJsonObject> *SettingsObj = nullptr;
    if (Payload->TryGetObjectField(TEXT("settings"), SettingsObj) && SettingsObj && SettingsObj->IsValid())
    {
        TotalApplied += ApplyObject(*SettingsObj);
    }

    if (TotalApplied > 0)
    {
        Target->Modify();
        Target->MarkPackageDirty();
        Target->PostEditChange();
    }

    return TotalApplied;
}

} // namespace McpEnvironmentHandlers
#endif
