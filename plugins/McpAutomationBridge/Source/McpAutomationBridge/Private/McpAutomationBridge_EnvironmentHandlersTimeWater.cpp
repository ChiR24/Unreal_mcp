#include "McpAutomationBridge_EnvironmentHandlersShared.h"

#if WITH_EDITOR
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

    double Hour = 12.0;
    McpTryGetNumberFromPayloadOrSettings(Payload, TEXT("CurrentHour"), Hour);
    McpTryGetNumberFromPayloadOrSettings(Payload, TEXT("hour"), Hour);
    McpTryGetNumberFromPayloadOrSettings(Payload, TEXT("time"), Hour);
    const float ClampedHour = FMath::Clamp(static_cast<float>(Hour), 0.0f, 24.0f);

    double Azimuth = 0.0;
    McpTryGetNumberFromPayloadOrSettings(Payload, TEXT("azimuth"), Azimuth);
    double Elevation = (ClampedHour / 24.0f) * 360.0f - 90.0f;
    McpTryGetNumberFromPayloadOrSettings(Payload, TEXT("elevation"), Elevation);

    double SunIntensity = 10.0;
    McpTryGetNumberFromPayloadOrSettings(Payload, TEXT("intensity"), SunIntensity);
    McpTryGetNumberFromPayloadOrSettings(Payload, TEXT("sunIntensity"), SunIntensity);
    double SkyIntensity = 1.0;
    McpTryGetNumberFromPayloadOrSettings(Payload, TEXT("skyLightIntensity"), SkyIntensity);
    McpTryGetNumberFromPayloadOrSettings(Payload, TEXT("skylightIntensity"), SkyIntensity);

    Actor->Modify();
    Actor->SetActorRotation(FRotator(static_cast<float>(Elevation), static_cast<float>(Azimuth), 0.0f));
    SunComponent->Modify();
    SunComponent->SetMobility(EComponentMobility::Movable);
    SunComponent->SetRelativeRotation(FRotator(static_cast<float>(Elevation), static_cast<float>(Azimuth), 0.0f));
    SunComponent->SetIntensity(static_cast<float>(SunIntensity));
    SunComponent->SetAtmosphereSunLight(true);
    SunComponent->SetAtmosphereSunLightIndex(0);
    SunComponent->MarkRenderStateDirty();

    SkyLightComponent->Modify();
    SkyLightComponent->SetMobility(EComponentMobility::Movable);
    SkyLightComponent->SetIntensity(static_cast<float>(SkyIntensity));
    SkyLightComponent->MarkRenderStateDirty();

    SkyAtmosphereComponent->Modify();
    SkyAtmosphereComponent->SetMobility(EComponentMobility::Movable);
    SkyAtmosphereComponent->MarkRenderStateDirty();

    TArray<FString> Applied;
    TArray<FString> Failed;
    const int32 ActorApplied = McpApplyPayloadSettings(Actor, Payload, Applied, Failed);
    const int32 SunApplied = McpApplyPayloadSettings(SunComponent, Payload, Applied, Failed);
    const int32 SkyLightApplied = McpApplyPayloadSettings(SkyLightComponent, Payload, Applied, Failed);
    const int32 SkyAtmosphereApplied = McpApplyPayloadSettings(SkyAtmosphereComponent, Payload, Applied, Failed);

    SunComponent->SetRelativeRotation(FRotator(static_cast<float>(Elevation), static_cast<float>(Azimuth), 0.0f));
    SunComponent->SetIntensity(static_cast<float>(SunIntensity));
    SunComponent->SetAtmosphereSunLight(true);
    SunComponent->SetAtmosphereSunLightIndex(0);
    SunComponent->MarkRenderStateDirty();
    SkyLightComponent->SetIntensity(static_cast<float>(SkyIntensity));
    SkyLightComponent->MarkRenderStateDirty();
    SkyAtmosphereComponent->MarkRenderStateDirty();

    Actor->MarkPackageDirty();
    Resp->SetStringField(TEXT("actorName"), Actor->GetActorLabel());
    Resp->SetStringField(TEXT("actorPath"), Actor->GetPathName());
    Resp->SetStringField(TEXT("sunComponentName"), SunComponent->GetName());
    Resp->SetStringField(TEXT("skyLightComponentName"), SkyLightComponent->GetName());
    Resp->SetStringField(TEXT("skyAtmosphereComponentName"), SkyAtmosphereComponent->GetName());
    Resp->SetBoolField(TEXT("hasSunLight"), true);
    Resp->SetBoolField(TEXT("hasSkyLight"), true);
    Resp->SetBoolField(TEXT("hasSkyAtmosphere"), true);
    Resp->SetNumberField(TEXT("componentCount"), 3);
    Resp->SetNumberField(TEXT("currentHour"), ClampedHour);
    Resp->SetNumberField(TEXT("azimuth"), Azimuth);
    Resp->SetNumberField(TEXT("elevation"), Elevation);
    Resp->SetNumberField(TEXT("sunIntensity"), SunIntensity);
    Resp->SetNumberField(TEXT("skyLightIntensity"), SkyIntensity);
    Resp->SetNumberField(TEXT("configuredPropertyCount"), ActorApplied + SunApplied + SkyLightApplied + SkyAtmosphereApplied);
    McpAddStringArrayField(Resp, TEXT("configuredProperties"), Applied);
    McpAddStringArrayField(Resp, TEXT("configurationErrors"), Failed);
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

        if (!McpInvokeObjectSetter(WaterActor, FName(TEXT("SetWaterWaves")), WaterWaves) &&
            !McpSetObjectPropertyValue(WaterActor, TEXT("WaterWaves"), WaterWaves))
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
    double NumberValue = 0.0;
    if (McpGetFirstNumberField(Payload, {TEXT("waveHeight")}, NumberValue))
    {
        const double Height = FMath::Max(NumberValue, 0.0001);
        McpApplyNumberProperty(Generator, {TEXT("MinAmplitude")}, Height, TEXT("waveHeight"), Resp, Applied);
        McpApplyNumberProperty(Generator, {TEXT("MaxAmplitude")}, Height, TEXT("waveHeight"), Resp, Applied);
    }
    else if (McpGetFirstNumberField(Payload, {TEXT("amplitude")}, NumberValue))
    {
        const double Height = FMath::Max(NumberValue, 0.0001);
        McpApplyNumberProperty(Generator, {TEXT("MinAmplitude")}, Height, TEXT("amplitude"), Resp, Applied);
        McpApplyNumberProperty(Generator, {TEXT("MaxAmplitude")}, Height, TEXT("amplitude"), Resp, Applied);
    }

    if (McpGetFirstNumberField(Payload, {TEXT("waveLength")}, NumberValue))
    {
        const double Wavelength = FMath::Max(NumberValue, 0.0001);
        McpApplyNumberProperty(Generator, {TEXT("MinWavelength")}, Wavelength, TEXT("waveLength"), Resp, Applied);
        McpApplyNumberProperty(Generator, {TEXT("MaxWavelength")}, Wavelength, TEXT("waveLength"), Resp, Applied);
    }

    if (McpGetFirstNumberField(Payload, {TEXT("steepness")}, NumberValue))
    {
        const double Steepness = FMath::Clamp(NumberValue, 0.0, 1.0);
        McpApplyNumberProperty(Generator, {TEXT("SmallWaveSteepness")}, Steepness, TEXT("steepness"), Resp, Applied);
        McpApplyNumberProperty(Generator, {TEXT("LargeWaveSteepness")}, Steepness, TEXT("steepness"), Resp, Applied);
    }

    const TSharedPtr<FJsonObject> *DirectionObj = nullptr;
    if (Payload.IsValid() && Payload->TryGetObjectField(TEXT("direction"), DirectionObj) && DirectionObj && DirectionObj->IsValid())
    {
        const FRotator Direction = McpGetRotatorField(Payload, TEXT("direction"), FRotator::ZeroRotator);
        McpApplyNumberProperty(Generator, {TEXT("WindAngleDeg")}, Direction.Yaw, TEXT("directionYaw"), Resp, Applied);
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
    Resp->SetStringField(TEXT("actorName"), TargetActor->GetActorLabel());
    Resp->SetStringField(TEXT("componentName"), Component->GetName());
    Resp->SetStringField(TEXT("componentPath"), Component->GetPathName());
    McpHandlerUtils::AddVerification(Resp, TargetActor);
    OutMessage = TEXT("Buoyancy component created");
    return true;
}

} // namespace McpEnvironmentHandlers
#endif
