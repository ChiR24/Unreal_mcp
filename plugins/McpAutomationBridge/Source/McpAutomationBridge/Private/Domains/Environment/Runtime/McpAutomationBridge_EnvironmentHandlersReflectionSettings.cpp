#include "Domains/Environment/McpAutomationBridge_EnvironmentHandlersShared.h"
#include "Engine/TextureCube.h"

namespace McpEnvironmentHandlers {

FProperty *McpFindPropertyCaseInsensitive(UObject *Object, const FString &PropertyName)
{
    if (!Object || PropertyName.IsEmpty())
    {
        return nullptr;
    }

    // FName lookup ignores case; a bool member may also be named without its "b" prefix.
    UClass *Class = Object->GetClass();
    FProperty *Property = Class->FindPropertyByName(FName(*PropertyName));
    return Property ? Property : Class->FindPropertyByName(FName(*(TEXT("b") + PropertyName)));
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
bool McpApplyNumberProperty(UObject *Target, const TCHAR *PropertyName, double Value,
                                   const FString &ResponseName, TSharedPtr<FJsonObject> Resp, TArray<FString> &Applied)
{
    FProperty *Property = McpFindPropertyCaseInsensitive(Target, PropertyName);
    FString ApplyError;
    if (!Property || !McpPropertyReflection::ApplyJsonValueToProperty(Target, Property, MakeShared<FJsonValueNumber>(Value), ApplyError))
    {
        return false;
    }
    Applied.Add(Property->GetName());
    Resp->SetNumberField(ResponseName, Value);
    return true;
}
int32 McpApplyPayloadSettings(UObject *Target, const TSharedPtr<FJsonObject> &Payload,
                                     TArray<FString> &AppliedProperties, TArray<FString> &FailedProperties)
{
    if (!Target || !Payload.IsValid())
    {
        return 0;
    }

    // The canonical payload key skyLightIntensity does not match the
    // USkyLightComponent property name "Intensity" (McpApplyPayloadSettings
    // below does a case-insensitive exact-name match only), so translate it
    // explicitly before the generic pass. Mirrors spawn_sky_light's top-level
    // intensity handling (LightingHandlersSky.cpp).
    // Live-discovered: configure_sky_light passes the ASkyLight actor
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
        // cubemapPath names no property (it is Cubemap, and it needs SourceType set too), so it
        // was never applied. Handled on the component pass only, not again for its actor.
        FString CubemapPath;
        if (SkyComp == Target && Payload->TryGetStringField(TEXT("cubemapPath"), CubemapPath) && !CubemapPath.IsEmpty())
        {
            if (UTextureCube *Cubemap = LoadObject<UTextureCube>(nullptr, *CubemapPath))
            {
                SkyComp->Modify();
                SkyComp->SourceType = ESkyLightSourceType::SLS_SpecifiedCubemap;
                SkyComp->SetCubemap(Cubemap);
                AppliedProperties.Add(TEXT("Cubemap"));
                ++SkyApplied;
            }
            else
            {
                FailedProperties.Add(FString::Printf(TEXT("cubemapPath: no cube texture at %s"), *CubemapPath));
            }
        }
    }

    // "direction" and "rotation" sit in IgnoredFields below so the reflection
    // pass never tries to write them as a UPROPERTY -- but nothing else picked
    // them up either, so configure_weather wind accepted a direction and
    // silently dropped it while reporting only ["Speed"] as applied. An
    // environment actor's direction IS its rotation; apply it here and count it.
    int32 RotationApplied = 0;
    if (AActor *RotatableActor = Cast<AActor>(Target))
    {
        const TSharedPtr<FJsonObject> *RotationObject = nullptr;
        const TCHAR *RotationKey = nullptr;
        if (Payload->TryGetObjectField(TEXT("direction"), RotationObject))
        {
            RotationKey = TEXT("direction");
        }
        else if (Payload->TryGetObjectField(TEXT("rotation"), RotationObject))
        {
            RotationKey = TEXT("rotation");
        }
        if (RotationObject && RotationObject->IsValid())
        {
            const FRotator NewRotation =
                ExtractRotatorField(Payload, RotationKey, RotatableActor->GetActorRotation());
            RotatableActor->Modify();
            RotatableActor->SetActorRotation(NewRotation);
            RotatableActor->MarkPackageDirty();
            AppliedProperties.Add(TEXT("Rotation"));
            RotationApplied = 1;
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

    int32 TotalApplied = SkyApplied + RotationApplied + ApplyObject(Payload);

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
