#include "Domains/Environment/McpAutomationBridge_EnvironmentHandlersShared.h"

namespace McpEnvironmentHandlers {

bool McpCreateTimeOfDaySystem(const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FJsonObject> Resp,
                                    FString &OutMessage, FString &OutErrorCode)
{
    AActor *Actor = McpFindOrSpawnEnvironmentActor(Payload, AActor::StaticClass(), TEXT("TimeOfDaySystem"));
    if (!Actor)
    {
        OutMessage = TEXT("Failed to create time-of-day system actor");
        OutErrorCode = TEXT("SPAWN_FAILED");
        return false;
    }

    UDirectionalLightComponent *SunComponent = Cast<UDirectionalLightComponent>(
        McpFindOrAddComponent(Actor, UDirectionalLightComponent::StaticClass(), TEXT("TimeOfDaySun")));
    USkyLightComponent *SkyLightComponent = Cast<USkyLightComponent>(
        McpFindOrAddComponent(Actor, USkyLightComponent::StaticClass(), TEXT("TimeOfDaySkyLight")));
    USkyAtmosphereComponent *SkyAtmosphereComponent = Cast<USkyAtmosphereComponent>(
        McpFindOrAddComponent(Actor, USkyAtmosphereComponent::StaticClass(), TEXT("TimeOfDaySkyAtmosphere")));
    if (!SunComponent || !SkyLightComponent || !SkyAtmosphereComponent)
    {
        OutMessage = TEXT("Failed to create time-of-day lighting components");
        OutErrorCode = TEXT("COMPONENT_CREATION_FAILED");
        return false;
    }

    // create_time_of_day_system declares only name/path/location: the rig starts at noon;
    // set_time_of_day moves the sun afterwards.
    constexpr double NoonElevation = 90.0;
    constexpr float SunIntensity = 10.0f;
    constexpr float SkyIntensity = 1.0f;

    Actor->Modify();
    Actor->SetActorRotation(McpSunRotation(NoonElevation, 0.0));
    SunComponent->Modify();
    SunComponent->SetMobility(EComponentMobility::Movable);
    SunComponent->SetRelativeRotation(McpSunRotation(NoonElevation, 0.0));
    SunComponent->SetIntensity(SunIntensity);
    SunComponent->SetAtmosphereSunLight(true);
    SunComponent->SetAtmosphereSunLightIndex(0);
    SunComponent->MarkRenderStateDirty();
    SkyLightComponent->Modify();
    SkyLightComponent->SetMobility(EComponentMobility::Movable);
    SkyLightComponent->SetIntensity(SkyIntensity);
    SkyLightComponent->MarkRenderStateDirty();
    SkyAtmosphereComponent->Modify();
    SkyAtmosphereComponent->SetMobility(EComponentMobility::Movable);
    SkyAtmosphereComponent->MarkRenderStateDirty();
    Actor->MarkPackageDirty();

    Resp->SetStringField(TEXT("actorName"), McpActorRef(Actor));
    Resp->SetStringField(TEXT("actorPath"), Actor->GetPathName());
    Resp->SetStringField(TEXT("sunComponentName"), SunComponent->GetName());
    Resp->SetStringField(TEXT("skyLightComponentName"), SkyLightComponent->GetName());
    Resp->SetStringField(TEXT("skyAtmosphereComponentName"), SkyAtmosphereComponent->GetName());
    Resp->SetNumberField(TEXT("currentHour"), 12.0);
    Resp->SetNumberField(TEXT("sunIntensity"), SunIntensity);
    Resp->SetNumberField(TEXT("skyLightIntensity"), SkyIntensity);
    McpHandlerUtils::AddVerification(Resp, Actor);
    OutMessage = TEXT("Time-of-day lighting rig created");
    return true;
}
bool McpConfigureWaterWavesOnActor(AActor *WaterActor, const TSharedPtr<FJsonObject> &Payload,
                                          TSharedPtr<FJsonObject> Resp, FString &OutMessage, FString &OutErrorCode)
{
    if (!WaterActor)
    {
        OutMessage = TEXT("Water body actor not found");
        OutErrorCode = TEXT("WATER_BODY_NOT_FOUND");
        return false;
    }

    UClass *GerstnerWavesClass = LoadClass<UObject>(nullptr, TEXT("/Script/Water.GerstnerWaterWaves"));
    UClass *GerstnerGeneratorClass = LoadClass<UObject>(nullptr, TEXT("/Script/Water.GerstnerWaterWaveGeneratorSimple"));
    if (!GerstnerWavesClass || !GerstnerGeneratorClass)
    {
        OutMessage = TEXT("Water plugin Gerstner wave classes are unavailable");
        OutErrorCode = TEXT("CLASS_NOT_FOUND");
        return false;
    }

    UObject *WaterWaves = McpGetObjectPropertyValue(WaterActor, TEXT("WaterWaves"));
    if (!WaterWaves)
    {
        WaterWaves = McpInvokeObjectGetter(WaterActor, FName(TEXT("GetWaterWaves")));
    }
    if (!WaterWaves || !WaterWaves->IsA(GerstnerWavesClass))
    {
        WaterWaves = NewObject<UObject>(WaterActor, GerstnerWavesClass,
            MakeUniqueObjectName(WaterActor, GerstnerWavesClass, TEXT("McpGerstnerWaterWaves")), RF_Transactional);
        if (!WaterWaves)
        {
            OutMessage = TEXT("Failed to create Gerstner water waves");
            OutErrorCode = TEXT("CREATION_FAILED");
            return false;
        }

        if (!McpAssignWaterWaves(WaterActor, WaterWaves))
        {
            OutMessage = TEXT("Failed to assign Gerstner water waves to water body");
            OutErrorCode = TEXT("PROPERTY_SET_FAILED");
            return false;
        }
    }

    UObject *Generator = McpGetObjectPropertyValue(WaterWaves, TEXT("GerstnerWaveGenerator"));
    if (!Generator || !Generator->IsA(GerstnerGeneratorClass))
    {
        Generator = NewObject<UObject>(WaterWaves, GerstnerGeneratorClass,
            MakeUniqueObjectName(WaterWaves, GerstnerGeneratorClass, TEXT("McpGerstnerWaterWaveGenerator")), RF_Transactional);
        if (!Generator || !McpSetObjectPropertyValue(WaterWaves, TEXT("GerstnerWaveGenerator"), Generator))
        {
            OutMessage = TEXT("Failed to create Gerstner wave generator");
            OutErrorCode = TEXT("CREATION_FAILED");
            return false;
        }
    }

    TArray<FString> Applied;
    // The value given is the largest wave's; the generator's other waves shrink toward a share of it, as a
    // real sea's do. One size for all 16 lined their crests up and folded the surface into black creases.
    auto ApplyRange = [&](const TCHAR *Key, const TCHAR *SmallEnd, const TCHAR *LargeEnd, double Value, double SmallShare)
    {
        McpApplyNumberProperty(Generator, SmallEnd, Value * SmallShare, Key, Resp, Applied);
        McpApplyNumberProperty(Generator, LargeEnd, Value, Key, Resp, Applied);
    };
    double NumberValue = 0.0;
    for (const TCHAR *Key : {TEXT("waveHeight"), TEXT("amplitude")})
    {
        if (McpGetFirstNumberField(Payload, {Key}, NumberValue))
        {
            ApplyRange(Key, TEXT("MinAmplitude"), TEXT("MaxAmplitude"), FMath::Max(NumberValue, 0.0001), 0.15);
            break;
        }
    }
    if (McpGetFirstNumberField(Payload, {TEXT("waveLength")}, NumberValue))
    {
        ApplyRange(TEXT("waveLength"), TEXT("MinWavelength"), TEXT("MaxWavelength"), FMath::Max(NumberValue, 0.0001), 0.13);
    }
    // The long waves are the gentle ones: steepness is the short waves', the long ones take about half.
    if (McpGetFirstNumberField(Payload, {TEXT("steepness")}, NumberValue))
    {
        ApplyRange(TEXT("steepness"), TEXT("LargeWaveSteepness"), TEXT("SmallWaveSteepness"), FMath::Clamp(NumberValue, 0.0, 1.0), 0.55);
    }

    const TSharedPtr<FJsonObject> *DirectionObj = nullptr;
    if (Payload.IsValid() && Payload->TryGetObjectField(TEXT("direction"), DirectionObj) && DirectionObj && DirectionObj->IsValid())
    {
        const FRotator Direction = ExtractRotatorField(Payload, TEXT("direction"), FRotator::ZeroRotator);
        McpApplyNumberProperty(Generator, TEXT("WindAngleDeg"), Direction.Yaw, TEXT("directionYaw"), Resp, Applied);
    }

    if (Applied.Num() == 0)
    {
        OutMessage = TEXT("No supported water wave properties were applied");
        OutErrorCode = TEXT("PROPERTY_NOT_FOUND");
        Resp->SetBoolField(TEXT("waterWaveConfigured"), false);
        return false;
    }

    Generator->Modify();
    Generator->MarkPackageDirty();
    WaterWaves->Modify();
    WaterWaves->MarkPackageDirty();
    WaterWaves->PostEditChange();
    WaterActor->MarkPackageDirty();

    Resp->SetBoolField(TEXT("waterWaveConfigured"), true);
    Resp->SetStringField(TEXT("waterWaveClass"), WaterWaves->GetClass()->GetPathName());
    Resp->SetStringField(TEXT("waveGeneratorClass"), Generator->GetClass()->GetPathName());
    Resp->SetNumberField(TEXT("waterWaveConfiguredPropertyCount"), Applied.Num());
    McpAddStringArrayField(Resp, TEXT("waterWaveConfiguredProperties"), Applied);
    OutMessage = TEXT("Water waves configured");
    return true;
}
bool McpCreateBuoyancyComponent(const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FJsonObject> Resp,
                                       FString &OutMessage, FString &OutErrorCode)
{
    AActor *TargetActor = McpFindActorFromEnvironmentPayload(Payload);
    if (!TargetActor)
    {
        OutMessage = TEXT("actorPath, targetActor, actorName, or name required for create_buoyancy_component");
        OutErrorCode = TEXT("ACTOR_NOT_FOUND");
        return false;
    }
    UClass *BuoyancyClass = LoadClass<UActorComponent>(nullptr, TEXT("/Script/Water.BuoyancyComponent"));
    if (!BuoyancyClass)
    {
        OutMessage = TEXT("Water plugin buoyancy component class is unavailable");
        OutErrorCode = TEXT("CLASS_NOT_FOUND");
        return false;
    }
    UActorComponent *Component = McpFindOrAddComponent(TargetActor, BuoyancyClass, TEXT("BuoyancyComponent"));
    if (!Component)
    {
        OutMessage = TEXT("Failed to create buoyancy component");
        OutErrorCode = TEXT("COMPONENT_CREATION_FAILED");
        return false;
    }

    McpApplyEnvironmentSettings(Component, Payload, Resp);
    McpEnableWaterOverlaps(TargetActor);
    Resp->SetStringField(TEXT("actorName"), McpActorRef(TargetActor));
    Resp->SetStringField(TEXT("componentName"), Component->GetName());
    Resp->SetStringField(TEXT("componentPath"), Component->GetPathName());
    McpHandlerUtils::AddVerification(Resp, TargetActor);
    OutMessage = TEXT("Buoyancy component created");
    return true;
}

} // namespace McpEnvironmentHandlers
